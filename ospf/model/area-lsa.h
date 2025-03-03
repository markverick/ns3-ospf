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

#ifndef AREA_LSA_H
#define AREA_LSA_H

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
class AreaLink
{
public:
  AreaLink ();
  AreaLink (uint32_t areaId, uint32_t originRouter, uint8_t type, uint16_t metric);
  uint32_t m_areaId;
  uint8_t m_originRouter;
  uint8_t m_type;
  uint16_t m_metric;
};

class AreaLsa : public Lsa
{
public:
  /**
   * \brief Construct a router LSA
   */
  AreaLsa ();
  AreaLsa (bool bitV, bool bitE, bool bitB);
  AreaLsa (Ptr<Packet> packet);

  void SetBitV (bool size);
  bool GetBitV (void) const;

  void SetBitE (bool size);
  bool GetBitE (void) const;

  void SetBitB (bool size);
  bool GetBitB (void) const;

  void AddLink (AreaLink AreaLink);
  AreaLink GetLink (uint32_t index);
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
  std::vector<AreaLink> m_links;
};

} // namespace ns3

#endif /* AREA_LSA_H */
