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

#ifndef ROUTER_LSA_H
#define ROUTER_LSA_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "lsa.h"
#include "area-lsa.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief Router LSA
 */
class RouterLink
{
public:
  RouterLink ();
  // Type 1: linkId = Remote Router ID, linkData = Self interface IP
  RouterLink (uint32_t linkId, uint32_t linkData, uint8_t type, uint16_t metric);
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
  bool
  operator== (const RouterLink &other) const
  {
    return m_linkId == other.m_linkId && m_linkData == other.m_linkData && m_type == other.m_type &&
           m_metric == other.m_metric;
  }
  bool
  operator<(const RouterLink &other) const
  {
    return std::tie (m_linkId, m_linkData, m_type, m_metric) <
           std::tie (other.m_linkId, other.m_linkData, other.m_type, other.m_metric);
  }
  RouterLink &
  operator= (const RouterLink &other)
  {
    if (this != &other)
      { // Prevent self-assignment
        m_linkId = other.m_linkId;
        m_linkData = other.m_linkData;
        m_type = other.m_type;
        m_metric = other.m_metric;
      }
    return *this;
  }
  std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>
  Get ()
  {
    return std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> (m_linkId, m_linkData, m_type,
                                                               m_metric);
  }
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
  bool GetBitV (void) const;

  void SetBitE (bool size);
  bool GetBitE (void) const;

  void SetBitB (bool size);
  bool GetBitB (void) const;

  void AddLink (RouterLink routerLink);
  RouterLink GetLink (uint32_t index);
  uint16_t GetNLink ();
  std::vector<uint32_t> GetRouterLinkData ();
  std::vector<AreaLink> GetCrossAreaLinks ();
  void ClearLinks ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual uint32_t Serialize (Buffer::Iterator start) const;
  virtual Ptr<Packet> ConstructPacket () const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t Deserialize (Ptr<Packet> packet);
  virtual Ptr<Lsa> Copy ();

private:
  bool m_bitV;
  bool m_bitE;
  bool m_bitB;
  std::vector<RouterLink> m_links;
};

} // namespace ns3

#endif /* ROUTER_LSA_H */
