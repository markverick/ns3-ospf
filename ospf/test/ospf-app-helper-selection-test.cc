/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"

#include "ospf-app-helper-test-utils.h"
#include "ospf-test-utils.h"

namespace ns3 {

namespace {

using ospf_app_helper_test_utils::FetchSelfL1Summary;
using ospf_app_helper_test_utils::HasSummaryRoute;

class OspfHelperExplicitInterfaceInstallTestCase : public TestCase
{
public:
  OspfHelperExplicitInterfaceInstallTestCase ()
    : TestCase ("OspfAppHelper explicit install binds only selected interfaces and ignores loopback")
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

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (5000)));
    NetDeviceContainer d12 = csma.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.2.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.2.2.0", "255.255.255.0");
    ipv4.Assign (d12);

    OspfAppHelper ospf;

    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    const uint32_t d01If0 = ipv40->GetInterfaceForDevice (d01.Get (0));
    const uint32_t d01If1 = ipv41->GetInterfaceForDevice (d01.Get (1));
    const uint32_t d12If1 = ipv41->GetInterfaceForDevice (d12.Get (0));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, d01If0), true,
                           "node0 should bind its selected p2p interface");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app1, d01If1), true,
                           "node1 should bind its selected p2p interface");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app1, d12If1), false,
                           "node1 should not bind the unselected csma interface");
    NS_TEST_EXPECT_MSG_EQ (app1->GetRouterId (), Ipv4Address ("10.2.1.2"),
                           "router id should come from the first selected non-loopback interface");

    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    Ptr<L1SummaryLsa> l10 = FetchSelfL1Summary (app0);
    Ptr<L1SummaryLsa> l11 = FetchSelfL1Summary (app1);
    NS_TEST_EXPECT_MSG_NE (l10, nullptr, "expected node0 self-originated L1SummaryLSA");
    NS_TEST_EXPECT_MSG_NE (l11, nullptr, "expected node1 self-originated L1SummaryLSA");

    const Ipv4Mask p2pMask ("255.255.255.252");
    const Ipv4Mask csmaMask ("255.255.255.0");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l10->GetRoutes (), Ipv4Address ("10.2.1.0"), p2pMask, 1),
                           true, "node0 should advertise only the selected p2p subnet");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (), Ipv4Address ("10.2.1.0"), p2pMask, 1),
                           true, "node1 should advertise the selected p2p subnet");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (), Ipv4Address ("10.2.2.0"), csmaMask, 1),
                           false, "node1 should not advertise an unselected interface subnet");

    Simulator::Destroy ();
  }
};

class OspfHelperExplicitInstallUsesOnlyRegisteredSelectionTestCase : public TestCase
{
public:
  OspfHelperExplicitInstallUsesOnlyRegisteredSelectionTestCase ()
    : TestCase ("OspfAppHelper explicit install binds only registered selected interfaces and does not borrow router ids from excluded interfaces")
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
    NetDeviceContainer d02 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.6.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.6.2.0", "255.255.255.252");
    ipv4.Assign (d02);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    const uint32_t d01If0 = ipv40->GetInterfaceForDevice (d01.Get (0));
    const uint32_t d02If0 = ipv40->GetInterfaceForDevice (d02.Get (0));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, d01If0), true,
                           "node0 should bind the selected registered interface");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, d02If0), false,
                           "node0 should exclude the addressed but unselected interface");
    NS_TEST_EXPECT_MSG_EQ (app0->GetRouterId (), Ipv4Address ("10.6.1.1"),
                           "router id should come from the selected registered interface");
    NS_TEST_EXPECT_MSG_EQ (app1->GetRouterId (), Ipv4Address ("10.6.1.2"),
                           "peer router id should also come from its selected registered interface");

    Simulator::Destroy ();
  }
};

