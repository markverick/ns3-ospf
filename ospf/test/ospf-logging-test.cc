/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/ospf-app-helper.h"

#include "ospf-test-utils.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace ns3 {

namespace {

using ospf_test_utils::ConfigureFastColdStart;
using ospf_test_utils::ReadAll;

struct LogRunResult
{
  std::filesystem::path outDir;
  uint32_t node0Id;
  uint32_t node1Id;
};

static std::filesystem::path
CreateTempDir (const std::string &tag)
{
  std::error_code ec;
  const auto base = std::filesystem::temp_directory_path (ec);
  if (ec.value () != 0)
    {
      return {};
    }

  std::filesystem::create_directories (base, ec);
  if (ec.value () != 0)
    {
      return {};
    }

  std::filesystem::path tmplPath = base / ("ns3-" + tag + "-XXXXXX");
  std::string tmpl = tmplPath.string ();
  std::vector<char> buf (tmpl.begin (), tmpl.end ());
  buf.push_back ('\0');

  // mkdtemp mutates the template string in-place.
  char *res = ::mkdtemp (buf.data ());
  if (res == nullptr)
    {
      return {};
    }
  return std::filesystem::path (res);
}

static std::vector<std::string>
SplitCsvLine (const std::string &line)
{
  std::vector<std::string> fields;
  std::string cur;
  std::istringstream iss (line);
  while (std::getline (iss, cur, ','))
    {
      fields.push_back (cur);
    }
  return fields;
}

static std::vector<std::string>
ReadLines (const std::filesystem::path &path)
{
  std::string all = ReadAll (path);
  std::vector<std::string> lines;
  std::istringstream iss (all);
  for (std::string line; std::getline (iss, line);)
    {
      if (!line.empty () && line.back () == '\r')
        {
          line.pop_back ();
        }
      if (!line.empty ())
        {
          lines.push_back (line);
        }
    }
  return lines;
}

static LogRunResult
RunTwoNodeOspfWithLogs (bool enablePacketLog, bool includeHello, bool enableLsaTimingLog)
{
  RngSeedManager::SetSeed (123);
  RngSeedManager::SetRun (1);

  const std::filesystem::path outDir = CreateTempDir ("ospf-logging");

  NodeContainer nodes;
  nodes.Create (2);

  InternetStackHelper internet;
  internet.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.252");
  ipv4.Assign (devices);

  OspfAppHelper ospf;
  ConfigureFastColdStart (ospf);
  ospf.SetAttribute ("AreaMask", Ipv4MaskValue (Ipv4Mask ("255.255.255.252")));
  ospf.SetAttribute ("LogDir", StringValue (outDir.string ()));
  ospf.SetAttribute ("EnablePacketLog", BooleanValue (enablePacketLog));
  ospf.SetAttribute ("IncludeHelloInPacketLog", BooleanValue (includeHello));
  ospf.SetAttribute ("EnableLsaTimingLog", BooleanValue (enableLsaTimingLog));

  ApplicationContainer apps = ospf.Install (nodes);
  ospf.ConfigureReachablePrefixesFromInterfaces (nodes);
  apps.Start (Seconds (0.5));
  apps.Stop (Seconds (1.4));

  Simulator::Stop (Seconds (1.6));
  Simulator::Run ();
  Simulator::Destroy ();

  return LogRunResult{outDir, nodes.Get (0)->GetId (), nodes.Get (1)->GetId ()};
}

} // namespace

class OspfPacketLogHelloFilteredByDefaultTestCase : public TestCase
{
public:
  OspfPacketLogHelloFilteredByDefaultTestCase ()
    : TestCase ("Packet log is created and Hello packets can be excluded")
  {
  }

  void
  DoRun () override
  {
    const auto r = RunTwoNodeOspfWithLogs (true /*packet*/, false /*includeHello*/, false /*lsaTiming*/);
    NS_TEST_ASSERT_MSG_EQ (r.outDir.empty (), false, "failed to create temp dir");

    const std::filesystem::path pkt0 = r.outDir / "ospf-packets" / (std::to_string (r.node0Id) + ".csv");
    NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (pkt0), true, "packet log for node0 exists");

    auto lines = ReadLines (pkt0);
    NS_TEST_ASSERT_MSG_EQ (lines.empty (), false, "packet log is non-empty");
    NS_TEST_ASSERT_MSG_EQ (lines.front (), "timestamp,size,type,lsa_level", "packet log header");

    // Hello packets are frequent, but should not be logged when IncludeHelloInPacketLog=false.
    for (size_t i = 1; i < lines.size (); ++i)
      {
        auto fields = SplitCsvLine (lines[i]);
        if (fields.size () >= 3)
          {
            NS_TEST_ASSERT_MSG_NE (fields[2], "1", "Hello packet should be excluded from packet log");
          }
      }
  }
};

class OspfPacketLogIncludesHelloWhenEnabledTestCase : public TestCase
{
public:
  OspfPacketLogIncludesHelloWhenEnabledTestCase ()
    : TestCase ("Packet log records Hello packets when enabled")
  {
  }

  void
  DoRun () override
  {
    const auto r = RunTwoNodeOspfWithLogs (true /*packet*/, true /*includeHello*/, false /*lsaTiming*/);
    NS_TEST_ASSERT_MSG_EQ (r.outDir.empty (), false, "failed to create temp dir");

    const std::filesystem::path pkt0 = r.outDir / "ospf-packets" / (std::to_string (r.node0Id) + ".csv");
    NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (pkt0), true, "packet log for node0 exists");

