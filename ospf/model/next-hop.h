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

#ifndef OSPF_NEXT_HOP
#define OSPF_NEXT_HOP

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/packet.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief Generic LSA
 */

class NextHop
{
public:
  NextHop () : ifIndex (0), ipAddress (Ipv4Address::GetZero ()), metric (0){};
  NextHop (uint32_t ifIndex_, uint32_t ipAddress_, uint16_t metric_)
      : ifIndex (ifIndex_), ipAddress (Ipv4Address (ipAddress_)), metric (metric_){};
  NextHop (uint32_t ifIndex_, Ipv4Address ipAddress_, uint16_t metric_)
      : ifIndex (ifIndex_), ipAddress (ipAddress_), metric (metric_){};
  uint32_t ifIndex;
  Ipv4Address ipAddress;
  uint16_t metric;
  bool
  operator== (const NextHop &other) const
  {
    return ifIndex == other.ifIndex && ipAddress == other.ipAddress && metric == other.metric;
  }
  bool
  operator<(const NextHop &other) const
  {
    return std::tie (ifIndex, ipAddress, metric) <
           std::tie (other.ifIndex, other.ipAddress, other.metric);
  }
  NextHop &
  operator= (const NextHop &other)
  {
    if (this != &other)
      {
        ifIndex = other.ifIndex;
        ipAddress = other.ipAddress;
        metric = other.metric;
      }
    return *this;
  }
};
} // namespace ns3

#endif /* OSPF_NEXT_HOP */