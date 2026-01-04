/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/area-lsa.h"
#include "ns3/lsa-header.h"
#include "ns3/l1-summary-lsa.h"
#include "ns3/l2-summary-lsa.h"
#include "ns3/packet.h"
#include "ns3/router-lsa.h"

namespace ns3 {

class OspfLsaHeaderRoundtripTestCase : public TestCase
{
public:
  OspfLsaHeaderRoundtripTestCase ()
    : TestCase ("LsaHeader serialize and deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    LsaHeader in;
    in.SetType (LsaHeader::LsType::RouterLSAs);
    in.SetLength (20);
    in.SetLsId (Ipv4Address ("10.0.0.1").Get ());
    in.SetAdvertisingRouter (Ipv4Address ("10.0.0.2").Get ());
    in.SetSeqNum (0x12345678);

    Ptr<Packet> p = Create<Packet> ();
    p->AddHeader (in);

    LsaHeader out;
    bool ok = p->RemoveHeader (out);
    NS_TEST_EXPECT_MSG_EQ (ok, true, "header should be removable");

    NS_TEST_EXPECT_MSG_EQ (out.GetType (), in.GetType (), "type");
    NS_TEST_EXPECT_MSG_EQ (out.GetLength (), in.GetLength (), "length");
    NS_TEST_EXPECT_MSG_EQ (out.GetLsId (), in.GetLsId (), "ls id");
    NS_TEST_EXPECT_MSG_EQ (out.GetAdvertisingRouter (), in.GetAdvertisingRouter (), "adv router");
    NS_TEST_EXPECT_MSG_EQ (out.GetSeqNum (), in.GetSeqNum (), "seq num");

    // Key helpers should be stable (compare tuple elements explicitly).
    const auto outKey = out.GetKey ();
    const auto inKey = in.GetKey ();
    NS_TEST_EXPECT_MSG_EQ (std::get<0> (outKey), std::get<0> (inKey), "key type");
    NS_TEST_EXPECT_MSG_EQ (std::get<1> (outKey), std::get<1> (inKey), "key lsId");
    NS_TEST_EXPECT_MSG_EQ (std::get<2> (outKey), std::get<2> (inKey), "key adv router");
    NS_TEST_EXPECT_MSG_EQ (LsaHeader::GetKeyString (out.GetKey ()),
                           LsaHeader::GetKeyString (in.GetKey ()),
                           "key string");
  }
};

class OspfLsaHeaderTruncationRobustnessTestCase : public TestCase
{
public:
  OspfLsaHeaderTruncationRobustnessTestCase ()
    : TestCase ("LsaHeader Deserialize handles truncation")
  {
  }

  void
  DoRun () override
  {
    LsaHeader in;
    in.SetType (LsaHeader::LsType::RouterLSAs);
    in.SetLength (20);
    in.SetLsId (Ipv4Address ("10.0.0.1").Get ());
    in.SetAdvertisingRouter (Ipv4Address ("10.0.0.2").Get ());
    in.SetSeqNum (0x12345678);

    Buffer full;
    const uint32_t fullSize = in.GetSerializedSize ();
    full.AddAtEnd (fullSize);
    in.Serialize (full.Begin ());

    std::vector<uint8_t> bytes (fullSize);
    full.Begin ().Read (bytes.data (), bytes.size ());

    for (uint32_t len = 0; len < fullSize; ++len)
      {
        Buffer b;
        b.AddAtEnd (len);
        if (len > 0)
          {
            b.Begin ().Write (bytes.data (), len);
          }
        LsaHeader out;
        const uint32_t consumed = out.Deserialize (b.Begin ());
        NS_TEST_EXPECT_MSG_EQ (consumed, 0u, "truncated header should not deserialize");
      }
  }
};

class OspfRouterLsaRoundtripTestCase : public TestCase
{
public:
  OspfRouterLsaRoundtripTestCase ()
    : TestCase ("RouterLsa ConstructPacket and Deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    Ptr<RouterLsa> in = Create<RouterLsa> (true, false, true);

    // Add a couple of links.
    in->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (), Ipv4Address ("10.1.1.1").Get (), 1, 10));
    in->AddLink (RouterLink (Ipv4Address ("10.1.2.0").Get (), Ipv4Address ("255.255.255.0").Get (), 3, 1));