class OspfHelperExplicitRebindUpdatesRouterIdTestCase : public TestCase
{
public:
  OspfHelperExplicitRebindUpdatesRouterIdTestCase ()
    : TestCase ("OspfAppHelper explicit rebind updates router id to the newly selected interface")
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
    NetDeviceContainer d02 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.7.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.7.2.0", "255.255.255.252");
    ipv4.Assign (d02);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    const uint32_t d01If0 = ipv40->GetInterfaceForDevice (d01.Get (0));
    const uint32_t d02If0 = ipv40->GetInterfaceForDevice (d02.Get (0));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, d01If0), true,
                           "node0 should initially select the first point-to-point interface");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, d02If0), false,
                           "node0 should initially exclude the second point-to-point interface");
    NS_TEST_EXPECT_MSG_EQ (app0->GetRouterId (), Ipv4Address ("10.7.1.1"),
                           "initial router id should come from the initially selected interface");

    app0->SetBoundNetDevices (NetDeviceContainer (d02.Get (0)));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, d01If0), false,
                           "node0 should unselect the first point-to-point interface after rebind");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, d02If0), true,
                           "node0 should select the second point-to-point interface after rebind");
    NS_TEST_EXPECT_MSG_EQ (app0->GetRouterId (), Ipv4Address ("10.7.2.1"),
                           "router id should move to the newly selected interface after rebind");

    NodeContainer reboundNodes;
    reboundNodes.Add (nodes.Get (0));
    ospf.ConfigureReachablePrefixesFromInterfaces (reboundNodes);

    const auto l1SummaryLsdb = app0->GetL1SummaryLsdb ();
    NS_TEST_EXPECT_MSG_EQ (l1SummaryLsdb.count (Ipv4Address ("10.7.1.1").Get ()), 0u,
                 "rebind must discard self L1SummaryLSAs keyed by the old router id");
    NS_TEST_EXPECT_MSG_EQ (l1SummaryLsdb.count (Ipv4Address ("10.7.2.1").Get ()), 1u,
                 "rebind must retain only the self L1SummaryLSA keyed by the new router id");

    Ptr<L1SummaryLsa> l10 = FetchSelfL1Summary (app0);
    NS_TEST_EXPECT_MSG_NE (l10, nullptr, "expected node0 self-originated L1SummaryLSA after rebind");
    if (l10 != nullptr)
      {
        const Ipv4Mask mask ("255.255.255.252");
        NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l10->GetRoutes (), Ipv4Address ("10.7.1.0"), mask, 1),
                               false,
                               "node0 should stop advertising the deselected subnet after rebind");
        NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l10->GetRoutes (), Ipv4Address ("10.7.2.0"), mask, 1),
                               true,
                               "node0 should advertise the newly selected subnet after rebind");
      }

    Simulator::Destroy ();
  }
};

class OspfHelperExplicitRebindUsesRemoteSelectedGatewayTestCase : public TestCase
{
public:
  OspfHelperExplicitRebindUsesRemoteSelectedGatewayTestCase ()
    : TestCase ("OspfAppHelper explicit rebind uses the remote selected interface address as the p2p gateway")
  {
  }

  void
  DoRun () override
  {
    NodeContainer nodes;
    nodes.Create (4);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));
    NetDeviceContainer d02 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (2)));
    NetDeviceContainer d13 = p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (3)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.8.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if01 = ipv4.Assign (d01);
    ipv4.SetBase ("10.8.2.0", "255.255.255.252");
    Ipv4InterfaceContainer if02 = ipv4.Assign (d02);
    ipv4.SetBase ("10.8.3.0", "255.255.255.252");
    Ipv4InterfaceContainer if13 = ipv4.Assign (d13);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    app0->SetBoundNetDevices (NetDeviceContainer (d01.Get (0)));
    app1->SetBoundNetDevices (NetDeviceContainer (d01.Get (1)));

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    const uint32_t d01If0 = ipv40->GetInterfaceForDevice (d01.Get (0));
    const uint32_t d01If1 = ipv41->GetInterfaceForDevice (d01.Get (1));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::GetInterfaceGateway (app0, d01If0), if01.GetAddress (1),
                           "node0 should use node1's selected d01 address as its p2p gateway");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::GetInterfaceGateway (app1, d01If1), if01.GetAddress (0),
                           "node1 should use node0's selected d01 address as its p2p gateway");
    NS_TEST_EXPECT_MSG_NE (OspfAppTestPeer::GetInterfaceGateway (app0, d01If0), if13.GetAddress (0),
                           "node0 must not use node1's unrelated addressed interface as its p2p gateway");
    NS_TEST_EXPECT_MSG_NE (OspfAppTestPeer::GetInterfaceGateway (app1, d01If1), if02.GetAddress (0),
                           "node1 must not use node0's unrelated addressed interface as its p2p gateway");

    Simulator::Destroy ();
  }
};

