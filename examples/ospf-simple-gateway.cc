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
// Network topology
//
//  n0
//     \ 5 Mb/s, 2ms
//      \          1.5Mb/s, 10ms
//       n2 -------------------------n3--n4 (gateway)
//      /
//     / 5 Mb/s, 2ms
//   n1
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

NS_LOG_COMPONENT_DEFINE("OspfSimpleGateway");

Ipv4Address ospfHelloAddress("224.0.0.5");
const uint32_t SIM_SECONDS = 16;

void SetLinkDown(Ptr<NetDevice> nd) {
    Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
    pem->SetRate(1.0);
    nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

void SetLinkUp(Ptr<NetDevice> nd) {
    Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
    pem->SetRate(0.0);
    nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

int
main(int argc, char* argv[])
{
    // Users may find it convenient to turn on explicit debugging
    // for selected modules; the below lines suggest how to do this
    LogComponentEnable ("OspfSimpleGateway", LOG_LEVEL_INFO);
    // Set up some default values for the simulation.  Use the

    // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

    // Allow the user to override any of the defaults and the above
    // DefaultValue::Bind ()s at run-time, via command-line arguments
    CommandLine cmd(__FILE__);
    bool enableFlowMonitor = false;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Create results folder
    std::filesystem::path dirName = "results/ospf-simple-gateway";
  
    try {
        std::filesystem::create_directories(dirName);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }


    // Here, we will explicitly create four nodes.  In more sophisticated
    // topologies, we could configure a node factory.
    NS_LOG_INFO("Create nodes.");
    NodeContainer c, d;
    c.Create(4);
    d.Create(1);
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
    NodeContainer n3n2 = NodeContainer(c.Get(3), c.Get(2));
    NodeContainer n3n4 = NodeContainer(c.Get(3), d.Get(0));

    InternetStackHelper internet;
    internet.Install(c);
    internet.Install(d);

    // We create the channels first without any IP addressing information
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer d0d2 = p2p.Install(n0n2);


    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    p2p.SetDeviceAttribute("DataRate", StringValue("1500kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d3d2 = p2p.Install(n3n2);
    NetDeviceContainer d3d4 = p2p.Install(n3n4);

    // Later, we add IP addresses.


    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4("10.1.1.0", "255.255.255.252");
    ipv4.Assign(d0d2);
    ipv4.NewNetwork();
    ipv4.Assign(d1d2);
    ipv4.NewNetwork();
    ipv4.Assign(d3d2);

    Ipv4AddressHelper external("8.8.8.8", "255.255.255.252");
    external.Assign(d3d4);

    // Create router nodes, initialize routing database and set up the routing
    // tables in the nodes.
    NS_LOG_INFO("Configuring default routes.");
    Ipv4StaticRoutingHelper ipv4RoutingHelper;


    OspfAppHelper ospfAppHelper(9);
    ospfAppHelper.SetAttribute("HelloInterval", TimeValue(Seconds(10)));
    ospfAppHelper.SetAttribute("HelloAddress", Ipv4AddressValue(ospfHelloAddress));
    ospfAppHelper.SetAttribute("RouterDeadInterval", TimeValue(Seconds(30)));
    ospfAppHelper.SetAttribute("LSUInterval", TimeValue(Seconds(5)));
 
    ApplicationContainer ospfApp = ospfAppHelper.Install(c);
    std::vector<uint32_t> ifIndices;
    ifIndices.emplace_back(d3d4.Get(0)->GetIfIndex());
    ospfAppHelper.InstallGateway(c.Get(3), ifIndices, Ipv4Address("8.8.8.10"));
    ospfApp.Start(Seconds(1.0));
    ospfApp.Stop(Seconds(SIM_SECONDS));

    // User Traffic
    uint16_t port = 9;  // well-known echo port number
    UdpEchoServerHelper server (port);
    ApplicationContainer apps = server.Install (d.Get (0));
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (SIM_SECONDS));

    uint32_t tSize = 1024;
    uint32_t maxPacketCount = 200;
    Time interPacketInterval = Seconds (1.);
    UdpEchoClientHelper client (Ipv4Address("8.8.8.10"), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (tSize));
    apps = client.Install (c.Get (0));
    apps.Start (Seconds (2.0));
    apps.Stop (Seconds (SIM_SECONDS));

    // Set c3 as gateway
    Ptr<OspfApp> c3App  = DynamicCast<OspfApp>(c.Get(3)->GetApplication(0));
    // c3App->AddInterfaceNeighbor(d3d4.Get(0)->GetIfIndex(), Ipv4Address("0.0.0.0"), Ipv4Address("10.1.1.14"));
    // Print LSDB
    Ptr<OspfApp> app  = DynamicCast<OspfApp>(c.Get(1)->GetApplication(0));
    Simulator::Schedule(Seconds(100), &OspfApp::PrintLsdb, app);
    Simulator::Schedule(Seconds(100), &OspfApp::PrintRouting, app, dirName, "route.routes");

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