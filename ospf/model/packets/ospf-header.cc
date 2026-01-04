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
#include "ospf-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfHeader");

NS_OBJECT_ENSURE_REGISTERED (OspfHeader);

OspfHeader::OspfHeader ()
    : m_calcChecksum (false),
      m_type (0),
      m_payloadSize (0),
      m_routerId (0),
      m_area (0),
      m_checksum (0),
      m_goodChecksum (true),
      m_headerSize (24)
{
}

void
OspfHeader::EnableChecksum (void)
{
  NS_LOG_FUNCTION (this);
  m_calcChecksum = true;
}

void
OspfHeader::SetType (OspfType type)
{
  NS_LOG_FUNCTION (this << type);
  m_type = type;
}

OspfHeader::OspfType
OspfHeader::GetType (void) const
{
  NS_LOG_FUNCTION (this);
  return OspfType (m_type);
}

void
OspfHeader::SetPayloadSize (uint16_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_payloadSize = size;
}
uint16_t
OspfHeader::GetPayloadSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_payloadSize;
}

void
OspfHeader::SetRouterId (uint32_t routerId)
{
  NS_LOG_FUNCTION (this << routerId);
  m_routerId = routerId;
}
uint32_t
OspfHeader::GetRouterId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_routerId;
}

void
OspfHeader::SetArea (uint32_t area)
{
  NS_LOG_FUNCTION (this << area);
  m_area = area;
}
uint32_t
OspfHeader::GetArea (void) const
{
  NS_LOG_FUNCTION (this);
  return m_area;
}

bool
OspfHeader::IsChecksumOk (void) const
{
  NS_LOG_FUNCTION (this);
  return m_goodChecksum;
}

std::string
OspfHeader::OspfTypeToString (OspfType type) const
{
  NS_LOG_FUNCTION (this << type);
  switch (type)
    {
    case OspfHello:
      return "Hello";
    case OspfDBD:
      return "Database Description";
    case OspfLSRequest:
      return "Link State Request";
    case OspfLSUpdate:
      return "Link State Update";
    case OspfLSAck:
      return "Link State Acknowledgment";
    default:
      return "Unrecognized OSPF Type";
    };
}

TypeId
OspfHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OspfHeader")
                          .SetParent<Header> ()
                          .SetGroupName ("Ospf")
                          .AddConstructor<OspfHeader> ();
  return tid;
}
TypeId
OspfHeader::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}
void
OspfHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "version 2"
     << " "
     << "type " << OspfTypeToString (OspfType (m_type)) << " "
     << "length: " << m_payloadSize + m_headerSize << " "
     << "router id: " << m_routerId << " "
     << "area id: " << m_area << " ";
}
uint32_t
OspfHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_headerSize;
}

void
OspfHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  i.WriteU8 (2);
  i.WriteU8 (m_type);
  i.WriteHtonU16 (m_payloadSize + m_headerSize);
  i.WriteHtonU32 (m_routerId);
  i.WriteHtonU32 (m_area);

  if (m_calcChecksum)
    {
      i = start;
      uint16_t checksum = i.CalculateIpChecksum (12);
      NS_LOG_LOGIC ("checksum=" << checksum);
      i = start;
      i.Next (12);
      i.WriteHtonU16 (checksum);
    }
  i.WriteU16 (0);
  i.WriteU64 (0);
}
uint32_t
OspfHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  const uint32_t available = start.GetRemainingSize ();
  Buffer::Iterator i = start;

  if (available < m_headerSize)
    {
      NS_LOG_WARN ("OSPF header truncated");
      return 0;
    }

  uint8_t ver = i.ReadU8 ();

  if (ver != 2)
    {
      NS_LOG_WARN ("Trying to decode a non-OSPF header, refusing to do it.");
      return 0;
    }

  m_type = i.ReadU8 ();
  uint16_t size = i.ReadNtohU16 ();

  if (size < m_headerSize)
    {
      NS_LOG_WARN ("OSPF header length is smaller than header size");
      return 0;
    }

  if (size > available)
    {
      NS_LOG_WARN ("OSPF header length exceeds available bytes");
      return 0;
    }
  m_payloadSize = size - m_headerSize;
  m_routerId = i.ReadNtohU32 ();
  m_area = i.ReadNtohU32 ();
  m_checksum = i.ReadNtohU16 ();

  // Consume the remaining bytes of the fixed-size OSPF header.
  i.ReadU16 ();
  i.ReadU64 ();

  if (m_calcChecksum)
    {
      i = start;
      uint16_t checksum = i.CalculateIpChecksum (m_headerSize);
      NS_LOG_LOGIC ("checksum=" << checksum);

      m_goodChecksum = (checksum == 0);
    }
  return GetSerializedSize ();
}

} // namespace ns3
