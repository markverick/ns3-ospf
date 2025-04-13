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

#ifndef AS_EXTERNAL_LSA_H
#define AS_EXTERNAL_LSA_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "lsa.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief AS External LSA
 */
class ExternalRoute
{
public:
  ExternalRoute ();
  ExternalRoute (uint32_t address);

  ExternalRoute (uint32_t address, uint32_t routeTag);
  uint32_t m_address;
  uint32_t m_routeTag;
};

class AsExternalLsa : public Lsa
{
public:
  /**
   * \brief Construct a Summary LSA
   */
  AsExternalLsa ();
  AsExternalLsa (uint32_t mask, uint32_t metric);
  AsExternalLsa (Ptr<Packet> packet);

  void SetMask (uint32_t mask);
  uint32_t GetMask ();

  void SetMetric (uint32_t metric);
  uint32_t GetMetric ();

  void AddRoute (ExternalRoute route);
  ExternalRoute GetRoute (uint32_t index);
  uint16_t GetNRoutes ();
  void ClearRoutes ();

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
  std::vector<ExternalRoute> m_routes;
  uint32_t m_mask;
  uint32_t m_metric;
  // TODO: Support bitE
};

} // namespace ns3

#endif /* AS_EXTERNAL_LSA_H */
