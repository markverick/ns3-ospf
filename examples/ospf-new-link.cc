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
#include <filesystem>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfFourNode");

Ipv4Address ospfHelloAddress("224.0.0.5");

const uint32_t SIM_SECONDS = 100;

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
    LogComponentEnable ("OspfFourNode", LOG_LEVEL_INFO);
    // Set up some default values for the simulation.  Use the

    // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

    // Allow the user to override any of the defaults and the above
    // DefaultValue::Bind ()s at run-time, via command-line arguments
    CommandLine cmd(__FILE__);
    bool enableFlowMonitor = false;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Create results folder
    std::filesystem::path dirName = "results/ospf-new-link";
  
    try {
        std::filesystem::create_directories(dirName);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Here, we will explicitly create four nodes.  In more sophisticated
    // topologies, we could configure a node factory.
    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(4);
    NodeContainer n0n1 = NodeContainer(c.Get(0), c.Get(1));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
    NodeContainer n2n3 = NodeContainer(c.Get(2), c.Get(3));

    InternetStackHelper internet;
    internet.Install(c);

    // We create the channels first without any IP addressing information
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // Later, we add IP addresses.


    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4("10.1.1.0", "255.255.255.252");
    ipv4.Assign(d0d1);
    ipv4.NewNetwork();
    ipv4.Assign(d1d2);
    ipv4.NewNetwork();
    ipv4.Assign(d2d3);

    // Fake new link (for now)
    Simulator::Schedule(Seconds(0.5), &SetLinkDown, d2d3.Get(0));
    Simulator::Schedule(Seconds(0.5), &SetLinkDown, d2d3.Get(1));
    Simulator::Schedule(Seconds(50), &SetLinkUp, d2d3.Get(0));
    Simulator::Schedule(Seconds(50), &SetLinkUp, d2d3.Get(1));

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
    ospfAppHelper.SetAttribute("RouterDeadInterval", TimeValue(Seconds(30)));
    ospfAppHelper.SetAttribute("LSUInterval", TimeValue(Seconds(5)));
 
    ApplicationContainer ospfApp = ospfAppHelper.Install(c);
    ospfApp.Start(Seconds(1.0));
    ospfApp.Stop(Seconds(SIM_SECONDS));

    // User Traffic
    // uint16_t port = 9;  // well-known echo port number
    // UdpEchoServerHelper server (port);
    // ApplicationContainer apps = server.Install (c.Get (3));
    // apps.Start (Seconds (1.0));
    // apps.Stop (Seconds (SIM_SECONDS));

    // uint32_t tSize = 1024;
    // uint32_t maxPacketCount = 200;
    // Time interPacketInterval = Seconds (1.);
    // UdpEchoClientHelper client (Ipv4Address("10.1.1.9"), port);
    // client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    // client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    // client.SetAttribute ("PacketSize", UintegerValue (tSize));
    // apps = client.Install (c.Get (1));
    // apps.Start (Seconds (2.0));
    // apps.Stop (Seconds (SIM_SECONDS));

    // Print LSDB
    Ptr<OspfApp> app3  = DynamicCast<OspfApp>(c.Get(3)->GetApplication(0));
    // Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintLsdb, app);
    Simulator::Schedule(Seconds(80), &OspfApp::PrintRouting, app3, dirName, "n3t80.routes");
    Simulator::Schedule(Seconds(45), &OspfApp::PrintRouting, app3, dirName, "n3t45.routes");

    Ptr<OspfApp> app0  = DynamicCast<OspfApp>(c.Get(0)->GetApplication(0));
    // Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintLsdb, app);
    Simulator::Schedule(Seconds(80), &OspfApp::PrintRouting, app0, dirName, "n0t80.routes");
    Simulator::Schedule(Seconds(45), &OspfApp::PrintRouting, app0, dirName, "n0t45.routes");
    for (int i = 0; i < 4; i++) {
        Ptr<OspfApp> app  = DynamicCast<OspfApp>(c.Get(i)->GetApplication(0));
        Simulator::Schedule(Seconds(SIM_SECONDS - 1), &OspfApp::PrintLsdb, app);
    }
    // app  = DynamicCast<OspfApp>(c.Get(1)->GetApplication(0));
    // Simulator::Schedule(Seconds(146), &OspfApp::PrintLSDB, app);
    // app  = DynamicCast<OspfApp>(c.Get(2)->GetApplication(0));
    // Simulator::Schedule(Seconds(147), &OspfApp::PrintLSDB, app);
    // app  = DynamicCast<OspfApp>(c.Get(3)->GetApplication(0));
    // Simulator::Schedule(Seconds(148), &OspfApp::PrintLSDB, app);



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