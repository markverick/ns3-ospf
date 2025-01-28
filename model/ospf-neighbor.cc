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

#include "lsa-header.h"
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

uint32_t
OspfNeighbor::GetDDSeqNum() {
  return m_ddSeqNum;
}

void
OspfNeighbor::SetDDSeqNum(uint32_t ddSeqNum) {
  m_ddSeqNum = ddSeqNum;
}

void
OspfNeighbor::IncrementDDSeqNum() {
  m_ddSeqNum++;
}

void
OspfNeighbor::ClearDbdQueue() {
  while (m_dbdQueue.empty()) {
    m_dbdQueue.pop();
  }
}

void
OspfNeighbor::AddDbdQueue(LsaHeader routerLsa) {
  m_dbdQueue.emplace(routerLsa);
}

LsaHeader
OspfNeighbor::PopDbdQueue() {
  LsaHeader lsa = m_dbdQueue.front();
  m_dbdQueue.pop();
  return lsa;
}

bool
OspfNeighbor::IsDbdQueueEmpty() {
  return m_dbdQueue.empty();
}

std::vector<LsaHeader>
OspfNeighbor::PopMaxMtuDbdQueue(uint32_t mtu) {
  mtu =  mtu - 100; // Just in case of encapsulations
  std::vector<LsaHeader> lsaHeaderList;
  uint32_t currentBytes = 0;
  LsaHeader header = m_dbdQueue.front();
  // Keep popping until the queue runs out or reaching the mtu
  while (!m_dbdQueue.empty() && currentBytes < mtu) {
    uint32_t lsaSize = header.GetSerializedSize();
    if (currentBytes + lsaSize <= mtu) {
      lsaHeaderList.emplace_back(header);
      m_dbdQueue.pop();
      currentBytes += lsaSize;
    }
  }
  return lsaHeaderList;
}

Ptr<OspfDbd>
OspfNeighbor::GetLastDbdSent() {
  return m_lastDbdSent;
}

void
OspfNeighbor::SetLastDbdSent(Ptr<OspfDbd> dbd) {
  m_lastDbdSent = dbd;
}

void
OspfNeighbor::InsertLsaKey(LsaHeader lsaHeader) {
  InsertLsaKey(lsaHeader.GetKey(), lsaHeader.GetSeqNum());
}

void
OspfNeighbor::InsertLsaKey(LsaHeader::LsaKey lsaKey, uint32_t seqNum) {
  m_lsaSeqNums[lsaKey] = seqNum;
}
uint32_t
OspfNeighbor::GetLsaKeySeqNum(LsaHeader::LsaKey lsaKey) {
  if (m_lsaSeqNums.find(lsaKey) == m_lsaSeqNums.end()) {
    return 0;
  }
  return m_lsaSeqNums[lsaKey];
}
void
OspfNeighbor::ClearLsaKey() {
  m_lsaSeqNums.clear();
}
bool
OspfNeighbor::IsLsaKeyOutdated(LsaHeader lsaHeader) {
  return IsLsaKeyOutdated(lsaHeader.GetKey(), lsaHeader.GetSeqNum());
}

bool
OspfNeighbor::IsLsaKeyOutdated(LsaHeader::LsaKey lsaKey, uint32_t ddSeqNum) {
  if (m_lsaSeqNums.find(lsaKey) == m_lsaSeqNums.end()) {
    return true;
  }
  return ddSeqNum < m_lsaSeqNums[lsaKey];
}

void
OspfNeighbor::RemoveEvent() {
  if (m_event.IsRunning()) {
    m_event.Remove();
  }
}

void
OspfNeighbor::BindEvent(EventId event) {
  if (m_event.IsRunning()) {
    m_event.Remove();
  }
  m_event = event;
}

void
OspfNeighbor::RefreshLastHelloReceived () {
  m_lastHelloReceived = Simulator::Now();
}

std::string
OspfNeighbor::GetNeighborString() {
  std::stringstream ss;
  ss << "(" << m_routerId << "," << m_ipAddress << ")";
  return ss.str();
}
}