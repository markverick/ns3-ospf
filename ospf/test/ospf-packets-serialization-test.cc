/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include <cstring>
#include <vector>

#include "ns3/buffer.h"
#include "ns3/ipv4-address.h"
#include "ns3/lsa-header.h"
#include "ns3/ls-ack.h"
#include "ns3/ls-request.h"
#include "ns3/ls-update.h"
#include "ns3/ospf-dbd.h"
#include "ns3/ospf-header.h"
#include "ns3/ospf-hello.h"
#include "ns3/packet.h"
#include "ns3/router-lsa.h"

namespace ns3 {

class OspfHeaderRoundtripTestCase : public TestCase
{
public:
  OspfHeaderRoundtripTestCase ()
    : TestCase ("OspfHeader serialize and deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    OspfHeader in;
    in.SetType (OspfHeader::OspfType::OspfHello);
    in.SetPayloadSize (42);
    in.SetRouterId (Ipv4Address ("10.0.0.1").Get ());
    in.SetArea (123);

    Ptr<Packet> p = Create<Packet> (in.GetPayloadSize ());
    p->AddHeader (in);

    OspfHeader out;
    bool ok = p->RemoveHeader (out);
    NS_TEST_EXPECT_MSG_EQ (ok, true, "remove header");

    NS_TEST_EXPECT_MSG_EQ (out.GetType (), in.GetType (), "type");
    NS_TEST_EXPECT_MSG_EQ (out.GetPayloadSize (), in.GetPayloadSize (), "payload size");
    NS_TEST_EXPECT_MSG_EQ (out.GetRouterId (), in.GetRouterId (), "router id");
    NS_TEST_EXPECT_MSG_EQ (out.GetArea (), in.GetArea (), "area");
  }
};

class OspfAccessorOutOfRangeNoCrashTestCase : public TestCase
{
public:
  OspfAccessorOutOfRangeNoCrashTestCase ()
    : TestCase ("Packet accessors handle out-of-range indices")
  {
  }

  void
  DoRun () override
  {
    // LsRequest::GetLsaKey
    {
      LsRequest r;
      auto k = r.GetLsaKey (0);
      NS_TEST_ASSERT_MSG_EQ (std::get<0> (k), static_cast<uint8_t> (LsaHeader::RouterLSAs),
                             "default key type");
      NS_TEST_ASSERT_MSG_EQ (std::get<1> (k), 0u, "default key lsId");
      NS_TEST_ASSERT_MSG_EQ (std::get<2> (k), 0u, "default key advRouter");
    }

    // LsAck::GetLsaHeader
    {
      LsAck ack;
      LsaHeader h = ack.GetLsaHeader (0);
      NS_TEST_ASSERT_MSG_EQ (h.GetLength (), 0u, "default header length");
      NS_TEST_ASSERT_MSG_EQ (h.GetLsId (), 0u, "default header lsId");
      NS_TEST_ASSERT_MSG_EQ (h.GetAdvertisingRouter (), 0u, "default header advRouter");
    }

    // OspfDbd::GetLsaHeader
    {
      OspfDbd dbd;
      LsaHeader h = dbd.GetLsaHeader (0);
      NS_TEST_ASSERT_MSG_EQ (h.GetLength (), 0u, "default header length");
      NS_TEST_ASSERT_MSG_EQ (h.GetLsId (), 0u, "default header lsId");
      NS_TEST_ASSERT_MSG_EQ (h.GetAdvertisingRouter (), 0u, "default header advRouter");
    }

    // OspfHello::GetNeighbor
    {
      Ptr<OspfHello> hello = Create<OspfHello> (Ipv4Mask ("255.255.255.0").Get (), 10, 40);
      NS_TEST_ASSERT_MSG_EQ (hello->GetNeighbor (0), 0u, "default neighbor id on empty list");
      hello->AddNeighbor (Ipv4Address ("10.0.0.2").Get ());
      NS_TEST_ASSERT_MSG_EQ (hello->GetNeighbor (1), 0u, "default neighbor id on out-of-range");
    }
  }
};

class OspfHeaderTruncationRobustnessTestCase : public TestCase
{
public:
  OspfHeaderTruncationRobustnessTestCase ()
    : TestCase ("OspfHeader Deserialize handles truncation and invalid length")
  {
  }

