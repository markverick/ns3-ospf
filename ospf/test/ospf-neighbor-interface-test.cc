/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-header.h"
#include "ns3/ls-request.h"
#include "ns3/lsa-header.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/ospf-app.h"
#include "ns3/ospf-dbd.h"
#include "ns3/ospf-header.h"
#include "ns3/ospf-interface.h"
#include "ns3/ospf-neighbor.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simulator.h"

#include "ospf-test-utils.h"

namespace ns3 {

namespace {

void
DoNothing ()
{
}

LsaHeader
MakeLsaHeader (LsaHeader::LsType type, Ipv4Address lsId, Ipv4Address advRouter, uint32_t seq)
{
  LsaHeader h;
  h.SetType (type);
  h.SetLsId (lsId.Get ());
  h.SetAdvertisingRouter (advRouter.Get ());
  h.SetSeqNum (seq);
  h.SetLength (h.GetSerializedSize ());
  return h;
}

NetDeviceContainer
CollectNodeDevices (Ptr<Node> node)
{
  NetDeviceContainer devices;
  for (uint32_t i = 0; i < node->GetNDevices (); ++i)
    {
      devices.Add (node->GetDevice (i));
    }
  return devices;
}

} // namespace

class OspfInterfaceNeighborCrudTestCase : public TestCase
{
public:
  OspfInterfaceNeighborCrudTestCase ()
    : TestCase ("OspfInterface Add Get Is RemoveNeighbor")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfInterface> iface = Create<OspfInterface> (Ipv4Address ("10.0.0.1"), Ipv4Mask ("255.255.255.0"),
                                                     /*helloInterval*/ 10, /*dead*/ 40,
                                                     /*area*/ 1, /*metric*/ 10, /*mtu*/ 1500);

    const Ipv4Address rid ("10.0.0.2");
    const Ipv4Address rip ("10.0.0.2");

    NS_TEST_EXPECT_MSG_EQ (iface->IsNeighbor (rid, rip), false, "not neighbor initially");
    NS_TEST_EXPECT_MSG_EQ (iface->GetNeighbor (rid, rip), nullptr, "GetNeighbor initially null");

    Ptr<OspfNeighbor> n = iface->AddNeighbor (rid, rip, /*remoteArea*/ 1, OspfNeighbor::Full);
    NS_TEST_EXPECT_MSG_NE (n, nullptr, "AddNeighbor returns neighbor");

    NS_TEST_EXPECT_MSG_EQ (iface->IsNeighbor (rid, rip), true, "IsNeighbor true after add");
    NS_TEST_EXPECT_MSG_EQ (iface->GetNeighbor (rid, rip), n, "GetNeighbor returns same object");

    NS_TEST_EXPECT_MSG_EQ (iface->RemoveNeighbor (rid, rip), true, "RemoveNeighbor true");
    NS_TEST_EXPECT_MSG_EQ (iface->IsNeighbor (rid, rip), false, "IsNeighbor false after remove");
    NS_TEST_EXPECT_MSG_EQ (iface->GetNeighbor (rid, rip), nullptr, "GetNeighbor null after remove");

    NS_TEST_EXPECT_MSG_EQ (iface->RemoveNeighbor (rid, rip), false, "RemoveNeighbor false when absent");
  }
};

class OspfInterfaceActiveRouterLinksTestCase : public TestCase
{
public:
  OspfInterfaceActiveRouterLinksTestCase ()
    : TestCase ("OspfInterface GetActiveRouterLinks returns expected RouterLink entries")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfInterface> iface = Create<OspfInterface> (Ipv4Address ("10.0.0.1"), Ipv4Mask ("255.255.255.0"),
                                                     /*helloInterval*/ 10, /*dead*/ 40,
                                                     /*area*/ 1, /*metric*/ 7, /*mtu*/ 1500);

    // Full neighbor in same area -> type 1 link to neighbor router ID.
    iface->AddNeighbor (Ipv4Address ("10.0.0.2"), Ipv4Address ("10.0.0.2"), /*remoteArea*/ 1, OspfNeighbor::Full);

    // Full neighbor in different area -> type 5 "inter-area" link keyed by area id.
    iface->AddNeighbor (Ipv4Address ("10.0.0.3"), Ipv4Address ("10.0.0.3"), /*remoteArea*/ 2, OspfNeighbor::Full);

