/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/csma-helper.h"
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

#include <filesystem>

using namespace ns3;

namespace {

using ospf_test_utils::ConfigureFastColdStart;
using ospf_test_utils::FindStaticRoute;
using ospf_test_utils::GetOspfApp;
using ospf_test_utils::GetOspfRouting;
using ospf_test_utils::Ipv4ToString;
using ospf_test_utils::LookupOspfRoute;
using ospf_test_utils::ReadAll;

static bool
HasPrefixOwnerMetric (const std::string &table, const std::string &dst, const std::string &owner,
                      uint32_t metric)
{
  const auto token = "[" + owner + " metric=" + std::to_string (metric) + "]";
  std::istringstream iss (table);
  for (std::string line; std::getline (iss, line);)
    {
      const auto firstNonSpace = line.find_first_not_of (" \t");
      const auto trimmed = firstNonSpace == std::string::npos ? std::string () : line.substr (firstNonSpace);
      if (trimmed.rfind (dst, 0) == 0 && trimmed.find (token) != std::string::npos)
        {
          return true;
        }
    }
  return false;
}

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

struct RouterVsAreaTieContext
{
  NodeContainer nodes;
  NetDeviceContainer d01;
  NetDeviceContainer d02;
  NetDeviceContainer d23;
  Ipv4InterfaceContainer if01;
  Ipv4InterfaceContainer if02;
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
CreateThreeNodeLine (bool preload, bool autoSyncInterfaces = false,
                     bool advertiseInterfacePrefixes = true)
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
  ospf.SetAttribute ("AutoSyncInterfaces", BooleanValue (autoSyncInterfaces));

  context.apps = ospf.Install (context.nodes);
  if (advertiseInterfacePrefixes)
    {
      ospf.ConfigureReachablePrefixesFromInterfaces (context.nodes);
    }
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

static RouterVsAreaTieContext
CreateRouterVsAreaTieTopology (bool preload)
{
  RouterVsAreaTieContext context;

  context.nodes.Create (4);

  InternetStackHelper internet;
  internet.Install (context.nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  context.d01 = p2p.Install (context.nodes.Get (0), context.nodes.Get (1));
  context.d02 = p2p.Install (context.nodes.Get (0), context.nodes.Get (2));
  context.d23 = p2p.Install (context.nodes.Get (2), context.nodes.Get (3));

  Ipv4AddressHelper address;
  address.SetBase ("10.71.1.0", "255.255.255.252");
  context.if01 = address.Assign (context.d01);
  address.SetBase ("10.71.2.0", "255.255.255.252");
  context.if02 = address.Assign (context.d02);
  address.SetBase ("10.71.3.0", "255.255.255.252");
  context.if23 = address.Assign (context.d23);

  OspfAppHelper ospf;
  ConfigureFastColdStart (ospf);
  ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
  ospf.SetAttribute ("EnableAreaProxy", BooleanValue (true));

  context.apps = ospf.Install (context.nodes);

  DynamicCast<OspfApp> (context.apps.Get (0))->SetArea (0);
  DynamicCast<OspfApp> (context.apps.Get (1))->SetArea (0);
  DynamicCast<OspfApp> (context.apps.Get (2))->SetArea (0);
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
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
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
    if (state.route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
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
    if (before == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
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

class OspfInterfacePrefixesAreDisabledByDefaultTest : public TestCase
{
public:
  OspfInterfacePrefixesAreDisabledByDefaultTest ()
    : TestCase ("Interface prefixes are not routable unless explicitly enabled per interface")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (false, false, false);

    Simulator::Stop (Seconds (1.5));
    Simulator::Run ();

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), context.if12.GetAddress (1), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (route, nullptr,
                           "router link interface prefixes should not be routable by default");
    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOROUTETOHOST,
                           "default interface-prefix behavior should report no route");

    Simulator::Destroy ();
  }
};

class OspfInterfaceDerivedLinkPrefixesAreRoutableTest : public TestCase
{
public:
  OspfInterfaceDerivedLinkPrefixesAreRoutableTest ()
    : TestCase ("Interface-derived point-to-point link prefixes remain routable without injected prefixes")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), context.if12.GetAddress (1), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "node0 should route to node2's point-to-point interface address without any injected prefix");
    NS_TEST_ASSERT_MSG_NE (route, nullptr,
                           "the remote point-to-point interface address should resolve through the interface-derived prefix");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), context.if01.GetAddress (1),
                           "interface-derived link prefixes should resolve through the next router hop");

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
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
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

