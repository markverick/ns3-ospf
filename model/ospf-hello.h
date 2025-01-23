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
 * \brief OSPF  Hello Packet
 */

class OspfHello : public Object
{
public:
  /**
   * \brief Construct a router LSA
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
