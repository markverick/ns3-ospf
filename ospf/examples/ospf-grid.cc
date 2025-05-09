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

//
// Network topology: Grid

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfGrid");

Ipv4Address ospfHelloAddress ("224.0.0.5");

const uint32_t GRID_WIDTH = 2;
const uint32_t GRID_HEIGHT = 10;
const uint32_t SIM_SECONDS = 100;

// Fill static routes with
void
SetLinkDown (Ptr<NetDevice> nd)
{
  Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
  pem->SetRate (1.0);
  nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

void
SetLinkError (Ptr<NetDevice> nd)
{
  Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
  pem->SetRate (0.005);
  nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

void
SetLinkUp (Ptr<NetDevice> nd)
{
  Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
  pem->SetRate (0.0);
  nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

void
CompareLsdb (NodeContainer nodes)
{
  NS_ASSERT (nodes.GetN () > 0);
  Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (0)->GetApplication (0));
  uint32_t hash = app->GetLsdbHash ();

  for (uint32_t i = 1; i < nodes.GetN (); i++)
    {
      app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
      if (hash != app->GetLsdbHash ())
        {
          std::cout << "[" << Simulator::Now () << "] LSDBs mismatched" << std::endl;
          return;
        }
    }
  std::cout << "[" << Simulator::Now () << "] LSDBs matched" << std::endl;
  ;
  return;
}

void
VerifyNeighbor (NodeContainer nodes)
{
  NS_ASSERT (nodes.GetN () > 0);
  bool match = true;
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
      for (auto &pair : app->GetLsdb ())
        {
          if (pair.second.second->GetNLink () !=
              nodes.Get (app->GetNode ()->GetId ())->GetNDevices () - 1)
            {
              std::cout << "[" << Simulator::Now () << "] LSDB entry [" << Ipv4Address (pair.first)
                        << "] of node [" << i << "] is incorrect ("
                        << pair.second.second->GetNLink ()
                        << " != " << nodes.Get (app->GetNode ()->GetId ())->GetNDevices () - 1
                        << ")" << std::endl;
              match = false;
              for (uint32_t j = 0; j < pair.second.second->GetNLink (); j++)
                {
                  std::cout << "  " << Ipv4Address (pair.second.second->GetLink (j).m_linkId);
                }
              std::cout << std::endl;
            }
        }
    }
  if (match)
    std::cout << "[" << Simulator::Now () << "] LSDB entries correct" << std::endl;
  return;
}

int
main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  LogComponentEnable ("OspfGrid", LOG_LEVEL_INFO);
  // Set up some default values for the simulation.  Use the

  // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // Create results folder
  std::filesystem::path dirName = "results/ospf-grid";

  try
    {
      std::filesystem::create_directories (dirName);
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error: " << e.what () << std::endl;
    }

  // Create nodes
  NS_LOG_INFO ("Create nodes.");
  NodeContainer c;
  c.Create (GRID_HEIGHT * GRID_WIDTH);

  // Install IP Stack
  InternetStackHelper internet;
  internet.Install (c);

  // Create channels
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer ndc;
  for (uint32_t i = 0; i < GRID_HEIGHT; i++)
    {
      for (uint32_t j = 0; j < GRID_WIDTH; j++)
        {
          // Horizontal
          ndc.Add (p2p.Install (c.Get (i * GRID_WIDTH + j),
                                c.Get (i * GRID_WIDTH + ((j + 1) % GRID_WIDTH))));
          // Vertical
          ndc.Add (p2p.Install (c.Get (i * GRID_WIDTH + j),
                                c.Get (((i + 1) % GRID_HEIGHT) * GRID_WIDTH + j)));
        }
    }
  NS_LOG_INFO ("Total Net Devices Installed: " << ndc.GetN ());

  // Add IP addresses.
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4 ("10.1.1.0", "255.255.255.0");
  for (uint32_t i = 0; i < ndc.GetN (); i += 2)
    {
      ipv4.Assign (ndc.Get (i));
      ipv4.Assign (ndc.Get (i + 1));
      ipv4.NewNetwork ();
    }

  // Create router nodes, initialize routing database and set up the routing
  // tables in the nodes.
  NS_LOG_INFO ("Configuring default routes.");
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  OspfAppHelper ospfAppHelper;
  ospfAppHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (10)));
  ospfAppHelper.SetAttribute ("HelloAddress", Ipv4AddressValue (ospfHelloAddress));
  ospfAppHelper.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (30)));
  ospfAppHelper.SetAttribute ("LSUInterval", TimeValue (Seconds (5)));

  ApplicationContainer ospfApp = ospfAppHelper.Install (c);
  // ospfAppHelper.Preload (c);
  ospfApp.Start (Seconds (1.0));
  ospfApp.Stop (Seconds (SIM_SECONDS));

  // User Traffic
  // uint16_t port = 9;  // well-known echo port number
  // UdpEchoServerHelper server (port);
  // ApplicationContainer apps = server.Install (c.Get (3));
  // apps.Start (Seconds (1.0));
  // apps.Stop (Seconds (SIM_SECONDS));

  // uint32_t tSize = 1024;
  // uint32_t maxPacketCount = 200;
  // Time interPacketInterval = Seconds (1.);
  // UdpEchoClientHelper client (Ipv4Address("10.1.3.1"), port);
  // client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  // client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  // client.SetAttribute ("PacketSize", UintegerValue (tSize));
  // apps = client.Install (c.Get (1));
  // apps.Start (Seconds (2.0));
  // apps.Stop (Seconds (SIM_SECONDS));

  // Test Error
  // for (uint32_t i = 0; i < ndc.GetN(); i++) {
  //     Simulator::Schedule(Seconds(5), &SetLinkError, ndc.Get(i));
  // }
  // Simulator::Schedule(Seconds(35), &SetLinkDown, ndc.Get(0));
  // Simulator::Schedule(Seconds(35), &SetLinkDown, ndc.Get(1));
  // Simulator::Schedule(Seconds(85), &SetLinkUp, ndc.Get(0));
  // Simulator::Schedule(Seconds(85), &SetLinkUp, ndc.Get(1));

  // Print LSDB
  Ptr<OspfApp> app;
  for (uint32_t i = 0; i < c.GetN (); i++)
    {
      app = DynamicCast<OspfApp> (c.Get (i)->GetApplication (0));
      // Simulator::Schedule(Seconds(1.001), &OspfApp::PrintLsdbHash, app);
      // Simulator::Schedule(Seconds(30), &OspfApp::PrintLsdbHash, app);
      // Simulator::Schedule(Seconds(80), &OspfApp::PrintLsdbHash, app);
      // Simulator::Schedule(Seconds(SIM_SECONDS), &OspfApp::PrintLsdbHash, app);
    }
  app = DynamicCast<OspfApp> (c.Get (0)->GetApplication (0));
  Simulator::Schedule (Seconds (SIM_SECONDS), &OspfApp::PrintLsdb, app);
  Simulator::Schedule (Seconds (100), &OspfApp::PrintRouting, app, dirName, "route.routes");

  // Print progress
  for (uint32_t i = 0; i < SIM_SECONDS; i += 10)
    {
      Simulator::Schedule (Seconds (i), &OspfApp::PrintLsdb, app);
    }
  Simulator::Schedule (Seconds (SIM_SECONDS), CompareLsdb, c);
  Simulator::Schedule (Seconds (SIM_SECONDS), VerifyNeighbor, c);
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