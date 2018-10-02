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


#include "ndn-consumer-window-mic.h"
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

NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerWindowMic");

namespace ns3 {
namespace ndn {
    
NS_OBJECT_ENSURE_REGISTERED (ConsumerWindowMic);
    
TypeId
ConsumerWindowMic::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerWindowMic")
    .SetGroupName ("Ndn")
    .SetParent<Consumer> ()
    .AddConstructor<ConsumerWindowMic> ()

    .AddAttribute ("Window", "Initial size of the window",
                   StringValue ("5"),
                   MakeUintegerAccessor (&ConsumerWindowMic::SetWindow),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("SlowStartThreshold", "Initial ssth",
				   StringValue ("0"),
				   MakeUintegerAccessor (&ConsumerWindowMic::SetSsth),
				   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("PathWindow", "Initial # of the paths",
				   StringValue ("5"),
				   MakeUintegerAccessor (&ConsumerWindowMic::SetPathWindow),
				   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("PayloadSize", "Average size of content object size (to calculate interest generation rate)",
                   UintegerValue (1040),
                   MakeUintegerAccessor (&ConsumerWindowMic::GetPayloadSize, &ConsumerWindowMic::SetPayloadSize),
                   MakeUintegerChecker<uint32_t>())
    .AddAttribute ("Size", "Amount of data in megabytes to request (relies on PayloadSize parameter)",
                   DoubleValue (-1), // don't impose limit by default
                   MakeDoubleAccessor (&ConsumerWindowMic::GetMaxSize, &ConsumerWindowMic::SetMaxSize),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Frequency", "Frequency of request for content",
				   StringValue ("1.0"),
				   MakeDoubleAccessor (&ConsumerWindowMic::m_frequency),
				   MakeDoubleChecker<double> ())
    .AddAttribute ("Randomize", "Type of send time reandomization: none (default), uniform, exponential",
				   StringValue ("none"),
				   MakeStringAccessor (&ConsumerWindowMic::SetRandomize, &ConsumerWindowMic::GetRandomize),
				   MakeStringChecker ())
    .AddAttribute ("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ConsumerWindowMic::m_setInitialWindowOnTimeout),
                   MakeBooleanChecker ())
    .AddAttribute ("Index", "Node index of this consumer",
                   StringValue ("0"),
                   MakeUintegerAccessor (&ConsumerWindowMic::m_index),
                   MakeUintegerChecker<uint32_t> ())
/*
	.AddAttribute ("RetxTimer", "Timeout defining how frequent retx timouts should be checked",
			       StringValue("50ms"),
				   MakeTimeAccessor (&ConsumerWindowMic::GetRetxTimer, &ConsumerWindowMic::SetRetxTimer),
				   MakeTimeChecker ())
*/
    .AddTraceSource ("WindowTrace",
                     "Window that controls how many outstanding interests are allowed",
                     MakeTraceSourceAccessor (&ConsumerWindowMic::m_totalWindow))
    .AddTraceSource ("InFlight",
                     "Current number of outstanding interests",
                     MakeTraceSourceAccessor (&ConsumerWindowMic::m_totalInFlight))
    ;

  return tid;
}

ConsumerWindowMic::ConsumerWindowMic ()
  : m_payloadSize (1040)
  , m_q (0.0)				//zipf param
  , m_s (1.0)				//zipf param
  , m_SeqRng (0.0, 1.0)		//random rng
  , m_frequency (1.0)		//request interval
  , m_random (0)			//type of request interval
  , m_contentRank (0)		//content number
  , m_initialWindow (3)		//
  , m_initialSsth(0)
  , m_snpCalled (true)		//flag for next content retrieval
  , m_nm(0)					//# of maximum windows
  , m_nb(0)					//# of best throughputs
//  , m_windowFraction (0)		//to increase window size
  , m_initialPathWindow (3) //
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
	m_l1[i] = 0;
	m_l2[i] = 0;
	m_l[i] = 0;
	m_r1[i] = 0;
	m_r2[i] = 0;
	m_m[i] = 0;
	m_b[i] = 0;
	m_inFlight[i] = 0;
	m_windowFraction[i] = 0;
	m_timeoutCount[i] = 0;
	//cout << i << ":\t";
    m_pathId[i] = i;
	m_trajNum[i] = -1;
	m_rtt[i] = CreateObject<RttMeanDeviation> ();
        m_rtt[i]->SetMaxRto(Seconds(2));
  }
}

void
ConsumerWindowMic::SetRetxTimer (Time retxTimer)
{
  m_retxTimer = retxTimer;
  NS_LOG_DEBUG("SRT(): RetxTimer is set: " << m_retxTimer);
  if (m_retxEvent.IsRunning ())
  {
    // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
	Simulator::Remove (m_retxEvent); // slower, but better for memory
  }
  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule (m_retxTimer, &ConsumerWindowMic::CheckRetxTimeout, this);
}

Time
ConsumerWindowMic::GetRetxTimer () const
{
  return m_retxTimer;
}

void
ConsumerWindowMic::CheckRetxTimeout ()
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
	    OnTimeout (seqNo);
	  }
      else
	  {
	    break; // nothing else to do. All later packets need not be retransmitted
	  }
	}
  }
  m_retxEvent = Simulator::Schedule (m_retxTimer, &ConsumerWindowMic::CheckRetxTimeout, this); 
}

