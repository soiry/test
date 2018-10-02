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


#include "ndn-consumer-window-single.h"
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

NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerWindowSingle");

namespace ns3 {
namespace ndn {
    
NS_OBJECT_ENSURE_REGISTERED (ConsumerWindowSingle);
    
TypeId
ConsumerWindowSingle::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerWindowSingle")
    .SetGroupName ("Ndn")
    .SetParent<Consumer> ()
    .AddConstructor<ConsumerWindowSingle> ()

    .AddAttribute ("Window", "Initial size of the window",
                   StringValue ("3"),
                   MakeUintegerAccessor (&ConsumerWindowSingle::GetWindow, &ConsumerWindowSingle::SetWindow),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("Ssth", "Slow start threshold",
                   StringValue ("0"),
                   MakeUintegerAccessor (&ConsumerWindowSingle::SetSsth),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("PayloadSize", "Average size of content object size (to calculate interest generation rate)",
                   UintegerValue (1040),
                   MakeUintegerAccessor (&ConsumerWindowSingle::GetPayloadSize, &ConsumerWindowSingle::SetPayloadSize),
                   MakeUintegerChecker<uint32_t>())
    .AddAttribute ("Size", "Amount of data in megabytes to request (relies on PayloadSize parameter)",
                   DoubleValue (-1), // don't impose limit by default
                   MakeDoubleAccessor (&ConsumerWindowSingle::GetMaxSize, &ConsumerWindowSingle::SetMaxSize),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Frequency", "Frequency of request for content",
		   StringValue ("1.0"),
		   MakeDoubleAccessor (&ConsumerWindowSingle::m_frequency),
		   MakeDoubleChecker<double> ())
    .AddAttribute ("Randomize", "Type of send time reandomization: none (default), uniform, exponential",
		   StringValue ("none"),
		   MakeStringAccessor (&ConsumerWindowSingle::SetRandomize, &ConsumerWindowSingle::GetRandomize),
		   MakeStringChecker ())
    .AddAttribute ("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ConsumerWindowSingle::m_setInitialWindowOnTimeout),
                   MakeBooleanChecker ())
    .AddAttribute ("Index", "Node index of this consumer",
                   StringValue ("0"),
                   MakeUintegerAccessor (&ConsumerWindowSingle::m_index),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("WindowTrace",
                     "Window that controls how many outstanding interests are allowed",
                     MakeTraceSourceAccessor (&ConsumerWindowSingle::m_window))
    .AddTraceSource ("InFlight",
                     "Current number of outstanding interests",
                     MakeTraceSourceAccessor (&ConsumerWindowSingle::m_inFlight))
    ;

  return tid;
}

ConsumerWindowSingle::ConsumerWindowSingle ()
  : m_payloadSize (1040)
  , m_q (0.0)				//zipf param
  , m_s (1.0)				//zipf param
  , m_SeqRng (0.0, 1.0)		//random rng
  , m_frequency (1.0)		//request interval
  , m_random (0)			//type of request interval
  , m_contentRank (0)		//content number
  , m_initialSsth (0)
  , m_ssth (0)
  , m_windowCount (0)		//to increase window size
  , m_preWindow (0)			//previous window size
  , m_branchSeed (1)		//random seed
  , m_totalContent (0)		//num of received content
  , m_avgCompletionTime (0.0)	//average download completion time
  , m_inFlight (0)
{
  SetNumberOfContents(10000);	//make zipf distribution
  for (int i=0; i<MAX_PATHID; i++) {	//pathId assignment
    m_pathId[i] = i;
  }
  m_rtt->SetMaxRto(Seconds(2));
}

void
ConsumerWindowSingle::WindowOutput(ofstream &ofs)
{
  unsigned int cum = 0;
  for (int i=0; i<MAX_PATHID; i++)
  {
    cum += m_poolCounter[i];
  }
  ofs << m_index << "\t" << Simulator::Now().ToDouble(Time::S) << "\t" << m_window << "\t" << cum << "\n";
}

void
ConsumerWindowSingle::SetWindow (uint32_t window)
{
  NS_LOG_DEBUG("Window is set.");
  m_initialWindow = window;
  m_window = m_initialWindow;
}

void
ConsumerWindowSingle::SetSsth (uint32_t ssth)
{
  m_initialSsth = ssth;
  m_ssth = m_initialSsth;
}

uint32_t
ConsumerWindowSingle::GetWindow () const
{
  return m_window;
}

uint32_t
ConsumerWindowSingle::GetPayloadSize () const
{
  return m_payloadSize;
}

void
ConsumerWindowSingle::SetPayloadSize (uint32_t payload)
{
  m_payloadSize = payload;
}

void
ConsumerWindowSingle::SetRandomize (const std::string &value)
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
ConsumerWindowSingle::GetRandomize () const
{
  return m_randomType;
}

double
ConsumerWindowSingle::GetMaxSize () const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerWindowSingle::SetMaxSize (double size)
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
ConsumerWindowSingle::SetNumberOfContents (uint32_t numOfContents)
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
ConsumerWindowSingle::GetNextSeq()
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
  //if (p_random == 0)
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
  NS_LOG_INFO("m_inFlight: " << m_inFlight);
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
ConsumerWindowSingle::SendPacket ()
{
  if (!m_active) {
	  NS_LOG_INFO("SP(int): m_active is false!!!!!");
	  return;
  }

  //NS_LOG_FUNCTION_NOARGS ();
  m_inFlight++;

  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid

  //NS_LOG_INFO ("retx size: " << m_retxSeqs.size());

  // check retx queue
  while (m_retxSeqs.size ())
  {
	NS_LOG_DEBUG("SP(): retransmission starts!");
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase (m_retxSeqs.begin());
    break;
  }
  // if there's no retx packet
  if (seq == std::numeric_limits<uint32_t>::max ())
  {
    if (m_seq == 0)  // request for next content
    {
      m_contentRank = GetNextSeq();
      m_seq = m_contentRank * m_seqMax;
      m_startTime.insert( map<uint32_t, Time>::value_type( m_contentRank, Simulator::Now() ));
      m_chunkCounter.insert( map<uint32_t, int>::value_type( m_contentRank, 0));
    }
    seq = m_seq++;
  }

  /*
  if (seq > (m_contentRank+1) * m_seqMax)
  {
	m_inFlight--;
	return;
  }
  */
  
  Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_interestName);
  (*nameWithSequence) (seq);

  InterestHeader interestHeader;
  interestHeader.SetNonce               (m_rand.GetValue ());
  interestHeader.SetName                (nameWithSequence);

  int pathId = seq % MAX_PATHID;
  for (int i=0; i<MAX_PATHID; i++)
  {
    interestHeader.SetPathId(pathId);
  }
  m_branchIntMap.insert( map<uint32_t, int>::value_type(seq, pathId));
      
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (interestHeader);
  m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqFullDelay.insert (SeqTimeout (seq, Simulator::Now ()));

  m_seqLastDelay.erase (seq);
  m_seqLastDelay.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqRetxCounts[seq] ++;

  m_transmittedInterests (&interestHeader, this, m_face);

  m_rtt->SentSeq (SequenceNumber32 (seq), 1);

  FwHopCountTag hopCountTag;
  packet->AddPacketTag (hopCountTag);

  m_protocolHandler (packet);

  //NS_LOG_DEBUG ("pathId: " << pathId << ", m_window: " << m_window << ", m_inFlight: " << m_inFlight);
  
  ScheduleNextPacket ();
}

void
ConsumerWindowSingle::ScheduleNextPacket ()
{
  // new download starts
  if (m_seq == static_cast<uint32_t> (0))
  {
	NS_LOG_DEBUG("SP(): m_seq zero! new content download starts!");
	Simulator::Remove (m_sendEvent);
	m_retxSeqs.clear();
	m_branchIntMap.clear();
	m_nextBranchInt.clear();
	m_seqLastDelay.clear();
	m_seqFullDelay.clear();
	m_seqRetxCounts.clear();
	m_ssth = m_initialSsth;
	m_windowCount = 0;
	m_startTime.clear();
	m_chunkCounter.clear();

        mt19937 gen(m_index);
        uniform_int<> dst(0, 1);
        variate_generator< mt19937, uniform_int<> > rand(gen,dst);
        // randomized interval
        //int interval = rand();

        //fixed interval
        int interval = 1;

	//m_sendEvent = Simulator::Schedule (Seconds (100*m_totalContent - Simulator::Now().ToDouble(Time::S)), &ConsumerWindowSingle::SendPacket, this);
	//m_sendEvent = Simulator::Schedule (Seconds (0.5), &ConsumerWindowSingle::SendPacket, this);
	m_sendEvent = Simulator::Schedule (Seconds (interval), &ConsumerWindowSingle::SendPacket, this);
	//m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S))), &ConsumerWindowSingle::SendPacket, this);

	m_window = m_initialWindow;
	m_inFlight = 0;
  }

  // if it is not new download
  else
  {
    if (m_inFlight >= m_window)
    {
	  // simply do nothing
    }
    else
    {
      if (m_sendEvent.IsRunning ())
      {
  	    Simulator::Remove (m_sendEvent);
      }
	  m_sendEvent = Simulator::ScheduleNow (&ConsumerWindowSingle::SendPacket, this);
    }
  }
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerWindowSingle::OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                                     Ptr<Packet> payload)
{
  if (m_seq == 0)
  {
    return;
  }
  uint32_t seq = boost::lexical_cast<uint32_t> (contentObject->GetName ().GetComponents ().back ());
  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find (seq);
  double chunkRTT = entry->time.ToDouble(Time::S);
  chunkRTT = Simulator::Now().ToDouble(Time::S) - chunkRTT;

  if (Simulator::Now().ToDouble(Time::S) > 490) {
    char fileName[50];
    sprintf(fileName, "%d_branchStats_single.txt", m_index);
    m_ofs2.open(fileName, ios::out);
    m_ofs2 << "#" << "\tCounts" << "\tavgRTT\n";
    for(int i=0; i<MAX_PATHID; i++) {
      m_ofs2 << i << "\t" << m_poolCounter[i] << "\t" << m_poolDelay[i] / m_poolCounter[i] << "\n";
    }
    m_ofs2.close();
  }

  Consumer::OnContentObject (contentObject, payload);

  // window size increase occurs when subscriber receive some amount of chunks
  // this some amount is current window size in this code(can be changed)

  // congestion avoidance
  if (m_ssth < m_window)
  {
    m_windowCount = m_windowCount + 1;
    if (m_windowCount >= m_window)
    {
      m_window = m_window+1;
      m_windowCount = 0;
	  if(m_window % 5 == (uint32_t) 0)
	  {
		char fileName[30];
        sprintf(fileName, "%d_window_single.txt", m_index);
        m_ofs.open(fileName, ios::app);
        WindowOutput(m_ofs);
        m_ofs.close();
	  }
    }
  }
  // slow-start
  else
  {
    m_window++;
	m_windowCount = 0;
	if (m_window % 5 == (uint32_t) 0)
	{
      char fileName[30];
      sprintf(fileName, "%d_window_single.txt", m_index);
      m_ofs.open(fileName, ios::app);
      WindowOutput(m_ofs);
      m_ofs.close();
	}
  }

  if (m_inFlight > static_cast<uint32_t> (0)) {
    m_inFlight--;
  }


  map<uint32_t, int>::iterator findIter = m_branchIntMap.find(seq);
  if(findIter == m_branchIntMap.end())
  {
	  NS_LOG_DEBUG("No matching in branchIntMap");
  }
  else
  {
    m_branchIntMap.erase(seq);
    m_poolCounter[findIter->second]++;
    m_poolDelay[findIter->second] += chunkRTT;
  }

  map<uint32_t, uint32_t>::iterator findIter2 = m_chunkCounter.find(m_contentRank);

  //NS_LOG_DEBUG ("m_window: " << m_window << ", m_windowCount: " << m_windowCount << ", m_inFlight: " << m_inFlight << ", chunkCount: " << findIter2->second);

  if(findIter2 == m_chunkCounter.end())
  {
	  NS_LOG_DEBUG("No matching in chunkCounter");
	  if(m_seq == (uint32_t) 0)
	  {
		// new content download will starts.
	  }
	  else
	  {
		// new content download already starts.
	  }
  }
  else
  {
    findIter2->second++;
  
    if (findIter2->second >= m_seqMax)
    {
      NS_LOG_DEBUG("Content download ends: contentID" << m_contentRank);
      map<uint32_t, Time>::iterator findIter3 = m_startTime.find(m_contentRank);
      if (findIter3 == m_startTime.end())
      {
        NS_LOG_DEBUG("No matching in startTime");
      }
      double timeTemp = findIter3->second.ToDouble(Time::S);
      timeTemp = Simulator::Now().ToDouble(Time::S) - timeTemp;
      m_avgCompletionTime = ((double)m_totalContent/(m_totalContent+1)) * m_avgCompletionTime + ((double)1/(m_totalContent+1)) * timeTemp;
      cout << m_index << "\tcontentNum: " << m_contentRank << "\ttimeTemp: " << timeTemp << ", contentTotal: " << m_totalContent << ", avg comp time: " << m_avgCompletionTime << ", m_window: " << m_window << "\n";
      m_totalContent++;
      m_startTime.erase(m_contentRank);
      m_chunkCounter.erase(m_contentRank);
      // m_seq == 0 ===> start new content download
      m_seq = 0;
    }
    ScheduleNextPacket();
  }
}

void
ConsumerWindowSingle::OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> payload)
{
  Consumer::OnNack(interest, payload);

  if (m_inFlight > static_cast<uint32_t> (0))
  {
    m_inFlight--;
  }

  if (m_window > static_cast<uint32_t> (0))
  {
      //if (m_totalWindow > uint32_t(1)) m_totalWindow=m_totalWindow-1;
      //m_windowCount = 0;
  }
  NS_LOG_DEBUG ("Nack: Window: " << m_window << ", inFlight: " << m_inFlight);
}
/*
void
ConsumerWindow::OnTimeout (uint32_t contentRank, uint32_t sequenceNumber)
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
ConsumerWindowSingle::OnTimeout (uint32_t sequenceNumber)
{
  if (m_seq == (uint32_t)0)
  {
    return;
  }
  //NS_LOG_DEBUG("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&OT Called&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&");
  if (m_inFlight > static_cast<uint32_t> (0)) {
    m_inFlight--;
  }

  if (m_setInitialWindowOnTimeout)
  {
    //if(m_window > (uint32_t)1 && m_window > m_preWindow*3/5)
    if(m_window > (uint32_t)1)
    {
	  m_preWindow = m_window;
	  m_window /= 2;
	  m_ssth = m_window;
	  m_windowCount = 0;

//    m_window = m_initialWindow;
//    m_windowCount = 0;
    }
    char fileName[30];
    sprintf(fileName, "%d_window_single.txt", m_index);
    m_ofs.open(fileName, ios::app);
    WindowOutput(m_ofs);
    m_ofs.close();

    NS_LOG_DEBUG ("Timeout: index: " << m_index << ", m_window: " << m_window << ", m_inFlight: " << m_inFlight);

    Consumer::OnTimeout (sequenceNumber);
    ScheduleNextPacket();
  }
}

uint32_t
ConsumerWindowSingle::GetMaxIndex(uint32_t* src)
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

}
}
