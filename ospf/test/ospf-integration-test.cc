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
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

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

bool
HasRouteLineViaAnyGateway (const std::string &table, const std::string &dst,
                           const std::initializer_list<std::string> &gws)
{
  for (const auto &gw : gws)
    {
      if (HasRouteLine (table, dst, gw))
        {
          return true;
        }
    }
  return false;
}

std::string
Ipv4ToString (Ipv4Address addr)
{
  std::ostringstream os;
  os << addr;
  return os.str ();
}

static void
IncrementTxCounter (uint32_t *counter, Ptr<const Packet>)
{
  ++(*counter);
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
    // Preload LSDB/neighbor state for a deterministic system test.
    ospf.Preload (nodes);

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (2.0));

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration");
    std::filesystem::create_directories (outDir);

    for (uint32_t i = 0; i < nodes.GetN (); ++i)
      {
        Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
        NS_TEST_ASSERT_MSG_NE (app, nullptr, "expected OspfApp at application index 0");
        Simulator::Schedule (Seconds (1.0), &OspfApp::PrintRouting, app, outDir,
                             "n" + std::to_string (i) + ".routes");
      }

    Simulator::Stop (Seconds (2.0));
    Simulator::Run ();

    const std::string n0 = ReadAll (outDir / "n0.routes");
    const std::string n2 = ReadAll (outDir / "n2.routes");

    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n0, "10.1.2.0", "10.1.1.2"), true,
                           "node0 should have a route to 10.1.2.0/30 via 10.1.1.2\n" + n0);
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n2, "10.1.1.0", "10.1.2.1"), true,
                           "node2 should have a route to 10.1.1.0/30 via 10.1.2.1\n" + n2);

    Simulator::Destroy ();
  }
};

class OspfColdStartConvergesIntegrationTestCase : public TestCase
{
public:
  OspfColdStartConvergesIntegrationTestCase ()
    : TestCase ("OSPF cold start installs learned routes (no Preload)")
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

    // Fast timings for test runtime (cold start, no Preload).
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "expected OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "expected OspfApp");

    // Configure reachable prefixes (advertised routes) from each node's interfaces.
    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    uint32_t tx0 = 0;
    uint32_t tx1 = 0;
    uint32_t tx2 = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx0));
    app1->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx1));
    app2->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx2));

    // Capture a routing snapshot before apps start.
    apps.Start (Seconds (0.5));
    apps.Stop (Seconds (8.0));

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-cold-start");
    std::filesystem::create_directories (outDir);

    Simulator::Schedule (Seconds (0.2), &OspfApp::PrintRouting, app0, outDir, "n0.pre.routes");
    Simulator::Schedule (Seconds (0.2), &OspfApp::PrintRouting, app2, outDir, "n2.pre.routes");

    // Capture a routing snapshot after convergence.
    Simulator::Schedule (Seconds (7.0), &OspfApp::PrintRouting, app0, outDir, "n0.post.routes");
    Simulator::Schedule (Seconds (7.0), &OspfApp::PrintRouting, app2, outDir, "n2.post.routes");

    Simulator::Stop (Seconds (8.0));
    Simulator::Run ();

    const std::string n0Pre = ReadAll (outDir / "n0.pre.routes");
    const std::string n2Pre = ReadAll (outDir / "n2.pre.routes");

    NS_TEST_ASSERT_MSG_EQ (
      HasRouteLineViaAnyGateway (n0Pre, "10.1.2.0", {"10.1.1.2"}), false,
      "node0 should not have learned routes before OSPF starts (blank state)\n" + n0Pre);
    NS_TEST_ASSERT_MSG_EQ (
      HasRouteLineViaAnyGateway (n2Pre, "10.1.1.0", {"10.1.2.1"}), false,
      "node2 should not have learned routes before OSPF starts (blank state)\n" + n2Pre);

    const std::string n0Post = ReadAll (outDir / "n0.post.routes");
    const std::string n2Post = ReadAll (outDir / "n2.post.routes");

    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n0Post, "10.1.2.0", "10.1.1.2"), true,
                 "node0 should have a route to 10.1.2.0/30 via 10.1.1.2 after cold start\n" +
                   n0Post);
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n2Post, "10.1.1.0", "10.1.2.1"), true,
                 "node2 should have a route to 10.1.1.0/30 via 10.1.2.1 after cold start\n" +
                   n2Post);

    NS_TEST_ASSERT_MSG_GT (tx0 + tx1 + tx2, 0u, "expected at least one Tx trace event");

    const uint32_t h0 = app0->GetLsdbHash ();
    const uint32_t h1 = app1->GetLsdbHash ();
    const uint32_t h2 = app2->GetLsdbHash ();

    NS_TEST_ASSERT_MSG_NE (h0, 0u, "expected non-zero LSDB hash after cold start");
    NS_TEST_ASSERT_MSG_EQ (h0, h1, "expected LSDB hashes to match after cold start (n0 vs n1)");
    NS_TEST_ASSERT_MSG_EQ (h0, h2, "expected LSDB hashes to match after cold start (n0 vs n2)");

    Simulator::Destroy ();
  }
};

