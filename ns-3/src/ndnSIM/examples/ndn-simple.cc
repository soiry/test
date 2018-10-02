/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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
// ndn-simple.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

using namespace ns3;

/**
 * This scenario simulates a very simple network topology:
 *
 *
 *      +----------+     1Mbps      +--------+     1Mbps      +----------+
 *      | consumer | <------------> | router | <------------> | producer |
 *      +----------+         10ms   +--------+          10ms  +----------+
 *
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-simple
 */


uint32_t m_N;
double m_q;
double m_s;
std::vector<double> m_Pcum;
double m_frequency;
RandomVariable *m_random;
std::string m_randomType;


int 
main (int argc, char *argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault ("ns3::PointToPointNetDevice::DataRate", StringValue ("1Mbps"));
  Config::SetDefault ("ns3::PointToPointChannel::Delay", StringValue ("10ms"));
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", StringValue ("1"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install (nodes.Get (0), nodes.Get (1));
  //p2p.Install (nodes.Get (1), nodes.Get (2));
  p2p.Install (nodes.Get (3), nodes.Get (1));
  p2p.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
  p2p.Install (nodes.Get(1), nodes.Get(2));

  // Install CCNx stack on all nodes
  ndn::StackHelper ccnxHelper;
  ndn::GlobalRoutingHelper nGRH;
  ccnxHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
  ccnxHelper.Install (nodes);
  nGRH.Install(nodes);
  nGRH.AddOrigins("/prefix", nodes.Get (2));
  nGRH.CalculateRoutes ();

  // Installing applications

  // Consumer
  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix ("/prefix");
  //consumerHelper.SetAttribute ("Window", StringValue ("1"));
  //consumerHelper.SetAttribute ("Size", DoubleValue (1)); 
  consumerHelper.Install (nodes.Get (0)); // first node

  ndn::AppHelper conHelper ("ns3::ndn::ConsumerCbr");
  conHelper.SetPrefix ("/prefix");
  conHelper.SetAttribute ("Frequency", StringValue("10.0"));
  //conHelper.SetAttribute ("Window", StringValue ("1"));
  //conHelper.SetAttribute ("Size", DoubleValue (1));
  conHelper.Install (nodes.Get (3));

  // Producer
  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix ("/prefix");
  producerHelper.SetAttribute ("PayloadSize", UintegerValue(1024));
  producerHelper.Install (nodes.Get (2)); // last node

  Simulator::Stop (Seconds (20.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
/*
void SetNumberOfContents (uint32_t numOfContents)
{
  m_N = numOfContents;

  m_Pcum = std::vector<double> (m_N + 1);

  m_Pcum[0] = 0.0;
  for (uint32_t i=1; i<m_N; i++)  {
    m_Pcum[i] = m_Pcum[i-1] + 1.0/pow(i+m_q, m_s);
  }

  for (uint32_t i=1; i<m_N; i++)  {
    m_Pcum[i] = m_Pcum[i] / m_Pcum[m_N];
  }
}

uint32_t GetNextSeq()
{
  uint32_t contentIndex = 1;
  double p_sum = 0;

  double p_random = [0, 1].GetValue();
  while (p_random == 0) {
    p_random = [0, 1].GetValue();
  }

  for(uint32_t i=1; i<=m_N; i++) {
    p_sum = m_Pcum[i];
    if (p_random <= p_sum) {
      contentIndex = i;
      break;
    }
  }
  return contentIndex;
}

void SetRandomize (const std::string &value)
{
  if (m_random)
    delete m_random;

  else if (value == "uniform") {
    m_random = new UniformVariable (0.0, 2 * 1.0 / m_frequency);
  }

  else if (value == "exponential") {
    m_random = new exponentialVariable(1.0 / m_frequency, 50*1.0/m_frequency);
  }

  else
    m_random = 0;

  m_randomType = value;
}
*/