  void
  DoRun () override
  {
    OspfHeader in;
    in.SetType (OspfHeader::OspfType::OspfHello);
    in.SetPayloadSize (42);
    in.SetRouterId (Ipv4Address ("10.0.0.1").Get ());
    in.SetArea (123);

    Buffer full;
    const uint32_t fullSize = in.GetSerializedSize ();
    full.AddAtEnd (fullSize);
    in.Serialize (full.Begin ());

    std::vector<uint8_t> bytes (fullSize);
    full.Begin ().Read (bytes.data (), bytes.size ());

    // Truncation sweep: should never crash; for short buffers Deserialize returns 0.
    for (uint32_t len = 0; len < fullSize; ++len)
      {
        Buffer b;
        b.AddAtEnd (len);
        if (len > 0)
          {
            b.Begin ().Write (bytes.data (), len);
          }
        OspfHeader out;
        const uint32_t consumed = out.Deserialize (b.Begin ());
        NS_TEST_EXPECT_MSG_EQ (consumed, 0u, "truncated header should not deserialize");
      }

    // Invalid declared length (smaller than header size) should be rejected.
    std::vector<uint8_t> bad = bytes;
    // Length field is at bytes[2..3] in network order.
    bad[2] = 0;
    bad[3] = 0;
    Buffer b;
    b.AddAtEnd (bad.size ());
    b.Begin ().Write (bad.data (), bad.size ());
    OspfHeader out;
    const uint32_t consumed = out.Deserialize (b.Begin ());
    NS_TEST_EXPECT_MSG_EQ (consumed, 0u, "invalid declared length rejected");

    // Invalid declared length (larger than available bytes) should be rejected.
    {
      std::vector<uint8_t> tooLong = bytes;
      const uint16_t declared = static_cast<uint16_t> (fullSize + 1);
      tooLong[2] = static_cast<uint8_t> ((declared >> 8) & 0xff);
      tooLong[3] = static_cast<uint8_t> (declared & 0xff);

      Buffer bb;
      bb.AddAtEnd (tooLong.size ());
      bb.Begin ().Write (tooLong.data (), tooLong.size ());

      OspfHeader out2;
      const uint32_t consumed2 = out2.Deserialize (bb.Begin ());
      NS_TEST_EXPECT_MSG_EQ (consumed2, 0u, "declared length beyond buffer rejected");
    }
  }
};

class OspfHelloRoundtripTestCase : public TestCase
{
public:
  OspfHelloRoundtripTestCase ()
    : TestCase ("OspfHello ConstructPacket and Deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfHello> in = Create<OspfHello> (Ipv4Mask ("255.255.255.0").Get (), 10, 40);
    in->SetOptions (2);
    in->SetRouterPriority (1);
    in->SetDesignatedRouter (Ipv4Address ("10.0.0.9").Get ());
    in->SetBackupDesignatedRouter (Ipv4Address ("10.0.0.8").Get ());
    in->AddNeighbor (Ipv4Address ("10.0.0.2").Get ());
    in->AddNeighbor (Ipv4Address ("10.0.0.3").Get ());

    Ptr<Packet> payload = in->ConstructPacket ();
    OspfHello out (payload);

    NS_TEST_EXPECT_MSG_EQ (out.GetMask (), in->GetMask (), "mask");
    NS_TEST_EXPECT_MSG_EQ (out.GetHelloInterval (), in->GetHelloInterval (), "hello interval");
    NS_TEST_EXPECT_MSG_EQ (out.GetRouterDeadInterval (), in->GetRouterDeadInterval (), "dead interval");
    NS_TEST_EXPECT_MSG_EQ (out.GetOptions (), in->GetOptions (), "options");
    NS_TEST_EXPECT_MSG_EQ (out.GetRouterPriority (), in->GetRouterPriority (), "priority");
    NS_TEST_EXPECT_MSG_EQ (out.GetDesignatedRouter (), in->GetDesignatedRouter (), "dr");
    NS_TEST_EXPECT_MSG_EQ (out.GetBackupDesignatedRouter (), in->GetBackupDesignatedRouter (), "bdr");

    NS_TEST_EXPECT_MSG_EQ (out.GetNNeighbors (), in->GetNNeighbors (), "neighbors count");
    for (uint32_t i = 0; i < in->GetNNeighbors (); ++i)
      {
        NS_TEST_EXPECT_MSG_EQ (out.GetNeighbor (i), in->GetNeighbor (i), "neighbor id");
      }
  }
};

class OspfDbdRoundtripTestCase : public TestCase
{
public:
  OspfDbdRoundtripTestCase ()
    : TestCase ("OspfDbd ConstructPacket and Deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    OspfDbd in;
    in.SetMtu (1500);
    in.SetOptions (7);
    in.SetBitI (true);
    in.SetBitM (true);
    in.SetBitMS (false);
    in.SetDDSeqNum (0xABCDEF01);

    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.2").Get ());
    h.SetSeqNum (100);
    h.SetLength (20);
    in.AddLsaHeader (h);

    Ptr<Packet> payload = in.ConstructPacket ();
    OspfDbd out (payload);

    NS_TEST_EXPECT_MSG_EQ (out.GetMtu (), in.GetMtu (), "mtu");
    NS_TEST_EXPECT_MSG_EQ (out.GetOptions (), in.GetOptions (), "options");
    NS_TEST_EXPECT_MSG_EQ (out.GetBitI (), in.GetBitI (), "I bit");
    NS_TEST_EXPECT_MSG_EQ (out.GetBitM (), in.GetBitM (), "M bit");
    NS_TEST_EXPECT_MSG_EQ (out.GetBitMS (), in.GetBitMS (), "MS bit");
    NS_TEST_EXPECT_MSG_EQ (out.GetDDSeqNum (), in.GetDDSeqNum (), "dd seq");

    NS_TEST_EXPECT_MSG_EQ (out.GetNLsaHeaders (), in.GetNLsaHeaders (), "lsa header count");
    if (out.GetNLsaHeaders () > 0)
      {
        const LsaHeader a = out.GetLsaHeader (0);
        NS_TEST_EXPECT_MSG_EQ (a.GetType (), h.GetType (), "lsa type");
        NS_TEST_EXPECT_MSG_EQ (a.GetLsId (), h.GetLsId (), "lsa id");
        NS_TEST_EXPECT_MSG_EQ (a.GetAdvertisingRouter (), h.GetAdvertisingRouter (), "lsa adv");
        NS_TEST_EXPECT_MSG_EQ (a.GetSeqNum (), h.GetSeqNum (), "lsa seq");
      }
  }
};

class OspfLsAckRoundtripTestCase : public TestCase
{
public:
  OspfLsAckRoundtripTestCase ()
    : TestCase ("LsAck ConstructPacket and Deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.0.0.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.0.0.2").Get ());
    h.SetSeqNum (1);
    h.SetLength (20);