    Ptr<Packet> payload = in->ConstructPacket ();
    RouterLsa out (payload);

    NS_TEST_EXPECT_MSG_EQ (out.GetBitV (), in->GetBitV (), "bit V");
    NS_TEST_EXPECT_MSG_EQ (out.GetBitE (), in->GetBitE (), "bit E");
    NS_TEST_EXPECT_MSG_EQ (out.GetBitB (), in->GetBitB (), "bit B");

    NS_TEST_EXPECT_MSG_EQ (out.GetNLink (), in->GetNLink (), "link count");

    for (uint16_t i = 0; i < in->GetNLink (); ++i)
      {
        const RouterLink a = in->GetLink (i);
        const RouterLink b = out.GetLink (i);
        NS_TEST_EXPECT_MSG_EQ (a.m_linkId, b.m_linkId, "linkId");
        NS_TEST_EXPECT_MSG_EQ (a.m_linkData, b.m_linkData, "linkData");
        NS_TEST_EXPECT_MSG_EQ (a.m_type, b.m_type, "type");
        NS_TEST_EXPECT_MSG_EQ (a.m_metric, b.m_metric, "metric");
      }

    // Copy() should yield an equivalent object.
    Ptr<RouterLsa> copy = DynamicCast<RouterLsa> (out.Copy ());
    NS_TEST_EXPECT_MSG_NE (copy, nullptr, "copy should be RouterLsa");
    NS_TEST_EXPECT_MSG_EQ (copy->GetNLink (), out.GetNLink (), "copy link count");
  }
};

class OspfAreaLsaRoundtripTestCase : public TestCase
{
public:
  OspfAreaLsaRoundtripTestCase ()
    : TestCase ("AreaLsa ConstructPacket and Deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    Ptr<AreaLsa> in = Create<AreaLsa> ();
    in->AddLink (AreaLink (/*areaId*/ 1, Ipv4Address ("10.0.0.0").Get (), /*metric*/ 10));
    in->AddLink (AreaLink (/*areaId*/ 2, Ipv4Address ("10.1.0.0").Get (), /*metric*/ 20));

    Ptr<Packet> payload = in->ConstructPacket ();
    AreaLsa out (payload);

    NS_TEST_EXPECT_MSG_EQ (out.GetNLink (), in->GetNLink (), "link count");
    for (uint16_t i = 0; i < in->GetNLink (); ++i)
      {
        const AreaLink a = in->GetLink (i);
        const AreaLink b = out.GetLink (i);
        NS_TEST_EXPECT_MSG_EQ (a.m_areaId, b.m_areaId, "areaId");
        NS_TEST_EXPECT_MSG_EQ (a.m_ipAddress, b.m_ipAddress, "ipAddress");
        NS_TEST_EXPECT_MSG_EQ (a.m_metric, b.m_metric, "metric");
      }

    Ptr<AreaLsa> copy = DynamicCast<AreaLsa> (out.Copy ());
    NS_TEST_EXPECT_MSG_NE (copy, nullptr, "copy should be AreaLsa");
    NS_TEST_EXPECT_MSG_EQ (copy->GetNLink (), out.GetNLink (), "copy link count");
  }
};

class OspfLsaAccessorOutOfRangeNoCrashTestCase : public TestCase
{
public:
  OspfLsaAccessorOutOfRangeNoCrashTestCase ()
    : TestCase ("LSA accessors handle out-of-range indices")
  {
  }