    // Non-Full neighbor should not contribute.
    iface->AddNeighbor (Ipv4Address ("10.0.0.4"), Ipv4Address ("10.0.0.4"), /*remoteArea*/ 1, OspfNeighbor::TwoWay);

    const auto links = iface->GetActiveRouterLinks ();
    NS_TEST_EXPECT_MSG_EQ (links.size (), 2u, "only Full neighbors contribute");

    RouterLink expectedA (Ipv4Address ("10.0.0.2").Get (), Ipv4Address ("10.0.0.1").Get (), 1, 7);
    RouterLink expectedB (/*linkId*/ 2, Ipv4Address ("10.0.0.1").Get (), 5, 7);

    const bool firstMatches = (links[0] == expectedA);
    const bool secondMatches = (links[1] == expectedB);
    NS_TEST_EXPECT_MSG_EQ (firstMatches, true, "first link matches same-area neighbor");
    NS_TEST_EXPECT_MSG_EQ (secondMatches, true, "second link matches inter-area neighbor");
  }
};

class OspfInterfaceActiveRouterLinksEmptyTestCase : public TestCase
{
public:
  OspfInterfaceActiveRouterLinksEmptyTestCase ()
    : TestCase ("OspfInterface GetActiveRouterLinks is empty with no Full neighbors")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfInterface> iface = Create<OspfInterface> (Ipv4Address ("10.0.0.1"), Ipv4Mask ("255.255.255.0"),
                                                     /*helloInterval*/ 10, /*dead*/ 40,
                                                     /*area*/ 1, /*metric*/ 7, /*mtu*/ 1500);

    // Non-Full neighbor should not contribute.
    iface->AddNeighbor (Ipv4Address ("10.0.0.4"), Ipv4Address ("10.0.0.4"), /*remoteArea*/ 1, OspfNeighbor::TwoWay);

    const auto links = iface->GetActiveRouterLinks ();
    NS_TEST_EXPECT_MSG_EQ (links.size (), 0u, "no Full neighbors => no active router links");
  }
};

class OspfNeighborDbdQueueTestCase : public TestCase
{
public:
  OspfNeighborDbdQueueTestCase ()
    : TestCase ("OspfNeighbor DBD queue pop respects MTU")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfNeighbor> n = Create<OspfNeighbor> (Ipv4Address ("10.0.0.2"), Ipv4Address ("10.0.0.2"), 1);

    LsaHeader h1 = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.2"), Ipv4Address ("10.0.0.2"), 1);
    LsaHeader h2 = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.3"), Ipv4Address ("10.0.0.2"), 2);
    LsaHeader h3 = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.4"), Ipv4Address ("10.0.0.2"), 3);

    n->AddDbdQueue (h1);
    n->AddDbdQueue (h2);
    n->AddDbdQueue (h3);

    const uint32_t headerSize = h1.GetSerializedSize ();

    // Effective MTU = mtu-100. Choose mtu so exactly 2 headers fit.
    const uint32_t mtu = 100 + 2 * headerSize;
    const auto popped = n->PopMaxMtuFromDbdQueue (mtu);

    NS_TEST_EXPECT_MSG_EQ (popped.size (), 2u, "pops two headers");
    NS_TEST_EXPECT_MSG_EQ (popped[0].GetLsId (), h1.GetLsId (), "first popped is FIFO");
    NS_TEST_EXPECT_MSG_EQ (popped[1].GetLsId (), h2.GetLsId (), "second popped is FIFO");

    NS_TEST_EXPECT_MSG_EQ (n->IsDbdQueueEmpty (), false, "one entry remains");
    const auto last = n->PopDbdQueue ();
    NS_TEST_EXPECT_MSG_EQ (last.GetLsId (), h3.GetLsId (), "remaining entry is last");
    NS_TEST_EXPECT_MSG_EQ (n->IsDbdQueueEmpty (), true, "queue empty after final pop");

    // Small MTU should pop nothing.
    n->AddDbdQueue (h1);
    const auto none = n->PopMaxMtuFromDbdQueue (/*mtu*/ 100);
    NS_TEST_EXPECT_MSG_EQ (none.size (), 0u, "mtu too small pops nothing");
    NS_TEST_EXPECT_MSG_EQ (n->IsDbdQueueEmpty (), false, "still has entry");

