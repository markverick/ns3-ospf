/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

#include "ospf-app-helper-test-utils.h"
#include "ospf-test-utils.h"

namespace ns3 {

namespace {

using ospf_app_helper_test_utils::FetchSelfRouterLsa;

class OspfHelperRuntimeRebindReinitializesProtocolTestCase : public TestCase
{
public:
  OspfHelperRuntimeRebindReinitializesProtocolTestCase ()
    : TestCase ("OspfApp explicit rebind while running reinitializes live protocol state")
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
    NetDeviceContainer d02 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.9.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.9.2.0", "255.255.255.252");
    Ipv4InterfaceContainer if02 = ipv4.Assign (d02);

    OspfAppHelper ospf;
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));
    apps.Add (ospf.Install (nodes.Get (2), NetDeviceContainer (d02.Get (1))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app2, nullptr, "expected OspfApp on node2");

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv42 = nodes.Get (2)->GetObject<Ipv4> ();
    NS_TEST_EXPECT_MSG_NE (ipv40, nullptr, "expected Ipv4 stack on node0");
    NS_TEST_EXPECT_MSG_NE (ipv42, nullptr, "expected Ipv4 stack on node2");

    const uint32_t d02If0 = ipv40->GetInterfaceForDevice (d02.Get (0));
    const uint32_t d02If2 = ipv42->GetInterfaceForDevice (d02.Get (1));
    app2->AddReachableAddress (d02If2, Ipv4Address ("10.99.0.0"), Ipv4Mask ("255.255.0.0"));

    NodeContainer ospfNodes;
    ospfNodes.Add (nodes.Get (0));
    ospfNodes.Add (nodes.Get (1));
    ospfNodes.Add (nodes.Get (2));
    ospf.ConfigureReachablePrefixesFromInterfaces (ospfNodes);

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (5.0));

    Simulator::Schedule (Seconds (1.5), &OspfApp::SetBoundNetDevices, app0,
                         NetDeviceContainer (d02.Get (0)));

    Simulator::Stop (Seconds (5.0));
    Simulator::Run ();

    const auto routerLsdb = app0->GetLsdb ();
    const auto l1SummaryLsdb = app0->GetL1SummaryLsdb ();
    NS_TEST_EXPECT_MSG_EQ (routerLsdb.count (Ipv4Address ("10.9.1.1").Get ()), 0u,
                 "live rebind must discard self RouterLSAs keyed by the old router id");
    NS_TEST_EXPECT_MSG_EQ (routerLsdb.count (Ipv4Address ("10.9.2.1").Get ()), 1u,
                 "live rebind must retain only the current self RouterLSA");
    NS_TEST_EXPECT_MSG_EQ (l1SummaryLsdb.count (Ipv4Address ("10.9.1.1").Get ()), 0u,
                 "live rebind must discard self L1SummaryLSAs keyed by the old router id");
    NS_TEST_EXPECT_MSG_EQ (l1SummaryLsdb.count (Ipv4Address ("10.9.2.1").Get ()), 1u,
                 "live rebind must retain only the current self L1SummaryLSA");

    OspfApp::ForwardingEntry entry;
    const bool found = app0->LookupForwardingEntry (Ipv4Address ("10.99.0.1"), -1, entry);
    NS_TEST_EXPECT_MSG_EQ (found, true,
                           "node0 should learn node2's advertised prefix after live rebind");
    if (found)
      {
        NS_TEST_EXPECT_MSG_EQ (entry.ifIndex, d02If0,
                               "live rebind should forward over the newly selected interface");
        NS_TEST_EXPECT_MSG_EQ (entry.nextHop, if02.GetAddress (1),
                               "live rebind should use node2 as the next hop on the new adjacency");
      }

    Simulator::Destroy ();
  }
};

class OspfPreloadRespectsExplicitInterfaceSelectionTestCase : public TestCase
{
public:
  OspfPreloadRespectsExplicitInterfaceSelectionTestCase ()
    : TestCase ("OspfAppHelper::Preload only seeds adjacencies on explicitly selected interfaces")
  {
  }

  void
  DoRun () override
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
    ipv4.SetBase ("10.4.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.4.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));
    apps.Add (ospf.Install (nodes.Get (2), NetDeviceContainer (d12.Get (1))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");
    NS_TEST_EXPECT_MSG_NE (app2, nullptr, "expected OspfApp on node2");

    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    const uint32_t d12If1 = ipv41->GetInterfaceForDevice (d12.Get (0));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app1, d12If1), false,
                           "node1 should not bind the second point-to-point interface");

    ospf.Preload (nodes);

    Ptr<RouterLsa> routerLsa = FetchSelfRouterLsa (app1);
    NS_TEST_EXPECT_MSG_NE (routerLsa, nullptr, "expected a preloaded self RouterLSA for node1");
    if (routerLsa == nullptr)
      {
        Simulator::Destroy ();
        return;
      }

    NS_TEST_EXPECT_MSG_EQ (routerLsa->GetNLink (), 1u,
                           "node1 RouterLSA should only include the explicitly selected adjacency");
    if (routerLsa->GetNLink () > 0)
      {
        NS_TEST_EXPECT_MSG_EQ (routerLsa->GetLink (0).m_linkId, app0->GetRouterId ().Get (),
                               "the only advertised adjacency should be the selected node0 link");
      }

    Simulator::Destroy ();
  }
};

