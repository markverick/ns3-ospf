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
#include "ls-ack.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsAck");

NS_OBJECT_ENSURE_REGISTERED (LsAck);

LsAck::LsAck ()
{
}

LsAck::LsAck (std::vector<LsaHeader> lsaHeaders)
{
  m_lsaHeaders.clear();
  for (auto l : lsaHeaders) {
    m_lsaHeaders.emplace_back(l);
  }
}

LsAck::LsAck (Ptr<Packet> packet)
{
  Deserialize(packet);
}

void
LsAck::AddLsaHeader (LsaHeader lsaHeader) {
  m_lsaHeaders.emplace_back(lsaHeader);
}

void
LsAck::ClearLsaHeaders () {
  m_lsaHeaders.clear();
}

bool
LsAck::HasLsaHeader (LsaHeader lsaHeader) {
  for(auto l : m_lsaHeaders) {
    if (l.GetKey() == lsaHeader.GetKey()) {
      return true;
    }
  }
  return false;
}

LsaHeader
LsAck::GetLsaHeader (uint32_t index) {
  NS_ASSERT(index < m_lsaHeaders.size());
  return m_lsaHeaders[index];
}

std::vector<LsaHeader>
LsAck::GetLsaHeaders () {
  return m_lsaHeaders;
}

uint32_t
LsAck::GetNLsaHeaders () {
  return m_lsaHeaders.size();
}

TypeId 
LsAck::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LsAck")
    .SetGroupName ("Ospf")
    .AddConstructor<LsAck> ()
  ;
  return tid;
}
TypeId 
LsAck::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void 
LsAck::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "# LSAs: " << m_lsaHeaders.size() << " ";
  os << std::endl;
}
uint32_t 
LsAck::GetSerializedSize (void) const
{
	return m_lsaHeaders.size() * 20;
}

Ptr<Packet>
LsAck::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart(GetSerializedSize());
  Serialize(buffer.Begin());
  
  Ptr<Packet> packet = Create<Packet>(buffer.PeekData(), GetSerializedSize());
  return packet;
}

uint32_t
LsAck::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  for (auto lsaHeader : m_lsaHeaders) {
    lsaHeader.Serialize(i);
    i.Next(lsaHeader.GetSerializedSize());
  }
  return GetSerializedSize();
}

uint32_t
LsAck::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  while (!i.IsEnd()) {
    LsaHeader lsaHeader;
    i.Next(lsaHeader.Deserialize(i));
    m_lsaHeaders.emplace_back(lsaHeader);
  }
  return GetSerializedSize ();
}

uint32_t
LsAck::Deserialize (Ptr<Packet> packet)
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
