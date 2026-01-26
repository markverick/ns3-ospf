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
#include "lsa-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsaHeader");

NS_OBJECT_ENSURE_REGISTERED (LsaHeader);

LsaHeader::LsaHeader ()
    : m_calcChecksum (false),
      m_lsAge (0),
      m_options (0),
      m_type (0),
      m_length (0),
      m_lsId (0),
      m_advertisingRouter (0),
      m_seqNum (0),
      m_checksum (0),
      m_goodChecksum (true),
      m_headerSize (20)
{
}

LsaHeader::LsaHeader (LsaKey lsaKey)
    : m_calcChecksum (false),
      m_lsAge (0),
      m_options (0),
      m_type (std::get<0> (lsaKey)),
      m_length (0),
      m_lsId (std::get<1> (lsaKey)),
      m_advertisingRouter (std::get<2> (lsaKey)),
      m_seqNum (0),
      m_checksum (0),
      m_goodChecksum (true),
      m_headerSize (20)
{
}

void
LsaHeader::EnableChecksum (void)
{
  NS_LOG_FUNCTION (this);
  m_calcChecksum = true;
}

void
LsaHeader::SetLsAge (uint16_t lsAge)
{
  NS_LOG_FUNCTION (this << lsAge);
  m_lsAge = lsAge;
}

uint16_t
LsaHeader::GetLsAge (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lsAge;
}

void
LsaHeader::SetType (LsType type)
{
  NS_LOG_FUNCTION (this << type);
  m_type = type;
}

LsaHeader::LsType
LsaHeader::GetType (void) const
{
  NS_LOG_FUNCTION (this);
  return LsType (m_type);
}

void
LsaHeader::SetLength (uint16_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_length = size;
}
uint16_t
LsaHeader::GetLength (void) const
{
  NS_LOG_FUNCTION (this);
  return m_length;
}

void
LsaHeader::SetSeqNum (uint32_t seqNum)
{
  NS_LOG_FUNCTION (this << seqNum);
  m_seqNum = seqNum;
}
uint32_t
LsaHeader::GetSeqNum (void) const
{
  NS_LOG_FUNCTION (this);
  return m_seqNum;
}

void
LsaHeader::SetLsId (uint32_t lsId)
{
  NS_LOG_FUNCTION (this << lsId);
  m_lsId = lsId;
}
uint32_t
LsaHeader::GetLsId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lsId;
}

void
LsaHeader::SetAdvertisingRouter (uint32_t advertisingRouter)
{
  NS_LOG_FUNCTION (this << advertisingRouter);
  m_advertisingRouter = advertisingRouter;
}
uint32_t
LsaHeader::GetAdvertisingRouter (void) const
{
  NS_LOG_FUNCTION (this);
  return m_advertisingRouter;
}

// Get the unique key <LS Type, Link-State ID, Advertising Router>
LsaHeader::LsaKey
LsaHeader::GetKey ()
{
  return std::make_tuple (m_type, m_lsId, m_advertisingRouter);
}

std::string
LsaHeader::GetKeyString (LsaKey lsaKey)
{
  std::stringstream ss;
  auto k0 = LsaHeader::LsTypeToString (LsaHeader::LsType (std::get<0> (lsaKey)));
  auto k1 = Ipv4Address (std::get<1> (lsaKey));
  auto k2 = Ipv4Address (std::get<2> (lsaKey));
  ss << k0 << "-" << k1 << "-" << k2;
  return ss.str ();
}

std::string
LsaHeader::GetKeyString (uint32_t seqNum, LsaKey lsaKey)
{
  return std::to_string (seqNum) + "-" + GetKeyString (lsaKey);
}

bool
LsaHeader::IsChecksumOk (void) const
{
  NS_LOG_FUNCTION (this);
  return m_goodChecksum;
}

std::string
LsaHeader::LsTypeToString (LsType type)
{
  NS_LOG_FUNCTION (type);
  switch (type)
    {
    case RouterLSAs:
      return "Router-LSAs";
    case NetworkLSAs:
      return "Network-LSAs";
    case SummaryLSAsIP:
      return "Summary-LSAs (IP network)";
    case SummaryLSAsASBR:
      return "Summary-LSAs (ASBR)";
    case ASExternalLSAs:
      return "AS-external-LSAs";
    case AreaLSAs:
      return "Area-LSAs";
    case L1SummaryLSAs:
      return "L1-Summary-LSAs";
    case L2SummaryLSAs:
      return "L2-Summary-LSAs";
    default:
      return "Unrecognized LSA Type";
    };
}

TypeId
LsaHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LsaHeader")
                          .SetParent<Header> ()
                          .SetGroupName ("Ospf")
                          .AddConstructor<LsaHeader> ();
  return tid;
}
TypeId
LsaHeader::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
LsaHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "age " << m_lsAge << " "
     << "options " << m_options << " "
     << "type " << LsTypeToString (LsType (m_type)) << " "
     << "link state id: " << m_lsId << " "
     << "advertising router: " << m_advertisingRouter << " "
     << "sequence number: " << m_seqNum << " "
     << "lsa length: " << m_length << " ";
}
uint32_t
LsaHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_headerSize;
}

void
LsaHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  i.WriteHtonU16 (m_lsAge);
  i.WriteU8 (m_options);
  i.WriteU8 (m_type);
  i.WriteHtonU32 (m_lsId);
  i.WriteHtonU32 (m_advertisingRouter);
  i.WriteHtonU32 (m_seqNum);

  i.WriteHtonU16 (0); // checksum is disabled for now
  i.WriteHtonU16 (m_length);
}
uint32_t
LsaHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  if (i.GetRemainingSize () < m_headerSize)
    {
      NS_LOG_WARN ("LSA header truncated");
      return 0;
    }

  m_lsAge = i.ReadNtohU16 ();
  m_options = i.ReadU8 ();
  m_type = i.ReadU8 ();
  m_lsId = i.ReadNtohU32 ();
  m_advertisingRouter = i.ReadNtohU32 ();
  m_seqNum = i.ReadNtohU32 ();
  m_checksum = i.ReadNtohU16 (); // checksum is disabled for now
  m_length = i.ReadNtohU16 ();

  return GetSerializedSize ();
}

LsaHeader
LsaHeader::Copy ()
{
  // Not very optimized way of copying
  Buffer buff;
  buff.AddAtStart (GetSerializedSize ());
  LsaHeader copy;
  Serialize (buff.Begin ());
  copy.Deserialize (buff.Begin ());
  return copy;
}

} // namespace ns3