class OspfLsdbConsistentAfterPreloadIntegrationTestCase : public TestCase
{
public:
  OspfLsdbConsistentAfterPreloadIntegrationTestCase ()
    : TestCase ("OSPF Preload produces consistent LSDB across nodes")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (4);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));
    NetDeviceContainer d12 = p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));
    NetDeviceContainer d23 = p2p.Install (NodeContainer (nodes.Get (2), nodes.Get (3)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.1.2.0", "255.255.255.252");
    ipv4.Assign (d12);
    ipv4.SetBase ("10.1.3.0", "255.255.255.252");
    ipv4.Assign (d23);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("HelloInterval", TimeValue (Seconds (1)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (3)));
    ospf.SetAttribute ("LSUInterval", TimeValue (Seconds (1)));

    ApplicationContainer apps = ospf.Install (nodes);
    ospf.Preload (nodes);

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (1.0));

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    std::vector<Ptr<OspfApp>> ospfApps;
    ospfApps.reserve (nodes.GetN ());
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
      {
        Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
        NS_TEST_ASSERT_MSG_NE (app, nullptr, "expected OspfApp at application index 0");
        ospfApps.push_back (app);
      }

    const uint32_t routerHash = ospfApps[0]->GetLsdbHash ();
    const uint32_t l1Hash = ospfApps[0]->GetL1SummaryLsdbHash ();
    const uint32_t areaHash = ospfApps[0]->GetAreaLsdbHash ();
    const uint32_t l2Hash = ospfApps[0]->GetL2SummaryLsdbHash ();

    for (uint32_t i = 1; i < ospfApps.size (); ++i)
      {
        NS_TEST_ASSERT_MSG_EQ (ospfApps[i]->GetLsdbHash (), routerHash, "router LSDB hash mismatch");
        NS_TEST_ASSERT_MSG_EQ (ospfApps[i]->GetL1SummaryLsdbHash (), l1Hash,
                               "L1 summary LSDB hash mismatch");
        NS_TEST_ASSERT_MSG_EQ (ospfApps[i]->GetAreaLsdbHash (), areaHash, "area LSDB hash mismatch");
        NS_TEST_ASSERT_MSG_EQ (ospfApps[i]->GetL2SummaryLsdbHash (), l2Hash, "L2 summary LSDB hash mismatch");
      }

    Simulator::Destroy ();
  }
};

class OspfImportExportRoundtripIntegrationTestCase : public TestCase
{
public:
  OspfImportExportRoundtripIntegrationTestCase ()
    : TestCase ("OSPF ExportOspf and ImportOspf roundtrip preserves LSDB and routing")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-export");
    std::filesystem::create_directories (outDir);

    std::vector<uint32_t> baselineRouterHashes;
    std::vector<uint32_t> baselineL1Hashes;
    std::vector<uint32_t> baselineAreaHashes;
    std::vector<uint32_t> baselineL2Hashes;

