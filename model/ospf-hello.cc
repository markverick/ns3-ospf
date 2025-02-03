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
#include "ospf-hello.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfHello");

NS_OBJECT_ENSURE_REGISTERED (OspfHello);

OspfHello::OspfHello()
  : m_mask(0),
    m_helloInterval(0),
    m_options(0),
    m_routerPriority(0),
    m_routerDeadInterval(0),
    m_dr(0),
    m_bdr(0)
{
}

OspfHello::OspfHello(uint32_t mask, uint16_t helloInterval, uint32_t routerDeadInterval)
  : m_mask(mask),
    m_helloInterval(helloInterval),
    m_options(0),
    m_routerPriority(0),
    m_routerDeadInterval(routerDeadInterval),
    m_dr(0),
    m_bdr(0)
{
}

OspfHello::OspfHello(uint32_t mask, uint16_t helloInterval, uint32_t routerDeadInterval,
            std::vector<uint32_t> neighbors)
  : m_mask(mask),
    m_helloInterval(helloInterval),
    m_options(0),
    m_routerPriority(0),
    m_routerDeadInterval(routerDeadInterval),
    m_dr(0),
    m_bdr(0)
{
  for (auto n : neighbors) {
    m_neighbors.emplace_back(n);
  }
}

OspfHello::OspfHello (Ptr<Packet> packet)
{
  Deserialize(packet);
}

void
OspfHello::SetMask(uint32_t mask)
{
  m_mask = mask;
}

uint32_t
OspfHello::GetMask(void) const
{
  return m_mask;
}

void
OspfHello::SetHelloInterval(uint16_t helloInterval)
{
  m_helloInterval = helloInterval;
}

uint16_t
OspfHello::GetHelloInterval(void) const
{
  return m_helloInterval;
}

void
OspfHello::SetOptions(uint8_t options)
{
  m_options = options;
}

uint8_t
OspfHello::GetOptions(void) const
{
  return m_options;
}

void
OspfHello::SetRouterPriority(uint8_t routerPriority)
{
  m_routerPriority = routerPriority;
}

uint8_t
OspfHello::GetRouterPriority(void) const
{
  return m_routerPriority;
}

void
OspfHello::SetRouterDeadInterval(uint32_t routerDeadInterval)
{
  m_routerDeadInterval = routerDeadInterval;
}

uint32_t
OspfHello::GetRouterDeadInterval(void) const
{
  return m_routerDeadInterval;
}

void
OspfHello::SetDesignatedRouter(uint32_t designatedRouter)
{
  m_dr = designatedRouter;
}

uint32_t
OspfHello::GetDesignatedRouter(void) const
{
  return m_dr;
}

void
OspfHello::SetBackupDesignatedRouter(uint32_t backupDesignatedRouter)
{
  m_bdr = backupDesignatedRouter;
}

uint32_t
OspfHello::GetBackupDesignatedRouter(void) const
{
  return m_bdr;
}

void
OspfHello::AddNeighbor(uint32_t neighborRouterId)
{
  m_neighbors.emplace_back(neighborRouterId);
}

void
OspfHello::ClearNeighbor(void)
{
  m_neighbors.clear();
}

bool
OspfHello::IsNeighbor(uint32_t neighborRouterId)
{
  for (auto n : m_neighbors) {
    if (n == neighborRouterId) {
        return true;
    }
  }
  return false;
}

uint32_t
OspfHello::GetNeighbor(uint32_t index)
{
  NS_ASSERT_MSG(index >= 0 && index < m_neighbors.size(), "Invalid link index");
  return m_neighbors[index];
}

// Return the number of neighbors
uint32_t
OspfHello::SetNeighbors(std::vector<uint32_t> neighbors)
{
  m_neighbors.clear();
  for (auto n : neighbors) {
    m_neighbors.emplace_back(n);
  }
  return m_neighbors.size();
}

uint32_t
OspfHello::GetNNeighbors ()
{
  return m_neighbors.size();
}

TypeId 
OspfHello::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OspfHello")
    .SetGroupName ("Ospf")
    .AddConstructor<OspfHello> ()
  ;
  return tid;
}
TypeId 
OspfHello::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void 
OspfHello::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "mask: " << m_mask << " "
     << "helloInterval: " << m_helloInterval << " "
     << "routerDeadInterval: " << m_routerDeadInterval << " "
     << "# neighbors: " << m_neighbors.size() << " ";
  for (auto n : m_neighbors) {
    os << "(" << Ipv4Address(n) << ")" << " ";
  }
  os << std::endl;
}
uint32_t 
OspfHello::GetSerializedSize (void) const
{
	return 20 + m_neighbors.size() * 4; // Assumed no TOS
}

Ptr<Packet>
OspfHello::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart(GetSerializedSize());
  Serialize(buffer.Begin());
  
  Ptr<Packet> packet = Create<Packet>(buffer.PeekData(), GetSerializedSize());
  return packet;
}

uint32_t
OspfHello::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  i.WriteHtonU32 (m_mask);
  i.WriteHtonU16 (m_helloInterval);
  i.WriteU8 (m_options);
  i.WriteU8 (m_routerPriority);
  i.WriteHtonU32 (m_routerDeadInterval);
  i.WriteHtonU32 (m_dr);
  i.WriteHtonU32 (m_bdr);
  for (auto n : m_neighbors) {
    i.WriteHtonU32 (n);
  }
  return GetSerializedSize();
}

uint32_t
OspfHello::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  m_mask = i.ReadNtohU32 ();
  m_helloInterval = i.ReadNtohU16 ();
  m_options = i.ReadU8 ();
  m_routerPriority = i.ReadU8 ();
  m_routerDeadInterval = i.ReadNtohU32 ();
  m_dr = i.ReadNtohU32 ();
  m_bdr = i.ReadNtohU32 ();
  while (!i.IsEnd()) {
    m_neighbors.emplace_back(i.ReadNtohU32 ());
  }
  return GetSerializedSize ();
}

uint32_t
OspfHello::Deserialize (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << &packet);
  uint32_t payloadSize = packet->GetSize();
  uint8_t *payload = new uint8_t[payloadSize];
  packet->CopyData(payload, payloadSize);
  Buffer buffer;
  buffer.AddAtStart(payloadSize);
  buffer.Begin().Write(payload, payloadSize);
  Deserialize(buffer.Begin());
  return payloadSize;
}

} // namespace ns3
