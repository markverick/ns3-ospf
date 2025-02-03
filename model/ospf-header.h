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