    // Phase 1: preload and export.
    {
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
      ospf.SetAttribute ("HelloInterval", TimeValue (Seconds (1)));
      ospf.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (3)));
      ospf.SetAttribute ("LSUInterval", TimeValue (Seconds (1)));

      ApplicationContainer apps = ospf.Install (nodes);
      ospf.Preload (nodes);

      apps.Start (Seconds (0.0));
      apps.Stop (Seconds (1.0));

      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
          NS_TEST_ASSERT_MSG_NE (app, nullptr, "expected OspfApp at application index 0");
          Simulator::Schedule (Seconds (0.2), &OspfApp::ExportOspf, app, outDir,
                               "n" + std::to_string (i));
        }

      Simulator::Stop (Seconds (1.0));
      Simulator::Run ();

      baselineRouterHashes.reserve (nodes.GetN ());
      baselineL1Hashes.reserve (nodes.GetN ());
      baselineAreaHashes.reserve (nodes.GetN ());
      baselineL2Hashes.reserve (nodes.GetN ());

      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
          baselineRouterHashes.push_back (app->GetLsdbHash ());
          baselineL1Hashes.push_back (app->GetL1SummaryLsdbHash ());
          baselineAreaHashes.push_back (app->GetAreaLsdbHash ());
          baselineL2Hashes.push_back (app->GetL2SummaryLsdbHash ());

          const auto meta = outDir / ("n" + std::to_string (i) + ".meta");
          const auto lsdb = outDir / ("n" + std::to_string (i) + ".lsdb");
          const auto neighbors = outDir / ("n" + std::to_string (i) + ".neighbors");
          const auto prefixes = outDir / ("n" + std::to_string (i) + ".prefixes");

          NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (meta), true,
                                 "missing expected file: " + meta.string ());
          NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (lsdb), true,
                                 "missing expected file: " + lsdb.string ());
          NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (neighbors), true,
                                 "missing expected file: " + neighbors.string ());
          NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (prefixes), true,
                                 "missing expected file: " + prefixes.string ());

          NS_TEST_ASSERT_MSG_GT (std::filesystem::file_size (meta), 0u,
                                 "expected non-empty file: " + meta.string ());
          NS_TEST_ASSERT_MSG_GT (std::filesystem::file_size (lsdb), 0u,
                                 "expected non-empty file: " + lsdb.string ());
          NS_TEST_ASSERT_MSG_GT (std::filesystem::file_size (neighbors), 0u,
                                 "expected non-empty file: " + neighbors.string ());
          NS_TEST_ASSERT_MSG_GT (std::filesystem::file_size (prefixes), 0u,
                                 "expected non-empty file: " + prefixes.string ());
        }

      Simulator::Destroy ();
    }

    // Phase 2: import into a fresh simulation and validate.
    {
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
      // Use a single area so Preload() can deterministically seed multi-hop routes.
      // Preload does not currently generate L2 area summaries.
      ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.0.0")));
      ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));
      ospf.SetAttribute ("HelloInterval", TimeValue (Seconds (1)));
      ospf.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (3)));
      ospf.SetAttribute ("LSUInterval", TimeValue (Seconds (1)));

      ApplicationContainer apps = ospf.Install (nodes);
      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
          NS_TEST_ASSERT_MSG_NE (app, nullptr, "expected OspfApp at application index 0");
          app->ImportOspf (outDir, "n" + std::to_string (i));
        }

      apps.Start (Seconds (0.0));
      apps.Stop (Seconds (1.0));

      const std::filesystem::path routesDir = CreateTempDirFilename ("ospf-integration-import");
      std::filesystem::create_directories (routesDir);

      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
          Simulator::Schedule (Seconds (0.5), &OspfApp::PrintRouting, app, routesDir,
                               "n" + std::to_string (i) + ".routes");
        }

      Simulator::Stop (Seconds (1.0));
      Simulator::Run ();

      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
          NS_TEST_ASSERT_MSG_EQ (app->GetLsdbHash (), baselineRouterHashes[i],
                                 "router LSDB hash mismatch after import");
          NS_TEST_ASSERT_MSG_EQ (app->GetL1SummaryLsdbHash (), baselineL1Hashes[i],
                                 "L1 summary LSDB hash mismatch after import");
          NS_TEST_ASSERT_MSG_EQ (app->GetAreaLsdbHash (), baselineAreaHashes[i],
                                 "area LSDB hash mismatch after import");
          NS_TEST_ASSERT_MSG_EQ (app->GetL2SummaryLsdbHash (), baselineL2Hashes[i],
                                 "L2 summary LSDB hash mismatch after import");
        }

      const std::string n0 = ReadAll (routesDir / "n0.routes");
      const std::string n2 = ReadAll (routesDir / "n2.routes");

      NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n0, "10.1.2.0", "10.1.1.2"), true,
                             "node0 should have route to 10.1.2.0/30 via 10.1.1.2 after import\n" +
                                 n0);
      NS_TEST_ASSERT_MSG_EQ (HasRouteLine (n2, "10.1.1.0", "10.1.2.1"), true,
                             "node2 should have route to 10.1.1.0/30 via 10.1.2.1 after import\n" +
                                 n2);

      Simulator::Destroy ();
    }
  }
};