  void
  DoRun () override
  {
    // RouterLsa::GetLink
    {
      Ptr<RouterLsa> r = Create<RouterLsa> (true, false, true);
      RouterLink link = r->GetLink (0);
      NS_TEST_ASSERT_MSG_EQ (link.m_linkId, 0u, "default linkId");
      NS_TEST_ASSERT_MSG_EQ (link.m_linkData, 0u, "default linkData");
      NS_TEST_ASSERT_MSG_EQ (link.m_type, 0u, "default type");
      NS_TEST_ASSERT_MSG_EQ (link.m_metric, 0u, "default metric");

      r->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (), Ipv4Address ("10.1.1.1").Get (), 1, 10));
      RouterLink link2 = r->GetLink (1);
      NS_TEST_ASSERT_MSG_EQ (link2.m_linkId, 0u, "default linkId out-of-range");
    }

    // AreaLsa::GetLink
    {
      Ptr<AreaLsa> a = Create<AreaLsa> ();
      AreaLink link = a->GetLink (0);
      NS_TEST_ASSERT_MSG_EQ (link.m_areaId, 0u, "default areaId");
      NS_TEST_ASSERT_MSG_EQ (link.m_ipAddress, 0u, "default ipAddress");
      NS_TEST_ASSERT_MSG_EQ (link.m_metric, 0u, "default metric");

      a->AddLink (AreaLink (/*areaId*/ 1, Ipv4Address ("10.0.0.0").Get (), /*metric*/ 10));
      AreaLink link2 = a->GetLink (1);
      NS_TEST_ASSERT_MSG_EQ (link2.m_areaId, 0u, "default areaId out-of-range");
    }
  }
};