class OspfMultiAccessInterfacePrefixCanBeEnabledTest : public TestCase
{
public:
  OspfMultiAccessInterfacePrefixCanBeEnabledTest ()
    : TestCase ("A multi-access interface prefix becomes routable only when enabled on that interface")
  {
  }

private:
  void DoRun () override
  {
    NodeContainer nodes;
    nodes.Create (3);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
    auto d01 = p2p.Install (nodes.Get (0), nodes.Get (1));

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (5000)));
    NetDeviceContainer d12 = csma.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));

    Ipv4AddressHelper address;
    address.SetBase ("10.81.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if01 = address.Assign (d01);
    address.SetBase ("10.81.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address.Assign (d12);

    OspfAppHelper ospf;
    ConfigureFastColdStart (ospf);
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0)));
    apps.Add (ospf.Install (nodes.Get (1)));

    auto app1 = GetOspfApp (nodes.Get (1));
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "node1 should host an OspfApp");

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (5.0));

    Simulator::Schedule (Seconds (1.0), &OspfApp::SetInterfacePrefixRoutable, app1,
                         d12.Get (0)->GetIfIndex (), true);

    Simulator::Stop (Seconds (2.5));
    Simulator::Run ();

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (nodes.Get (0), if12.GetAddress (1), nullptr, &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "node0 should learn the enabled multi-access interface prefix through node1");
    NS_TEST_ASSERT_MSG_NE (route, nullptr,
                           "the enabled multi-access interface prefix should be routable");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), if01.GetAddress (1),
                           "traffic to the multi-access subnet should use node1 as the next hop");

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

class OspfRouteOutputRejectsDownInterfaceTest : public TestCase
{
public:
  OspfRouteOutputRejectsDownInterfaceTest ()
    : TestCase ("RouteOutput rejects candidates on a down interface and reports no route")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto ipv41 = context.nodes.Get (1)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv41, nullptr, "node1 should expose Ipv4");

    const uint32_t ifIndex = context.d12.Get (0)->GetIfIndex ();
    ipv41->SetDown (ifIndex);

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (1), context.if12.GetAddress (1), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (route, nullptr,
                           "lookup must not return a route through an interface that is already down");
    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOROUTETOHOST,
                           "down-interface lookup should report no route to host");

    Simulator::Destroy ();
  }
};

class OspfInterfaceDownWithdrawsRoutesWithoutPollingTest : public TestCase
{
public:
  OspfInterfaceDownWithdrawsRoutesWithoutPollingTest ()
    : TestCase ("Local interface notifications withdraw remote routes without AutoSync polling")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (false, false);

    Simulator::Stop (Seconds (2.0));
    Simulator::Run ();

    Socket::SocketErrno beforeErr = Socket::ERROR_NOTERROR;
    auto before = LookupOspfRoute (context.nodes.Get (0), context.if12.GetAddress (1), nullptr,
                                   &beforeErr);
    NS_TEST_ASSERT_MSG_NE (before, nullptr, "node0 should have a route to node2 before the link-down event");
    if (before == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (beforeErr, Socket::ERROR_NOTERROR,
                           "precondition route lookup should succeed before the link-down event");

    auto ipv41 = context.nodes.Get (1)->GetObject<Ipv4> ();
    auto ipv42 = context.nodes.Get (2)->GetObject<Ipv4> ();
    NS_TEST_ASSERT_MSG_NE (ipv41, nullptr, "node1 should expose Ipv4");
    NS_TEST_ASSERT_MSG_NE (ipv42, nullptr, "node2 should expose Ipv4");

    const uint32_t if12Node1 = context.d12.Get (0)->GetIfIndex ();
    const uint32_t if12Node2 = context.d12.Get (1)->GetIfIndex ();

    Simulator::Schedule (Seconds (0.0), &Ipv4::SetDown, ipv41, if12Node1);
    Simulator::Schedule (Seconds (0.0), &Ipv4::SetDown, ipv42, if12Node2);

    Simulator::Stop (Seconds (0.4));
    Simulator::Run ();

    Socket::SocketErrno afterErr = Socket::ERROR_NOTERROR;
    auto after = LookupOspfRoute (context.nodes.Get (0), context.if12.GetAddress (1), nullptr,
                                  &afterErr);

    NS_TEST_ASSERT_MSG_EQ (after, nullptr,
                           "node0 should withdraw the remote route after the local interface-down notifications fire");
    NS_TEST_ASSERT_MSG_EQ (afterErr, Socket::ERROR_NOROUTETOHOST,
                           "route withdrawal should surface as no route to host");

    Simulator::Destroy ();
  }
};

class OspfIntraAreaPrefixChurnAvoidsSpfTest : public TestCase
{
public:
  OspfIntraAreaPrefixChurnAvoidsSpfTest ()
    : TestCase ("Intra-area prefix churn updates owner tables without rerunning L1 SPF")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto app0 = GetOspfApp (context.nodes.Get (0));
    auto app1 = GetOspfApp (context.nodes.Get (1));
    auto app2 = GetOspfApp (context.nodes.Get (2));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "node0 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "node1 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");

