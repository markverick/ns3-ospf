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

#ifndef OSPF_HELLO_H
#define OSPF_HELLO_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ospf-interface.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief OSPF Hello Object
 */

class OspfHello : public Object
{
public:
  /**
   * \brief Construct a Hello object
   */

  OspfHello ();
  OspfHello (uint32_t mask, uint16_t helloInterval,
         uint32_t routerDeadInterval);
  OspfHello (uint32_t mask, uint16_t helloInterval,
         uint32_t routerDeadInterval, std::vector<uint32_t> neighbors);
  OspfHello (Ptr<Packet> packet);

  void SetMask (uint32_t mask);
  uint32_t GetMask(void) const;

  void SetHelloInterval (uint16_t helloInterval);
  uint16_t GetHelloInterval(void) const;

  void SetOptions (uint8_t options);
  uint8_t GetOptions(void) const;

  void SetRouterPriority (uint8_t routerPriority);
  uint8_t GetRouterPriority(void) const;
  
  void SetRouterDeadInterval (uint32_t routerDeadInterval);
  uint32_t GetRouterDeadInterval(void) const;

  void SetDesignatedRouter (uint32_t designatedRouter);
  uint32_t GetDesignatedRouter(void) const;

  void SetBackupDesignatedRouter (uint32_t backupDesignatedRouter);
  uint32_t GetBackupDesignatedRouter(void) const;

  void AddNeighbor (uint32_t neighborRouterId);
  void ClearNeighbor (void);
  bool IsNeighbor (uint32_t neighborRouterId);
  uint32_t SetNeighbors (std::vector<uint32_t> neighborRouterId);

  uint32_t GetNeighbor (uint32_t index);
  uint32_t GetNNeighbors ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual uint32_t Serialize (Buffer::Iterator start) const;
  virtual Ptr<Packet> ConstructPacket () const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t Deserialize (Ptr<Packet> packet);
private:

  uint32_t m_mask;
  uint16_t m_helloInterval;
  uint8_t m_options;
  uint8_t m_routerPriority; // For DR/BDR
  uint32_t m_routerDeadInterval;
  uint32_t m_dr;
  uint32_t m_bdr;
  std::vector<uint32_t> m_neighbors; //storing neighbor's router ID
};

} // namespace ns3


#endif /* OSPF_HELLO_H */