class OspfTxTraceProducesPacketsIntegrationTestCase : public TestCase
{
public:
  OspfTxTraceProducesPacketsIntegrationTestCase ()
    : TestCase ("OSPF emits packets (Tx trace fires) on a simple link")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (2);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Fast timings for test runtime.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

    ApplicationContainer apps = ospf.Install (nodes);
    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (1.0));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "expected OspfApp");

    uint32_t tx0 = 0;
    uint32_t tx1 = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx0));
    app1->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx1));

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (tx0 + tx1, 0u, "expected at least one Tx trace event");
    Simulator::Destroy ();
  }
};

class OspfTopologyChangeReroutesIntegrationTestCase : public TestCase
{
public:
  OspfTopologyChangeReroutesIntegrationTestCase ()
    : TestCase ("OSPF produces correct routes after a topology change")
  {
  }

  void
  DoRun () override
  {
    struct RunResult
    {
      bool ok{false};
      std::string routes;
      std::string error;
    };

    auto runAndGetNode0Routes = [this] (bool includeLink23, const std::string &tag) {
      RngSeedManager::SetSeed (1);
      RngSeedManager::SetRun (1);

      NodeContainer nodes;
      nodes.Create (4);

      InternetStackHelper internet;
      internet.Install (nodes);

      PointToPointHelper p2p;
      p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
      p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

      NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));
      NetDeviceContainer d12 = p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));
      NetDeviceContainer d23;
      if (includeLink23)
        {
          d23 = p2p.Install (NodeContainer (nodes.Get (2), nodes.Get (3)));
        }

      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.252");
      ipv4.Assign (d01);
      ipv4.SetBase ("10.1.2.0", "255.255.255.252");
      ipv4.Assign (d12);
      if (includeLink23)
        {
          ipv4.SetBase ("10.1.3.0", "255.255.255.252");
          ipv4.Assign (d23);
        }

      NodeContainer routers;
      routers.Add (nodes.Get (0));
      routers.Add (nodes.Get (1));
      routers.Add (nodes.Get (2));
      if (includeLink23)
        {
          routers.Add (nodes.Get (3));
        }

      OspfAppHelper ospf;
      ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
      ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
      ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

      // Fast timings for test runtime.
      ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
      ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
      ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
      ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

      ApplicationContainer apps = ospf.Install (routers);
      if (apps.GetN () != routers.GetN ())
        {
          return RunResult{false, {}, "ospf.Install() did not return one application per router"};
        }

      // Deterministic LSDB/neighbor seeding.
      ospf.Preload (routers);

      Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
      if (app0 == nullptr)
        {
          return RunResult{false, {}, "apps.Get(0) was not an OspfApp"};
        }

      apps.Start (Seconds (0.0));
      apps.Stop (Seconds (2.0));

      const std::filesystem::path outDir = CreateTempDirFilename (tag);
      std::filesystem::create_directories (outDir);
      Simulator::Schedule (Seconds (1.0), &OspfApp::PrintRouting, app0, outDir, "n0.routes");

      Simulator::Stop (Seconds (2.0));
      Simulator::Run ();
      const std::string routes = ReadAll (outDir / "n0.routes");
      Simulator::Destroy ();
      return RunResult{true, routes, {}};
    };

    const RunResult fullResult = runAndGetNode0Routes (true, "ospf-integration-topo-full");
    NS_TEST_ASSERT_MSG_EQ (fullResult.ok, true, fullResult.error);
    const std::string &full = fullResult.routes;
    NS_TEST_ASSERT_MSG_EQ (
        HasRouteLine (full, "10.1.3.0", "10.1.1.2"), true,
        "node0 should have a route to 10.1.3.0/30 via 10.1.1.2 when link (2,3) exists\n" + full);

    const RunResult reducedResult = runAndGetNode0Routes (false, "ospf-integration-topo-reduced");
    NS_TEST_ASSERT_MSG_EQ (reducedResult.ok, true, reducedResult.error);
    const std::string &reduced = reducedResult.routes;
    NS_TEST_ASSERT_MSG_EQ (
        HasRouteLineViaAnyGateway (reduced, "10.1.3.0", {"10.1.1.2", "10.1.2.2"}), false,
        "node0 should not have a route to 10.1.3.0/30 when link (2,3) is absent\n" + reduced);
  }
};

class OspfSixNodesLineIntegrationTestCase : public TestCase
{
public:
  OspfSixNodesLineIntegrationTestCase ()
    : TestCase ("OSPF installs routes in a 6-node line topology")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (6);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    std::vector<NetDeviceContainer> links;
    links.reserve (5);
    for (uint32_t i = 0; i < 5; ++i)
      {
        links.push_back (p2p.Install (NodeContainer (nodes.Get (i), nodes.Get (i + 1))));
      }