class OspfSparseExplicitInstallColdStartTestCase : public TestCase
{
public:
  OspfSparseExplicitInstallColdStartTestCase ()
    : TestCase ("Sparse explicit install should cold-start on selected interfaces without crashing")
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
    ipv4.SetBase ("10.5.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.5.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
    ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));
    ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));

    NodeContainer ospfNodes;
    ospfNodes.Add (nodes.Get (0));
    ospfNodes.Add (nodes.Get (1));
    ospf.ConfigureReachablePrefixesFromInterfaces (ospfNodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (2.0));

    Simulator::Stop (Seconds (2.0));
    Simulator::Run ();

    Ptr<RouterLsa> routerLsa0 = FetchSelfRouterLsa (app0);
    Ptr<RouterLsa> routerLsa1 = FetchSelfRouterLsa (app1);
    NS_TEST_EXPECT_MSG_NE (routerLsa0, nullptr,
                           "sparse explicit install should originate a RouterLSA on node0");
    NS_TEST_EXPECT_MSG_NE (routerLsa1, nullptr,
                           "sparse explicit install should originate a RouterLSA on node1");

    Simulator::Destroy ();
  }
};

class OspfPreloadSkipsUnsetRouterIdsTestCase : public TestCase
{
public:
  OspfPreloadSkipsUnsetRouterIdsTestCase ()
    : TestCase ("OspfAppHelper::Preload skips apps whose router ID is unset")
  {
  }

  void
  DoRun () override
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
    ipv4.SetBase ("10.15.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.15.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));
    apps.Add (ospf.Install (nodes.Get (2), NetDeviceContainer (d12.Get (1))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");
    NS_TEST_EXPECT_MSG_NE (app2, nullptr, "expected OspfApp on node2");

    app0->SetRouterId (Ipv4Address::GetZero ());
    app1->SetRouterId (Ipv4Address::GetZero ());

    NS_TEST_EXPECT_MSG_EQ (app0->GetRouterId (), Ipv4Address::GetZero (),
                 "node0 should expose an unset router ID after explicit clearing");
    NS_TEST_EXPECT_MSG_EQ (app1->GetRouterId (), Ipv4Address::GetZero (),
                 "node1 should expose an unset router ID after explicit clearing");
    NS_TEST_EXPECT_MSG_NE (app2->GetRouterId (), Ipv4Address::GetZero (),
                           "node2 should have a valid router ID on its addressed selected interface");

    ospf.Preload (nodes);

    NS_TEST_EXPECT_MSG_EQ (FetchSelfRouterLsa (app0), nullptr,
                           "preload should skip injecting LSAs for apps with unset router IDs");
    NS_TEST_EXPECT_MSG_EQ (FetchSelfRouterLsa (app1), nullptr,
                           "preload should skip injecting LSAs for apps with unset router IDs");
    NS_TEST_EXPECT_MSG_EQ (app0->GetAreaLsdb ().empty (), true,
                 "preload should not inject area-proxy LSAs into skipped apps");
    NS_TEST_EXPECT_MSG_EQ (app1->GetAreaLsdb ().empty (), true,
                 "preload should not inject area-proxy LSAs into skipped apps");
    NS_TEST_EXPECT_MSG_EQ (app0->GetL2SummaryLsdb ().empty (), true,
                 "preload should not inject L2 summary LSAs into skipped apps");
    NS_TEST_EXPECT_MSG_EQ (app1->GetL2SummaryLsdb ().empty (), true,
                 "preload should not inject L2 summary LSAs into skipped apps");
    Ptr<RouterLsa> app2RouterLsa = FetchSelfRouterLsa (app2);
    NS_TEST_EXPECT_MSG_NE (app2RouterLsa, nullptr,
                           "preload should still seed apps whose router IDs are valid");
    if (app2RouterLsa != nullptr)
      {
        NS_TEST_EXPECT_MSG_EQ (app2RouterLsa->GetNLink (), 0u,
                               "preload must not seed adjacencies to neighbors whose router ID is unset");
      }
    NS_TEST_EXPECT_MSG_EQ (app0->IsAreaLeader (), false,
                           "skipped apps should not be marked as area leaders");
    NS_TEST_EXPECT_MSG_EQ (app1->IsAreaLeader (), false,
                           "skipped apps should not be marked as area leaders");

    Simulator::Destroy ();
  }
};

class OspfAppHelperRuntimeRebindRegressionSuite : public TestSuite
{
public:
  OspfAppHelperRuntimeRebindRegressionSuite ()
    : TestSuite ("ospf-app-helper-runtime-rebind-regression", UNIT)
  {
    AddTestCase (new OspfHelperRuntimeRebindReinitializesProtocolTestCase (), TestCase::QUICK);
  }
};

static OspfAppHelperRuntimeRebindRegressionSuite g_ospfAppHelperRuntimeRebindRegressionSuite;

class OspfAppHelperPreloadSelectionRegressionSuite : public TestSuite
{
public:
  OspfAppHelperPreloadSelectionRegressionSuite ()
    : TestSuite ("ospf-app-helper-preload-selection-regression", UNIT)
  {
    AddTestCase (new OspfPreloadRespectsExplicitInterfaceSelectionTestCase (), TestCase::QUICK);
    AddTestCase (new OspfPreloadSkipsUnsetRouterIdsTestCase (), TestCase::QUICK);
  }
};

static OspfAppHelperPreloadSelectionRegressionSuite g_ospfAppHelperPreloadSelectionRegressionSuite;

class OspfAppHelperSparseRuntimeRegressionSuite : public TestSuite
{
public:
  OspfAppHelperSparseRuntimeRegressionSuite ()
    : TestSuite ("ospf-app-helper-sparse-runtime-regression", UNIT)
  {
    AddTestCase (new OspfSparseExplicitInstallColdStartTestCase (), TestCase::QUICK);
  }
};

static OspfAppHelperSparseRuntimeRegressionSuite g_ospfAppHelperSparseRuntimeRegressionSuite;

} // namespace

} // namespace ns3