    const uint64_t beforeSpf = app0->GetL1ShortestPathRunCount ();
    const uint64_t beforeSpf1 = app1->GetL1ShortestPathRunCount ();
    const uint64_t beforeSpf2 = app2->GetL1ShortestPathRunCount ();
    NS_TEST_ASSERT_MSG_GT (beforeSpf, 0u, "node0 should have completed initial L1 SPF before churn");

    app2->AddReachableAddress (context.d12.Get (1)->GetIfIndex (), Ipv4Address ("10.200.0.0"),
                   Ipv4Mask ("255.255.255.0"), Ipv4Address::GetAny (), 7);

    Simulator::Stop (Seconds (0.6));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_EQ (app0->GetL1ShortestPathRunCount (), beforeSpf,
                           "pure intra-area prefix churn must not rerun L1 SPF on node0");
    NS_TEST_ASSERT_MSG_EQ (app1->GetL1ShortestPathRunCount (), beforeSpf1,
                 "pure intra-area prefix churn must not rerun L1 SPF on node1");
    NS_TEST_ASSERT_MSG_EQ (app2->GetL1ShortestPathRunCount (), beforeSpf2,
                 "pure intra-area prefix churn must not rerun L1 SPF on node2");

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.200.0.42"), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "node0 should resolve the newly advertised remote prefix");
    NS_TEST_ASSERT_MSG_NE (route, nullptr, "lookup should return a route for the new remote prefix");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), context.if01.GetAddress (1),
                           "node0 should still use node1 as the first hop after prefix churn");
    NS_TEST_ASSERT_MSG_EQ (route->GetOutputDevice (), context.d01.Get (0),
                           "node0 should still emit churned-prefix traffic on the (0,1) interface");

    Simulator::Destroy ();
  }
};

class OspfInterAreaPrefixChurnAvoidsSpfTest : public TestCase
{
public:
  OspfInterAreaPrefixChurnAvoidsSpfTest ()
    : TestCase ("Inter-area prefix churn updates proxy summaries without rerunning L2 SPF")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateTwoAreaLine (true);

    Simulator::Stop (Seconds (1.5));
    Simulator::Run ();

    auto app0 = GetOspfApp (context.nodes.Get (0));
    auto app1 = GetOspfApp (context.nodes.Get (1));
    auto app2 = GetOspfApp (context.nodes.Get (2));
    auto app3 = GetOspfApp (context.nodes.Get (3));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "node0 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "node1 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app3, nullptr, "node3 should host an OspfApp");

    const uint64_t beforeSpf = app0->GetL2ShortestPathRunCount ();
    const uint64_t beforeSpf1 = app1->GetL2ShortestPathRunCount ();
    const uint64_t beforeSpf2 = app2->GetL2ShortestPathRunCount ();
    const uint64_t beforeSpf3 = app3->GetL2ShortestPathRunCount ();
    const uint64_t beforeL1Spf0 = app0->GetL1ShortestPathRunCount ();
    const uint64_t beforeL1Spf1 = app1->GetL1ShortestPathRunCount ();
    const uint64_t beforeL1Spf2 = app2->GetL1ShortestPathRunCount ();
    const uint64_t beforeL1Spf3 = app3->GetL1ShortestPathRunCount ();
    NS_TEST_ASSERT_MSG_GT (beforeSpf, 0u, "node0 should have completed initial L2 SPF before churn");

    app3->AddReachableAddress (context.d23.Get (1)->GetIfIndex (), Ipv4Address ("10.201.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 9);

    Simulator::Stop (Seconds (0.9));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_EQ (app0->GetL2ShortestPathRunCount (), beforeSpf,
                           "pure inter-area prefix churn must not rerun L2 SPF on node0");
    NS_TEST_ASSERT_MSG_EQ (app1->GetL2ShortestPathRunCount (), beforeSpf1,
                 "pure inter-area prefix churn must not rerun L2 SPF on node1");
    NS_TEST_ASSERT_MSG_EQ (app2->GetL2ShortestPathRunCount (), beforeSpf2,
                 "pure inter-area prefix churn must not rerun L2 SPF on node2");
    NS_TEST_ASSERT_MSG_EQ (app3->GetL2ShortestPathRunCount (), beforeSpf3,
                 "pure inter-area prefix churn must not rerun L2 SPF on node3");
    NS_TEST_ASSERT_MSG_EQ (app0->GetL1ShortestPathRunCount (), beforeL1Spf0,
                 "pure inter-area prefix churn must not rerun L1 SPF on node0");
    NS_TEST_ASSERT_MSG_EQ (app1->GetL1ShortestPathRunCount (), beforeL1Spf1,
                 "pure inter-area prefix churn must not rerun L1 SPF on node1");
    NS_TEST_ASSERT_MSG_EQ (app2->GetL1ShortestPathRunCount (), beforeL1Spf2,
                 "pure inter-area prefix churn must not rerun L1 SPF on node2");
    NS_TEST_ASSERT_MSG_EQ (app3->GetL1ShortestPathRunCount (), beforeL1Spf3,
                 "pure inter-area prefix churn must not rerun L1 SPF on node3");

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.201.7.9"), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "node0 should resolve the newly advertised remote area prefix");
    NS_TEST_ASSERT_MSG_NE (route, nullptr, "lookup should return a route for the new remote area prefix");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), context.if01.GetAddress (1),
                           "node0 should still use node1 as the first border hop after inter-area churn");
    NS_TEST_ASSERT_MSG_EQ (route->GetOutputDevice (), context.d01.Get (0),
                           "node0 should still emit churned inter-area traffic on the (0,1) interface");

    Simulator::Destroy ();
  }
};

