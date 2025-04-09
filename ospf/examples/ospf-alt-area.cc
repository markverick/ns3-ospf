/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Sirapop Theeranantachai
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
 * Author: Sirapop Theeranantachaoi <stheera@g.ucla.edu>
 */

/*
Network topology: Areas seperated by routers, not links
             A0    A1  
             n2----n4
           /          \  
 A0    A0 /            \  A1   A1   
  n0----n1              n6----n7
          \            /
           \          /  
             n3----n5
             A0    A1
*/
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"
#include "ns3/ospf-runtime-helper.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfAltArea");

Ipv4Address ospfHelloAddress ("224.0.0.5");
// Link Down at t=35
// Link Up at t=85
const uint32_t SIM_SECONDS = 105;

int
main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  LogComponentEnable ("OspfAltArea", LOG_LEVEL_INFO);
  // Set up some default values for the simulation.  Use the

  // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // Create results folder
  std::filesystem::path dirName = "results/ospf-alt-area";

  try
    {
      std::filesystem::create_directories (dirName);
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error: " << e.what () << std::endl;
    }

  // Here, we will explicitly create four nodes.  In more sophisticated
  // topologies, we could configure a node factory.
  NS_LOG_INFO ("Create nodes.");
  NodeContainer c, c0, c1;
  c.Create (8);
  for (uint32_t i = 0; i < 4; i++)
    {
      c0.Add (c.Get (i));
      c1.Add (c.Get (4 + i));
    }
  std::vector<NodeContainer> allNodes;
  allNodes.emplace_back (NodeContainer (c.Get (0), c.Get (1)));
  allNodes.emplace_back (NodeContainer (c.Get (1), c.Get (2)));
  allNodes.emplace_back (NodeContainer (c.Get (1), c.Get (3)));
  allNodes.emplace_back (NodeContainer (c.Get (2), c.Get (4)));
  allNodes.emplace_back (NodeContainer (c.Get (3), c.Get (5)));
  allNodes.emplace_back (NodeContainer (c.Get (4), c.Get (6)));
  allNodes.emplace_back (NodeContainer (c.Get (5), c.Get (6)));
  allNodes.emplace_back (NodeContainer (c.Get (6), c.Get (7)));

  InternetStackHelper internet;
  internet.Install (c);

  // We create the channels first without any IP addressing information
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install p2p net devices
  std::vector<NetDeviceContainer> allDevices;
  for (auto nodes : allNodes)
    {
      allDevices.emplace_back (p2p.Install (nodes));
    }

  // Later, we add IP addresses.

  NS_LOG_INFO ("Assign IP Addresses.");
  // Link Addresses
  Ipv4AddressHelper ipv4 ("10.1.1.0", "255.255.255.252");
  for (auto devices : allDevices)
    {
      ipv4.Assign (devices);
      ipv4.NewNetwork ();
    }

  // Create router nodes, initialize routing database and set up the routing
  // tables in the nodes.
  NS_LOG_INFO ("Configuring default routes.");
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  // Populate Routes
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  // populateBroadcastPointToPointRoute(c, ospfHelloAddress);
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  // staticRouting0->AddHostRouteTo (Ipv4Address ("10.1.3.1"), Ipv4Address ("10.1.1.2"), d0d2.Get(0)->GetIfIndex());
  // staticRouting3->AddHostRouteTo (Ipv4Address ("10.1.1.1"), Ipv4Address ("10.1.3.2"), d3d2.Get(0)->GetIfIndex());

  OspfAppHelper ospfAppHelper;
  ospfAppHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (10)));
  ospfAppHelper.SetAttribute ("HelloAddress", Ipv4AddressValue (ospfHelloAddress));
  ospfAppHelper.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (30)));
  ospfAppHelper.SetAttribute ("LSUInterval", TimeValue (Seconds (5)));

  // Area Addresses
  Ipv4Mask areaMask ("255.255.255.0");
  Ipv4AddressHelper areaIpv4 ("172.16.0.0", areaMask);
  // Install OSPF app with metrices
  ApplicationContainer ospfApp;
  for (uint32_t i = 0; i < c0.GetN (); i++)
    {
      auto app = DynamicCast<OspfApp> (ospfAppHelper.Install (c0.Get (i)).Get (0));
      app->SetArea (0, areaIpv4.NewAddress (), areaMask);
      ospfApp.Add (app);
    }
  areaIpv4.NewNetwork ();
  for (uint32_t i = 0; i < c1.GetN (); i++)
    {
      auto app = DynamicCast<OspfApp> (ospfAppHelper.Install (c1.Get (i)).Get (0));
      app->SetArea (1, areaIpv4.NewAddress (), areaMask);
      ospfApp.Add (app);
    }

  ospfAppHelper.Preload (c);
  ospfApp.Start (Seconds (1.0));
  ospfApp.Stop (Seconds (SIM_SECONDS));

  // User Traffic
  uint16_t port = 9; // well-known echo port number
  UdpEchoServerHelper server (port);
  ApplicationContainer apps = server.Install (c.Get (2));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (SIM_SECONDS));

  uint32_t tSize = 1024;
  uint32_t maxPacketCount = 200;
  Time interPacketInterval = Seconds (1.);
  UdpEchoClientHelper client (Ipv4Address ("172.16.0.3"), port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (tSize));
  apps = client.Install (c.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (SIM_SECONDS));

  // Print LSDB
  // Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintLsdb, app);
  Ptr<OspfApp> app = DynamicCast<OspfApp> (c.Get (0)->GetApplication (0));
  Simulator::Schedule (Seconds (SIM_SECONDS - 1), &OspfApp::PrintRouting, app, dirName,
                       "route0.routes");
  app = DynamicCast<OspfApp> (c.Get (7)->GetApplication (0));
  Simulator::Schedule (Seconds (SIM_SECONDS - 1), &OspfApp::PrintRouting, app, dirName,
                       "route7.routes");
  Simulator::Schedule (Seconds (SIM_SECONDS - 0.5), &CompareLsdb, c0);
  Simulator::Schedule (Seconds (SIM_SECONDS - 0.5), &CompareL1PrefixLsdb, c0);
  Simulator::Schedule (Seconds (SIM_SECONDS - 0.5), &CompareLsdb, c1);
  Simulator::Schedule (Seconds (SIM_SECONDS - 0.5), &CompareL1PrefixLsdb, c1);
  Simulator::Schedule (Seconds (SIM_SECONDS - 0.5), &CompareAreaLsdb, c);
  Simulator::Schedule (Seconds (SIM_SECONDS - 0.5), &CompareSummaryLsdb, c);
  for (int i = 0; i < 8; i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (c.Get (i)->GetApplication (0));

      Simulator::Schedule (Seconds (SIM_SECONDS - 1), &OspfApp::PrintRouting, app, dirName,
                           "route" + std::to_string (i) + ".routes");

      Simulator::Schedule (Seconds (SIM_SECONDS - 1.03), &OspfApp::PrintLsdb, app);
      Simulator::Schedule (Seconds (SIM_SECONDS - 1.02), &OspfApp::PrintL1PrefixLsdb, app);
      Simulator::Schedule (Seconds (SIM_SECONDS - 1.01), &OspfApp::PrintAreaLsdb, app);
      Simulator::Schedule (Seconds (SIM_SECONDS - 1.00), &OspfApp::PrintSummaryLsdb, app);
      // Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintRouting, app, dirName);
    }

  // Enable Pcap
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream (dirName / "ascii.tr"));
  p2p.EnablePcapAll (dirName / "pcap");

  // Flow Monitor
  FlowMonitorHelper flowmonHelper;
  if (enableFlowMonitor)
    {
      flowmonHelper.InstallAll ();
    }
  if (enableFlowMonitor)
    {
      flowmonHelper.SerializeToXmlFile (dirName / "flow.flowmon", false, false);
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}