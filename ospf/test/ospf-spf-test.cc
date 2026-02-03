/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ospf-app.h"
#include "ns3/ospf-app-helper.h"

#include "ospf-test-utils.h"

#include <optional>
#include <sstream>
#include <vector>
namespace ns3 {

namespace {

using ospf_test_utils::FindStaticRoute;
using ospf_test_utils::ConfigureFastColdStart;

} // namespace

class OspfL1ShortestPathLinearColdStartTest : public TestCase
{
public:
  OspfL1ShortestPathLinearColdStartTest ()
    : TestCase ("L1 SPF installs learned routes (cold start, 3-node line)")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (1);

    NodeContainer nodes;
    nodes.Create (3);

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (1)));
    NetDeviceContainer d12 = p2p.Install (NodeContainer (nodes.Get (1), nodes.Get (2)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if01 = ipv4.Assign (d01);
    ipv4.SetBase ("10.1.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ConfigureFastColdStart (ospf);
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ApplicationContainer apps = ospf.Install (nodes);
    ospf.ConfigureReachablePrefixesFromInterfaces (nodes);
    apps.Start (Seconds (0.5));

    Simulator::Stop (Seconds (3.0));
    Simulator::Run ();

    const auto r0to12 = FindStaticRoute (nodes.Get (0), Ipv4Address ("10.1.2.0"),
                                        Ipv4Mask ("255.255.255.252"));
    NS_TEST_ASSERT_MSG_EQ (r0to12.has_value (), true,
                           "node0 should have a route to 10.1.2.0/30");
    if (r0to12)
      {
        const Ipv4Address expectedGw = if01.GetAddress (1);
        NS_TEST_ASSERT_MSG_EQ (r0to12->gateway, expectedGw,
                               "node0 should route to 10.1.2.0/30 via node1 (10.1.1.2)");
      }

    Simulator::Destroy ();
  }
};

class OspfL1TwoPathShortestHopCountTest : public TestCase
{
public:
  OspfL1TwoPathShortestHopCountTest ()
    : TestCase ("L1 SPF chooses the true shortest path (multi-hop, 2 alternatives)")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (2);
    RngSeedManager::SetRun (1);

    // Multi-hop topology with 2 alternative paths of different hop count.
    // All OSPF interface metrics default to 1, so hop count is the differentiator.
    //
    // Routers: r0..r6 (all run OSPF)
    // Stub host: h7 attached to r6 (does not run OSPF)
    //
    // Short path to stub network behind r6:
    //   r0 - r1 - r3 - r6
    // Long path:
    //   r0 - r2 - r4 - r5 - r6

    NodeContainer routers;
    routers.Create (7);
    Ptr<Node> stub = CreateObject<Node> ();

    NodeContainer all;
    all.Add (routers);
    all.Add (stub);

    InternetStackHelper internet;
    internet.Install (all);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

    NetDeviceContainer d01 = p2p.Install (NodeContainer (routers.Get (0), routers.Get (1)));
    NetDeviceContainer d13 = p2p.Install (NodeContainer (routers.Get (1), routers.Get (3)));
    NetDeviceContainer d36 = p2p.Install (NodeContainer (routers.Get (3), routers.Get (6)));

    NetDeviceContainer d02 = p2p.Install (NodeContainer (routers.Get (0), routers.Get (2)));
    NetDeviceContainer d24 = p2p.Install (NodeContainer (routers.Get (2), routers.Get (4)));
    NetDeviceContainer d45 = p2p.Install (NodeContainer (routers.Get (4), routers.Get (5)));
    NetDeviceContainer d56 = p2p.Install (NodeContainer (routers.Get (5), routers.Get (6)));

    NetDeviceContainer d6h = p2p.Install (NodeContainer (routers.Get (6), stub));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.10.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if01 = ipv4.Assign (d01);
    ipv4.SetBase ("10.10.2.0", "255.255.255.252");
    ipv4.Assign (d13);
    ipv4.SetBase ("10.10.3.0", "255.255.255.252");
    ipv4.Assign (d36);

    ipv4.SetBase ("10.20.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if02 = ipv4.Assign (d02);
    ipv4.SetBase ("10.20.2.0", "255.255.255.252");
    ipv4.Assign (d24);
    ipv4.SetBase ("10.20.3.0", "255.255.255.252");
    ipv4.Assign (d45);
    ipv4.SetBase ("10.20.4.0", "255.255.255.252");
    ipv4.Assign (d56);

    ipv4.SetBase ("10.99.0.0", "255.255.255.252");
    ipv4.Assign (d6h);

    OspfAppHelper ospf;
    ConfigureFastColdStart (ospf);
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ApplicationContainer apps = ospf.Install (routers);
    ospf.ConfigureReachablePrefixesFromInterfaces (routers);
    apps.Start (Seconds (0.5));

    Simulator::Stop (Seconds (4.0));
    Simulator::Run ();

    const auto r0toStub = FindStaticRoute (routers.Get (0), Ipv4Address ("10.99.0.0"),
                                          Ipv4Mask ("255.255.255.252"));
    NS_TEST_ASSERT_MSG_EQ (r0toStub.has_value (), true,
                           "router0 should have a route to stub network 10.99.0.0/30");

    if (r0toStub)
      {
        const Ipv4Address expectedGw = if01.GetAddress (1); // r1 on r0-r1 link
        const Ipv4Address notExpectedGw = if02.GetAddress (1); // r2 on r0-r2 link
        NS_TEST_ASSERT_MSG_EQ (r0toStub->gateway, expectedGw,
                               "router0 should choose the shortest path via r1");
        NS_TEST_ASSERT_MSG_NE (r0toStub->gateway, notExpectedGw,
                               "router0 should not choose the longer path via r2");
      }

    Simulator::Destroy ();
  }
};

class OspfL2MultiAreaShortestAreaPathTest : public TestCase
{
public:
  OspfL2MultiAreaShortestAreaPathTest ()
    : TestCase ("L2 SPF chooses the shortest inter-area path (4 areas, 4 routers per area)")
  {
  }

  void
  DoRun () override
  {
    RngSeedManager::SetSeed (3);
    RngSeedManager::SetRun (1);

    // 4 areas with 4 routers each (16 routers total).
    // This exercises inter-area routing with non-trivial intra-area membership.
    //
    // Area graph (area IDs in parentheses):
    //   A(1) is connected to B(2) directly, and also to C(3).
    //   C(3) is connected to B(2).
    //   B(2) is connected to D(4), and D has the destination stub network.
    //
    // Two inter-area paths exist from A to D:
    //   short: A -> B -> D  (2 area-hops)
    //   long:  A -> C -> B -> D (3 area-hops)
    //
    // We assert that a router in A installs the stub route via the A-B neighbor (not A-C).

    static constexpr uint32_t kAreas = 4;
    static constexpr uint32_t kRoutersPerArea = 4;
    static constexpr uint32_t kNumRouters = kAreas * kRoutersPerArea;
    static constexpr uint32_t kNumStubNetsA = 4;
    static constexpr uint32_t kNumStubNetsC = 4;
    static constexpr uint32_t kNumStubNetsD = 12;
    static constexpr uint32_t kNumStubNets = kNumStubNetsA + kNumStubNetsC + kNumStubNetsD;

    NodeContainer routers;
    routers.Create (kNumRouters);
    NodeContainer stubs;
    stubs.Create (kNumStubNets);

    NodeContainer all;
    all.Add (routers);
    all.Add (stubs);

    InternetStackHelper internet;
    internet.Install (all);

    auto idx = [] (uint32_t areaIndex, uint32_t memberIndex) -> uint32_t {
      return areaIndex * kRoutersPerArea + memberIndex;
    };

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

    // Within each area, connect members in a simple chain: 0-1-2-3.
    std::vector<NetDeviceContainer> areaLinks;
    areaLinks.reserve (kAreas * (kRoutersPerArea - 1));
    for (uint32_t area = 0; area < kAreas; ++area)
      {
        for (uint32_t m = 0; m + 1 < kRoutersPerArea; ++m)
          {
            areaLinks.emplace_back (
              p2p.Install (NodeContainer (routers.Get (idx (area, m)), routers.Get (idx (area, m + 1)))));
          }
      }

    // Cross-area links defining the area graph.
    // A(1)<->B(2)
    NetDeviceContainer linkAB =
      p2p.Install (NodeContainer (routers.Get (idx (0, 0)), routers.Get (idx (1, 0))));
    // A(1)<->C(3)
    NetDeviceContainer linkAC =
      p2p.Install (NodeContainer (routers.Get (idx (0, 1)), routers.Get (idx (2, 0))));
    // C(3)<->B(2)
    NetDeviceContainer linkCB =
      p2p.Install (NodeContainer (routers.Get (idx (2, 1)), routers.Get (idx (1, 1))));
    // B(2)<->D(4)
    NetDeviceContainer linkBD =
      p2p.Install (NodeContainer (routers.Get (idx (1, 2)), routers.Get (idx (3, 0))));

    // Stub networks: some in A, some in C, and many in D.
    std::vector<Ipv4Address> stubNetworksA;
    std::vector<Ipv4Address> stubNetworksC;
    std::vector<Ipv4Address> stubNetworksD;
    stubNetworksA.reserve (kNumStubNetsA);
    stubNetworksC.reserve (kNumStubNetsC);
    stubNetworksD.reserve (kNumStubNetsD);

    std::vector<NetDeviceContainer> stubLinks;
    stubLinks.reserve (kNumStubNets);
    std::vector<uint32_t> stubWhichGroup;
    stubWhichGroup.reserve (kNumStubNets);

    uint32_t stubIdx = 0;
    for (uint32_t i = 0; i < kNumStubNetsD; ++i, ++stubIdx)
      {
        const uint32_t dMember = i % kRoutersPerArea;
        stubLinks.emplace_back (
          p2p.Install (NodeContainer (routers.Get (idx (3, dMember)), stubs.Get (stubIdx))));
        stubWhichGroup.push_back (3);
      }

    for (uint32_t i = 0; i < kNumStubNetsA; ++i, ++stubIdx)
      {
        stubLinks.emplace_back (
          p2p.Install (NodeContainer (routers.Get (idx (0, 3)), stubs.Get (stubIdx))));
        stubWhichGroup.push_back (1);
      }

    for (uint32_t i = 0; i < kNumStubNetsC; ++i, ++stubIdx)
      {
        stubLinks.emplace_back (
          p2p.Install (NodeContainer (routers.Get (idx (2, 3)), stubs.Get (stubIdx))));
        stubWhichGroup.push_back (2);
      }

    // Addressing.
    Ipv4AddressHelper ipv4;
    ipv4.SetBase (Ipv4Address ("10.0.0.0"), Ipv4Mask ("255.255.255.252"));
    Ipv4InterfaceContainer ifA01;
    Ipv4InterfaceContainer ifA12;
    Ipv4InterfaceContainer ifB01;
    Ipv4InterfaceContainer ifB23;
    Ipv4InterfaceContainer ifC01;
    for (uint32_t area = 0; area < kAreas; ++area)
      {
        for (uint32_t m = 0; m + 1 < kRoutersPerArea; ++m)
          {
            const uint32_t linkIndex = area * (kRoutersPerArea - 1) + m;
            Ipv4InterfaceContainer ifc = ipv4.Assign (areaLinks[linkIndex]);

            if (area == 0 && m == 0)
              {
                ifA01 = ifc;
              }
            else if (area == 0 && m == 1)
              {
                ifA12 = ifc;
              }
            else if (area == 1 && m == 0)
              {
                ifB01 = ifc;
              }
            else if (area == 1 && m == 2)
              {
                ifB23 = ifc;
              }
            else if (area == 2 && m == 0)
              {
                ifC01 = ifc;
              }

            ipv4.NewNetwork ();
          }
      }

    Ipv4InterfaceContainer ifAB = ipv4.Assign (linkAB);
    ipv4.NewNetwork ();
    Ipv4InterfaceContainer ifAC = ipv4.Assign (linkAC);
    ipv4.NewNetwork ();
    Ipv4InterfaceContainer ifCB = ipv4.Assign (linkCB);
    ipv4.NewNetwork ();
    Ipv4InterfaceContainer ifBD = ipv4.Assign (linkBD);
    ipv4.NewNetwork ();

    Ipv4AddressHelper ipv4Stub;
    for (uint32_t i = 0; i < kNumStubNets; ++i)
      {
        std::ostringstream base;
        const uint32_t group = stubWhichGroup[i];
        const uint32_t netIndex = i + 1;
        if (group == 3)
          {
            base << "10.99." << netIndex << ".0";
          }
        else if (group == 2)
          {
            base << "10.97." << netIndex << ".0";
          }
        else
          {
            base << "10.98." << netIndex << ".0";
          }

        const Ipv4Address network (base.str ().c_str ());

        ipv4Stub.SetBase (network, Ipv4Mask ("255.255.255.252"));
        ipv4Stub.Assign (stubLinks[i]);

        if (group == 3)
          {
            stubNetworksD.emplace_back (network);
          }
        else if (group == 2)
          {
            stubNetworksC.emplace_back (network);
          }
        else
          {
            stubNetworksA.emplace_back (network);
          }
      }

    OspfAppHelper ospf;
    ConfigureFastColdStart (ospf);
    ospf.SetAttribute ("EnableAreaProxy", BooleanValue (true));
    ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));

    ApplicationContainer apps = ospf.Install (routers);
    NS_TEST_ASSERT_MSG_EQ (apps.GetN (), routers.GetN (), "expected one OspfApp per router");

    for (uint32_t i = 0; i < kNumRouters; ++i)
      {
        Ptr<OspfApp> app = DynamicCast<OspfApp> (apps.Get (i));
        NS_TEST_ASSERT_MSG_NE (app, nullptr, "expected OspfApp");
        const uint32_t areaId = (i / kRoutersPerArea) + 1;
        app->SetArea (areaId);
      }

    ospf.ConfigureReachablePrefixesFromInterfaces (routers);
    apps.Start (Seconds (0.5));

    Simulator::Stop (Seconds (8.0));
    Simulator::Run ();

    const Ipv4Mask stubMask ("255.255.255.252");

    auto AssertRoutes = [&] (Ptr<Node> src, const std::vector<Ipv4Address> &dstNetworks,
                             Ipv4Address expectedGw, std::optional<Ipv4Address> forbiddenGw,
                             const char *label) {
      for (const auto &dst : dstNetworks)
        {
          const auto r = FindStaticRoute (src, dst, stubMask);
          NS_TEST_ASSERT_MSG_EQ (r.has_value (), true, label);
          if (r)
            {
              NS_TEST_ASSERT_MSG_EQ (r->gateway, expectedGw, label);
              if (forbiddenGw)
                {
                  NS_TEST_ASSERT_MSG_NE (r->gateway, *forbiddenGw, label);
                }
            }
        }
    };

    // Many endpoint pairs across multiple areas.
    Ptr<Node> a0 = routers.Get (idx (0, 0));
    Ptr<Node> a2 = routers.Get (idx (0, 2));
    Ptr<Node> b0 = routers.Get (idx (1, 0));
    Ptr<Node> b3 = routers.Get (idx (1, 3));
    Ptr<Node> c0 = routers.Get (idx (2, 0));
    Ptr<Node> d0 = routers.Get (idx (3, 0));

    // A0 -> D stubs: should go to B0 via A-B (not toward A1 / A-C).
    AssertRoutes (a0, stubNetworksD,
                  ifAB.GetAddress (1),
                  std::optional<Ipv4Address> (ifA01.GetAddress (1)),
                  "A0 route to D stubs should use A-B");

    // A2 -> D stubs: should go toward A1 (toward A0 and then A-B).
    AssertRoutes (a2, stubNetworksD,
                  ifA12.GetAddress (0),
                  std::nullopt,
                  "A2 route to D stubs should go toward A1");

    // C0 -> D stubs: should go toward C1 and then C-B (not via A-C).
    AssertRoutes (c0, stubNetworksD,
                  ifC01.GetAddress (1),
                  std::optional<Ipv4Address> (ifAC.GetAddress (0)),
                  "C0 route to D stubs should use C-B (not C-A)");

    // B3 -> D stubs: should go toward B2 (toward B-D).
    AssertRoutes (b3, stubNetworksD,
                  ifB23.GetAddress (0),
                  std::nullopt,
                  "B3 route to D stubs should go toward B2");

    // D0 -> A stubs: should go toward B2 over B-D.
    AssertRoutes (d0, stubNetworksA,
                  ifBD.GetAddress (0),
                  std::nullopt,
                  "D0 route to A stubs should go via B-D");

    // C0 -> A stubs: should go directly via A-C (not via C-B).
    AssertRoutes (c0, stubNetworksA,
                  ifAC.GetAddress (0),
                  std::optional<Ipv4Address> (ifC01.GetAddress (1)),
                  "C0 route to A stubs should use A-C (not C-B)" );

    // B0 -> C stubs: should go toward B1 and then C-B (not over A-B).
    AssertRoutes (b0, stubNetworksC,
                  ifB01.GetAddress (1),
                  std::optional<Ipv4Address> (ifAB.GetAddress (0)),
                  "B0 route to C stubs should use B-C (not B-A)" );

    Simulator::Destroy ();
  }
};

class OspfSpfTestSuite : public TestSuite
{
public:
  OspfSpfTestSuite ()
    : TestSuite ("ospf-spf", UNIT)
  {
    AddTestCase (new OspfL1ShortestPathLinearColdStartTest, TestCase::QUICK);
    AddTestCase (new OspfL1TwoPathShortestHopCountTest, TestCase::QUICK);
    AddTestCase (new OspfL2MultiAreaShortestAreaPathTest, TestCase::QUICK);
  }
};

static OspfSpfTestSuite g_ospfSpfTestSuite;

} // namespace ns3