    Ptr<LsAck> in = Create<LsAck> (std::vector<LsaHeader>{h});
    Ptr<Packet> payload = in->ConstructPacket ();

    LsAck out (payload);
    NS_TEST_EXPECT_MSG_EQ (out.GetNLsaHeaders (), 1u, "lsa header count");
    const LsaHeader outH = out.GetLsaHeader (0);
    NS_TEST_EXPECT_MSG_EQ (outH.GetLsId (), h.GetLsId (), "ls id");
    NS_TEST_EXPECT_MSG_EQ (outH.GetAdvertisingRouter (), h.GetAdvertisingRouter (), "adv router");
  }
};

class OspfLsRequestRoundtripTestCase : public TestCase
{
public:
  OspfLsRequestRoundtripTestCase ()
    : TestCase ("LsRequest ConstructPacket and Deserialize roundtrip")
  {
  }

  void
  DoRun () override
  {
    const LsaHeader::LsaKey k1 =
        std::make_tuple (static_cast<uint8_t> (LsaHeader::LsType::RouterLSAs),
                         Ipv4Address ("10.0.0.1").Get (), Ipv4Address ("10.0.0.2").Get ());
    const LsaHeader::LsaKey k2 =
        std::make_tuple (static_cast<uint8_t> (LsaHeader::LsType::AreaLSAs),
                         Ipv4Address ("10.0.1.1").Get (), Ipv4Address ("10.0.1.2").Get ());

    Ptr<LsRequest> in = Create<LsRequest> (std::vector<LsaHeader::LsaKey>{k1, k2});
    Ptr<Packet> payload = in->ConstructPacket ();

    LsRequest out (payload);
    NS_TEST_EXPECT_MSG_EQ (out.GetNLsaKeys (), 2u, "key count");

    const auto outK1 = out.GetLsaKey (0);
    NS_TEST_EXPECT_MSG_EQ (std::get<0> (outK1), std::get<0> (k1), "key type");
    NS_TEST_EXPECT_MSG_EQ (std::get<1> (outK1), std::get<1> (k1), "key lsId");
    NS_TEST_EXPECT_MSG_EQ (std::get<2> (outK1), std::get<2> (k1), "key adv");
  }
};

