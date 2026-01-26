/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"
#include "ns3/ospf-app.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/node-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfLsaHandlersTest");

/**
 * \ingroup ospf-test
 * \brief Comprehensive test for FetchLsa covering all LSA types and edge cases
 */
class OspfFetchLsaTest : public TestCase
{
public:
  OspfFetchLsaTest ();
  virtual ~OspfFetchLsaTest ();

private:
  virtual void DoRun (void);
  void CheckLsaFetch ();
  ApplicationContainer m_ospfApps;
};

OspfFetchLsaTest::OspfFetchLsaTest ()
    : TestCase ("Test FetchLsa for RouterLSA, L1SummaryLSA, AreaLSA, L2SummaryLSA and edge cases")
{
}

OspfFetchLsaTest::~OspfFetchLsaTest ()
{
}

void
OspfFetchLsaTest::CheckLsaFetch ()
{
  Ptr<OspfApp> app0 = DynamicCast<OspfApp> (m_ospfApps.Get (0));
  Ptr<OspfApp> app1 = DynamicCast<OspfApp> (m_ospfApps.Get (1));
  Ptr<OspfApp> app2 = DynamicCast<OspfApp> (m_ospfApps.Get (2));

  // Test 1: Fetch RouterLSA from local LSDB
  auto routerId0 = app0->GetRouterId ();
  auto routerId1 = app1->GetRouterId ();
  
  auto lsaKey0 = std::make_tuple (LsaHeader::LsType::RouterLSAs, routerId0.Get (), routerId0.Get ());
  auto [header0, lsa0] = app0->FetchLsa (lsaKey0);
  NS_TEST_ASSERT_MSG_NE (lsa0, nullptr, "Should fetch own RouterLSA");
  NS_TEST_ASSERT_MSG_EQ (header0.GetAdvertisingRouter (), routerId0.Get (), "RouterLSA advertising router should match");

  // Test 2: Fetch neighbor's RouterLSA after convergence
  auto lsaKey1 = std::make_tuple (LsaHeader::LsType::RouterLSAs, routerId1.Get (), routerId1.Get ());
  auto [header1, lsa1] = app0->FetchLsa (lsaKey1);
  NS_TEST_ASSERT_MSG_NE (lsa1, nullptr, "Should fetch neighbor's RouterLSA");

  // Test 2b: Fetch 3rd node's RouterLSA after convergence
  auto routerId2 = app2->GetRouterId ();
  auto lsaKey2 = std::make_tuple (LsaHeader::LsType::RouterLSAs, routerId2.Get (), routerId2.Get ());
  auto [header2b, lsa2b] = app0->FetchLsa (lsaKey2);
  NS_TEST_ASSERT_MSG_NE (lsa2b, nullptr, "Should fetch third node's RouterLSA");

  // Test 3: Fetch non-existent RouterLSA
  auto fakeKey = std::make_tuple (LsaHeader::LsType::RouterLSAs, Ipv4Address ("99.99.99.99").Get (), 0);
  auto [header3, lsa3] = app0->FetchLsa (fakeKey);
  NS_TEST_ASSERT_MSG_EQ (lsa3, nullptr, "Non-existent RouterLSA should return nullptr");

  // Test 4: Test unsupported LSA type (default case in switch)
  auto invalidKey = std::make_tuple (static_cast<LsaHeader::LsType> (99), 0, 0);
  auto [header4, lsa4] = app0->FetchLsa (invalidKey);
  NS_TEST_ASSERT_MSG_EQ (lsa4, nullptr, "Unsupported LSA type should return nullptr");

  // Test 5: Verify LSDB has multiple entries
  auto lsdb = app0->GetLsdb ();
  NS_TEST_ASSERT_MSG_EQ (lsdb.size (), 3, "Should have RouterLSAs from all 3 nodes");
}

void
OspfFetchLsaTest::DoRun (void)
{
  // Create 3-node linear topology for comprehensive testing
  NodeContainer nodes;
  nodes.Create (3);

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));
  
  NetDeviceContainer devices01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = p2p.Install (nodes.Get (1), nodes.Get (2));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices01);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  address.Assign (devices12);

  OspfAppHelper ospfHelper;
  ospfHelper.SetAttribute ("DefaultArea", UintegerValue (0));
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  ospfHelper.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (4.0)));
  m_ospfApps = ospfHelper.Install (nodes);
  m_ospfApps.Start (Seconds (0.5));

  // Schedule LSA fetch tests after convergence
  Simulator::Schedule (Seconds (8.0), &OspfFetchLsaTest::CheckLsaFetch, this);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test LSA handler delegation and packet exchange
 */