    // Edge case: MTU so that exactly 1 header fits.
    const uint32_t mtuOne = 100 + headerSize;
    const auto one = n->PopMaxMtuFromDbdQueue (mtuOne);
    NS_TEST_EXPECT_MSG_EQ (one.size (), 1u, "mtu fits exactly one header");
    NS_TEST_EXPECT_MSG_EQ (n->IsDbdQueueEmpty (), true, "queue empty after popping the only entry");

    // Edge case: very large MTU pops everything.
    n->AddDbdQueue (h1);
    n->AddDbdQueue (h2);
    n->AddDbdQueue (h3);
    const auto all = n->PopMaxMtuFromDbdQueue (/*mtu*/ 100000);
    NS_TEST_EXPECT_MSG_EQ (all.size (), 3u, "large mtu pops all headers");
    NS_TEST_EXPECT_MSG_EQ (n->IsDbdQueueEmpty (), true, "queue empty after popping all");
  }
};

class OspfNeighborOutdatedKeysAndTimeoutsTestCase : public TestCase
{
public:
  OspfNeighborOutdatedKeysAndTimeoutsTestCase ()
    : TestCase ("OspfNeighbor detects outdated keys and clears keyed timeouts")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfNeighbor> n = Create<OspfNeighbor> (Ipv4Address ("10.0.0.2"), Ipv4Address ("10.0.0.2"), 1);

    // Neighbor's view of LSDB (seq numbers).
    LsaHeader aRemote = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.10"), Ipv4Address ("10.0.0.20"), 10);
    LsaHeader bRemote = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.11"), Ipv4Address ("10.0.0.21"), 5);
    n->InsertLsaKey (aRemote);
    n->InsertLsaKey (bRemote);

    // Local headers are missing nothing but one is outdated.
    LsaHeader aLocal = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.10"), Ipv4Address ("10.0.0.20"), 9);
    LsaHeader bLocal = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.11"), Ipv4Address ("10.0.0.21"), 5);

    NS_TEST_EXPECT_MSG_EQ (n->IsLsaKeyOutdated (aLocal), true, "A is outdated");
    NS_TEST_EXPECT_MSG_EQ (n->IsLsaKeyOutdated (bLocal), false, "B is not outdated");

    // A key never seen should not be considered outdated.
    LsaHeader cLocal = MakeLsaHeader (LsaHeader::RouterLSAs, Ipv4Address ("10.0.0.12"), Ipv4Address ("10.0.0.22"), 1);
    NS_TEST_EXPECT_MSG_EQ (n->IsLsaKeyOutdated (cLocal), false, "unknown key not outdated");

    n->AddOutdatedLsaKeysToQueue ({aLocal, bLocal});
    NS_TEST_EXPECT_MSG_EQ (n->GetLsrQueueSize (), 1u, "one outdated key queued");

    const auto keys = n->PopMaxMtuFromLsrQueue (/*mtu*/ 1500);
    NS_TEST_EXPECT_MSG_EQ (keys.size (), 1u, "popped one key");
    const bool queuedKeyIsA = (keys[0] == aRemote.GetKey ());
    NS_TEST_EXPECT_MSG_EQ (queuedKeyIsA, true, "queued key is A");

    // Timeout clearing: after ClearKeyedTimeouts(), keys should be gone.
    const auto keyA = aRemote.GetKey ();
    const auto keyB = bRemote.GetKey ();

    EventId e1 = Simulator::Schedule (Seconds (1), &DoNothing);
    EventId e2 = Simulator::Schedule (Seconds (2), &DoNothing);
    n->BindKeyedTimeout (keyA, e1);
    n->BindKeyedTimeout (keyB, e2);

    n->ClearKeyedTimeouts ();

    NS_TEST_EXPECT_MSG_EQ (n->RemoveKeyedTimeout (keyA), false, "keyA removed by ClearKeyedTimeouts");
    NS_TEST_EXPECT_MSG_EQ (n->RemoveKeyedTimeout (keyB), false, "keyB removed by ClearKeyedTimeouts");

