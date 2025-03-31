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
#include "ns3/router-lsa.h"
#include "ospf-neighbor.h"
#include "algorithm"

namespace ns3 {

class OspfInterface : public Object
{
public:
  OspfInterface ();
  OspfInterface (Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval,
                 uint32_t routerDeadInterval, uint32_t area, uint32_t metric, uint32_t mtu);

  ~OspfInterface ();

  Ipv4Address GetAddress ();
  void SetAddress (Ipv4Address ipAddress);

  Ipv4Address GetGateway ();
  void SetGateway (Ipv4Address ipAddress);

  Ipv4Mask GetMask ();
  void SetMask (Ipv4Mask ipMask);

  uint32_t GetMetric ();
  void SetMetric (uint32_t metric);

  uint32_t GetArea ();
  void SetArea (uint32_t area);

  uint32_t GetMtu ();
  void SetMtu (uint32_t mtu);

  uint16_t GetHelloInterval ();
  void SetHelloInterval (uint16_t helloInterval);

  uint32_t GetRouterDeadInterval ();
  void SetRouterDeadInterval (uint32_t routerDeadInterval);

  Ptr<OspfNeighbor> GetNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  std::vector<Ptr<OspfNeighbor>> GetNeighbors ();

  Ptr<OspfNeighbor> AddNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  Ptr<OspfNeighbor> AddNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp,
                                 uint32_t remoteAreaId, OspfNeighbor::NeighborState state);

  bool RemoveNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  bool IsNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  //  Vector of <neighbor's routerIds, its own interface ipAddress>
  std::vector<RouterLink> GetActiveRouterLinks ();

private:
  Ipv4Address m_ipAddress;
  Ipv4Address m_gateway;
  Ipv4Mask m_ipMask;
  uint16_t m_helloInterval;
  uint32_t m_routerDeadInterval;
  uint32_t m_area;
  uint32_t m_metric;
  uint32_t m_mtu;
  std::vector<Ptr<OspfNeighbor>> m_neighbors;
};
} // namespace ns3

#endif /* OSPF_INTERFACE_H */