    Ipv4AddressHelper ipv4;
    for (uint32_t i = 0; i < links.size (); ++i)
      {
        const std::string base = "10.1." + std::to_string (i + 1) + ".0";
        ipv4.SetBase (base.c_str (), "255.255.255.252");
        ipv4.Assign (links[i]);
      }

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Fast timings for test runtime.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

    ApplicationContainer apps = ospf.Install (nodes);
    ospf.Preload (nodes);

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (6.0));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    Ptr<OspfApp> app5 = DynamicCast<OspfApp> (apps.Get (5));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "expected OspfApp");
    NS_TEST_ASSERT_MSG_NE (app5, nullptr, "expected OspfApp");

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-six-line");
    std::filesystem::create_directories (outDir);

    Simulator::Schedule (Seconds (5.0), &OspfApp::PrintRouting, app0, outDir, "n0.routes");
    Simulator::Schedule (Seconds (5.0), &OspfApp::PrintRouting, app2, outDir, "n2.routes");
    Simulator::Schedule (Seconds (5.0), &OspfApp::PrintRouting, app5, outDir, "n5.routes");

    Simulator::Stop (Seconds (6.0));
    Simulator::Run ();

    const std::string n0 = ReadAll (outDir / "n0.routes");
    const std::string n2 = ReadAll (outDir / "n2.routes");
    const std::string n5 = ReadAll (outDir / "n5.routes");

    NS_TEST_ASSERT_MSG_EQ (
        HasRouteLine (n0, "10.1.5.0", "10.1.1.2"), true,
        "node0 should have a route to 10.1.5.0/30 via 10.1.1.2 in a 6-node line\n" + n0);
    NS_TEST_ASSERT_MSG_EQ (
        HasRouteLine (n2, "10.1.5.0", "10.1.3.2"), true,
        "node2 should have a route to 10.1.5.0/30 via 10.1.3.2 in a 6-node line\n" + n2);
    NS_TEST_ASSERT_MSG_EQ (
        HasRouteLine (n5, "10.1.1.0", "10.1.5.1"), true,
        "node5 should have a route to 10.1.1.0/30 via 10.1.5.1 in a 6-node line\n" + n5);

    Simulator::Destroy ();
  }
};

class OspfTwoAreasLineIntegrationTestCase : public TestCase
{
public:
  OspfTwoAreasLineIntegrationTestCase ()
    : TestCase ("OSPF installs routes across two areas in a line topology")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (4);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));
    NetDeviceContainer d12 = p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));
    NetDeviceContainer d23 = p2p.Install (NodeContainer (nodes.Get (2), nodes.Get (3)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.1.2.0", "255.255.255.252");
    ipv4.Assign (d12);
    ipv4.SetBase ("10.1.3.0", "255.255.255.252");
    ipv4.Assign (d23);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("EnableAreaProxy", BooleanValue (true));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Fast timings for test runtime.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

    ApplicationContainer apps = ospf.Install (nodes);
    NS_TEST_ASSERT_MSG_EQ (apps.GetN (), nodes.GetN (), "expected one OspfApp per node");

    // Split nodes into two areas: (0,1) in area 0; (2,3) in area 1.
    DynamicCast<OspfApp> (apps.Get (0))->SetArea (0);
    DynamicCast<OspfApp> (apps.Get (1))->SetArea (0);
    DynamicCast<OspfApp> (apps.Get (2))->SetArea (1);
    DynamicCast<OspfApp> (apps.Get (3))->SetArea (1);

    // Deterministic seeding for neighbor state and initial LSDB.
    ospf.Preload (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app3 = DynamicCast<OspfApp> (apps.Get (3));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");
    NS_TEST_ASSERT_MSG_NE (app3, nullptr, "expected OspfApp");

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (8.0));

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-two-areas");
    std::filesystem::create_directories (outDir);

    Simulator::Schedule (Seconds (7.0), &OspfApp::PrintRouting, app0, outDir, "n0.routes");
    Simulator::Schedule (Seconds (7.0), &OspfApp::PrintRouting, app3, outDir, "n3.routes");

    Simulator::Stop (Seconds (8.0));
    Simulator::Run ();

    const std::string n0 = ReadAll (outDir / "n0.routes");
    const std::string n3 = ReadAll (outDir / "n3.routes");

    NS_TEST_ASSERT_MSG_EQ (
        HasRouteLine (n0, "10.1.3.0", "10.1.1.2"), true,
        "node0 should have a route to 10.1.3.0/30 via 10.1.1.2 across areas\n" + n0);
    NS_TEST_ASSERT_MSG_EQ (
        HasRouteLine (n3, "10.1.1.0", "10.1.3.1"), true,
        "node3 should have a route to 10.1.1.0/30 via 10.1.3.1 across areas\n" + n3);

    Simulator::Destroy ();
  }
};