class OspfEqualCostRouterTieBreakPrefersLowerRouterIdTest : public TestCase
{
public:
  OspfEqualCostRouterTieBreakPrefersLowerRouterIdTest ()
    : TestCase ("Equal-cost router owners prefer the lower router ID deterministically")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto app1 = GetOspfApp (context.nodes.Get (1));
    auto app2 = GetOspfApp (context.nodes.Get (2));
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "node1 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");

    app1->AddReachableAddress (context.d01.Get (1)->GetIfIndex (), Ipv4Address ("10.210.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 2);
    app2->AddReachableAddress (context.d12.Get (1)->GetIfIndex (), Ipv4Address ("10.210.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 1);

    Simulator::Stop (Seconds (0.8));
    Simulator::Run ();

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.210.7.9"), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "node0 should resolve the overlapping router-owned prefix");
    NS_TEST_ASSERT_MSG_NE (route, nullptr, "lookup should return a route for the overlapping prefix");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), context.if01.GetAddress (1),
                           "equal-cost router owners should prefer the lower router ID (node1)");
    NS_TEST_ASSERT_MSG_EQ (route->GetOutputDevice (), context.d01.Get (0),
                           "the chosen equal-cost router owner should drive the egress interface");

    Simulator::Destroy ();
  }
};

class OspfEqualCostRouterBeatsAreaOwnerTest : public TestCase
{
public:
  OspfEqualCostRouterBeatsAreaOwnerTest ()
    : TestCase ("Equal-cost router ownership beats area ownership for the same prefix")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateRouterVsAreaTieTopology (true);

    Simulator::Stop (Seconds (1.5));
    Simulator::Run ();

    auto app1 = GetOspfApp (context.nodes.Get (1));
    auto app3 = GetOspfApp (context.nodes.Get (3));
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "node1 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app3, nullptr, "node3 should host an OspfApp");

    app1->AddReachableAddress (context.d01.Get (1)->GetIfIndex (), Ipv4Address ("10.211.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 3);
    app3->AddReachableAddress (context.d23.Get (1)->GetIfIndex (), Ipv4Address ("10.211.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 1);

    Simulator::Stop (Seconds (0.9));
    Simulator::Run ();

    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.211.7.9"), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "node0 should resolve the overlapping router-owned and area-owned prefix");
    NS_TEST_ASSERT_MSG_NE (route, nullptr, "lookup should return a route for the overlapping prefix");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), context.if01.GetAddress (1),
                           "equal-cost router ownership should win over area ownership");
    NS_TEST_ASSERT_MSG_EQ (route->GetOutputDevice (), context.d01.Get (0),
                           "router ownership preference should keep traffic on node1's link");

    Simulator::Destroy ();
  }
};

class OspfAreaOwnerKeepsLowestDuplicateMetricTest : public TestCase
{
public:
  OspfAreaOwnerKeepsLowestDuplicateMetricTest ()
    : TestCase ("Area-owned duplicate prefixes retain the lowest advertised metric")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateTwoAreaLine (true);

    Simulator::Stop (Seconds (1.5));
    Simulator::Run ();

    auto app0 = GetOspfApp (context.nodes.Get (0));
    auto app2 = GetOspfApp (context.nodes.Get (2));
    auto app3 = GetOspfApp (context.nodes.Get (3));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "node0 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app3, nullptr, "node3 should host an OspfApp");

