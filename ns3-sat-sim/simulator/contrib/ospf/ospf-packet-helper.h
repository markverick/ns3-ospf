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

namespace ns3
{


Ptr<Packet> ConstructHelloPayload(Ipv4Address routerId, uint32_t areaId, Ipv4Mask mask, uint16_t m_helloInterval, uint32_t neighborTimeout);
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
ConstructHelloPayload(Ipv4Address routerId, uint32_t areaId, Ipv4Mask mask, uint16_t helloInterval, uint32_t neighborTimeout) {
    // Create a 40-byte payload
    // Actual OSPF will contain neighbors, so the size will be variable
    uint8_t payload[40] = {0};

    // Fill in the fields based on the given format

    // Version # (8 bits) and Type (8 bits)
    payload[0] = 0x02;
    payload[1] = 0x01;

    // Packet length (16 bits)
    payload[2] = 0x00; // Higher 8 bits
    payload[3] = 32;   // Lower 8 bits (packet length in bytes)

    // Router ID (32 bits)
    routerId.Serialize(payload + 4);

    // Area ID (32 bits)
    writeBigEndian(payload, 8, areaId);

    // Checksum (16 bits)
    payload[12] = 0x00; // Example value
    payload[13] = 0x00;

    // Autype (16 bits)
    payload[14] = 0x00;
    payload[15] = 0x01;

    // Authentication (64 bits)
    for (int i = 16; i < 24; ++i) {
        payload[i] = 0x00; // Zero-filled for simplicity
    }

    // Network Mask (32 bits)
    writeBigEndian(payload, 24, mask.Get());

    // HelloInt (16 bits) and padding (16 bits)
    payload[28] = (helloInterval >> 8) & 0xFF ;
    payload[29] = helloInterval & 0xFF;
    for (int i = 30; i < 32; ++i) {
        payload[i] = 0x00; // Padding with zeros
    }

    // Router Dead Interval (32 bits)
    writeBigEndian(payload, 32, neighborTimeout);

    // Padding for future DR and BDR
    for (int i = 32; i < 40; ++i) {
        payload[i] = 0x00; // Padding with zeros
    }

    // Calculate checksum
    uint16_t checksum = CalculateChecksum(payload, 40);
    payload[12] = (checksum >> 8) & 0xFF; // Higher 8 bits
    payload[13] = checksum & 0xFF;        // Lower 8 bits

    // Create a packet with the payload
    Ptr<Packet> packet = Create<Packet>(payload, 40);
    return packet;
}

// Return null if ttl becomes zero
Ptr<Packet>
CopyAndDecrementTtl(Ptr<Packet> lsuPayload) {
    uint32_t payloadSize = lsuPayload->GetSize();

    uint8_t *buffer = new uint8_t[payloadSize];
    lsuPayload->CopyData(buffer, payloadSize);
    uint16_t ttl = static_cast<int>(buffer[26] << 8) + static_cast<int>(buffer[27]) - 1;
    // std::cout << "    Decrement TTL to " << ttl << std::endl;
    if (ttl <= 0) return nullptr;
    buffer[26] = (ttl >> 8 ) & 0xFF;
    buffer[27] = ttl & 0xFF;
    return Create<Packet>(buffer, payloadSize);
}

// Return null if ttl becomes zero
Ptr<Packet>
CopyAndIncrementSeqNumber(Ptr<Packet> lsuPayload) {
    uint32_t payloadSize = lsuPayload->GetSize();

    uint8_t *buffer = new uint8_t[payloadSize];
    lsuPayload->CopyData(buffer, payloadSize);
    uint16_t seqNum = static_cast<int>(buffer[24] << 8) + static_cast<int>(buffer[25]) + 1;
    // std::cout << "    Increment seqNum to " << seqNum << std::endl;
    if (seqNum <= 0) return nullptr;
    buffer[24] = (seqNum >> 8 ) & 0xFF;
    buffer[25] = seqNum & 0xFF;
    return Create<Packet>(buffer, payloadSize);
}

Ptr<Packet>
ConstructLSUPayload(Ipv4Address routerId, uint32_t areaId, uint16_t seqNum, uint16_t ttl,
                    std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > lsAdvertisements) {
    // Create a payload of 32 + 12 x numAds bytes
    // Can store 8 LSAs (12 bytes each) in total (for now)
    uint8_t payload[128] = {0};

    // Fill in the fields based on the given format

    // Version # (8 bits) and Type (8 bits)
    payload[0] = 0x02;
    payload[1] = 0x04;

    // Packet length (16 bits)
    payload[2] = 0x00; // Higher 8 bits
    payload[3] = 32;   // Lower 8 bits (packet length in bytes)

    // Router ID (32 bits)
    routerId.Serialize(payload + 4);

    // Area ID (32 bits)
    writeBigEndian(payload, 8, areaId);

    // Checksum (16 bits)
    payload[12] = 0x00; // Example value
    payload[13] = 0x00;

    // Autype (16 bits)
    payload[14] = 0x00;
    payload[15] = 0x01;

    // Authentication (64 bits)
    for (int i = 16; i < 24; ++i) {
        payload[i] = 0x00; // Zero-filled for simplicity
    }

    // Sequence Number (16 bits)
    payload[24] = (seqNum >> 8 ) & 0xFF;
    payload[25] = seqNum & 0xFF;

    // TTL (16 bits)
    payload[26] = (ttl >> 8 ) & 0xFF;
    payload[27] = ttl & 0xFF;

    // Number of Advertisement (32 bits)
    uint32_t numAds = lsAdvertisements.size();
    writeBigEndian(payload, 28, numAds);

    // Link State Advertisement (32 * numAds bits)
    // std::cout << "Write advertisements: " << std::endl;
    for (uint32_t i = 0; i < numAds; i++) {
        const auto& [subnet, mask, remoteRouterId] = lsAdvertisements[i];
        // std::cout << "  [" << (i) << "](" << subnet << ", " << mask << ", " << remoteRouterId << ")" << std::endl;
        writeBigEndian(payload, 32 + 12 * i, subnet);
        writeBigEndian(payload, 36 + 12 * i, mask);
        writeBigEndian(payload, 40 + 12 * i, remoteRouterId);
    }

    // Create a packet with the payload
    Ptr<Packet> packet = Create<Packet>(payload, 32 + 12 * numAds);
    return packet;
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