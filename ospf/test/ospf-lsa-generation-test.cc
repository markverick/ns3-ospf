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

NS_LOG_COMPONENT_DEFINE ("OspfLsaGenerationTest");

/**
 * \ingroup ospf-test
 * \brief Comprehensive test for RouterLSA generation with sequence numbers
 */
class OspfRecomputeRouterLsaTest : public TestCase
{
public:
  OspfRecomputeRouterLsaTest ();
  virtual ~OspfRecomputeRouterLsaTest ();

private:
  virtual void DoRun (void);
  void VerifyRouterLsaGeneration ();
  void TriggerRecompute ();
  void VerifySeqNumIncrement ();
  ApplicationContainer m_ospfApps;
  NodeContainer m_nodes;
  std::map<uint32_t, uint32_t> m_initialSeqNums;
};

OspfRecomputeRouterLsaTest::OspfRecomputeRouterLsaTest ()
    : TestCase ("Test RecomputeRouterLsa generation, flooding, and sequence number increment")
{
}

OspfRecomputeRouterLsaTest::~OspfRecomputeRouterLsaTest ()
{
}

void
OspfRecomputeRouterLsaTest::VerifyRouterLsaGeneration ()
{
  for (uint32_t i = 0; i < m_ospfApps.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (m_ospfApps.Get (i));
      auto lsdb = app->GetLsdb ();
      
      NS_TEST_ASSERT_MSG_EQ (lsdb.size (), m_ospfApps.GetN (), 
                             "Should have RouterLSAs from all nodes");
      
      auto routerId = app->GetRouterId ();
      NS_TEST_ASSERT_MSG_GT (lsdb[routerId.Get ()].first.GetSeqNum (), 0,
                             "Own RouterLSA sequence number should be positive");
      
      // Store initial sequence numbers
      m_initialSeqNums[routerId.Get ()] = lsdb[routerId.Get ()].first.GetSeqNum ();
    }
}

void
OspfRecomputeRouterLsaTest::TriggerRecompute ()
{
  // Bring down an interface to trigger LSA recomputation
  Ptr<Ipv4> ipv4 = m_nodes.Get (0)->GetObject<Ipv4> ();
  if (ipv4 && ipv4->GetNInterfaces () > 1)
    {
      ipv4->SetDown (1);
    }
}

void
OspfRecomputeRouterLsaTest::VerifySeqNumIncrement ()
{
  Ptr<OspfApp> app0 = DynamicCast<OspfApp> (m_ospfApps.Get (0));
  auto lsdb = app0->GetLsdb ();
  auto routerId = app0->GetRouterId ();
  
  uint32_t currentSeqNum = lsdb[routerId.Get ()].first.GetSeqNum ();
  uint32_t initialSeqNum = m_initialSeqNums[routerId.Get ()];
  
  NS_TEST_ASSERT_MSG_GT (currentSeqNum, initialSeqNum,
                         "Sequence number should increment after topology change");
}

void
OspfRecomputeRouterLsaTest::DoRun (void)
{
  m_nodes.Create (4);

  InternetStackHelper stack;
  stack.Install (m_nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));
  
  NetDeviceContainer devices01 = p2p.Install (m_nodes.Get (0), m_nodes.Get (1));
  NetDeviceContainer devices12 = p2p.Install (m_nodes.Get (1), m_nodes.Get (2));
  NetDeviceContainer devices23 = p2p.Install (m_nodes.Get (2), m_nodes.Get (3));
  NetDeviceContainer devices30 = p2p.Install (m_nodes.Get (3), m_nodes.Get (0));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices01);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  address.Assign (devices12);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (devices23);
  address.SetBase ("10.1.4.0", "255.255.255.0");
  address.Assign (devices30);

  OspfAppHelper ospfHelper;
  // Needed for the interface-down event to be reflected in OSPF's bound interfaces.
  ospfHelper.SetAttribute ("AutoSyncInterfaces", BooleanValue (true));
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  m_ospfApps = ospfHelper.Install (m_nodes);
  m_ospfApps.Start (Seconds (0.5));

  Simulator::Schedule (Seconds (8.0), &OspfRecomputeRouterLsaTest::VerifyRouterLsaGeneration, this);
  Simulator::Schedule (Seconds (10.0), &OspfRecomputeRouterLsaTest::TriggerRecompute, this);
  Simulator::Schedule (Seconds (15.0), &OspfRecomputeRouterLsaTest::VerifySeqNumIncrement, this);

  Simulator::Stop (Seconds (18.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test L1SummaryLSA generation with external routes
 */
class OspfRecomputeL1SummaryLsaTest : public TestCase
{
public:
  OspfRecomputeL1SummaryLsaTest ();
  virtual ~OspfRecomputeL1SummaryLsaTest ();

private:
  virtual void DoRun (void);
  void VerifyL1SummaryGeneration ();
  ApplicationContainer m_ospfApps;
};

OspfRecomputeL1SummaryLsaTest::OspfRecomputeL1SummaryLsaTest ()
    : TestCase ("Test RecomputeL1SummaryLsa with external routes")
{
}

OspfRecomputeL1SummaryLsaTest::~OspfRecomputeL1SummaryLsaTest ()
{
}

void
OspfRecomputeL1SummaryLsaTest::VerifyL1SummaryGeneration ()
{
  for (uint32_t i = 0; i < m_ospfApps.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (m_ospfApps.Get (i));
      auto l1SummaryLsdb = app->GetL1SummaryLsdb ();
      
      NS_TEST_ASSERT_MSG_GT (l1SummaryLsdb.size (), 0, "Should have L1SummaryLSAs");
      
      for (const auto& [id, lsaPair] : l1SummaryLsdb)
        {
          NS_TEST_ASSERT_MSG_GT (lsaPair.first.GetSeqNum (), 0,
                                 "L1Summary sequence number should be positive");
          NS_TEST_ASSERT_MSG_NE (lsaPair.second, nullptr,
                                 "L1Summary body should not be null");
        }
    }
}

void
OspfRecomputeL1SummaryLsaTest::DoRun (void)
{
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
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  m_ospfApps = ospfHelper.Install (nodes);
  m_ospfApps.Start (Seconds (0.5));

  Simulator::Schedule (Seconds (8.0), &OspfRecomputeL1SummaryLsaTest::VerifyL1SummaryGeneration, this);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test Suite for LSA Generation
 */
class OspfLsaGenerationTestSuite : public TestSuite
{
public:
  OspfLsaGenerationTestSuite ();
};

OspfLsaGenerationTestSuite::OspfLsaGenerationTestSuite ()
    : TestSuite ("ospf-lsa-generation", UNIT)
{
  AddTestCase (new OspfRecomputeRouterLsaTest, TestCase::QUICK);
  AddTestCase (new OspfRecomputeL1SummaryLsaTest, TestCase::QUICK);
}

static OspfLsaGenerationTestSuite g_ospfLsaGenerationTestSuite;
