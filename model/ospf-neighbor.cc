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
    : m_routerId (remoteRouterId), m_ipAddress (ipAddress), m_area (area)
{
}

OspfNeighbor::OspfNeighbor (Ipv4Address remoteRouterId, Ipv4Address ipAddress, uint32_t area,
                            OspfNeighbor::NeighborState state)
    : m_routerId (remoteRouterId), m_ipAddress (ipAddress), m_area (area), m_state (state)
{
}

Ipv4Address
OspfNeighbor::GetRouterId ()
{
  return m_routerId;
}

void
OspfNeighbor::SetRouterId (Ipv4Address routerId)
{
  m_routerId = routerId;
}

void
OspfNeighbor::SetRouterId (uint32_t routerId)
{
  m_routerId = Ipv4Address (routerId);
}

Ipv4Address
OspfNeighbor::GetIpAddress ()
{
  return m_ipAddress;
}

void
OspfNeighbor::SetIpAddress (Ipv4Address ipAddress)
{
  m_ipAddress = ipAddress;
}

void
OspfNeighbor::SetIpAddress (uint32_t ipAddress)
{
  m_ipAddress = Ipv4Address (ipAddress);
}

uint32_t
OspfNeighbor::GetArea ()
{
  return m_area;
}

void
OspfNeighbor::SetArea (uint32_t area)
{
  m_area = area;
}

OspfNeighbor::NeighborState
OspfNeighbor::GetState ()
{
  return m_state;
}

void
OspfNeighbor::SetState (OspfNeighbor::NeighborState state)
{
  m_state = state;
}

uint32_t
OspfNeighbor::GetDDSeqNum ()
{
  return m_ddSeqNum;
}

void
OspfNeighbor::SetDDSeqNum (uint32_t ddSeqNum)
{
  m_ddSeqNum = ddSeqNum;
}

void
OspfNeighbor::IncrementDDSeqNum ()
{
  m_ddSeqNum++;
}

void
OspfNeighbor::ClearDbdQueue ()
{
  while (m_dbdQueue.empty ())
    {
      m_dbdQueue.pop ();
    }
}

void
OspfNeighbor::AddDbdQueue (LsaHeader routerLsa)
{
  m_dbdQueue.emplace (routerLsa);
}

LsaHeader
OspfNeighbor::PopDbdQueue ()
{
  LsaHeader lsa = m_dbdQueue.front ();
  m_dbdQueue.pop ();
  return lsa;
}

bool
OspfNeighbor::IsDbdQueueEmpty ()
{
  return m_dbdQueue.empty ();
}

std::vector<LsaHeader>
OspfNeighbor::PopMaxMtuFromDbdQueue (uint32_t mtu)
{
  mtu = mtu - 100; // Just in case of encapsulations
  std::vector<LsaHeader> lsaHeaderList;
  uint32_t currentBytes = 0;
  LsaHeader header;
  // Keep popping until the queue runs out or reaching the mtu
  while (!m_dbdQueue.empty ())
    {
      header = m_dbdQueue.front ();
      uint32_t lsaSize = header.GetSerializedSize ();
      if (currentBytes + lsaSize > mtu)
        {
          break;
        }
      lsaHeaderList.emplace_back (header);
      m_dbdQueue.pop ();
      currentBytes += lsaSize;
    }
  return lsaHeaderList;
}

Ptr<OspfDbd>
OspfNeighbor::GetLastDbdSent ()
{
  return m_lastDbdSent;
}

void
OspfNeighbor::SetLastDbdSent (Ptr<OspfDbd> dbd)
{
  m_lastDbdSent = dbd;
}

Ptr<LsRequest>
OspfNeighbor::GetLastLsrSent ()
{
  return m_lastLsrSent;
}

void
OspfNeighbor::SetLastLsrSent (Ptr<LsRequest> lsr)
{
  m_lastLsrSent = lsr;
}

void
OspfNeighbor::InsertLsaKey (LsaHeader lsaHeader)
{
  InsertLsaKey (lsaHeader.GetKey (), lsaHeader.GetSeqNum ());
}

void
OspfNeighbor::InsertLsaKey (LsaHeader::LsaKey lsaKey, uint32_t seqNum)
{
  m_lsaSeqNums[lsaKey] = seqNum;
}
uint32_t
OspfNeighbor::GetLsaKeySeqNum (LsaHeader::LsaKey lsaKey)
{
  if (m_lsaSeqNums.find (lsaKey) == m_lsaSeqNums.end ())
    {
      return 0;
    }
  return m_lsaSeqNums[lsaKey];
}
void
OspfNeighbor::ClearLsaKey ()
{
  m_lsaSeqNums.clear ();
}
bool
OspfNeighbor::IsLsaKeyOutdated (LsaHeader lsaHeader)
{
  return IsLsaKeyOutdated (lsaHeader.GetKey (), lsaHeader.GetSeqNum ());
}

// Check if a single LsaKey is outdated
bool
OspfNeighbor::IsLsaKeyOutdated (LsaHeader::LsaKey lsaKey, uint32_t ddSeqNum)
{
  if (m_lsaSeqNums.find (lsaKey) == m_lsaSeqNums.end ())
    {
      return false;
    }
  return ddSeqNum < m_lsaSeqNums[lsaKey];
}

