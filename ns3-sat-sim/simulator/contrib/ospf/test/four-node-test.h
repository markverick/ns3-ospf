/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <chrono>
#include <stdexcept>

#include "ns3/basic-simulation.h"
#include "ns3/udp-burst-scheduler.h"
#include "ns3/topology-satellite-network.h"
#include "ns3/tcp-optimizer.h"
#include "ns3/arbiter-single-forward-helper.h"
#include "ns3/ipv4-arbiter-routing-helper.h"
#include "ns3/gsl-if-bandwidth-helper.h"

#include "ns3/test.h"
#include "test-helpers.h"

using namespace ns3;

////////////////////////////////////////////////////////////////////////////////////////

class FourNodeTestCase : public TestCase {
public:
    FourNodeTestCase () : TestCase ("four-node") {};

    void DoRun () {

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


    OSPFAppHelper ospfAppHelper(9);
    ospfAppHelper.SetAttribute("HelloInterval", TimeValue(Seconds(10)));
    ospfAppHelper.SetAttribute("HelloAddress", Ipv4AddressValue(ospfHelloAddress));
    ospfAppHelper.SetAttribute("NeighborTimeout", TimeValue(Seconds(30)));
    ospfAppHelper.SetAttribute("LSUInterval", TimeValue(Seconds(30)));
 
    ApplicationContainer ospfApp = ospfAppHelper.Install(c);
    ospfApp.Start(Seconds(1.0));
    ospfApp.Stop(Seconds(200.0));

    // User Traffic
    uint16_t port = 9;  // well-known echo port number
    UdpEchoServerHelper server (port);
    ApplicationContainer apps = server.Install (c.Get (3));
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (200.0));

    uint32_t tSize = 1024;
    uint32_t maxPacketCount = 200;
    Time interPacketInterval = Seconds (1.);
    UdpEchoClientHelper client (Ipv4Address("10.1.3.1"), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (tSize));
    apps = client.Install (c.Get (1));
    apps.Start (Seconds (2.0));
    apps.Stop (Seconds (200.0));

    // Test Error
    Simulator::Schedule(Seconds(35), &SetLinkDown, d1d2.Get(0));
    Simulator::Schedule(Seconds(35), &SetLinkDown, d1d2.Get(1));
    Simulator::Schedule(Seconds(85), &SetLinkUp, d1d2.Get(0));
    Simulator::Schedule(Seconds(85), &SetLinkUp, d1d2.Get(1));

    }

};

////////////////////////////////////////////////////////////////////////////////////////