    app2->AddReachableAddress (context.d12.Get (1)->GetIfIndex (), Ipv4Address ("10.212.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 9);
    app3->AddReachableAddress (context.d23.Get (1)->GetIfIndex (), Ipv4Address ("10.212.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 1);

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-routing-area-duplicate-metric");
    std::filesystem::create_directories (outDir);
    Simulator::Schedule (Seconds (0.8), &OspfApp::PrintRouting, app0, outDir, "n0.routes");

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    const std::string n0 = ReadAll (outDir / "n0.routes");
    Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.212.5.9"), nullptr,
                                  &sockerr);

    NS_TEST_ASSERT_MSG_EQ (sockerr, Socket::ERROR_NOTERROR,
                           "node0 should resolve the duplicate remote-area prefix");
    NS_TEST_ASSERT_MSG_NE (route, nullptr, "lookup should return a route for the duplicate area prefix");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (HasPrefixOwnerMetric (n0, "10.212.0.0", "area=1", 1), true,
                           "area-owned duplicate prefixes should keep the lowest metric in the prefix-owner table\n" +
                               n0);
    NS_TEST_ASSERT_MSG_EQ (HasPrefixOwnerMetric (n0, "10.212.0.0", "area=1", 9), false,
                           "area-owned duplicate prefixes must not keep a stale higher metric in the prefix-owner table\n" +
                               n0);

    Simulator::Destroy ();
  }
};

class OspfRemoveReachableAddressWithdrawsInjectedPrefixTest : public TestCase
{
public:
  OspfRemoveReachableAddressWithdrawsInjectedPrefixTest ()
    : TestCase ("Removing an injected reachable prefix withdraws it cleanly at runtime")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto app0 = GetOspfApp (context.nodes.Get (0));
    auto app2 = GetOspfApp (context.nodes.Get (2));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "node0 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");

    const auto prefix = Ipv4Address ("10.213.0.0");
    const auto mask = Ipv4Mask ("255.255.0.0");
    const auto ifIndex = context.d12.Get (1)->GetIfIndex ();

    app2->AddReachableAddress (ifIndex, prefix, mask, Ipv4Address::GetAny (), 4);

    Simulator::Stop (Seconds (0.8));
    Simulator::Run ();

    Socket::SocketErrno beforeErr = Socket::ERROR_NOTERROR;
    auto before = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.213.1.1"), nullptr,
                                   &beforeErr);
    NS_TEST_ASSERT_MSG_EQ (beforeErr, Socket::ERROR_NOTERROR,
                           "node0 should resolve the injected prefix before removal");
    NS_TEST_ASSERT_MSG_NE (before, nullptr, "lookup should return a route before removal");
    if (before == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (before->GetGateway (), context.if01.GetAddress (1),
                           "the injected prefix should use node1 as the first hop before removal");

    app2->RemoveReachableAddress (ifIndex, prefix, mask);

    Simulator::Stop (Seconds (0.6));
    Simulator::Run ();

    Socket::SocketErrno afterErr = Socket::ERROR_NOTERROR;
    auto after = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.213.1.1"), nullptr,
                                  &afterErr);
    NS_TEST_ASSERT_MSG_EQ (after, nullptr,
                           "node0 should withdraw the injected prefix after removal");
    NS_TEST_ASSERT_MSG_EQ (afterErr, Socket::ERROR_NOROUTETOHOST,
                           "after runtime removal, lookup should report no route");

    Simulator::Destroy ();
  }
};

class OspfSetReachableAddressesReplacesInjectedSetTest : public TestCase
{
public:
  OspfSetReachableAddressesReplacesInjectedSetTest ()
    : TestCase ("Setting injected reachable prefixes replaces only the injected set and preserves interface-derived routes")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto app0 = GetOspfApp (context.nodes.Get (0));
    auto app2 = GetOspfApp (context.nodes.Get (2));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "node0 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");

    std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> firstInjected;
    firstInjected.emplace_back (context.d12.Get (1)->GetIfIndex (), Ipv4Address ("10.214.0.0").Get (),
                                Ipv4Mask ("255.255.0.0").Get (), Ipv4Address::GetAny ().Get (), 4);
    const bool firstChanged = app2->SetReachableAddresses (std::move (firstInjected));
    NS_TEST_ASSERT_MSG_EQ (firstChanged, true,
                           "the first injected-set update should report a change");

    Simulator::Stop (Seconds (1.8));
    Simulator::Run ();

    Socket::SocketErrno beforeErr = Socket::ERROR_NOTERROR;
    auto before = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.214.1.1"), nullptr,
                                   &beforeErr);
    NS_TEST_ASSERT_MSG_EQ (beforeErr, Socket::ERROR_NOTERROR,
                           "node0 should learn the initially injected prefix");
    NS_TEST_ASSERT_MSG_NE (before, nullptr,
                           "the initially injected prefix should be reachable before replacement");
    if (before == nullptr)
      {
        Simulator::Destroy ();
        return;
      }

    std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> replacementInjected;
    replacementInjected.emplace_back (context.d12.Get (1)->GetIfIndex (), Ipv4Address ("10.215.0.0").Get (),
                                      Ipv4Mask ("255.255.0.0").Get (), Ipv4Address::GetAny ().Get (), 6);
    const bool replacementChanged = app2->SetReachableAddresses (std::move (replacementInjected));
    NS_TEST_ASSERT_MSG_EQ (replacementChanged, true,
                           "replacing the injected set should report a change");

    Simulator::Stop (Seconds (0.7));
    Simulator::Run ();

    Socket::SocketErrno oldErr = Socket::ERROR_NOTERROR;
    auto oldRoute = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.214.1.1"), nullptr,
                                     &oldErr);
    NS_TEST_ASSERT_MSG_EQ (oldRoute, nullptr,
                           "replacing the injected set must withdraw prefixes that were removed from the injected set");
    NS_TEST_ASSERT_MSG_EQ (oldErr, Socket::ERROR_NOROUTETOHOST,
                           "the removed injected prefix should no longer be routable");

    Socket::SocketErrno baseErr = Socket::ERROR_NOTERROR;
    auto baseRoute = LookupOspfRoute (context.nodes.Get (0), context.if12.GetAddress (1), nullptr,
                                      &baseErr);
    NS_TEST_ASSERT_MSG_EQ (baseErr, Socket::ERROR_NOTERROR,
                           "replacing injected prefixes must not break the base route to node2");
    NS_TEST_ASSERT_MSG_NE (baseRoute, nullptr,
                           "node0 should still resolve node2's interface prefix after injected-set replacement");
    if (baseRoute == nullptr)
      {
        Simulator::Destroy ();
        return;
      }

    Socket::SocketErrno newErr = Socket::ERROR_NOTERROR;
    auto newRoute = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.215.1.1"), nullptr,
                                     &newErr);
    NS_TEST_ASSERT_MSG_EQ (newErr, Socket::ERROR_NOTERROR,
                           "the replacement injected prefix should be routable");
    NS_TEST_ASSERT_MSG_NE (newRoute, nullptr,
                           "the replacement injected prefix should be installed successfully");
    if (newRoute == nullptr)
      {
        Simulator::Destroy ();
        return;
      }

    Socket::SocketErrno interfaceErr = Socket::ERROR_NOTERROR;
    auto interfaceRoute = LookupOspfRoute (context.nodes.Get (0), context.if12.GetAddress (1), nullptr,
                                           &interfaceErr);
    NS_TEST_ASSERT_MSG_EQ (interfaceErr, Socket::ERROR_NOTERROR,
                           "replacing injected prefixes must not disturb interface-derived reachability");
    NS_TEST_ASSERT_MSG_NE (interfaceRoute, nullptr,
                           "the point-to-point interface prefix should remain routable after injected-set replacement");
    if (interfaceRoute == nullptr)
      {
        Simulator::Destroy ();
        return;
      }

    Simulator::Destroy ();
  }
};

