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
Network topology: Two OSPF routers plus a local gateway behind the second router

  r0 -------- r1 -------- gw

r1 injects 192.168.50.0/24 through gw using an explicit gateway route. The
resulting routing dumps show the two-step lookup split between prefix ownership
and local owner resolution.
*/

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"
#include "ns3/point-to-point-module.h"

#include <filesystem>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfTwoStepGateway");

namespace {

const uint32_t SIM_SECONDS = 40;

} // namespace

int
main (int argc, char *argv[])
{
  LogComponentEnable ("OspfTwoStepGateway", LOG_LEVEL_INFO);

  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  std::filesystem::path dirName = "results/ospf-two-step-gateway";
  try
    {
      std::filesystem::create_directories (dirName);
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error: " << e.what () << std::endl;
    }

  Ptr<Node> r0 = CreateObject<Node> ();
  Ptr<Node> r1 = CreateObject<Node> ();
  Ptr<Node> gw = CreateObject<Node> ();
  NodeContainer routers (r0, r1);
  NodeContainer allNodes (r0, r1, gw);

  InternetStackHelper internet;
  internet.Install (allNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer r0r1 = p2p.Install (NodeContainer (r0, r1));
  NetDeviceContainer r1gw = p2p.Install (NodeContainer (r1, gw));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.252");
  ipv4.Assign (r0r1);
  ipv4.SetBase ("10.1.2.0", "255.255.255.252");
  Ipv4InterfaceContainer r1gwIfaces = ipv4.Assign (r1gw);

  OspfAppHelper ospfAppHelper;
  ospfAppHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (10)));
  ospfAppHelper.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (30)));
  ospfAppHelper.SetAttribute ("LSUInterval", TimeValue (Seconds (5)));

  ApplicationContainer ospfApps = ospfAppHelper.Install (routers);
  auto app0 = DynamicCast<OspfApp> (ospfApps.Get (0));
  auto app1 = DynamicCast<OspfApp> (ospfApps.Get (1));
  app0->SetRouterId (Ipv4Address ("1.1.1.1"));
  app1->SetRouterId (Ipv4Address ("1.1.1.2"));
  ospfAppHelper.ConfigureReachablePrefixesFromInterfaces (routers);

  const uint32_t gatewayIfIndex = r1gw.Get (0)->GetIfIndex ();
  app1->AddReachableAddress (gatewayIfIndex,
                             Ipv4Address ("192.168.50.0"),
                             Ipv4Mask ("255.255.255.0"),
                             r1gwIfaces.GetAddress (1),
                             5);

  ospfApps.Start (Seconds (1.0));
  ospfApps.Stop (Seconds (SIM_SECONDS));

  Simulator::Schedule (Seconds (SIM_SECONDS - 1), &OspfApp::PrintRouting, app0, dirName,
                       "r0.routes");
  Simulator::Schedule (Seconds (SIM_SECONDS - 1), &OspfApp::PrintRouting, app1, dirName,
                       "r1.routes");
  Simulator::Schedule (Seconds (SIM_SECONDS - 1.1), &OspfApp::PrintLsdb, app0);
  Simulator::Schedule (Seconds (SIM_SECONDS - 1.0), &OspfApp::PrintLsdb, app1);

  p2p.EnablePcapAll (dirName / "pcap");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}