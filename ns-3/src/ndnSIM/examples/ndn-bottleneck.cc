/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2012 University of California, Los Angeles
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

// ndn-bottleneck.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"

#include <ns3/ndnSIM/utils/tracers/ndn-l3-aggregate-tracer.h>

using namespace ns3;


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  AnnotatedTopologyReader topologyReader ("", 1);
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-bottleneck.txt");
  topologyReader.Read ();
  
 
  NodeContainer mic1; 
  mic1.Add( Names::Find<Node> ("mic-1"));
  NodeContainer mic2;
  mic2.Add( Names::Find<Node> ("mic-2"));
  NodeContainer mic3;
  mic3.Add( Names::Find<Node> ("mic-3"));

  NodeContainer icp1;
  icp1.Add( Names::Find<Node> ("icp-1"));

  NodeContainer accs;
  accs.Add( Names::Find<Node> ("acc-1"));
  accs.Add( Names::Find<Node> ("acc-2"));
  accs.Add( Names::Find<Node> ("acc-3"));
  accs.Add( Names::Find<Node> ("acc-4"));
  accs.Add( Names::Find<Node> ("bot"));

  NodeContainer pub;
  pub.Add( Names::Find<Node> ("pub"));

  // Install CCNx stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  ndnHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "100830");
  ndnHelper.Install(accs);
  ndnHelper.Install(pub);

  ndn::StackHelper subHelper;
  subHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  subHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "1");
  subHelper.Install(mic1);
  subHelper.Install(mic2);
  subHelper.Install(mic3);
  subHelper.Install(icp1);


  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ccnxGlobalRoutingHelper;
  ccnxGlobalRoutingHelper.InstallAll ();


  ndn::AppHelper consumerhelper ("ns3::ndn::ConsumerWindowSingle");
  consumerhelper.SetPrefix ("/prefix");
  consumerhelper.SetAttribute ("Size", DoubleValue(10));
  consumerhelper.SetAttribute ("Frequency", StringValue("0.4"));
  consumerhelper.SetAttribute ("Ssth", StringValue("1"));
  consumerhelper.SetAttribute ("Randomize", StringValue("none"));
  consumerhelper.SetAttribute ("Window", StringValue("1"));
  consumerhelper.SetAttribute ("Index", StringValue("0"));
  consumerhelper.Install (icp1);

  ndn::AppHelper micHelper ("ns3::ndn::ConsumerWindowSmic");
  micHelper.SetPrefix ("/prefix");
  micHelper.SetAttribute ("Size", DoubleValue(10));
  micHelper.SetAttribute ("Frequency", StringValue("0.4"));
  micHelper.SetAttribute ("Randomize", StringValue("none"));
  micHelper.SetAttribute ("SlowStartThreshold", StringValue("1"));
  micHelper.SetAttribute ("PathWindow", StringValue("1"));
  micHelper.SetAttribute ("Window", StringValue("1"));
  micHelper.SetAttribute ("Index", StringValue("1"));
  micHelper.Install (mic1);
  micHelper.SetAttribute ("Index", StringValue("2"));
  micHelper.Install (mic2);
  micHelper.SetAttribute ("Index", StringValue("3"));
  micHelper.Install (mic3);

  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));  
  producerHelper.SetPrefix ("/prefix");

  // Register /root prefix with global routing controller and
  // install producer that will satisfy Interests in /root namespace
  ccnxGlobalRoutingHelper.AddOrigins ("/prefix", pub);
  producerHelper.Install (pub);

  // Calculate and install FIBs
  ccnxGlobalRoutingHelper.CalculateRoutes ();

  Simulator::Stop (Seconds (1000.0));

  boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<ndn::L3AggregateTracer> > > aggTracers = ndn::L3AggregateTracer::InstallAll ("tree_bottleneck_ls.txt", Seconds (999));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
