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
#include "as-external-lsa.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AsExternalLsa");

NS_OBJECT_ENSURE_REGISTERED (AsExternalLsa);

ExternalRoute::ExternalRoute (uint32_t address) : m_address (address), m_routeTag (0)
{
}
ExternalRoute::ExternalRoute (uint32_t address, uint32_t routeTag)
    : m_address (address), m_routeTag (routeTag)
{
}
AsExternalLsa::AsExternalLsa () : m_mask (0), m_metric (0)
{
}
AsExternalLsa::AsExternalLsa (uint32_t mask, uint32_t metric) : m_mask (mask), m_metric (metric)
{
}

AsExternalLsa::AsExternalLsa (Ptr<Packet> packet)
{
  Deserialize (packet);
}

void
AsExternalLsa::SetMask (uint32_t mask)
{
  m_mask = mask;
}

uint32_t
AsExternalLsa::GetMask (void)
{
  return m_mask;
}

void
AsExternalLsa::AddRoute (ExternalRoute route)
{
  m_routes.emplace_back (route);
}

ExternalRoute
AsExternalLsa::GetRoute (uint32_t index)
{
  NS_ASSERT_MSG (index >= 0 && index < m_routes.size (), "Invalid route index");
  return m_routes[index];
}

void
AsExternalLsa::ClearRoutes ()
{
  m_routes.clear ();
}

uint16_t
AsExternalLsa::GetNRoutes ()
{
  NS_LOG_FUNCTION (this);
  return m_routes.size ();
}

TypeId
AsExternalLsa::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::AsExternalLsa").SetGroupName ("Ospf").AddConstructor<AsExternalLsa> ();
  return tid;
}
TypeId
AsExternalLsa::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
AsExternalLsa::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "# external routes: " << m_routes.size () << std::endl;
}
uint32_t
AsExternalLsa::GetSerializedSize (void) const
{
  return 8 + m_routes.size () * 12; // Assumed no TOS
}

Ptr<Packet>
AsExternalLsa::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
AsExternalLsa::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  i.WriteHtonU32 (m_mask);
  i.WriteHtonU32 (m_metric);
  for (uint16_t j = 0; j < m_routes.size (); j++)
    {
      i.WriteHtonU32 (m_routes[j].m_address);
      i.WriteHtonU32 (m_routes[j].m_routeTag);
      i.WriteHtonU32 (0); // TOS is not used
    }
  return GetSerializedSize ();
}

uint32_t
AsExternalLsa::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  m_routes.clear ();
  m_mask = i.ReadNtohU32 ();
  m_metric = i.ReadNtohU32 ();

  uint16_t linkNum = i.GetRemainingSize () / 12;

  for (uint16_t j = 0; j < linkNum; j++)
    {
      uint32_t addr = i.ReadNtohU32 ();
      uint32_t routeTag = i.ReadNtohU32 ();
      i.Next (4); // Skip TOS
      m_routes.emplace_back (ExternalRoute (addr, routeTag));
    }
  return GetSerializedSize ();
}

uint32_t
AsExternalLsa::Deserialize (Ptr<Packet> packet)
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
