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
Network topology: two routers in area 0, two routers in area 1

  a0-r0 -------- a0-r1 -------- a1-r0 -------- a1-r1

The example uses ReachableLowestRouterId leader selection and injects one stub
prefix at each edge so Area-LSAs and L2 Summary-LSAs are easy to inspect.
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

NS_LOG_COMPONENT_DEFINE ("OspfAreaLeader");

namespace {

const uint32_t SIM_SECONDS = 60;

void
PrintLeaderState (NodeContainer nodes)
{
  std::cout << "==== Area leader state at t=" << Simulator::Now ().GetSeconds () << " ===="
            << std::endl;
  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
    {
      auto app = DynamicCast<OspfApp> ((*i)->GetApplication (0));
      std::cout << "  router=" << app->GetRouterId () << " area=" << app->GetArea ()
                << " leader=" << (app->IsAreaLeader () ? "yes" : "no") << std::endl;
    }
  std::cout << std::endl;
}

} // namespace

int
main (int argc, char *argv[])
{
  LogComponentEnable ("OspfAreaLeader", LOG_LEVEL_INFO);

  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  std::filesystem::path dirName = "results/ospf-area-leader";
  try
    {
      std::filesystem::create_directories (dirName);
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error: " << e.what () << std::endl;
    }

  NodeContainer c;
  c.Create (4);

  InternetStackHelper internet;
  internet.Install (c);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d01 = p2p.Install (NodeContainer (c.Get (0), c.Get (1)));
  NetDeviceContainer d12 = p2p.Install (NodeContainer (c.Get (1), c.Get (2)));
  NetDeviceContainer d23 = p2p.Install (NodeContainer (c.Get (2), c.Get (3)));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.252");
  ipv4.Assign (d01);
  ipv4.SetBase ("10.1.2.0", "255.255.255.252");
  ipv4.Assign (d12);
  ipv4.SetBase ("10.1.3.0", "255.255.255.252");
  ipv4.Assign (d23);

  OspfAppHelper ospfAppHelper;
  ospfAppHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (10)));
  ospfAppHelper.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (30)));
  ospfAppHelper.SetAttribute ("LSUInterval", TimeValue (Seconds (5)));
  ospfAppHelper.SetAttribute (
      "AreaLeaderMode",
      EnumValue (OspfApp::AREA_LEADER_REACHABLE_LOWEST_ROUTER_ID));

  ApplicationContainer ospfApps = ospfAppHelper.Install (c);
  auto app0 = DynamicCast<OspfApp> (ospfApps.Get (0));
  auto app1 = DynamicCast<OspfApp> (ospfApps.Get (1));
  auto app2 = DynamicCast<OspfApp> (ospfApps.Get (2));
  auto app3 = DynamicCast<OspfApp> (ospfApps.Get (3));

  app0->SetRouterId (Ipv4Address ("1.0.0.10"));
  app1->SetRouterId (Ipv4Address ("1.0.0.11"));
  app2->SetRouterId (Ipv4Address ("1.0.0.20"));
  app3->SetRouterId (Ipv4Address ("1.0.0.21"));

  app0->SetArea (0);
  app1->SetArea (0);
  app2->SetArea (1);
  app3->SetArea (1);
  ospfAppHelper.ConfigureReachablePrefixesFromInterfaces (c);

  app0->AddReachableAddress (0, Ipv4Address ("172.16.0.0"), Ipv4Mask ("255.255.255.0"));
  app3->AddReachableAddress (0, Ipv4Address ("172.16.1.0"), Ipv4Mask ("255.255.255.0"));

  ospfApps.Start (Seconds (1.0));
  ospfApps.Stop (Seconds (SIM_SECONDS));

  Simulator::Schedule (Seconds (20), &PrintLeaderState, c);
  Simulator::Schedule (Seconds (SIM_SECONDS - 2), &PrintLeaderState, c);
  for (uint32_t i = 0; i < c.GetN (); ++i)
    {
      auto app = DynamicCast<OspfApp> (ospfApps.Get (i));
      Simulator::Schedule (Seconds (SIM_SECONDS - 1), &OspfApp::PrintRouting, app, dirName,
                           "n" + std::to_string (i) + ".routes");
      Simulator::Schedule (Seconds (SIM_SECONDS - 1.03), &OspfApp::PrintLsdb, app);
      Simulator::Schedule (Seconds (SIM_SECONDS - 1.02), &OspfApp::PrintL1SummaryLsdb, app);
      Simulator::Schedule (Seconds (SIM_SECONDS - 1.01), &OspfApp::PrintAreaLsdb, app);
      Simulator::Schedule (Seconds (SIM_SECONDS - 1.00), &OspfApp::PrintL2SummaryLsdb, app);
    }

  p2p.EnablePcapAll (dirName / "pcap");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}