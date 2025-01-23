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

#ifndef OSPF_NEIGHBOR_H
#define OSPF_NEIGHBOR_H

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
/**
 * \ingroup ospf
 *
 * \brief OSPF Interface's Neighbor
 */
class OspfNeighbor : public Object
{
public:
   /**
   * \enum NeighborState
   * \brief Neighbor state machine
   *
   * The values correspond to the OSPF neighbor state machine in \RFC{2328}.
   */
  enum NeighborState
  {
    Down = 0x1,
    Attempt = 0x2, // for multi-access
    Init = 0x3, // received hello, one-way
    TwoWay = 0x4, // received two-way for multi-access
    ExStart = 0x5,  // after received two-way for point-to-point or designated/backup routers are agreed for multi-access
    Exchange = 0x6, // after agreed on slave/master relation and starting seqNum
    Loading = 0x7, // unused. loading is instantaneus in the simulation
    Full = 0x8, // exchange is done
  };

  // Nodes in the same subnet may have different area ID in this area proxy modification
  OspfNeighbor (Ipv4Address routerId, Ipv4Address ipAddress, uint32_t area);
  OspfNeighbor (Ipv4Address routerId, Ipv4Address ipAddress, uint32_t area, NeighborState state);

  Ipv4Address GetRouterId();
  void SetRouterId(Ipv4Address routerId);
  void SetRouterId(uint32_t routerId);

  Ipv4Address GetIpAddress();
  void SetIpAddress(Ipv4Address ipAddress);
  void SetIpAddress(uint32_t ipAddress);

  uint32_t GetArea();
  void SetArea(uint32_t area);

  NeighborState GetState();
  void SetState(NeighborState);

  void RefreshLastHelloReceived ();




  Ipv4Address m_routerId;
  Ipv4Address m_ipAddress;
  uint32_t m_area;
  NeighborState m_state;
  Time m_lastHelloReceived;
};

} // namespace ns3

#endif /* OSPF_NEIGHBOR_H */
