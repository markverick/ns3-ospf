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

#ifndef OSPF_DBD_H
#define OSPF_DBD_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/lsa-header.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief Database Description Payload
 */

class OspfDbd : public Object
{
public:
  /**
   * \brief Construct a router LSA
   */
  OspfDbd ();
  OspfDbd (uint16_t mtu, uint8_t options, uint8_t flags, bool bitI, bool bitM, bool bitMS, uint32_t ddSeqNum);
  OspfDbd (Ptr<Packet> packet);

  void SetMtu (uint16_t mtu);
  uint16_t GetMtu() const;

  void SetOptions (uint8_t options);
  uint8_t GetOptions() const;

  bool IsNegotiate () const;

  void SetBitI (bool bitI);
  bool GetBitI() const;

  void SetBitM (bool bitM);
  bool GetBitM() const;

  void SetBitMS (bool bitMS);
  bool GetBitMS() const;

  void SetFlags(uint8_t field);
  uint8_t GetFlags() const;

  void SetDDSeqNum (uint32_t ddSeqNum);
  uint32_t GetDDSeqNum(void) const;

  void AddLsaHeader (LsaHeader lsaHeader);
  void ClearLsaHeader (void);
  bool HasLsaHeader (LsaHeader lsaHeader);
  LsaHeader SetLsaHeaders (std::vector<LsaHeader> lsaHeaders);

  LsaHeader GetLsaHeader (uint32_t index);
  std::vector<LsaHeader> GetLsaHeaders ();
  uint32_t GetNLsaHeaders ();


  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual uint32_t Serialize (Buffer::Iterator start) const;
  virtual Ptr<Packet> ConstructPacket () const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t Deserialize (Ptr<Packet> packet);
private:

  uint16_t m_mtu;
  uint8_t m_options;
  uint8_t m_flags;
  bool m_bitI;
  bool m_bitM;
  bool m_bitMS;
  uint32_t m_ddSeqNum;
  std::vector<LsaHeader> m_lsaHeaders;
};

} // namespace ns3


#endif /* OSPF_DBD_H */
