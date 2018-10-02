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
	Propagate Interests to one face, selected using headers' branchInt
	Assume that all faces are in "yellow state"(modified in fib/ndn-fib-entry.cc->UpdateStatus()
*/

#include "smart-flooding.h"

#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-content-object.h"
#include "ns3/ndn-content-store.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"

#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include "boost/random.hpp"

using namespace boost;
namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {
namespace fw {

NS_OBJECT_ENSURE_REGISTERED (SmartFlooding);

LogComponent SmartFlooding::g_log = LogComponent (SmartFlooding::GetLogName ().c_str ());

std::string
SmartFlooding::GetLogName ()
{
  return super::GetLogName ()+".SmartFlooding";
}

TypeId
SmartFlooding::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::fw::SmartFlooding")
    .SetGroupName ("Ndn")
    .SetParent <GreenYellowRed> ()
    .AddConstructor <SmartFlooding> ()
    ;
  return tid;
}
    
SmartFlooding::SmartFlooding ()
{
}

bool
SmartFlooding::DoPropagateInterest (Ptr<Face> inFace,
                                    Ptr<const InterestHeader> header,
                                    Ptr<const Packet> origPacket,
                                    Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this);

  // Try to work out with just green faces
  bool greenOk = super::DoPropagateInterest (inFace, header, origPacket, pitEntry);
  if (greenOk)
    return true;

  int propagatedCount = 0;

  // SMIC operation variables
  uint8_t faceCount = 0;
  FwHopCountTag HCT;
  origPacket->PeekPacketTag(HCT);
  int index = (int)HCT.Get();
  int pathId = (*header).GetPathId();
  int outfaces = pitEntry->GetFibEntry()->m_faces.size();
  int nodeId = inFace->GetNode()->GetId();
  mt19937 gen(pathId);
  uniform_int<> dst(0,nodeId);
  variate_generator< mt19937, uniform_int<> > rand(gen, dst);
  int hash = rand();

  BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
  {
    NS_LOG_DEBUG ("1: Trying " << boost::cref(metricFace));
    if (metricFace.m_status == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
      break;
    
    if (pathId == -1) {
      NS_LOG_DEBUG ("1: pathId: " << pathId << ", index: " << index << ", hash: " << hash);
      if (!TrySendOutInterest (inFace, metricFace.m_face, header, origPacket, pitEntry)) {
        continue;
      } 
      propagatedCount++;
    }
    else {
      if(pathId % outfaces == faceCount) {
        if(metricFace.m_face != inFace)
        {
          NS_LOG_DEBUG ("2: pathId == " << pathId << ", hopcount: " << index << ", hash: " << hash << ", outfaces: " << outfaces);
    	  TrySendOutInterest (inFace, metricFace.m_face, header, origPacket, pitEntry);
          propagatedCount++;
          break;
        }
        else
        {
          outfaces--;
          break;
        }
      }
      else {
        faceCount++;
        continue;
      }
    }
  }

  if (propagatedCount < 1) {

    faceCount = 0;
    BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
    {
      NS_LOG_DEBUG ("3: Trying " << boost::cref(metricFace));
      if (metricFace.m_status == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
        break;
      if (hash == -1) {
        NS_LOG_DEBUG ("hash: " << hash << ", index: " << index);
        if (!TrySendOutInterest (inFace, metricFace.m_face, header, origPacket, pitEntry)) {
          continue;
        } 
        propagatedCount++;
      }
      else {
//        if (!TrySendOutInterest (inFace, metricFace.m_face, header, origPacket, pitEntry)) {
//          faceCount++;
//          continue;
//        }
//        propagatedCount++;
        if(pathId % outfaces == faceCount) {
          if(metricFace.m_face != inFace)
          {
            NS_LOG_DEBUG ("3: pathId == " << pathId << ", hopcount: " << index << ", hash: " << hash << ", outfaces: " << outfaces);
            TrySendOutInterest (inFace, metricFace.m_face, header, origPacket, pitEntry);
            propagatedCount++;
            break;
          }
          else
          {
            continue;
          }
        }
        else {
          faceCount++;
	  continue;
        }
      }
    }  
  }

  NS_LOG_INFO ("Propagated to " << propagatedCount << " faces");
  return propagatedCount > 0;
}

void
SmartFlooding::OnData (Ptr<Face> inFace,
                       Ptr<const ContentObjectHeader> header,
                       Ptr<Packet> payload,
                       Ptr<const Packet> origPacket)
{
  NS_LOG_FUNCTION ( (int) (inFace->GetNode()->GetId()) << inFace->GetNode() << inFace << header->GetName () << payload << origPacket);

  static ContentObjectTail tail;
  Ptr<ContentObjectHeader> newHeader = Create<ContentObjectHeader> ();
  newHeader->SetName (Create<NameComponents> (header->GetName ()));
  newHeader->SetFreshness (header->GetFreshness ());
  newHeader->SetHash (header->GetHash ());
  newHeader->UpdateHash ( (uint32_t) (inFace->GetNode()->GetId()) );

  Ptr<Packet> newPacket = Create<Packet> (origPacket->GetSize());
  newPacket->AddHeader (*newHeader);
  newPacket->AddTrailer (tail);

  NS_LOG_DEBUG ("Old hash: " << header->GetHash() << ", Updated hash: " << newHeader->GetHash());

  m_inData (newHeader, payload, inFace);
  
  // Lookup PIT entry
  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*newHeader);
  if (pitEntry == 0)
    {
      DidReceiveUnsolicitedData (inFace, newHeader, payload, origPacket);
      return;
    }
  else
    {
      FwHopCountTag hopCountTag;
      if (payload->PeekPacketTag (hopCountTag))
        {
          Ptr<Packet> payloadCopy = payload->Copy ();
          payloadCopy->RemovePacketTag (hopCountTag);
          
          // Add or update entry in the content store
          m_contentStore->Add (newHeader, payloadCopy);
        }
      else
        {
          // Add or update entry in the content store
          m_contentStore->Add (newHeader, payload); // no need for extra copy
        }
    }

  while (pitEntry != 0)
    {
      // Do data plane performance measurements
      WillSatisfyPendingInterest (inFace, pitEntry);

      // Actually satisfy pending interest
      SatisfyPendingInterest (inFace, newHeader, payload, newPacket, pitEntry);

      // Lookup another PIT entry
      pitEntry = m_pit->Lookup (*newHeader);
    }
}

} // namespace fw
} // namespace ndn
} // namespace ns3
