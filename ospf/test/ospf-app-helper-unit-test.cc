/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/lsa-header.h"
#include "ns3/l1-summary-lsa.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

#include <set>
#include <tuple>

namespace ns3 {

namespace {

bool
HasSummaryRoute (const std::set<SummaryRoute> &routes, Ipv4Address address, Ipv4Mask mask,
                 uint32_t metric)
{
  return routes.find (SummaryRoute (address.Get (), mask.Get (), metric)) != routes.end ();
}

Ptr<L1SummaryLsa>
FetchSelfL1Summary (const Ptr<OspfApp> &app)
{
  const auto key =
      std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, app->GetRouterId ().Get (),
                       app->GetRouterId ().Get ());
  auto pair = app->FetchLsa (key);
  if (pair.second == nullptr)
    {
      return nullptr;
    }
  return DynamicCast<L1SummaryLsa> (pair.second);
}

} // namespace

class OspfConfigureReachablePrefixesHelperTestCase : public TestCase
{
public:
  OspfConfigureReachablePrefixesHelperTestCase ()
    : TestCase ("OspfAppHelper::ConfigureReachablePrefixesFromInterfaces advertises connected p2p networks")
  {
  }

  void
  DoRun () override
  {
    NodeContainer nodes;
    nodes.Create (2);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.252");
    ipv4.Assign (d01);

    OspfAppHelper ospf;
    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    Ptr<L1SummaryLsa> l10 = FetchSelfL1Summary (app0);
    Ptr<L1SummaryLsa> l11 = FetchSelfL1Summary (app1);
    NS_TEST_EXPECT_MSG_NE (l10, nullptr, "expected node0 to have a self-originated L1SummaryLSA");
    NS_TEST_EXPECT_MSG_NE (l11, nullptr, "expected node1 to have a self-originated L1SummaryLSA");

    const Ipv4Address net ("10.1.1.0");
    const Ipv4Mask mask ("255.255.255.252");

    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l10->GetRoutes (), net, mask, 1), true,
                           "node0 should advertise 10.1.1.0/30 with metric 1");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (), net, mask, 1), true,
                           "node1 should advertise 10.1.1.0/30 with metric 1");

    Simulator::Destroy ();
  }
};

class OspfPreloadSeedsL1SummaryTestCase : public TestCase
{
public:
  OspfPreloadSeedsL1SummaryTestCase ()
    : TestCase ("OspfAppHelper::Preload seeds L1SummaryLSA routes for connected p2p networks")
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
    ipv4.SetBase ("10.1.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.1.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ApplicationContainer apps = ospf.Install (nodes);

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    Ptr<OspfApp> app2 = DynamicCast<OspfApp> (apps.Get (2));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");
    NS_TEST_EXPECT_MSG_NE (app2, nullptr, "expected OspfApp on node2");

    ospf.Preload (nodes);

    Ptr<L1SummaryLsa> l10 = FetchSelfL1Summary (app0);
    Ptr<L1SummaryLsa> l11 = FetchSelfL1Summary (app1);
    Ptr<L1SummaryLsa> l12 = FetchSelfL1Summary (app2);

    NS_TEST_EXPECT_MSG_NE (l10, nullptr, "expected node0 self-originated L1SummaryLSA after Preload");
    NS_TEST_EXPECT_MSG_NE (l11, nullptr, "expected node1 self-originated L1SummaryLSA after Preload");
    NS_TEST_EXPECT_MSG_NE (l12, nullptr, "expected node2 self-originated L1SummaryLSA after Preload");

    const Ipv4Mask mask ("255.255.255.252");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l10->GetRoutes (), Ipv4Address ("10.1.1.0"), mask, 1),
                           true, "node0 should advertise 10.1.1.0/30");

    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (), Ipv4Address ("10.1.1.0"), mask, 1),
                           true, "node1 should advertise 10.1.1.0/30");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (), Ipv4Address ("10.1.2.0"), mask, 1),
                           true, "node1 should advertise 10.1.2.0/30");

    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l12->GetRoutes (), Ipv4Address ("10.1.2.0"), mask, 1),
                           true, "node2 should advertise 10.1.2.0/30");

    Simulator::Destroy ();
  }
};

class OspfAppHelperUnitTestSuite : public TestSuite
{
public:
  OspfAppHelperUnitTestSuite ()
    : TestSuite ("ospf-app-helper", UNIT)
  {
    AddTestCase (new OspfConfigureReachablePrefixesHelperTestCase (), TestCase::QUICK);
    AddTestCase (new OspfPreloadSeedsL1SummaryTestCase (), TestCase::QUICK);
  }
};

static OspfAppHelperUnitTestSuite g_ospfAppHelperUnitTestSuite;

} // namespace ns3
