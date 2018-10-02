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
                   StringValue ("1"),
                   MakeUintegerAccessor (&ConsumerWindow::GetWindow, &ConsumerWindow::SetWindow),
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
                     MakeTraceSourceAccessor (&ConsumerWindow::m_window))
    .AddTraceSource ("InFlight",
                     "Current number of outstanding interests",
                     MakeTraceSourceAccessor (&ConsumerWindow::m_window))
    ;

  return tid;
}

ConsumerWindow::ConsumerWindow ()
  : m_payloadSize (1040)
  , m_q (1.0)			//zipf param
  , m_s (1.0)			//zipf param
  , m_SeqRng (0.0, 1.0)		//random rng
  , m_frequency (1.0)		//request interval
  , m_random (0)		//type of request interval
  , m_contentRank (0)		//content number
  , m_windowCount (0)		//to increase window size
  , m_branchSeed (1)		//random seed
  , m_totalContent (0)		//num of received content
  , m_avgCompletionTime (0.0)	//average download completion time
  , m_inFlight (0)		//the number of in-network interests
{
  SetNumberOfContents(1000);	//make zipf distribution
  mt19937 gen(m_branchSeed);
  uniform_int<> dst(0,43);
  variate_generator< mt19937, uniform_int<> > rand(gen, dst);
//  mt19937 gen(m_branchSeed);    //make branchIntPool
//  uniform_int<> dst(0,43);
//  uniform_int<> pool(0,101);
//  variate_generator< mt19937, uniform_int<> > rand(gen, dst);
//  variate_generator< mt19937, uniform_int<> > poolRand(gen, pool);
  for (int i=0; i<101; i++) {
    for (int j=0; j<20; j++) {
      m_branchIntPool[i][j] = rand();
    }
    //m_poolCounter[i] = 0;
    //m_poolDelay[i] = 0.0;
  }
}

void
ConsumerWindow::WindowOutput(ofstream &ofs)
{
  ofs << m_index << "\t" << Simulator::Now().ToDouble(Time::S) << "\t" << m_window << "\n";
}

void
ConsumerWindow::SetWindow (uint32_t window)
{
  m_initialWindow = window;
  m_window = m_initialWindow;
}

uint32_t
ConsumerWindow::GetWindow () const
{
  return m_window;
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
      if (p_random <= p_sum && i != m_contentRank)
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
ConsumerWindow::SendPacket ()
{
  if (!m_active) return;

  NS_LOG_FUNCTION_NOARGS ();
  m_inFlight++;

  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid

  NS_LOG_INFO ("retx size: " << m_retxSeqs.size());

  while (m_retxSeqs.size ())
  {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase (m_retxSeqs.begin());
    break;
  }
  if (seq == std::numeric_limits<uint32_t>::max ())
  {
    if (m_seqMax != std::numeric_limits<uint32_t>::max ())
    {
      if (m_seq % m_seqMax == 0)  // request for next content
      {
        m_window = m_initialWindow;
        m_contentRank = GetNextSeq();
        m_seq = m_contentRank * m_seqMax;
        m_startTime.insert( map<uint32_t, Time>::value_type( m_contentRank, Simulator::Now() ));
        m_chunkCounter.insert( map<uint32_t, int>::value_type( m_contentRank, 0));
      }
    }
    seq = m_seq++;
  }
  
  Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_interestName);
  (*nameWithSequence) (seq);

  InterestHeader interestHeader;
  interestHeader.SetNonce               (m_rand.GetValue ());
  interestHeader.SetName                (nameWithSequence);

  // set branchInt
  // if there're nextBranchInt, use that
  // else, use random pool
  // (seq, branch pool index) is save in map
  if (m_nextBranchInt.size())
  {
    int branchTemp = m_nextBranchInt.front(); // branchInt pool number
    m_nextBranchInt.pop_front();
    for (int i=0; i<20; i++)
    {
      interestHeader.SetBranchInt(i, m_branchIntPool[branchTemp][i]);
    }
    m_branchIntMap.insert( map<uint32_t, int>::value_type(seq, branchTemp));
  }
  else // if there's no next branch info, use random branchint in pool
  {
    //int poolNumber = seq % 101;
    mt19937 gen(time(NULL));
    uniform_int<> pool(0,101);
    variate_generator< mt19937, uniform_int<> > poolRand(gen, pool);
    int poolNumber = poolRand();
    for (int i=0; i<20; i++)
    {
      interestHeader.SetBranchInt(i, m_branchIntPool[poolNumber][i]);
    }
    m_branchIntMap.insert( map<uint32_t, int>::value_type(seq, poolNumber));
  }
      
/*
  mt19937 gen((unsigned long)time(NULL));
  uniform_int<> dst(0,30);
  variate_generator< mt19937, uniform_int<> > rand(gen, dst);
  for (int i=0; i<20; i++) {
    interestHeader.SetBranchInt(i, rand());
  }
*/
  //NS_LOG_INFO ("Requesting Interest: \n" << interestHeader);
  //NS_LOG_INFO ("> branchInt: " << interestHeader.GetOneBranch(13));
  //NS_LOG_INFO ("> Interest for " << seq);
  //NS_LOG_INFO ("Window: "<<m_window<<", inFlight: "<<m_inFlight);

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

  ScheduleNextPacket ();
}


