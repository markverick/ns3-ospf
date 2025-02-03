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

#ifndef OSPF_HEADER_H
#define OSPF_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief Packet header for OSPF
 */
class OspfHeader : public Header 
{
public:
  /**
   * \brief Construct a null OSPF header
   */
  OspfHeader ();
  /**
   * \brief Enable checksum calculation for this header.
   */
  void EnableChecksum (void);
  /**
   * \enum OspfType
   * \brief OSPF Packet Types
   *
   * The values correspond to the OSPF packet header's type in \RFC{2328}.
   */
  enum OspfType
    {
      OspfHello = 0x1,
      OspfDBD = 0x2,
      OspfLSRequest = 0x3,
      OspfLSUpdate = 0x4,
      OspfLSAck = 0x5
    };

  /**
   * \brief Set Ospf Type Field
   * \param type OSPF Type value
   */
  void SetType (OspfType type);
  /**
   * \returns the type of OSPF
   */
  OspfType GetType(void) const;

  /**
   * \param size the size of the payload in bytes
   */
  void SetPayloadSize (uint16_t size);
  /**
   * \returns the size of the payload in bytes
   */
  uint16_t GetPayloadSize(void) const;

  /**
   * \param routerId the router ID of the packet's source
   */
  void SetRouterId (uint32_t routerId);
  /**
   * \returns the router ID
   */
  uint32_t GetRouterId(void) const;

  /**
   * \param area the area ID
   */
  void SetArea (uint32_t area);
  /**
   * \returns the area ID
   */
  uint32_t GetArea(void) const;

  /**
   * \returns true if the ipv4 checksum is correct, false otherwise.
   *
   * If OspfHeader::EnableChecksums has not been called prior to
   * deserializing this header, this method will always return true.
   */
  bool IsChecksumOk (void) const;

  std::string OspfTypeToString (OspfType type) const;

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
private:

  bool m_calcChecksum; //!< true if the checksum must be calculated

  uint8_t m_version; //!< OSPF version number
  uint8_t m_type; //!< OSPF packet type
  uint16_t m_payloadSize; //!< payload size in bytes, excluding the OSPF header
  uint32_t m_routerId; //!< router ID of the packet's source
  uint32_t m_area; //!< area ID
  uint16_t m_checksum; //!< checksum
  bool m_goodChecksum; //!< true if checksum is correct
  uint16_t m_autype; //!< authentication type
  uint64_t m_authentication; //!< authentication
  uint16_t m_headerSize; //!< OSPF header size
};

} // namespace ns3


#endif /* OSPF_HEADER_H */