    Simulator::Destroy ();
  }
};

class OspfNeighborAdjacencySyncResetTestCase : public TestCase
{
public:
  OspfNeighborAdjacencySyncResetTestCase ()
    : TestCase ("OspfNeighbor clears adjacency sync state cleanly")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfNeighbor> n = Create<OspfNeighbor> (Ipv4Address ("10.0.0.2"),
                                                Ipv4Address ("10.0.0.2"),
                                                1,
                                                OspfNeighbor::Exchange);

    LsaHeader h1 = MakeLsaHeader (LsaHeader::RouterLSAs,
                                  Ipv4Address ("10.0.0.2"),
                                  Ipv4Address ("10.0.0.2"),
                                  7);
    LsaHeader h2 = MakeLsaHeader (LsaHeader::L1SummaryLSAs,
                                  Ipv4Address ("10.0.0.3"),
                                  Ipv4Address ("10.0.0.2"),
                                  9);

    n->SetLastDbdSent (Create<OspfDbd> (1500, 0, 0, 0, 1, 1, 11));
    n->SetLastLsrSent (Create<LsRequest> (std::vector<LsaHeader::LsaKey>{h1.GetKey ()}));
    n->AddDbdQueue (h1);
    n->AddDbdQueue (h2);
    n->InsertLsaKey (h1);
    n->AddOutdatedLsaKeysToQueue ({h2});

    NS_TEST_EXPECT_MSG_EQ (n->IsDbdQueueEmpty (), false, "DBD queue seeded");
    NS_TEST_EXPECT_MSG_EQ (n->IsLsrQueueEmpty (), false, "LSR queue seeded");
    NS_TEST_EXPECT_MSG_NE (n->GetLastDbdSent (), nullptr, "last DBD seeded");
    NS_TEST_EXPECT_MSG_NE (n->GetLastLsrSent (), nullptr, "last LSR seeded");
    NS_TEST_EXPECT_MSG_EQ (n->GetLsaKeySeqNum (h1.GetKey ()), 7u, "LSA cache seeded");

    n->ClearAdjacencySyncState ();

    NS_TEST_EXPECT_MSG_EQ (n->IsDbdQueueEmpty (), true, "DBD queue cleared");
    NS_TEST_EXPECT_MSG_EQ (n->IsLsrQueueEmpty (), true, "LSR queue cleared");
    NS_TEST_EXPECT_MSG_EQ (n->GetLastDbdSent (), nullptr, "last DBD cleared");
    NS_TEST_EXPECT_MSG_EQ (n->GetLastLsrSent (), nullptr, "last LSR cleared");
    NS_TEST_EXPECT_MSG_EQ (n->GetLsaKeySeqNum (h1.GetKey ()), 0u, "LSA cache cleared");
  }
};

