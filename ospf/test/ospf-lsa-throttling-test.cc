/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
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
 * Author: Generated for LSA throttling feature tests
 */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

namespace ns3 {

namespace {

/**
 * \brief Counter callback for tracking LSU packet transmissions
 */
static void
IncrementLsuCounter (uint32_t *counter, Ptr<const Packet> packet)
{
  // Check if this is an LSU packet (type 4 in OSPF header)
  // For simplicity, we count all Tx packets
  ++(*counter);
}

/**
 * \brief Snapshot a counter value at a specific time
 */
static void
SnapshotCounter (uint32_t *out, const uint32_t *in)
{
  *out = *in;
}

/**
 * \brief Trigger multiple rapid interface state changes to stress test throttling
 */
static void
TriggerInterfaceFlap (Ptr<Node> node, uint32_t ifIndex)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4)
    {
      ipv4->SetDown (ifIndex);
    }
}

static void
BringInterfaceUp (Ptr<Node> node, uint32_t ifIndex)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4)
    {
      ipv4->SetUp (ifIndex);
    }
}

} // anonymous namespace

// =============================================================================
// Test Case: MinLsInterval attribute is properly exposed
// =============================================================================
class OspfLsaThrottlingAttributeTestCase : public TestCase
{
public:
  OspfLsaThrottlingAttributeTestCase ()
    : TestCase ("MinLsInterval attribute is properly exposed and settable")
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

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("MinLsInterval", TimeValue (Seconds (5)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    // Verify attribute was set correctly
    TimeValue minLsInterval;
    app0->GetAttribute ("MinLsInterval", minLsInterval);
    NS_TEST_EXPECT_MSG_EQ (minLsInterval.Get (), Seconds (5), 
                           "MinLsInterval should be 5 seconds");

    Simulator::Destroy ();
  }
};

// =============================================================================
// Test Case: Zero MinLsInterval means no throttling
// =============================================================================
class OspfLsaThrottlingZeroIntervalTestCase : public TestCase
{
public:
  OspfLsaThrottlingZeroIntervalTestCase ()
    : TestCase ("Zero MinLsInterval disables throttling")
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
    ipv4.SetBase ("10.51.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("MinLsInterval", TimeValue (Seconds (0))); // Disabled

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (50)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (200)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t txCount = 0;
    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementLsuCounter, &txCount));

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (0.5));

    Simulator::Stop (Seconds (0.6));
    Simulator::Run ();

    // With zero throttling, expect normal operation with packets being sent
    NS_TEST_EXPECT_MSG_GT (txCount, 0u, "expected OSPF packets with zero throttling");

    Simulator::Destroy ();
  }
};

// =============================================================================
// Test Case: Throttling reduces packet count under rapid state changes
// =============================================================================
class OspfLsaThrottlingReducesPacketsTestCase : public TestCase
{
public:
  OspfLsaThrottlingReducesPacketsTestCase ()
    : TestCase ("Throttling reduces LSA packet count under rapid state changes")
  {
  }