void
ConsumerWindowMic::WindowOutput(ofstream &ofs)
{
  unsigned int cum = 0; 
  for(int i=0; i<MAX_PATHID; i++)
  {
    cum += m_poolCounter[i];
  }
  ofs << m_index << "\t" << Simulator::Now().ToDouble(Time::S) << "\t" << m_totalWindow << "\t" << cum << "\n";
}

void
ConsumerWindowMic::SetWindow (uint32_t window)
{
  m_initialWindow = window;
  m_totalWindow = m_initialWindow;
  for(int i=0; i<MAX_PATHID; i++)
  {
	m_window[i] = 0;
  }
}

uint32_t
ConsumerWindowMic::GetWindow () const
{
  return m_totalWindow;
}

void
ConsumerWindowMic::SetSsth (uint32_t ssth)
{
  m_initialSsth = ssth;
  for (int i=0; i < MAX_PATHID; i++)
  {
    m_ssth[i] = m_initialSsth;
  }
}

void
ConsumerWindowMic::SetPathWindow (uint32_t pathWindow)
{
  m_initialPathWindow = pathWindow;
  m_pathWindow = m_initialPathWindow;
}

uint32_t
ConsumerWindowMic::GetPathWindow () const
{
  return m_pathWindow;
}

uint32_t
ConsumerWindowMic::GetPayloadSize () const
{
  return m_payloadSize;
}

void
ConsumerWindowMic::SetPayloadSize (uint32_t payload)
{
  m_payloadSize = payload;
}

void
ConsumerWindowMic::SetRandomize (const std::string &value)
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
ConsumerWindowMic::GetRandomize () const
{
  return m_randomType;
}

double
ConsumerWindowMic::GetMaxSize () const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerWindowMic::SetMaxSize (double size)
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
ConsumerWindowMic::SetNumberOfContents (uint32_t numOfContents)
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
ConsumerWindowMic::GetNextSeq()
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
ConsumerWindowMic::SendPacket (int pathId)
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
//  NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");
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

  //NS_LOG_DEBUG ("pathId: " << pathId << ", m_totalWindow: " << m_totalWindow << ", m_totalInFlight: " << m_totalInFlight << ", m_window[pathId]: " << m_window[pathId] << ", m_inFlight[pathId]: " << m_inFlight[pathId]);
  
  ScheduleNextPacket (pathId);
}


void
ConsumerWindowMic::ScheduleNextPacket (int pathId)
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
      m_sendEvent = Simulator::ScheduleNow (&ConsumerWindowMic::SendPacket, this, pathId);
	}
  }
}

