/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Sirapop Theeranantachai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Sirapop Theeranantachaoi <stheera@g.ucla.edu>
 */

#include "ns3/assert.h"
#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/packet.h"
#include "ns3/router-lsa.h"
#include "ns3/l1-summary-lsa.h"
#include "ns3/l2-summary-lsa.h"
#include "ls-update.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsUpdate");

NS_OBJECT_ENSURE_REGISTERED (LsUpdate);

LsUpdate::LsUpdate ()
{
  m_serializedSize = 4;
}
LsUpdate::LsUpdate (Ptr<Packet> packet)
{
  Deserialize (packet);
}

void
LsUpdate::AddLsa (LsaHeader header, Ptr<Lsa> lsa)
{
  const uint16_t expectedLength =
      static_cast<uint16_t> (header.GetSerializedSize () + lsa->GetSerializedSize ());
  header.SetLength (expectedLength);
  m_lsaList.emplace_back (header, lsa);
  m_serializedSize += expectedLength;
}
void
LsUpdate::AddLsa (std::pair<LsaHeader, Ptr<Lsa>> lsa)
{
  const uint16_t expectedLength =
      static_cast<uint16_t> (lsa.first.GetSerializedSize () + lsa.second->GetSerializedSize ());
  lsa.first.SetLength (expectedLength);
  m_lsaList.emplace_back (lsa);
  m_serializedSize += expectedLength;
}

std::vector<std::pair<LsaHeader, Ptr<Lsa>>>
LsUpdate::GetLsaList ()
{
  return m_lsaList;
}

uint32_t
LsUpdate::GetNLsa ()
{
  return m_lsaList.size ();
}

TypeId
LsUpdate::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LsUpdate").SetGroupName ("Ospf").AddConstructor<LsUpdate> ();
  return tid;
}
TypeId
LsUpdate::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
LsUpdate::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "# LSAs: " << m_lsaList.size () << " ";
  os << std::endl;
}
uint32_t
LsUpdate::GetSerializedSize (void) const
{
  uint32_t size = 4;
  for (const auto &lsa : m_lsaList)
    {
      size += lsa.first.GetSerializedSize ();
      size += lsa.second->GetSerializedSize ();
    }
  return size;
}

Ptr<Packet>
LsUpdate::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
LsUpdate::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteHtonU32 (m_lsaList.size ());
  for (const auto &lsa : m_lsaList)
    {
      // Ensure we never emit a malformed length field.
      LsaHeader header = lsa.first;
      const uint16_t expectedLength = static_cast<uint16_t> (
          header.GetSerializedSize () + lsa.second->GetSerializedSize ());
      header.SetLength (expectedLength);

      header.Serialize (i);
      i.Next (header.GetSerializedSize ());

      lsa.second->Serialize (i);
      i.Next (lsa.second->GetSerializedSize ());
    }
  return GetSerializedSize ();
}

