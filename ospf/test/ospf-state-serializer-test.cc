/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/ospf-app.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ns3 {

namespace {

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

} // namespace ns3
