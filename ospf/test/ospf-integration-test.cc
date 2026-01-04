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

    // Ensure each router's Router-LSA is present in the LSDB on every node.
    std::vector<uint32_t> routerIds;
    routerIds.reserve (ospfApps.size ());
    for (const auto &app : ospfApps)
      {
        routerIds.push_back (app->GetRouterId ().Get ());
      }
    for (const auto &app : ospfApps)
      {
        const auto lsdb = app->GetLsdb ();
        for (const auto rid : routerIds)
          {
            const bool hasRouterLsa = (lsdb.count (rid) > 0);
            std::ostringstream msg;
            msg << "missing Router-LSA for routerId=" << Ipv4Address (rid);
            NS_TEST_ASSERT_MSG_EQ (hasRouterLsa, true, msg.str ());
          }
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

          NS_TEST_ASSERT_MSG_EQ (std::filesystem::is_regular_file (meta), true,
                                 "expected regular file: " + meta.string ());
          NS_TEST_ASSERT_MSG_EQ (std::filesystem::is_regular_file (lsdb), true,
                                 "expected regular file: " + lsdb.string ());
          NS_TEST_ASSERT_MSG_EQ (std::filesystem::is_regular_file (neighbors), true,
                                 "expected regular file: " + neighbors.string ());
          NS_TEST_ASSERT_MSG_EQ (std::filesystem::is_regular_file (prefixes), true,
                                 "expected regular file: " + prefixes.string ());

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
      ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
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

static void
IncrementTxCounter (uint32_t *counter, Ptr<const Packet>)
{
  ++(*counter);
}

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

    uint32_t tx0 = 0;
    uint32_t tx1 = 0;

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (nodes.Get (0)->GetApplication (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (nodes.Get (1)->GetApplication (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp at node0 application index 0");
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "expected OspfApp at node1 application index 0");

    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx0));
    app1->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx1));

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (tx0, 0u, "expected node0 to transmit at least one OSPF packet");
    NS_TEST_ASSERT_MSG_GT (tx1, 0u, "expected node1 to transmit at least one OSPF packet");

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
    AddTestCase (new OspfLsdbConsistentAfterPreloadIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfImportExportRoundtripIntegrationTestCase (), TestCase::QUICK);
    AddTestCase (new OspfTxTraceProducesPacketsIntegrationTestCase (), TestCase::QUICK);
  }
};

static OspfIntegrationTestSuite g_ospfIntegrationTestSuite;

} // namespace ns3