class OspfInjectedGatewayRoutePreservedLocallyTest : public TestCase
{
public:
  OspfInjectedGatewayRoutePreservedLocallyTest ()
    : TestCase ("Local injected routes preserve their configured gateway in the two-step forwarding table")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto app2 = GetOspfApp (context.nodes.Get (2));
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");

    const auto prefix = Ipv4Address ("10.216.0.0");
    const auto mask = Ipv4Mask ("255.255.0.0");
    const auto ifIndex = context.d12.Get (1)->GetIfIndex ();
    const auto gateway = context.if12.GetAddress (0);

    app2->AddReachableAddress (ifIndex, prefix, mask, gateway, 4);

    const std::filesystem::path outDir = CreateTempDirFilename ("ospf-routing-injected-gateway");
    std::filesystem::create_directories (outDir);
    Simulator::Schedule (Seconds (0.8), &OspfApp::PrintRouting, app2, outDir, "n2.routes");

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    Socket::SocketErrno err = Socket::ERROR_NOTERROR;
    auto route = LookupOspfRoute (context.nodes.Get (2), Ipv4Address ("10.216.1.1"), nullptr, &err);
    NS_TEST_ASSERT_MSG_EQ (err, Socket::ERROR_NOTERROR,
                           "node2 should resolve the locally injected prefix");
    NS_TEST_ASSERT_MSG_NE (route, nullptr,
                           "node2 should return a forwarding entry for the locally injected prefix");
    if (route == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (route->GetGateway (), gateway,
                           "the forwarding lookup should preserve the configured explicit gateway");
    const bool usesConfiguredDevice = route->GetOutputDevice () == context.d12.Get (1);
    NS_TEST_ASSERT_MSG_EQ (usesConfiguredDevice, true,
                           "the forwarding lookup should use the configured outgoing interface");

    const std::string n2 = ReadAll (outDir / "n2.routes");
    const bool hasGatewayLine = n2.find ("next-hop=" + Ipv4ToString (gateway)) != std::string::npos;
    NS_TEST_ASSERT_MSG_EQ (hasGatewayLine, true,
                           "the owner-resolution table should retain the explicit gateway\n" + n2);

    Simulator::Destroy ();
  }
};

class OspfInjectedGatewayOwnersDoNotCollideTest : public TestCase
{
public:
  OspfInjectedGatewayOwnersDoNotCollideTest ()
    : TestCase ("Distinct injected gateway owners remain distinct even when their old hashed IDs would collide")
  {
  }

private:
  void DoRun () override
  {
    auto context = CreateThreeNodeLine (true);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    auto app1 = GetOspfApp (context.nodes.Get (1));
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "node1 should host an OspfApp");

