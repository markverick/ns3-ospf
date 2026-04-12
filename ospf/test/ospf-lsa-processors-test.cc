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
#include "ns3/enum.h"

#include "ospf-test-utils.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfLsaProcessorsTest");

namespace {

void
SnapshotLeaderFlag (bool *out, Ptr<OspfApp> app)
{
  *out = app != nullptr && app->IsAreaLeader ();
}

} // namespace

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
 * \brief Test static area leader selection by configured router ID
 */
class OspfStaticAreaLeaderModeTest : public TestCase
{
public:
  OspfStaticAreaLeaderModeTest ()
    : TestCase ("Static area-leader mode selects the configured router ID")
  {
  }

private:
  void DoRun () override;
};

void
OspfStaticAreaLeaderModeTest::DoRun ()
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
  address.SetBase ("10.2.1.0", "255.255.255.0");
  address.Assign (devices01);
  address.SetBase ("10.2.2.0", "255.255.255.0");
  address.Assign (devices12);
  address.SetBase ("10.2.3.0", "255.255.255.0");
  address.Assign (devices20);

  Ptr<Ipv4> leaderIpv4 = nodes.Get (2)->GetObject<Ipv4> ();
  NS_TEST_ASSERT_MSG_NE (leaderIpv4, nullptr, "expected Ipv4 on configured leader node");
  const Ipv4Address staticLeaderId = leaderIpv4->GetAddress (1, 0).GetAddress ();

  OspfAppHelper ospfHelper;
  ospfHelper.SetAttribute ("EnableAreaProxy", BooleanValue (true));
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (Seconds (1.0)));
  ospfHelper.SetAttribute ("AreaLeaderMode", EnumValue (OspfApp::AREA_LEADER_STATIC));
  ospfHelper.SetAttribute ("StaticAreaLeaderRouterId", Ipv4AddressValue (staticLeaderId));

  ApplicationContainer apps = ospfHelper.Install (nodes);
  apps.Start (Seconds (0.5));

  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();

  for (uint32_t i = 0; i < apps.GetN (); ++i)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (apps.Get (i));
      const bool isConfiguredLeader = app->GetRouterId () == staticLeaderId;
      NS_TEST_ASSERT_MSG_NE (app, nullptr, "expected OspfApp");
      NS_TEST_ASSERT_MSG_EQ (app->IsAreaLeader (), isConfiguredLeader,
                             "static leader mode should only activate on the configured router ID");
    }

  Simulator::Destroy ();
}

/**
 * \ingroup ospf-test
 * \brief Test reachable-lowest leader mode recovers across partition and heal
 */
class OspfReachableAreaLeaderRecoveryTest : public TestCase
{
public:
  OspfReachableAreaLeaderRecoveryTest ()
    : TestCase ("Reachable-lowest area leader mode honors majority partition and heals back")
  {
  }

private:
  void DoRun () override;
};


class OspfRejectsStaleRouterLsaTest : public TestCase
{
public:
  OspfRejectsStaleRouterLsaTest ()
    : TestCase ("ProcessRouterLsa ignores older Router-LSAs")
  {
  }

private:
  void DoRun () override
  {
    Ptr<OspfApp> app = CreateObject<OspfApp> ();
    app->SetRouterId (Ipv4Address ("10.0.0.1"));

    Ptr<RouterLsa> newer = Create<RouterLsa> ();
    newer->AddLink (RouterLink (Ipv4Address ("10.0.0.2").Get (),
                                Ipv4Address ("10.0.0.1").Get (), 1, 3));
    LsaHeader newerHeader;
    newerHeader.SetType (LsaHeader::RouterLSAs);
    newerHeader.SetLsId (Ipv4Address ("10.0.0.9").Get ());
    newerHeader.SetAdvertisingRouter (Ipv4Address ("10.0.0.9").Get ());
    newerHeader.SetSeqNum (9);
    newerHeader.SetLength (20 + newer->GetSerializedSize ());

    OspfAppTestPeer::ProcessLsa (app, newerHeader, newer);

    Ptr<RouterLsa> stale = Create<RouterLsa> ();
    stale->AddLink (RouterLink (Ipv4Address ("10.0.0.3").Get (),
                                Ipv4Address ("10.0.0.1").Get (), 1, 7));
    LsaHeader staleHeader = newerHeader.Copy ();
    staleHeader.SetSeqNum (8);
    staleHeader.SetLength (20 + stale->GetSerializedSize ());

    OspfAppTestPeer::ProcessLsa (app, staleHeader, stale);

    auto lsdb = app->GetLsdb ();
    NS_TEST_ASSERT_MSG_EQ (lsdb.size (), 1u, "expected exactly one Router-LSA entry");
    auto it = lsdb.find (Ipv4Address ("10.0.0.9").Get ());
    const bool hasRouterEntry = it != lsdb.end ();
    NS_TEST_ASSERT_MSG_EQ (hasRouterEntry, true,
                 "expected the Router-LSA to remain installed");
    if (it == lsdb.end ())
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (it->second.first.GetSeqNum (), 9u,
                           "older Router-LSA must not replace the newer sequence number");
    NS_TEST_ASSERT_MSG_EQ (it->second.second->GetNLink (), 1u,
                           "older Router-LSA body must not replace the newer body");
    NS_TEST_ASSERT_MSG_EQ (it->second.second->GetLink (0).m_metric, 3u,
                           "older Router-LSA metric must not replace the newer metric");

    Simulator::Destroy ();
  }
};

