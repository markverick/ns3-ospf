/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/ipv4-address.h"
#include "ns3/lsa-header.h"
#include "ns3/ospf-interface.h"
#include "ns3/ospf-neighbor.h"
#include "ns3/simulator.h"

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
  }
};

static OspfNeighborInterfaceTestSuite g_ospfNeighborInterfaceTestSuite;

} // namespace ns3