void
ConsumerWindow::ScheduleNextPacket ()
{
  if (m_window == static_cast<uint32_t> (0))
  {
    Simulator::Remove (m_sendEvent);

    // if next interest is for next content, let there be some interval
    // and window size for next content must be initialized
    if ((m_seq % m_seqMax == 0)  && (m_retxSeqs.size() < 1) && (m_seq!=0))
    {
      double tmp = (m_random == 0) ? 1.0/m_frequency : m_random->GetValue();
      m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5 + tmp, m_rtt->RetransmitTimeout().ToDouble(Time::S) + tmp)), &ConsumerWindow::SendPacket, this);
      m_window = m_initialWindow;
    }
    else
      m_sendEvent = Simulator::Schedule (Seconds (std::min<double> (0.5, m_rtt->RetransmitTimeout ().ToDouble (Time::S))), &ConsumerWindow::SendPacket, this);
  }
  else if (m_inFlight >= m_window)
  {
    // simply do nothing
  }
  else
  {
    if (m_sendEvent.IsRunning ())
    {
      Simulator::Remove (m_sendEvent);
    }

//  NS_LOG_DEBUG ("Window: " << m_window << ", InFlight: " << m_inFlight);
    // if next interest is for next content, let there be some interval
    // and window size of next content must be initialized
    if ((m_seq % m_seqMax == 0) && (m_retxSeqs.size() < 1) && (m_seq != 0))
    {
      m_sendEvent = Simulator::Schedule ((m_random == 0) ? Seconds(1.0/m_frequency) : Seconds(m_random->GetValue()), &ConsumerWindow::SendPacket, this);
    }
    else
    {
      m_sendEvent = Simulator::ScheduleNow (&ConsumerWindow::SendPacket, this);
    }
  }
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerWindow::OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                                     Ptr<Packet> payload)
{

  uint32_t seq = boost::lexical_cast<uint32_t> (contentObject->GetName ().GetComponents ().back ());
  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find (seq);
  double chunkRTT = entry->time.ToDouble(Time::S);
  chunkRTT = Simulator::Now().ToDouble(Time::S) - chunkRTT;

  if (Simulator::Now().ToDouble(Time::S) > 550) {
    char fileName[20];
    sprintf(fileName, "%d_branchStats.txt", m_index);
    m_ofs2.open(fileName, ios::out);
    m_ofs2 << "#" << "\tCounts" << "\tavgRTT\n";
    for(int i=0; i<101; i++) {
      m_ofs2 << i << "\t" << m_poolCounter[i] << "\t" << m_poolDelay[i] / m_poolCounter[i] << "\n";
    }
    m_ofs2.close();
  }

  Consumer::OnContentObject (contentObject, payload);

  // window size increase occurs when subscriber receive some amount of chunks
  // this some amount is current window size in this code(can be changed)
  m_windowCount = m_windowCount + 1;
  if (m_windowCount >= m_window)
  {
    m_window = m_window + 1;
    m_windowCount = 0;
    char fileName[20];
    sprintf(fileName, "%d_window.txt", m_index);
    m_ofs.open(fileName, ios::app);
    WindowOutput(m_ofs);
    m_ofs.close();
  }

  if (m_inFlight > static_cast<uint32_t> (0))
    m_inFlight--;
  NS_LOG_DEBUG ("Window: " << m_window << ", InFlight: " << m_inFlight);

  // using map, find out the branch int pool index
  // and push it to next branch info
  map<uint32_t, int>::iterator findIter = m_branchIntMap.find(seq);
  m_nextBranchInt.push_back(findIter->second);
  //m_nextBranchInt.push_back(findIter->second);
  m_branchIntMap.erase(seq);

  m_poolCounter[findIter->second]++;
  m_poolDelay[findIter->second] += chunkRTT;

//  if(seq % m_seqMax == 0)
//  {
//    m_chunkCounter.insert( map<uint32_t, uint32_t>::value_type( seq/m_seqMax, 1));
//  }
//  else
//  {
    map<uint32_t, uint32_t>::iterator findIter2 = m_chunkCounter.find(seq/m_seqMax);
    findIter2->second++;
    if (findIter2->second >= m_seqMax)
    {
      map<uint32_t, Time>::iterator findIter3 = m_startTime.find(seq/m_seqMax);
      double timeTemp = findIter3->second.ToDouble(Time::S);
      timeTemp = Simulator::Now().ToDouble(Time::S) - timeTemp;
      m_avgCompletionTime = ((double)m_totalContent/(m_totalContent+1)) * m_avgCompletionTime + ((double)1/(m_totalContent+1)) * timeTemp;
      cout << m_index << "\tcontentNum: " << seq/m_seqMax << "\ttimeTemp: " << timeTemp << ", contentTotal: " << m_totalContent << ", avg comp time: " << m_avgCompletionTime << "\n";
//      cout << m_index << "\tbranchIntSize: " << m_branchIntMap.size() << "\n";
      m_totalContent++;
      m_startTime.erase(seq/m_seqMax);
      m_chunkCounter.erase(seq/m_seqMax);
      m_nextBranchInt.clear();
      //m_branchIntMap.clear();
    }
//  }

  ScheduleNextPacket ();
}

void
ConsumerWindow::OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> payload)
{
  Consumer::OnNack(interest, payload);

  if (m_inFlight > static_cast<uint32_t> (0))
    m_inFlight--;

  if (m_window > static_cast<uint32_t> (0))
  {
      if (m_window > uint32_t(1)) m_window=m_window-1;
      m_windowCount = 0;
//      m_window = m_window*0.5;
//      m_windowCount = 0;
  }
  NS_LOG_DEBUG ("Window: " << m_window << ", inFlight: " << m_inFlight);

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
  if (m_inFlight > static_cast<uint32_t> (0))  m_inFlight--;

  if (m_setInitialWindowOnTimeout)
  {

    if(m_window > (uint32_t)1)
    {
      m_window--;
      m_windowCount = 0;
    }

//    m_window = m_initialWindow;
//    m_windowCount = 0;
  }
  char fileName[20];
  sprintf(fileName, "%d_window.txt", m_index);
  m_ofs.open(fileName, ios::app);
  WindowOutput(m_ofs);
  m_ofs.close();

  NS_LOG_DEBUG ("Window: " << m_window << ", InFlight: " << m_inFlight);
  Consumer::OnTimeout (sequenceNumber);
}
}
}
