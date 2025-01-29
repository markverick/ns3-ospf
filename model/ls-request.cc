/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 University of California, Los Angeles
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
 * Author: Sirapop Theeranantachai (stheera@g.ucla.edu)
 */

#include "ns3/assert.h"
#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/packet.h"
#include "ls-request.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsRequest");

NS_OBJECT_ENSURE_REGISTERED (LsRequest);

LsRequest::LsRequest ()
{
}

LsRequest::LsRequest (std::vector<LsaHeader::LsaKey> lsaKeys)
{
  m_lsaKeys.clear();
  for (auto l : lsaKeys) {
    m_lsaKeys.emplace_back(l);
  }
}

LsRequest::LsRequest (Ptr<Packet> packet)
{
  Deserialize(packet);
}

void
LsRequest::AddLsaKey (LsaHeader::LsaKey lsaKey) {
  m_lsaKeys.emplace_back(lsaKey);
}

void
LsRequest::ClearLsaKeys () {
  m_lsaKeys.clear();
}

bool
LsRequest::HasLsaKey (LsaHeader::LsaKey lsaKey) {
  for(auto l : m_lsaKeys) {
    if (l == lsaKey) {
      return true;
    }
  }
  return false;
}

LsaHeader::LsaKey
LsRequest::GetLsaKey (uint32_t index) {
  NS_ASSERT(index < m_lsaKeys.size());
  return m_lsaKeys[index];
}

std::vector<LsaHeader::LsaKey>
LsRequest::GetLsaKeys () {
  return m_lsaKeys;
}

uint32_t
LsRequest::GetNLsaKeys () {
  return m_lsaKeys.size();
}

TypeId 
LsRequest::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LsRequest")
    .SetGroupName ("Ospf")
    .AddConstructor<LsRequest> ()
  ;
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
  os << "# LSAs: " << m_lsaKeys.size() << " ";
  os << std::endl;
}
uint32_t 
LsRequest::GetSerializedSize (void) const
{
	return m_lsaKeys.size() * 12;
}

Ptr<Packet>
LsRequest::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart(GetSerializedSize());
  Serialize(buffer.Begin());
  
  Ptr<Packet> packet = Create<Packet>(buffer.PeekData(), GetSerializedSize());
  return packet;
}

uint32_t
LsRequest::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  for (auto lsaKey : m_lsaKeys) {
    auto [type, lsId, advertisingRouter] = lsaKey;
    i.WriteHtonU32(type);
    i.WriteHtonU32(lsId);
    i.WriteHtonU32(advertisingRouter);
  }
  return GetSerializedSize();
}

uint32_t
LsRequest::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint32_t type, lsId, advertisingRouter;
  while (!i.IsEnd()) {
    type = i.ReadNtohU32();
    lsId = i.ReadNtohU32();
    advertisingRouter = i.ReadNtohU32();
    m_lsaKeys.emplace_back(type, lsId, advertisingRouter);
  }
  return GetSerializedSize ();
}

uint32_t
LsRequest::Deserialize (Ptr<Packet> packet)
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
