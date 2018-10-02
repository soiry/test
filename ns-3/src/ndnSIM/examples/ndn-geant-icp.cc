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
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-geant.txt");
  topologyReader.Read ();
  
 
  NodeContainer rtrs;
  rtrs.Add( Names::Find<Node> ("DE"));
  rtrs.Add( Names::Find<Node> ("DK"));
  rtrs.Add( Names::Find<Node> ("PL"));
  rtrs.Add( Names::Find<Node> ("CZ"));
  rtrs.Add( Names::Find<Node> ("AT"));
  rtrs.Add( Names::Find<Node> ("CH"));
  rtrs.Add( Names::Find<Node> ("LU"));
  rtrs.Add( Names::Find<Node> ("NL"));
  rtrs.Add( Names::Find<Node> ("NO"));
  rtrs.Add( Names::Find<Node> ("SE"));
  rtrs.Add( Names::Find<Node> ("LT"));
  rtrs.Add( Names::Find<Node> ("SK"));
  rtrs.Add( Names::Find<Node> ("SI"));
  rtrs.Add( Names::Find<Node> ("IT"));
  rtrs.Add( Names::Find<Node> ("FR"));
  rtrs.Add( Names::Find<Node> ("BE"));
  rtrs.Add( Names::Find<Node> ("FI"));
  rtrs.Add( Names::Find<Node> ("EE"));
  rtrs.Add( Names::Find<Node> ("LV"));
  rtrs.Add( Names::Find<Node> ("RU"));
  rtrs.Add( Names::Find<Node> ("HU"));
  rtrs.Add( Names::Find<Node> ("HR"));
  rtrs.Add( Names::Find<Node> ("MT"));
  rtrs.Add( Names::Find<Node> ("ES"));
  rtrs.Add( Names::Find<Node> ("UK"));
  rtrs.Add( Names::Find<Node> ("IS"));
  rtrs.Add( Names::Find<Node> ("RO"));
  rtrs.Add( Names::Find<Node> ("RS"));
  rtrs.Add( Names::Find<Node> ("ME"));
  rtrs.Add( Names::Find<Node> ("PT"));
  rtrs.Add( Names::Find<Node> ("IE"));
  rtrs.Add( Names::Find<Node> ("BG"));
  rtrs.Add( Names::Find<Node> ("MK"));
  rtrs.Add( Names::Find<Node> ("GR"));
  rtrs.Add( Names::Find<Node> ("CY"));
  rtrs.Add( Names::Find<Node> ("TR"));
  rtrs.Add( Names::Find<Node> ("IL"));

  NodeContainer consumers11;
  consumers11.Add( Names::Find<Node> ("11"));
  NodeContainer consumers12;
  consumers12.Add( Names::Find<Node> ("12"));
  NodeContainer consumers13;
  consumers13.Add( Names::Find<Node> ("13"));
  NodeContainer consumers14;
  consumers14.Add( Names::Find<Node> ("14"));
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
  NodeContainer consumers24;
  consumers24.Add( Names::Find<Node> ("24"));
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
  NodeContainer consumers31;
  consumers31.Add( Names::Find<Node> ("31"));
  NodeContainer consumers32;
  consumers32.Add( Names::Find<Node> ("32"));
  NodeContainer consumers33;
  consumers33.Add( Names::Find<Node> ("33"));
  NodeContainer consumers34;
  consumers34.Add( Names::Find<Node> ("34"));

  NodeContainer producer;
  producer.Add( Names::Find<Node> ("DK"));
  producer.Add( Names::Find<Node> ("ES"));
  producer.Add( Names::Find<Node> ("AT"));

  Ptr<Node> producers[3] = { Names::Find<Node> ("DK"), Names::Find<Node> ("ES"),
                             Names::Find<Node> ("AT")
  };

  // Install CCNx stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
  ndnHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "100830");
  ndnHelper.Install(rtrs);

  ndn::StackHelper subHelper;
  subHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
  subHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "1");
  subHelper.Install(consumers11);
  subHelper.Install(consumers12);
  subHelper.Install(consumers13);
  subHelper.Install(consumers14);
  subHelper.Install(consumers15);
  subHelper.Install(consumers16);
  subHelper.Install(consumers17);
  subHelper.Install(consumers18);
  subHelper.Install(consumers19);
  subHelper.Install(consumers20);
  subHelper.Install(consumers21);
  subHelper.Install(consumers22);
  subHelper.Install(consumers23);
  subHelper.Install(consumers24);
  subHelper.Install(consumers25);
  subHelper.Install(consumers26);
  subHelper.Install(consumers27);
  subHelper.Install(consumers28);
  subHelper.Install(consumers29);
  subHelper.Install(consumers30);
  subHelper.Install(consumers31);
  subHelper.Install(consumers32);
  subHelper.Install(consumers33);
  subHelper.Install(consumers34);

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ccnxGlobalRoutingHelper;
  ccnxGlobalRoutingHelper.InstallAll ();

  
  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerWindowSingle");
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.SetAttribute ("Size", DoubleValue(10));
  consumerHelper.SetAttribute ("Frequency", StringValue("0.4"));
  consumerHelper.SetAttribute ("Randomize", StringValue("exponential"));
  consumerHelper.SetAttribute ("Window", StringValue("10"));
  consumerHelper.SetAttribute ("Index", StringValue("11"));
  consumerHelper.Install (consumers11);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("12"));
  consumerHelper.Install (consumers12);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("13"));
  consumerHelper.Install (consumers13);

  consumerHelper.SetAttribute ("Index", StringValue("14"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers14);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("15"));
  consumerHelper.Install (consumers15);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("16"));
  consumerHelper.Install (consumers16);

  consumerHelper.SetAttribute ("Index", StringValue("17"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers17);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("18"));
  consumerHelper.Install (consumers18);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("19"));
  consumerHelper.Install (consumers19);

  consumerHelper.SetAttribute ("Index", StringValue("20"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers20);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("21"));
  consumerHelper.Install (consumers21);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("22"));
  consumerHelper.Install (consumers22);

  consumerHelper.SetAttribute ("Index", StringValue("23"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers23);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("24"));
  consumerHelper.Install (consumers24);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("25"));
  consumerHelper.Install (consumers25);

  consumerHelper.SetAttribute ("Index", StringValue("26"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers26);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("27"));
  consumerHelper.Install (consumers27);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("28"));
  consumerHelper.Install (consumers28);

  consumerHelper.SetAttribute ("Index", StringValue("29"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers29);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("30"));
  consumerHelper.Install (consumers30);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("31"));
  consumerHelper.Install (consumers31);

  consumerHelper.SetAttribute ("Index", StringValue("32"));
  consumerHelper.SetPrefix ("/prefix1");
  consumerHelper.Install (consumers32);
  consumerHelper.SetPrefix ("/prefix2");
  consumerHelper.SetAttribute ("Index", StringValue("33"));
  consumerHelper.Install (consumers33);
  consumerHelper.SetPrefix ("/prefix3");
  consumerHelper.SetAttribute ("Index", StringValue("34"));
  consumerHelper.Install (consumers34);

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
  ccnxGlobalRoutingHelper.CalculateRoutes ();

  // Manually install available paths
  /*
  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix1", Names::Find<Node> ("UK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("UK"), "/prefix1", Names::Find<Node> ("IS"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IS"), "/prefix1", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("UK"), "/prefix1", Names::Find<Node> ("BE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("BE"), "/prefix1", Names::Find<Node> ("NL"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("NL"), "/prefix1", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix1", Names::Find<Node> ("LU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LU"), "/prefix1", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix1", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix1", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("DE"), "/prefix1", Names::Find<Node> ("NL"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix1", Names::Find<Node> ("FR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix1", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IT"), "/prefix1", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("AT"), "/prefix1", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IT"), "/prefix1", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("AT"), "/prefix1", Names::Find<Node> ("SK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SK"), "/prefix1", Names::Find<Node> ("CZ"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CZ"), "/prefix1", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("BG"), "/prefix1", Names::Find<Node> ("GR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("GR"), "/prefix1", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("GR"), "/prefix1", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("BG"), "/prefix1", Names::Find<Node> ("HU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("HU"), "/prefix1", Names::Find<Node> ("SK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SK"), "/prefix1", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("RO"), "/prefix1", Names::Find<Node> ("HU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("RO"), "/prefix1", Names::Find<Node> ("BG"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SE"), "/prefix1", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SE"), "/prefix1", Names::Find<Node> ("NO"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("NO"), "/prefix1", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LV"), "/prefix1", Names::Find<Node> ("EE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("EE"), "/prefix1", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LV"), "/prefix1", Names::Find<Node> ("LT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LT"), "/prefix1", Names::Find<Node> ("PL"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("PL"), "/prefix1", Names::Find<Node> ("DE"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("11"), "/prefix1", Names::Find<Node> ("FR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("14"), "/prefix1", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("17"), "/prefix1", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("20"), "/prefix1", Names::Find<Node> ("BG"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("23"), "/prefix1", Names::Find<Node> ("GR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("26"), "/prefix1", Names::Find<Node> ("RO"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("29"), "/prefix1", Names::Find<Node> ("SE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("32"), "/prefix1", Names::Find<Node> ("LV"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix2", Names::Find<Node> ("ES"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix2", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix2", Names::Find<Node> ("ES"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix2", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IT"), "/prefix2", Names::Find<Node> ("ES"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix2", Names::Find<Node> ("UK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("UK"), "/prefix2", Names::Find<Node> ("PT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("PT"), "/prefix2", Names::Find<Node> ("ES"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix2", Names::Find<Node> ("FR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IT"), "/prefix2", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("BG"), "/prefix2", Names::Find<Node> ("GR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("GR"), "/prefix2", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("BG"), "/prefix2", Names::Find<Node> ("HU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("HU"), "/prefix2", Names::Find<Node> ("SK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SK"), "/prefix2", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("AT"), "/prefix2", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("GR"), "/prefix2", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("AT"), "/prefix2", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("DE"), "/prefix2", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("RO"), "/prefix2", Names::Find<Node> ("BG"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("RO"), "/prefix2", Names::Find<Node> ("HU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SE"), "/prefix2", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("DK"), "/prefix2", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SE"), "/prefix2", Names::Find<Node> ("NO"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("NO"), "/prefix2", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LV"), "/prefix2", Names::Find<Node> ("EE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("EE"), "/prefix2", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LV"), "/prefix2", Names::Find<Node> ("LT"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("12"), "/prefix2", Names::Find<Node> ("FR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("15"), "/prefix2", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("18"), "/prefix2", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("21"), "/prefix2", Names::Find<Node> ("BG"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("24"), "/prefix2", Names::Find<Node> ("GR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("27"), "/prefix2", Names::Find<Node> ("RO"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("30"), "/prefix2", Names::Find<Node> ("SE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("33"), "/prefix2", Names::Find<Node> ("LV"), 1);
 
  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix3", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix3", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IT"), "/prefix3", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IT"), "/prefix3", Names::Find<Node> ("GR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("GR"), "/prefix3", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CH"), "/prefix3", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("DE"), "/prefix3", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("FR"), "/prefix3", Names::Find<Node> ("LU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LU"), "/prefix3", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("IT"), "/prefix3", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("BG"), "/prefix3", Names::Find<Node> ("GR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("GR"), "/prefix3", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("BG"), "/prefix3", Names::Find<Node> ("HU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("HU"), "/prefix3", Names::Find<Node> ("SK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SK"), "/prefix3", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("HU"), "/prefix3", Names::Find<Node> ("HR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("HR"), "/prefix3", Names::Find<Node> ("SI"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SI"), "/prefix3", Names::Find<Node> ("AT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("RO"), "/prefix3", Names::Find<Node> ("HU"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("RO"), "/prefix3", Names::Find<Node> ("BG"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("SE"), "/prefix3", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("DK"), "/prefix3", Names::Find<Node> ("DE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LV"), "/prefix3", Names::Find<Node> ("EE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("EE"), "/prefix3", Names::Find<Node> ("DK"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LV"), "/prefix3", Names::Find<Node> ("LT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("LT"), "/prefix3", Names::Find<Node> ("PL"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("PL"), "/prefix3", Names::Find<Node> ("CZ"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("CZ"), "/prefix3", Names::Find<Node> ("SK"), 1);

  ndn::StackHelper::AddRoute (Names::Find<Node> ("13"), "/prefix3", Names::Find<Node> ("FR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("16"), "/prefix3", Names::Find<Node> ("CH"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("19"), "/prefix3", Names::Find<Node> ("IT"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("22"), "/prefix3", Names::Find<Node> ("BG"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("25"), "/prefix3", Names::Find<Node> ("GR"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("28"), "/prefix3", Names::Find<Node> ("RO"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("31"), "/prefix3", Names::Find<Node> ("SE"), 1);
  ndn::StackHelper::AddRoute (Names::Find<Node> ("34"), "/prefix3", Names::Find<Node> ("LV"), 1);
 */



  Simulator::Stop (Seconds (1500.0));

  boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<ndn::L3AggregateTracer> > > aggTracers = ndn::L3AggregateTracer::InstallAll ("geant_icp_ls.txt", Seconds (1499));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
