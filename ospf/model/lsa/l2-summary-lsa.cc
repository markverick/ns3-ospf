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
#include "l2-summary-lsa.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("L2SummaryLsa");

NS_OBJECT_ENSURE_REGISTERED (L2SummaryLsa);

SummaryRoute::SummaryRoute (uint32_t address, uint32_t mask, uint32_t metric)
    : m_address (address), m_mask (mask), m_metric (metric)
{
}
L2SummaryLsa::L2SummaryLsa ()
{
}

L2SummaryLsa::L2SummaryLsa (Ptr<Packet> packet)
{
  Deserialize (packet);
}

void
L2SummaryLsa::AddRoute (SummaryRoute route)
{
  m_routes.insert (route);
}

std::set<SummaryRoute>
L2SummaryLsa::GetRoutes ()
{
  return m_routes;
}

uint32_t
L2SummaryLsa::GetNRoute ()
{
  return m_routes.size ();
}

TypeId
L2SummaryLsa::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::L2SummaryLsa").SetGroupName ("Ospf").AddConstructor<L2SummaryLsa> ();
  return tid;
}
TypeId
L2SummaryLsa::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
L2SummaryLsa::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "# routes: " << m_routes.size () << std::endl;
}
uint32_t
L2SummaryLsa::GetSerializedSize (void) const
{
  return 4 + m_routes.size () * 12;
}

Ptr<Packet>
L2SummaryLsa::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
L2SummaryLsa::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteHtonU32 (m_routes.size ());
  for (auto route : m_routes)
    {
      i.WriteHtonU32 (route.m_address);
      i.WriteHtonU32 (route.m_mask);
      i.WriteHtonU32 (route.m_metric);
    }

  return GetSerializedSize ();
}

uint32_t
L2SummaryLsa::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  if (i.GetRemainingSize () < 4)
    {
      NS_LOG_WARN ("L2SummaryLsa truncated: missing route count");
      m_routes.clear ();
      return 0;
    }

  uint32_t routeNum = i.ReadNtohU32 ();
  uint32_t addr, mask, metric;
  m_routes.clear ();
  const uint32_t routeSize = 12;
  for (uint32_t j = 0; j < routeNum; j++)
    {
      if (i.GetRemainingSize () < routeSize)
        {
          NS_LOG_WARN ("L2SummaryLsa truncated: incomplete route entry");
          break;
        }
      addr = i.ReadNtohU32 ();
      mask = i.ReadNtohU32 ();
      metric = i.ReadNtohU32 ();
      m_routes.insert (SummaryRoute (addr, mask, metric));
    }

  return GetSerializedSize ();
}

uint32_t
L2SummaryLsa::Deserialize (Ptr<Packet> packet)
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
L2SummaryLsa::Copy ()
{
  // Not very optimized way of copying
  Buffer buff;
  buff.AddAtStart (GetSerializedSize ());
  Ptr<L2SummaryLsa> copy = Create<L2SummaryLsa> ();
  Serialize (buff.Begin ());
  copy->Deserialize (buff.Begin ());
  return copy;
}

} // namespace ns3
