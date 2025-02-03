/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
MIT License

Copyright (c) 2025 Sirapop Theeranantachai

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
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
#include "lsa.h"
#include "lsa-header.h"
#include "ospf-dbd.h"
#include "ls-request.h"
#include "queue"
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

  std::string GetNeighborString();

  // Database Descriptions
  uint32_t GetDDSeqNum();
  void SetDDSeqNum(uint32_t ddSeqNum);

  Ptr<OspfDbd> GetLastDbdSent();
  void SetLastDbdSent(Ptr<OspfDbd> dbd);

  // DB Description queue
  void IncrementDDSeqNum();
  void ClearDbdQueue();
  void AddDbdQueue(LsaHeader lsaHeader);
  LsaHeader PopDbdQueue();
  std::vector<LsaHeader> PopMaxMtuFromDbdQueue(uint32_t mtu);
  bool IsDbdQueueEmpty();

  // LS Request Queue
  Ptr<LsRequest> GetLastLsrSent();
  void SetLastLsrSent(Ptr<LsRequest> lsr);

  void InsertLsaKey(LsaHeader lsaHeader);
  void InsertLsaKey(LsaHeader::LsaKey lsaKey, uint32_t seqNum);
  uint32_t GetLsaKeySeqNum(LsaHeader::LsaKey lsaKey);
  void ClearLsaKey();
  bool IsLsaKeyOutdated(LsaHeader lsaHeader);
  bool IsLsaKeyOutdated(LsaHeader::LsaKey lsaKey, uint32_t seqNum);
  std::vector<LsaHeader::LsaKey> GetOutdatedLsaKeys(std::vector<LsaHeader> localLsaHeaders);
  void AddOutdatedLsaKeysToQueue(std::vector<LsaHeader> localLsaHeaders);
  uint32_t GetLsrQueueSize();
  bool IsLsrQueueEmpty();
  std::vector<LsaHeader::LsaKey> PopMaxMtuFromLsrQueue(uint32_t mtu);

  // LS Update Acks
  // Lsa-key specific timeout
  void BindLsuTimeout(LsaHeader::LsaKey lsaKey, EventId event);
  EventId GetLsuTimeout(LsaHeader::LsaKey lsaKey);
  bool RemoveLsuTimeout(LsaHeader::LsaKey lsaKey);
  void ClearLsuTimeouts();

  // Neighbor-specific timeout
  void RemoveTimeout();
  void BindTimeout(EventId event);



  Ipv4Address m_routerId;
  Ipv4Address m_ipAddress;
  uint32_t m_area;
  NeighborState m_state;

  // Database Descriptions
  uint32_t m_ddSeqNum;
  std::queue<LsaHeader> m_dbdQueue;
  std::queue<LsaHeader::LsaKey> m_lsrQueue;
  Ptr<OspfDbd> m_lastDbdSent;
  std::map<LsaHeader::LsaKey, uint32_t> m_lsaSeqNums; // Neighbor's headers
  EventId m_event;
  Time m_lastHelloReceived;

  // LS Request
  Ptr<LsRequest> m_lastLsrSent;

  // LS Update
  // Pending ack, value is <LsaHeader, LSA>
  std::map<LsaHeader::LsaKey, EventId> m_lsuTimeouts; // timeout events for LS Update
};

} // namespace ns3

#endif /* OSPF_NEIGHBOR_H */