    const uint32_t if01 = context.d01.Get (1)->GetIfIndex ();
    const uint32_t if12 = context.d12.Get (0)->GetIfIndex ();
    const uint32_t gateway1Value = context.if01.GetAddress (0).Get ();
    const uint32_t mixingConstant = 2654435761u;
    const uint32_t gateway2Value =
      gateway1Value ^ (if01 * mixingConstant) ^ (if12 * mixingConstant);
    const Ipv4Address gateway1 (gateway1Value);
    const Ipv4Address gateway2 (gateway2Value);
    const bool gateway2Usable = gateway2 != Ipv4Address::GetZero ();
    const bool gateway2Indirect = gateway2 != Ipv4Address::GetAny ();

    NS_TEST_ASSERT_MSG_EQ (gateway2Usable, true,
                           "the crafted second gateway must be usable for the collision regression");
    NS_TEST_ASSERT_MSG_EQ (gateway2Indirect, true,
                           "the crafted second gateway must not collapse into a direct route");

    app1->AddReachableAddress (if01, Ipv4Address ("10.217.0.0"), Ipv4Mask ("255.255.0.0"),
                               gateway1, 4);
    app1->AddReachableAddress (if12, Ipv4Address ("10.218.0.0"), Ipv4Mask ("255.255.0.0"),
                               gateway2, 6);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

    Socket::SocketErrno errA = Socket::ERROR_NOTERROR;
    auto routeA = LookupOspfRoute (context.nodes.Get (1), Ipv4Address ("10.217.1.1"), nullptr, &errA);
    NS_TEST_ASSERT_MSG_EQ (errA, Socket::ERROR_NOTERROR,
                           "node1 should resolve the first injected prefix");
    NS_TEST_ASSERT_MSG_NE (routeA, nullptr,
                           "node1 should return a forwarding entry for the first injected prefix");
    if (routeA == nullptr)
      {
        Simulator::Destroy ();
        return;
      }

    Socket::SocketErrno errB = Socket::ERROR_NOTERROR;
    auto routeB = LookupOspfRoute (context.nodes.Get (1), Ipv4Address ("10.218.1.1"), nullptr, &errB);
    NS_TEST_ASSERT_MSG_EQ (errB, Socket::ERROR_NOTERROR,
                           "node1 should resolve the second injected prefix");
    NS_TEST_ASSERT_MSG_NE (routeB, nullptr,
                           "node1 should return a forwarding entry for the second injected prefix");
    if (routeB == nullptr)
      {
        Simulator::Destroy ();
        return;
      }

    NS_TEST_ASSERT_MSG_EQ (routeA->GetGateway (), gateway1,
                           "the first injected prefix must retain its own explicit gateway");
    NS_TEST_ASSERT_MSG_EQ (routeB->GetGateway (), gateway2,
                           "the second injected prefix must retain its own explicit gateway");
    const bool routeAUsesIf01 = routeA->GetOutputDevice () == context.d01.Get (1);
    const bool routeBUsesIf12 = routeB->GetOutputDevice () == context.d12.Get (0);
    NS_TEST_ASSERT_MSG_EQ (routeAUsesIf01, true,
                           "the first injected prefix must retain its own interface");
    NS_TEST_ASSERT_MSG_EQ (routeBUsesIf12, true,
                           "the second injected prefix must retain its own interface");

    Simulator::Destroy ();
  }
};

class OspfMinLsIntervalDefersFreshSelfProcessingTest : public TestCase
{
public:
  OspfMinLsIntervalDefersFreshSelfProcessingTest ()
    : TestCase ("MinLsInterval defers local self-processing until a fresh summary is originated")
  {
  }

private:
  void DoRun () override
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
    address.SetBase ("10.73.1.0", "255.255.255.252");
    context.if01 = address.Assign (context.d01);
    address.SetBase ("10.73.2.0", "255.255.255.252");
    context.if12 = address.Assign (context.d12);
    address.SetBase ("10.73.3.0", "255.255.255.252");
    context.if23 = address.Assign (context.d23);

    OspfAppHelper ospf;
    ConfigureFastColdStart (ospf);
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
    ospf.SetAttribute ("EnableAreaProxy", BooleanValue (true));
    ospf.SetAttribute ("AreaLeaderMode",
               EnumValue (OspfApp::AREA_LEADER_LOWEST_ROUTER_ID));
    ospf.SetAttribute ("MinLsInterval", TimeValue (Seconds (2)));
    ospf.SetAttribute ("EnableLsaThrottleStats", BooleanValue (true));

    context.apps = ospf.Install (context.nodes);

    DynamicCast<OspfApp> (context.apps.Get (0))->SetArea (0);
    DynamicCast<OspfApp> (context.apps.Get (1))->SetArea (0);
    DynamicCast<OspfApp> (context.apps.Get (2))->SetArea (1);
    DynamicCast<OspfApp> (context.apps.Get (3))->SetArea (1);

