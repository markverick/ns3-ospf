/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2024 University of California, Los Angeles
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
#include "lsa-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsaHeader");

NS_OBJECT_ENSURE_REGISTERED (LsaHeader);

LsaHeader::LsaHeader ()
  : m_calcChecksum (false),
    m_type(0),
    m_options(0),
    m_length (0),
    m_lsId (0),
    m_advertisingRouter (0),
    m_seqNum (0),
    m_checksum (0),
    m_goodChecksum (true),
    m_headerSize(20)
{
}

void
LsaHeader::EnableChecksum (void)
{
  NS_LOG_FUNCTION (this);
  m_calcChecksum = true;
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

bool
LsaHeader::IsChecksumOk (void) const
{
  NS_LOG_FUNCTION (this);
  return m_goodChecksum;
}

std::string 
LsaHeader::LsTypeToString (LsType type) const
{
  NS_LOG_FUNCTION (this << type);
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
    .AddConstructor<LsaHeader> ()
  ;
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
     << "type " << LsTypeToString(LsType(m_type)) << " "
     << "link state id: " << m_lsId << " "
     << "advertising router: " << m_advertisingRouter << " "
     << "sequence number: " << m_seqNum << " "
     << "lsa length: " << m_length << " "
  ;
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

  m_lsAge = i.ReadNtohU16 ();
  m_options = i.ReadU8 ();
  m_type = i.ReadU8 ();
  m_lsId = i.ReadNtohU32 ();
  m_advertisingRouter = i.ReadNtohU32 ();
  m_seqNum = i.ReadNtohU32 ();
  m_checksum = i.ReadU16 (); // checksum is disabled for now
  m_length = i.ReadU16 ();
  
  return GetSerializedSize ();
}

} // namespace ns3