class OspfLsUpdateRoundtripTestCase : public TestCase
{
public:
  OspfLsUpdateRoundtripTestCase ()
    : TestCase ("LsUpdate ConstructPacket and Deserialize basic roundtrip")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);
    routerLsa->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (), Ipv4Address ("10.1.1.1").Get (), 1, 1));

    // Intentionally do NOT set LsaHeader length here.
    // LsUpdate should defensively set it based on header+payload sizes.
    const uint16_t expectedLength =
      static_cast<uint16_t> (h.GetSerializedSize () + routerLsa->GetSerializedSize ());

    Ptr<LsUpdate> in = Create<LsUpdate> ();
    in->AddLsa (h, routerLsa);

    Ptr<Packet> payload = in->ConstructPacket ();
    LsUpdate out (payload);

    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 1u, "lsa count");
    const auto list = out.GetLsaList ();
    NS_TEST_EXPECT_MSG_EQ (list.size (), 1u, "lsa list size");
    NS_TEST_EXPECT_MSG_EQ (list[0].first.GetType (), h.GetType (), "lsa header type");
    NS_TEST_EXPECT_MSG_EQ (list[0].first.GetLsId (), h.GetLsId (), "lsa header id");
    NS_TEST_EXPECT_MSG_EQ (list[0].first.GetLength (), expectedLength, "lsa header length");
  }
};

class OspfLsUpdateDeclaredLengthMismatchTestCase : public TestCase
{
public:
  OspfLsUpdateDeclaredLengthMismatchTestCase ()
    : TestCase ("LsUpdate Deserialize handles declared length mismatch")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    // Use a minimal RouterLSA payload (0 links), so payload size is 4.
    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);

    const uint32_t headerSize = h.GetSerializedSize ();
    const uint32_t payloadSize = routerLsa->GetSerializedSize ();
    const uint32_t expectedLength = headerSize + payloadSize;

    // Deliberately claim a payload 4 bytes longer than it really is.
    const uint32_t declaredLength = expectedLength + 4;
    h.SetLength (static_cast<uint16_t> (declaredLength));

    const uint32_t totalSize = 4 + headerSize + declaredLength - headerSize; // count + header + declared payload
    Buffer buffer;
    buffer.AddAtStart (totalSize);
    Buffer::Iterator it = buffer.Begin ();
    it.WriteHtonU32 (1);
    h.Serialize (it);
    it.Next (headerSize);
    routerLsa->Serialize (it);
    it.Next (payloadSize);
    // Pad the extra bytes so the declared length is actually present.
    it.WriteU8 (0);
    it.WriteU8 (0);
    it.WriteU8 (0);
    it.WriteU8 (0);

    Ptr<Packet> p = Create<Packet> (buffer.PeekData (), totalSize);
    LsUpdate out (p);

    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 1u, "lsa count");
    const auto list = out.GetLsaList ();
    NS_TEST_EXPECT_MSG_EQ (list.size (), 1u, "lsa list size");
    // Implementation should canonicalize the header length to the computed expected length.
    NS_TEST_EXPECT_MSG_EQ (list[0].first.GetLength (), expectedLength, "canonical header length");
  }
};

