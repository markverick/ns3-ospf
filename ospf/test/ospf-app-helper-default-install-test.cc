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

class OspfHelperDefaultInstallIncludesMultiAccessTestCase : public TestCase
{
public:
  OspfHelperDefaultInstallIncludesMultiAccessTestCase ()
    : TestCase ("OspfAppHelper IPv4-bound install includes non-p2p interfaces already registered with Ipv4")
  {
  }

  void
  DoRun () override
  {
    NodeContainer nodes;
    nodes.Create (2);

    InternetStackHelper internet;
    internet.Install (nodes);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (5000)));
    NetDeviceContainer devices = csma.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.3.1.0", "255.255.255.0");
    ipv4.Assign (devices);

    OspfAppHelper ospf;
    ApplicationContainer apps =
      ospf.Install (nodes);
    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    const uint32_t if0 = ipv40->GetInterfaceForDevice (devices.Get (0));
    const uint32_t if1 = ipv41->GetInterfaceForDevice (devices.Get (1));

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, if0), true,
                           "default install should include multi-access interfaces");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app1, if1), true,
                           "default install should include multi-access interfaces");

    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    Ptr<L1SummaryLsa> l10 = FetchSelfL1Summary (app0);
    Ptr<L1SummaryLsa> l11 = FetchSelfL1Summary (app1);
    NS_TEST_EXPECT_MSG_NE (l10, nullptr, "expected node0 self-originated L1SummaryLSA");
    NS_TEST_EXPECT_MSG_NE (l11, nullptr, "expected node1 self-originated L1SummaryLSA");

    const Ipv4Address network ("10.3.1.0");
    const Ipv4Mask mask ("255.255.255.0");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l10->GetRoutes (), network, mask, 1), true,
                           "node0 should advertise the multi-access subnet after default install");
    NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (), network, mask, 1), true,
                           "node1 should advertise the multi-access subnet after default install");

    Simulator::Destroy ();
  }
};

class OspfHelperDefaultInstallSkipsUnregisteredDevicesTestCase : public TestCase
{
public:
  OspfHelperDefaultInstallSkipsUnregisteredDevicesTestCase ()
    : TestCase ("OspfAppHelper IPv4-bound install skips node devices that are not registered with Ipv4")
  {
  }

  void
  DoRun () override
  {
    NodeContainer nodes;
    nodes.Create (2);

    InternetStackHelper internet;
    internet.Install (nodes);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (5000)));
    NetDeviceContainer devices = csma.Install (nodes);

    Ipv4AddressHelper ipv4Address;
    ipv4Address.SetBase ("10.3.2.0", "255.255.255.0");
    ipv4Address.Assign (devices);

    Ptr<SimpleNetDevice> stray = CreateObject<SimpleNetDevice> ();
    nodes.Get (0)->AddDevice (stray);
    Ptr<SimpleChannel> strayChannel = CreateObject<SimpleChannel> ();
    stray->SetChannel (strayChannel);

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    NS_TEST_EXPECT_MSG_NE (ipv40, nullptr, "expected Ipv4 on node0");

    const uint32_t beforeInterfaces = ipv40->GetNInterfaces ();
    NS_TEST_EXPECT_MSG_EQ (ipv40->GetInterfaceForDevice (stray), -1,
                           "stray device should not be registered with node0 IPv4 before install");

    OspfAppHelper ospf;
    ApplicationContainer apps =
      ospf.Install (nodes.Get (0));
    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");

    const uint32_t csmaIf0 = ipv40->GetInterfaceForDevice (devices.Get (0));
    NS_TEST_EXPECT_MSG_EQ (ipv40->GetNInterfaces (), beforeInterfaces,
                           "default install must not register stray node devices with IPv4");
    NS_TEST_EXPECT_MSG_EQ (ipv40->GetInterfaceForDevice (stray), -1,
                           "default install must not mutate IPv4 by binding stray node devices");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, csmaIf0), true,
                           "default install should still bind the real IPv4-backed interface");

    Simulator::Destroy ();
  }
};