class OspfGridTopologyIntegrationTestCase : public TestCase
{
public:
  OspfGridTopologyIntegrationTestCase ()
    : TestCase ("OSPF installs routes on a 3x3 grid topology")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (9);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    Ipv4AddressHelper ipv4;
    uint32_t subnet = 1;

    auto nextSubnet = [&] () {
      const std::string base = "10.9." + std::to_string (subnet++) + ".0";
      ipv4.SetBase (base.c_str (), "255.255.255.252");
    };

    // Grid indices:
    // 0 1 2
    // 3 4 5
    // 6 7 8
    // Horizontal links.
    nextSubnet ();
    Ipv4InterfaceContainer if01 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1))));
    nextSubnet ();
    Ipv4InterfaceContainer if12 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (2))));
    nextSubnet ();
    Ipv4InterfaceContainer if34 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (3), nodes.Get (4))));
    nextSubnet ();
    Ipv4InterfaceContainer if45 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (4), nodes.Get (5))));
    nextSubnet ();
    Ipv4InterfaceContainer if67 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (6), nodes.Get (7))));
    nextSubnet ();
    Ipv4InterfaceContainer if78 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (7), nodes.Get (8))));

    // Vertical links.
    nextSubnet ();
    Ipv4InterfaceContainer if03 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (3))));
    nextSubnet ();
    Ipv4InterfaceContainer if14 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (4))));
    nextSubnet ();
    Ipv4InterfaceContainer if25 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (2), nodes.Get (5))));
    nextSubnet ();
    Ipv4InterfaceContainer if36 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (3), nodes.Get (6))));
    nextSubnet ();
    Ipv4InterfaceContainer if47 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (4), nodes.Get (7))));
    nextSubnet ();
    Ipv4InterfaceContainer if58 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (5), nodes.Get (8))));

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Fast timings for test runtime.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

    ApplicationContainer apps = ospf.Install (nodes);

    // Seed adjacencies/LSDB to keep runtime deterministic.
    ospf.Preload (nodes);

    // Advertise a stub network from a single node so the destination is unambiguous.
    Ptr<OspfApp> app8 = DynamicCast<OspfApp> (apps.Get (8));
    NS_TEST_ASSERT_MSG_NE (app8, nullptr, "expected OspfApp");
    app8->AddReachableAddress (1, Ipv4Address ("10.250.0.0"), Ipv4Mask ("255.255.0.0"),
                               Ipv4Address ("10.250.0.1"), 1);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (6.0));

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-grid");
    std::filesystem::create_directories (outDir);
    Simulator::Schedule (Seconds (5.0), &OspfApp::PrintRouting, app0, outDir, "n0.routes");

    Simulator::Stop (Seconds (6.0));
    Simulator::Run ();

    const std::string n0 = ReadAll (outDir / "n0.routes");
    const std::string gwRight = Ipv4ToString (if01.GetAddress (1)); // node1 on (0,1)
    const std::string gwDown = Ipv4ToString (if03.GetAddress (1));  // node3 on (0,3)

    const bool viaRight = HasRouteLine (n0, "10.250.0.0", gwRight);
    const bool viaDown = HasRouteLine (n0, "10.250.0.0", gwDown);

    NS_TEST_ASSERT_MSG_EQ (
        (viaRight || viaDown), true,
        "node0 should have a route to 10.250.0.0/16 via either " + gwRight + " or " + gwDown +
            " (equal-cost paths)\n" + n0);
    NS_TEST_ASSERT_MSG_EQ (
        (viaRight && viaDown), false,
        "node0 should install a single next hop (ECMP not allowed)\n" + n0);

    Simulator::Destroy ();
  }
};

