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
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-tree.txt");
  topologyReader.Read ();
  
 
  NodeContainer rtrs;
  rtrs.Add( Names::Find<Node> ("rtr-1"));
  rtrs.Add( Names::Find<Node> ("rtr-2"));
  rtrs.Add( Names::Find<Node> ("rtr-3"));
  rtrs.Add (Names::Find<Node> ("rtr-4"));

  NodeContainer accs;
  accs.Add( Names::Find<Node> ("acc-1"));
  accs.Add( Names::Find<Node> ("acc-2"));

  NodeContainer root;
  root.Add( Names::Find<Node> ("root"));

  NodeContainer consumers;
  consumers.Add( Names::Find<Node> ("sub-1"));

  NodeContainer producer;

  producer.Add( Names::Find<Node> ("leaf-1"));
  producer.Add( Names::Find<Node> ("leaf-2"));
  producer.Add( Names::Find<Node> ("leaf-3"));
  producer.Add( Names::Find<Node> ("leaf-4"));
  producer.Add( Names::Find<Node> ("leaf-5"));
  producer.Add( Names::Find<Node> ("leaf-6"));
  producer.Add (Names::Find<Node> ("leaf-7"));
  producer.Add (Names::Find<Node> ("leaf-8"));

  Ptr<Node> producers[8] = { Names::Find<Node> ("leaf-1"), Names::Find<Node> ("leaf-2"),
                             Names::Find<Node> ("leaf-3"), Names::Find<Node> ("leaf-4"),
							 Names::Find<Node> ("leaf-5"), Names::Find<Node> ("leaf-6"),
                             Names::Find<Node> ("leaf-7"), Names::Find<Node> ("leaf-8")
  };

  // Install CCNx stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  ndnHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "100830");
  ndnHelper.Install(producer);
  ndnHelper.Install(rtrs);
  ndnHelper.Install(accs);
  ndnHelper.Install(root);

  ndn::StackHelper subHelper;
  subHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  subHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "1");
  subHelper.Install(consumers);

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ccnxGlobalRoutingHelper;
  ccnxGlobalRoutingHelper.InstallAll ();

  
  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerWindowMic");
  consumerHelper.SetPrefix ("/prefix");
  consumerHelper.SetAttribute ("Size", DoubleValue(10));
  consumerHelper.SetAttribute ("Frequency", StringValue("0.4"));
  consumerHelper.SetAttribute ("Randomize", StringValue("exponential"));
  consumerHelper.SetAttribute ("PathWindow", StringValue("10"));
  consumerHelper.SetAttribute ("Window", StringValue("10"));
  consumerHelper.SetAttribute ("Index", StringValue("0"));
  consumerHelper.Install (consumers);

  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));  
  producerHelper.SetPrefix ("/prefix");

  // Register /root prefix with global routing controller and
  // install producer that will satisfy Interests in /root namespace
  for(int i=0; i<8; i++)
  {
    ccnxGlobalRoutingHelper.AddOrigins ("/prefix", producers[i]);
    producerHelper.Install (producers[i]);
  }

  // Calculate and install FIBs
  ccnxGlobalRoutingHelper.CalculateRoutes ();

  Simulator::Stop (Seconds (1000.0));

  boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<ndn::L3AggregateTracer> > > aggTracers = ndn::L3AggregateTracer::InstallAll ("tree_mic_ls.txt", Seconds (999));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
