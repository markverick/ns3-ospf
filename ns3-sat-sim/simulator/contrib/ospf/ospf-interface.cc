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

NS_LOG_COMPONENT_DEFINE ("OSPFInterface");

OSPFInterface::OSPFInterface() {
  m_ipAddress = Ipv4Address::GetAny();
  m_ipMask = Ipv4Mask(0xffffff00);
  m_helloInterval = 0;
}

OSPFInterface::~OSPFInterface() {

}

OSPFInterface::OSPFInterface(Ipv4Address ipAddress, uint16_t helloInterval) {
  NS_LOG_FUNCTION(this << ipAddress << helloInterval);
  m_ipAddress = ipAddress;
  m_ipMask = Ipv4Mask(0xffffff00);
  m_helloInterval = helloInterval;
}

OSPFInterface::OSPFInterface(Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval) {
  NS_LOG_FUNCTION(this << ipAddress << ipMask << helloInterval);
  m_ipAddress = ipAddress;
  m_ipMask = ipMask;
  m_helloInterval = helloInterval;
}

// Only one LSA per interface for point-to-point
// Vector of <subnet, mask, neighbor's router ID>
std::vector<std::tuple<uint32_t, uint32_t, uint32_t> >
OSPFInterface::GetLSAdvertisement() {
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > lsAdvertisements;
  auto neighbors = GetNeighbors();
  for (auto n : neighbors) {
    lsAdvertisements.emplace_back(m_ipAddress.CombineMask(m_ipMask).Get(), m_ipMask.Get(), n.remoteRouterId.Get());
  }
  return lsAdvertisements;
}

}