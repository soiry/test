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
 * Author: Ilya Moiseenko <iliamo@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef NDN_CONSUMER_WINDOW_SINGLE_H
#define NDN_CONSUMER_WINDOW_SINGLE_H
#define MAX_PATHID 100

#include "ndn-consumer.h"
#include "ns3/traced-value.h"
#include <fstream>

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn
 * \brief Ndn application for sending out Interest packets (window-based)
 *
 * !!! ATTENTION !!! This is highly experimental and relies on experimental features of the simulator.
 * Behavior may be unpredictable if used incorrectly.
 */
class ConsumerWindowSingle: public Consumer
{
public: 
  static TypeId GetTypeId ();
        
  /**
   * \brief Default constructor 
   */
  ConsumerWindowSingle ();

  void
  WindowOutput(std::ofstream &ofs);

  uint32_t
  GetWindow () const;
  // From App
  // virtual void
  // OnInterest (const Ptr<const InterestHeader> &interest);

  virtual void
  OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> payload);

  virtual void
  OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                   Ptr<Packet> payload);

  virtual void
  OnTimeout (uint32_t sequenceNumber);

//  virtual void
//  OnTimeout (uint32_t contentRank, uint32_t sequenceNumber);

  virtual void
  SendPacket();

  uint32_t
  GetMaxIndex(uint32_t* src);
 
protected:
  /**
   * \brief Constructs the Interest packet and sends it using a callback to the underlying NDN protocol
   */
  virtual void
  ScheduleNextPacket ();

private:

  void
  SetRandomize (const std::string &value);	//interval type

  std::string
  GetRandomize () const;

  virtual void
  SetWindow (uint32_t window);

  void
  SetSsth (uint32_t ssth);

  virtual void
  SetPayloadSize (uint32_t payload);

  uint32_t
  GetPayloadSize () const;

  double
  GetMaxSize () const;

  void
  SetMaxSize (double size);

  void
  SetNumberOfContents (uint32_t numberOfContents);	//make zipf distribution

  uint32_t
  GetNextSeq ();		//get next content number

private:
  uint32_t		m_payloadSize; // expected payload size
  double		m_maxSize; // max size to request
  uint32_t		m_N; // number of contents - jhsong
  std::vector<double>	m_Pcum;	// zipf variable(just be used, not set)
  double		m_q;	// zipf param
  double		m_s;	// zipf param
  UniformVariable	m_SeqRng;
  double		m_frequency;
  RandomVariable	*m_random;
  std::string		m_randomType;
  uint32_t		m_contentRank;
  uint32_t		m_initialWindow;
  uint32_t		m_initialSsth;
  uint32_t		m_ssth;
  uint32_t		m_windowCount;
  uint32_t		m_preWindow;
  bool			m_setInitialWindowOnTimeout;
  int			m_branchSeed;
  int			m_totalContent;
  double		m_avgCompletionTime;
  uint32_t		m_index;
  std::ofstream		m_ofs;
  std::ofstream		m_ofs2;
  

  TracedValue<uint32_t>	m_window;
  TracedValue<uint32_t>	m_inFlight;


  std::map<uint32_t, int> m_branchIntMap;
  std::deque<int> m_nextBranchInt;
  int m_pathId[MAX_PATHID];
  int m_poolCounter[MAX_PATHID];
  double m_poolDelay[MAX_PATHID];
  std::map<uint32_t, Time> m_startTime;
  std::map<uint32_t, uint32_t> m_chunkCounter;

  
/*
  struct RetxConSeqsList
  {
    RetxConSeqsList (uint32_t _contentRank, uint32_t _seq) : contentRank (_contentRank), seq (_seq) { }

    uint32_t contentRank;
    uint32_t seq;
  };

  std::list<RetxConSeqsList*> m_retxConSeqs;

  struct ConSeqTimeout
  {
    ConSeqTimeout (uint32_t _seq, uint32_t _rank, Time _time) : seq (_seq), rank(_rank), time (_time) {}

    uint32_t seq;
    uint32_t rank;
    Time time;
  };

  class i_seq { };
  class i_timestamp { };

  struct ConSeqTimeoutsContainer :
    public boost::multi_index::multi_index_container<
    ConSeqTimeout,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<i_seq>,
        boost::multi_index::member<ConSeqTimeout, uint32_t, &ConSeqTimeout::seq>
        >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<i_timestamp>,
        boost::multi_index::member<ConSeqTimeout, Time, &ConSeqTimeout::time>
        >
      >
    > { } ;

  ConSeqTimeoutsContainer m_conSeqTimeouts;

  ConSeqTimeoutsContainer m_conSeqlastDelay;
  ConSeqTimeoutsContainer m_conSeqFullDelay;
*/
};

} // namespace ndn
} // namespace ns3

#endif