// Not used for now.
std::vector<LsaHeader::LsaKey>
OspfNeighbor::GetOutdatedLsaKeys (std::vector<LsaHeader> localLsaHeaders)
{
  std::vector<LsaHeader::LsaKey> lsaKeys;
  std::map<LsaHeader::LsaKey, uint32_t> localLsaSeqNum;
  // Map the given keys
  for (auto lsaHeader : localLsaHeaders)
    {
      localLsaSeqNum[lsaHeader.GetKey ()] = lsaHeader.GetSeqNum ();
    }
  for (auto &[lsaKey, seqNum] : m_lsaSeqNums)
    {
      if (localLsaSeqNum.find (lsaKey) == localLsaSeqNum.end ())
        {
          // Get missing keys
          lsaKeys.emplace_back (lsaKey);
        }
      else
        {
          // Get outdated keys
          if (localLsaSeqNum[lsaKey] < seqNum)
            {
              lsaKeys.emplace_back (lsaKey);
            }
        }
    }
  NS_LOG_INFO ("Number of outdated keys: " << lsaKeys.size ());
  return lsaKeys;
}

// Check localLsaHeaders to see which one is outdated or missing
// Then add to LSR queue
void
OspfNeighbor::AddOutdatedLsaKeysToQueue (std::vector<LsaHeader> localLsaHeaders)
{
  // Reset the queue
  while (!m_lsrQueue.empty ())
    {
      m_lsrQueue.pop ();
    }
  std::map<LsaHeader::LsaKey, uint32_t> localLsaSeqNum;
  // Map the given keys
  for (auto lsaHeader : localLsaHeaders)
    {
      localLsaSeqNum[lsaHeader.GetKey ()] = lsaHeader.GetSeqNum ();
    }
  for (auto &[lsaKey, seqNum] : m_lsaSeqNums)
    {
      if (localLsaSeqNum.find (lsaKey) == localLsaSeqNum.end ())
        {
          // Get missing keys
          m_lsrQueue.emplace (lsaKey);
        }
      else
        {
          // Get outdated keys
          if (localLsaSeqNum[lsaKey] < seqNum)
            {
              m_lsrQueue.emplace (lsaKey);
            }
        }
    }
  NS_LOG_INFO ("Number of outdated keys: " << m_lsrQueue.size ());
}
uint32_t
OspfNeighbor::GetLsrQueueSize ()
{
  return m_lsrQueue.size ();
}
bool
OspfNeighbor::IsLsrQueueEmpty ()
{
  return m_lsrQueue.empty ();
}

std::vector<LsaHeader::LsaKey>
OspfNeighbor::PopMaxMtuFromLsrQueue (uint32_t mtu)
{
  mtu = mtu - 100; // Just in case of encapsulations
  std::vector<LsaHeader::LsaKey> lsaKeyList;
  uint32_t currentBytes = 0;
  LsaHeader::LsaKey key;
  // Keep popping until the queue runs out or reaching the mtu
  uint32_t lsaKeySize = 12;
  while (!m_lsrQueue.empty ())
    {
      if (currentBytes + lsaKeySize > mtu)
        {
          break;
        }
      key = m_lsrQueue.front ();
      lsaKeyList.emplace_back (key);
      m_lsrQueue.pop ();
      currentBytes += lsaKeySize;
    }
  return lsaKeyList;
}

// LS Update / Acknowledge
void
OspfNeighbor::BindKeyedTimeout (LsaHeader::LsaKey lsaKey, EventId event)
{
  if (m_keyedTimeouts[lsaKey].IsRunning ())
    {
      m_keyedTimeouts[lsaKey].Remove ();
    }
  m_keyedTimeouts[lsaKey] = event;
}
EventId
OspfNeighbor::GetKeyedTimeout (LsaHeader::LsaKey lsaKey)
{
  return m_keyedTimeouts[lsaKey];
}
bool
OspfNeighbor::RemoveKeyedTimeout (LsaHeader::LsaKey lsaKey)
{
  auto it = m_keyedTimeouts.find (lsaKey);
  if (it != m_keyedTimeouts.end ())
    {
      it->second.Remove ();
      m_keyedTimeouts.erase (it);
      return true;
    }
  return false;
}
void
OspfNeighbor::ClearKeyedTimeouts (void)
{
  for (auto pair : m_keyedTimeouts)
    {
      pair.second.Remove ();
    }
}

// Sequential Event
void
OspfNeighbor::RemoveTimeout ()
{
  if (m_event.IsRunning ())
    {
      m_event.Remove ();
    }
}

void
OspfNeighbor::BindTimeout (EventId event)
{
  if (m_event.IsRunning ())
    {
      m_event.Remove ();
    }
  m_event = event;
}

void
OspfNeighbor::RefreshLastHelloReceived ()
{
  m_lastHelloReceived = Simulator::Now ();
}

std::string
OspfNeighbor::GetNeighborString ()
{
  std::stringstream ss;
  ss << "(" << m_routerId << "," << m_ipAddress << ")";
  return ss.str ();
}
} // namespace ns3