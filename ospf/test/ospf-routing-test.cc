/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-route.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/boolean.h"

#include "ns3/ospf-app.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-routing.h"

#include "ospf-test-utils.h"

using namespace ns3;

namespace {

using ospf_test_utils::ConfigureFastColdStart;
using ospf_test_utils::FindStaticRoute;
using ospf_test_utils::GetOspfApp;
using ospf_test_utils::GetOspfRouting;
using ospf_test_utils::LookupOspfRoute;

struct ThreeNodeLineContext
{
  NodeContainer nodes;
  NetDeviceContainer d01;
  NetDeviceContainer d12;
  Ipv4InterfaceContainer if01;
  Ipv4InterfaceContainer if12;
  ApplicationContainer apps;
};

struct TwoAreaLineContext
{
  NodeContainer nodes;
  NetDeviceContainer d01;
  NetDeviceContainer d12;
  NetDeviceContainer d23;
  Ipv4InterfaceContainer if01;
  Ipv4InterfaceContainer if12;
  Ipv4InterfaceContainer if23;
  ApplicationContainer apps;
};

struct RouteInputState
{
  bool forwarded = false;
  bool localDelivered = false;
  bool errored = false;
  Ptr<Ipv4Route> route;
  uint32_t localInterface = std::numeric_limits<uint32_t>::max ();
  Socket::SocketErrno socketError = Socket::ERROR_NOTERROR;
};

static void
CaptureForward (RouteInputState *state, Ptr<Ipv4Route> route, Ptr<const Packet>, const Ipv4Header &)
{
  state->forwarded = true;
  state->route = route;
}

static void
CaptureLocal (RouteInputState *state, Ptr<const Packet>, const Ipv4Header &, uint32_t interface)
{
  state->localDelivered = true;
  state->localInterface = interface;
}

static void
CaptureError (RouteInputState *state, Ptr<const Packet>, const Ipv4Header &,
              Socket::SocketErrno socketError)
{
  state->errored = true;
  state->socketError = socketError;
}

static ThreeNodeLineContext
CreateThreeNodeLine (bool preload)
{
  ThreeNodeLineContext context;

  context.nodes.Create (3);

  InternetStackHelper internet;
  internet.Install (context.nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  context.d01 = p2p.Install (context.nodes.Get (0), context.nodes.Get (1));
  context.d12 = p2p.Install (context.nodes.Get (1), context.nodes.Get (2));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  context.if01 = address.Assign (context.d01);
  address.SetBase ("10.1.2.0", "255.255.255.252");
  context.if12 = address.Assign (context.d12);

  OspfAppHelper ospf;
  ConfigureFastColdStart (ospf);
  ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

  context.apps = ospf.Install (context.nodes);
  ospf.ConfigureReachablePrefixesFromInterfaces (context.nodes);
  if (preload)
    {
      ospf.Preload (context.nodes);
    }

  context.apps.Start (Seconds (0.0));
  context.apps.Stop (Seconds (5.0));
  return context;
}

static TwoAreaLineContext
CreateTwoAreaLine (bool preload)
{
  TwoAreaLineContext context;

  context.nodes.Create (4);

  InternetStackHelper internet;
  internet.Install (context.nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  context.d01 = p2p.Install (context.nodes.Get (0), context.nodes.Get (1));
  context.d12 = p2p.Install (context.nodes.Get (1), context.nodes.Get (2));
  context.d23 = p2p.Install (context.nodes.Get (2), context.nodes.Get (3));

  Ipv4AddressHelper address;
  address.SetBase ("10.63.1.0", "255.255.255.252");
  context.if01 = address.Assign (context.d01);
  address.SetBase ("10.63.2.0", "255.255.255.252");
  context.if12 = address.Assign (context.d12);
  address.SetBase ("10.63.3.0", "255.255.255.252");
  context.if23 = address.Assign (context.d23);

  OspfAppHelper ospf;
  ConfigureFastColdStart (ospf);
  ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
  ospf.SetAttribute ("EnableAreaProxy", BooleanValue (true));

  context.apps = ospf.Install (context.nodes);

  DynamicCast<OspfApp> (context.apps.Get (0))->SetArea (0);
  DynamicCast<OspfApp> (context.apps.Get (1))->SetArea (0);
  DynamicCast<OspfApp> (context.apps.Get (2))->SetArea (1);
  DynamicCast<OspfApp> (context.apps.Get (3))->SetArea (1);

  ospf.ConfigureReachablePrefixesFromInterfaces (context.nodes);
  if (preload)
    {
      ospf.Preload (context.nodes);
    }

  context.apps.Start (Seconds (0.0));
  context.apps.Stop (Seconds (6.0));
  return context;
}

} // namespace

NS_LOG_COMPONENT_DEFINE ("OspfRoutingTest");

class OspfRouteOutputLearnsRemotePrefixTest : public TestCase
{
public:
  OspfRouteOutputLearnsRemotePrefixTest ()
    : TestCase ("Two-step lookup resolves a remote intra-area prefix without flattening into static routing")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    const auto destination = context.if12.GetAddress (1);
    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), destination, nullptr, &sockerr);

