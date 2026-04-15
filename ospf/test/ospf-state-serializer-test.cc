/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/ospf-app.h"
#include "ns3/ospf-app-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/string.h"

#include "ospf-test-utils.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ns3 {

namespace {

void
IncrementCounter (uint32_t *counter)
{
  ++(*counter);
}

std::vector<uint8_t>
ReadFile (const std::filesystem::path &path)
{
  std::ifstream in (path, std::ios::binary);
  if (!in)
    {
      return {};
    }
  return std::vector<uint8_t> ((std::istreambuf_iterator<char> (in)),
                               std::istreambuf_iterator<char> ());
}

bool
WriteFile (const std::filesystem::path &path, const std::vector<uint8_t> &data)
{
  std::ofstream out (path, std::ios::binary);
  if (!out.good ())
    {
      return false;
    }
  out.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  return out.good ();
}

std::pair<LsaHeader, Ptr<Lsa>>
MakeRouterLsaRecord (Ipv4Address routerId, Ipv4Address neighborId)
{
  Ptr<RouterLsa> routerLsa = Create<RouterLsa> ();
  routerLsa->AddLink (RouterLink (neighborId.Get (), routerId.Get (), 1, 1));

  LsaHeader header (std::make_tuple (LsaHeader::LsType::RouterLSAs,
                                     routerId.Get (),
                                     routerId.Get ()));
  header.SetLength (20 + routerLsa->GetSerializedSize ());
  header.SetSeqNum (1);
  return {header, routerLsa};
}

struct TwoNodeOspfApps
{
  NodeContainer nodes;
  NetDeviceContainer devices;
  ApplicationContainer apps;
  Ptr<OspfApp> src;
  Ptr<OspfApp> dst;
  uint32_t srcIfIndex = 0;
  uint32_t dstIfIndex = 0;
  std::string error;
};

std::filesystem::path
PrepareTempDir (std::filesystem::path dir)
{
  std::error_code ec;
  std::filesystem::remove_all (dir, ec);
  std::filesystem::create_directories (dir, ec);
  NS_ABORT_MSG_IF (ec.value () != 0, "failed to create temp dir");
  return dir;
}

TwoNodeOspfApps
BuildTwoNodeOspfApps (const std::string &base)
{
  TwoNodeOspfApps topology;
  topology.nodes.Create (2);

  InternetStackHelper internet;
  internet.Install (topology.nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  topology.devices = p2p.Install (topology.nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase (base.c_str (), "255.255.255.252");
  ipv4.Assign (topology.devices);

  OspfAppHelper ospf;
  topology.apps = ospf.Install (topology.nodes);
  topology.src = DynamicCast<OspfApp> (topology.apps.Get (0));
  topology.dst = DynamicCast<OspfApp> (topology.apps.Get (1));
  if (topology.src == nullptr)
    {
      topology.error = "expected source OspfApp";
      return topology;
    }
  if (topology.dst == nullptr)
    {
      topology.error = "expected destination OspfApp";
      return topology;
    }

  Ptr<Ipv4> ipv40 = topology.nodes.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv41 = topology.nodes.Get (1)->GetObject<Ipv4> ();
  if (ipv40 == nullptr)
    {
      topology.error = "expected source Ipv4 stack";
      return topology;
    }
  if (ipv41 == nullptr)
    {
      topology.error = "expected destination Ipv4 stack";
      return topology;
    }

  topology.srcIfIndex = ipv40->GetInterfaceForDevice (topology.devices.Get (0));
  topology.dstIfIndex = ipv41->GetInterfaceForDevice (topology.devices.Get (1));
  if (topology.srcIfIndex == static_cast<uint32_t> (-1) ||
      topology.dstIfIndex == static_cast<uint32_t> (-1))
    {
      topology.error = "expected point-to-point devices to be registered with IPv4";
    }
  return topology;
}

std::string
AssertLsdbReplacementScenario (const std::filesystem::path &dir,
                               Ipv4Address srcRouterId,
                               Ipv4Address srcNeighborId,
                               Ipv4Address dstRouterId,
                               Ipv4Address dstNeighborId)
{
  Ptr<OspfApp> src = CreateObject<OspfApp> ();
  Ptr<OspfApp> dst = CreateObject<OspfApp> ();

  src->InjectLsa ({MakeRouterLsaRecord (srcRouterId, srcNeighborId)});
  dst->InjectLsa ({MakeRouterLsaRecord (dstRouterId, dstNeighborId)});

  src->ExportLsdb (dir, "src.lsdb");
  dst->ImportLsdb (dir, "src.lsdb");

  auto lsdb = dst->GetLsdb ();
  if (lsdb.size () != 1u)
    {
      return "imported LSDB should replace previous entries";
    }
  if (lsdb.count (srcRouterId.Get ()) != 1u)
    {
      return "imported LSDB should contain the source router";
    }
  if (lsdb.count (dstRouterId.Get ()) != 0u)
    {
      return "stale local LSDB entries should be removed on import";
    }
  return "";
}

std::string
AssertPrefixReplacementScenario (const std::filesystem::path &dir,
                                 const std::string &base,
                                 Ipv4Address srcNetwork,
                                 Ipv4Address srcGateway,
                                 uint32_t srcMetric,
                                 Ipv4Address dstNetwork,
                                 Ipv4Address dstGateway,
                                 uint32_t dstMetric)
{
  auto topology = BuildTwoNodeOspfApps (base);
  if (!topology.error.empty ())
    {
      Simulator::Destroy ();
      return topology.error;
    }

  topology.src->SetReachableAddresses ({OspfApp::ReachableRoute (topology.srcIfIndex,
                                                                 srcNetwork.Get (),
                                                                 Ipv4Mask ("255.255.255.0").Get (),
                                                                 srcGateway.Get (),
                                                                 srcMetric)});
  topology.dst->SetReachableAddresses ({OspfApp::ReachableRoute (topology.dstIfIndex,
                                                                 dstNetwork.Get (),
                                                                 Ipv4Mask ("255.255.255.0").Get (),
                                                                 dstGateway.Get (),
                                                                 dstMetric)});

  topology.src->ExportPrefixes (dir, "src.prefixes");
  const auto expected = ReadFile (dir / "src.prefixes");

  topology.dst->ImportPrefixes (dir, "src.prefixes");
  topology.dst->ExportPrefixes (dir, "dst.prefixes");
  const auto actual = ReadFile (dir / "dst.prefixes");

  if (actual != expected)
    {
      Simulator::Destroy ();
      return "imported prefixes should replace previous routes, not append to them";
    }
  Simulator::Destroy ();
  return "";
}

std::string
AssertNeighborReplacementScenario (const std::filesystem::path &dir,
                                   const std::string &base,
                                   Ipv4Address srcRouterId,
                                   Ipv4Address srcNeighborIp,
                                   Ipv4Address dstRouterId,
                                   Ipv4Address dstNeighborIp)
{
  auto topology = BuildTwoNodeOspfApps (base);
  if (!topology.error.empty ())
    {
      Simulator::Destroy ();
      return topology.error;
    }

  topology.src->AddNeighbor (topology.srcIfIndex,
                             Create<OspfNeighbor> (srcRouterId,
                                                   srcNeighborIp,
                                                   0,
                                                   OspfNeighbor::Full));
  topology.dst->AddNeighbor (topology.dstIfIndex,
                             Create<OspfNeighbor> (dstRouterId,
                                                   dstNeighborIp,
                                                   0,
                                                   OspfNeighbor::Full));

  topology.src->ExportNeighbors (dir, "src.neighbors");
  const auto expected = ReadFile (dir / "src.neighbors");

  topology.dst->ImportNeighbors (dir, "src.neighbors");
  topology.dst->ExportNeighbors (dir, "dst.neighbors");
  const auto actual = ReadFile (dir / "dst.neighbors");

  if (actual != expected)
    {
      Simulator::Destroy ();
      return "imported neighbors should replace previous neighbors, not append to them";
    }
  Simulator::Destroy ();
  return "";
}

std::string
AssertTopologyReplacementAggregateScenario (const std::filesystem::path &dir,
                                            const std::string &base,
                                            Ipv4Address prefixSrcNetwork,
                                            Ipv4Address prefixSrcGateway,
                                            uint32_t prefixSrcMetric,
                                            Ipv4Address prefixDstNetwork,
                                            Ipv4Address prefixDstGateway,
                                            uint32_t prefixDstMetric,
                                            Ipv4Address neighborSrcRouterId,
                                            Ipv4Address neighborSrcIp,
                                            Ipv4Address neighborDstRouterId,
                                            Ipv4Address neighborDstIp)
{
  auto topology = BuildTwoNodeOspfApps (base);
  if (!topology.error.empty ())
    {
      Simulator::Destroy ();
      return topology.error;
    }

  topology.src->SetReachableAddresses ({OspfApp::ReachableRoute (topology.srcIfIndex,
                                                                 prefixSrcNetwork.Get (),
                                                                 Ipv4Mask ("255.255.255.0").Get (),
                                                                 prefixSrcGateway.Get (),
                                                                 prefixSrcMetric)});
  topology.dst->SetReachableAddresses ({OspfApp::ReachableRoute (topology.dstIfIndex,
                                                                 prefixDstNetwork.Get (),
                                                                 Ipv4Mask ("255.255.255.0").Get (),
                                                                 prefixDstGateway.Get (),
                                                                 prefixDstMetric)});

  topology.src->ExportPrefixes (dir, "src.prefixes");
  const auto expectedPrefixes = ReadFile (dir / "src.prefixes");
  topology.dst->ImportPrefixes (dir, "src.prefixes");
  topology.dst->ExportPrefixes (dir, "dst.prefixes");
  if (ReadFile (dir / "dst.prefixes") != expectedPrefixes)
    {
      Simulator::Destroy ();
      return "imported prefixes should replace previous routes, not append to them";
    }

  topology.src->AddNeighbor (topology.srcIfIndex,
                             Create<OspfNeighbor> (neighborSrcRouterId,
                                                   neighborSrcIp,
                                                   0,
                                                   OspfNeighbor::Full));
  topology.dst->AddNeighbor (topology.dstIfIndex,
                             Create<OspfNeighbor> (neighborDstRouterId,
                                                   neighborDstIp,
                                                   0,
                                                   OspfNeighbor::Full));

  topology.src->ExportNeighbors (dir, "src.neighbors");
  const auto expectedNeighbors = ReadFile (dir / "src.neighbors");
  topology.dst->ImportNeighbors (dir, "src.neighbors");
  topology.dst->ExportNeighbors (dir, "dst.neighbors");
  if (ReadFile (dir / "dst.neighbors") != expectedNeighbors)
    {
      Simulator::Destroy ();
      return "imported neighbors should replace previous neighbors, not append to them";
    }

  Simulator::Destroy ();
  return "";
}

} // namespace

class OspfStateSerializerMetadataRoundTripTestCase : public TestCase
{
public:
  OspfStateSerializerMetadataRoundTripTestCase ()
    : TestCase ("ExportMetadata and ImportMetadata round-trip")
  {
  }

  void
  DoRun () override
  {
    std::filesystem::path dir (CreateTempDirFilename ("ospf-state-serializer-meta"));
    std::error_code ec;
    std::filesystem::remove_all (dir, ec);
    std::filesystem::create_directories (dir, ec);
    NS_TEST_ASSERT_MSG_EQ (ec.value (), 0, "failed to create temp dir");

    Ptr<OspfApp> app = CreateObject<OspfApp> ();

    app->SetAreaLeader (true);
    app->ExportMetadata (dir, "node.meta");

    auto bytes = ReadFile (dir / "node.meta");
    NS_TEST_ASSERT_MSG_EQ (bytes.size (), 4u, "metadata size is 4 bytes");
    NS_TEST_ASSERT_MSG_EQ (bytes[0], 0, "isLeader serialized");
    NS_TEST_ASSERT_MSG_EQ (bytes[1], 0, "isLeader serialized");
    NS_TEST_ASSERT_MSG_EQ (bytes[2], 0, "isLeader serialized");
    NS_TEST_ASSERT_MSG_EQ (bytes[3], 1, "isLeader serialized");

    app->SetAreaLeader (false);
    app->ImportMetadata (dir, "node.meta");

    app->ExportMetadata (dir, "node2.meta");
    auto bytes2 = ReadFile (dir / "node2.meta");
    NS_TEST_ASSERT_MSG_EQ (bytes2.size (), 4u, "metadata size is 4 bytes");
    NS_TEST_ASSERT_MSG_EQ (bytes2[0], 0, "isLeader restored");
    NS_TEST_ASSERT_MSG_EQ (bytes2[1], 0, "isLeader restored");
    NS_TEST_ASSERT_MSG_EQ (bytes2[2], 0, "isLeader restored");
    NS_TEST_ASSERT_MSG_EQ (bytes2[3], 1, "isLeader restored");
  }
};

class OspfStateSerializerImportMetadataTruncatedNoCrashTestCase : public TestCase
{
public:
  OspfStateSerializerImportMetadataTruncatedNoCrashTestCase ()
    : TestCase ("ImportMetadata handles truncation")
  {
  }

  void
  DoRun () override
  {
    std::filesystem::path dir (CreateTempDirFilename ("ospf-state-serializer-meta-trunc"));
    std::error_code ec;
    std::filesystem::remove_all (dir, ec);
    std::filesystem::create_directories (dir, ec);
    NS_TEST_ASSERT_MSG_EQ (ec.value (), 0, "failed to create temp dir");

    NS_TEST_ASSERT_MSG_EQ (WriteFile (dir / "bad.meta", {}), true, "write bad.meta");

    Ptr<OspfApp> app = CreateObject<OspfApp> ();
    app->SetAreaLeader (true);

    app->ImportMetadata (dir, "bad.meta");

    app->ExportMetadata (dir, "out.meta");
    auto out = ReadFile (dir / "out.meta");
    NS_TEST_ASSERT_MSG_EQ (out.size (), 4u, "metadata size is 4 bytes");
    NS_TEST_ASSERT_MSG_EQ (out[0], 0, "truncated import should not change state");
    NS_TEST_ASSERT_MSG_EQ (out[1], 0, "truncated import should not change state");
    NS_TEST_ASSERT_MSG_EQ (out[2], 0, "truncated import should not change state");
    NS_TEST_ASSERT_MSG_EQ (out[3], 1, "truncated import should not change state");
  }
};

class OspfStateSerializerImportPrefixesTruncatedNoCrashTestCase : public TestCase
{
public:
  OspfStateSerializerImportPrefixesTruncatedNoCrashTestCase ()
    : TestCase ("ImportPrefixes handles truncation")
  {
  }

  void
  DoRun () override
  {
    std::filesystem::path dir (CreateTempDirFilename ("ospf-state-serializer-prefix-trunc"));
    std::error_code ec;
    std::filesystem::remove_all (dir, ec);
    std::filesystem::create_directories (dir, ec);
    NS_TEST_ASSERT_MSG_EQ (ec.value (), 0, "failed to create temp dir");

    // routeNum = 1, but no route entries
    NS_TEST_ASSERT_MSG_EQ (WriteFile (dir / "bad.prefixes", std::vector<uint8_t> {0, 0, 0, 1}),
                 true, "write bad.prefixes");

    Ptr<OspfApp> app = CreateObject<OspfApp> ();

    app->ExportPrefixes (dir, "before.prefixes");
    auto before = ReadFile (dir / "before.prefixes");

    app->ImportPrefixes (dir, "bad.prefixes");

    app->ExportPrefixes (dir, "after.prefixes");
    auto after = ReadFile (dir / "after.prefixes");

    NS_TEST_ASSERT_MSG_EQ ((before == after), true,
                           "truncated prefixes import should not mutate routes");
  }
};

class OspfStateSerializerImportNeighborsMismatchNoCrashTestCase : public TestCase
{
public:
  OspfStateSerializerImportNeighborsMismatchNoCrashTestCase ()
    : TestCase ("ImportNeighbors handles interface mismatch")
  {
  }

  void
  DoRun () override
  {
    std::filesystem::path dir (CreateTempDirFilename ("ospf-state-serializer-nei-mismatch"));
    std::error_code ec;
    std::filesystem::remove_all (dir, ec);
    std::filesystem::create_directories (dir, ec);
    NS_TEST_ASSERT_MSG_EQ (ec.value (), 0, "failed to create temp dir");

    // nInterfaces = 0
    NS_TEST_ASSERT_MSG_EQ (WriteFile (dir / "bad.neighbors", std::vector<uint8_t> {0, 0, 0, 0}),
                 true, "write bad.neighbors");

    Ptr<OspfApp> app = CreateObject<OspfApp> ();

    // Should not assert/crash even if interfaces not initialized.
    app->ImportNeighbors (dir, "bad.neighbors");
  }
};

class OspfStateSerializerImportLsdbTruncationNoCrashTestCase : public TestCase
{
public:
  OspfStateSerializerImportLsdbTruncationNoCrashTestCase ()
    : TestCase ("ImportLsdb handles empty and truncated")
  {
  }

  void
  DoRun () override
  {
    std::filesystem::path dir (CreateTempDirFilename ("ospf-state-serializer-lsdb-trunc"));
    std::error_code ec;
    std::filesystem::remove_all (dir, ec);
    std::filesystem::create_directories (dir, ec);
    NS_TEST_ASSERT_MSG_EQ (ec.value (), 0, "failed to create temp dir");

    Ptr<OspfApp> app = CreateObject<OspfApp> ();

    // Empty file
    NS_TEST_ASSERT_MSG_EQ (WriteFile (dir / "empty.lsdb", {}), true, "write empty.lsdb");
    app->ImportLsdb (dir, "empty.lsdb");
    NS_TEST_ASSERT_MSG_EQ (app->GetLsdb ().empty (), true, "router LSDB unchanged");
    NS_TEST_ASSERT_MSG_EQ (app->GetL1SummaryLsdb ().empty (), true, "l1 summary LSDB unchanged");
    NS_TEST_ASSERT_MSG_EQ (app->GetAreaLsdb ().empty (), true, "area LSDB unchanged");
    NS_TEST_ASSERT_MSG_EQ (app->GetL2SummaryLsdb ().empty (), true, "l2 summary LSDB unchanged");

    // Truncated file
    NS_TEST_ASSERT_MSG_EQ (WriteFile (dir / "trunc.lsdb", std::vector<uint8_t> {0x01}), true,
                           "write trunc.lsdb");
    app->ImportLsdb (dir, "trunc.lsdb");
    NS_TEST_ASSERT_MSG_EQ (app->GetLsdb ().empty (), true, "router LSDB unchanged");
    NS_TEST_ASSERT_MSG_EQ (app->GetL1SummaryLsdb ().empty (), true, "l1 summary LSDB unchanged");
    NS_TEST_ASSERT_MSG_EQ (app->GetAreaLsdb ().empty (), true, "area LSDB unchanged");
    NS_TEST_ASSERT_MSG_EQ (app->GetL2SummaryLsdb ().empty (), true, "l2 summary LSDB unchanged");
  }
};

class OspfStateSerializerSparseNeighborExportRegressionTestCase : public TestCase
{
public:
  OspfStateSerializerSparseNeighborExportRegressionTestCase ()
    : TestCase ("ExportNeighbors handles sparse explicit bindings")
  {
  }

  void
  DoRun () override
  {
    std::filesystem::path dir (CreateTempDirFilename ("ospf-state-serializer-sparse-neighbors"));
    std::error_code ec;
    std::filesystem::remove_all (dir, ec);
    std::filesystem::create_directories (dir, ec);
    NS_TEST_ASSERT_MSG_EQ (ec.value (), 0, "failed to create temp dir");

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
    ipv4.SetBase ("10.61.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.61.2.0", "255.255.255.252");
    ipv4.Assign (d12);

    OspfAppHelper ospf;
    ApplicationContainer apps;
    apps.Add (ospf.Install (nodes.Get (0), NetDeviceContainer (d01.Get (0))));
    apps.Add (ospf.Install (nodes.Get (1), NetDeviceContainer (d01.Get (1))));
    apps.Add (ospf.Install (nodes.Get (2), NetDeviceContainer (d12.Get (1))));

    Ptr<OspfApp> app1 = DynamicCast<OspfApp> (apps.Get (1));
    NS_TEST_ASSERT_MSG_NE (app1, nullptr, "expected OspfApp on node1");

    Ptr<Ipv4> ipv41 = nodes.Get (1)->GetObject<Ipv4> ();
    const uint32_t d01If1 = ipv41->GetInterfaceForDevice (d01.Get (1));

    app1->AddNeighbor (d01If1,
                       Create<OspfNeighbor> (Ipv4Address ("10.61.0.1"), Ipv4Address ("10.61.1.1"),
                                             0, OspfNeighbor::Full));

    app1->ExportNeighbors (dir, "node1.neighbors");

    auto bytes = ReadFile (dir / "node1.neighbors");
    NS_TEST_ASSERT_MSG_EQ (bytes.empty (), false,
                           "sparse neighbor export should produce output instead of crashing");

    Simulator::Destroy ();
  }
};

class OspfStateSerializerImportLsdbReplacesStateTestCase : public TestCase
{
public:
  OspfStateSerializerImportLsdbReplacesStateTestCase ()
    : TestCase ("ImportLsdb replaces existing LSDB state")
  {
  }

  void
  DoRun () override
  {
    const auto error =
        AssertLsdbReplacementScenario (PrepareTempDir (CreateTempDirFilename ("ospf-state-serializer-lsdb-replace")),
                                       Ipv4Address ("10.62.0.2"),
                                       Ipv4Address ("10.62.0.1"),
                                       Ipv4Address ("10.62.0.3"),
                                       Ipv4Address ("10.62.0.4"));
    NS_TEST_ASSERT_MSG_EQ (error.empty (), true, error);
  }
};

class OspfStateSerializerImportLsdbCancelsPendingSelfRegenerationTestCase : public TestCase
{
public:
  OspfStateSerializerImportLsdbCancelsPendingSelfRegenerationTestCase ()
    : TestCase ("ImportLsdb cancels pending self-originated LSA regeneration")
  {
  }

  void
  DoRun () override
  {
    auto topology = BuildTwoNodeOspfApps ("10.69.1.0");
    NS_TEST_ASSERT_MSG_EQ (topology.error.empty (), true, topology.error);

    const auto dir =
        PrepareTempDir (CreateTempDirFilename ("ospf-state-serializer-lsdb-pending-replace"));
    const auto routerKey = std::make_tuple (LsaHeader::LsType::RouterLSAs,
                                            topology.dst->GetRouterId ().Get (),
                                            topology.dst->GetRouterId ().Get ());
    uint32_t staleTimeoutsFired = 0;

    OspfAppTestPeer::TrackPendingLsaRegeneration (
        topology.dst,
        routerKey,
        Simulator::Schedule (MilliSeconds (10), &IncrementCounter, &staleTimeoutsFired),
        Simulator::Now ());
    NS_TEST_ASSERT_MSG_EQ (OspfAppTestPeer::HasPendingLsaRegeneration (topology.dst, routerKey),
                           true,
                           "test setup should create a pending self-regeneration event before import");

    topology.src->InjectLsa ({MakeRouterLsaRecord (Ipv4Address ("10.69.0.10"),
                                                   Ipv4Address ("10.69.0.11"))});
    topology.src->ExportLsdb (dir, "src.lsdb");

    topology.dst->ImportLsdb (dir, "src.lsdb");
    NS_TEST_ASSERT_MSG_EQ (OspfAppTestPeer::HasPendingLsaRegeneration (topology.dst, routerKey),
                           false,
                           "ImportLsdb should clear stale pending self-regeneration state");

    Simulator::Stop (MilliSeconds (20));
    Simulator::Run ();

    auto lsdb = topology.dst->GetLsdb ();
    NS_TEST_ASSERT_MSG_EQ (lsdb.count (Ipv4Address ("10.69.0.10").Get ()), 1u,
                           "imported LSDB should retain the imported router after pending events run");
    NS_TEST_ASSERT_MSG_EQ (staleTimeoutsFired, 0u,
                           "cleared pending self-regeneration events must not fire after import");

    Simulator::Destroy ();
  }
};

class OspfStateSerializerImportPrefixesReplacesStateTestCase : public TestCase
{
public:
  OspfStateSerializerImportPrefixesReplacesStateTestCase ()
    : TestCase ("ImportPrefixes replaces existing prefix state")
  {
  }

  void
  DoRun () override
  {
    const auto error =
        AssertPrefixReplacementScenario (PrepareTempDir (CreateTempDirFilename ("ospf-state-serializer-prefix-replace")),
                                         "10.63.0.0",
                                         Ipv4Address ("10.63.1.0"),
                                         Ipv4Address ("10.63.1.1"),
                                         7,
                                         Ipv4Address ("10.63.2.0"),
                                         Ipv4Address ("10.63.2.1"),
                                         3);
    NS_TEST_ASSERT_MSG_EQ (error.empty (), true, error);
  }
};

class OspfStateSerializerImportNeighborsReplacesStateTestCase : public TestCase
{
public:
  OspfStateSerializerImportNeighborsReplacesStateTestCase ()
    : TestCase ("ImportNeighbors replaces existing neighbor state")
  {
  }

  void
  DoRun () override
  {
    const auto error =
        AssertNeighborReplacementScenario (PrepareTempDir (CreateTempDirFilename ("ospf-state-serializer-neighbor-replace")),
                                           "10.64.1.0",
                                           Ipv4Address ("10.64.0.1"),
                                           Ipv4Address ("10.64.1.2"),
                                           Ipv4Address ("10.64.0.2"),
                                           Ipv4Address ("10.64.1.1"));
    NS_TEST_ASSERT_MSG_EQ (error.empty (), true, error);
  }
};

class OspfStateSerializerImportNeighborsCancelsStaleTimeoutsTestCase : public TestCase
{
public:
  OspfStateSerializerImportNeighborsCancelsStaleTimeoutsTestCase ()
    : TestCase ("ImportNeighbors cancels stale neighbor-owned timeout events")
  {
  }

  void
  DoRun () override
  {
    auto topology = BuildTwoNodeOspfApps ("10.66.1.0");
    NS_TEST_ASSERT_MSG_EQ (topology.error.empty (), true, topology.error);

    const auto dir =
        PrepareTempDir (CreateTempDirFilename ("ospf-state-serializer-neighbor-timeout-replace"));

    topology.src->AddNeighbor (topology.srcIfIndex,
                               Create<OspfNeighbor> (Ipv4Address ("10.66.0.1"),
                                                     Ipv4Address ("10.66.1.2"),
                                                     0,
                                                     OspfNeighbor::Full));
    topology.src->ExportNeighbors (dir, "src.neighbors");

    uint32_t staleTimeoutsFired = 0;
    Ptr<OspfNeighbor> staleNeighbor =
        Create<OspfNeighbor> (Ipv4Address ("10.66.0.2"), Ipv4Address ("10.66.1.1"), 0,
                              OspfNeighbor::Full);
    staleNeighbor->BindTimeout (
        Simulator::Schedule (MilliSeconds (10), &IncrementCounter, &staleTimeoutsFired));
    staleNeighbor->BindKeyedTimeout (
        std::make_tuple (LsaHeader::LsType::RouterLSAs, 1u, 1u),
        Simulator::Schedule (MilliSeconds (20), &IncrementCounter, &staleTimeoutsFired));
    topology.dst->AddNeighbor (topology.dstIfIndex, staleNeighbor);

    topology.dst->ImportNeighbors (dir, "src.neighbors");

    Simulator::Stop (MilliSeconds (40));
    Simulator::Run ();

    NS_TEST_ASSERT_MSG_EQ (staleTimeoutsFired, 0u,
                           "neighbor replacement should cancel stale retransmission and timeout events");

    Simulator::Destroy ();
  }
};

class OspfStateSerializerSparseNeighborRoundTripAcrossDifferentHoleLayoutsTestCase : public TestCase
{
public:
  OspfStateSerializerSparseNeighborRoundTripAcrossDifferentHoleLayoutsTestCase ()
    : TestCase ("Sparse neighbor snapshots round-trip across different local hole layouts")
  {
  }

  void
  DoRun () override
  {
    const auto dir =
        PrepareTempDir (CreateTempDirFilename ("ospf-state-serializer-sparse-neighbor-roundtrip"));

    NodeContainer sourceNodes;
    sourceNodes.Create (4);

    InternetStackHelper internet;
    internet.Install (sourceNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer s01 = p2p.Install (NodeContainer (sourceNodes.Get (0), sourceNodes.Get (1)));
    NetDeviceContainer s02 = p2p.Install (NodeContainer (sourceNodes.Get (0), sourceNodes.Get (2)));
    NetDeviceContainer s03 = p2p.Install (NodeContainer (sourceNodes.Get (0), sourceNodes.Get (3)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.67.1.0", "255.255.255.252");
    ipv4.Assign (s01);
    ipv4.SetBase ("10.67.2.0", "255.255.255.252");
    ipv4.Assign (s02);
    ipv4.SetBase ("10.67.3.0", "255.255.255.252");
    ipv4.Assign (s03);

    OspfAppHelper ospf;
    NetDeviceContainer sourceSelected;
    sourceSelected.Add (s01.Get (0));
    sourceSelected.Add (s03.Get (0));
    Ptr<OspfApp> src =
        DynamicCast<OspfApp> (ospf.Install (sourceNodes.Get (0), sourceSelected).Get (0));
    NS_TEST_ASSERT_MSG_NE (src, nullptr, "expected sparse source OspfApp");

    Ptr<Ipv4> srcIpv4 = sourceNodes.Get (0)->GetObject<Ipv4> ();
    const uint32_t srcIfA = srcIpv4->GetInterfaceForDevice (s01.Get (0));
    const uint32_t srcIfB = srcIpv4->GetInterfaceForDevice (s03.Get (0));
    src->AddNeighbor (srcIfA,
                      Create<OspfNeighbor> (Ipv4Address ("10.67.0.1"),
                                            Ipv4Address ("10.67.1.2"),
                                            0,
                                            OspfNeighbor::Full));
    src->AddNeighbor (srcIfB,
                      Create<OspfNeighbor> (Ipv4Address ("10.67.0.2"),
                                            Ipv4Address ("10.67.3.2"),
                                            1,
                                            OspfNeighbor::Full));
    src->ExportNeighbors (dir, "src.neighbors");
    const auto expected = ReadFile (dir / "src.neighbors");

    NodeContainer destNodes;
    destNodes.Create (3);
    internet.Install (destNodes);

    NetDeviceContainer d01 = p2p.Install (NodeContainer (destNodes.Get (0), destNodes.Get (1)));
    NetDeviceContainer d02 = p2p.Install (NodeContainer (destNodes.Get (0), destNodes.Get (2)));

    ipv4.SetBase ("10.68.1.0", "255.255.255.252");
    ipv4.Assign (d01);
    ipv4.SetBase ("10.68.2.0", "255.255.255.252");
    ipv4.Assign (d02);

    NetDeviceContainer destSelected;
    destSelected.Add (d01.Get (0));
    destSelected.Add (d02.Get (0));
    Ptr<OspfApp> dst = DynamicCast<OspfApp> (ospf.Install (destNodes.Get (0), destSelected).Get (0));
    NS_TEST_ASSERT_MSG_NE (dst, nullptr, "expected sparse destination OspfApp");

    dst->ImportNeighbors (dir, "src.neighbors");
    dst->ExportNeighbors (dir, "dst.neighbors");

    const auto actual = ReadFile (dir / "dst.neighbors");
    NS_TEST_ASSERT_MSG_EQ ((actual == expected), true,
                           "neighbor snapshots should be encoded by active OSPF interface order, not sparse holes");

    Simulator::Destroy ();
  }
};

class OspfStateSerializerTestSuite : public TestSuite
{
public:
  OspfStateSerializerTestSuite ()
    : TestSuite ("ospf-state-serializer", Type::UNIT)
  {
    AddTestCase (new OspfStateSerializerMetadataRoundTripTestCase, TestCase::QUICK);
    AddTestCase (new OspfStateSerializerImportMetadataTruncatedNoCrashTestCase,
                 TestCase::QUICK);
    AddTestCase (new OspfStateSerializerImportPrefixesTruncatedNoCrashTestCase,
                 TestCase::QUICK);
    AddTestCase (new OspfStateSerializerImportNeighborsMismatchNoCrashTestCase,
                 TestCase::QUICK);
    AddTestCase (new OspfStateSerializerImportLsdbTruncationNoCrashTestCase, TestCase::QUICK);
  }
};

static OspfStateSerializerTestSuite g_ospfStateSerializerTestSuite;

class OspfStateSerializerSparseRegressionSuite : public TestSuite
{
public:
  OspfStateSerializerSparseRegressionSuite ()
    : TestSuite ("ospf-state-serializer-sparse-regression", Type::UNIT)
  {
    AddTestCase (new OspfStateSerializerSparseNeighborExportRegressionTestCase,
                 TestCase::QUICK);
    AddTestCase (new OspfStateSerializerSparseNeighborRoundTripAcrossDifferentHoleLayoutsTestCase,
                 TestCase::QUICK);
  }
};

static OspfStateSerializerSparseRegressionSuite g_ospfStateSerializerSparseRegressionSuite;

class OspfStateSerializerReplacementRegressionCompatibilityTestCase : public TestCase
{
public:
  OspfStateSerializerReplacementRegressionCompatibilityTestCase ()
    : TestCase ("Aggregate replacement-regression suite name is retained for compatibility")
  {
  }

  void
  DoRun () override
  {
    const auto dir = PrepareTempDir (CreateTempDirFilename ("ospf-state-serializer-replacement-aggregate"));
    const auto lsdbError = AssertLsdbReplacementScenario (dir,
                                                          Ipv4Address ("10.65.0.2"),
                                                          Ipv4Address ("10.65.0.1"),
                                                          Ipv4Address ("10.65.0.3"),
                                                          Ipv4Address ("10.65.0.4"));
    NS_TEST_ASSERT_MSG_EQ (lsdbError.empty (), true, lsdbError);

    const auto topologyError = AssertTopologyReplacementAggregateScenario (dir,
                                                                           "10.65.1.0",
                                                                           Ipv4Address ("10.65.10.0"),
                                                                           Ipv4Address ("10.65.10.1"),
                                                                           7,
                                                                           Ipv4Address ("10.65.20.0"),
                                                                           Ipv4Address ("10.65.20.1"),
                                                                           3,
                                                                           Ipv4Address ("10.65.0.1"),
                                                                           Ipv4Address ("10.65.1.2"),
                                                                           Ipv4Address ("10.65.0.2"),
                                                                           Ipv4Address ("10.65.1.1"));
    NS_TEST_ASSERT_MSG_EQ (topologyError.empty (), true, topologyError);
  }
};

class OspfStateSerializerReplacementRegressionSuite : public TestSuite
{
public:
  OspfStateSerializerReplacementRegressionSuite ()
    : TestSuite ("ospf-state-serializer-replacement-regression", Type::UNIT)
  {
    // Keep the aggregate suite name as a stable target for existing commands,
    // while the real replacement regressions live in the dedicated suites below.
    AddTestCase (new OspfStateSerializerReplacementRegressionCompatibilityTestCase,
                 TestCase::QUICK);
  }
};

static OspfStateSerializerReplacementRegressionSuite g_ospfStateSerializerReplacementRegressionSuite;

class OspfStateSerializerLsdbReplacementRegressionSuite : public TestSuite
{
public:
  OspfStateSerializerLsdbReplacementRegressionSuite ()
    : TestSuite ("ospf-state-serializer-lsdb-replacement-regression", Type::UNIT)
  {
    AddTestCase (new OspfStateSerializerImportLsdbReplacesStateTestCase, TestCase::QUICK);
    AddTestCase (new OspfStateSerializerImportLsdbCancelsPendingSelfRegenerationTestCase,
                 TestCase::QUICK);
  }
};

static OspfStateSerializerLsdbReplacementRegressionSuite g_ospfStateSerializerLsdbReplacementRegressionSuite;

class OspfStateSerializerPrefixesReplacementRegressionSuite : public TestSuite
{
public:
  OspfStateSerializerPrefixesReplacementRegressionSuite ()
    : TestSuite ("ospf-state-serializer-prefixes-replacement-regression", Type::UNIT)
  {
    AddTestCase (new OspfStateSerializerImportPrefixesReplacesStateTestCase, TestCase::QUICK);
  }
};

static OspfStateSerializerPrefixesReplacementRegressionSuite g_ospfStateSerializerPrefixesReplacementRegressionSuite;

class OspfStateSerializerNeighborsReplacementRegressionSuite : public TestSuite
{
public:
  OspfStateSerializerNeighborsReplacementRegressionSuite ()
    : TestSuite ("ospf-state-serializer-neighbors-replacement-regression", Type::UNIT)
  {
    AddTestCase (new OspfStateSerializerImportNeighborsReplacesStateTestCase, TestCase::QUICK);
    AddTestCase (new OspfStateSerializerImportNeighborsCancelsStaleTimeoutsTestCase,
                 TestCase::QUICK);
  }
};

static OspfStateSerializerNeighborsReplacementRegressionSuite g_ospfStateSerializerNeighborsReplacementRegressionSuite;

} // namespace ns3
