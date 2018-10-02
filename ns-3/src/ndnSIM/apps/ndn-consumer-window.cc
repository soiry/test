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


#include "ndn-consumer-window.h"
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

NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerWindow");

namespace ns3 {
namespace ndn {
    
NS_OBJECT_ENSURE_REGISTERED (ConsumerWindow);
    
TypeId
ConsumerWindow::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerWindow")
    .SetGroupName ("Ndn")
    .SetParent<Consumer> ()
    .AddConstructor<ConsumerWindow> ()

    .AddAttribute ("Window", "Initial size of the window",
                   StringValue ("5"),
                   MakeUintegerAccessor (&ConsumerWindow::SetWindow),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("SlowStartThreshold", "Initial ssth",
				   StringValue ("0"),
				   MakeUintegerAccessor (&ConsumerWindow::SetSsth),
				   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("PathWindow", "Initial # of the paths",
				   StringValue ("5"),
				   MakeUintegerAccessor (&ConsumerWindow::SetPathWindow),
				   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("PayloadSize", "Average size of content object size (to calculate interest generation rate)",
                   UintegerValue (1040),
                   MakeUintegerAccessor (&ConsumerWindow::GetPayloadSize, &ConsumerWindow::SetPayloadSize),
                   MakeUintegerChecker<uint32_t>())
    .AddAttribute ("Size", "Amount of data in megabytes to request (relies on PayloadSize parameter)",
                   DoubleValue (-1), // don't impose limit by default
                   MakeDoubleAccessor (&ConsumerWindow::GetMaxSize, &ConsumerWindow::SetMaxSize),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Frequency", "Frequency of request for content",
				   StringValue ("1.0"),
				   MakeDoubleAccessor (&ConsumerWindow::m_frequency),
				   MakeDoubleChecker<double> ())
    .AddAttribute ("Randomize", "Type of send time reandomization: none (default), uniform, exponential",
				   StringValue ("none"),
				   MakeStringAccessor (&ConsumerWindow::SetRandomize, &ConsumerWindow::GetRandomize),
				   MakeStringChecker ())
    .AddAttribute ("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ConsumerWindow::m_setInitialWindowOnTimeout),
                   MakeBooleanChecker ())
    .AddAttribute ("Index", "Node index of this consumer",
                   StringValue ("0"),
                   MakeUintegerAccessor (&ConsumerWindow::m_index),
                   MakeUintegerChecker<uint32_t> ())

    .AddTraceSource ("WindowTrace",
                     "Window that controls how many outstanding interests are allowed",
                     MakeTraceSourceAccessor (&ConsumerWindow::m_totalWindow))
    .AddTraceSource ("InFlight",
                     "Current number of outstanding interests",
                     MakeTraceSourceAccessor (&ConsumerWindow::m_totalInFlight))
    ;

  return tid;
}

ConsumerWindow::ConsumerWindow ()
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
//  , m_windowCount (0)		//to increase window size
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
	m_inFlight[i] = 0;
	m_windowCount[i] = 0;
	m_timeoutCount[i] = 0;
    m_pathId[i] = i;
  }
}

void
ConsumerWindow::WindowOutput(ofstream &ofs)
{
  ofs << m_index << "\t" << Simulator::Now().ToDouble(Time::S) << "\t" << m_totalWindow << "\n";
}

void
ConsumerWindow::SetWindow (uint32_t window)
{
  m_initialWindow = window;
  m_totalWindow = m_initialWindow;
  for(int i=0; i<MAX_PATHID; i++)
  {
	m_window[i] = 0;
  }
}

uint32_t
ConsumerWindow::GetWindow () const
{
  return m_totalWindow;
}

void
ConsumerWindow::SetSsth (uint32_t ssth)
{
  m_initialSsth = ssth;
  for (int i=0; i < MAX_PATHID; i++)
  {
    m_ssth[i] = m_initialSsth;
  }
}

void
ConsumerWindow::SetPathWindow (uint32_t pathWindow)
{
  m_initialPathWindow = pathWindow;
  m_pathWindow = m_initialPathWindow;
}

uint32_t
ConsumerWindow::GetPathWindow () const
{
  return m_pathWindow;
}

uint32_t
ConsumerWindow::GetPayloadSize () const
{
  return m_payloadSize;
}

void
ConsumerWindow::SetPayloadSize (uint32_t payload)
{
  m_payloadSize = payload;
}

void
ConsumerWindow::SetRandomize (const std::string &value)
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
ConsumerWindow::GetRandomize () const
{
  return m_randomType;
}

double
ConsumerWindow::GetMaxSize () const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerWindow::SetMaxSize (double size)
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
ConsumerWindow::SetNumberOfContents (uint32_t numOfContents)
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
ConsumerWindow::GetNextSeq()
{
  uint32_t content_index = 1; //[1, m_N]
  double p_sum = 0;

  double p_random = m_SeqRng.GetValue();
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
ConsumerWindow::SendPacket (int pathId)
{
  if (!m_active) {
	  NS_LOG_INFO("SP(int): m_active is false!!!!!");
	  return;
  }

  //NS_LOG_FUNCTION_NOARGS ();
  m_totalInFlight++;
  m_inFlight[pathId]++;
  /*
  if(m_inFlight[pathId] > m_window[pathId]) {
	  m_totalInFlight--;
	  m_inFlight[pathId]--;
	  return;
  }
  */

  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid

  //NS_LOG_INFO ("retx size: " << m_retxSeqs.size());

  while (m_retxSeqs.size ())
  {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase (m_retxSeqs.begin());
    break;
  }
  if (m_snpCalled == true)
  {
    m_contentRank = GetNextSeq();
    m_seq = m_contentRank * m_seqMax;
    m_startTime.insert( map<uint32_t, Time>::value_type( m_contentRank, Simulator::Now() ));
    m_chunkCounter.insert( map<uint32_t, int>::value_type( m_contentRank, 0));
    m_snpCalled = false;
  }
  if (seq == std::numeric_limits<uint32_t>::max ())
  {
    if (m_seqMax != std::numeric_limits<uint32_t>::max ())
    {
	  /*
      if (m_seq % m_seqMax == 0)  // request for next content
      {
		if(m_totalWindow != m_initialWindow) {
		 m_inFlight[pathId]--;
	      m_totalInFlight--;
		  return;
		}
        m_totalWindow = m_initialWindow;
		m_pathWindow = m_initialPathWindow;
		m_pathCount = 0;
		m_totalTimeoutCount = 0;
		for (int i=0; i<MAX_PATHID; i++) {
		  m_window[i] = 0;
		  m_windowCount[i] = 0;
		}
		if (m_snpCalled == true)
		{
          m_contentRank = GetNextSeq();
          m_seq = m_contentRank * m_seqMax;
          m_startTime.insert( map<uint32_t, Time>::value_type( m_contentRank, Simulator::Now() ));
          m_chunkCounter.insert( map<uint32_t, int>::value_type( m_contentRank, 0));
		  m_snpCalled = false;
		}
      }
	  */
    }
	/*
    if (m_snpCalled == true)
	{
      m_contentRank = GetNextSeq();
      m_seq = m_contentRank * m_seqMax;
      m_startTime.insert( map<uint32_t, Time>::value_type( m_contentRank, Simulator::Now() ));
      m_chunkCounter.insert( map<uint32_t, int>::value_type( m_contentRank, 0));
	  m_snpCalled = false;
	}
	*/
    seq = m_seq++;
  }
  
  Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_interestName);
  (*nameWithSequence) (seq);

  InterestHeader interestHeader;
  interestHeader.SetNonce               (m_rand.GetValue ());
  interestHeader.SetName                (nameWithSequence);

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

  m_rtt->SentSeq (SequenceNumber32 (seq), 1);

  FwHopCountTag hopCountTag;
  packet->AddPacketTag (hopCountTag);

  m_protocolHandler (packet);

  NS_LOG_DEBUG ("pathId: " << pathId << ", m_totalWindow: " << m_totalWindow << ", m_totalInFlight: " << m_totalInFlight << ", m_window[pathId]: " << m_window[pathId] << ", m_inFlight[pathId]: " << m_inFlight[pathId]);
  
  ScheduleNextPacket (pathId);
}


void
ConsumerWindow::ScheduleNextPacket (int pathId)
{
  NS_LOG_INFO("SNP(int): Request for consequent chunks. branch: "<<pathId << ", m_totalWindow: " << m_totalWindow << ", m_totalInFlight: " << m_totalInFlight << ", m_window[]: " << m_window[pathId] << ", m_inFligt[]: " << m_inFlight[pathId]);
  if (m_totalWindow == static_cast<uint32_t> (0))
  {
    //Simulator::Remove (m_sendEvent);

    // if next interest is for next content, call SNP()
    // and window size for next content must be initialized
    if ((m_seq % m_seqMax == 0)  && (m_retxSeqs.size() < 1) && (m_seq!=0))
    {
      //double tmp = (m_random == 0) ? 1.0/m_frequency : m_random->GetValue();
      //m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5 + tmp, m_rtt->RetransmitTimeout().ToDouble(Time::S) + tmp)), &ConsumerWindow::SendPacket(pathId), this);
      //m_totalWindow = m_initialWindow;
	  //for(int i=0; i<MAX_PATHID; i++) {
	//	  m_window[i] = 0;
	//	  m_windowCount[i] = 0;
	// }
	  //ScheduleNextPacket();
    }
    else
	{
	  NS_LOG_DEBUG("SNP(int): Total window size is 0, but not new content.\n");
	  // m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5, m_rtt->RetransmitTimeout ().ToDouble (Time::S))), &ConsumerWindow::SendPacket(pathId), this);
	}
  }
  else
  {
    if (m_inFlight[pathId] >= m_window[pathId]) {
	  NS_LOG_DEBUG("SNP(int): m_inFlight[] >= m_window[]");
		// simply do nothing
	}
	else if (m_window[pathId] == (uint32_t)0) {
	  NS_LOG_DEBUG("SNP(int): m_window[] == 0");
		// simply do nothing
	}
	else
	{
	/*
      if (m_sendEvent.IsRunning ())
      {
        Simulator::Remove (m_sendEvent);
      }
    */

  //  NS_LOG_DEBUG ("Window: " << m_window << ", InFlight: " << m_inFlight);
      // if next interest is for next content, call SNP()
      if ((m_seq % m_seqMax == 0) && (m_retxSeqs.size() < 1) && (m_seq != 0))
      {
		//NS_LOG_DEBUG("SNP(int): next content retrieval should be started.");
        m_sendEvent = Simulator::ScheduleNow (&ConsumerWindow::SendPacket, this, pathId);
		//ScheduleNextPacket();
      }
      else
      {
        m_sendEvent = Simulator::ScheduleNow (&ConsumerWindow::SendPacket, this, pathId);
		//NS_LOG_INFO("SNP(int): request for branch " << pathId << " is sent.");
      }
	}
  }
}

void
ConsumerWindow::ScheduleNextPacket ()
{
	Simulator::Remove(m_sendEvent);
//  if (m_totalWindow == static_cast<uint32_t> (0))
//  {
//    if(m_sendEvent.IsRunning()) {
//		Simulator::Remove (m_sendEvent);
//	}

    // if next interest is for next content, let there be some interval
    // and window size for next content must be initialized
//    if ((m_seq % m_seqMax == 0)  && (m_retxSeqs.size() < 1) && (m_seq!=0))
//    {
      m_snpCalled = true;
      //double tmp = (m_random == 0) ? 1.0/m_frequency : m_random->GetValue();
	  double tmp = 1.0;
	  // select new pathId
	  mt19937 gen(m_branchSeed);
      uniform_int<> dst(0,MAX_PATHID);
      variate_generator< mt19937, uniform_int<> > rand(gen, dst);

	  m_totalWindow = m_initialWindow;
	  m_pathWindow = m_initialPathWindow;
	  NS_LOG_DEBUG("SNP(): Request for new content, m_tW: " << m_totalWindow << ", m_pW: " << m_pathWindow);
	  m_totalInFlight = 0;
	  m_pathCount = 0;
	  m_totalTimeoutCount = 0;
	  m_branchIntMap.clear();
	  m_nextBranchInt.clear();
	  uint32_t temp = 0;
	  for(int i=0; i<MAX_PATHID; i++) {
		  m_window[i] = 0;
		  m_ssth[i] = m_initialSsth;
		  m_windowCount[i] = 0;
		  m_inFlight[i] = 0;
		  m_timeoutCount[i] = 0;
	  }

	  //int pathId = rand();
	  int pathId = (m_seq+1) % MAX_PATHID;
	  while(m_pathWindow > temp)
	  {
		if (m_window[pathId] == (uint32_t)0)
		{
		  m_window[pathId]++;
          m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5 + tmp, m_rtt->RetransmitTimeout().ToDouble(Time::S) + tmp)), &ConsumerWindow::SendPacket, this, pathId);
          //m_sendEvent = Simulator::ScheduleNow (&ConsumerWindow::SendPacket, this, pathId);
		  temp++;
		} else {
		  //pathId = rand();
		  pathId++;
		  if (pathId >= MAX_PATHID)
		  {
			  pathId = 0;
		  }
		}
	  }
