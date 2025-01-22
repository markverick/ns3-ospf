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

#ifndef OSPF_INTERFACE_H
#define OSPF_INTERFACE_H

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/header.h"
#include "algorithm"

namespace ns3 {

class NeighberInterface
{
public:
  NeighberInterface (Ipv4Address remoteRouterId_, Ipv4Address remoteIpAddress_)
    : remoteRouterId(remoteRouterId_), remoteIpAddress(remoteIpAddress_)
  {
  }
  // Comparator
  bool operator== (const NeighberInterface &other) const
  {
    return (remoteRouterId == other.remoteRouterId) && (remoteIpAddress == other.remoteIpAddress);
  }
  bool operator< (const NeighberInterface &other) const
  {
    if (remoteRouterId == other.remoteRouterId) {
      return remoteIpAddress < other.remoteIpAddress;
    }
    return remoteRouterId < other.remoteRouterId;
  }

  Ipv4Address remoteRouterId;
  Ipv4Address remoteIpAddress;
};

class OspfInterface: public Object
{
public:
  OspfInterface();

  OspfInterface(Ipv4Address ipAddress, uint16_t helloInterval);

  OspfInterface(Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval);

  OspfInterface(Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval, uint32_t area);

  ~OspfInterface();

  Ipv4Address
  GetAddress() {
    return m_ipAddress;
  }

  void
  SetAddress(Ipv4Address ipAddress) {
    m_ipAddress = ipAddress;
  }

  Ipv4Mask
  GetMask() {
    return m_ipMask;
  }

  void
  SetMask(Ipv4Mask ipMask) {
    m_ipMask = ipMask;
  }

  uint32_t
  GetMetric() {
    return m_metric;
  }

  void
  SetMetric(uint32_t metric) {
    m_metric = metric;
  }

  uint32_t
  GetArea() {
    return m_area;
  }

  void
  SetArea(uint32_t area) {
    m_area = area;
  }

  uint16_t
  GetHelloInterval() {
    return m_helloInterval;
  }

  std::vector<NeighberInterface>
  GetNeighbors() {
    return m_neighbors;
  }

  void AddNeighbor(NeighberInterface neighbor) {
    m_neighbors.emplace_back(neighbor);
  }

  void RemoveNeighbor(NeighberInterface neighbor) {
    m_neighbors.erase(std::find(m_neighbors.begin(), m_neighbors.end(), neighbor));
  }

  bool IsNeighbor(Ipv4Address routerId) {
    for (auto n : m_neighbors) {
      if (n.remoteRouterId == routerId) {
        return true;
      }
    }
    return false;
  }

  bool IsNeighborIp(Ipv4Address remoteIp) {
    for (auto n : m_neighbors) {
      if (n.remoteIpAddress == remoteIp) {
        return true;
      }
    }
    return false;
  }

  //  Vector of <neighbor's routerIds, its own interface ipAddress>
  std::vector<std::pair<uint32_t, uint32_t> > GetNeighborLinks();


  private:
    Ipv4Address m_ipAddress;
    Ipv4Mask m_ipMask;
    uint16_t m_helloInterval;
    uint32_t m_area;
    uint32_t m_metric;
    std::vector<NeighberInterface> m_neighbors;
};
}

#endif /* OSPF_INTERFACE_H */