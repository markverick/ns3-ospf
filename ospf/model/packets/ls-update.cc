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
#include "ns3/router-lsa.h"
#include "ns3/summary-lsa.h"
#include "ns3/as-external-lsa.h"
#include "ls-update.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsUpdate");

NS_OBJECT_ENSURE_REGISTERED (LsUpdate);

LsUpdate::LsUpdate ()
{
  m_serializedSize = 4;
}
LsUpdate::LsUpdate (Ptr<Packet> packet)
{
  Deserialize (packet);
}

void
LsUpdate::AddLsa (LsaHeader header, Ptr<Lsa> lsa)
{
  m_lsaList.emplace_back (header, lsa);
  m_serializedSize += header.GetLength ();
}
void
LsUpdate::AddLsa (std::pair<LsaHeader, Ptr<Lsa>> lsa)
{
  m_lsaList.emplace_back (lsa);
  m_serializedSize += lsa.first.GetLength ();
}

std::vector<std::pair<LsaHeader, Ptr<Lsa>>>
LsUpdate::GetLsaList ()
{
  return m_lsaList;
}

uint32_t
LsUpdate::GetNLsa ()
{
  return m_lsaList.size ();
}

TypeId
LsUpdate::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LsUpdate").SetGroupName ("Ospf").AddConstructor<LsUpdate> ();
  return tid;
}
TypeId
LsUpdate::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
LsUpdate::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "# LSAs: " << m_lsaList.size () << " ";
  os << std::endl;
}
uint32_t
LsUpdate::GetSerializedSize (void) const
{
  return m_serializedSize;
}

Ptr<Packet>
LsUpdate::ConstructPacket () const
{
  NS_LOG_FUNCTION (this);

  Buffer buffer;
  buffer.AddAtStart (GetSerializedSize ());
  Serialize (buffer.Begin ());

  Ptr<Packet> packet = Create<Packet> (buffer.PeekData (), GetSerializedSize ());
  return packet;
}

uint32_t
LsUpdate::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteHtonU32 (m_lsaList.size ());
  for (auto lsa : m_lsaList)
    {
      lsa.first.Serialize (i);
      i.Next (lsa.first.GetSerializedSize ());
      lsa.second->Serialize (i);
      i.Next (lsa.second->GetSerializedSize ());
    }
  return GetSerializedSize ();
}

uint32_t
LsUpdate::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint32_t numLsa = i.ReadNtohU32 ();

  m_serializedSize = 4;
  for (uint32_t j = 0; j < numLsa; j++)
    {
      LsaHeader lsaHeader;
      i.Next (lsaHeader.Deserialize (i));
      if (lsaHeader.GetType () == LsaHeader::RouterLSAs)
        {
          Ptr<RouterLsa> lsa = Create<RouterLsa> ();
          i.Next (lsa->Deserialize (i));
          m_lsaList.emplace_back (lsaHeader, lsa);
          m_serializedSize += lsaHeader.GetLength ();
        }
      else if (lsaHeader.GetType () == LsaHeader::AreaLSAs)
        {
          Ptr<AreaLsa> lsa = Create<AreaLsa> ();
          i.Next (lsa->Deserialize (i));
          m_lsaList.emplace_back (lsaHeader, lsa);
          m_serializedSize += lsaHeader.GetLength ();
        }
      else if (lsaHeader.GetType () == LsaHeader::SummaryLSAsArea)
        {
          Ptr<SummaryLsa> lsa = Create<SummaryLsa> ();
          i.Next (lsa->Deserialize (i));
          m_lsaList.emplace_back (lsaHeader, lsa);
          m_serializedSize += lsaHeader.GetLength ();
        }
      else if (lsaHeader.GetType () == LsaHeader::ASExternalLSAs)
        {
          Ptr<AsExternalLsa> lsa = Create<AsExternalLsa> ();
          i.Next (lsa->Deserialize (i));
          m_lsaList.emplace_back (lsaHeader, lsa);
          m_serializedSize += lsaHeader.GetLength ();
        }
      else
        {
          NS_ASSERT_MSG (true, "Unsupported LSA Type");
        }
    }
  return m_serializedSize;
}

uint32_t
LsUpdate::Deserialize (Ptr<Packet> packet)
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