class OspfOtherPacketsTruncationRobustnessTestCase : public TestCase
{
public:
  OspfOtherPacketsTruncationRobustnessTestCase ()
    : TestCase ("Hello DBD LSAck LSRequest Deserialize handles truncation safely")
  {
  }

  void
  DoRun () override
  {
    // Hello: fixed header is 20 bytes, then 4 bytes per neighbor.
    {
      Ptr<OspfHello> in = Create<OspfHello> (Ipv4Mask ("255.255.255.0").Get (), 10, 40);
      in->AddNeighbor (Ipv4Address ("10.0.0.2").Get ());
      in->AddNeighbor (Ipv4Address ("10.0.0.3").Get ());
      Ptr<Packet> payload = in->ConstructPacket ();

      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      // Truncate inside fixed header.
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 19);
        OspfHello out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNNeighbors (), 0u, "truncated hello yields 0 neighbors");
      }

      // Truncate leaving one full neighbor plus a partial neighbor.
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 20 + 4 + 1);
        OspfHello out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNNeighbors (), 1u, "partial trailing neighbor is ignored");
      }
    }

    // DBD: fixed 8 bytes, then 20 bytes per LsaHeader.
    {
      OspfDbd in;
      in.SetMtu (1500);
      in.SetOptions (7);
      in.SetBitI (true);
      in.SetBitM (true);
      in.SetBitMS (false);
      in.SetDDSeqNum (0xABCDEF01);

      LsaHeader h;
      h.SetType (LsaHeader::LsType::RouterLSAs);
      h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
      h.SetAdvertisingRouter (Ipv4Address ("10.1.1.2").Get ());
      h.SetSeqNum (100);
      h.SetLength (20);
      in.AddLsaHeader (h);
      in.AddLsaHeader (h);

      Ptr<Packet> payload = in.ConstructPacket ();
      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      // Truncate inside fixed header.
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 7);
        OspfDbd out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLsaHeaders (), 0u, "truncated dbd yields 0 headers");
      }

      // Truncate in the middle of the second LSA header.
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 8 + 20 + 3);
        OspfDbd out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLsaHeaders (), 1u, "partial trailing LSA header ignored");
      }
    }

    // LSAck: 20 bytes per LsaHeader.
    {
      LsaHeader h;
      h.SetType (LsaHeader::LsType::RouterLSAs);
      h.SetLsId (Ipv4Address ("10.0.0.1").Get ());
      h.SetAdvertisingRouter (Ipv4Address ("10.0.0.2").Get ());
      h.SetSeqNum (1);
      h.SetLength (20);

      Ptr<LsAck> in = Create<LsAck> (std::vector<LsaHeader>{h, h});
      Ptr<Packet> payload = in->ConstructPacket ();
      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 20 + 2);
        LsAck out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLsaHeaders (), 1u, "partial trailing ack header ignored");
      }
    }

    // LSRequest: 12 bytes per LsaKey.
    {
      const LsaHeader::LsaKey k1 =
          std::make_tuple (static_cast<uint8_t> (LsaHeader::LsType::RouterLSAs),
                           Ipv4Address ("10.0.0.1").Get (), Ipv4Address ("10.0.0.2").Get ());
      const LsaHeader::LsaKey k2 =
          std::make_tuple (static_cast<uint8_t> (LsaHeader::LsType::AreaLSAs),
                           Ipv4Address ("10.0.1.1").Get (), Ipv4Address ("10.0.1.2").Get ());

      Ptr<LsRequest> in = Create<LsRequest> (std::vector<LsaHeader::LsaKey>{k1, k2});
      Ptr<Packet> payload = in->ConstructPacket ();
      std::vector<uint8_t> bytes (payload->GetSize ());
      payload->CopyData (bytes.data (), bytes.size ());

      // Truncate mid-key: should parse exactly one key.
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), 12 + 1);
        LsRequest out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLsaKeys (), 1u, "partial trailing request key ignored");
      }

      // Empty buffer should parse 0 keys.
      {
        Ptr<Packet> p = Create<Packet> (reinterpret_cast<const uint8_t *> (""), 0);
        LsRequest out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLsaKeys (), 0u, "empty request yields 0 keys");
      }

      // Unsupported/out-of-range type should be ignored (no crash).
      {
        Buffer buf;
        buf.AddAtStart (24);
        auto it = buf.Begin ();

        // First key: out-of-range LS type (implementation stores uint8_t types).
        it.WriteHtonU32 (0x12345678);
        it.WriteHtonU32 (Ipv4Address ("10.9.9.9").Get ());
        it.WriteHtonU32 (Ipv4Address ("10.9.9.8").Get ());

        // Second key: valid RouterLSAs.
        it.WriteHtonU32 (static_cast<uint32_t> (LsaHeader::LsType::RouterLSAs));
        it.WriteHtonU32 (Ipv4Address ("10.0.0.1").Get ());
        it.WriteHtonU32 (Ipv4Address ("10.0.0.2").Get ());

        Ptr<Packet> p = Create<Packet> (buf.PeekData (), 24);
        LsRequest out (p);
        NS_TEST_EXPECT_MSG_EQ (out.GetNLsaKeys (), 1u, "invalid type key ignored");
        const auto outK = out.GetLsaKey (0);
        NS_TEST_EXPECT_MSG_EQ (std::get<0> (outK), static_cast<uint8_t> (LsaHeader::LsType::RouterLSAs),
                               "valid key type preserved");
      }
    }
  }
};