class OspfHelperExcludedInterfacePrefixCannotBeForcedRoutableTestCase : public TestCase
{
public:
  OspfHelperExcludedInterfacePrefixCannotBeForcedRoutableTestCase ()
    : TestCase ("OspfApp ignores routable-prefix requests on interfaces excluded from explicit selection")
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

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (5000)));
    NetDeviceContainer d12 = csma.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.12.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.12.2.0", "255.255.255.0");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));

    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    const uint32_t d12If1 = ipv41->GetInterfaceForDevice (d12.Get (0));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app1, d12If1), false,
                           "node1 should exclude the csma interface from OSPF selection");

    app1->SetInterfacePrefixRoutable (d12If1, true);
    NS_TEST_EXPECT_MSG_EQ (app1->GetInterfacePrefixRoutable (d12If1), false,
                           "excluded interfaces must reject connected-prefix advertisement");

    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    Ptr<L1SummaryLsa> l11 = FetchSelfL1Summary (app1);
    NS_TEST_EXPECT_MSG_NE (l11, nullptr, "expected node1 self-originated L1SummaryLSA");
    if (l11 != nullptr)
      {
        NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (),
                                                Ipv4Address ("10.12.2.0"),
                                                Ipv4Mask ("255.255.255.0"),
                                                1),
                               false,
                               "excluded interface prefixes must not leak into advertisements");
      }

    Simulator::Destroy ();
  }
};

class OspfHelperExplicitMetricPersistsAcrossRefreshAndRebindTestCase : public TestCase
{
public:
  OspfHelperExplicitMetricPersistsAcrossRefreshAndRebindTestCase ()
    : TestCase ("OspfApp keeps configured interface metrics across refresh and explicit rebind")
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
    NetDeviceContainer d02 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.13.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.13.2.0", "255.255.255.252");
    ipv4.Assign (d02);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));

    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    const uint32_t d01If0 = ipv40->GetInterfaceForDevice (d01.Get (0));
    const uint32_t d02If0 = ipv40->GetInterfaceForDevice (d02.Get (0));

    std::vector<uint32_t> metrics (ipv40->GetNInterfaces (), 1);
    metrics[d01If0] = 9;
    metrics[d02If0] = 4;
    app0->SetMetrices (metrics);

    NS_TEST_EXPECT_MSG_EQ (app0->GetMetric (d01If0), 9u,
                           "configured metric should be applied immediately to the selected interface");

    app0->SetInterfacePrefixRoutable (d01If0, true);
    NS_TEST_EXPECT_MSG_EQ (app0->GetMetric (d01If0), 9u,
                           "interface refresh must preserve the configured metric");

    app0->SetBoundNetDevices (NetDeviceContainer (d01.Get (0)));
    NS_TEST_EXPECT_MSG_EQ (app0->GetMetric (d01If0), 9u,
                           "explicit rebind must preserve the configured metric on the selected interface");

    Simulator::Destroy ();
  }
};

class OspfAppHelperSelectionTestSuite : public TestSuite
{
public:
  OspfAppHelperSelectionTestSuite ()
    : TestSuite ("ospf-app-helper-selection", UNIT)
  {
    AddTestCase (new OspfHelperExplicitInterfaceInstallTestCase (), TestCase::QUICK);
    AddTestCase (new OspfHelperExplicitInstallUsesOnlyRegisteredSelectionTestCase (),
           TestCase::QUICK);
    AddTestCase (new OspfHelperExplicitRebindUpdatesRouterIdTestCase (), TestCase::QUICK);
    AddTestCase (new OspfHelperExplicitRebindUsesRemoteSelectedGatewayTestCase (), TestCase::QUICK);
    AddTestCase (new OspfHelperExcludedInterfacePrefixCannotBeForcedRoutableTestCase (),
                 TestCase::QUICK);
    AddTestCase (new OspfHelperExplicitMetricPersistsAcrossRefreshAndRebindTestCase (),
                 TestCase::QUICK);
  }
};

static OspfAppHelperSelectionTestSuite g_ospfAppHelperSelectionTestSuite;

} // namespace

} // namespace ns3