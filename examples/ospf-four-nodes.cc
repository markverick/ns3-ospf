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
//  n0
//     \ 5 Mb/s, 2ms
//      \          1.5Mb/s, 10ms
//       n2 -------------------------n3
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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OSPFFourNode");

Ipv4Address ospfHelloAddress("224.0.0.5");
const uint32_t SIM_SECONDS = 120;

void AddRouteCustom(Ptr<Ipv4StaticRouting> staticRouting, Ipv4Address dest, Ipv4Address nextHop, uint32_t interface, uint32_t metric=0) {
    staticRouting->AddHostRouteTo(dest, nextHop, interface, metric);
}

// Fill static routes with 
void populateBroadcastPointToPointRoute(NodeContainer c, Ipv4Address helloAddress) {
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < c.GetN(); i++) {
        Ptr<Node> n = c.Get(i);
        Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
        Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (ipv4);
        if (n->GetNDevices() == 0) continue;
        std::vector<uint32_t> outputInterfaces;
        for (uint32_t j = 1; j < n->GetNDevices(); j++) {
            outputInterfaces.emplace_back(n->GetDevice(j)->GetIfIndex());
            // NS_LOG_INFO(n->GetDevice(j)->GetAddress());
        }
        NS_LOG_INFO(outputInterfaces.size());
        routing->AddMulticastRoute(Ipv4Address::GetAny(), helloAddress, n->GetDevice(1)->GetIfIndex(), outputInterfaces);
    }
}

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
    LogComponentEnable ("OSPFFourNode", LOG_LEVEL_INFO);
    // Set up some default values for the simulation.  Use the

    // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

    // Allow the user to override any of the defaults and the above
    // DefaultValue::Bind ()s at run-time, via command-line arguments
    CommandLine cmd(__FILE__);
    bool enableFlowMonitor = false;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Here, we will explicitly create four nodes.  In more sophisticated
    // topologies, we could configure a node factory.
    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(4);
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
    NodeContainer n3n2 = NodeContainer(c.Get(3), c.Get(2));

    InternetStackHelper internet;
    internet.Install(c);

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

    // Later, we add IP addresses.


    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4("10.1.1.0", "255.255.255.252");
    ipv4.Assign(d0d2);
    ipv4.NewNetwork();
    ipv4.Assign(d1d2);
    ipv4.NewNetwork();
    ipv4.Assign(d3d2);

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

    OSPFAppHelper ospfAppHelper(9);
    ospfAppHelper.SetAttribute("HelloInterval", TimeValue(Seconds(10)));
    ospfAppHelper.SetAttribute("HelloAddress", Ipv4AddressValue(ospfHelloAddress));
    ospfAppHelper.SetAttribute("NeighborTimeout", TimeValue(Seconds(30)));
    ospfAppHelper.SetAttribute("LSUInterval", TimeValue(Seconds(5)));
 
    ApplicationContainer ospfApp = ospfAppHelper.Install(c);
    ospfApp.Start(Seconds(1.0));
    ospfApp.Stop(Seconds(SIM_SECONDS));

    // User Traffic
    uint16_t port = 9;  // well-known echo port number
    UdpEchoServerHelper server (port);
    ApplicationContainer apps = server.Install (c.Get (3));
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (SIM_SECONDS));

    uint32_t tSize = 1024;
    uint32_t maxPacketCount = 200;
    Time interPacketInterval = Seconds (1.);
    UdpEchoClientHelper client (Ipv4Address("10.1.1.10"), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (tSize));
    apps = client.Install (c.Get (1));
    apps.Start (Seconds (2.0));
    apps.Stop (Seconds (SIM_SECONDS));

    // Test Error
    Simulator::Schedule(Seconds(35), &SetLinkDown, d1d2.Get(0));
    Simulator::Schedule(Seconds(35), &SetLinkDown, d1d2.Get(1));
    Simulator::Schedule(Seconds(85), &SetLinkUp, d1d2.Get(0));
    Simulator::Schedule(Seconds(85), &SetLinkUp, d1d2.Get(1));

    // Print LSDB
    Ptr<OSPFApp> app  = DynamicCast<OSPFApp>(c.Get(0)->GetApplication(0));
    Simulator::Schedule(Seconds(145), &OSPFApp::PrintLSDB, app);
    Simulator::Schedule(Seconds(145), &OSPFApp::PrintRouting, app);
    // app  = DynamicCast<OSPFApp>(c.Get(1)->GetApplication(0));
    // Simulator::Schedule(Seconds(146), &OSPFApp::PrintLSDB, app);
    // app  = DynamicCast<OSPFApp>(c.Get(2)->GetApplication(0));
    // Simulator::Schedule(Seconds(147), &OSPFApp::PrintLSDB, app);
    // app  = DynamicCast<OSPFApp>(c.Get(3)->GetApplication(0));
    // Simulator::Schedule(Seconds(148), &OSPFApp::PrintLSDB, app);



    // Enable Pcap
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll (ascii.CreateFileStream ("simple-ospf-routing.tr"));
    p2p.EnablePcapAll ("simple-ospf-routing");

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    if (enableFlowMonitor)
    {
      flowmonHelper.InstallAll ();
    }
    if (enableFlowMonitor)
    {
      flowmonHelper.SerializeToXmlFile ("simple-ospf-routing.flowmon", false, false);
    }
    
 
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}