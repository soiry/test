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
 * To run scenario and see what is happening, use the following command:
 *
 *     ./waf --run=ndn-abilene-mic
 */

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  AnnotatedTopologyReader topologyReader ("", 1);
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-abilene.txt");
  topologyReader.Read ();
  
 
  NodeContainer rtrs;
  rtrs.Add( Names::Find<Node> ("0"));
  rtrs.Add( Names::Find<Node> ("1"));
  rtrs.Add( Names::Find<Node> ("2"));
  rtrs.Add( Names::Find<Node> ("3"));
  rtrs.Add( Names::Find<Node> ("4"));
  rtrs.Add( Names::Find<Node> ("5"));
  rtrs.Add( Names::Find<Node> ("6"));
  rtrs.Add( Names::Find<Node> ("7"));
  rtrs.Add( Names::Find<Node> ("8"));
  rtrs.Add( Names::Find<Node> ("9"));
  rtrs.Add (Names::Find<Node> ("10"));

  NodeContainer consumers11;
  consumers11.Add( Names::Find<Node> ("11"));
  NodeContainer consumers12;
  consumers12.Add( Names::Find<Node> ("12"));
  NodeContainer consumers13;
  consumers13.Add( Names::Find<Node> ("13"));
  NodeContainer consumers15;
  consumers15.Add( Names::Find<Node> ("15"));
  NodeContainer consumers16;
  consumers16.Add( Names::Find<Node> ("16"));
  NodeContainer consumers17;
  consumers17.Add( Names::Find<Node> ("17"));
  NodeContainer consumers18;
  consumers18.Add( Names::Find<Node> ("18"));
  NodeContainer consumers19;
  consumers19.Add( Names::Find<Node> ("19"));
  NodeContainer consumers20;
  consumers20.Add( Names::Find<Node> ("20"));
  NodeContainer consumers21;
  consumers21.Add( Names::Find<Node> ("21"));
  NodeContainer consumers22;
  consumers22.Add( Names::Find<Node> ("22"));
  NodeContainer consumers23;
  consumers23.Add( Names::Find<Node> ("23"));
  NodeContainer consumers25;
  consumers25.Add( Names::Find<Node> ("25"));
  NodeContainer consumers26;
  consumers26.Add( Names::Find<Node> ("26"));
  NodeContainer consumers27;
  consumers27.Add( Names::Find<Node> ("27"));
  NodeContainer consumers28;
  consumers28.Add( Names::Find<Node> ("28"));
  NodeContainer consumers29;
  consumers29.Add( Names::Find<Node> ("29"));
  NodeContainer consumers30;
  consumers30.Add( Names::Find<Node> ("30"));
  NodeContainer consumers32;
  consumers32.Add( Names::Find<Node> ("32"));
  NodeContainer consumers33;
  consumers33.Add( Names::Find<Node> ("33"));
  NodeContainer consumers34;
  consumers34.Add( Names::Find<Node> ("34"));
  NodeContainer consumers35;
  consumers35.Add( Names::Find<Node> ("35"));
  NodeContainer consumers36;
  consumers36.Add( Names::Find<Node> ("36"));
  NodeContainer consumers37;
  consumers37.Add( Names::Find<Node> ("37"));


  NodeContainer producer;
  producer.Add( Names::Find<Node> ("14"));
  producer.Add( Names::Find<Node> ("24"));
  producer.Add( Names::Find<Node> ("31"));

  Ptr<Node> producers[3] = { Names::Find<Node> ("14"), Names::Find<Node> ("24"),
                             Names::Find<Node> ("31")
  };

  // Install CCNx stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  ndnHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "100830");
  ndnHelper.Install(producer);
  ndnHelper.Install(rtrs);

  ndn::StackHelper subHelper;
  subHelper.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding");
  subHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "1");
  subHelper.Install(consumers11);
  subHelper.Install(consumers12);
  subHelper.Install(consumers13);
  subHelper.Install(consumers15);
  subHelper.Install(consumers16);
  subHelper.Install(consumers17);
  subHelper.Install(consumers18);
  subHelper.Install(consumers19);
  subHelper.Install(consumers20);
  subHelper.Install(consumers21);
  subHelper.Install(consumers22);
  subHelper.Install(consumers23);
  subHelper.Install(consumers25);
  subHelper.Install(consumers26);
  subHelper.Install(consumers27);
  subHelper.Install(consumers28);
  subHelper.Install(consumers29);
  subHelper.Install(consumers30);
  subHelper.Install(consumers32);
  subHelper.Install(consumers33);
  subHelper.Install(consumers34);
  subHelper.Install(consumers35);
  subHelper.Install(consumers36);
  subHelper.Install(consumers37);

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ccnxGlobalRoutingHelper;
  ccnxGlobalRoutingHelper.InstallAll ();

  
  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerWindowMic");
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.SetAttribute ("Size", DoubleValue(10));
  consumerHelper.SetAttribute ("Frequency", StringValue("0.4"));
  consumerHelper.SetAttribute ("Randomize", StringValue("exponential"));
  consumerHelper.SetAttribute ("PathWindow", StringValue("10"));
  consumerHelper.SetAttribute ("Window", StringValue("10"));
  consumerHelper.SetAttribute ("Index", StringValue("11"));
  consumerHelper.Install (consumers11);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("12"));
  consumerHelper.Install (consumers12);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("13"));
  consumerHelper.Install (consumers13);

  consumerHelper.SetAttribute ("Index", StringValue("15"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers15);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("16"));
  consumerHelper.Install (consumers16);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("17"));
  consumerHelper.Install (consumers17);

  consumerHelper.SetAttribute ("Index", StringValue("18"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers18);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("19"));
  consumerHelper.Install (consumers19);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("20"));
  consumerHelper.Install (consumers20);

  consumerHelper.SetAttribute ("Index", StringValue("21"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers21);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("22"));
  consumerHelper.Install (consumers22);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("23"));
  consumerHelper.Install (consumers23);

  consumerHelper.SetAttribute ("Index", StringValue("25"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers25);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("26"));
  consumerHelper.Install (consumers26);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("27"));
  consumerHelper.Install (consumers27);

  consumerHelper.SetAttribute ("Index", StringValue("28"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers28);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("29"));
  consumerHelper.Install (consumers29);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("30"));
  consumerHelper.Install (consumers30);

  consumerHelper.SetAttribute ("Index", StringValue("32"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers32);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("33"));
  consumerHelper.Install (consumers33);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("34"));
  consumerHelper.Install (consumers34);

  consumerHelper.SetAttribute ("Index", StringValue("35"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers35);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("36"));
  consumerHelper.Install (consumers36);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("37"));
  consumerHelper.Install (consumers37);

  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));  
  producerHelper.SetPrefix ("/prefix1");
  ccnxGlobalRoutingHelper.AddOrigins ("/prefix1", producers[0]);
  producerHelper.Install (producers[0]);

  producerHelper.SetPrefix ("/prefix2");
  ccnxGlobalRoutingHelper.AddOrigins ("/prefix2", producers[1]);
  producerHelper.Install (producers[1]);

  producerHelper.SetPrefix ("/prefix3");
  ccnxGlobalRoutingHelper.AddOrigins ("/prefix3", producers[2]);
  producerHelper.Install (producers[2]);


  // Calculate and install FIBs
  //ccnxGlobalRoutingHelper.CalculateRoutes ();

  // Manually install available paths
  ndn::StackHelper::AddRoute (Names::Find<Node> ("0"), "/prefix1", Names::Find<Node> ("1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("1"), "/prefix1", Names::Find<Node> ("14"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("2"), "/prefix1", Names::Find<Node> ("1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("3"), "/prefix1", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("4"), "/prefix1", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("5"), "/prefix1", Names::Find<Node> ("4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("6"), "/prefix1", Names::Find<Node> ("5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("6"), "/prefix1", Names::Find<Node> ("7"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("7"), "/prefix1", Names::Find<Node> ("8"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("7"), "/prefix1", Names::Find<Node> ("5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("8"), "/prefix1", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("8"), "/prefix1", Names::Find<Node> ("9"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("9"), "/prefix1", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("10"), "/prefix1", Names::Find<Node> ("9"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("10"), "/prefix1", Names::Find<Node> ("0"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("11"), "/prefix1", Names::Find<Node> ("0"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("15"), "/prefix1", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("18"), "/prefix1", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("21"), "/prefix1", Names::Find<Node> ("4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("25"), "/prefix1", Names::Find<Node> ("6"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("28"), "/prefix1", Names::Find<Node> ("7"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("32"), "/prefix1", Names::Find<Node> ("9"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("35"), "/prefix1", Names::Find<Node> ("10"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("0"), "/prefix2", Names::Find<Node> ("1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("1"), "/prefix2", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("2"), "/prefix2", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("3"), "/prefix2", Names::Find<Node> ("4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("3"), "/prefix2", Names::Find<Node> ("8"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("4"), "/prefix2", Names::Find<Node> ("5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("5"), "/prefix2", Names::Find<Node> ("24"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("6"), "/prefix2", Names::Find<Node> ("5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("6"), "/prefix2", Names::Find<Node> ("7"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("7"), "/prefix2", Names::Find<Node> ("5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("8"), "/prefix2", Names::Find<Node> ("7"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("9"), "/prefix2", Names::Find<Node> ("8"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("9"), "/prefix2", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("10"), "/prefix2", Names::Find<Node> ("9"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("12"), "/prefix2", Names::Find<Node> ("0"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("16"), "/prefix2", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("19"), "/prefix2", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("22"), "/prefix2", Names::Find<Node> ("4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("26"), "/prefix2", Names::Find<Node> ("6"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("29"), "/prefix2", Names::Find<Node> ("7"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("33"), "/prefix2", Names::Find<Node> ("9"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("36"), "/prefix2", Names::Find<Node> ("10"), 1);
 
  ndn::StackHelper::AddRoute (Names::Find<Node> ("0"), "/prefix3", Names::Find<Node> ("1"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("0"), "/prefix3", Names::Find<Node> ("10"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("1"), "/prefix3", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("2"), "/prefix3", Names::Find<Node> ("9"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("2"), "/prefix3", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("3"), "/prefix3", Names::Find<Node> ("8"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("4"), "/prefix3", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("5"), "/prefix3", Names::Find<Node> ("4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("6"), "/prefix3", Names::Find<Node> ("5"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("6"), "/prefix3", Names::Find<Node> ("7"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("7"), "/prefix3", Names::Find<Node> ("8"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("8"), "/prefix3", Names::Find<Node> ("31"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("9"), "/prefix3", Names::Find<Node> ("8"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("10"), "/prefix3", Names::Find<Node> ("9"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("13"), "/prefix3", Names::Find<Node> ("0"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("17"), "/prefix3", Names::Find<Node> ("2"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("20"), "/prefix3", Names::Find<Node> ("3"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("23"), "/prefix3", Names::Find<Node> ("4"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("27"), "/prefix3", Names::Find<Node> ("6"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("30"), "/prefix3", Names::Find<Node> ("7"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("34"), "/prefix3", Names::Find<Node> ("9"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("37"), "/prefix3", Names::Find<Node> ("10"), 1);
 



  Simulator::Stop (Seconds (3000.0));

  boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<ndn::L3AggregateTracer> > > aggTracers = ndn::L3AggregateTracer::InstallAll ("abilene_mic_ls.txt", Seconds (2999));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