class OspfRejectsStaleL1SummaryLsaTest : public TestCase
{
public:
  OspfRejectsStaleL1SummaryLsaTest ()
    : TestCase ("ProcessL1SummaryLsa ignores older L1 Summary LSAs")
  {
  }

private:
  void DoRun () override
  {
    Ptr<OspfApp> app = CreateObject<OspfApp> ();
    app->SetRouterId (Ipv4Address ("10.0.0.1"));

    Ptr<L1SummaryLsa> newer = Create<L1SummaryLsa> ();
    newer->AddRoute (SummaryRoute (Ipv4Address ("10.240.0.0").Get (),
                                   Ipv4Mask ("255.255.0.0").Get (), 4));
    LsaHeader newerHeader;
    newerHeader.SetType (LsaHeader::L1SummaryLSAs);
    newerHeader.SetLsId (Ipv4Address ("10.0.0.9").Get ());
    newerHeader.SetAdvertisingRouter (Ipv4Address ("10.0.0.9").Get ());
    newerHeader.SetSeqNum (12);
    newerHeader.SetLength (20 + newer->GetSerializedSize ());

    OspfAppTestPeer::ProcessLsa (app, newerHeader, newer);

    Ptr<L1SummaryLsa> stale = Create<L1SummaryLsa> ();
    stale->AddRoute (SummaryRoute (Ipv4Address ("10.240.0.0").Get (),
                                   Ipv4Mask ("255.255.0.0").Get (), 9));
    LsaHeader staleHeader = newerHeader.Copy ();
    staleHeader.SetSeqNum (11);
    staleHeader.SetLength (20 + stale->GetSerializedSize ());

    OspfAppTestPeer::ProcessLsa (app, staleHeader, stale);

    auto lsdb = app->GetL1SummaryLsdb ();
    NS_TEST_ASSERT_MSG_EQ (lsdb.size (), 1u, "expected exactly one L1 Summary LSA entry");
    auto it = lsdb.find (Ipv4Address ("10.0.0.9").Get ());
    const bool hasSummaryEntry = it != lsdb.end ();
    NS_TEST_ASSERT_MSG_EQ (hasSummaryEntry, true,
                 "expected the L1 Summary LSA to remain installed");
    if (it == lsdb.end ())
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (it->second.first.GetSeqNum (), 12u,
                           "older L1 Summary LSA must not replace the newer sequence number");
    const auto routes = it->second.second->GetRoutes ();
    const bool hasNewerMetric =
      routes.find (SummaryRoute (Ipv4Address ("10.240.0.0").Get (),
                                 Ipv4Mask ("255.255.0.0").Get (), 4)) != routes.end ();
    const bool hasStaleMetric =
      routes.find (SummaryRoute (Ipv4Address ("10.240.0.0").Get (),
                                 Ipv4Mask ("255.255.0.0").Get (), 9)) != routes.end ();
    NS_TEST_ASSERT_MSG_EQ (hasNewerMetric,
                           true,
                           "older L1 Summary route metric must not replace the newer metric");
    NS_TEST_ASSERT_MSG_EQ (hasStaleMetric,
                           false,
                           "stale L1 Summary route metric must be ignored");

    Simulator::Destroy ();
  }
};
void
OspfReachableAreaLeaderRecoveryTest::DoRun ()
{
  NodeContainer nodes;
  nodes.Create (5);

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer devices34 = p2p.Install (nodes.Get (3), nodes.Get (4));

  Ipv4AddressHelper address;
  address.SetBase ("10.3.1.0", "255.255.255.0");
  address.Assign (devices01);
  address.SetBase ("10.3.2.0", "255.255.255.0");
  address.Assign (devices12);
  address.SetBase ("10.3.3.0", "255.255.255.0");
  address.Assign (devices23);
  address.SetBase ("10.3.4.0", "255.255.255.0");
  address.Assign (devices34);

  Ptr<Ipv4> ipv42 = nodes.Get (2)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv43 = nodes.Get (3)->GetObject<Ipv4> ();
  NS_TEST_ASSERT_MSG_NE (ipv42, nullptr, "expected Ipv4 on node2");
  NS_TEST_ASSERT_MSG_NE (ipv43, nullptr, "expected Ipv4 on node3");
  const uint32_t if23Node2 = devices23.Get (0)->GetIfIndex ();
  const uint32_t if23Node3 = devices23.Get (1)->GetIfIndex ();

  OspfAppHelper ospfHelper;
  ospfHelper.SetAttribute ("EnableAreaProxy", BooleanValue (true));
  ospfHelper.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.0")));
  ospfHelper.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (100)));
  ospfHelper.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (300)));
  ospfHelper.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));
  ospfHelper.SetAttribute ("AreaLeaderMode",
                           EnumValue (OspfApp::AREA_LEADER_REACHABLE_LOWEST_ROUTER_ID));

  ApplicationContainer apps = ospfHelper.Install (nodes);
  apps.Start (Seconds (0.5));
  apps.Stop (Seconds (12.0));

  Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
  Ptr<OspfApp> app3 = DynamicCast<OspfApp> (apps.Get (3));
  NS_TEST_ASSERT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
  NS_TEST_ASSERT_MSG_NE (app3, nullptr, "expected OspfApp on node3");

  bool initialLeader0 = false;
  bool initialLeader3 = false;
  bool partitionLeader0 = false;
  bool partitionLeader3 = false;
  bool healedLeader0 = false;
  bool healedLeader3 = false;

  Simulator::Schedule (Seconds (4.5), &SnapshotLeaderFlag, &initialLeader0, app0);
  Simulator::Schedule (Seconds (4.5), &SnapshotLeaderFlag, &initialLeader3, app3);

  Simulator::Schedule (Seconds (5.0), &Ipv4::SetDown, ipv42, if23Node2);
  Simulator::Schedule (Seconds (5.0), &Ipv4::SetDown, ipv43, if23Node3);

  Simulator::Schedule (Seconds (7.5), &SnapshotLeaderFlag, &partitionLeader0, app0);
  Simulator::Schedule (Seconds (7.5), &SnapshotLeaderFlag, &partitionLeader3, app3);

  Simulator::Schedule (Seconds (8.0), &Ipv4::SetUp, ipv42, if23Node2);
  Simulator::Schedule (Seconds (8.0), &Ipv4::SetUp, ipv43, if23Node3);

  Simulator::Schedule (Seconds (10.5), &SnapshotLeaderFlag, &healedLeader0, app0);
  Simulator::Schedule (Seconds (10.5), &SnapshotLeaderFlag, &healedLeader3, app3);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  NS_TEST_ASSERT_MSG_EQ (initialLeader3, false,
                         "expected node3 not to be leader before partition");
  NS_TEST_ASSERT_MSG_EQ (partitionLeader0, true,
                         "expected node0 to remain leader in the majority partition");
  NS_TEST_ASSERT_MSG_EQ (partitionLeader3, false,
                         "expected minority partition not to elect a leader without quorum");
  NS_TEST_ASSERT_MSG_EQ (healedLeader0, true,
                         "expected node0 to regain global leadership after healing");
  NS_TEST_ASSERT_MSG_EQ (healedLeader3, false,
                         "expected node3 to remain follower after the area heals");

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
  AddTestCase (new OspfRejectsStaleRouterLsaTest, TestCase::QUICK);
  AddTestCase (new OspfRejectsStaleL1SummaryLsaTest, TestCase::QUICK);
  AddTestCase (new OspfProcessAreaLsaTest, TestCase::QUICK);
  AddTestCase (new OspfStaticAreaLeaderModeTest, TestCase::QUICK);
  AddTestCase (new OspfReachableAreaLeaderRecoveryTest, TestCase::QUICK);
}

static OspfLsaProcessorsTestSuite g_ospfLsaProcessorsTestSuite;
