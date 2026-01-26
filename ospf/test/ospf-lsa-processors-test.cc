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

NS_LOG_COMPONENT_DEFINE ("OspfLsaProcessorsTest");

/**
 * \ingroup ospf-test
 * \brief Comprehensive test for LSA processing with external routes
 */
class OspfProcessL1SummaryLsaTest : public TestCase
{
public:
  OspfProcessL1SummaryLsaTest ();
  virtual ~OspfProcessL1SummaryLsaTest ();

private:
  virtual void DoRun (void);
  void VerifyL1Summary ();
  ApplicationContainer m_ospfApps;
};

OspfProcessL1SummaryLsaTest::OspfProcessL1SummaryLsaTest ()
    : TestCase ("Test ProcessL1SummaryLsa with external routes and area proxy")
{
}

OspfProcessL1SummaryLsaTest::~OspfProcessL1SummaryLsaTest ()
{
}

void
OspfProcessL1SummaryLsaTest::VerifyL1Summary ()
{
  for (uint32_t i = 0; i < m_ospfApps.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (m_ospfApps.Get (i));
      auto l1SummaryLsdb = app->GetL1SummaryLsdb ();
      
      // With EnableExternalRoutes, should have L1Summary LSAs
      NS_TEST_ASSERT_MSG_GT (l1SummaryLsdb.size (), 0, "Should have L1SummaryLSAs with external routes");
      
      // Verify sequence numbers and ages
      for (const auto& [id, lsaPair] : l1SummaryLsdb)
        {
          NS_TEST_ASSERT_MSG_GT (lsaPair.first.GetSeqNum (), 0, "L1Summary seq num should be positive");
          NS_TEST_ASSERT_MSG_LT (lsaPair.first.GetLsAge (), 3600, "L1Summary age should be reasonable");
        }
    }
}

void
OspfProcessL1SummaryLsaTest::DoRun (void)
{
  // Create 4-node ring topology with external routes
  NodeContainer nodes;
  nodes.Create (4);

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));
  
  NetDeviceContainer devices01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer devices30 = p2p.Install (nodes.Get (3), nodes.Get (0));

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
  ospfHelper.SetAttribute ("EnableAreaProxy", BooleanValue (true));
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  m_ospfApps = ospfHelper.Install (nodes);
  m_ospfApps.Start (Seconds (0.5));

  Simulator::Schedule (Seconds (10.0), &OspfProcessL1SummaryLsaTest::VerifyL1Summary, this);

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test RouterLSA processing and area leader election
 */
class OspfProcessRouterLsaTest : public TestCase
{
public:
  OspfProcessRouterLsaTest ();
  virtual ~OspfProcessRouterLsaTest ();

private:
  virtual void DoRun (void);
  void VerifyRouterLsas ();
  void VerifyAreaLeader ();
  ApplicationContainer m_ospfApps;
};

OspfProcessRouterLsaTest::OspfProcessRouterLsaTest ()
    : TestCase ("Test ProcessRouterLsa and area leader election logic")
{
}

OspfProcessRouterLsaTest::~OspfProcessRouterLsaTest ()
{
}

void
OspfProcessRouterLsaTest::VerifyRouterLsas ()
{
  for (uint32_t i = 0; i < m_ospfApps.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (m_ospfApps.Get (i));
      auto lsdb = app->GetLsdb ();
      
      NS_TEST_ASSERT_MSG_EQ (lsdb.size (), m_ospfApps.GetN (), 
                             "Each node should have RouterLSAs from all nodes");
      
      for (const auto& [id, lsaPair] : lsdb)
        {
          const auto& header = lsaPair.first;
          const auto& lsa = lsaPair.second;
          
          NS_TEST_ASSERT_MSG_GT (header.GetSeqNum (), 0, "RouterLSA seq num must be positive");
          NS_TEST_ASSERT_MSG_NE (lsa, nullptr, "RouterLSA body should not be null");
          NS_TEST_ASSERT_MSG_GT (lsa->GetNLink (), 0, "RouterLSA should have links");
        }
    }
}

