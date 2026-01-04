/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace ns3 {

namespace {

std::string
ReadAll (const std::filesystem::path &path)
{
  std::ifstream in (path);
  std::stringstream ss;
  ss << in.rdbuf ();
  return ss.str ();
}

bool
HasRouteLine (const std::string &table, const std::string &dst, const std::string &gw)
{
  std::istringstream iss (table);
  for (std::string line; std::getline (iss, line);)
    {
      // Ipv4StaticRouting table lines begin with the destination.
      if (line.rfind (dst, 0) == 0 && line.find (gw) != std::string::npos)
        {
          return true;
        }
    }
  return false;
}

} // namespace

class OspfThreeNodesIntegrationTestCase : public TestCase
{
public:
  OspfThreeNodesIntegrationTestCase ()
    : TestCase ("OSPF installs learned routes in a 3-node line topology")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (3);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));
    NetDeviceContainer d12 = p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));

    Ipv4AddressHelper ipv4;

    ipv4.SetBase ("10.1.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    ipv4.SetBase ("10.1.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Speed up convergence for test runtime.
    ospf.SetAttribute ("HelloInterval", TimeValue (Seconds (1)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (3)));
    ospf.SetAttribute ("LSUInterval", TimeValue (Seconds (1)));

    ApplicationContainer apps = ospf.Install (nodes);
    // Preload LSDB/neighbor state for a deterministic, quick system test.
    ospf.Preload (nodes);

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (2));

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration");
    std::filesystem::create_directories (outDir);

    for (uint32_t i = 0; i < nodes.GetN (); ++i)
      {
        Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
        NS_TEST_ASSERT_MSG_NE (app, nullptr, "expected OspfApp at application index 0");

        Simulator::Schedule (Seconds (1), &OspfApp::PrintRouting, app, outDir,
                             "n" + std::to_string (i) + ".routes");
      }

    Simulator::Stop (Seconds (2));
    Simulator::Run ();

    const std::string n0 = ReadAll (outDir / "n0.routes");
    const std::string n2 = ReadAll (outDir / "n2.routes");

    // Node0 should learn the subnet behind node2 via node1 (10.1.1.2).
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n0, "10.1.2.0", "10.1.1.2"), true,
                           "node0 should have a route to 10.1.2.0/30 via 10.1.1.2\n" + n0);

    // Node2 should learn the subnet behind node0 via node1 (10.1.2.1).
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n2, "10.1.1.0", "10.1.2.1"), true,
                           "node2 should have a route to 10.1.1.0/30 via 10.1.2.1\n" + n2);

    Simulator::Destroy ();
  }
};

class OspfIntegrationTestSuite : public TestSuite
{
public:
  OspfIntegrationTestSuite ()
    : TestSuite ("ospf-integration", SYSTEM)
  {
    AddTestCase (new OspfThreeNodesIntegrationTestCase (), TestCase::QUICK);
  }
};

static OspfIntegrationTestSuite g_ospfIntegrationTestSuite;

} // namespace ns3