    NS_TEST_ASSERT_MSG_NE (GetOspfRouting (context.nodes.Get (0)), nullptr,
                           "node0 should expose OspfRouting in the IPv4 stack");
    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "route lookup to node2 should succeed");
    NS_TEST_ASSERT_MSG_NE (route, nullptr, "route lookup to node2 should return a route");
    NS_TEST_ASSERT_MSG_EQ (route->GetDestination (), destination,
                           "resolved route should target the exact destination address");
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), context.if01.GetAddress (1),
                           "node0 should send remote traffic to node1 as the first hop");
    NS_TEST_ASSERT_MSG_EQ (route->GetOutputDevice (), context.d01.Get (0),
                           "node0 should emit remote traffic on the (0,1) interface");
    NS_TEST_ASSERT_MSG_EQ (route->GetSource (), context.if01.GetAddress (0),
                           "node0 should source the route from its outgoing interface address");

    const auto staticRoute =
        FindStaticRoute (context.nodes.Get (0), destination.CombineMask (Ipv4Mask ("255.255.255.252")),
                         Ipv4Mask ("255.255.255.252"));
    NS_TEST_ASSERT_MSG_EQ (staticRoute.has_value (), false,
                           "node0 must not have a flattened static route for the learned remote prefix");

    Simulator::Destroy ();
  }
};

class OspfRouteInputForwardsPacketTest : public TestCase
{
public:
  OspfRouteInputForwardsPacketTest ()
    : TestCase ("RouteInput forwards on the resolved interface and does not mis-classify transit traffic")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto routing = GetOspfRouting (context.nodes.Get (1));
    NS_TEST_ASSERT_MSG_NE (routing, nullptr, "node1 should expose OspfRouting in the IPv4 stack");

    Ipv4Header header;
    header.SetSource (context.if01.GetAddress (0));
    header.SetDestination (context.if12.GetAddress (1));

    RouteInputState state;
    const bool handled = routing->RouteInput (Create<Packet> (64), header, context.d01.Get (1),
                                              MakeBoundCallback (&CaptureForward, &state),
                                              Ipv4RoutingProtocol::MulticastForwardCallback (),
                                              MakeBoundCallback (&CaptureLocal, &state),
                                              MakeBoundCallback (&CaptureError, &state));

    NS_TEST_ASSERT_MSG_EQ (handled, true, "node1 should take responsibility for the transit packet");
    NS_TEST_ASSERT_MSG_EQ (state.forwarded, true,
                           "node1 should forward transit traffic toward node2");
    NS_TEST_ASSERT_MSG_EQ (state.localDelivered, false,
                           "node1 must not locally deliver transit traffic destined to node2");
    NS_TEST_ASSERT_MSG_EQ (state.errored, false,
                           "node1 should not emit an error while forwarding a reachable packet");
    NS_TEST_ASSERT_MSG_NE (state.route, nullptr, "forward callback should receive the chosen route");
    NS_TEST_ASSERT_MSG_EQ (state.route->GetOutputDevice (), context.d12.Get (0),
                           "node1 should forward the packet on the (1,2) interface");
    NS_TEST_ASSERT_MSG_EQ (state.route->GetGateway (), Ipv4Address::GetZero (),
                           "node1 should use its directly connected route to node2's subnet");
    NS_TEST_ASSERT_MSG_EQ (state.route->GetSource (), context.if12.GetAddress (0),
                           "node1 should source the forwarded packet from the egress interface address");

    Simulator::Destroy ();
  }
};