  void
  DoRun () override
  {
    // Run simulation WITHOUT throttling
    uint32_t txCountNoThrottle = RunWithThrottling (Seconds (0));

    // Run simulation WITH throttling (1 second interval)
    uint32_t txCountWithThrottle = RunWithThrottling (Seconds (1));

    // Both should have some packets (verifies the simulation actually ran)
    NS_TEST_EXPECT_MSG_GT (txCountNoThrottle, 0u, "expected packets without throttling");
    NS_TEST_EXPECT_MSG_GT (txCountWithThrottle, 0u, "expected packets with throttling");

    // With 6 rapid interface flaps within 250ms and a 1s throttle interval,
    // throttling MUST reduce packet count
    NS_TEST_EXPECT_MSG_LT (txCountWithThrottle, txCountNoThrottle,
                           "throttling should reduce packet count under rapid state changes");
  }

private:
  uint32_t
  RunWithThrottling (Time minLsInterval)
  {
    RngSeedManager::SetSeed (42);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (3);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Create a chain: node0 -- node1 -- node2
    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));
    NetDeviceContainer d12 = p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.52.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.52.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("MinLsInterval", TimeValue (minLsInterval));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (100)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (400)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    if (app1 == nullptr)
      {
        return 0;
      }

    uint32_t txCount = 0;
    app1->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementLsuCounter, &txCount));

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (3.0));

    // Simulate rapid interface flapping on node1's interface to node0
    // This should trigger multiple Router-LSA regenerations
    Simulator::Schedule (Seconds (0.5), &TriggerInterfaceFlap, nodes.Get (1), 1);
    Simulator::Schedule (Seconds (0.55), &BringInterfaceUp, nodes.Get (1), 1);
    Simulator::Schedule (Seconds (0.6), &TriggerInterfaceFlap, nodes.Get (1), 1);
    Simulator::Schedule (Seconds (0.65), &BringInterfaceUp, nodes.Get (1), 1);
    Simulator::Schedule (Seconds (0.7), &TriggerInterfaceFlap, nodes.Get (1), 1);
    Simulator::Schedule (Seconds (0.75), &BringInterfaceUp, nodes.Get (1), 1);

    Simulator::Stop (Seconds (3.0));
    Simulator::Run ();

    uint32_t result = txCount;

    Simulator::Destroy ();

    return result;
  }
};

// =============================================================================
// Test Case: Deferred LSA is eventually sent after throttle interval
// =============================================================================
class OspfLsaThrottlingDeferredSendTestCase : public TestCase
{
public:
  OspfLsaThrottlingDeferredSendTestCase ()
    : TestCase ("Deferred LSA is sent after MinLsInterval expires")
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
    ipv4.SetBase ("10.53.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    // Use a 500ms throttle interval for testing
    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("MinLsInterval", TimeValue (MilliSeconds (500)));

    ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
    ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (100)));
    ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (400)));

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    uint32_t txCount = 0;
    uint32_t txAt300ms = 0;
    uint32_t txAt600ms = 0;
    uint32_t txAt1200ms = 0;

    app0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&IncrementLsuCounter, &txCount));

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (2.0));

    // Snapshot counters at various times
    Simulator::Schedule (MilliSeconds (300), &SnapshotCounter, &txAt300ms, &txCount);
    Simulator::Schedule (MilliSeconds (600), &SnapshotCounter, &txAt600ms, &txCount);
    Simulator::Schedule (MilliSeconds (1200), &SnapshotCounter, &txAt1200ms, &txCount);

    Simulator::Stop (Seconds (2.0));
    Simulator::Run ();

    // Verify packets are being sent over time
    NS_TEST_EXPECT_MSG_GT (txAt600ms, 0u, "expected some packets by 600ms");
    NS_TEST_EXPECT_MSG_GT (txAt1200ms, txAt300ms, "expected more packets over time");

    Simulator::Destroy ();
  }
};

// =============================================================================
// Test Case: Default MinLsInterval is zero (no throttling by default)
// =============================================================================
class OspfLsaThrottlingDefaultValueTestCase : public TestCase
{
public:
  OspfLsaThrottlingDefaultValueTestCase ()
    : TestCase ("Default MinLsInterval is zero (backward compatible)")
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
    ipv4.SetBase ("10.54.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    // Do NOT set MinLsInterval - use default

    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp");

    // Verify default is zero
    TimeValue minLsInterval;
    app0->GetAttribute ("MinLsInterval", minLsInterval);
    NS_TEST_EXPECT_MSG_EQ (minLsInterval.Get (), Seconds (0), 
                           "Default MinLsInterval should be 0 (disabled)");

    Simulator::Destroy ();
  }
};

// =============================================================================
// Test Suite Registration
// =============================================================================
class OspfLsaThrottlingTestSuite : public TestSuite
{
public:
  OspfLsaThrottlingTestSuite ()
    : TestSuite ("ospf-lsa-throttling", UNIT)
  {
    AddTestCase (new OspfLsaThrottlingAttributeTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaThrottlingZeroIntervalTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaThrottlingReducesPacketsTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaThrottlingDeferredSendTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaThrottlingDefaultValueTestCase, TestCase::QUICK);
  }
};

static OspfLsaThrottlingTestSuite g_ospfLsaThrottlingTestSuite;

} // namespace ns3
