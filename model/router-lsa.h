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

#ifndef ROUTER_LSA_H
#define ROUTER_LSA_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "lsa.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief Router LSA
 */
class RouterLink
{
public:
  RouterLink();
  RouterLink(uint32_t linkId, uint32_t linkData, uint8_t type, uint16_t metric);
  uint32_t m_linkId;
  /*
      Type   Link ID
    ______________________________________
    1      Neighboring router's Router ID
    2      IP address of Designated Router
    3      IP network/subnet number
    4      Neighboring router's Router ID
  */
  uint32_t m_linkData;
  uint8_t m_type;
  uint16_t m_metric;
};

class RouterLsa : public Lsa
{
public:
  /**
   * \brief Construct a router LSA
   */
  RouterLsa ();
  RouterLsa (bool bitV, bool bitE, bool bitB);
  RouterLsa (Ptr<Packet> packet);


  void SetBitV (bool size);
  bool GetBitV(void) const;

  void SetBitE (bool size);
  bool GetBitE(void) const;

  void SetBitB (bool size);
  bool GetBitB(void) const;

  void AddLink (RouterLink routerLink);
  RouterLink GetLink (uint32_t index);
  uint16_t GetNLink ();
  void ClearLinks ();


  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual uint32_t Serialize (Buffer::Iterator start) const;
  virtual Ptr<Packet> ConstructPacket () const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t Deserialize (Ptr<Packet> packet);
private:

  bool m_bitV;
  bool m_bitE;
  bool m_bitB;
  std::vector<RouterLink> m_links;
};

} // namespace ns3


#endif /* ROUTER_LSA_H */
