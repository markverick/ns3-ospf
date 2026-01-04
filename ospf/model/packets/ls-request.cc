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
#include "ls-request.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsRequest");

NS_OBJECT_ENSURE_REGISTERED (LsRequest);

LsRequest::LsRequest ()
{
}

LsRequest::LsRequest (std::vector<LsaHeader::LsaKey> lsaKeys)
{
  m_lsaKeys.clear ();
  for (auto l : lsaKeys)
    {
      m_lsaKeys.emplace_back (l);
    }
}

LsRequest::LsRequest (Ptr<Packet> packet)
{
  Deserialize (packet);
}

void
LsRequest::AddLsaKey (LsaHeader::LsaKey lsaKey)
{
  m_lsaKeys.emplace_back (lsaKey);
}

void
LsRequest::ClearLsaKeys ()
{
  m_lsaKeys.clear ();
}

bool
LsRequest::HasLsaKey (LsaHeader::LsaKey lsaKey)
{
  for (auto l : m_lsaKeys)
    {
      if (l == lsaKey)
        {
          return true;
        }
    }
  return false;
}

bool
LsRequest::RemoveLsaKey (LsaHeader::LsaKey lsaKey)
{
  for (auto it = m_lsaKeys.begin (); it != m_lsaKeys.end (); it++)
    {
      if (*it == lsaKey)
        {
          m_lsaKeys.erase (it);
          return true;
        }
    }
  return false;
}

bool
LsRequest::IsLsaKeyEmpty ()
{
  return m_lsaKeys.empty ();
}

LsaHeader::LsaKey
LsRequest::GetLsaKey (uint32_t index)
{
  if (index >= m_lsaKeys.size ())
    {
      NS_LOG_WARN ("GetLsaKey out of range: " << index << " (size=" << m_lsaKeys.size () << ")");
      return std::make_tuple (LsaHeader::LsType::RouterLSAs, 0u, 0u);
    }
  return m_lsaKeys[index];
}

std::vector<LsaHeader::LsaKey>
LsRequest::GetLsaKeys ()
{
  return m_lsaKeys;
}

uint32_t
LsRequest::GetNLsaKeys ()
{
  return m_lsaKeys.size ();
}

TypeId
LsRequest::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LsRequest").SetGroupName ("Ospf").AddConstructor<LsRequest> ();
  return tid;
}
TypeId
LsRequest::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
LsRequest::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "# LSAs: " << m_lsaKeys.size () << " ";
  os << std::endl;
}
uint32_t
LsRequest::GetSerializedSize (void) const
{
  return m_lsaKeys.size () * 12;
}

Ptr<Packet>
LsRequest::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
LsRequest::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  for (auto lsaKey : m_lsaKeys)
    {
      auto [type, lsId, advertisingRouter] = lsaKey;
      i.WriteHtonU32 (type);
      i.WriteHtonU32 (lsId);
      i.WriteHtonU32 (advertisingRouter);
    }
  return GetSerializedSize ();
}

uint32_t
LsRequest::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint32_t type, lsId, advertisingRouter;
  m_lsaKeys.clear ();
  while (!i.IsEnd ())
    {
      if (i.GetRemainingSize () < 12)
        {
          NS_LOG_WARN ("LsRequest truncated: incomplete LSA key");
          break;
        }
      type = i.ReadNtohU32 ();
      lsId = i.ReadNtohU32 ();
      advertisingRouter = i.ReadNtohU32 ();

      if (type > 0xff)
        {
          NS_LOG_WARN ("LsRequest invalid LS type (out of range for implementation): " << type);
          continue;
        }

      const uint8_t t8 = static_cast<uint8_t> (type);
      switch (static_cast<LsaHeader::LsType> (t8))
        {
        case LsaHeader::RouterLSAs:
        case LsaHeader::NetworkLSAs:
        case LsaHeader::SummaryLSAsIP:
        case LsaHeader::SummaryLSAsASBR:
        case LsaHeader::ASExternalLSAs:
        case LsaHeader::AreaLSAs:
        case LsaHeader::L1SummaryLSAs:
        case LsaHeader::L2SummaryLSAs:
          m_lsaKeys.emplace_back (t8, lsId, advertisingRouter);
          break;
        default:
          NS_LOG_WARN ("LsRequest unsupported LS type: " << static_cast<uint32_t> (t8));
          break;
        }
    }
  return GetSerializedSize ();
}

uint32_t
LsRequest::Deserialize (Ptr<Packet> packet)
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