class OspfPartialMeshIntegrationTestCase : public TestCase
{
public:
  OspfPartialMeshIntegrationTestCase ()
    : TestCase ("OSPF installs routes on a partial mesh topology")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (5);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.20.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if01 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1))));
    ipv4.SetBase ("10.20.2.0", "255.255.255.252");
    Ipv4InterfaceContainer if02 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (2))));
    ipv4.SetBase ("10.20.3.0", "255.255.255.252");
    Ipv4InterfaceContainer if13 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (3))));
    ipv4.SetBase ("10.20.4.0", "255.255.255.252");
    Ipv4InterfaceContainer if23 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (2), nodes.Get (3))));
    ipv4.SetBase ("10.20.5.0", "255.255.255.252");
    Ipv4InterfaceContainer if34 = ipv4.Assign (p2p.Install (NodeContainer (nodes.Get (3), nodes.Get (4))));

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Fast timings for test runtime.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

    ApplicationContainer apps = ospf.Install (nodes);
    ospf.Preload (nodes);

    // Advertise a stub network from node4.
    Ptr<OspfApp> app4 = DynamicCast<OspfApp> (apps.Get (4));
    NS_TEST_ASSERT_MSG_NE (app4, nullptr, "expected OspfApp");
    app4->AddReachableAddress (1, Ipv4Address ("10.251.0.0"), Ipv4Mask ("255.255.0.0"),
                               Ipv4Address ("10.251.0.1"), 1);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (5.0));

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-partial-mesh");
    std::filesystem::create_directories (outDir);
    Simulator::Schedule (Seconds (4.0), &OspfApp::PrintRouting, app0, outDir, "n0.routes");

    Simulator::Stop (Seconds (5.0));
    Simulator::Run ();

    const std::string n0 = ReadAll (outDir / "n0.routes");
    const std::string gw1 = Ipv4ToString (if01.GetAddress (1)); // node1 on (0,1)
    const std::string gw2 = Ipv4ToString (if02.GetAddress (1)); // node2 on (0,2)

    const bool via1 = HasRouteLine (n0, "10.251.0.0", gw1);
    const bool via2 = HasRouteLine (n0, "10.251.0.0", gw2);

    NS_TEST_ASSERT_MSG_EQ (
        (via1 || via2), true,
        "node0 should have a route to 10.251.0.0/16 via either " + gw1 + " or " + gw2 +
            " (equal-cost paths)\n" + n0);
    NS_TEST_ASSERT_MSG_EQ (
        (via1 && via2), false,
        "node0 should install a single next hop (ECMP not allowed)\n" + n0);

    Simulator::Destroy ();
  }
};

class OspfInterfaceDownRemovesRoutesIntegrationTestCase : public TestCase
{
public:
  OspfInterfaceDownRemovesRoutesIntegrationTestCase ()
    : TestCase ("OSPF updates learned routes when an interface goes down-up (AutoSyncInterfaces)")
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
    ipv4.SetBase ("10.30.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if01 = ipv4.Assign (d01);
    ipv4.SetBase ("10.30.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Fast timings for test runtime.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));

    // Enable interface auto-tracking.
    ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
    ospf.SetAttribute ("InterfaceSyncInterval", TimeValue (MilliSeconds (50)));

    ApplicationContainer apps = ospf.Install (nodes);

    // Configure reachable prefixes (advertised routes) from each node's interfaces.
    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    // Advertise a stub network from node2 so node0 learns it via node1.
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "expected OspfApp");
    app2->AddReachableAddress (1, Ipv4Address ("10.252.0.0"), Ipv4Mask ("255.255.0.0"),
                               Ipv4Address ("10.252.0.1"), 1);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    apps.Start (Seconds (0.5));
    apps.Stop (Seconds (9.0));

    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv42 = nodes.Get (2)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv41, nullptr, "expected Ipv4");
    NS_TEST_ASSERT_MSG_NE (ipv42, nullptr, "expected Ipv4");
    const uint32_t if1 = d12.Get (0)->GetIfIndex (); // node1 side of (1,2)
    const uint32_t if2 = d12.Get (1)->GetIfIndex (); // node2 side of (1,2)

    // Drop the (1,2) link after initial convergence.
    Simulator::Schedule (Seconds (4.0), &Ipv4::SetDown, ipv41, if1);
    Simulator::Schedule (Seconds (4.0), &Ipv4::SetDown, ipv42, if2);

    // Bring the link back up later to validate re-convergence.
    Simulator::Schedule (Seconds (5.0), &Ipv4::SetUp, ipv41, if1);
    Simulator::Schedule (Seconds (5.0), &Ipv4::SetUp, ipv42, if2);

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-if-down");
    std::filesystem::create_directories (outDir);