class OspfNeighborRoleConflictRestartTestCase : public TestCase
{
public:
  OspfNeighborRoleConflictRestartTestCase (const std::string &name,
                                           Ipv4Address localRouterId,
                                           Ipv4Address remoteRouterId,
                                           bool remoteClaimsMaster)
    : TestCase (name),
      m_localRouterId (localRouterId),
      m_remoteRouterId (remoteRouterId),
      m_remoteClaimsMaster (remoteClaimsMaster)
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
    NetDeviceContainer devices = p2p.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);

    Ptr<OspfApp> app = CreateObject<OspfApp> ();
    nodes.Get (0)->AddApplication (app);
      app->SetRouterId (m_localRouterId);
    app->SetArea (7);
    app->SetBoundNetDevices (CollectNodeDevices (nodes.Get (0)));

    Ptr<OspfNeighbor> neighbor =
        Create<OspfNeighbor> (m_remoteRouterId, ifaces.GetAddress (1), 7, OspfNeighbor::Exchange);
    neighbor->SetDDSeqNum (41);

    LsaHeader h1 = MakeLsaHeader (LsaHeader::RouterLSAs,
                                  Ipv4Address ("10.0.0.10"),
                                    m_remoteRouterId,
                                  5);
    LsaHeader h2 = MakeLsaHeader (LsaHeader::L1SummaryLSAs,
                                  Ipv4Address ("10.0.0.20"),
                                    m_remoteRouterId,
                                  6);

      neighbor->SetLastDbdSent (Create<OspfDbd> (1500, 0, 0, 0, 1,
                                                 m_remoteClaimsMaster, 41));
    neighbor->SetLastLsrSent (Create<LsRequest> (std::vector<LsaHeader::LsaKey>{h1.GetKey ()}));
    neighbor->AddDbdQueue (h1);
    neighbor->AddDbdQueue (h2);
    neighbor->InsertLsaKey (h1);
    neighbor->AddOutdatedLsaKeysToQueue ({h2});
    neighbor->BindTimeout (Simulator::Schedule (Seconds (10), &DoNothing));
    neighbor->BindKeyedTimeout (h2.GetKey (), Simulator::Schedule (Seconds (11), &DoNothing));

    app->AddNeighbor (1, neighbor);

    Ptr<OspfDbd> conflictingDbd =
          Create<OspfDbd> (1500, 0, 0, 0, 1, m_remoteClaimsMaster, 41);
    OspfAppTestPeer::ReceiveDbd (*app, 1, ifaces.GetAddress (1), m_remoteRouterId, 7,
                                   conflictingDbd);

    NS_TEST_EXPECT_MSG_EQ (neighbor->GetState (), OspfNeighbor::ExStart,
                           "role conflict should restart negotiation in ExStart");
    NS_TEST_EXPECT_MSG_EQ (neighbor->IsDbdQueueEmpty (), true,
                           "role conflict restart should clear DBD queue");
    NS_TEST_EXPECT_MSG_EQ (neighbor->IsLsrQueueEmpty (), true,
                           "role conflict restart should clear LSR queue");
    NS_TEST_EXPECT_MSG_EQ (neighbor->GetLastDbdSent (), nullptr,
                           "role conflict restart should clear last DBD");
    NS_TEST_EXPECT_MSG_EQ (neighbor->GetLastLsrSent (), nullptr,
                           "role conflict restart should clear last LSR");
    NS_TEST_EXPECT_MSG_EQ (neighbor->GetLsaKeySeqNum (h1.GetKey ()), 0u,
                           "role conflict restart should clear cached neighbor LSA state");
    NS_TEST_EXPECT_MSG_EQ (neighbor->HasKeyedTimeout (h2.GetKey ()), false,
               "role conflict restart should clear keyed retransmission timeouts");
    NS_TEST_EXPECT_MSG_EQ (neighbor->HasTimeout (), false,
               "role conflict restart should clear the neighbor retransmission timeout");

    Simulator::Destroy ();
  }

private:
  Ipv4Address m_localRouterId;
  Ipv4Address m_remoteRouterId;
  bool m_remoteClaimsMaster;
};

class OspfNeighborInterfaceTestSuite : public TestSuite
{
public:
  OspfNeighborInterfaceTestSuite ()
    : TestSuite ("ospf-neighbor-interface", UNIT)
  {
    AddTestCase (new OspfInterfaceNeighborCrudTestCase, TestCase::QUICK);
    AddTestCase (new OspfInterfaceActiveRouterLinksTestCase, TestCase::QUICK);
    AddTestCase (new OspfInterfaceActiveRouterLinksEmptyTestCase, TestCase::QUICK);
    AddTestCase (new OspfNeighborDbdQueueTestCase, TestCase::QUICK);
    AddTestCase (new OspfNeighborOutdatedKeysAndTimeoutsTestCase, TestCase::QUICK);
    AddTestCase (new OspfNeighborAdjacencySyncResetTestCase, TestCase::QUICK);
      AddTestCase (new OspfNeighborRoleConflictRestartTestCase (
                       "OspfNeighbor restarts adjacency negotiation on master-master DBD conflict",
                       Ipv4Address ("10.0.0.2"), Ipv4Address ("10.0.0.1"), true),
                   TestCase::QUICK);
      AddTestCase (new OspfNeighborRoleConflictRestartTestCase (
                       "OspfNeighbor restarts adjacency negotiation on slave-slave DBD conflict",
                       Ipv4Address ("10.0.0.1"), Ipv4Address ("10.0.0.2"), false),
                   TestCase::QUICK);
  }
};

static OspfNeighborInterfaceTestSuite g_ospfNeighborInterfaceTestSuite;

} // namespace ns3
