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
#include "summary-lsa.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SummaryLsa");

NS_OBJECT_ENSURE_REGISTERED (SummaryLsa);

SummaryLsa::SummaryLsa ()
{
}

SummaryLsa::SummaryLsa (uint32_t mask) : m_mask (mask), m_metric (0)
{
}

SummaryLsa::SummaryLsa (uint32_t mask, uint32_t metric) : m_mask (mask), m_metric (metric)
{
}

SummaryLsa::SummaryLsa (Ptr<Packet> packet)
{
  Deserialize (packet);
}

void
SummaryLsa::SetMask (uint32_t mask)
{
  m_mask = mask;
}

uint32_t
SummaryLsa::GetMask ()
{
  return m_mask;
}

void
SummaryLsa::SetMetric (uint32_t mask)
{
  m_mask = mask;
}

uint32_t
SummaryLsa::GetMetric ()
{
  return m_mask;
}

TypeId
SummaryLsa::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::SummaryLsa").SetGroupName ("Ospf").AddConstructor<SummaryLsa> ();
  return tid;
}
TypeId
SummaryLsa::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
SummaryLsa::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "mask: " << m_mask << ", metric: " << m_metric << std::endl;
}
uint32_t
SummaryLsa::GetSerializedSize (void) const
{
  return 12; // Assumed no TOS
}

Ptr<Packet>
SummaryLsa::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
SummaryLsa::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  i.WriteHtonU32 (m_mask);
  i.WriteHtonU32 (m_metric);
  i.WriteHtonU32 (0); // TOS is not used

  return GetSerializedSize ();
}

uint32_t
SummaryLsa::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  m_mask = i.ReadNtohU32 ();
  m_metric = i.ReadNtohU32 ();
  i.Next (4); // Skip TOS

  return GetSerializedSize ();
}

uint32_t
SummaryLsa::Deserialize (Ptr<Packet> packet)
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
SummaryLsa::Copy ()
{
  // Not very optimized way of copying
  Buffer buff;
  buff.AddAtStart (GetSerializedSize ());
  Ptr<SummaryLsa> copy = Create<SummaryLsa> ();
  Serialize (buff.Begin ());
  copy->Deserialize (buff.Begin ());
  return copy;
}

} // namespace ns3
