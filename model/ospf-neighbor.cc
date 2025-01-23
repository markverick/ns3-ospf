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

#include "ospf-neighbor.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfNeighbor");

OspfNeighbor::OspfNeighbor (Ipv4Address remoteRouterId, Ipv4Address ipAddress, uint32_t area)
  : m_routerId(remoteRouterId),
    m_ipAddress(ipAddress),
    m_area(area)
{
}

OspfNeighbor::OspfNeighbor(Ipv4Address remoteRouterId, Ipv4Address ipAddress, uint32_t area, OspfNeighbor::NeighborState state)
  : m_routerId(remoteRouterId),
    m_ipAddress(ipAddress),
    m_area(area),
    m_state(state)
{
}

Ipv4Address
OspfNeighbor::GetRouterId() {
  return m_routerId;
}

void
OspfNeighbor::SetRouterId(Ipv4Address routerId) {
  m_routerId = routerId;
}

void
OspfNeighbor::SetRouterId(uint32_t routerId) {
  m_routerId = Ipv4Address(routerId);
}

Ipv4Address
OspfNeighbor::GetIpAddress() {
  return m_ipAddress;
}

void
OspfNeighbor::SetIpAddress(Ipv4Address ipAddress) {
  m_ipAddress = ipAddress;
}

void
OspfNeighbor::SetIpAddress(uint32_t ipAddress) {
  m_ipAddress = Ipv4Address(ipAddress);
}

uint32_t
OspfNeighbor::GetArea() {
  return m_area;
}

void
OspfNeighbor::SetArea(uint32_t area) {
  m_area = area;
}

OspfNeighbor::NeighborState
OspfNeighbor::GetState() {
  return m_state;
}

void
OspfNeighbor::SetState(OspfNeighbor::NeighborState state) {
  m_state = state;
}

void
OspfNeighbor::RefreshLastHelloReceived () {
  m_lastHelloReceived = Simulator::Now();
}


}