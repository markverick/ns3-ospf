/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"
#include "ns3/ipv4.h"

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

class OspfEnableDisableUnitTestCase : public TestCase
{
public:
  OspfEnableDisableUnitTestCase ()
    : TestCase ("Enable-Disable can toggle OSPF Tx")
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
    ipv4.SetBase ("10.43.1.0", "255.255.255.252");
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
    NS_TEST_ASSERT_MSG_EQ (txWhileDisabled, txAfterDisable, "expected Tx to stop while disabled");
    NS_TEST_ASSERT_MSG_GT (txAfterEnable, txWhileDisabled, "expected Tx to resume after Enable()");

    Simulator::Destroy ();
  }
};

class OspfAutoSyncPollDoesNotResumeWhenDisabledUnitTestCase : public TestCase
{
public:
  OspfAutoSyncPollDoesNotResumeWhenDisabledUnitTestCase ()
    : TestCase ("AutoSync polling does not resume Tx while disabled")
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
    ipv4.SetBase ("10.45.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (200)));

    // Enable polling so we'd notice if it accidentally restarts sockets/hellos while disabled.
    ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
    ospf.SetAttribute ("InterfaceSyncInterval", TimeValue (MilliSeconds (20)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t tx = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx));

    uint32_t txAfterDisable = 0;
    uint32_t txWhileDisabled = 0;

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.7));

    Simulator::Schedule (Seconds (0.16), &OspfApp::Disable, app0);
    Simulator::Schedule (Seconds (0.18), &SnapshotU32, &txAfterDisable, &tx);
    Simulator::Schedule (Seconds (0.60), &SnapshotU32, &txWhileDisabled, &tx);

    Simulator::Stop (Seconds (0.7));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (txAfterDisable, 0u, "expected some Tx before Disable() takes effect");
    NS_TEST_ASSERT_MSG_EQ (txWhileDisabled, txAfterDisable, "expected Tx to stay stopped while disabled");

    Simulator::Destroy ();
  }
};

class OspfEnableReflectsInterfaceChangesWhileDisabledUnitTestCase : public TestCase
{
public:
  OspfEnableReflectsInterfaceChangesWhileDisabledUnitTestCase ()
    : TestCase ("Enable reflects interface changes while disabled")
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
    ipv4.SetBase ("10.46.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (200)));

    // Enable auto-sync so Enable() seeds from current Ipv4 interface state.
    ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
    ospf.SetAttribute ("InterfaceSyncInterval", TimeValue (MilliSeconds (20)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv40, nullptr, "expected Ipv4");
    const uint32_t if0 = d01.Get (0)->GetIfIndex ();

    uint32_t tx = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementTxCounter, &tx));

    uint32_t txAfterDisable = 0;
    uint32_t txAfterEnableWithIfDown = 0;
    uint32_t txAfterIfUp = 0;

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (1.0));

    // Disable, then take interface down while disabled.
    Simulator::Schedule (Seconds (0.16), &OspfApp::Disable, app0);
    Simulator::Schedule (Seconds (0.18), &SnapshotU32, &txAfterDisable, &tx);
    Simulator::Schedule (Seconds (0.20), &Ipv4::SetDown, ipv40, if0);

    // Enable while interface is down: should not resume Tx.
    Simulator::Schedule (Seconds (0.30), &OspfApp::Enable, app0);
    Simulator::Schedule (Seconds (0.45), &SnapshotU32, &txAfterEnableWithIfDown, &tx);

    // Bring interface up and expect Tx to resume.
    Simulator::Schedule (Seconds (0.50), &Ipv4::SetUp, ipv40, if0);
    Simulator::Schedule (Seconds (0.80), &SnapshotU32, &txAfterIfUp, &tx);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (txAfterDisable, 0u, "expected some Tx before Disable() takes effect");
    NS_TEST_ASSERT_MSG_EQ (txAfterEnableWithIfDown, txAfterDisable,
                           "expected Tx to remain stopped when enabled with interface down");
    NS_TEST_ASSERT_MSG_GT (txAfterIfUp, txAfterEnableWithIfDown,
                           "expected Tx to resume after interface is brought up");

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
    ipv4.SetBase ("10.47.1.0", "255.255.255.252");
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
    Simulator::Schedule (Seconds (0.17), &OspfApp::Disable, app0);
    Simulator::Schedule (Seconds (0.18), &SnapshotU32, &txAfterDisable, &tx);
    Simulator::Schedule (Seconds (0.35), &SnapshotU32, &txWhileDisabled, &tx);
    Simulator::Schedule (Seconds (0.36), &OspfApp::Enable, app0);
    Simulator::Schedule (Seconds (0.37), &OspfApp::Enable, app0);
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

class OspfInterfaceUpWhileDisabledDoesNotResumeUntilEnableUnitTestCase : public TestCase
{
public:
  OspfInterfaceUpWhileDisabledDoesNotResumeUntilEnableUnitTestCase ()
    : TestCase ("Interface up while disabled does not resume Tx until Enable")
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
    ipv4.SetBase ("10.50.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv40, nullptr, "expected Ipv4");
    const uint32_t if0 = d01.Get (0)->GetIfIndex ();

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

    uint32_t txAfterDisable = 0;
    uint32_t txAfterIfUpWhileDisabled = 0;
    uint32_t txAfterEnable = 0;

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.8));

    // Disable, then bring the interface down and up while still disabled.
    Simulator::Schedule (Seconds (0.16), &OspfApp::Disable, app0);
    Simulator::Schedule (Seconds (0.18), &SnapshotU32, &txAfterDisable, &tx);
    Simulator::Schedule (Seconds (0.20), &Ipv4::SetDown, ipv40, if0);
    Simulator::Schedule (Seconds (0.30), &Ipv4::SetUp, ipv40, if0);
    Simulator::Schedule (Seconds (0.55), &SnapshotU32, &txAfterIfUpWhileDisabled, &tx);

    // Only after Enable should Tx resume.
    Simulator::Schedule (Seconds (0.56), &OspfApp::Enable, app0);
    Simulator::Schedule (Seconds (0.75), &SnapshotU32, &txAfterEnable, &tx);

    Simulator::Stop (Seconds (0.8));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_GT (txAfterDisable, 0u, "expected some Tx before Disable() takes effect");
    NS_TEST_ASSERT_MSG_EQ (txAfterIfUpWhileDisabled, txAfterDisable,
                           "expected Tx to stay stopped even if interface becomes up while disabled");
    NS_TEST_ASSERT_MSG_GT (txAfterEnable, txAfterIfUpWhileDisabled,
                           "expected Tx to resume only after Enable()");

    Simulator::Destroy ();
  }
};

class OspfEnableDisableUnitTestSuite : public TestSuite
{
public:
  OspfEnableDisableUnitTestSuite ()
    : TestSuite ("ospf-enable-disable", UNIT)
  {
    AddTestCase (new OspfEnableDisableUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfAutoSyncPollDoesNotResumeWhenDisabledUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfEnableReflectsInterfaceChangesWhileDisabledUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfDisableEnableIdempotentUnitTestCase (), TestCase::QUICK);
    AddTestCase (new OspfInterfaceUpWhileDisabledDoesNotResumeUntilEnableUnitTestCase (), TestCase::QUICK);
  }
};

static OspfEnableDisableUnitTestSuite g_ospfEnableDisableUnitTestSuite;

} // namespace ns3
