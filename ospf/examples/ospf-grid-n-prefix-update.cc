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
//

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ospf-runtime-helper.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfGridNPrefixUpdate");

Ipv4Address ospfHelloAddress ("224.0.0.5");
const uint32_t GRID_WIDTH = 6;
const uint32_t GRID_HEIGHT = 6;
const uint32_t SIM_SECONDS = 1000;
const uint32_t MTTU = 100;
Ptr<ExponentialRandomVariable> mttuRv = CreateObject<ExponentialRandomVariable>();
Ipv4Address GenerateRandomAddress(const Ipv4Address& network, const Ipv4Mask& mask) {
  // Convert to uint32_t
  uint32_t net = network.Get();
  uint32_t msk = mask.Get();

  // Host bits are the inverse of the mask
  uint32_t hostBits = ~msk;

  // Exclude .0 (network) and .255 (broadcast) if more than 2 usable addresses
  uint32_t minHost = (hostBits > 1) ? 1 : 0;
  uint32_t maxHost = (hostBits > 1) ? (hostBits - 1) : hostBits;

  // Uniform distribution between minHost and maxHost
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
  uint32_t hostPart = rand->GetInteger(minHost, maxHost);

  uint32_t finalAddr = (net & msk) | (hostPart & hostBits);
  return Ipv4Address(finalAddr);
}

void SchedulePrefixAddition(Ptr<OspfApp> app, const Ipv4Address& network, const Ipv4Mask& mask) {
  Ipv4Address addr = GenerateRandomAddress(network, mask);
  app->AddReachableAddress(0, addr, mask, Ipv4Address("100.0.0.1"), 1);

  // Schedule the next addition
  Time nextTime = Seconds(mttuRv->GetValue());
  Simulator::Schedule(nextTime, &SchedulePrefixAddition, app, network, mask);
}

int
main (int argc, char *argv[])
{
  mttuRv->SetAttribute("Mean", DoubleValue(MTTU)); // MTTU per link
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  LogComponentEnable ("OspfGridNPrefixUpdate", LOG_LEVEL_INFO);
  // Set up some default values for the simulation.  Use the

  // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // Create results folder
  std::filesystem::path dirName = "results/ospf-grid-n-prefix-update";

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
  NetDeviceContainer ndc, ndcSeam;
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
  Ipv4AddressHelper ipv4 ("10.1.1.0", "255.255.255.252");
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
  ospfApp.Start (Seconds (1.0));
  ospfApp.Stop (Seconds (SIM_SECONDS));

  // Test Error
  // Set up random variables
  for (uint32_t i = 0; i < ospfApp.GetN(); ++i) {
    Ptr<OspfApp> app = DynamicCast<OspfApp>(ospfApp.Get(i));
    if (!app) continue;
    // Schedule initial event with randomized offset
    Time initialDelay = Seconds(10);
    auto network = Ipv4Address("100.0.0.0");
    auto mask = Ipv4Mask("255.0.0.0");
    Simulator::Schedule(initialDelay, &SchedulePrefixAddition, app, network, mask);
  }

  // Print LSDB
  Ptr<OspfApp> app;
  // for (uint32_t i = 0; i < c.GetN (); i++)
  //   {
  //     app = DynamicCast<OspfApp> (c.Get (i)->GetApplication (0));
  //     Simulator::Schedule(Seconds(30), &OspfApp::PrintLsdb, app);
  //     // Simulator::Schedule(Seconds(80), &OspfApp::PrintLsdbHash, app);
  //     // Simulator::Schedule(Seconds(SIM_SECONDS), &OspfApp::PrintLsdbHash, app);
  //   }
  app = DynamicCast<OspfApp> (c.Get (0)->GetApplication (0));
  // Simulator::Schedule (Seconds (30), &OspfApp::PrintLsdb, app);
  Simulator::Schedule (Seconds (SIM_SECONDS), &OspfApp::PrintRouting, app, dirName, "route.routes");

  // Print progress
  for (uint32_t i = 0; i < SIM_SECONDS; i += 10)
    {
      Simulator::Schedule (Seconds (i), &OspfApp::PrintRouting, app, dirName,
                           std::to_string (i) + ".routes");
      // Simulator::Schedule (Seconds (i), &OspfApp::PrintLsdb, app);
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