//    }
//    else
//	{
      //m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5, m_rtt->RetransmitTimeout ().ToDouble (Time::S))), &ConsumerWindow::SendPacket, this);
//	  NS_LOG_DEBUG("SNP(): Total window size is 0, but not new content.\n");
//	  exit(1);
//	}
//  }
//  else
//  {
//	  NS_LOG_DEBUG("New content retrieval is started, but total window size is not 0.\n");
//  }
}


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerWindow::OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
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

	SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find (seq);
    double chunkRTT = entry->time.ToDouble(Time::S);
    chunkRTT = Simulator::Now().ToDouble(Time::S) - chunkRTT;

    if (Simulator::Now().ToDouble(Time::S) > 950) {
      char fileName[50];
      sprintf(fileName, "%d_branchStats_multi.txt", m_index);
      m_ofs2.open(fileName, ios::out);
      m_ofs2 << "#" << "\tCounts" << "\tavgRTT\n";
      for(int i=0; i<MAX_PATHID; i++) {
        m_ofs2 << i << "\t" << m_poolCounter[i] << "\t" << m_poolDelay[i] / m_poolCounter[i] << "\n";
      }
      m_ofs2.close();
    }
	m_nextBranchInt.push_back(poolIndex);
	m_branchIntMap.erase(seq);
  
    Consumer::OnContentObject (contentObject, payload);

    // window size increase occurs when subscriber receive some amount of chunks
    // this some amount is current window size in this code(can be changed)

	// if this path is activated
    if (m_window[poolIndex] > (uint32_t) 0)
    {
	  // congestion-avoidance phase
	  if (m_ssth[poolIndex] < m_window[poolIndex])
	  {
        m_windowCount[poolIndex] = m_windowCount[poolIndex] + 1;
        if (m_windowCount[poolIndex] >= m_pathCount * m_pathCount * m_window[poolIndex])
        {
          m_window[poolIndex] = m_window[poolIndex]+1;
          m_totalWindow = m_totalWindow + 1;
          m_windowCount[poolIndex] = 0;
          if (m_totalWindow % 5 == (uint32_t)0)
		  {
            char fileName[30];
            sprintf(fileName, "%d_window_multi.txt", m_index);
            m_ofs.open(fileName, ios::app);
            WindowOutput(m_ofs);
            m_ofs.close();
	 	  }
		  /* path increase part
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
		    //Simulator::ScheduleNow (&ConsumerWindow::SendPacket, this, branchTemp);

		    ScheduleNextPacket(branchTemp);
	      }
		  */

		}
	  }
	  // slow-start phase
	  else
	  {
		if (m_windowCount[poolIndex] > m_pathCount * m_pathCount)
		{
	      m_window[poolIndex]++;
		  m_totalWindow++;
		  m_windowCount[poolIndex] = 0;
          if (m_totalWindow % 5 == (uint32_t)0)
		  {
            char fileName[30];
            sprintf(fileName, "%d_window_multi.txt", m_index);
            m_ofs.open(fileName, ios::app);
            WindowOutput(m_ofs);
            m_ofs.close();
		  }
		  /* path increase part
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
		  //Simulator::ScheduleNow (&ConsumerWindow::SendPacket, this, branchTemp);

		    ScheduleNextPacket(branchTemp);
	      }
		  */
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
    NS_LOG_DEBUG ("m_pathWindow: " << m_pathWindow << ", m_totalWindow: " << m_totalWindow << ", poolIndex: " << poolIndex << ", m_window[poolIndex]: " << m_window[poolIndex] << ", m_windowCount[]: " << m_windowCount[poolIndex] << ", m_totalInFlight: " << m_totalInFlight);

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
ConsumerWindow::OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> payload)
{
  Consumer::OnNack(interest, payload);

  if (m_totalInFlight > static_cast<uint32_t> (0))
  {
    m_totalInFlight--;
  }

  if (m_totalWindow > static_cast<uint32_t> (0))
  {
      //if (m_totalWindow > uint32_t(1)) m_totalWindow=m_totalWindow-1;
      //m_windowCount = 0;
  }
  NS_LOG_DEBUG ("Window: " << m_totalWindow << ", inFlight: " << m_totalInFlight);

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
ConsumerWindow::OnTimeout (uint32_t sequenceNumber)
{
  map<uint32_t, int>::iterator findIter = m_branchIntMap.find(sequenceNumber);
  // if new content transmission is started
  if(findIter == m_branchIntMap.end()) {
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
	NS_LOG_DEBUG("OT(): snpCalled is true");
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

      if(m_window[poolIndex] > (uint32_t)1 && m_window[poolIndex] >= m_preWindow[poolIndex]*11/20)
      {
		m_preWindow[poolIndex] = m_window[poolIndex];
		m_ssth[poolIndex] = m_window[poolIndex] / 2;
        m_totalWindow = m_totalWindow - (m_window[poolIndex] - m_window[poolIndex]/2);
	    //if (m_totalWindow < m_initialWindow) m_totalWindow = m_initialWindow;
	    m_window[poolIndex] /= 2;
      }
	  m_windowCount[poolIndex] = 0;

	  if(m_window[poolIndex] > (uint32_t)0)
  	  {
	    m_totalTimeoutCount++;
        m_timeoutCount[poolIndex]++;
	  }
    }

	/* path deletion part
    uint32_t deleteIndex = GetMaxIndex(m_timeoutCount);
    bool deleteFlag = false;
    if(m_totalTimeoutCount > m_pathWindow)
    {
	  if((m_pathWindow > m_initialPathWindow) && (deleteIndex != MAX_PATHID) && (m_timeoutCount[deleteIndex] != 0))
	  {
		  NS_LOG_DEBUG("Path delete. m_index: " <<  m_index << ", deleted path: " << deleteIndex << ", deletedWindow: " << m_window[deleteIndex] << ", totalWindow: " << m_totalWindow << ", m_inFlight[]: " << m_inFlight[deleteIndex] << ", totalInFlight: " << m_totalInFlight << ", m_pathWindow:  " << m_pathWindow);
		  deleteFlag = true;
		  m_pathWindow--;
		  m_totalTimeoutCount = 0;
		  m_timeoutCount[deleteIndex] = 0;
		  m_pathCount = 0;
		  m_totalWindow = m_totalWindow - m_window[deleteIndex];
		  m_window[deleteIndex] = 0;
		  m_inFlight[deleteIndex] = 0;
		  m_windowCount[deleteIndex] = 0;
	  }
    }
	*/


    char fileName[30];
    sprintf(fileName, "%d_window_multi.txt", m_index);
    m_ofs.open(fileName, ios::app);
    WindowOutput(m_ofs);
    m_ofs.close();

    NS_LOG_DEBUG ("m_totalWindow: " << m_totalWindow << ", m_totalInFlight: " << m_totalInFlight << ", poolIndex: " << poolIndex << ", m_window[poolIndex]: " << m_window[poolIndex] << ", m_inFlight[poolindex]: "<<m_inFlight[poolIndex]);
    Consumer::OnTimeout (sequenceNumber);
	/* path deletion part
    if(deleteFlag == true && deleteIndex == (uint32_t)poolIndex)
	{
      // simply do nothing
    } else {
	  ScheduleNextPacket(poolIndex);
    }
	*/
	// if path deletion is activated, delete below
	ScheduleNextPacket(poolIndex);
  }
}

uint32_t
ConsumerWindow::GetMaxIndex(uint32_t* src)
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
