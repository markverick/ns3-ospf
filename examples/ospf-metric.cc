/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2024 University of California, Los Angeles
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
 * Author: Sirapop Theeranantachai (stheera@g.ucla.edu)
 */

//
// Network topology
//
//            n1
//       6  /    \  1
//         /      \          10
//       n0        n3-------------------------n4
//         \      /
//       1  \    /  3
//            n2
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

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfMetric");

Ipv4Address ospfHelloAddress("224.0.0.5");
// Link Down at t=35
// Link Up at t=85
const uint32_t SIM_SECONDS = 105;

int
main(int argc, char* argv[])
{
    // Users may find it convenient to turn on explicit debugging
    // for selected modules; the below lines suggest how to do this
    LogComponentEnable ("OspfMetric", LOG_LEVEL_INFO);
    // Set up some default values for the simulation.  Use the

    // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

    // Allow the user to override any of the defaults and the above
    // DefaultValue::Bind ()s at run-time, via command-line arguments
    CommandLine cmd(__FILE__);
    bool enableFlowMonitor = false;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Create results folder
    std::filesystem::path dirName = "results/ospf-metric";
  
    try {
        std::filesystem::create_directories(dirName);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Here, we will explicitly create four nodes.  In more sophisticated
    // topologies, we could configure a node factory.
    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(5);
    NodeContainer n0n1 = NodeContainer(c.Get(0), c.Get(1));
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n3 = NodeContainer(c.Get(1), c.Get(3));
    NodeContainer n2n3 = NodeContainer(c.Get(2), c.Get(3));
    NodeContainer n3n4 = NodeContainer(c.Get(3), c.Get(4));

    // Prepare interface metrices
    std::vector<uint32_t> m0 = {0, 6, 1};
    std::vector<uint32_t> m1 = {0, 6, 1};
    std::vector<uint32_t> m2 = {0, 1, 3};
    std::vector<uint32_t> m3 = {0, 1, 3, 10};
    std::vector<uint32_t> m4 = {0, 10};

    InternetStackHelper internet;
    internet.Install(c);

    // We create the channels first without any IP addressing information
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d3 = p2p.Install(n1n3);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    p2p.SetDeviceAttribute("DataRate", StringValue("1500kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d3d4 = p2p.Install(n3n4);

    // Later, we add IP addresses.


    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4("10.1.1.0", "255.255.255.252");
    ipv4.Assign(d0d1);
    ipv4.NewNetwork();
    ipv4.Assign(d0d2);
    ipv4.NewNetwork();
    ipv4.Assign(d1d3);
    ipv4.NewNetwork();
    ipv4.Assign(d2d3);
    ipv4.NewNetwork();
    ipv4.Assign(d3d4);

    // Create router nodes, initialize routing database and set up the routing
    // tables in the nodes.
    NS_LOG_INFO("Configuring default routes.");
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    // Populate Routes
    // Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    // populateBroadcastPointToPointRoute(c, ospfHelloAddress);
    // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    // staticRouting0->AddHostRouteTo (Ipv4Address ("10.1.3.1"), Ipv4Address ("10.1.1.2"), d0d2.Get(0)->GetIfIndex());
    // staticRouting3->AddHostRouteTo (Ipv4Address ("10.1.1.1"), Ipv4Address ("10.1.3.2"), d3d2.Get(0)->GetIfIndex());

    OspfAppHelper ospfAppHelper(9);
    ospfAppHelper.SetAttribute("HelloInterval", TimeValue(Seconds(10)));
    ospfAppHelper.SetAttribute("HelloAddress", Ipv4AddressValue(ospfHelloAddress));
    ospfAppHelper.SetAttribute("NeighborTimeout", TimeValue(Seconds(30)));
    ospfAppHelper.SetAttribute("LSUInterval", TimeValue(Seconds(5)));
 
    // Install OSPF app with metrices
    std::vector<uint32_t> emptyVec;
    ApplicationContainer ospfApp;
    ospfApp.Add(ospfAppHelper.Install(c.Get(0), emptyVec, m0));
    ospfApp.Add(ospfAppHelper.Install(c.Get(1), emptyVec, m1));
    ospfApp.Add(ospfAppHelper.Install(c.Get(2), emptyVec, m2));
    ospfApp.Add(ospfAppHelper.Install(c.Get(3), emptyVec, m3));
    ospfApp.Add(ospfAppHelper.Install(c.Get(4), emptyVec, m4));


    ospfApp.Start(Seconds(1.0));
    ospfApp.Stop(Seconds(SIM_SECONDS));

    // // User Traffic
    // uint16_t port = 9;  // well-known echo port number
    // UdpEchoServerHelper server (port);
    // ApplicationContainer apps = server.Install (c.Get (4));
    // apps.Start (Seconds (1.0));
    // apps.Stop (Seconds (SIM_SECONDS));

    // uint32_t tSize = 1024;
    // uint32_t maxPacketCount = 200;
    // Time interPacketInterval = Seconds (1.);
    // UdpEchoClientHelper client (Ipv4Address("10.1.1.9"), port);
    // client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    // client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    // client.SetAttribute ("PacketSize", UintegerValue (tSize));
    // apps = client.Install (c.Get (0));
    // apps.Start (Seconds (2.0));
    // apps.Stop (Seconds (SIM_SECONDS));


    // Print LSDB
    Ptr<OspfApp> app  = DynamicCast<OspfApp>(c.Get(1)->GetApplication(0));
    // Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintLsdb, app);
    Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintRouting, app, dirName);
    for (int i = 0; i < 5; i++) {
        Ptr<OspfApp> app  = DynamicCast<OspfApp>(c.Get(i)->GetApplication(0));
        Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintLsdb, app);
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
    
 
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}