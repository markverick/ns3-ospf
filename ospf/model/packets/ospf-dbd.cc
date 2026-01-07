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
#include "ospf-dbd.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfDbd");

NS_OBJECT_ENSURE_REGISTERED (OspfDbd);

OspfDbd::OspfDbd ()
    : m_mtu (1500), m_options (0), m_flags (0), m_bitI (0), m_bitM (0), m_bitMS (0), m_ddSeqNum (0)
{
}

OspfDbd::OspfDbd (Ptr<Packet> packet)
{
  Deserialize (packet);
}

OspfDbd::OspfDbd (uint16_t mtu, uint8_t options, uint8_t flags, bool bitI, bool bitM, bool bitMS,
                  uint32_t ddSeqNum)
    : m_mtu (mtu),
      m_options (options),
      m_flags (flags),
      m_bitI (bitI),
      m_bitM (bitM),
      m_bitMS (bitMS),
      m_ddSeqNum (ddSeqNum)
{
}

void
OspfDbd::SetMtu (uint16_t mtu)
{
  m_mtu = mtu;
}

uint16_t
OspfDbd::GetMtu () const
{
  return m_mtu;
}

bool
OspfDbd::IsNegotiate () const
{
  if (m_bitI && m_bitM && m_bitMS)
    {
      return true;
    }
  return false;
}

void
OspfDbd::SetOptions (uint8_t options)
{
  m_options = options;
}

uint8_t
OspfDbd::GetOptions () const
{
  return m_options;
}

void
OspfDbd::SetBitI (bool bitI)
{
  m_bitI = bitI;
}

bool
OspfDbd::GetBitI () const
{
  return m_bitI;
}

void
OspfDbd::SetBitM (bool bitM)
{
  m_bitM = bitM;
}

bool
OspfDbd::GetBitM () const
{
  return m_bitM;
}

void
OspfDbd::SetBitMS (bool bitMS)
{
  m_bitMS = bitMS;
}

bool
OspfDbd::GetBitMS () const
{
  return m_bitMS;
}

void
OspfDbd::SetDDSeqNum (uint32_t ddSeqNum)
{
  m_ddSeqNum = ddSeqNum;
}

uint32_t
OspfDbd::GetDDSeqNum () const
{
  return m_ddSeqNum;
}

void
OspfDbd::AddLsaHeader (LsaHeader lsaHeader)
{
  m_lsaHeaders.emplace_back (lsaHeader);
}

void
OspfDbd::ClearLsaHeader ()
{
  m_lsaHeaders.clear ();
}

bool
OspfDbd::HasLsaHeader (LsaHeader lsaHeader)
{
  for (auto l : m_lsaHeaders)
    {
      if (l.GetKey () == lsaHeader.GetKey ())
        {
          return true;
        }
    }
  return false;
}

LsaHeader
OspfDbd::GetLsaHeader (uint32_t index)
{
  if (index >= m_lsaHeaders.size ())
    {
      NS_LOG_WARN ("GetLsaHeader out of range: " << index << " (size=" << m_lsaHeaders.size ()
                                                  << ")");
      return LsaHeader ();
    }
  return m_lsaHeaders[index];
}

std::vector<LsaHeader>
OspfDbd::GetLsaHeaders ()
{
  return m_lsaHeaders;
}

uint32_t
OspfDbd::GetNLsaHeaders ()
{
  return m_lsaHeaders.size ();
}

TypeId
OspfDbd::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OspfDbd").SetGroupName ("Ospf").AddConstructor<OspfDbd> ();
  return tid;
}
TypeId
OspfDbd::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
OspfDbd::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "MTU: " << m_mtu << " "
     << "Options: " << m_options << " "
     << "I: " << m_bitI << " "
     << "M: " << m_bitM << " "
     << "MS: " << m_bitMS << " "
     << "DD sequence number: " << m_ddSeqNum << " "
     << "# headers: " << m_lsaHeaders.size () << std::endl;
}
uint32_t
OspfDbd::GetSerializedSize (void) const
{
  return 8 + m_lsaHeaders.size () * 20; // Assumed no TOS
}

uint8_t
OspfDbd::GetFlags () const
{
  uint8_t field = 0; // Initialize the 16-bit field to 0

  // Set the V, E, B bits at their respective positions (bit 2, 1, and 0)
  if (m_bitI)
    field |= (1 << 2);
  if (m_bitM)
    field |= (1 << 1);
  if (m_bitMS)
    field |= (1 << 0);

  return field;
}

void
OspfDbd::SetFlags (uint8_t field)
{
  // Extract the V, E, B bits from their respective positions (bit 2, 1, and 0)
  m_bitI = (field & (1 << 2)) != 0;
  m_bitM = (field & (1 << 1)) != 0;
  m_bitMS = (field & (1 << 0)) != 0;
}

Ptr<Packet>
OspfDbd::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
OspfDbd::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  i.WriteHtonU16 (m_mtu);
  i.WriteU8 (m_options);
  i.WriteU8 (GetFlags ());
  i.WriteHtonU32 (m_ddSeqNum);
  for (auto lsaHeader : m_lsaHeaders)
    {
      lsaHeader.Serialize (i);
      i.Next (lsaHeader.GetSerializedSize ());
    }
  return GetSerializedSize ();
}

uint32_t
OspfDbd::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  // Fixed header is 8 bytes.
  if (i.GetRemainingSize () < 8)
    {
      NS_LOG_WARN ("OspfDbd truncated: missing fixed header");
      m_lsaHeaders.clear ();
      return 0;
    }

  m_mtu = i.ReadNtohU16 ();
  m_options = i.ReadU8 ();
  SetFlags (i.ReadU8 ());
  m_ddSeqNum = i.ReadNtohU32 ();

  m_lsaHeaders.clear ();

  const uint32_t lsaHeaderSize = LsaHeader ().GetSerializedSize ();
  while (!i.IsEnd ())
    {
      if (i.GetRemainingSize () < lsaHeaderSize)
        {
          NS_LOG_WARN ("OspfDbd truncated: incomplete LSA header");
          break;
        }
      LsaHeader lsaHeader;
      i.Next (lsaHeader.Deserialize (i));
      m_lsaHeaders.emplace_back (lsaHeader);
    }
  return GetSerializedSize ();
}

uint32_t
OspfDbd::Deserialize (Ptr<Packet> packet)
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
