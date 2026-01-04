/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

#include "ns3/ospf-interface.h"

namespace ns3 {

namespace {

static void
IncrementTxCounter (uint32_t *counter, Ptr<const Packet>)
{
  ++(*counter);
}

static void
SnapshotU32 (uint32_t *out, const uint32_t *in)
{
  *out = *in;
}

} // namespace

class OspfInterfaceFlagsUnitTestCase : public TestCase
{
public:
  OspfInterfaceFlagsUnitTestCase ()
    : TestCase ("OspfInterface exposes up-down state and clears neighbors")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfInterface> iface = CreateObject<OspfInterface> ();
    NS_TEST_ASSERT_MSG_EQ (iface->IsUp (), true, "default OspfInterface should be up");

    iface->SetUp (false);
    NS_TEST_ASSERT_MSG_EQ (iface->IsUp (), false, "SetUp(false) should mark interface down");

    iface->SetUp (true);
    NS_TEST_ASSERT_MSG_EQ (iface->IsUp (), true, "SetUp(true) should mark interface up");

    Ptr<OspfNeighbor> n1 = CreateObject<OspfNeighbor> (Ipv4Address ("10.0.0.1"), Ipv4Address ("10.0.0.2"),
                                                      0, OspfNeighbor::Init);
    Ptr<OspfNeighbor> n2 = CreateObject<OspfNeighbor> (Ipv4Address ("10.0.0.3"), Ipv4Address ("10.0.0.4"),
                                                      0, OspfNeighbor::Init);

    iface->AddNeighbor (n1);
    iface->AddNeighbor (n2);
    NS_TEST_ASSERT_MSG_EQ (iface->GetNeighbors ().size (), 2u, "expected 2 neighbors");

    iface->ClearNeighbors ();
    NS_TEST_ASSERT_MSG_EQ (iface->GetNeighbors ().size (), 0u, "expected neighbors cleared");
  }
};

class OspfAutoSyncSkipsDownInterfaceUnitTestCase : public TestCase
{
public:
  OspfAutoSyncSkipsDownInterfaceUnitTestCase ()
    : TestCase ("AutoSyncInterfaces skips a down Ipv4 interface (no OSPF Tx)")
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

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.40.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    // Bring the only non-loopback interface down before app start.
    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv40, nullptr, "expected Ipv4");
    const uint32_t if0 = d01.Get (0)->GetIfIndex ();
    ipv40->SetDown (if0);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    // Tight timings so any unexpected Tx would show up.
    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (100)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (300)));

    // Enable auto-sync, but disable periodic polling to keep this test minimal.
    ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
    ospf.SetAttribute ("InterfaceSyncInterval", TimeValue (Seconds (0)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t tx0 = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx0));

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.5));

    Simulator::Stop (Seconds (0.5));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_EQ (tx0, 0u, "expected no OSPF Tx when the only interface is down");

    Simulator::Destroy ();
  }
};

class OspfAutoSyncSendsOnUpInterfaceUnitTestCase : public TestCase
{
public:
  OspfAutoSyncSendsOnUpInterfaceUnitTestCase ()
    : TestCase ("AutoSyncInterfaces still sends on an up Ipv4 interface")
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

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.41.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (100)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (300)));

    ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
    ospf.SetAttribute ("InterfaceSyncInterval", TimeValue (Seconds (0)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t tx0 = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx0));

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.5));

    Simulator::Stop (Seconds (0.5));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (tx0, 0u, "expected OSPF Tx on an up interface");

    Simulator::Destroy ();
  }
};

class OspfAutoSyncPollingUpTransitionStartsTxUnitTestCase : public TestCase
{
public:
  OspfAutoSyncPollingUpTransitionStartsTxUnitTestCase ()
    : TestCase ("AutoSync polling starts Tx when interface becomes up")
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

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.48.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv40, nullptr, "expected Ipv4");
    const uint32_t if0 = d01.Get (0)->GetIfIndex ();
    ipv40->SetDown (if0);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (200)));

    ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
    ospf.SetAttribute ("InterfaceSyncInterval", TimeValue (MilliSeconds (20)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t tx = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx));

    uint32_t txBeforeUp = 0;
    uint32_t txAfterUp = 0;

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.6));

    Simulator::Schedule (Seconds (0.20), &SnapshotU32, &txBeforeUp, &tx);
    Simulator::Schedule (Seconds (0.25), &Ipv4::SetUp, ipv40, if0);
    Simulator::Schedule (Seconds (0.55), &SnapshotU32, &txAfterUp, &tx);

    Simulator::Stop (Seconds (0.6));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_EQ (txBeforeUp, 0u, "expected no Tx before interface is brought up");
    NS_TEST_ASSERT_MSG_GT (txAfterUp, txBeforeUp, "expected Tx after interface becomes up");

    Simulator::Destroy ();
  }
};