    // Snapshot before and after the interface is brought down.
    Simulator::Schedule (Seconds (3.5), &OspfApp::PrintRouting, app0, outDir, "before.routes");
    Simulator::Schedule (Seconds (4.8), &OspfApp::PrintRouting, app0, outDir, "down.routes");
    Simulator::Schedule (Seconds (7.5), &OspfApp::PrintRouting, app0, outDir, "up.routes");

    Simulator::Stop (Seconds (9.0));
    Simulator::Run ();

    const std::string before = ReadAll (outDir / "before.routes");
    const std::string down = ReadAll (outDir / "down.routes");
    const std::string up = ReadAll (outDir / "up.routes");
    const std::string gw = Ipv4ToString (if01.GetAddress (1)); // node1 on (0,1)

    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (before, "10.252.0.0", gw), true,
                           "node0 should learn 10.252.0.0/16 before link-down\n" + before);
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (down, "10.252.0.0", gw), false,
                 "node0 should remove the learned route after link-down\n" + down);
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (up, "10.252.0.0", gw), true,
                 "node0 should re-learn the route after link-up\n" + up);

    Simulator::Destroy ();
  }
};

class OspfNodeDisableEnableRelearnsRoutesIntegrationTestCase : public TestCase
{
public:
  OspfNodeDisableEnableRelearnsRoutesIntegrationTestCase ()
    : TestCase ("OSPF can be disabled-enabled at runtime (node join-leave)")
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
    ipv4.SetBase ("10.31.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if01 = ipv4.Assign (d01);
    ipv4.SetBase ("10.31.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    // Fast timings for test runtime.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (200)));

    // When Disable() is used, reset state so Enable() behaves like a clean re-join.
    ospf.SetAttribute ("ResetStateOnDisable", BooleanValue (true));

    ApplicationContainer apps = ospf.Install (nodes);

    // Configure reachable prefixes (advertised routes) from each node's interfaces.
    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    // Advertise a stub network from node2 so node0 learns it via node1.
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "expected OspfApp");
    app2->AddReachableAddress (1, Ipv4Address ("10.253.0.0"), Ipv4Mask ("255.255.0.0"),
                               Ipv4Address ("10.253.0.1"), 1);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    apps.Start (Seconds (0.5));
    apps.Stop (Seconds (7.0));

    // Disable node2's OSPF to model node removal; re-enable to model node add.
    Simulator::Schedule (Seconds (2.0), &OspfApp::Disable, app2);
    Simulator::Schedule (Seconds (4.0), &OspfApp::Enable, app2);

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-integration-node-toggle");
    std::filesystem::create_directories (outDir);

    Simulator::Schedule (Seconds (1.7), &OspfApp::PrintRouting, app0, outDir, "before.routes");
    Simulator::Schedule (Seconds (3.6), &OspfApp::PrintRouting, app0, outDir, "disabled.routes");
    Simulator::Schedule (Seconds (6.2), &OspfApp::PrintRouting, app0, outDir, "reenabled.routes");

    Simulator::Stop (Seconds (7.0));
    Simulator::Run ();

    const std::string before = ReadAll (outDir / "before.routes");
    const std::string disabled = ReadAll (outDir / "disabled.routes");
    const std::string reenabled = ReadAll (outDir / "reenabled.routes");
    const std::string gw = Ipv4ToString (if01.GetAddress (1)); // node1 on (0,1)

    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (before, "10.253.0.0", gw), true,
                           "node0 should learn 10.253.0.0/16 before disable\n" + before);
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (disabled, "10.253.0.0", gw), false,
                           "node0 should remove the learned route after node2 disable\n" +
                               disabled);
    NS_TEST_ASSERT_MSG_EQ (HasRouteLine (reenabled, "10.253.0.0", gw), true,
                           "node0 should re-learn the route after node2 re-enable\n" + reenabled);

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
    AddTestCase (new OspfColdStartConvergesIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfLsdbConsistentAfterPreloadIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfImportExportRoundtripIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfTxTraceProducesPacketsIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfTopologyChangeReroutesIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfSixNodesLineIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfTwoAreasLineIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfGridTopologyIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfPartialMeshIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfInterfaceDownRemovesRoutesIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfNodeDisableEnableRelearnsRoutesIntegrationTestCase (), TestCase::QUICK);
  }
};

static OspfIntegrationTestSuite g_ospfIntegrationTestSuite;

} // namespace ns3
