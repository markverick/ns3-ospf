/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/ipv4-address.h"
#include "ns3/lsa-header.h"
#include "ns3/ospf-header.h"
#include "ns3/ospf-packet-helper.h"
#include "ns3/packet.h"
#include "ns3/router-lsa.h"

namespace ns3 {

class OspfBigEndianTestCase : public TestCase
{
public:
  OspfBigEndianTestCase ()
    : TestCase ("writeBigEndian and readBigEndian roundtrip")
  {
  }

  void
  DoRun () override
  {
    uint8_t buffer[12] = {0};

    writeBigEndian (buffer, 0, 0x01020304);
    NS_TEST_EXPECT_MSG_EQ (readBigEndian (buffer, 0), 0x01020304u, "roundtrip at offset 0");

    writeBigEndian (buffer, 4, 0xA0B0C0D0);
    NS_TEST_EXPECT_MSG_EQ (readBigEndian (buffer, 4), 0xA0B0C0D0u, "roundtrip at offset 4");
  }
};

class OspfChecksumTestCase : public TestCase
{
public:
  OspfChecksumTestCase ()
    : TestCase ("CalculateChecksum produces expected values")
  {
  }

  void
  DoRun () override
  {
    {
      const uint8_t data[2] = {0x00, 0x01};
      NS_TEST_EXPECT_MSG_EQ (CalculateChecksum (data, 2), 0xFFFEu, "checksum of 0x0001");
    }

    {
      const uint8_t data[1] = {0x01};
      NS_TEST_EXPECT_MSG_EQ (CalculateChecksum (data, 1), 0xFEFFu, "checksum of odd-length buffer");
    }
  }
};

class OspfTtlSeqHelpersTestCase : public TestCase
{
public:
  OspfTtlSeqHelpersTestCase ()
    : TestCase ("CopyAndDecrementTtl and CopyAndIncrementSeqNumber behavior")
  {
  }

  void
  DoRun () override
  {
    {
      // Layout expected by helpers: [0..1]=seqNum, [2..3]=ttl
      const uint8_t payload[4] = {0x00, 0x05, 0x00, 0x02};
      Ptr<Packet> p = Create<Packet> (payload, sizeof (payload));

      Ptr<Packet> dec = CopyAndDecrementTtl (p);
      NS_TEST_EXPECT_MSG_NE (dec, nullptr, "ttl=2 should decrement to 1");

      uint8_t out[4] = {0};
      dec->CopyData (out, sizeof (out));
      NS_TEST_EXPECT_MSG_EQ (static_cast<uint16_t> (out[2] << 8 | out[3]), 1u, "ttl decremented");

      Ptr<Packet> inc = CopyAndIncrementSeqNumber (p);
      NS_TEST_EXPECT_MSG_NE (inc, nullptr, "seq=5 should increment to 6");
      inc->CopyData (out, sizeof (out));
      NS_TEST_EXPECT_MSG_EQ (static_cast<uint16_t> (out[0] << 8 | out[1]), 6u, "seq incremented");
    }

    {
      const uint8_t payload[4] = {0x00, 0x05, 0x00, 0x01};
      Ptr<Packet> p = Create<Packet> (payload, sizeof (payload));
      Ptr<Packet> dec = CopyAndDecrementTtl (p);
      NS_TEST_EXPECT_MSG_EQ (dec, nullptr, "ttl=1 decrements to 0 -> nullptr");
    }
  }
};

class OspfConstructLsuPacketTestCase : public TestCase
{
public:
  OspfConstructLsuPacketTestCase ()
    : TestCase ("ConstructLSUPacket sets OSPF and LSA headers correctly")
  {
  }

  void
  DoRun () override
  {
    const Ipv4Address routerId ("10.1.1.1");
    const uint32_t areaId = 0;
    const uint16_t seqNum = 123;

    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);

    Ptr<Packet> p = ConstructLSUPacket (routerId, areaId, seqNum, routerLsa);

    OspfHeader ospf;
    bool ok = p->RemoveHeader (ospf);
    NS_TEST_EXPECT_MSG_EQ (ok, true, "remove OSPF header");
    NS_TEST_EXPECT_MSG_EQ (ospf.GetType (), OspfHeader::OspfType::OspfLSUpdate, "type");
    NS_TEST_EXPECT_MSG_EQ (ospf.GetRouterId (), routerId.Get (), "router id");
    NS_TEST_EXPECT_MSG_EQ (ospf.GetArea (), areaId, "area id");

    LsaHeader lsa;
    ok = p->RemoveHeader (lsa);
    NS_TEST_EXPECT_MSG_EQ (ok, true, "remove LSA header");
    NS_TEST_EXPECT_MSG_EQ (lsa.GetType (), LsaHeader::LsType::RouterLSAs, "lsa type");
    NS_TEST_EXPECT_MSG_EQ (lsa.GetLsId (), routerId.Get (), "ls id");
    NS_TEST_EXPECT_MSG_EQ (lsa.GetAdvertisingRouter (), routerId.Get (), "adv router");
    NS_TEST_EXPECT_MSG_EQ (lsa.GetSeqNum (), static_cast<uint32_t> (seqNum), "seq");
  }
};

class OspfPacketHelperTestSuite : public TestSuite
{
public:
  OspfPacketHelperTestSuite ()
    : TestSuite ("ospf-packet-helper", UNIT)
  {
    AddTestCase (new OspfBigEndianTestCase, TestCase::QUICK);
    AddTestCase (new OspfChecksumTestCase, TestCase::QUICK);
    AddTestCase (new OspfTtlSeqHelpersTestCase, TestCase::QUICK);
    AddTestCase (new OspfConstructLsuPacketTestCase, TestCase::QUICK);
  }
};

static OspfPacketHelperTestSuite g_ospfPacketHelperTestSuite;

} // namespace ns3