class OspfDisableStopsLookupTest : public TestCase
{
public:
  OspfDisableStopsLookupTest ()
    : TestCase ("Disabling OSPF stops two-step forwarding immediately and does not fall back to learned static routes")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    const auto destination = context.if12.GetAddress (1);
    auto app0 = GetOspfApp (context.nodes.Get (0));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "node0 should host an OspfApp");

    Socket::SocketErrno beforeErr = Socket::ERROR_NOTERROR;
    auto before = LookupOspfRoute (context.nodes.Get (0), destination, nullptr, &beforeErr);
    NS_TEST_ASSERT_MSG_NE (before, nullptr, "route should exist before disabling OSPF");
    NS_TEST_ASSERT_MSG_EQ (beforeErr, Socket::ERROR_NOTERROR,
                           "route lookup before disable should succeed");

    app0->Disable ();

    Socket::SocketErrno afterErr = Socket::ERROR_NOTERROR;
    auto after = LookupOspfRoute (context.nodes.Get (0), destination, nullptr, &afterErr);
    NS_TEST_ASSERT_MSG_EQ (after, nullptr,
                           "route lookup should fail immediately after disabling OSPF");
    NS_TEST_ASSERT_MSG_EQ (afterErr, Socket::ERROR_NOROUTETOHOST,
                           "disabled OSPF should report no route to host");

    const auto staticRoute =
        FindStaticRoute (context.nodes.Get (0), destination.CombineMask (Ipv4Mask ("255.255.255.252")),
                         Ipv4Mask ("255.255.255.252"));
    NS_TEST_ASSERT_MSG_EQ (staticRoute.has_value (), false,
                           "node0 must not silently fall back to a flattened static route after disable");

    Simulator::Destroy ();
  }
};

class OspfInterAreaRouteOutputUsesFirstBorderHopTest : public TestCase
{
public:
  OspfInterAreaRouteOutputUsesFirstBorderHopTest ()
    : TestCase ("Inter-area two-step lookup resolves a remote area through the first border-router hop")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateTwoAreaLine (true);

    Simulator::Stop (Seconds (1.5));
    Simulator::Run ();

    const auto destination = context.if23.GetAddress (1);
    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), destination, nullptr, &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "inter-area route lookup from node0 to node3 should succeed");
    NS_TEST_ASSERT_MSG_NE (route, nullptr, "inter-area route lookup should return a route");
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), context.if01.GetAddress (1),
                           "node0 should send inter-area traffic to node1 as the first border hop");
    NS_TEST_ASSERT_MSG_EQ (route->GetOutputDevice (), context.d01.Get (0),
                           "node0 should emit inter-area traffic on the (0,1) interface");
    NS_TEST_ASSERT_MSG_EQ (route->GetSource (), context.if01.GetAddress (0),
                           "node0 should source inter-area traffic from the outgoing interface address");

    const auto staticRoute =
        FindStaticRoute (context.nodes.Get (0), destination.CombineMask (Ipv4Mask ("255.255.255.252")),
                         Ipv4Mask ("255.255.255.252"));
    NS_TEST_ASSERT_MSG_EQ (staticRoute.has_value (), false,
                           "node0 must not have a flattened static route for the remote area prefix");

    Simulator::Destroy ();
  }
};

class OspfRouteOutputReturnsNoRouteWithWrongInterfaceTest : public TestCase
{
public:
  OspfRouteOutputReturnsNoRouteWithWrongInterfaceTest ()
    : TestCase ("RouteOutput respects the caller's output-interface constraint")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateTwoAreaLine (true);

    Simulator::Stop (Seconds (1.5));
    Simulator::Run ();

    auto routing = GetOspfRouting (context.nodes.Get (1));
    NS_TEST_ASSERT_MSG_NE (routing, nullptr, "node1 should expose OspfRouting in the IPv4 stack");

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    Ipv4Header header;
    header.SetDestination (context.if01.GetAddress (0));
    auto route = routing->RouteOutput (Create<Packet> (), header, context.d12.Get (0), sockerr);

    NS_TEST_ASSERT_MSG_EQ (route, nullptr,
                           "lookup constrained to the wrong interface must fail rather than ignore the socket binding");
    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOROUTETOHOST,
                           "wrong-interface lookup should report no route to host");

    Simulator::Destroy ();
  }
};

class OspfRoutingTestSuite : public TestSuite
{
public:
  OspfRoutingTestSuite ()
    : TestSuite ("ospf-routing", UNIT)
  {
    AddTestCase (new OspfRouteOutputLearnsRemotePrefixTest, TestCase::QUICK);
    AddTestCase (new OspfRouteInputForwardsPacketTest, TestCase::QUICK);
    AddTestCase (new OspfDisableStopsLookupTest, TestCase::QUICK);
    AddTestCase (new OspfInterAreaRouteOutputUsesFirstBorderHopTest, TestCase::QUICK);
    AddTestCase (new OspfRouteOutputReturnsNoRouteWithWrongInterfaceTest, TestCase::QUICK);
  }
};

static OspfRoutingTestSuite g_ospfRoutingTestSuite;
