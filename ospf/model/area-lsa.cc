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
#include "area-lsa.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AreaLsa");

NS_OBJECT_ENSURE_REGISTERED (AreaLsa);

AreaLink::AreaLink (uint32_t areaId, uint32_t ipAddress, uint8_t type, uint16_t metric)
    : m_areaId (areaId), m_ipAddress (ipAddress), m_type (type), m_metric (metric)
{
}
AreaLsa::AreaLsa () : m_bitV (0), m_bitE (0), m_bitB (0)
{
}

AreaLsa::AreaLsa (Ptr<Packet> packet)
{
  Deserialize (packet);
}

AreaLsa::AreaLsa (bool bitV, bool bitE, bool bitB) : m_bitV (bitV), m_bitE (bitE), m_bitB (bitB)
{
}

void
AreaLsa::AddLink (AreaLink link)
{
  m_links.emplace_back (link);
}

AreaLink
AreaLsa::GetLink (uint32_t index)
{
  NS_ASSERT_MSG (index >= 0 && index < m_links.size (), "Invalid link index");
  return m_links[index];
}

void
AreaLsa::ClearLinks ()
{
  m_links.clear ();
}

uint16_t
AreaLsa::GetNLink ()
{
  NS_LOG_FUNCTION (this);
  return m_links.size ();
}

TypeId
AreaLsa::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AreaLsa").SetGroupName ("Ospf").AddConstructor<AreaLsa> ();
  return tid;
}
TypeId
AreaLsa::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
AreaLsa::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "V: " << m_bitV << " "
     << "E: " << m_bitE << " "
     << "B: " << m_bitE << " "
     << "# links: " << m_links.size () << std::endl;
}
uint32_t
AreaLsa::GetSerializedSize (void) const
{
  return 4 + m_links.size () * 12; // Assumed no TOS
}

uint16_t
setFlags (bool V, bool E, bool B)
{
  uint16_t field = 0; // Initialize the 16-bit field to 0

  // Set the V, E, B bits at their respective positions (bit 7, 6, and 5)
  if (V)
    field |= (1 << 7);
  if (E)
    field |= (1 << 6);
  if (B)
    field |= (1 << 5);

  return field;
}

void
extractFlags (uint16_t field, bool &V, bool &E, bool &B)
{
  // Extract the V, E, B bits from their respective positions (bit 7, 6, and 5)
  V = (field & (1 << 7)) != 0;
  E = (field & (1 << 6)) != 0;
  B = (field & (1 << 5)) != 0;
}

Ptr<Packet>
AreaLsa::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
AreaLsa::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint16_t flags = setFlags (m_bitV, m_bitE, m_bitB);
  i.WriteHtonU16 (flags);
  i.WriteHtonU16 (m_links.size ());
  for (uint16_t j = 0; j < m_links.size (); j++)
    {
      i.WriteHtonU32 (m_links[j].m_areaId);
      i.WriteHtonU32 (m_links[j].m_ipAddress);
      i.WriteU8 (m_links[j].m_type);
      i.WriteU8 (0);
      i.WriteHtonU16 (m_links[j].m_metric);
    }
  return GetSerializedSize ();
}

uint32_t
AreaLsa::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint16_t flags = i.ReadNtohU16 ();
  extractFlags (flags, m_bitV, m_bitE, m_bitB);
  uint16_t linkNum = i.ReadNtohU16 ();

  m_links.clear ();
  for (uint16_t j = 0; j < linkNum; j++)
    {
      uint32_t areaId = i.ReadNtohU32 ();
      uint32_t ipAddress = i.ReadNtohU32 ();
      uint8_t type = i.ReadU8 ();
      i.Next (1); // Skip TOS
      // uint8_t tos = i.ReadU8 ();
      uint16_t metric = i.ReadNtohU16 ();
      m_links.emplace_back (AreaLink (areaId, ipAddress, type, metric));
    }
  return GetSerializedSize ();
}

uint32_t
AreaLsa::Deserialize (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << &packet);
  uint32_t payloadSize = packet->GetSize ();
  uint8_t *payload = new uint8_t[payloadSize];
  packet->CopyData (payload, payloadSize);
  Buffer buffer;
  buffer.AddAtStart (payloadSize);
  buffer.Begin ().Write (payload, payloadSize);
  Deserialize (buffer.Begin ());
  return payloadSize;
}

} // namespace ns3