class OspfLsUpdateTruncatedPayloadTestCase : public TestCase
{
public:
  OspfLsUpdateTruncatedPayloadTestCase ()
    : TestCase ("LsUpdate Deserialize handles truncated payload")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);
    routerLsa->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (),
                                   Ipv4Address ("10.1.1.1").Get (),
                                   1,
                                   1));

    Ptr<LsUpdate> in = Create<LsUpdate> ();
    in->AddLsa (h, routerLsa);

    Ptr<Packet> full = in->ConstructPacket ();
    const uint32_t fullSize = full->GetSize ();
    NS_TEST_EXPECT_MSG_GT (fullSize, 1u, "sanity: non-empty LSU");

    std::vector<uint8_t> bytes (fullSize);
    full->CopyData (bytes.data (), fullSize);

    // Remove one byte from the end to simulate truncation.
    Ptr<Packet> truncated = Create<Packet> (bytes.data (), fullSize - 1);
    LsUpdate out (truncated);

    // The key requirement: no crash. A truncated LSU should not yield any LSAs.
    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 0u, "truncated LSU should parse zero LSAs");
  }
};

class OspfLsUpdateMutationAfterAddTestCase : public TestCase
{
public:
  OspfLsUpdateMutationAfterAddTestCase ()
    : TestCase ("LsUpdate ConstructPacket stays consistent if LSA mutates after AddLsa")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    // Start with a small RouterLSA.
    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);

    Ptr<LsUpdate> in = Create<LsUpdate> ();
    in->AddLsa (h, routerLsa);

    // Mutate the payload after AddLsa() to increase its serialized size.
    routerLsa->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (),
                                   Ipv4Address ("10.1.1.1").Get (),
                                   1,
                                   1));

    const uint16_t expectedLength = static_cast<uint16_t> (
        h.GetSerializedSize () + routerLsa->GetSerializedSize ());

    Ptr<Packet> payload = in->ConstructPacket ();
    LsUpdate out (payload);

    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 1u, "lsa count");
    const auto list = out.GetLsaList ();
    NS_TEST_EXPECT_MSG_EQ (list.size (), 1u, "lsa list size");
    NS_TEST_EXPECT_MSG_EQ (list[0].first.GetLength (), expectedLength, "lsa header length");
  }
};

class OspfLsUpdateDeclaredLengthExceedsBufferTestCase : public TestCase
{
public:
  OspfLsUpdateDeclaredLengthExceedsBufferTestCase ()
    : TestCase ("LsUpdate Deserialize handles declared length exceeding buffer")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);
    routerLsa->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (),
                                   Ipv4Address ("10.1.1.1").Get (),
                                   1,
                                   1));

    const uint32_t headerSize = h.GetSerializedSize ();
    const uint32_t payloadSize = routerLsa->GetSerializedSize ();
    const uint32_t expectedLength = headerSize + payloadSize;

    // Claim a larger payload than we actually provide (no padding).
    const uint32_t declaredLength = expectedLength + 8;
    h.SetLength (static_cast<uint16_t> (declaredLength));

    const uint32_t totalSize = 4 + headerSize + payloadSize;
    Buffer buffer;
    buffer.AddAtStart (totalSize);
    Buffer::Iterator it = buffer.Begin ();
    it.WriteHtonU32 (1);
    h.Serialize (it);
    it.Next (headerSize);
    routerLsa->Serialize (it);
    it.Next (payloadSize);

    Ptr<Packet> p = Create<Packet> (buffer.PeekData (), totalSize);
    LsUpdate out (p);

    // Should fail safely (no crash) and yield no LSAs.
    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 0u, "length-exceeds-buffer LSU should parse zero LSAs");
  }
};

