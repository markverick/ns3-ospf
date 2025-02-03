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

#ifndef LSA_HEADER_H
#define LSA_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief Packet header for LSA
 */
class LsaHeader : public Header 
{
public:
  /**
   * \brief Construct a null LSA header
   */
  LsaHeader ();
  /**
   * \brief Enable checksum calculation for this header.
   */
  void EnableChecksum (void);

  void SetLsAge (uint16_t lsAge);
  uint16_t GetLsAge(void) const;

  /**
   * \enum LsType
   * \brief LSA Packet Types
   *
   * The values correspond to the LSA packet header's type in \RFC{2328}.
   */
  enum LsType
    {
      RouterLSAs = 0x1,
      NetworkLSAs = 0x2,
      SummaryLSAsIP = 0x3,
      SummaryLSAsASBR = 0x4,
      ASExternalLSAs = 0x5
    };

  typedef std::tuple<uint8_t, uint32_t, uint32_t> LsaKey;

  /**
   * \brief Set Ospf Type Field
   * \param type OSPF Type value
   */
  void SetType (LsType type);
  /**
   * \returns the type of LSA
   */
  LsType GetType(void) const;

  /**
   * \param length the number of LSAs
   */
  void SetLength (uint16_t length);

  /**
   * \returns the number of LSAs
   */
  uint16_t GetLength(void) const;

  /**
   * \param lsId Link State ID
   */
  void SetLsId (uint32_t lsId);

  /**
   * \returns Link State ID
   */
  uint32_t GetLsId(void) const;

  void SetAdvertisingRouter (uint32_t advertisingRouter);
  uint32_t GetAdvertisingRouter(void) const;

  LsaKey GetKey();

  void SetSeqNum (uint32_t seqNum);
  uint32_t GetSeqNum(void) const;

  /**
   * \returns true if the ipv4 checksum is correct, false otherwise.
   *
   * If LsaHeader::EnableChecksums has not been called prior to
   * deserializing this header, this method will always return true.
   */
  bool IsChecksumOk (void) const;

  std::string LsTypeToString (LsType type) const;

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

  uint16_t m_lsAge; //!< time in seconds since the LSA was originated
  uint8_t m_options; //!< options
  uint8_t m_type; //!< LSA type
  uint16_t m_length; //!< number of LSAs
  uint32_t m_lsId; //!< link state ID
  uint32_t m_advertisingRouter; //!< advertising router
  uint32_t m_seqNum; //!< link state sequence number
  uint16_t m_checksum; //!< checksum
  bool m_goodChecksum; //!< true if checksum is correct
  uint16_t m_headerSize; //!< LSA header size
};

} // namespace ns3


#endif /* LSA_HEADER_H */