    ospf.ConfigureReachablePrefixesFromInterfaces (context.nodes);

    context.apps.Start (Seconds (0.0));
    context.apps.Stop (Seconds (6.0));

    Simulator::Stop (Seconds (1.2));
    Simulator::Run ();

    auto app0 = GetOspfApp (context.nodes.Get (0));
    auto app2 = GetOspfApp (context.nodes.Get (2));
    NS_TEST_ASSERT_MSG_NE (app0, nullptr, "node0 should host an OspfApp");
    NS_TEST_ASSERT_MSG_NE (app2, nullptr, "node2 should host an OspfApp");

    app2->ResetLsaThrottleStats ();
    app2->AddReachableAddress (context.d12.Get (1)->GetIfIndex (), Ipv4Address ("10.220.0.0"),
                   Ipv4Mask ("255.255.0.0"), Ipv4Address::GetAny (), 5);

    const auto immediateStats = app2->GetLsaThrottleStats ();
    NS_TEST_ASSERT_MSG_EQ (immediateStats.recomputeTriggers, 1u,
                           "only the local L1 summary should be throttled immediately after the prefix change");
    NS_TEST_ASSERT_MSG_EQ (immediateStats.deferredScheduled, 1u,
                           "the local L1 summary should schedule exactly one deferred regeneration");
    NS_TEST_ASSERT_MSG_EQ (immediateStats.immediate, 0u,
                           "the local L1 summary should not be regenerated immediately inside MinLsInterval");

    Simulator::Stop (Seconds (4.5));
    Simulator::Run ();

    Socket::SocketErrno afterErr = Socket::ERROR_NOTERROR;
    auto after = LookupOspfRoute (context.nodes.Get (0), Ipv4Address ("10.220.1.1"), nullptr,
                                  &afterErr);
    NS_TEST_ASSERT_MSG_EQ (afterErr, Socket::ERROR_NOTERROR,
                           "the remote area should learn the new prefix after the deferred local summary runs");
    NS_TEST_ASSERT_MSG_NE (after, nullptr,
                           "lookup after the deferred local summary should return a route");
    if (after == nullptr)
      {
        Simulator::Destroy ();
        return;
      }
    NS_TEST_ASSERT_MSG_EQ (after->GetGateway (), context.if01.GetAddress (1),
                           "the propagated route should still use node1 as the first border hop");

    const auto finalStats = app2->GetLsaThrottleStats ();
    NS_TEST_ASSERT_MSG_EQ (static_cast<bool> (finalStats.recomputeTriggers >= 2u), true,
                 "after the deferred local summary runs, the area summary should be triggered");
    NS_TEST_ASSERT_MSG_EQ (static_cast<bool> (finalStats.deferredScheduled >= 1u), true,
                 "the throttled local summary path should keep at least one deferred regeneration scheduled");

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
    AddTestCase (new OspfInterfacePrefixesAreDisabledByDefaultTest, TestCase::QUICK);
    AddTestCase (new OspfInterfaceDerivedLinkPrefixesAreRoutableTest, TestCase::QUICK);
    AddTestCase (new OspfMultiAccessInterfacePrefixCanBeEnabledTest, TestCase::QUICK);
    AddTestCase (new OspfInterAreaRouteOutputUsesFirstBorderHopTest, TestCase::QUICK);
    AddTestCase (new OspfRouteOutputReturnsNoRouteWithWrongInterfaceTest, TestCase::QUICK);
    AddTestCase (new OspfRouteOutputRejectsDownInterfaceTest, TestCase::QUICK);
    AddTestCase (new OspfInterfaceDownWithdrawsRoutesWithoutPollingTest, TestCase::QUICK);
    AddTestCase (new OspfIntraAreaPrefixChurnAvoidsSpfTest, TestCase::QUICK);
    AddTestCase (new OspfInterAreaPrefixChurnAvoidsSpfTest, TestCase::QUICK);
    AddTestCase (new OspfEqualCostRouterTieBreakPrefersLowerRouterIdTest, TestCase::QUICK);
    AddTestCase (new OspfEqualCostRouterBeatsAreaOwnerTest, TestCase::QUICK);
    AddTestCase (new OspfAreaOwnerKeepsLowestDuplicateMetricTest, TestCase::QUICK);
    AddTestCase (new OspfRemoveReachableAddressWithdrawsInjectedPrefixTest, TestCase::QUICK);
    AddTestCase (new OspfSetReachableAddressesReplacesInjectedSetTest, TestCase::QUICK);
    AddTestCase (new OspfInjectedGatewayRoutePreservedLocallyTest, TestCase::QUICK);
    AddTestCase (new OspfInjectedGatewayOwnersDoNotCollideTest, TestCase::QUICK);
    AddTestCase (new OspfMinLsIntervalDefersFreshSelfProcessingTest, TestCase::QUICK);
  }
};

static OspfRoutingTestSuite g_ospfRoutingTestSuite;
