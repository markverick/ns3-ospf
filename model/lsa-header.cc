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
#include "lsa-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LsaHeader");

NS_OBJECT_ENSURE_REGISTERED (LsaHeader);

LsaHeader::LsaHeader ()
  : m_calcChecksum (false),
    m_lsAge(0),
    m_options(0),
    m_type(0),
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
LsaHeader::GetKey()
{
  return std::make_tuple(m_type, m_lsId, m_advertisingRouter);
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
  m_checksum = i.ReadNtohU16 (); // checksum is disabled for now
  m_length = i.ReadNtohU16 ();
  
  return GetSerializedSize ();
}

} // namespace ns3
