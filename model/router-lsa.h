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

#ifndef ROUTER_LSA_H
#define ROUTER_LSA_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"

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

class RouterLsa : public Object
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


#endif /* OSPF_HEADER_H */
