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

#include "ospf-interface.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfInterface");

OspfInterface::OspfInterface ()
{
  m_ipAddress = Ipv4Address::GetAny ();
  m_ipMask = Ipv4Mask (0xffffffff); // default to /32
  m_helloInterval = 0;
  m_area = 0;
  m_metric = 0;
}
OspfInterface::~OspfInterface ()
{
}

OspfInterface::OspfInterface (Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval,
                              uint32_t routerDeadInterval, uint32_t area, uint32_t metric,
                              uint32_t mtu)
    : m_ipAddress (ipAddress),
      m_ipMask (ipMask),
      m_helloInterval (helloInterval),
      m_routerDeadInterval (routerDeadInterval),
      m_area (area),
      m_metric (metric),
      m_mtu (mtu)
{
}

Ipv4Address
OspfInterface::GetAddress ()
{
  return m_ipAddress;
}

void
OspfInterface::SetAddress (Ipv4Address ipAddress)
{
  m_ipAddress = ipAddress;
}

Ipv4Address
OspfInterface::GetGateway ()
{
  return m_gateway;
}

void
OspfInterface::SetGateway (Ipv4Address gateway)
{
  m_gateway = gateway;
}

Ipv4Mask
OspfInterface::GetMask ()
{
  return m_ipMask;
}

void
OspfInterface::SetMask (Ipv4Mask ipMask)
{
  m_ipMask = ipMask;
}

uint32_t
OspfInterface::GetMetric ()
{
  return m_metric;
}

void
OspfInterface::SetMetric (uint32_t metric)
{
  m_metric = metric;
}

uint32_t
OspfInterface::GetArea ()
{
  return m_area;
}

void
OspfInterface::SetArea (uint32_t area)
{
  m_area = area;
}

uint32_t
OspfInterface::GetMtu ()
{
  return m_mtu;
}

void
OspfInterface::SetMtu (uint32_t mtu)
{
  m_mtu = mtu;
}

uint16_t
OspfInterface::GetHelloInterval ()
{
  return m_helloInterval;
}

void
OspfInterface::SetHelloInterval (uint16_t helloInterval)
{
  m_helloInterval = helloInterval;
}

uint32_t
OspfInterface::GetRouterDeadInterval ()
{
  return m_routerDeadInterval;
}

void
OspfInterface::SetRouterDeadInterval (uint32_t routerDeadInterval)
{
  m_routerDeadInterval = routerDeadInterval;
}

Ptr<OspfNeighbor>
OspfInterface::GetNeighbor (Ipv4Address routerId, Ipv4Address remoteIp)
{
  for (auto n : m_neighbors)
    {
      if (n->GetRouterId () == routerId && n->GetIpAddress () == remoteIp)
        {
          return n;
        }
    }
  return nullptr;
}
std::vector<Ptr<OspfNeighbor>>
OspfInterface::GetNeighbors ()
{
  return m_neighbors;
}

bool
OspfInterface::IsUp () const
{
  return m_isUp;
}

void
OspfInterface::SetUp (bool isUp)
{
  m_isUp = isUp;
}

void
OspfInterface::AddNeighbor (Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_FUNCTION (this);
  m_neighbors.emplace_back (neighbor);
  return;
}

Ptr<OspfNeighbor>
OspfInterface::AddNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp, uint32_t remoteAreaId,
                            OspfNeighbor::NeighborState state)
{
  NS_LOG_FUNCTION (this << remoteRouterId << remoteIp << remoteAreaId << state);
  Ptr<OspfNeighbor> neighbor = Create<OspfNeighbor> (remoteRouterId, remoteIp, remoteAreaId, state);
  m_neighbors.emplace_back (neighbor);
  return neighbor;
}

bool
OspfInterface::RemoveNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp)
{
  for (auto it = m_neighbors.begin (); it != m_neighbors.end (); it++)
    {
      auto n = *it;
      if (n->GetRouterId () == remoteRouterId && n->GetIpAddress () == remoteIp)
        {
          m_neighbors.erase (it);
          return true;
        }
    }
  return false;
}

bool
OspfInterface::IsNeighbor (Ipv4Address remoteRouterId, Ipv4Address remoteIp)
{
  for (auto n : m_neighbors)
    {
      NS_LOG_FUNCTION (this << n->GetRouterId () << remoteRouterId << n->GetIpAddress ()
                            << remoteIp);
      if (n->GetRouterId () == remoteRouterId && n->GetIpAddress () == remoteIp)
        {
          return true;
        }
    }
  return false;
}

void
OspfInterface::ClearNeighbors ()
{
  m_neighbors.clear ();
}

// Get a list of <neighbor's router ID, router's IP address, neighbor's areaId>
std::vector<RouterLink>
OspfInterface::GetActiveRouterLinks ()
{
  std::vector<RouterLink> links;
  auto neighbors = GetNeighbors ();
  // NS_LOG_INFO("# neighbors: " << neighbors.size());
  for (auto n : neighbors)
    {
      // Only aggregate neighbors that is at least in ExStart
      // NS_LOG_INFO("  (" << n->GetRouterId().Get() << ", " << m_ipAddress.Get() << ")");
      if (n->GetState () == OspfNeighbor::Full)
        {
          if (n->GetArea () == m_area)
            {
              // Type 1 link
              links.emplace_back (
                  RouterLink (n->GetRouterId ().Get (), m_ipAddress.Get (), 1, m_metric));
            }
          else
            {
              // Type 5 link (Inter-Area)
              links.emplace_back (RouterLink (n->GetArea (), m_ipAddress.Get (), 5, m_metric));
            }
        }
    }
  return links;
}

} // namespace ns3