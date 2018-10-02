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

// ndn-tree-app-delay-tracer.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"

#include <ns3/ndnSIM/utils/tracers/ndn-l3-aggregate-tracer.h>

using namespace ns3;

/**
 * This scenario simulates a tree topology (using topology reader module)
 *
 *    /------\      /------\      /------\      /------\
 *    |leaf-1|      |leaf-2|      |leaf-3|      |leaf-4|
 *    \------/      \------/      \------/      \------/
 *         ^          ^                ^           ^	
 *         |          |                |           |
 *    	    \        /                  \         / 
 *           \      /  			 \  	 /    10Mbps / 1ms
 *            \    /  			  \ 	/
 *             |  |  			   |   | 
 *    	       v  v                        v   v     
 *          /-------\                    /-------\
 *          | rtr-1 |                    | rtr-2 |
 *          \-------/                    \-------/
 *                ^                        ^                      
 *      	  |	 		   |
 *      	   \			  /  10 Mpbs / 1ms 
 *      	    +--------+  +--------+ 
 *      		     |  |      
 *                           v  v
 *      		  /--------\
 *      		  |  root  |
 *                        \--------/
 *
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     ./waf --run=ndn-tree-app-delay-tracer
 */

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  AnnotatedTopologyReader topologyReader ("", 1);
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-disjoint-4c.txt");
  topologyReader.Read ();
  
 
  NodeContainer rtrs;
  rtrs.Add( Names::Find<Node> ("rtr-1"));
  rtrs.Add( Names::Find<Node> ("rtr-2"));
  rtrs.Add( Names::Find<Node> ("rtr-3"));
  rtrs.Add (Names::Find<Node> ("rtr-4"));
  rtrs.Add (Names::Find<Node> ("rtr-5"));

  NodeContainer mptcpConsumer;
  mptcpConsumer.Add( Names::Find<Node> ("sub-1"));
  NodeContainer mptcpConsumer2;
  mptcpConsumer2.Add( Names::Find<Node> ("sub-7"));
  NodeContainer mptcpConsumer3;
  mptcpConsumer3.Add( Names::Find<Node> ("sub-8"));
  NodeContainer mptcpConsumer4;
  mptcpConsumer4.Add( Names::Find<Node> ("sub-9"));

  NodeContainer icp1;
  icp1.Add( Names::Find<Node> ("sub-2"));
  NodeContainer icp2;
  icp2.Add( Names::Find<Node> ("sub-3"));
  NodeContainer icp3;
  icp3.Add( Names::Find<Node> ("sub-4"));
  NodeContainer icp4;
  icp4.Add( Names::Find<Node> ("sub-5"));
  NodeContainer icp5;
  icp5.Add( Names::Find<Node> ("sub-6"));

  NodeContainer producer;

  producer.Add( Names::Find<Node> ("svr"));

  Ptr<Node> producers[1] = { Names::Find<Node> ("svr") };

  // Install CCNx stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  ndnHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "100830");
  ndnHelper.Install(producer);
  ndnHelper.Install(rtrs);

  ndn::StackHelper subHelper;
  subHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  subHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "1");
  subHelper.Install(mptcpConsumer);
  subHelper.Install(mptcpConsumer2);
  subHelper.Install(mptcpConsumer3);
  subHelper.Install(mptcpConsumer4);

  subHelper.Install(icp1);
  subHelper.Install(icp2);
  subHelper.Install(icp3);
  subHelper.Install(icp4);
  subHelper.Install(icp5);

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ccnxGlobalRoutingHelper;
  ccnxGlobalRoutingHelper.InstallAll ();

  
  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerWindowSmic");
  consumerHelper.SetPrefix ("/prefix");
  consumerHelper.SetAttribute ("Size", DoubleValue(10));
  consumerHelper.SetAttribute ("Frequency", StringValue("0.4"));
  consumerHelper.SetAttribute ("Randomize", StringValue("exponential"));
  consumerHelper.SetAttribute ("SlowStartThreshold", StringValue("5"));
  consumerHelper.SetAttribute ("PathWindow", StringValue("5"));
  consumerHelper.SetAttribute ("Window", StringValue("5"));
  consumerHelper.SetAttribute ("Index", StringValue("0"));
  consumerHelper.Install (mptcpConsumer);
  consumerHelper.SetAttribute ("Index", StringValue("6"));
  consumerHelper.Install (mptcpConsumer2);
  consumerHelper.SetAttribute ("Index", StringValue("7"));
  consumerHelper.Install (mptcpConsumer3);
  consumerHelper.SetAttribute ("Index", StringValue("8"));
  consumerHelper.Install (mptcpConsumer4);

  ndn::AppHelper consumerhelper ("ns3::ndn::ConsumerWindowSingle");
  consumerhelper.SetPrefix ("/prefix");
  consumerhelper.SetAttribute ("Size", DoubleValue(10));
  consumerhelper.SetAttribute ("Frequency", StringValue("0.4"));
  consumerhelper.SetAttribute ("Ssth", StringValue("25"));
  consumerhelper.SetAttribute ("Randomize", StringValue("exponential"));
  consumerhelper.SetAttribute ("Window", StringValue("1"));
  consumerhelper.SetAttribute ("Index", StringValue("1"));
  consumerhelper.Install (icp1);
  consumerhelper.SetAttribute ("Index", StringValue("2"));
  consumerhelper.Install (icp2);
  consumerhelper.SetAttribute ("Index", StringValue("3"));
  consumerhelper.Install (icp3);
  consumerhelper.SetAttribute ("Index", StringValue("4"));
  consumerhelper.Install (icp4);
  consumerhelper.SetAttribute ("Index", StringValue("5"));
  consumerhelper.Install (icp5);

  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));  
  producerHelper.SetPrefix ("/prefix");

  // Register /root prefix with global routing controller and
  // install producer that will satisfy Interests in /root namespace
  ccnxGlobalRoutingHelper.AddOrigins ("/prefix", producers[0]);
  producerHelper.Install (producers[0]);

  // Calculate and install FIBs
  ccnxGlobalRoutingHelper.CalculateRoutes ();

  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-1"), "/prefix", Names::Find<Node> ("rtr-1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-1"), "/prefix", Names::Find<Node> ("rtr-2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-1"), "/prefix", Names::Find<Node> ("rtr-3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-1"), "/prefix", Names::Find<Node> ("rtr-4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-1"), "/prefix", Names::Find<Node> ("rtr-5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-7"), "/prefix", Names::Find<Node> ("rtr-1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-7"), "/prefix", Names::Find<Node> ("rtr-2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-7"), "/prefix", Names::Find<Node> ("rtr-3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-7"), "/prefix", Names::Find<Node> ("rtr-4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-7"), "/prefix", Names::Find<Node> ("rtr-5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-8"), "/prefix", Names::Find<Node> ("rtr-1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-8"), "/prefix", Names::Find<Node> ("rtr-2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-8"), "/prefix", Names::Find<Node> ("rtr-3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-8"), "/prefix", Names::Find<Node> ("rtr-4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-8"), "/prefix", Names::Find<Node> ("rtr-5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-9"), "/prefix", Names::Find<Node> ("rtr-1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-9"), "/prefix", Names::Find<Node> ("rtr-2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-9"), "/prefix", Names::Find<Node> ("rtr-3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-9"), "/prefix", Names::Find<Node> ("rtr-4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("sub-9"), "/prefix", Names::Find<Node> ("rtr-5"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("rtr-1"), "/prefix", Names::Find<Node> ("svr"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("rtr-2"), "/prefix", Names::Find<Node> ("svr"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("rtr-3"), "/prefix", Names::Find<Node> ("svr"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("rtr-4"), "/prefix", Names::Find<Node> ("svr"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("rtr-5"), "/prefix", Names::Find<Node> ("svr"), 1);

  Simulator::Stop (Seconds (200.0));

  boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<ndn::L3AggregateTracer> > > aggTracers = ndn::L3AggregateTracer::InstallAll ("disjoint_smic_ls.txt", Seconds (199));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