uint32_t
LsUpdate::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  if (i.GetRemainingSize () < 4)
    {
      NS_LOG_WARN ("LsUpdate truncated: missing LSA count");
      m_lsaList.clear ();
      m_serializedSize = 0;
      return 0;
    }

  const uint32_t numLsa = i.ReadNtohU32 ();
  m_lsaList.clear ();
  m_serializedSize = 4;

  const uint32_t lsaHeaderSize = LsaHeader ().GetSerializedSize ();

  for (uint32_t j = 0; j < numLsa; j++)
    {
      LsaHeader lsaHeader;

      if (i.GetRemainingSize () < lsaHeaderSize)
        {
          NS_LOG_WARN ("LsUpdate truncated: missing LSA header");
          break;
        }

      i.Next (lsaHeader.Deserialize (i));

      if (lsaHeader.GetLength () < lsaHeaderSize)
        {
          NS_LOG_WARN ("LsUpdate malformed: LSA length smaller than header");
          break;
        }

      const uint32_t declaredPayloadSize = lsaHeader.GetLength () - lsaHeaderSize;
      if (i.GetRemainingSize () < declaredPayloadSize)
        {
          NS_LOG_WARN ("LsUpdate truncated: LSA payload exceeds remaining buffer");
          break;
        }

      if (declaredPayloadSize < 4)
        {
          NS_LOG_WARN ("LsUpdate malformed: LSA payload too small");
          break;
        }

      std::vector<uint8_t> payloadBytes (declaredPayloadSize);
      i.Read (payloadBytes.data (), declaredPayloadSize);
      Buffer payloadBuffer;
      payloadBuffer.AddAtStart (declaredPayloadSize);
      payloadBuffer.Begin ().Write (payloadBytes.data (), declaredPayloadSize);

      Buffer::Iterator payloadIt = payloadBuffer.Begin ();

      if (lsaHeader.GetType () == LsaHeader::RouterLSAs)
        {
          Ptr<RouterLsa> lsa = Create<RouterLsa> ();
          payloadIt.Next (lsa->Deserialize (payloadIt));
          m_lsaList.emplace_back (lsaHeader, lsa);
          const uint16_t expectedLength = static_cast<uint16_t> (
              lsaHeader.GetSerializedSize () + lsa->GetSerializedSize ());
          if (lsaHeader.GetLength () != expectedLength)
            {
              NS_LOG_WARN ("LsUpdate RouterLSA length mismatch (declared=" << lsaHeader.GetLength ()
                                                                        << ", expected=" << expectedLength
                                                                        << ")");
              lsaHeader.SetLength (expectedLength);
              m_lsaList.back ().first = lsaHeader;
            }

          m_serializedSize += expectedLength;
        }
      else if (lsaHeader.GetType () == LsaHeader::AreaLSAs)
        {
          Ptr<AreaLsa> lsa = Create<AreaLsa> ();
          payloadIt.Next (lsa->Deserialize (payloadIt));
          m_lsaList.emplace_back (lsaHeader, lsa);
          const uint16_t expectedLength = static_cast<uint16_t> (
              lsaHeader.GetSerializedSize () + lsa->GetSerializedSize ());
          if (lsaHeader.GetLength () != expectedLength)
            {
              NS_LOG_WARN ("LsUpdate AreaLSA length mismatch (declared=" << lsaHeader.GetLength ()
                                                                      << ", expected=" << expectedLength
                                                                      << ")");
              lsaHeader.SetLength (expectedLength);
              m_lsaList.back ().first = lsaHeader;
            }
          m_serializedSize += expectedLength;
        }
      else if (lsaHeader.GetType () == LsaHeader::L2SummaryLSAs)
        {
          Ptr<L2SummaryLsa> lsa = Create<L2SummaryLsa> ();
          payloadIt.Next (lsa->Deserialize (payloadIt));
          m_lsaList.emplace_back (lsaHeader, lsa);
          const uint16_t expectedLength = static_cast<uint16_t> (
              lsaHeader.GetSerializedSize () + lsa->GetSerializedSize ());
          if (lsaHeader.GetLength () != expectedLength)
            {
              NS_LOG_WARN ("LsUpdate L2SummaryLSA length mismatch (declared=" << lsaHeader.GetLength ()
                                                                            << ", expected=" << expectedLength
                                                                            << ")");
              lsaHeader.SetLength (expectedLength);
              m_lsaList.back ().first = lsaHeader;
            }
          m_serializedSize += expectedLength;
        }
      else if (lsaHeader.GetType () == LsaHeader::L1SummaryLSAs)
        {
          Ptr<L1SummaryLsa> lsa = Create<L1SummaryLsa> ();
          payloadIt.Next (lsa->Deserialize (payloadIt));
          m_lsaList.emplace_back (lsaHeader, lsa);
          const uint16_t expectedLength = static_cast<uint16_t> (
              lsaHeader.GetSerializedSize () + lsa->GetSerializedSize ());
          if (lsaHeader.GetLength () != expectedLength)
            {
              NS_LOG_WARN ("LsUpdate L1SummaryLSA length mismatch (declared=" << lsaHeader.GetLength ()
                                                                            << ", expected=" << expectedLength
                                                                            << ")");
              lsaHeader.SetLength (expectedLength);
              m_lsaList.back ().first = lsaHeader;
            }
          m_serializedSize += expectedLength;
        }
      else
        {
          NS_LOG_WARN ("LsUpdate unsupported LSA type: " << static_cast<uint32_t> (lsaHeader.GetType ()));
          // Length is known (declaredPayloadSize already consumed). Stop parsing.
          break;
        }
    }

  return m_serializedSize;
}

uint32_t
LsUpdate::Deserialize (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << &packet);
  uint32_t payloadSize = packet->GetSize ();
  std::vector<uint8_t> payload (payloadSize);
  packet->CopyData (payload.data (), payloadSize);
  Buffer buffer;
  buffer.AddAtStart (payloadSize);
  buffer.Begin ().Write (payload.data (), payloadSize);
  Deserialize (buffer.Begin ());
  return payloadSize;
}

} // namespace ns3