    auto lines = ReadLines (pkt0);
    NS_TEST_ASSERT_MSG_EQ (lines.empty (), false, "packet log is non-empty");
    NS_TEST_ASSERT_MSG_EQ (lines.front (), "timestamp,size,type,lsa_level", "packet log header");

    bool sawHello = false;
    for (size_t i = 1; i < lines.size (); ++i)
      {
        auto fields = SplitCsvLine (lines[i]);
        if (fields.size () >= 3 && fields[2] == "1")
          {
            sawHello = true;
            break;
          }
      }

    NS_TEST_ASSERT_MSG_EQ (sawHello, true, "expected at least one Hello packet entry");
  }
};

class OspfLsaTimingLogCreatesFilesTestCase : public TestCase
{
public:
  OspfLsaTimingLogCreatesFilesTestCase ()
    : TestCase ("LSA timing log creates expected files and headers")
  {
  }

  void
  DoRun () override
  {
    const auto r = RunTwoNodeOspfWithLogs (false /*packet*/, false /*includeHello*/, true /*lsaTiming*/);
    NS_TEST_ASSERT_MSG_EQ (r.outDir.empty (), false, "failed to create temp dir");

    const std::filesystem::path timing0 = r.outDir / "lsa-timings" / (std::to_string (r.node0Id) + ".csv");
    NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (timing0), true, "lsa timing log for node0 exists");

    auto lines = ReadLines (timing0);
    NS_TEST_ASSERT_MSG_EQ (lines.empty (), false, "lsa timing log is non-empty");
    NS_TEST_ASSERT_MSG_EQ (lines.front (), "timestamp,lsa_key", "lsa timing header");

    const std::filesystem::path mapping = r.outDir / "lsa_mapping.csv";
    NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (mapping), true, "lsa mapping file exists");

    auto mlines = ReadLines (mapping);
    NS_TEST_ASSERT_MSG_EQ (mlines.empty (), false, "lsa mapping file is non-empty");
    NS_TEST_ASSERT_MSG_EQ (mlines.front (), "l1_key,l2_key", "lsa mapping header");
  }
};

class OspfPacketLogLsaLevelPopulatedTestCase : public TestCase
{
public:
  OspfPacketLogLsaLevelPopulatedTestCase ()
    : TestCase ("Packet log has lsa_level populated for LSU and LSAck packets")
  {
  }

  void
  DoRun () override
  {
    const auto r = RunTwoNodeOspfWithLogs (true /*packet*/, false /*includeHello*/, false /*lsaTiming*/);
    NS_TEST_ASSERT_MSG_EQ (r.outDir.empty (), false, "failed to create temp dir");

    const std::filesystem::path pkt0 = r.outDir / "ospf-packets" / (std::to_string (r.node0Id) + ".csv");
    NS_TEST_ASSERT_MSG_EQ (std::filesystem::exists (pkt0), true, "packet log for node0 exists");

    auto lines = ReadLines (pkt0);
    NS_TEST_ASSERT_MSG_EQ (lines.empty (), false, "packet log is non-empty");
    NS_TEST_ASSERT_MSG_EQ (lines.front (), "timestamp,size,type,lsa_level", "packet log header");

    // Check that LSU (type 4) and LSAck (type 5) packets have non-empty lsa_level
    int lsuCount = 0;
    int lsackCount = 0;
    int lsuWithLevel = 0;
    int lsackWithLevel = 0;

    for (size_t i = 1; i < lines.size (); ++i)
      {
        auto fields = SplitCsvLine (lines[i]);
        if (fields.size () >= 4)
          {
            const std::string &packetType = fields[2];
            const std::string &lsaLevel = fields[3];

            if (packetType == "4") // LSU
              {
                lsuCount++;
                if (!lsaLevel.empty () && (lsaLevel == "L1" || lsaLevel == "L2"))
                  {
                    lsuWithLevel++;
                  }
              }
            else if (packetType == "5") // LSAck
              {
                lsackCount++;
                if (!lsaLevel.empty () && (lsaLevel == "L1" || lsaLevel == "L2"))
                  {
                    lsackWithLevel++;
                  }
              }
          }
      }

    // We expect at least some LSU and LSAck packets in a 2-node OSPF simulation
    NS_TEST_ASSERT_MSG_GT (lsuCount, 0, "expected at least one LSU packet");
    NS_TEST_ASSERT_MSG_GT (lsackCount, 0, "expected at least one LSAck packet");

    // All LSU and LSAck packets should have lsa_level populated
    NS_TEST_ASSERT_MSG_EQ (lsuWithLevel, lsuCount,
                           "all LSU packets should have lsa_level (L1 or L2)");
    NS_TEST_ASSERT_MSG_EQ (lsackWithLevel, lsackCount,
                           "all LSAck packets should have lsa_level (L1 or L2)");
  }
};

class OspfLoggingTestSuite : public TestSuite
{
public:
  OspfLoggingTestSuite ()
    : TestSuite ("ospf-logging", UNIT)
  {
    AddTestCase (new OspfPacketLogHelloFilteredByDefaultTestCase, TestCase::QUICK);
    AddTestCase (new OspfPacketLogIncludesHelloWhenEnabledTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaTimingLogCreatesFilesTestCase, TestCase::QUICK);
    AddTestCase (new OspfPacketLogLsaLevelPopulatedTestCase, TestCase::QUICK);
  }
};

static OspfLoggingTestSuite g_ospfLoggingTestSuite;

} // namespace ns3
