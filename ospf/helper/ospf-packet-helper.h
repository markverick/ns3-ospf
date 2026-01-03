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

#ifndef OSPF_PACKET_HELPER_H
#define OSPF_PACKET_HELPER_H

#include "ns3/log.h"
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
#include "ns3/lsa-header.h"
#include "ns3/ospf-header.h"
#include "ns3/ospf-interface.h"
#include "ns3/ospf-neighbor.h"
#include "ns3/router-lsa.h"
#include "ns3/area-lsa.h"
#include "ns3/l2-summary-lsa.h"
#include "ns3/ospf-hello.h"
#include "ns3/ospf-dbd.h"
#include "ns3/ls-ack.h"

namespace ns3 {

Ptr<Packet> ConstructHelloPacket (Ipv4Address routerId, uint32_t areaId, Ipv4Mask mask,
                                  uint16_t m_helloInterval, uint32_t routerDeadInterval,
                                  std::vector<Ptr<OspfNeighbor>> neighbors);
uint16_t CalculateChecksum (const uint8_t *data, uint32_t length);

void writeBigEndian (uint8_t *payload, uint32_t offset, uint32_t value);
uint32_t readBigEndian (const uint8_t *payload, uint32_t offset);

Ptr<Packet> CopyAndDecrementTtl (Ptr<Packet> lsuPayload);
Ptr<Packet> CopyAndIncrementSeqNumber (Ptr<Packet> lsuPayload);

Ptr<RouterLsa> ConstructRouterLsa (std::vector<RouterLink> neighborLinks);
Ptr<AreaLsa> ConstructAreaLsa (std::vector<AreaLink> areaLinks);

Ptr<Packet> ConstructLSUPacket (OspfHeader ospfHeader, LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa);
Ptr<Packet> ConstructLSUPacket (Ipv4Address routerId, uint32_t areaId, uint16_t seqNum,
                                Ptr<RouterLsa> routerLsa);
Ptr<Packet> ConstructLSUPacket (Ipv4Address routerId, uint32_t areaId, uint16_t seqNum, uint16_t ttl,
                                std::vector<Ptr<RouterLsa>> routerLsas);

Ptr<Packet> ConstructLSAckPacket (Ipv4Address routerId, uint32_t areaId, std::vector<LsaHeader> lsaHeaders);
Ptr<Packet> ConstructLSAckPacket (Ipv4Address routerId, uint32_t areaId, LsaHeader lsaHeader);

void EncapsulateOspfPacket (Ptr<Packet> packet, Ipv4Address routerId, uint32_t areaId,
                            OspfHeader::OspfType type);

std::tuple<Ipv4Address, Ipv4Mask, Ipv4Address> GetAdvertisement (uint8_t *buffer);
} // namespace ns3

#endif /* OSPF_PACKET_HELPER_H */