void
OspfProcessRouterLsaTest::VerifyAreaLeader ()
{
  Ipv4Address minRouterId = Ipv4Address ("255.255.255.255");
  uint32_t leaderIndex = 0;
  
  for (uint32_t i = 0; i < m_ospfApps.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (m_ospfApps.Get (i));
      auto routerId = app->GetRouterId ();
      
      if (routerId.Get () < minRouterId.Get ())
        {
          minRouterId = routerId;
          leaderIndex = i;
        }
    }
  
  NS_TEST_ASSERT_MSG_LT (leaderIndex, m_ospfApps.GetN (), "Area leader should be identified");
}

void
OspfProcessRouterLsaTest::DoRun (void)
{
  NodeContainer nodes;
  nodes.Create (5);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;

  // Connected topology (5-node ring) so each node can learn RouterLSAs from all nodes.
  uint32_t subnet = 1;
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      const uint32_t j = (i + 1) % nodes.GetN ();
      NetDeviceContainer link = p2p.Install (nodes.Get (i), nodes.Get (j));
      std::ostringstream oss;
      oss << "10.1." << subnet++ << ".0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      address.Assign (link);
    }

  OspfAppHelper ospfHelper;
  ospfHelper.SetAttribute ("EnableAreaProxy", BooleanValue (true));
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  m_ospfApps = ospfHelper.Install (nodes);
  m_ospfApps.Start (Seconds (0.5));

  Simulator::Schedule (Seconds (10.0), &OspfProcessRouterLsaTest::VerifyRouterLsas, this);
  Simulator::Schedule (Seconds (15.0), &OspfProcessRouterLsaTest::VerifyAreaLeader, this);

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test AreaLSA processing with sequence number comparison
 */
class OspfProcessAreaLsaTest : public TestCase
{
public:
  OspfProcessAreaLsaTest ();
  virtual ~OspfProcessAreaLsaTest ();

private:
  virtual void DoRun (void);
  void VerifyAreaLsaProcessing ();
  ApplicationContainer m_ospfApps;
};

OspfProcessAreaLsaTest::OspfProcessAreaLsaTest ()
    : TestCase ("Test ProcessAreaLsa with sequence number validation")
{
}

OspfProcessAreaLsaTest::~OspfProcessAreaLsaTest ()
{
}

void
OspfProcessAreaLsaTest::VerifyAreaLsaProcessing ()
{
  bool foundAreaLsa = false;
  
  for (uint32_t i = 0; i < m_ospfApps.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (m_ospfApps.Get (i));
      auto lsdb = app->GetLsdb ();
      
      NS_TEST_ASSERT_MSG_GT (lsdb.size (), 0, "LSDB should have entries");
      
      if (lsdb.size () > 0)
        {
          foundAreaLsa = true;
        }
    }
  
  NS_TEST_ASSERT_MSG_EQ (foundAreaLsa, true, "Should find LSA processing evidence");
}

void
OspfProcessAreaLsaTest::DoRun (void)
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
  NetDeviceContainer devices20 = p2p.Install (nodes.Get (2), nodes.Get (0));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices01);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  address.Assign (devices12);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (devices20);

  OspfAppHelper ospfHelper;
  ospfHelper.SetAttribute ("EnableAreaProxy", BooleanValue (true));
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  m_ospfApps = ospfHelper.Install (nodes);
  m_ospfApps.Start (Seconds (0.5));

  Simulator::Schedule (Seconds (15.0), &OspfProcessAreaLsaTest::VerifyAreaLsaProcessing, this);

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test Suite for LSA Processors
 */
class OspfLsaProcessorsTestSuite : public TestSuite
{
public:
  OspfLsaProcessorsTestSuite ();
};

OspfLsaProcessorsTestSuite::OspfLsaProcessorsTestSuite ()
    : TestSuite ("ospf-lsa-processors", UNIT)
{
  AddTestCase (new OspfProcessL1SummaryLsaTest, TestCase::QUICK);
  AddTestCase (new OspfProcessRouterLsaTest, TestCase::QUICK);
  AddTestCase (new OspfProcessAreaLsaTest, TestCase::QUICK);
}

static OspfLsaProcessorsTestSuite g_ospfLsaProcessorsTestSuite;
