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
#include "router-lsa.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AreaLsa");

NS_OBJECT_ENSURE_REGISTERED (AreaLsa);

AreaLink::AreaLink (uint32_t areaId, uint32_t ipAddress, uint16_t metric)
    : m_areaId (areaId), m_ipAddress (ipAddress), m_metric (metric)
{
}

AreaLsa::AreaLsa ()
{
}

AreaLsa::AreaLsa (std::vector<AreaLink> links)
{
  for (auto link : links)
    {
      AddLink (link);
    }
}

AreaLsa::AreaLsa (Ptr<Packet> packet)
{
  Deserialize (packet);
}

void
AreaLsa::AddLink (AreaLink link)
{
  m_links.emplace_back (link);
}

AreaLink
AreaLsa::GetLink (uint32_t index)
{
  if (index >= m_links.size ())
    {
      NS_LOG_WARN ("GetLink: out-of-range index=" << index << " size=" << m_links.size ());
      return AreaLink (0, 0, 0);
    }
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

std::vector<AreaLink>
AreaLsa::GetLinks ()
{
  return m_links;
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
  os << "# links: " << m_links.size () << std::endl;
}
uint32_t
AreaLsa::GetSerializedSize (void) const
{
  return 4 + m_links.size () * 12; // Assumed no TOS
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

  i.WriteHtonU16 (0);
  i.WriteHtonU16 (m_links.size ());
  for (uint16_t j = 0; j < m_links.size (); j++)
    {
      i.WriteHtonU32 (m_links[j].m_areaId);
      i.WriteHtonU32 (m_links[j].m_ipAddress);
      i.Next (2);
      i.WriteHtonU16 (m_links[j].m_metric);
    }
  return GetSerializedSize ();
}

uint32_t
AreaLsa::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  // Fixed header is 4 bytes: reserved (2) + link count (2)
  if (i.GetRemainingSize () < 4)
    {
      NS_LOG_WARN ("AreaLsa truncated: missing fixed header");
      m_links.clear ();
      return 0;
    }

  i.Next (2);
  uint16_t linkNum = i.ReadNtohU16 ();

  m_links.clear ();
  const uint32_t linkSize = 12;
  for (uint16_t j = 0; j < linkNum; j++)
    {
      if (i.GetRemainingSize () < linkSize)
        {
          NS_LOG_WARN ("AreaLsa truncated: incomplete link entry");
          break;
        }
      uint32_t areaId = i.ReadNtohU32 ();
      uint32_t ipAddress = i.ReadNtohU32 ();
      i.Next (2);
      uint16_t metric = i.ReadNtohU16 ();
      m_links.emplace_back (areaId, ipAddress, metric);
    }
  return GetSerializedSize ();
}

uint32_t
AreaLsa::Deserialize (Ptr<Packet> packet)
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

Ptr<Lsa>
AreaLsa::Copy ()
{
  // Not very optimized way of copying
  Buffer buff;
  buff.AddAtStart (GetSerializedSize ());
  Ptr<AreaLsa> copy = Create<AreaLsa> ();
  Serialize (buff.Begin ());
  copy->Deserialize (buff.Begin ());
  return copy;
}

} // namespace ns3
