/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

/* Modifier: J. Song
	Content distribution follows M.Zipf's law.
	Each packet is the chunk of a content
	seq: (# of chunks of one content * content rank) + chunk number
	contentIndex: rank of content
	contentName: prefix/<contentIndex*seqMax + seq>
	support multipath forwarding
	Unique window, Unique RTT
*/

/* Second modifying
	Sepearte window & RTT per each path
*/


#include "ndn-consumer-window-mptcp.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

#include <string>
#include <fstream>

#include "ns3/ndn-app-face.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"

//#include "stdlib.h"
#include "boost/random.hpp"
#include "deque"
#include "map"

using namespace boost;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerWindowMptcp");

namespace ns3 {
namespace ndn {
    
NS_OBJECT_ENSURE_REGISTERED (ConsumerWindowMptcp);
    
TypeId
ConsumerWindowMptcp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerWindowMptcp")
    .SetGroupName ("Ndn")
    .SetParent<Consumer> ()
    .AddConstructor<ConsumerWindowMptcp> ()

    .AddAttribute ("Window", "Initial size of the window",
                   StringValue ("5"),
                   MakeUintegerAccessor (&ConsumerWindowMptcp::SetWindow),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("SlowStartThreshold", "Initial ssth",
				   StringValue ("0"),
				   MakeUintegerAccessor (&ConsumerWindowMptcp::SetSsth),
				   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("PathWindow", "Initial # of the paths",
				   StringValue ("5"),
				   MakeUintegerAccessor (&ConsumerWindowMptcp::SetPathWindow),
				   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("PayloadSize", "Average size of content object size (to calculate interest generation rate)",
                   UintegerValue (1040),
                   MakeUintegerAccessor (&ConsumerWindowMptcp::GetPayloadSize, &ConsumerWindowMptcp::SetPayloadSize),
                   MakeUintegerChecker<uint32_t>())
    .AddAttribute ("Size", "Amount of data in megabytes to request (relies on PayloadSize parameter)",
                   DoubleValue (-1), // don't impose limit by default
                   MakeDoubleAccessor (&ConsumerWindowMptcp::GetMaxSize, &ConsumerWindowMptcp::SetMaxSize),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Frequency", "Frequency of request for content",
				   StringValue ("1.0"),
				   MakeDoubleAccessor (&ConsumerWindowMptcp::m_frequency),
				   MakeDoubleChecker<double> ())
    .AddAttribute ("Randomize", "Type of send time reandomization: none (default), uniform, exponential",
				   StringValue ("none"),
				   MakeStringAccessor (&ConsumerWindowMptcp::SetRandomize, &ConsumerWindowMptcp::GetRandomize),
				   MakeStringChecker ())
    .AddAttribute ("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ConsumerWindowMptcp::m_setInitialWindowOnTimeout),
                   MakeBooleanChecker ())
    .AddAttribute ("Index", "Node index of this consumer",
                   StringValue ("0"),
                   MakeUintegerAccessor (&ConsumerWindowMptcp::m_index),
                   MakeUintegerChecker<uint32_t> ())
/*
	.AddAttribute ("RetxTimer", "Timeout defining how frequent retx timouts should be checked",
			       StringValue("50ms"),
				   MakeTimeAccessor (&ConsumerWindowMptcp::GetRetxTimer, &ConsumerWindowMptcp::SetRetxTimer),
				   MakeTimeChecker ())
*/
    .AddTraceSource ("WindowTrace",
                     "Window that controls how many outstanding interests are allowed",
                     MakeTraceSourceAccessor (&ConsumerWindowMptcp::m_totalWindow))
    .AddTraceSource ("InFlight",
                     "Current number of outstanding interests",
                     MakeTraceSourceAccessor (&ConsumerWindowMptcp::m_totalInFlight))
    ;

  return tid;
}

ConsumerWindowMptcp::ConsumerWindowMptcp ()
  : m_payloadSize (1040)
  , m_q (0.0)				//zipf param
  , m_s (1.0)				//zipf param
  , m_SeqRng (0.0, 1.0)		//random rng
  , m_frequency (1.0)		//request interval
  , m_random (0)			//type of request interval
  , m_contentRank (0)		//content number
  , m_initialWindow (5)		//
  , m_initialSsth(0)
  , m_snpCalled (true)		//flag for next content retrieval
//  , m_windowFraction (0)		//to increase window size
  , m_initialPathWindow (5) //
  , m_pathCount(0)			//counter for increasing # of paths
  , m_totalTimeoutCount(0)	//counter for decreasing # of paths
  , m_branchSeed (1)		//random seed
  , m_totalContent (0)		//num of received content
  , m_avgCompletionTime (0.0)	//average download completion time
  , m_totalInFlight (0)
{
  SetNumberOfContents(10000);	//make zipf distribution
  for (int i=0; i<MAX_PATHID; i++) {
	m_window[i] = 0;
	m_preWindow[i] = 0;
	m_ssth[i] = 0;
	m_avgRtt[i] = 0;
	m_inFlight[i] = 0;
	m_windowFraction[i] = 0.0;
	m_timeoutCount[i] = 0;
	//cout << i << ":\t";
	m_pathId[i] = i;
	m_trajNum[i] = -1;
	m_rtt[i] = CreateObject<RttMeanDeviation> ();
        m_rtt[i]->SetMaxRto(Seconds(2));
  }
}

void
ConsumerWindowMptcp::SetRetxTimer (Time retxTimer)
{
  m_retxTimer = retxTimer;
  NS_LOG_DEBUG("SRT(): RetxTimer is set: " << m_retxTimer);
  if (m_retxEvent.IsRunning ())
  {
    // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
	Simulator::Remove (m_retxEvent); // slower, but better for memory
  }
  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule (m_retxTimer, &ConsumerWindowMptcp::CheckRetxTimeout, this);
}

Time
ConsumerWindowMptcp::GetRetxTimer () const
{
  return m_retxTimer;
}

void
ConsumerWindowMptcp::CheckRetxTimeout ()
{
  Time now = Simulator::Now ();
  while (!m_seqTimeouts.empty ())
  {
    NS_LOG_DEBUG("RETRANSMISSION CHECK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry = m_seqTimeouts.get<i_timestamp> ().begin ();
    uint32_t seqNo = entry->seq;
    map<uint32_t, int>::iterator findIter = m_branchIntMap.find(seqNo);
    if (findIter == m_branchIntMap.end())
    {
      
    }
    else
    {
      Time rto = m_rtt[findIter->second]->RetransmitTimeout ();
      NS_LOG_INFO ("rto[" << findIter->second << "]: " << rto);
      if (entry->time + rto <= now) // timeout expired?
      {
        m_seqTimeouts.get<i_timestamp> ().erase (entry);
        NS_LOG_DEBUG("timeout occrus. seqNo: "<<seqNo);
        OnTimeout (seqNo);
      }
      else
      {
        NS_LOG_DEBUG("no timeouts.");
        break; // nothing else to do. All later packets need not be retransmitted
      }
    }
  }
  m_retxEvent = Simulator::Schedule (m_retxTimer, &ConsumerWindowMptcp::CheckRetxTimeout, this); 
  NS_LOG_DEBUG("Retransmission check ends.");
}

void
ConsumerWindowMptcp::WindowOutput(ofstream &ofs)
{
  unsigned int cum = 0; 
  for(int i=0; i<MAX_PATHID; i++)
  {
    cum += m_poolCounter[i];
  }
  ofs << m_index << "\t" << Simulator::Now().ToDouble(Time::S) << "\t" << m_totalWindow << "\t" << cum << "\n";
}

void
ConsumerWindowMptcp::SetWindow (uint32_t window)
{
  m_initialWindow = window;
  m_totalWindow = m_initialWindow;
  for(int i=0; i<MAX_PATHID; i++)
  {
	m_window[i] = 0;
  }
}

uint32_t
ConsumerWindowMptcp::GetWindow () const
{
  return m_totalWindow;
}

void
ConsumerWindowMptcp::SetSsth (uint32_t ssth)
{
  m_initialSsth = ssth;
  for (int i=0; i < MAX_PATHID; i++)
  {
    m_ssth[i] = m_initialSsth;
  }
}

void
ConsumerWindowMptcp::SetPathWindow (uint32_t pathWindow)
{
  m_initialPathWindow = pathWindow;
  m_pathWindow = m_initialPathWindow;
}

uint32_t
ConsumerWindowMptcp::GetPathWindow () const
{
  return m_pathWindow;
}

uint32_t
ConsumerWindowMptcp::GetPayloadSize () const
{
  return m_payloadSize;
}

void
ConsumerWindowMptcp::SetPayloadSize (uint32_t payload)
{
  m_payloadSize = payload;
}

void
ConsumerWindowMptcp::SetRandomize (const std::string &value)
{
  if (m_random)
    delete m_random;

  else if (value == "uniform")
  {
    m_random = new UniformVariable (0.0, 2 * 1.0 / m_frequency);
  }
  else if ( value == "exponential")
  {
    m_random = new ExponentialVariable (1.0 / m_frequency, 50 * 1.0 / m_frequency);
  }
  else
    m_random = 0;

  m_randomType = value;
}

std::string
ConsumerWindowMptcp::GetRandomize () const
{
  return m_randomType;
}

double
ConsumerWindowMptcp::GetMaxSize () const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerWindowMptcp::SetMaxSize (double size)
{
  m_maxSize = size;
  if (m_maxSize < 0)
    {
      m_seqMax = 0;
      return;
    }

  m_seqMax = floor(1.0 + m_maxSize * 1024.0 * 1024.0 / m_payloadSize);
  NS_LOG_DEBUG ("MaxSeqNo: " << m_seqMax);
  // std::cout << "MaxSeqNo: " << m_seqMax << "\n";
}

void
ConsumerWindowMptcp::SetNumberOfContents (uint32_t numOfContents)
{
  m_N = numOfContents;

  m_Pcum = std::vector<double> (m_N + 1);

  m_Pcum[0] = 0.0;
  for (uint32_t i=1; i<=m_N; i++)
  {
    m_Pcum[i] = m_Pcum[i-1] + 1.0/pow(i+m_q, m_s);
  }

  for (uint32_t i=1; i<=m_N; i++)
  {
    m_Pcum[i] = m_Pcum[i] / m_Pcum[m_N];
    //NS_LOG_LOGIC("cum Probability ["<<i<<"]="<<m_Pcum[i]);
  }
}

uint32_t
ConsumerWindowMptcp::GetNextSeq()
{
  uint32_t content_index = 1; //[1, m_N]
  double p_sum = 0;

  double p_random = m_SeqRng.GetValue();
  for(uint32_t i=0; i<m_index; i++)
  {
	  p_random = m_SeqRng.GetValue();
  }
  while (p_random == 0)
    {
      p_random = m_SeqRng.GetValue();
    }
  NS_LOG_LOGIC("p_random="<<p_random);
  for (uint32_t i=1; i<=m_N; i++)
    {
      p_sum = m_Pcum[i];   //m_Pcum[i] = m_Pcum[i-1] + p[i], p[0] = 0;   e.g.: p_cum[1] = p[1], p_cum[2] = p[1] + p[2]
      if (p_random <= p_sum)
        {
          content_index = i;
          break;
        } //if
    } //for
    //content_index = 1;
  NS_LOG_DEBUG("ContentIndex="<<content_index);
  NS_LOG_INFO("m_totalInFlight: " << m_totalInFlight);
  return content_index;
}


/* originally, in parent object ndn-consumer.cc
 * for special purpose, override on this object
 * (like branchInt setting, sequece numbering, etc)
 * overriding allows increasing m_inFlight be here(originally in scheduling)
 * when m_inFlight++ is in scheduing, request for next content cannot utilize
 * window size precisely
 * - jhsong */
void
ConsumerWindowMptcp::SendPacket (int pathId)
{
  if (!m_active) {
	  NS_LOG_INFO("SP(int): m_active is false!!!!!");
	  return;
  }

  m_totalInFlight++;
  m_inFlight[pathId]++;

  uint32_t seq=std::numeric_limits<uint32_t>::max ();

  // if there are packets to be retransmitted
  while (m_retxSeqs[pathId].size ())
  {
    seq = *m_retxSeqs[pathId].begin();
    m_retxSeqs[pathId].erase (m_retxSeqs[pathId].begin());
	NS_LOG_DEBUG("SP(): RETRANSMISSION PACKET!!!!!!!!!!!!!!!!!!!!!!");
    break;
  }
  // if there is no packets to be retransmitted
  if (seq == std::numeric_limits<uint32_t>::max ())
  {
    if (m_snpCalled == true)
    {
      m_contentRank = GetNextSeq();
      m_seq = m_contentRank * m_seqMax;
      m_startTime.insert( map<uint32_t, Time>::value_type( m_contentRank, Simulator::Now() ));
      m_chunkCounter.insert( map<uint32_t, int>::value_type( m_contentRank, 0));
      m_snpCalled = false;
    }
    seq = m_seq++;
  }
  
  Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_interestName);
  (*nameWithSequence) (seq);

  InterestHeader interestHeader;
  interestHeader.SetNonce               (m_rand.GetValue ());
  interestHeader.SetName                (nameWithSequence);

  // Set pathId of Interest
  interestHeader.SetPathId(pathId);
  
  m_branchIntMap.insert( map<uint32_t, int>::value_type(seq, pathId));
      
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (interestHeader);
//  NS_LOG_DEBUG ("Interest packet size: " << packet->GetSize ());
  //NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");
  m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqFullDelay.insert (SeqTimeout (seq, Simulator::Now ()));

  m_seqLastDelay.erase (seq);
  m_seqLastDelay.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqRetxCounts[seq] ++;

  m_transmittedInterests (&interestHeader, this, m_face);

  m_rtt[pathId]->SentSeq (SequenceNumber32 (seq), 1);

  FwHopCountTag hopCountTag;
  packet->AddPacketTag (hopCountTag);

  m_protocolHandler (packet);

  NS_LOG_DEBUG ("pathId: " << pathId << ", m_totalWindow: " << m_totalWindow << ", m_totalInFlight: " << m_totalInFlight << ", m_window[pathId]: " << m_window[pathId] << ", m_inFlight[pathId]: " << m_inFlight[pathId]);
  
  ScheduleNextPacket (pathId);
}


void
ConsumerWindowMptcp::ScheduleNextPacket (int pathId)
{
  //NS_LOG_INFO("SNP(int): Request for consequent chunks. path: "<<pathId << ", m_totalWindow: " << m_totalWindow << ", m_totalInFlight: " << m_totalInFlight << ", m_window[]: " << m_window[pathId] << ", m_inFligt[]: " << m_inFlight[pathId]);
  if (m_totalWindow == static_cast<uint32_t> (0))
  {
    NS_LOG_DEBUG("SNP(int): Total window size is 0, but not new content.\n");
  }
  else
  {
    if (m_inFlight[pathId] >= m_window[pathId]) {
    //NS_LOG_DEBUG("SNP(int): m_inFlight[] >= m_window[]");
    // simply do nothing
    }
    else if (m_window[pathId] == (uint32_t)0) {
      NS_LOG_DEBUG("SNP(int): m_window[] == 0");
      // simply do nothing
    }
    else
    {
      m_sendEvent = Simulator::ScheduleNow (&ConsumerWindowMptcp::SendPacket, this, pathId);
      NS_LOG_DEBUG("SNP(int): Next packet is scheduled, pathId: "<<pathId);
    }
  }
}

void
ConsumerWindowMptcp::ScheduleNextPacket ()
{
  Simulator::Remove(m_sendEvent);
  m_snpCalled = true;
  // select new pathId
  mt19937 gen(m_branchSeed);
  uniform_int<> dst(0,MAX_PATHID);
  variate_generator< mt19937, uniform_int<> > rand(gen, dst);

  mt19937 in(m_index);
  uniform_int<> inter(0,1);
  variate_generator< mt19937, uniform_int<> > rand2(in, inter);

  m_totalWindow = m_initialWindow;
  m_pathWindow = m_initialPathWindow;
  NS_LOG_DEBUG("SNP(): Request for new content, m_tW: " << m_totalWindow << ", m_pW: " << m_pathWindow);
  m_totalInFlight = 0;
  m_pathCount = 0;
  m_totalTimeoutCount = 0;
  m_branchIntMap.clear();
  m_nextBranchInt.clear();
  //m_retxSeqs.clear();
  m_seqTimeouts.clear();
  m_seqLastDelay.clear();
  m_seqFullDelay.clear();
  m_seqRetxCounts.clear();
  for(int i=0; i<MAX_PATHID; i++) {
    m_window[i] = 0;
    m_ssth[i] = m_initialSsth;
    m_preWindow[i] = 0;
    m_avgRtt[i] = 0;
    m_windowFraction[i] = 0.0;
    m_inFlight[i] = 0;
    m_timeoutCount[i] = 0;
    m_pathId[i] = i;
    m_trajNum[i] = -1;
    m_rtt[i]->Reset ();
    m_rtt[i]->SetMaxRto(Seconds(2));
    m_retxSeqs[i].clear();
  }

  // randomize pathId
  int pathId = rand() % MAX_PATHID;
	  
  // include best path
  //int pathId = 8 * (int)(rand() % (MAX_PATHID / 8));

  // linear increase
  uint32_t temp = 0;
  // randomize inteval
  //int interval = rand2();

  // fixed interval
  int interval = 1;

  while(m_pathWindow > temp)
  {
    if (m_window[pathId] == (uint32_t)0)
    {
      m_window[pathId]++;
      //m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5 + tmp + tmp2, m_rtt[pathId]->RetransmitTimeout().ToDouble(Time::S) + tmp + tmp2)), &ConsumerWindowMptcp::SendPacket, this, pathId);
      //m_sendEvent = Simulator::ScheduleNow (&ConsumerWindowMptcp::SendPacket, this, pathId);
      m_sendEvent = Simulator::Schedule (Seconds (interval), &ConsumerWindowMptcp::SendPacket, this, pathId);
      //m_sendEvent = Simulator::Schedule (Seconds (100*m_totalContent - Simulator::Now().ToDouble(Time::S)), &ConsumerWindowMptcp::SendPacket, this, pathId);
      //m_sendEvent = Simulator::Schedule (Seconds (0.5), &ConsumerWindowMptcp::SendPacket, this, pathId);
      temp++;
      //tmp += tmp2;
      NS_LOG_DEBUG("SNP(): path is selected: " << pathId);
    } else {
      pathId++;
      if (pathId >= MAX_PATHID)
      {
        pathId = 0;
      }
    }
  }

  /* randomize pathId
  uint32_t temp = 0;
  int interval = rand2();
  while(m_pathWindow > temp)
  {
    if (m_window[pathId] == (uint32_t)0)
    {
      m_window[pathId]++;
      //m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5 + tmp + tmp2, m_rtt[pathId]->RetransmitTimeout().ToDouble(Time::S) + tmp + tmp2)), &ConsumerWindowMptcp::SendPacket, this, pathId);
      //m_sendEvent = Simulator::ScheduleNow (&ConsumerWindowMptcp::SendPacket, this, pathId);
      m_sendEvent = Simulator::Schedule (Seconds (interval), &ConsumerWindowMptcp::SendPacket, this, pathId);
      //m_sendEvent = Simulator::Schedule (Seconds (100*m_totalContent - Simulator::Now().ToDouble(Time::S)), &ConsumerWindowMptcp::SendPacket, this, pathId);
      //m_sendEvent = Simulator::Schedule (Seconds (0.5), &ConsumerWindowMptcp::SendPacket, this, pathId);
      temp++;
      //tmp += tmp2;
      NS_LOG_DEBUG("SNP(): path is selected: " << pathId);
    } else {
      pathId = rand() % MAX_PATHID;
      if (pathId >= MAX_PATHID)
      {
        pathId = 0;
      }
    }
  }
  */

  NS_LOG_DEBUG("SNP(): Request for new content ends, interval: "<<interval);
}


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerWindowMptcp::OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                                     Ptr<Packet> payload)
{
  NS_LOG_DEBUG("OC() starts.");
  uint32_t seq = boost::lexical_cast<uint32_t> (contentObject->GetName ().GetComponents ().back ());

  // find pool index of corresponding chunk.
  map<uint32_t, int>::iterator findIter = m_branchIntMap.find(seq);
  NS_LOG_DEBUG("OC(): seq search ends.");
  // if there's no matching entry in branchIntMap
  // this means, new content transmission is started
  if(findIter == m_branchIntMap.end()) {
  /*
  int temp = 0;
  int count = 0;
  while (m_window[temp] != (uint32_t)0 && (m_inFlight[temp] < m_window[temp])) {
    temp = rand() % MAX_PATHID;
    count++;
    if(count > MAX_PATHID*2) {
      break;
    }
  }
  Consumer::OnContentObject (contentObject, payload);
  ScheduleNextPacket(temp);
  */
    NS_LOG_DEBUG("OC(): ERROR!! no matching sequence number!");
    return;
  }
  else if (m_snpCalled == true)
  {
    NS_LOG_DEBUG("OC(): snpCalled is true");
  }
  // if there's a matching entry in branchIntMap
  // this means, content transmission is now on-going
  else {
    NS_LOG_DEBUG("OC(): seq is found.");
    int poolIndex = findIter->second;
    int trajectory = contentObject->GetHash ();
    m_trajNum[poolIndex] = trajectory;
    int newIndex = FindArrayIndex(m_trajNum, trajectory);
    if (newIndex == -1)
    {
      NS_LOG_DEBUG("OC(): There is no duplicated paths.");
    }
    else if (poolIndex == newIndex)
    {
      NS_LOG_DEBUG("OC(): This path is not duplicated with others.");
    }
    else
    {
      NS_LOG_DEBUG ("OC(): Path(" << poolIndex << ") and Path(" << newIndex << ") are duplicated. Hash("<< trajectory << "). Merge into Path(" << newIndex << ").");
      NS_LOG_DEBUG ("merged window[" << newIndex << "]: " << m_window[newIndex] << ", removed window[" << poolIndex <<"]: " << m_window[poolIndex]);
      m_window[newIndex] += m_window[poolIndex];
      m_window[poolIndex] = 0;
      m_inFlight[newIndex] += m_inFlight[poolIndex];
      m_inFlight[poolIndex] = 0;
      m_windowFraction[newIndex] += m_windowFraction[poolIndex];
      m_windowFraction[poolIndex] = 0.0;
      poolIndex = newIndex;
      /*
      int i = 0;
      while (m_window[(trajectory%MAX_PATHID) + i] != (uint32_t)0)
      {
        i++;
      }
      NS_LOG_DEBUG("OC(): new path is selected: " << ((trajectory%MAX_PATHID) + i));
      m_window[(trajectory%MAX_PATHID) + i]++;
      m_sendEvent = Simulator::Schedule (Seconds (0), &ConsumerWindowMptcp::SendPacket, this, (trajectory%MAX_PATHID) + i);
      */
    }

    SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find (seq);
    double chunkRTT = entry->time.ToDouble(Time::S);
    chunkRTT = Simulator::Now().ToDouble(Time::S) - chunkRTT;

    if (Simulator::Now().ToDouble(Time::S) > 490) {
      char fileName[50];
      sprintf(fileName, "%d_branchStats_mptcp.txt", m_index);
      m_ofs2.open(fileName, ios::out);
      m_ofs2 << "#\tCounts\tavgRTT\t" << Simulator::Now().ToDouble(Time::S) << "\n";
      for(int i=0; i<MAX_PATHID; i++) {
        m_ofs2 << i << "\t" << m_poolCounter[i] << "\t" << m_poolDelay[i] / m_poolCounter[i] << "\n";
      }
      m_ofs2.close();
    }
    m_nextBranchInt.push_back(poolIndex);
    m_branchIntMap.erase(seq);

    // MPTCP part
    m_windowFraction[poolIndex] += GetWindowFraction(poolIndex);
    NS_LOG_DEBUG("OC(): MPTCP calculation ends.");

    NS_LOG_INFO("windowFraction: " << m_windowFraction[poolIndex]);
  
    Consumer::OnContentObject (contentObject, payload);

    m_rtt[poolIndex]->AckSeq (SequenceNumber32 (seq));
    m_rtt[poolIndex]->ResetMultiplier();

    // window size increase occurs when subscriber receive some amount of chunks
    // this some amount is current window size in this code(can be changed)

    // if this path is activated
    if (m_window[poolIndex] > (uint32_t) 0)
    {
      // congestion-avoidance phase
      if (m_ssth[poolIndex] < m_window[poolIndex])
      {
        if (m_windowFraction[poolIndex] >= (double)1.0)
        {
          m_window[poolIndex] = m_window[poolIndex]+1;
          m_totalWindow = m_totalWindow + 1;
          m_windowFraction[poolIndex] = 0.0;
          if (m_totalWindow % 5 == (uint32_t)0)
	  {
            char fileName[30];
            sprintf(fileName, "%d_window_mptcp.txt", m_index);
            m_ofs.open(fileName, ios::app);
            WindowOutput(m_ofs);
            m_ofs.close();
 	  }
	}
      }
      // slow-start phase
      else
      {
        m_window[poolIndex]++;
        m_totalWindow++;
	m_windowFraction[poolIndex] = 0.0;
        if (m_totalWindow % 5 == (uint32_t)0)
	{
          char fileName[30];
          sprintf(fileName, "%d_window_mptcp.txt", m_index);
          m_ofs.open(fileName, ios::app);
          WindowOutput(m_ofs);
          m_ofs.close();
	}
      }
    }
    // if this path is inactivated
    else
    {
      // simply do nothing
    }
  
    if (m_inFlight[poolIndex] > static_cast<uint32_t> (0)) {
      m_inFlight[poolIndex]--;
    }

    if (m_totalInFlight > static_cast<uint32_t> (0)) {
	    m_totalInFlight--;
    }
    //NS_LOG_DEBUG ("m_pathWindow: " << m_pathWindow << ", m_totalWindow: " << m_totalWindow << ", poolIndex: " << poolIndex << ", m_window[poolIndex]: " << m_window[poolIndex] << ", m_windowFraction[]: " << m_windowFraction[poolIndex] << ", m_totalInFlight: " << m_totalInFlight);

    m_poolCounter[poolIndex]++;
    m_poolDelay[poolIndex] += chunkRTT;

//  if(seq % m_seqMax == 0)
//  {
//    m_chunkCounter.insert( map<uint32_t, uint32_t>::value_type( seq/m_seqMax, 1));
//  }
//  else
//  {
    map<uint32_t, uint32_t>::iterator findIter2 = m_chunkCounter.find(m_contentRank);
    if (findIter2 == m_chunkCounter.end())
    {
      NS_LOG_DEBUG("failed to find content index in m_chunkCounter. seq: " << seq << ", seq/m_seqMax: " << seq/m_seqMax << ", m_contentRank: " << m_contentRank << ", findIter2->first :" << findIter2->first << ", findIter2->second :" << findIter2->second);
    }
    findIter2->second++;
    if (findIter2->second >= m_seqMax)
    {
      map<uint32_t, Time>::iterator findIter3 = m_startTime.find(m_contentRank);
      double timeTemp = findIter3->second.ToDouble(Time::S);
      uint32_t sumOfWindow = 0;
      for(int i=0; i<MAX_PATHID; i++) {
        sumOfWindow += m_window[i];
      }
      timeTemp = Simulator::Now().ToDouble(Time::S) - timeTemp;
      m_avgCompletionTime = ((double)m_totalContent/(m_totalContent+1)) * m_avgCompletionTime + ((double)1/(m_totalContent+1)) * timeTemp;
      cout << m_index << "\t" << Simulator::Now().ToDouble(Time::S) << "\tcontentNum: " << m_contentRank << "\ttimeTemp: " << timeTemp << ", contentTotal: " << m_totalContent << ", avg comp time: " << m_avgCompletionTime << ", m_totalWindow: " << m_totalWindow << ", sumOfWindow: " << sumOfWindow << "\n";
//      cout << m_index << "\tbranchIntSize: " << m_branchIntMap.size() << "\n";
      m_totalContent++;
      m_startTime.clear();
      m_chunkCounter.clear();
      //m_startTime.erase(m_contentRank);
      //m_chunkCounter.erase(m_contentRank);
      //m_nextBranchInt.clear();
      //m_branchIntMap.clear();
      ScheduleNextPacket();
    }
    else {
      if(m_window[poolIndex] > (uint32_t) 0) {
        ScheduleNextPacket(poolIndex);
      } else {
      }
    }
  }
}

void
ConsumerWindowMptcp::OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> payload)
{
  Consumer::OnNack(interest, payload);

  if (m_totalInFlight > static_cast<uint32_t> (0))
  {
    m_totalInFlight--;
  }

  if (m_totalWindow > static_cast<uint32_t> (0))
  {
      //if (m_totalWindow > uint32_t(1)) m_totalWindow=m_totalWindow-1;
      //m_windowFraction = 0;
  }
  NS_LOG_DEBUG ("Window: " << m_totalWindow << ", inFlight: " << m_totalInFlight);
  cout << m_index << ": OnNack!!!!!!!!!!!!!!!!!!!!!!!!!\n";

}
/*
void
ConsumerWindowMptcp::OnTimeout (uint32_t contentRank, uint32_t sequenceNumber)
{

  RetxConSeqsList* rcsl = new RetxConSeqsList(contentRank, sequenceNumber);
  m_retxConSeqs.push_back(rcsl);

  if (m_inFlight > static_cast<uint32_t> (0))
    m_inFlight = m_inFlight - 1;

  m_window = m_initialWindow;
  NS_LOG_DEBUG ("Rank: " << contentRank << ", Seq: " << sequenceNumber <<" must be retxed.");
 // NS_LOG_DEBUG ("Window: " << m_window << ", InFlight: " << m_inFlight);
  Consumer::OnTimeout (sequenceNumber);
}
*/
void
ConsumerWindowMptcp::OnTimeout (uint32_t sequenceNumber)
{
  NS_LOG_DEBUG("OT(): TIMEOUT OCCURS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  map<uint32_t, int>::iterator findIter = m_branchIntMap.find(sequenceNumber);
  // if new content transmission is started
  if(findIter == m_branchIntMap.end()) {

	  //ScheduleNextPacket();
	  //cout << m_index << "\tOT(): branchIntMap empty!!!!!!!!!!!!!!!!!!!\n";
	  /*
	  int temp = 0;
	  int count = 0;
	  while (m_window[temp] != (uint32_t)0 && (m_inFlight[temp] < m_window[temp])) {
		  temp = rand() % MAX_PATHID;
		  count++;
		  if(count > MAX_PATHID*2) {
			  return;
		  }
	  }
	  ScheduleNextPacket(temp);
	  return;
	  */
	  // simply do nothing
  }
  else if (m_snpCalled == true)
  {
	NS_LOG_DEBUG("******************OT(): snpCalled is true**********************");
  }
  // else, transmission is now on-going
  else
  {
    int poolIndex = findIter->second;

    if (m_inFlight[poolIndex] > static_cast<uint32_t> (0)) {
      m_inFlight[poolIndex]--;
    }

    if (m_totalInFlight > static_cast<uint32_t> (0)) {
      m_totalInFlight--;
    }

    if (m_setInitialWindowOnTimeout)
    {

//      if(m_window[poolIndex] > (uint32_t)1 && m_window[poolIndex] >= m_preWindow[poolIndex]*3/5)
      if(m_window[poolIndex] > (uint32_t)1)
      {
		m_preWindow[poolIndex] = m_window[poolIndex];
		m_ssth[poolIndex] = m_window[poolIndex] / 2;
        m_totalWindow = m_totalWindow - (m_window[poolIndex] - m_window[poolIndex]/2);
	    //if (m_totalWindow < m_initialWindow) m_totalWindow = m_initialWindow;
	    m_window[poolIndex] /= 2;
      }
	  m_windowFraction[poolIndex] = 0.0;

	  if(m_window[poolIndex] > (uint32_t)0)
  	  {
	    m_totalTimeoutCount++;
        m_timeoutCount[poolIndex]++;
	  }
    }

	/* path decrease
    uint32_t deleteIndex = GetMaxIndex(m_timeoutCount);
    bool deleteFlag = false;
    if(m_totalTimeoutCount > m_pathWindow)
    {
	  if((m_pathWindow > m_initialPathWindow) && (deleteIndex != MAX_PATHID) && (m_timeoutCount[deleteIndex] != 0))
	  {
		  NS_LOG_DEBUG("Path delete. m_index: " <<  m_index << ", deleted path: " << deleteIndex << ", deletedWindow: " << m_window[deleteIndex] << ", totalWindow: " << m_totalWindow << ", m_inFlight[]: " << m_inFlight[deleteIndex] << ", totalInFlight: " << m_totalInFlight << ", m_pathWindow:  " << m_pathWindow);
		  
		  //uint32_t sumOfWindow = 0;
		  //for (int z=0; z<MAX_PATHID; z++)
		  //{
			//sumOfWindow += m_window[z];
			//cout << "BranchIndex: " << z << ", m_window[]: " << m_window[z] << ", m_inFlight[]: " << m_inFlight[z] << "\n";
		  //}
		  //cout << "sumOfWindow: " << sumOfWindow << ", totalWindow: " << m_totalWindow << "\n";
		  
		  deleteFlag = true;
		  m_pathWindow--;
		  m_totalTimeoutCount = 0;
		  m_timeoutCount[deleteIndex] = 0;
		  m_pathCount = 0;
		  m_totalWindow = m_totalWindow - m_window[deleteIndex];
		  m_window[deleteIndex] = 0;
		  //m_inFlight[deleteIndex] = 0;
		  m_windowFraction[deleteIndex] = 0;
	  }
    }
	*/


    char fileName[30];
    sprintf(fileName, "%d_window_mptcp.txt", m_index);
    m_ofs.open(fileName, ios::app);
    WindowOutput(m_ofs);
    m_ofs.close();

    NS_LOG_DEBUG ("seq: " << sequenceNumber << ", m_totalWindow: " << m_totalWindow << ", m_totalInFlight: " << m_totalInFlight << ", poolIndex: " << poolIndex << ", m_window[poolIndex]: " << m_window[poolIndex] << ", m_inFlight[poolindex]: "<<m_inFlight[poolIndex]);
    Consumer::OnTimeout (sequenceNumber);
	m_retxSeqs[poolIndex].insert (sequenceNumber);
	m_rtt[poolIndex]->IncreaseMultiplier ();
	m_rtt[poolIndex]->SentSeq (SequenceNumber32 (sequenceNumber), 1);
	ScheduleNextPacket (poolIndex);

	// path decrease part
//    if(deleteFlag == true && deleteIndex == (uint32_t)poolIndex)
//	{
      /*
	  int temp = 0;
	  int count = 0;
	  while (m_window[temp] != (uint32_t)0 && (m_inFlight[temp] < m_window[temp]))
	  {
		  temp = rand() % MAX_PATHID;
		  count++;
		  if(count > MAX_PATHID*2) {
			  return;
		  }
	  }
	  ScheduleNextPacket(temp);
	  */
      // simply do nothing
//    } else {
//	  ScheduleNextPacket(poolIndex);
//    }
  }
}

uint32_t
ConsumerWindowMptcp::GetMaxIndex(uint32_t* src)
{
	uint32_t max, maxIndex;
	max = 0;
	maxIndex = MAX_PATHID;
	for(int i=0; i<MAX_PATHID; i++)
	{
		if(src[i] > max)
		{
			max = src[i];
			maxIndex = i;
		}
	}
	return maxIndex;
}

uint32_t
ConsumerWindowMptcp::GetMaxIndices(uint32_t* src, uint32_t* dst)
{
	uint32_t max, maxNum;
	max = 0;
	maxNum = 0;
	for(int i=0; i<MAX_PATHID; i++)
	{
		if(src[i] == 0) continue;
		else if(src[i] > max)
		{
			maxNum = 1;
			max = src[i];
			dst[0] = i;
		}
		else continue;
		/* duplicate version
		else if(src[i] > max)
		{
			maxNum = 0;
			max = src[i];
			dst[maxNum] = i;
			maxNum++;
		}
		else if (src[i] == max)
		{
			dst[maxNum] = i;
			maxNum++;
		}
		*/
	}
	return maxNum;
}

uint32_t
ConsumerWindowMptcp::GetMaxIndicesDouble(double* src, uint32_t* dst)
{
	double max;
	uint32_t maxNum;
	max = 0;
	maxNum = 0;
	for(int i=0; i<MAX_PATHID; i++)
	{
		if(src[i] == 0) continue;
		else if(src[i] > max)
		{
			maxNum = 1;
			max = src[i];
			dst[0] = i;
		}
		else continue;
		/* duplicate version
		else if(src[i] > max)
		{
			maxNum = 0;
			max = src[i];
			dst[maxNum] = i;
			maxNum++;
		}
		else if (src[i] == max)
		{
			dst[maxNum] = i;
			maxNum++;
		}
		*/
	}
	return maxNum;
}

bool
ConsumerWindowMptcp::IsInArray(uint32_t* src, uint32_t value)
{
	bool result = false;
	for(int i=0; i<MAX_PATHID; i++)
	{
		if (src[i] == value)
		{
			result = true;
			break;
		}
	}

	return result;
}

int
ConsumerWindowMptcp::FindArrayIndex(int* src, int value)
{
	int index = -1;
	for(int i=0; i<MAX_PATHID; i++)
	{
		if (src[i] == value)
		{
			index = i;
			break;
		}
	}

	return index;
}

uint32_t
ConsumerWindowMptcp::IsDuplicated(uint32_t* src1, uint32_t src1Length, uint32_t* src2, uint32_t src2Length)
{
	uint32_t result = 0;
	if (src1Length == 0) return 0;
	if (src2Length == 0) return 0;
	for(int i=0; i<(int)src1Length; i++)
	{
		for(int j=0; j<(int)src2Length; j++)
		{
			if(src1[i] == src2[j])
			{
				result++;
			}
		}
	}
	return result;
}

// input: current index and path window
double
ConsumerWindowMptcp::GetWindowFraction(int index)
{
  int indice[m_pathWindow];
  int count = 0;
  double numerator = 0;
  double denominator = 0;
//  double denoCandi[100] = 0;
  double result = 0;
  cout.precision(9);

  for (int i=0; i<(int)m_pathWindow; i++)
  {
    indice[i] = 0;
  }

  // activated subflow extraction
  for (int i=0; i<MAX_PATHID; i++)
  {
    if (m_window[i] != 0)
    {
      indice[count] = i;
      count++;
    }
  }

  //count = 0;
  
  // numerator, denominator extraction
  for (int i=0; i<(int)m_pathWindow; i++)
  {
    double temp = 0;
    if (m_poolCounter[indice[i]] == 0)
    {
      m_avgRtt[indice[i]] = 0;
    }
    else
    {
      m_avgRtt[indice[i]] = m_poolDelay[indice[i]] / m_poolCounter[indice[i]];
    }
    if (m_avgRtt[indice[i]] == 0)
    {
      temp = 0;
    }
    else
    {
      temp = (double)m_window[indice[i]] / m_avgRtt[indice[i]] / m_avgRtt[indice[i]];
      denominator += ((double)m_window[indice[i]] / m_avgRtt[indice[i]]);
    }

    if (temp > numerator) numerator = temp;
  }
  denominator *= denominator;


  if (denominator == (double)0)
  {
    result = 0;
  }
  else
  {
    result = (numerator / denominator);
//    cout << "1. index: " << index << ", num: " << numerator << ", den: " << denominator << ", result: " << result << "\n";
  }

  if ( result > ((double)1 / (double)m_window[index]) )
  {
    result = (double)1 / (double)m_window[index];
  }

//  cout << "2. index: " << index << ", num: " << numerator << ", den: " << denominator << ", result: " << result << ", 1/wr: " << (double)1/(double)m_window[index] << "\n";

  NS_LOG_DEBUG ("GWF: index: " << index << ", count: " << count << ", numerator: " << numerator << ", denominator: " << denominator << ", result: " << result);

  return result;


/*
  // denominators extraction
  for (int i=0; i<size; i++)
  {
    denoCandi[count] = (m_window[indice[i]] / m_rtt[indice[i]])^2;
    count++;
    for (int j=i+1; j<size; j++)
    {
      denoCandi[count] += m_window[indice[i]] / m_rtt[indice[i]];
      denoCandi[count] += m_window[indice[j]] / m_rtt[indice[j]];
      denoCandi[count] *= denoCandi[count];
      count++;
      for (int k=j+1; k<size; k++)
      {
        denoCandi[count] += m_window[indice[i]] / m_rtt[indice[i]];
        denoCandi[count] += m_window[indice[j]] / m_rtt[indice[j]];
        denoCandi[count] += m_window[indice[k]] / m_rtt[indice[k]];
        denoCandi[count] *= denoCandi[count];
        count++;
        for (int l=k+1; l<size; l++)
        {
          denoCandi[count] += m_window[indice[i]] / m_rtt[indice[i]];
          denoCandi[count] += m_window[indice[j]] / m_rtt[indice[j]];
          denoCandi[count] += m_window[indice[k]] / m_rtt[indice[k]];
          denoCandi[count] += m_window[indice[l]] / m_rtt[indice[l]];
          denoCandi[count] *= denoCandi[count];
          count++;
          for (int m=l+1; m<size; m++)
          {
            denoCandi[count] += m_window[indice[i]] / m_rtt[indice[i]];
            denoCandi[count] += m_window[indice[j]] / m_rtt[indice[j]];
            denoCandi[count] += m_window[indice[k]] / m_rtt[indice[k]];
            denoCandi[count] += m_window[indice[l]] / m_rtt[indice[l]];
            denoCandi[count] += m_window[indice[m]] / m_rtt[indice[m]];
            denoCandi[count] *= denoCandi[count];
            count++;
          }
        }
      }
    }
  }


  // minimum denominator extraction
  for(int i=0; i<count; i++)
  {
    if(denoCandi[count] < 
*/
}


}
}
