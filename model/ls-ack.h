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

#ifndef LS_ACK_H
#define LS_ACK_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ospf-interface.h"
#include "lsa-header.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief LS Acknowledge Object
 */

class LsAck : public Object
{
public:
  /**
   * \brief Construct a LS Acknowledge Object
   */

  LsAck ();
  LsAck (std::vector<LsaHeader> lsaHeaders);
  LsAck (Ptr<Packet> packet);

  void AddLsaHeader (LsaHeader lsaHeader);
  void ClearLsaHeaders (void);
  bool HasLsaHeader (LsaHeader lsaHeader);

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
  std::vector<LsaHeader> m_lsaHeaders; //storing neighbor's router ID
};

} // namespace ns3


#endif /* LS_ACK_H */
