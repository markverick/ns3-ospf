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

#include "ospf-interface.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfInterface");

OspfInterface::OspfInterface() {
  m_ipAddress = Ipv4Address::GetAny();
  m_ipMask = Ipv4Mask(0xffffff00); // default to /24
  m_helloInterval = 0;
  m_area = 0;
  m_metric = 1;
}

OspfInterface::~OspfInterface() {

}

OspfInterface::OspfInterface(Ipv4Address ipAddress, uint16_t helloInterval) {
  NS_LOG_FUNCTION(this << ipAddress << helloInterval);
  m_ipAddress = ipAddress;
  m_ipMask = Ipv4Mask(0xffffff00);
  m_helloInterval = helloInterval;
}

OspfInterface::OspfInterface(Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval) {
  NS_LOG_FUNCTION(this << ipAddress << ipMask << helloInterval);
  m_ipAddress = ipAddress;
  m_ipMask = ipMask;
  m_helloInterval = helloInterval;
}

OspfInterface::OspfInterface(Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval, uint32_t area) {
  NS_LOG_FUNCTION(this << ipAddress << ipMask << helloInterval);
  m_ipAddress = ipAddress;
  m_ipMask = ipMask;
  m_helloInterval = helloInterval;
  m_area = area;
}

// Get a list of <neighbor's router ID, router's IP address> as a vector
std::vector<std::pair<uint32_t, uint32_t> >
OspfInterface::GetNeighborLinks() {
  std::vector<std::pair<uint32_t, uint32_t> > links;
  auto neighbors = GetNeighbors();
  for (auto n : neighbors) {
    links.emplace_back(n.remoteRouterId.Get(), m_ipAddress.Get());
  }
  return links;
}

}