void
ConsumerWindowMic::ScheduleNextPacket ()
{
  Simulator::Remove(m_sendEvent);
  m_snpCalled = true;
  // select new pathId
  mt19937 gen(m_branchSeed);
  uniform_int<> dst(0,MAX_PATHID);
  variate_generator< mt19937, uniform_int<> > rand(gen, dst);

  mt19937 in(m_index);
  uniform_int<> inter(60,720);
  variate_generator< mt19937, uniform_int<> > rand2(in, inter);

  m_totalWindow = m_initialWindow;
  m_pathWindow = m_initialPathWindow;
  NS_LOG_DEBUG("SNP(): Request for new content, m_tW: " << m_totalWindow << ", m_pW: " << m_pathWindow);
  m_totalInFlight = 0;
  m_pathCount = 0;
  m_totalTimeoutCount = 0;
  m_nm = 0;
  m_nb = 0;
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
    m_l1[i] = 0;
    m_l2[i] = 0;
    m_l[i] = 0;
    m_r1[i] = 0;
    m_r2[i] = 0;
    m_m[i] = 0;
    m_b[i] = 0;
    m_windowFraction[i] = 0;
    m_inFlight[i] = 0;
    m_timeoutCount[i] = 0;
    m_pathId[i] = i;
    m_trajNum[i] = -1;
    m_rtt[i]->Reset ();
    m_rtt[i]->SetMaxRto(Seconds(2));
    m_retxSeqs[i].clear();
  }

  int pathId = rand() % MAX_PATHID;

  //randomize
  //int pathId = (m_seq+1) % MAX_PATHID;
	  
  //include best path
  //int pathId = 8 * (int)(rand() % (MAX_PATHID / 8));
  uint32_t temp = 0;
  //randomize interval
  //int interval = rand2();

  //fixed interval
  int interval = 1;
  while(m_pathWindow > temp)
  {
    if (m_window[pathId] == (uint32_t)0)
    {
      m_window[pathId]++;
      //m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5 + tmp + tmp2, m_rtt[pathId]->RetransmitTimeout().ToDouble(Time::S) + tmp + tmp2)), &ConsumerWindowMic::SendPacket, this, pathId);
      //m_sendEvent = Simulator::ScheduleNow (&ConsumerWindowMic::SendPacket, this, pathId);
      m_sendEvent = Simulator::Schedule (Seconds (interval), &ConsumerWindowMic::SendPacket, this, pathId);
      //m_sendEvent = Simulator::Schedule (Seconds (100*m_totalContent - Simulator::Now().ToDouble(Time::S)), &ConsumerWindowMic::SendPacket, this, pathId);
      //m_sendEvent = Simulator::Schedule (Seconds (0.5), &ConsumerWindowMic::SendPacket, this, pathId);
      temp++;
      //tmp += tmp2;
    } else {
      pathId++;
      //pathId = rand() % MAX_PATHID;
      if (pathId >= MAX_PATHID)
      {
        pathId = 0;
      }
    }
  }
  NS_LOG_DEBUG("SNP(): Request for new content ends");
}


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerWindowMic::OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                                     Ptr<Packet> payload)
{

  uint32_t seq = boost::lexical_cast<uint32_t> (contentObject->GetName ().GetComponents ().back ());

  // find pool index of corresponding chunk.
  map<uint32_t, int>::iterator findIter = m_branchIntMap.find(seq);
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
	  return;
  }
  else if (m_snpCalled == true)
  {
	  NS_LOG_DEBUG("OC(): snpCalled is true");
  }
  // if there's a matching entry in branchIntMap
  // this means, content transmission is now on-going
  else {
    int poolIndex = findIter->second;
	int trajectory = contentObject->GetHash ();
	m_trajNum[poolIndex] = trajectory;
	int newIndex = FindArrayIndex(m_trajNum, trajectory);
	if (newIndex == -1)
	{
	  
	}
	else if (poolIndex == newIndex)
	{
	  
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
	  m_windowFraction[poolIndex] = 0;
	  poolIndex = newIndex;
	}

	SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find (seq);
    double chunkRTT = entry->time.ToDouble(Time::S);
    chunkRTT = Simulator::Now().ToDouble(Time::S) - chunkRTT;

    if (Simulator::Now().ToDouble(Time::S) > 490) {
      char fileName[50];
      sprintf(fileName, "%d_branchStats_mic.txt", m_index);
      m_ofs2.open(fileName, ios::out);
      m_ofs2 << "#\tCounts\tavgRTT\t" << Simulator::Now().ToDouble(Time::S) << "\n";
      for(int i=0; i<MAX_PATHID; i++) {
        m_ofs2 << i << "\t" << m_poolCounter[i] << "\t" << m_poolDelay[i] / m_poolCounter[i] << "\n";
      }
      m_ofs2.close();
    }
	m_nextBranchInt.push_back(poolIndex);
	m_branchIntMap.erase(seq);

	// OLIA part
	m_l2[poolIndex]++;
	m_r2[poolIndex] = chunkRTT;
	if(m_r1[poolIndex] == 0) {
		m_l[poolIndex] = m_l2[poolIndex] / m_r2[poolIndex] / m_r2[poolIndex];
	} else if (m_r2[poolIndex] == 0) {
		m_l[poolIndex] = 0;
	} else {
		m_l[poolIndex] = ( (m_l2[poolIndex]/m_r2[poolIndex]/m_r2[poolIndex]) >= (m_l1[poolIndex]/m_r1[poolIndex]/m_r1[poolIndex]) ? m_l2[poolIndex]/m_r2[poolIndex]/m_r2[poolIndex] : m_l1[poolIndex]/m_r1[poolIndex]/m_r1[poolIndex]);
	}
	uint32_t temp_window[MAX_PATHID];
	for(int y=0; y<MAX_PATHID; y++)
	{
		temp_window[y] = m_window[y];
	}
	m_nm = GetMaxIndices(temp_window, m_m);
	m_nb = GetMaxIndicesDouble(m_l, m_b);
	double alpha = 0;
	if (IsInArray(m_m, poolIndex))
	{
		if(IsInArray(m_b, poolIndex)) // the path is max and best
		{
			alpha = 0;
			//NS_LOG_INFO("M & B");
		}
		else // the path is max but not best
		{
			alpha = -(((double)1/m_pathWindow)/(double)m_nm);
			//NS_LOG_INFO("M & NB");
		}
	}
	else
	{
		if(IsInArray(m_b, poolIndex)) // the path is not max but best
		{
			double below = m_nb;
			below -= IsDuplicated(m_b, m_nb, m_m, m_nm);
			alpha = ((double)1/m_pathWindow) / below;
			//NS_LOG_INFO("NM & B");
		}
		else // the path is not max and not best
		{
			alpha = 0;
			//NS_LOG_INFO("NM & NB");
		}
	}

	double denominator = 0;
	for (int y=0; y<MAX_PATHID; y++)
	{
		if(m_window[y] == (uint32_t)0) continue;
		else if(m_poolDelay[y] == 0) continue;
		denominator = denominator + (m_window[y] / (m_poolDelay[y]/m_poolCounter[y]));
	}
	denominator = denominator * denominator;
	double numerator = 0;
	numerator = ( m_window[poolIndex] / ((m_poolDelay[poolIndex]/m_poolCounter[poolIndex]) * (m_poolDelay[poolIndex]/m_poolCounter[poolIndex])));

	if(denominator == 0) {
		m_windowFraction[poolIndex] = 0;
	} else {
		m_windowFraction[poolIndex] = (double)m_windowFraction[poolIndex] + ((double)numerator / denominator) + (alpha / (double)m_window[poolIndex]);
	}
	//NS_LOG_INFO("windowFraction: " << m_windowFraction[poolIndex] << ", alpha: " << alpha << ", denominator: " << denominator << ", numerator :" << numerator << ", m_nm: " << m_nm << ", m_nb: " << m_nb);
  
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
          m_windowFraction[poolIndex] = 0;
          if (m_totalWindow % 5 == (uint32_t)0)
		  {
            char fileName[30];
            sprintf(fileName, "%d_window_mic.txt", m_index);
            m_ofs.open(fileName, ios::app);
            WindowOutput(m_ofs);
            m_ofs.close();
	 	  }
		  /* path increase
	      m_pathCount = m_pathCount+1;
	      if(m_pathCount >= m_pathWindow) {
		    // increasing path window
		    m_pathWindow = m_pathWindow+1;
		    m_pathCount = 0;
		    int branchTemp = (seq+1) % MAX_PATHID;
		    while(m_window[branchTemp] > (uint32_t)0) {
			  branchTemp++;
			  if(branchTemp >= MAX_PATHID)
		      {
			  branchTemp = 0;
			  }
		    }

		    // sending packet through new path
		    m_window[branchTemp]++;
		    m_totalWindow++;
		    NS_LOG_INFO("A new path is added. pathId: " << branchTemp << ", m_window[pathId]: "<<m_window[branchTemp]);
		    //Simulator::ScheduleNow (&ConsumerWindowMic::SendPacket, this, branchTemp);

		    ScheduleNextPacket(branchTemp);
	      }
		  */
		}
	  }
	  // slow-start phase
	  else
	  {
	    m_window[poolIndex]++;
		m_totalWindow++;
		m_windowFraction[poolIndex] = 0;
        if (m_totalWindow % 5 == (uint32_t)0)
		{
          char fileName[30];
          sprintf(fileName, "%d_window_mic.txt", m_index);
          m_ofs.open(fileName, ios::app);
          WindowOutput(m_ofs);
          m_ofs.close();
		}
		/* path increase
	    m_pathCount = m_pathCount+1;
	    if(m_pathCount >= m_pathWindow) {
		  // increasing path window
		  m_pathWindow = m_pathWindow+1;
		  m_pathCount = 0;
		  //m_totalTimeoutCount = 0; // is this needed?
		  //for(int i=0; i<MAX_PATHID; i++) {
		  //	m_timeoutCount[i] = 0;
		  //}
	      // selecting new path
    
	      //mt19937 gen(m_branchSeed);
          //uniform_int<> dst(0,MAX_PATHID);
          //variate_generator< mt19937, uniform_int<> > rand(gen, dst);
		  //int branchTemp = rand();
		    
		  int branchTemp = (seq+1) % MAX_PATHID;
		  while(m_window[branchTemp] > (uint32_t)0) {
		  branchTemp++;
		  if(branchTemp >= MAX_PATHID)
		  {
		    branchTemp = 0;
	 	  }
		}

		    // sending packet through new path
		    m_window[branchTemp]++;
		    m_totalWindow++;
		    NS_LOG_INFO("A new path is added. pathId: " << branchTemp << ", m_window[pathId]: "<<m_window[branchTemp]);
		  //Simulator::ScheduleNow (&ConsumerWindowMic::SendPacket, this, branchTemp);

		    ScheduleNextPacket(branchTemp);
	      }
		  */
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
		*/
	  }
	}
  }
}