class OspfHelperDefaultInstallDoesNotBootstrapLateInterfacesTestCase : public TestCase
{
public:
  OspfHelperDefaultInstallDoesNotBootstrapLateInterfacesTestCase ()
    : TestCase ("OspfAppHelper default install binds only interfaces present in IPv4 at install time")
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
    NetDeviceContainer devices = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));

    Ptr<Ipv4> ipv40 = nodes.Get (0)->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    NS_TEST_EXPECT_MSG_NE (ipv40, nullptr, "expected Ipv4 on node0");
    NS_TEST_EXPECT_MSG_NE (ipv41, nullptr, "expected Ipv4 on node1");

    const int32_t beforeIf0 = ipv40->GetInterfaceForDevice (devices.Get (0));
    const int32_t beforeIf1 = ipv41->GetInterfaceForDevice (devices.Get (1));
    NS_TEST_EXPECT_MSG_EQ (beforeIf0, -1,
                           "test requires the node0 link to be unregistered before default install");
    NS_TEST_EXPECT_MSG_EQ (beforeIf1, -1,
                           "test requires the node1 link to be unregistered before default install");

    OspfAppHelper ospf;
    ApplicationContainer apps = ospf.Install (nodes);
    Ptr<OspfApp> app0 = DynamicCast<OspfApp> (apps.Get (0));
    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_EXPECT_MSG_NE (app0, nullptr, "expected OspfApp on node0");
    NS_TEST_EXPECT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    const int32_t if0 = ipv40->GetInterfaceForDevice (devices.Get (0));
    const int32_t if1 = ipv41->GetInterfaceForDevice (devices.Get (1));
    NS_TEST_EXPECT_MSG_EQ (if0, -1,
                           "default install must not register the node0 link with IPv4");
    NS_TEST_EXPECT_MSG_EQ (if1, -1,
                           "default install must not register the node1 link with IPv4");

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.3.3.0", "255.255.255.252");
    ipv4.Assign (devices);

    const uint32_t afterIf0 = static_cast<uint32_t> (ipv40->GetInterfaceForDevice (devices.Get (0)));
    const uint32_t afterIf1 = static_cast<uint32_t> (ipv41->GetInterfaceForDevice (devices.Get (1)));

    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);

    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app0, afterIf0), false,
                           "default install must not auto-select interfaces that appear in IPv4 later");
    NS_TEST_EXPECT_MSG_EQ (OspfAppTestPeer::HasOspfInterface (app1, afterIf1), false,
                           "default install must not auto-select interfaces that appear in IPv4 later");

    Ptr<L1SummaryLsa> l10 = FetchSelfL1Summary (app0);
    Ptr<L1SummaryLsa> l11 = FetchSelfL1Summary (app1);
    const Ipv4Address network ("10.3.3.0");
    const Ipv4Mask mask ("255.255.255.252");
    if (l10 != nullptr)
      {
        NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l10->GetRoutes (), network, mask, 1),
                               false,
                               "node0 should not advertise a subnet that was absent from IPv4 at install time");
      }
    if (l11 != nullptr)
      {
        NS_TEST_EXPECT_MSG_EQ (HasSummaryRoute (l11->GetRoutes (), network, mask, 1),
                               false,
                               "node1 should not advertise a subnet that was absent from IPv4 at install time");
      }

    Simulator::Destroy ();
  }
};

class OspfAppHelperDefaultInstallTestSuite : public TestSuite
{
public:
  OspfAppHelperDefaultInstallTestSuite ()
    : TestSuite ("ospf-app-helper-default-install", UNIT)
  {
    AddTestCase (new OspfHelperDefaultInstallIncludesMultiAccessTestCase (), TestCase::QUICK);
    AddTestCase (new OspfHelperDefaultInstallSkipsUnregisteredDevicesTestCase (), TestCase::QUICK);
    AddTestCase (new OspfHelperDefaultInstallDoesNotBootstrapLateInterfacesTestCase (),
                 TestCase::QUICK);
  }
};

static OspfAppHelperDefaultInstallTestSuite g_ospfAppHelperDefaultInstallTestSuite;

} // namespace

} // namespace ns3