class OspfAutoSyncNoPollingDoesNotReactToUpTransitionUnitTestCase : public TestCase
{
public:
  OspfAutoSyncNoPollingDoesNotReactToUpTransitionUnitTestCase ()
    : TestCase ("AutoSync without polling does not react to later interface-up")
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

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.49.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv40, nullptr, "expected Ipv4");
    const uint32_t if0 = d01.Get (0)->GetIfIndex ();
    ipv40->SetDown (if0);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (200)));

    ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
    // One-shot sync only; no periodic polling.
    ospf.SetAttribute ("InterfaceSyncInterval", TimeValue (Seconds (0)));

    ApplicationContainer apps = ospf.Install (nodes);
    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t tx = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx));

    uint32_t txBeforeUp = 0;
    uint32_t txAfterUp = 0;

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.6));

    Simulator::Schedule (Seconds (0.20), &SnapshotU32, &txBeforeUp, &tx);
    Simulator::Schedule (Seconds (0.25), &Ipv4::SetUp, ipv40, if0);
    Simulator::Schedule (Seconds (0.55), &SnapshotU32, &txAfterUp, &tx);

    Simulator::Stop (Seconds (0.6));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_EQ (txBeforeUp, 0u, "expected no Tx before interface is brought up");
    NS_TEST_ASSERT_MSG_EQ (txAfterUp, 0u,
                           "expected no Tx after interface-up when InterfaceSyncInterval=0");

    Simulator::Destroy ();
  }
};

class OspfDisableEnableStopsAndResumesTxUnitTestCase : public TestCase
{
public:
  OspfDisableEnableStopsAndResumesTxUnitTestCase ()
    : TestCase ("Disable-Enable stops and resumes OSPF Tx")
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

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.42.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (200)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t tx = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx));

    uint32_t txBeforeDisable = 0;
    uint32_t txAfterDisable = 0;
    uint32_t txWhileDisabled = 0;
    uint32_t txAfterEnable = 0;

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.6));

    Simulator::Schedule (Seconds (0.15), &SnapshotU32, &txBeforeDisable, &tx);
    Simulator::Schedule (Seconds (0.16), &OspfApp::Disable, app0);
    // Allow any already-queued Tx at ~Disable() time to drain.
    Simulator::Schedule (Seconds (0.17), &SnapshotU32, &txAfterDisable, &tx);
    Simulator::Schedule (Seconds (0.30), &SnapshotU32, &txWhileDisabled, &tx);
    Simulator::Schedule (Seconds (0.31), &OspfApp::Enable, app0);
    Simulator::Schedule (Seconds (0.50), &SnapshotU32, &txAfterEnable, &tx);

    Simulator::Stop (Seconds (0.6));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (txBeforeDisable, 0u, "expected some OSPF Tx before Disable()");
    NS_TEST_ASSERT_MSG_EQ (txWhileDisabled, txAfterDisable,
                           "expected no additional Tx while disabled");
    NS_TEST_ASSERT_MSG_GT (txAfterEnable, txWhileDisabled, "expected Tx to resume after Enable()");

    Simulator::Destroy ();
  }
};

class OspfDisableEnableIdempotentUnitTestCase : public TestCase
{
public:
  OspfDisableEnableIdempotentUnitTestCase ()
    : TestCase ("Disable-Enable is idempotent")
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

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.44.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (200)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t tx = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx));

    uint32_t txBeforeDisable = 0;
    uint32_t txAfterDisable = 0;
    uint32_t txWhileDisabled = 0;
    uint32_t txAfterEnable = 0;

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.7));

    Simulator::Schedule (Seconds (0.15), &SnapshotU32, &txBeforeDisable, &tx);
    Simulator::Schedule (Seconds (0.16), &OspfApp::Disable, app0);
    Simulator::Schedule (Seconds (0.17), &OspfApp::Disable, app0); // idempotent
    Simulator::Schedule (Seconds (0.18), &SnapshotU32, &txAfterDisable, &tx);
    Simulator::Schedule (Seconds (0.35), &SnapshotU32, &txWhileDisabled, &tx);
    Simulator::Schedule (Seconds (0.36), &OspfApp::Enable, app0);
    Simulator::Schedule (Seconds (0.37), &OspfApp::Enable, app0); // idempotent
    Simulator::Schedule (Seconds (0.60), &SnapshotU32, &txAfterEnable, &tx);

    Simulator::Stop (Seconds (0.7));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (txBeforeDisable, 0u, "expected some OSPF Tx before Disable()");
    NS_TEST_ASSERT_MSG_EQ (txWhileDisabled, txAfterDisable,
                           "expected no additional Tx while disabled");
    NS_TEST_ASSERT_MSG_GT (txAfterEnable, txWhileDisabled, "expected Tx to resume after Enable()");

    Simulator::Destroy ();
  }
};

class OspfInterfaceTrackingUnitTestSuite : public TestSuite
{
public:
  OspfInterfaceTrackingUnitTestSuite ()
    : TestSuite ("ospf-interface-tracking", UNIT)
  {
    AddTestCase (new OspfInterfaceFlagsUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfAutoSyncSkipsDownInterfaceUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfAutoSyncSendsOnUpInterfaceUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfAutoSyncPollingUpTransitionStartsTxUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfAutoSyncNoPollingDoesNotReactToUpTransitionUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfDisableEnableStopsAndResumesTxUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfDisableEnableIdempotentUnitTestCase (), TestCase::QUICK);
  }
};

static OspfInterfaceTrackingUnitTestSuite g_ospfInterfaceTrackingUnitTestSuite;

} // namespace ns3
