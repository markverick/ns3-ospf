/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
MIT License

Copyright (c) 2025 Sirapop Theeranantachai

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
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

bool
LsRequest::RemoveLsaKey (LsaHeader::LsaKey lsaKey) {
  for (auto it = m_lsaKeys.begin(); it != m_lsaKeys.end(); it++) {
    if (*it == lsaKey) {
        m_lsaKeys.erase(it);
        return true;
    }
  }
  return false;
}

bool
LsRequest::IsLsaKeyEmpty () {
  return m_lsaKeys.empty();
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
