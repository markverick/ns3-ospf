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

#ifndef L2_SUMMARY_LSA_H
#define L2_SUMMARY_LSA_H

#include <set>
#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "lsa.h"

namespace ns3 {
class SummaryRoute
{
public:
  SummaryRoute ();
  SummaryRoute (uint32_t address, uint32_t mask, uint32_t metric);
  uint32_t m_address;
  uint32_t m_mask;
  uint32_t m_metric;
  bool
  operator== (const SummaryRoute &other) const
  {
    return m_address == other.m_address && m_mask == other.m_mask && m_metric == other.m_metric;
  }
  bool
  operator< (const SummaryRoute &other) const
  {
    return std::tie (m_address, m_mask, m_metric) <
           std::tie (other.m_address, other.m_mask, other.m_metric);
  }
  SummaryRoute &
  operator= (const SummaryRoute &other)
  {
    if (this != &other)
      { // Prevent self-assignment
        m_address = other.m_address;
        m_mask = other.m_mask;
        m_metric = other.m_metric;
      }
    return *this;
  };
  std::tuple<uint32_t, uint32_t, uint32_t>
  Get ()
  {
    return std::tuple<uint32_t, uint32_t, uint32_t> (m_address, m_mask, m_metric);
  }
};
/**
 * \ingroup ospf
 *
 * \brief Summary LSA
 */

class L2SummaryLsa : public Lsa
{
public:
  /**
   * \brief Construct a Summary LSA
   */
  L2SummaryLsa ();
  L2SummaryLsa (Ptr<Packet> packet);

  void AddRoute (SummaryRoute route);
  std::set<SummaryRoute> GetRoutes ();
  uint32_t GetNRoute ();

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
  std::set<SummaryRoute> m_routes;
};

} // namespace ns3

#endif /* L2_SUMMARY_LSA_H */