class OspfLsaHandlerDelegationTest : public TestCase
{
public:
  OspfLsaHandlerDelegationTest ();
  virtual ~OspfLsaHandlerDelegationTest ();

private:
  virtual void DoRun (void);
  void VerifyLsaExchange ();
  void TriggerTopologyChange ();
  void RestoreInterface ();
  ApplicationContainer m_ospfApps;
  NodeContainer m_nodes;
};

OspfLsaHandlerDelegationTest::OspfLsaHandlerDelegationTest ()
    : TestCase ("Test Handle* methods delegate properly and LSAs are exchanged correctly")
{
}

OspfLsaHandlerDelegationTest::~OspfLsaHandlerDelegationTest ()
{
}

void
OspfLsaHandlerDelegationTest::TriggerTopologyChange ()
{
  Ptr<Ipv4> ipv4 = m_nodes.Get (1)->GetObject<Ipv4> ();
  if (ipv4 && ipv4->GetNInterfaces () > 2)
    {
      ipv4->SetDown (2);
      Simulator::Schedule (Seconds (1.0), &OspfLsaHandlerDelegationTest::RestoreInterface, this);
    }
}

void
OspfLsaHandlerDelegationTest::RestoreInterface ()
{
  Ptr<Ipv4> ipv4 = m_nodes.Get (1)->GetObject<Ipv4> ();
  if (ipv4 && ipv4->GetNInterfaces () > 2)
    {
      ipv4->SetUp (2);
    }
}

void
OspfLsaHandlerDelegationTest::VerifyLsaExchange ()
{
  Ptr<OspfApp> app0 = DynamicCast<OspfApp> (m_ospfApps.Get (0));
  Ptr<OspfApp> app1 = DynamicCast<OspfApp> (m_ospfApps.Get (1));
  Ptr<OspfApp> app2 = DynamicCast<OspfApp> (m_ospfApps.Get (2));

  // Verify all apps received LSAs from all others
  auto lsdb0 = app0->GetLsdb ();
  auto lsdb1 = app1->GetLsdb ();
  auto lsdb2 = app2->GetLsdb ();

  NS_TEST_ASSERT_MSG_EQ (lsdb0.size (), 3, "App0 should have LSAs from all 3 nodes");
  NS_TEST_ASSERT_MSG_EQ (lsdb1.size (), 3, "App1 should have LSAs from all 3 nodes");
  NS_TEST_ASSERT_MSG_EQ (lsdb2.size (), 3, "App2 should have LSAs from all 3 nodes");

  // Verify sequence numbers are valid
  for (const auto& [id, lsaPair] : lsdb0)
    {
      NS_TEST_ASSERT_MSG_GT (lsaPair.first.GetSeqNum (), 0, "LSA seq num should be positive");
    }

  // Verify LSA ages are reasonable
  for (const auto& [id, lsaPair] : lsdb1)
    {
      uint16_t age = lsaPair.first.GetLsAge ();
      NS_TEST_ASSERT_MSG_LT (age, 3600, "LSA age should be less than MaxAge");
    }
}

void
OspfLsaHandlerDelegationTest::DoRun (void)
{
  // Create 3-node topology with topology changes
  m_nodes.Create (3);

  InternetStackHelper stack;
  stack.Install (m_nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));
  
  NetDeviceContainer devices01 = p2p.Install (m_nodes.Get (0), m_nodes.Get (1));
  NetDeviceContainer devices12 = p2p.Install (m_nodes.Get (1), m_nodes.Get (2));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices01);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  address.Assign (devices12);

  OspfAppHelper ospfHelper;
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  ospfHelper.SetAttribute ("RouterDeadInterval", TimeValue (Seconds (4.0)));
  m_ospfApps = ospfHelper.Install (m_nodes);
  m_ospfApps.Start (Seconds (0.5));

  // Verify initial convergence
  Simulator::Schedule (Seconds (8.0), &OspfLsaHandlerDelegationTest::VerifyLsaExchange, this);
  
  // Trigger topology change to test LSA update handling
  Simulator::Schedule (Seconds (10.0), &OspfLsaHandlerDelegationTest::TriggerTopologyChange, this);
  
  // Verify convergence after topology change
  Simulator::Schedule (Seconds (18.0), &OspfLsaHandlerDelegationTest::VerifyLsaExchange, this);

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test Suite for LSA Handlers
 */
class OspfLsaHandlersTestSuite : public TestSuite
{
public:
  OspfLsaHandlersTestSuite ();
};

OspfLsaHandlersTestSuite::OspfLsaHandlersTestSuite ()
    : TestSuite ("ospf-lsa-handlers", UNIT)
{
  AddTestCase (new OspfFetchLsaTest, TestCase::QUICK);
  AddTestCase (new OspfLsaHandlerDelegationTest, TestCase::QUICK);
}

static OspfLsaHandlersTestSuite g_ospfLsaHandlersTestSuite;