class OspfLsUpdateCountExceedsBufferTestCase : public TestCase
{
public:
  OspfLsUpdateCountExceedsBufferTestCase ()
    : TestCase ("LsUpdate Deserialize handles LSA count exceeding buffer")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);
    routerLsa->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (),
                                   Ipv4Address ("10.1.1.1").Get (),
                                   1,
                                   1));

    const uint32_t headerSize = h.GetSerializedSize ();
    const uint32_t payloadSize = routerLsa->GetSerializedSize ();
    const uint32_t expectedLength = headerSize + payloadSize;
    h.SetLength (static_cast<uint16_t> (expectedLength));

    // Write count=2 but only include one full LSA.
    const uint32_t totalSize = 4 + headerSize + payloadSize;
    Buffer buffer;
    buffer.AddAtStart (totalSize);
    Buffer::Iterator it = buffer.Begin ();
    it.WriteHtonU32 (2);
    h.Serialize (it);
    it.Next (headerSize);
    routerLsa->Serialize (it);
    it.Next (payloadSize);

    Ptr<Packet> p = Create<Packet> (buffer.PeekData (), totalSize);
    LsUpdate out (p);

    // First LSA should parse, second should be ignored due to truncation.
    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 1u, "count-exceeds-buffer LSU should parse one LSA");
  }
};

class OspfLsUpdateUnsupportedTypeTestCase : public TestCase
{
public:
  OspfLsUpdateUnsupportedTypeTestCase ()
    : TestCase ("LsUpdate Deserialize handles unsupported LSA type")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);
    routerLsa->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (),
                                   Ipv4Address ("10.1.1.1").Get (),
                                   1,
                                   1));

    const uint32_t headerSize = h.GetSerializedSize ();
    const uint32_t payloadSize = routerLsa->GetSerializedSize ();
    const uint32_t expectedLength = headerSize + payloadSize;
    h.SetLength (static_cast<uint16_t> (expectedLength));

    const uint32_t totalSize = 4 + headerSize + payloadSize;
    Buffer buffer;
    buffer.AddAtStart (totalSize);
    Buffer::Iterator it = buffer.Begin ();
    it.WriteHtonU32 (1);
    h.Serialize (it);
    it.Next (headerSize);
    routerLsa->Serialize (it);

    // Copy out bytes and mutate the LSA type field to an unsupported value.
    std::vector<uint8_t> bytes (totalSize);
    std::memcpy (bytes.data (), buffer.PeekData (), totalSize);
    // Offset: 4 bytes (LSA count) + 2 bytes (age) + 1 byte (options) = 7.
    bytes[7] = 0x9;

    Ptr<Packet> p = Create<Packet> (bytes.data (), totalSize);
    LsUpdate out (p);

    // Should not crash and should not produce any LSAs.
    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 0u, "unsupported-type LSU should parse zero LSAs");
  }
};

class OspfLsUpdateLengthTooSmallTestCase : public TestCase
{
public:
  OspfLsUpdateLengthTooSmallTestCase ()
    : TestCase ("LsUpdate Deserialize rejects LSA length smaller than header")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    const uint32_t headerSize = h.GetSerializedSize ();
    // Deliberately invalid: declared length smaller than header size.
    h.SetLength (static_cast<uint16_t> (headerSize - 1));

    const uint32_t totalSize = 4 + headerSize;
    Buffer buffer;
    buffer.AddAtStart (totalSize);
    Buffer::Iterator it = buffer.Begin ();
    it.WriteHtonU32 (1);
    h.Serialize (it);

