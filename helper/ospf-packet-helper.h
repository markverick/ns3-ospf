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
#include "ns3/ospf-hello.h"
#include "ns3/ls-ack.h"

namespace ns3
{


Ptr<Packet> ConstructHelloPacket(Ipv4Address routerId, uint32_t areaId, Ipv4Mask mask, uint16_t m_helloInterval,
                                uint32_t routerDeadInterval, std::vector<Ptr<OspfNeighbor> > neighbors);
uint16_t CalculateChecksum(const uint8_t* data, uint32_t length);

void writeBigEndian(uint8_t* payload, uint32_t offset, uint32_t value) {
    payload[offset]     = (value >> 24) & 0xFF; // Most significant byte
    payload[offset + 1] = (value >> 16) & 0xFF;
    payload[offset + 2] = (value >> 8) & 0xFF;
    payload[offset + 3] = value & 0xFF;         // Least significant byte
}

uint32_t readBigEndian(const uint8_t* payload, uint32_t offset) {
    return (static_cast<uint32_t>(payload[offset]) << 24) |
           (static_cast<uint32_t>(payload[offset + 1]) << 16) |
           (static_cast<uint32_t>(payload[offset + 2]) << 8) |
           (static_cast<uint32_t>(payload[offset + 3]));
}

Ptr<Packet>
ConstructHelloPacket(Ipv4Address routerId, uint32_t areaId, Ipv4Mask mask, uint16_t helloInterval,
                    uint32_t routerDeadInterval, std::vector<Ptr<OspfNeighbor> > neighbors) {
    // Create a hello payload
    Ptr<OspfHello> helloPayload = Create<OspfHello>(mask.Get(), helloInterval, routerDeadInterval);
    for (auto neighbor : neighbors) {
        helloPayload->AddNeighbor(neighbor->GetRouterId().Get());
    }

    // Create the OSPF header
    OspfHeader header;
    header.SetType(OspfHeader::OspfType::OspfHello);
    header.SetPayloadSize(helloPayload->GetSerializedSize());
    header.SetRouterId(routerId.Get());
    header.SetArea(areaId);

    // Add header to payload
    Ptr<Packet> packet = helloPayload->ConstructPacket();
    packet->AddHeader(header);
    return packet;
}


// Return null if ttl becomes zero
Ptr<Packet>
CopyAndDecrementTtl(Ptr<Packet> lsuPayload) {
    uint32_t payloadSize = lsuPayload->GetSize();

    uint8_t *buffer = new uint8_t[payloadSize];
    lsuPayload->CopyData(buffer, payloadSize);
    uint16_t ttl = static_cast<int>(buffer[2] << 8) + static_cast<int>(buffer[3]) - 1;
    // std::cout << "    Decrement TTL to " << ttl << std::endl;
    if (ttl <= 0) return nullptr;
    buffer[2] = (ttl >> 8 ) & 0xFF;
    buffer[3] = ttl & 0xFF;
    return Create<Packet>(buffer, payloadSize);
}

// Return null if ttl becomes zero
Ptr<Packet>
CopyAndIncrementSeqNumber(Ptr<Packet> lsuPayload) {
    uint32_t payloadSize = lsuPayload->GetSize();

    uint8_t *buffer = new uint8_t[payloadSize];
    lsuPayload->CopyData(buffer, payloadSize);
    uint16_t seqNum = static_cast<int>(buffer[0] << 8) + static_cast<int>(buffer[1]) + 1;
    // std::cout << "    Increment seqNum to " << seqNum << std::endl;
    if (seqNum <= 0) return nullptr;
    buffer[0] = (seqNum >> 8 ) & 0xFF;
    buffer[1] = seqNum & 0xFF;
    return Create<Packet>(buffer, payloadSize);
}

Ptr<RouterLsa>
ConstructRouterLsa(std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > neighborLinks) {
    // Create a Router-LSA
    Ptr<RouterLsa> routerLsa = Create<RouterLsa>(0, 0, 0);
    for (uint32_t j = 0; j < neighborLinks.size(); j++) {
        auto link = neighborLinks[j];
        routerLsa->AddLink(RouterLink(std::get<0>(link), std::get<1>(link), 1, std::get<2>(link)));
    }
    return routerLsa;
}

Ptr<Packet>
ConstructLSUPacket(OspfHeader ospfHeader, LsaHeader lsaHeader,
                    Ptr<RouterLsa> routerLsa) {
    // Read router LSAs and put it in a buffer
    // Create a packet with the payload and the OSPF header
    Ptr<Packet> packet = routerLsa->ConstructPacket();
    packet->AddHeader(lsaHeader);
    packet->AddHeader(ospfHeader);
    return packet;
}

Ptr<Packet>
ConstructLSUPacket(Ipv4Address routerId, uint32_t areaId, uint16_t seqNum,
                    Ptr<RouterLsa> routerLsa) {
    // Read router LSAs and put it in a buffer
    // Create an LSA payload
    Ptr<Packet> packet = routerLsa->ConstructPacket();

    // Add LSA header
    LsaHeader lsaHeader;
    lsaHeader.SetType(LsaHeader::LsType::RouterLSAs);
    lsaHeader.SetLength(20 + packet->GetSize());
    lsaHeader.SetSeqNum(seqNum);
    lsaHeader.SetLsId(routerId.Get());
    lsaHeader.SetAdvertisingRouter(routerId.Get());
    packet->AddHeader(lsaHeader);

    // Add OSPF header
    OspfHeader ospfHeader;
    ospfHeader.SetType(OspfHeader::OspfType::OspfLSUpdate);
    ospfHeader.SetPayloadSize(packet->GetSize() + lsaHeader.GetSerializedSize());
    ospfHeader.SetRouterId(routerId.Get());
    ospfHeader.SetArea(areaId);
    packet->AddHeader(ospfHeader);

    return packet;
}

Ptr<Packet>
ConstructLSUPacket(Ipv4Address routerId, uint32_t areaId, uint16_t seqNum, uint16_t ttl,
                    std::vector<Ptr<RouterLsa> > routerLsas) {
    // Read router LSAs and put it in a buffer
    Buffer buffer;
    uint32_t payloadSize = 12 * routerLsas.size();
    buffer.AddAtStart(payloadSize);
    auto i = buffer.Begin();
    for (Ptr<RouterLsa> routerLsa : routerLsas) {
        i.Next(routerLsa->Serialize(i));
    }

    // Create LSAs payload
    Ptr<Packet> packet = Create<Packet>(buffer.PeekData(), payloadSize);

    // Add LSA header
    LsaHeader lsaHeader;
    lsaHeader.SetType(LsaHeader::LsType::RouterLSAs);
    lsaHeader.SetLength(20 + packet->GetSize());
    lsaHeader.SetLsId(routerId.Get());
    lsaHeader.SetAdvertisingRouter(routerId.Get());
    packet->AddHeader(lsaHeader);

    // Add OSPF header
    OspfHeader ospfHeader;
    ospfHeader.SetType(OspfHeader::OspfType::OspfLSUpdate);
    ospfHeader.SetPayloadSize(payloadSize + lsaHeader.GetSerializedSize());
    ospfHeader.SetRouterId(routerId.Get());
    ospfHeader.SetArea(areaId);
    packet->AddHeader(ospfHeader);

    return packet;
}

Ptr<Packet>
ConstructLSAckPacket(Ipv4Address routerId, uint32_t areaId, std::vector<LsaHeader> lsaHeaders) {
    // Create a packet with the payload and the OSPF header
    Ptr<LsAck> lsAck = Create<LsAck>(lsaHeaders);
    Ptr<Packet> lsAckPayload = lsAck->ConstructPacket();

    // Add OSPF header
    OspfHeader ospfHeader;
    ospfHeader.SetType(OspfHeader::OspfType::OspfLSAck);
    ospfHeader.SetPayloadSize(lsAckPayload->GetSize());
    ospfHeader.SetRouterId(routerId.Get());
    ospfHeader.SetArea(areaId);
    lsAckPayload->AddHeader(ospfHeader);
    return lsAckPayload;
}

Ptr<Packet>
ConstructLSAckPacket(Ipv4Address routerId, uint32_t areaId, LsaHeader lsaHeader) {
    std::vector<LsaHeader> lsaHeaders;
    lsaHeaders.emplace_back(lsaHeader);
    return ConstructLSAckPacket(routerId, areaId, lsaHeaders);
}

//  Return a tuple of <subnet, mask, neighbor's router ID>
std::tuple<Ipv4Address, Ipv4Mask, Ipv4Address> GetAdvertisement(uint8_t* buffer) {
    Ipv4Address subnet = Ipv4Address::Deserialize(buffer);
    Ipv4Mask mask = Ipv4Mask(readBigEndian(buffer, 4));
    Ipv4Address remoteRouterId = Ipv4Address::Deserialize(buffer + 8);
    return std::make_tuple(subnet, mask, remoteRouterId);
}


uint16_t CalculateChecksum(const uint8_t* data, uint32_t length) {
    uint32_t sum = 0;

    // Sum each 16-bit word
    for (uint32_t i = 0; i < length - 1; i += 2) {
        uint16_t word = (data[i] << 8) + data[i + 1];
        sum += word;

        // Add carry if present
        if (sum & 0x10000) {
            sum = (sum & 0xFFFF) + 1;
        }
    }

    // Handle any remaining byte
    if (length % 2 != 0) {
        uint16_t lastByte = data[length - 1] << 8;
        sum += lastByte;

        if (sum & 0x10000) {
            sum = (sum & 0xFFFF) + 1;
        }
    }

    // Final one's complement
    return ~sum & 0xFFFF;
}

}

#endif /* OSPF_PACKET_HELPER_H */