void
ConsumerWindowMic::OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> payload)
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
ConsumerWindowMic::OnTimeout (uint32_t contentRank, uint32_t sequenceNumber)
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
ConsumerWindowMic::OnTimeout (uint32_t sequenceNumber)
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
	  m_windowFraction[poolIndex] = 0;

	  if(m_window[poolIndex] > (uint32_t)0)
  	  {
	    m_totalTimeoutCount++;
        m_timeoutCount[poolIndex]++;
	  }
    }

	//OLIA part
	for (int i=0; i<MAX_PATHID; i++)
	{
		m_l1[i] = m_l2[i];
		m_l2[i] = 0;
		m_r1[i] = m_r2[i];
		m_r2[i] = 0;
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
    sprintf(fileName, "%d_window_mic.txt", m_index);
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
ConsumerWindowMic::GetMaxIndex(uint32_t* src)
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
ConsumerWindowMic::GetMaxIndices(uint32_t* src, uint32_t* dst)
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
ConsumerWindowMic::GetMaxIndicesDouble(double* src, uint32_t* dst)
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
ConsumerWindowMic::IsInArray(uint32_t* src, uint32_t value)
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
ConsumerWindowMic::FindArrayIndex(int* src, int value)
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
ConsumerWindowMic::IsDuplicated(uint32_t* src1, uint32_t src1Length, uint32_t* src2, uint32_t src2Length)
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

}
}