    Ptr<Packet> p = Create<Packet> (buffer.PeekData (), totalSize);
    LsUpdate out (p);

    NS_TEST_EXPECT_MSG_EQ (out.GetNLsa (), 0u, "length-too-small LSU should parse zero LSAs");
  }
};

class OspfLsUpdateTruncationSweepRobustnessTestCase : public TestCase
{
public:
  OspfLsUpdateTruncationSweepRobustnessTestCase ()
    : TestCase ("LsUpdate robustness: truncation sweep does not crash")
  {
  }

  void
  DoRun () override
  {
    LsaHeader h;
    h.SetType (LsaHeader::LsType::RouterLSAs);
    h.SetLsId (Ipv4Address ("10.1.1.1").Get ());
    h.SetAdvertisingRouter (Ipv4Address ("10.1.1.1").Get ());
    h.SetSeqNum (1);

    Ptr<RouterLsa> routerLsa = Create<RouterLsa> (false, false, false);
    routerLsa->AddLink (RouterLink (Ipv4Address ("10.1.1.2").Get (),
                                   Ipv4Address ("10.1.1.1").Get (),
                                   1,
                                   1));

    Ptr<LsUpdate> in = Create<LsUpdate> ();
    in->AddLsa (h, routerLsa);

    Ptr<Packet> full = in->ConstructPacket ();
    const uint32_t fullSize = full->GetSize ();
    NS_TEST_EXPECT_MSG_GT (fullSize, 0u, "sanity: non-empty LSU");

    std::vector<uint8_t> bytes (fullSize);
    full->CopyData (bytes.data (), fullSize);

    const uint32_t lsaHeaderSize = LsaHeader ().GetSerializedSize ();

    // Probe many truncated lengths (deterministic, fast).
    const uint32_t sweepMax = std::min<uint32_t> (fullSize, 128u);
    for (uint32_t len = 0; len <= sweepMax; ++len)
      {
        Ptr<Packet> p = Create<Packet> (bytes.data (), len);
        LsUpdate out (p);
        // We encoded exactly 1 LSA in the source packet.
        const bool ok = (out.GetNLsa () <= 1u);
        NS_TEST_EXPECT_MSG_EQ (ok, true, "lsa count should be <= 1");
      }

    // Also hit a few boundary lengths near the LSA header.
    const std::vector<uint32_t> interesting = {
      0u,
      1u,
      3u,
      4u,
      4u + lsaHeaderSize - 1u,
      4u + lsaHeaderSize,
      fullSize > 0 ? fullSize - 1 : 0,
      fullSize
    };
    for (uint32_t len : interesting)
      {
        if (len > fullSize)
          {
            continue;
          }
        Ptr<Packet> p = Create<Packet> (bytes.data (), len);
        LsUpdate out (p);
        const bool ok = (out.GetNLsa () <= 1u);
        NS_TEST_EXPECT_MSG_EQ (ok, true, "lsa count should be <= 1");
      }
  }
};

class OspfPacketsSerializationTestSuite : public TestSuite
{
public:
  OspfPacketsSerializationTestSuite ()
    : TestSuite ("ospf-packets-serialization", UNIT)
  {
    AddTestCase (new OspfHeaderRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfHeaderTruncationRobustnessTestCase, TestCase::QUICK);
    AddTestCase (new OspfHelloRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfDbdRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsAckRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsRequestRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateRoundtripTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateDeclaredLengthMismatchTestCase, TestCase::QUICK);
    AddTestCase (new OspfOtherPacketsTruncationRobustnessTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateTruncatedPayloadTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateMutationAfterAddTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateDeclaredLengthExceedsBufferTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateCountExceedsBufferTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateUnsupportedTypeTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateLengthTooSmallTestCase, TestCase::QUICK);
    AddTestCase (new OspfLsUpdateTruncationSweepRobustnessTestCase, TestCase::QUICK);
    AddTestCase (new OspfAccessorOutOfRangeNoCrashTestCase, TestCase::QUICK);
  }
};

static OspfPacketsSerializationTestSuite g_ospfPacketsSerializationTestSuite;

} // namespace ns3