class OspfSummaryLsasRoundtripTestCase : public TestCase
{
public:
  OspfSummaryLsasRoundtripTestCase ()
    : TestCase ("L1SummaryLsa and L2SummaryLsa serialize and deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    // L2 summary
    {
      Ptr<L2SummaryLsa> in = Create<L2SummaryLsa> ();
      in->AddRoute (SummaryRoute (Ipv4Address ("192.0.2.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 1));
      in->AddRoute (SummaryRoute (Ipv4Address ("198.51.100.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 5));

      Ptr<Packet> payload = in->ConstructPacket ();
      L2SummaryLsa out (payload);

      NS_TEST_EXPECT_MSG_EQ (out.GetNRoute (), in->GetNRoute (), "L2 route count");

      const auto inRoutes = in->GetRoutes ();
      const auto outRoutes = out.GetRoutes ();
      auto itA = inRoutes.begin ();
      auto itB = outRoutes.begin ();
      for (; itA != inRoutes.end () && itB != outRoutes.end (); ++itA, ++itB)
        {
          NS_TEST_EXPECT_MSG_EQ (itA->m_address, itB->m_address, "L2 address");
          NS_TEST_EXPECT_MSG_EQ (itA->m_mask, itB->m_mask, "L2 mask");
          NS_TEST_EXPECT_MSG_EQ (itA->m_metric, itB->m_metric, "L2 metric");
        }

      Ptr<L2SummaryLsa> copy = DynamicCast<L2SummaryLsa> (out.Copy ());
      NS_TEST_EXPECT_MSG_NE (copy, nullptr, "copy should be L2SummaryLsa");
      NS_TEST_EXPECT_MSG_EQ (copy->GetNRoute (), out.GetNRoute (), "L2 copy route count");
    }

    // L1 summary
    {
      Ptr<L1SummaryLsa> in = Create<L1SummaryLsa> ();
      in->AddRoute (SummaryRoute (Ipv4Address ("203.0.113.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 2));
      in->AddRoute (SummaryRoute (Ipv4Address ("203.0.114.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 3));

      Ptr<Packet> payload = in->ConstructPacket ();
      L1SummaryLsa out (payload);

      NS_TEST_EXPECT_MSG_EQ (out.GetNRoutes (), in->GetNRoutes (), "L1 route count");

      const auto inRoutes = in->GetRoutes ();
      const auto outRoutes = out.GetRoutes ();
      auto itA = inRoutes.begin ();
      auto itB = outRoutes.begin ();
      for (; itA != inRoutes.end () && itB != outRoutes.end (); ++itA, ++itB)
        {
          NS_TEST_EXPECT_MSG_EQ (itA->m_address, itB->m_address, "L1 address");
          NS_TEST_EXPECT_MSG_EQ (itA->m_mask, itB->m_mask, "L1 mask");
          NS_TEST_EXPECT_MSG_EQ (itA->m_metric, itB->m_metric, "L1 metric");
        }

      Ptr<L1SummaryLsa> copy = DynamicCast<L1SummaryLsa> (out.Copy ());
      NS_TEST_EXPECT_MSG_NE (copy, nullptr, "copy should be L1SummaryLsa");
      NS_TEST_EXPECT_MSG_EQ (copy->GetNRoutes (), out.GetNRoutes (), "L1 copy route count");
    }
  }
};

class OspfLsaTruncationRobustnessTestCase : public TestCase
{
public:
  OspfLsaTruncationRobustnessTestCase ()
    : TestCase ("LSA payload Deserialize handles truncation safely")
  {
  }

  void
  DoRun () override
  {
    // RouterLSA: 4 byte header + 12 bytes per link.
    {
      Ptr<RouterLsa> in = Create<RouterLsa> (true, false, true);
      in->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (), Ipv4Address ("10.1.1.1").Get (), 1, 10));
      in->AddLink (RouterLink (Ipv4Address ("10.1.2.0").Get (), Ipv4Address ("255.255.255.0").Get (), 3, 1));

      Ptr<Packet> payload = in->ConstructPacket ();
      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      // Truncate before fixed header.
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 3);
        RouterLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLink (), 0u, "truncated router lsa yields 0 links");
      }

      // Truncate in the middle of the second link: should keep exactly one link.
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 4 + 12 + 1);
        RouterLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLink (), 1u, "partial trailing router link ignored");
      }
    }

    // AreaLSA: 4 byte header + 12 bytes per link.
    {
      Ptr<AreaLsa> in = Create<AreaLsa> ();
      in->AddLink (AreaLink (/*areaId*/ 1, Ipv4Address ("10.0.0.0").Get (), /*metric*/ 10));
      in->AddLink (AreaLink (/*areaId*/ 2, Ipv4Address ("10.1.0.0").Get (), /*metric*/ 20));

      Ptr<Packet> payload = in->ConstructPacket ();
      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 3);
        AreaLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLink (), 0u, "truncated area lsa yields 0 links");
      }

      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 4 + 12 + 2);
        AreaLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLink (), 1u, "partial trailing area link ignored");
      }
    }

    // L1 summary: 4 byte count + 12 bytes per route.
    {
      Ptr<L1SummaryLsa> in = Create<L1SummaryLsa> ();
      in->AddRoute (SummaryRoute (Ipv4Address ("203.0.113.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 2));
      in->AddRoute (SummaryRoute (Ipv4Address ("203.0.114.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 3));

      Ptr<Packet> payload = in->ConstructPacket ();
      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 3);
        L1SummaryLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNRoutes (), 0u, "truncated L1 summary yields 0 routes");
      }

      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 4 + 12 + 5);
        L1SummaryLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNRoutes (), 1u, "partial trailing L1 route ignored");
      }
    }

    // L2 summary: 4 byte count + 12 bytes per route.
    {
      Ptr<L2SummaryLsa> in = Create<L2SummaryLsa> ();
      in->AddRoute (SummaryRoute (Ipv4Address ("192.0.2.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 1));
      in->AddRoute (SummaryRoute (Ipv4Address ("198.51.100.0").Get (), Ipv4Mask ("255.255.255.0").Get (), 5));

      Ptr<Packet> payload = in->ConstructPacket ();
      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 3);
        L2SummaryLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNRoute (), 0u, "truncated L2 summary yields 0 routes");
      }

      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 4 + 12 + 7);
        L2SummaryLsa out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNRoute (), 1u, "partial trailing L2 route ignored");
      }
    }
  }
};

class OspfLsaSerializationTestSuite : public TestSuite
{
public:
  OspfLsaSerializationTestSuite ()
    : TestSuite ("ospf-lsa-serialization", UNIT)
  {
    AddTestCase (new OspfLsaHeaderRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaHeaderTruncationRobustnessTestCase, TestCase::QUICK);
    AddTestCase (new OspfRouterLsaRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfAreaLsaRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaAccessorOutOfRangeNoCrashTestCase, TestCase::QUICK);
    AddTestCase (new OspfSummaryLsasRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsaTruncationRobustnessTestCase, TestCase::QUICK);
  }
};

static OspfLsaSerializationTestSuite g_ospfLsaSerializationTestSuite;

} // namespace ns3
