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
#include "router-lsa.h"
#include "set"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RouterLsa");

NS_OBJECT_ENSURE_REGISTERED (RouterLsa);

RouterLink::RouterLink () : m_linkId (0), m_linkData (0), m_type (0), m_metric (0)
{
}

RouterLink::RouterLink (uint32_t linkId, uint32_t linkData, uint8_t type, uint16_t metric)
    : m_linkId (linkId), m_linkData (linkData), m_type (type), m_metric (metric)
{
}
RouterLsa::RouterLsa () : m_bitV (0), m_bitE (0), m_bitB (0)
{
}

RouterLsa::RouterLsa (Ptr<Packet> packet)
{
  Deserialize (packet);
}

RouterLsa::RouterLsa (bool bitV, bool bitE, bool bitB) : m_bitV (bitV), m_bitE (bitE), m_bitB (bitB)
{
}

void
RouterLsa::AddLink (RouterLink link)
{
  m_links.emplace_back (link);
}

RouterLink
RouterLsa::GetLink (uint32_t index)
{
  NS_ASSERT_MSG (index >= 0 && index < m_links.size (), "Invalid link index");
  return m_links[index];
}

void
RouterLsa::ClearLinks ()
{
  m_links.clear ();
}

uint16_t
RouterLsa::GetNLink ()
{
  NS_LOG_FUNCTION (this);
  return m_links.size ();
}

std::vector<uint32_t>
RouterLsa::GetRouterLinkData ()
{
  NS_LOG_FUNCTION (this);
  std::vector<uint32_t> linkData;
  for (auto link : m_links)
    {
      linkData.emplace_back (link.m_linkData);
    }
  return linkData;
}
std::vector<AreaLink>
RouterLsa::GetCrossAreaLinks ()
{
  NS_LOG_FUNCTION (this);
  std::vector<AreaLink> crossAreaLinks;
  for (auto link : m_links)
    {
      if (link.m_type == 5)
        {
          crossAreaLinks.emplace_back (AreaLink (link.m_linkId, link.m_linkData, link.m_metric));
        }
    }
  return crossAreaLinks;
}

TypeId
RouterLsa::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RouterLsa").SetGroupName ("Ospf").AddConstructor<RouterLsa> ();
  return tid;
}
TypeId
RouterLsa::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
RouterLsa::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "V: " << m_bitV << " "
     << "E: " << m_bitE << " "
     << "B: " << m_bitE << " "
     << "# links: " << m_links.size () << std::endl;
}
uint32_t
RouterLsa::GetSerializedSize (void) const
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
RouterLsa::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
RouterLsa::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint16_t flags = setFlags (m_bitV, m_bitE, m_bitB);
  i.WriteHtonU16 (flags);
  i.WriteHtonU16 (m_links.size ());
  for (uint16_t j = 0; j < m_links.size (); j++)
    {
      i.WriteHtonU32 (m_links[j].m_linkId);
      i.WriteHtonU32 (m_links[j].m_linkData);
      i.WriteU8 (m_links[j].m_type);
      i.WriteU8 (0);
      i.WriteHtonU16 (m_links[j].m_metric);
    }
  return GetSerializedSize ();
}

uint32_t
RouterLsa::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint16_t flags = i.ReadNtohU16 ();
  extractFlags (flags, m_bitV, m_bitE, m_bitB);
  uint16_t linkNum = i.ReadNtohU16 ();

  m_links.clear ();
  for (uint16_t j = 0; j < linkNum; j++)
    {
      uint32_t linkId = i.ReadNtohU32 ();
      uint32_t linkData = i.ReadNtohU32 ();
      uint8_t type = i.ReadU8 ();
      i.Next (1); // Skip TOS
      // uint8_t tos = i.ReadU8 ();
      uint16_t metric = i.ReadNtohU16 ();
      m_links.emplace_back (RouterLink (linkId, linkData, type, metric));
    }
  return GetSerializedSize ();
}

uint32_t
RouterLsa::Deserialize (Ptr<Packet> packet)
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

Ptr<Lsa>
RouterLsa::Copy ()
{
  // Not very optimized way of copying
  Buffer buff;
  buff.AddAtStart (GetSerializedSize ());
  Ptr<RouterLsa> copy = Create<RouterLsa> ();
  Serialize (buff.Begin ());
  copy->Deserialize (buff.Begin ());
  return copy;
}

} // namespace ns3
