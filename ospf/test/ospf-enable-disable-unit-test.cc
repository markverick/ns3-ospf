/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

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

class OspfEnableDisableUnitTestSuite : public TestSuite
{
public:
  OspfEnableDisableUnitTestSuite ()
    : TestSuite ("ospf-enable-disable", UNIT)
  {
    AddTestCase (new OspfEnableDisableUnitTestCase (), TestCase::QUICK);
  }
};

static OspfEnableDisableUnitTestSuite g_ospfEnableDisableUnitTestSuite;

} // namespace ns3
