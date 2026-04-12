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
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-app.h"
#include "ns3/ospf-routing.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/ospf-neighbor.h"
#include "ns3/lsa.h"
#include "ns3/area-lsa.h"
#include "ospf-app-helper.h"

#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace ns3 {

namespace {

using LsaRecord = std::pair<LsaHeader, Ptr<Lsa>>;
using AreaLsaSeedMap = std::map<uint32_t, std::vector<LsaRecord>>;

struct PreloadNodeContext
{
  Ptr<Node> node;
  Ptr<Ipv4> ipv4;
  Ptr<OspfApp> app;
};

struct PointToPointAdjacency
{
  uint32_t ifIndex = 0;
  Ipv4Address selfIp;
  Ipv4Address remoteRouterId;
  Ipv4Address remoteIp;
  uint32_t remoteAreaId = 0;
  uint16_t metric = 0;
};

Ptr<OspfApp>
FindOspfApp (const Ptr<Node> &node)
{
  for (uint32_t i = 0; i < node->GetNApplications (); ++i)
    {
      auto app = DynamicCast<OspfApp> (node->GetApplication (i));
      if (app != nullptr)
        {
          return app;
        }
    }
  return nullptr;
}

bool
SelectPrimaryInterfaceAddress (Ptr<Ipv4> ipv4, uint32_t ifIndex, Ipv4InterfaceAddress &out)
{
  if (ipv4 == nullptr || ifIndex >= ipv4->GetNInterfaces ())
    {
      return false;
    }

  const uint32_t nAddr = ipv4->GetNAddresses (ifIndex);
  for (uint32_t a = 0; a < nAddr; ++a)
    {
      const auto ifAddr = ipv4->GetAddress (ifIndex, a);
      const auto ip = ifAddr.GetAddress ();
      if (ip.IsLocalhost () || ip == Ipv4Address::GetAny ())
        {
          continue;
        }
      out = ifAddr;
      return true;
    }

  return false;
}

uint16_t
ToLinkMetric (uint32_t metric, uint32_t ifIndex)
{
  NS_ABORT_MSG_IF (metric > std::numeric_limits<uint16_t>::max (),
                   "OSPF preload metric " << metric << " on interface " << ifIndex
                                           << " exceeds RouterLink/AreaLink encoding width");
  return static_cast<uint16_t> (metric);
}

OspfApp::ReachableRouteList
CollectReachableRoutesFromIpv4 (const PreloadNodeContext &context)
{
  OspfApp::ReachableRouteList reachable;
  const uint32_t nIf = context.ipv4->GetNInterfaces ();
  for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
    {
      if (!context.app->GetInterfacePrefixRoutable (ifIndex))
        {
          continue;
        }

      Ipv4InterfaceAddress ifAddr;
      if (!SelectPrimaryInterfaceAddress (context.ipv4, ifIndex, ifAddr))
        {
          continue;
        }

      const auto addr = ifAddr.GetAddress ();
      const auto mask = ifAddr.GetMask ();
      reachable.emplace_back (ifIndex, addr.CombineMask (mask).Get (), mask.Get (), addr.Get (), 1);
    }

  return reachable;
}

std::vector<PreloadNodeContext>
CollectPreloadNodes (NodeContainer c)
{
  std::vector<PreloadNodeContext> nodes;
  nodes.reserve (c.GetN ());
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      if (ipv4 == nullptr)
        {
          continue;
        }

      auto app = FindOspfApp (node);
      if (app == nullptr)
        {
          continue;
        }

      nodes.push_back ({node, ipv4, app});
    }
  return nodes;
}

LsaRecord
MakeLsaRecord (LsaHeader::LsType type, uint32_t lsId, uint32_t advertisingRouter, Ptr<Lsa> lsa)
{
  LsaHeader header (std::make_tuple (type, lsId, advertisingRouter));
  header.SetLength (20 + lsa->GetSerializedSize ());
  header.SetSeqNum (1);
  return {header, lsa};
}

std::vector<PointToPointAdjacency>
CollectPointToPointAdjacencies (const PreloadNodeContext &context)
{
  std::vector<PointToPointAdjacency> adjacencies;
  const uint32_t nIf = context.ipv4->GetNInterfaces ();
  for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
    {
      Ptr<NetDevice> dev = context.node->GetDevice (ifIndex);
      if (dev == nullptr || !dev->IsPointToPoint ())
        {
          continue;
        }

      Ipv4InterfaceAddress selfIfAddr;
      if (!SelectPrimaryInterfaceAddress (context.ipv4, ifIndex, selfIfAddr))
        {
          continue;
        }

      auto channel = DynamicCast<Channel> (dev->GetChannel ());
      if (channel == nullptr)
        {
          continue;
        }

      for (uint32_t j = 0; j < channel->GetNDevices (); ++j)
        {
          Ptr<NetDevice> remoteDev = channel->GetDevice (j);
          if (remoteDev == nullptr || remoteDev == dev)
            {
              continue;
            }

          auto remoteIpv4 = remoteDev->GetNode ()->GetObject<Ipv4> ();
          auto remoteApp = FindOspfApp (remoteDev->GetNode ());
          if (remoteIpv4 == nullptr || remoteApp == nullptr)
            {
              continue;
            }

          Ipv4InterfaceAddress remoteIfAddr;
          if (!SelectPrimaryInterfaceAddress (remoteIpv4, remoteDev->GetIfIndex (), remoteIfAddr))
            {
              continue;
            }

          adjacencies.push_back ({ifIndex,
                                  selfIfAddr.GetAddress (),
                                  remoteApp->GetRouterId (),
                                  remoteIfAddr.GetAddress (),
                                  remoteApp->GetArea (),
                                  ToLinkMetric (context.app->GetMetric (ifIndex), ifIndex)});
          break;
        }
    }

  return adjacencies;
}

void
SeedRouterLsaAndNeighbors (const PreloadNodeContext &context,
                           const std::vector<PointToPointAdjacency> &adjacencies,
                           std::map<uint32_t, std::vector<AreaLink>> &areaAdj,
                           AreaLsaSeedMap &lsaSeeds)
{
  Ptr<RouterLsa> routerLsa = Create<RouterLsa> ();

  for (const auto &adjacency : adjacencies)
    {
      context.app->AddNeighbor (
          adjacency.ifIndex,
          Create<OspfNeighbor> (adjacency.remoteRouterId, adjacency.remoteIp, adjacency.remoteAreaId,
                    OspfNeighbor::NeighborState::Full));

      if (adjacency.remoteAreaId == context.app->GetArea ())
        {
          routerLsa->AddLink (RouterLink (adjacency.remoteRouterId.Get (),
                                          adjacency.selfIp.Get (),
                                          1,
                                          adjacency.metric));
        }
      else
        {
          routerLsa->AddLink (
              RouterLink (adjacency.remoteAreaId, adjacency.selfIp.Get (), 5, adjacency.metric));
          areaAdj[context.app->GetArea ()].emplace_back (
              AreaLink (adjacency.remoteAreaId, adjacency.selfIp.Get (), adjacency.metric));
        }
    }

  lsaSeeds[context.app->GetArea ()].emplace_back (
      MakeLsaRecord (LsaHeader::LsType::RouterLSAs,
                     context.app->GetRouterId ().Get (),
                     context.app->GetRouterId ().Get (),
                     routerLsa));
}

LsaRecord
BuildAreaLsaSeed (uint32_t areaId, uint32_t advertisingRouter, const std::vector<AreaLink> &links)
{
  Ptr<AreaLsa> areaLsa = Create<AreaLsa> ();
  for (const auto &areaLink : links)
    {
      areaLsa->AddLink (areaLink);
    }
  return MakeLsaRecord (LsaHeader::LsType::AreaLSAs, areaId, advertisingRouter, areaLsa);
}

LsaRecord
BuildL2SummaryLsaSeed (uint32_t areaId, uint32_t advertisingRouter,
                       const std::vector<LsaRecord> &areaSeedLsas)
{
  Ptr<L2SummaryLsa> l2Summary = Create<L2SummaryLsa> ();
  for (const auto &[header, lsa] : areaSeedLsas)
    {
      if (header.GetType () != LsaHeader::LsType::L1SummaryLSAs)
        {
          continue;
        }

      auto l1Summary = DynamicCast<L1SummaryLsa> (lsa);
      if (l1Summary == nullptr)
        {
          continue;
        }

      for (const auto &route : l1Summary->GetRoutes ())
        {
          l2Summary->AddRoute (route);
        }
    }

  return MakeLsaRecord (LsaHeader::LsType::L2SummaryLSAs, areaId, advertisingRouter, l2Summary);
}

std::vector<LsaRecord>
BuildProxiedLsas (const std::map<uint32_t, std::vector<AreaLink>> &areaAdj,
                  const std::map<uint32_t, std::set<uint32_t>> &areaMembers,
                  const AreaLsaSeedMap &lsaSeeds)
{
  std::vector<LsaRecord> proxiedLsas;
  for (const auto &[areaId, members] : areaMembers)
    {
      if (members.empty ())
        {
          continue;
        }

      const uint32_t leaderRouterId = *members.begin ();
      auto adjIt = areaAdj.find (areaId);
      const std::vector<AreaLink> emptyAdj;
      const auto &adj = adjIt != areaAdj.end () ? adjIt->second : emptyAdj;
      proxiedLsas.emplace_back (BuildAreaLsaSeed (areaId, leaderRouterId, adj));

      auto lsaIt = lsaSeeds.find (areaId);
      if (lsaIt != lsaSeeds.end ())
        {
          proxiedLsas.emplace_back (BuildL2SummaryLsaSeed (areaId, leaderRouterId, lsaIt->second));
        }
    }

  return proxiedLsas;
}

} // namespace

OspfAppHelper::OspfAppHelper ()
{
  m_factory.SetTypeId (OspfApp::GetTypeId ());
}

void
OspfAppHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
OspfAppHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (Install (*i));
    }
  return apps;
}

ApplicationContainer
OspfAppHelper::Install (Ptr<Node> n) const
{
  Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
  Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4->GetRoutingProtocol ();
  Ptr<OspfRouting> routing = Ipv4RoutingHelper::GetRouting<OspfRouting> (routingProtocol);
  if (routing == nullptr)
    {
      auto listRouting = DynamicCast<Ipv4ListRouting> (routingProtocol);
      NS_ABORT_MSG_IF (listRouting == nullptr,
                       "OspfAppHelper requires Ipv4ListRouting on the node IPv4 stack");
      routing = CreateObject<OspfRouting> ();
      listRouting->AddRoutingProtocol (routing, 10);
    }
  NetDeviceContainer devs;
  for (uint32_t j = 0; j < n->GetNDevices (); j++)
    {
      // Skip non localhost and non p2p
      if (j > 0 && !n->GetDevice (j)->IsPointToPoint ())
        continue;
      devs.Add (n->GetDevice (j));
    }
  return InstallPriv (n, routing, devs);
}

void
OspfAppHelper::ConfigureReachablePrefixesFromInterfaces (NodeContainer c) const
{
  for (const auto &context : CollectPreloadNodes (c))
    {
      const uint32_t nIf = context.ipv4->GetNInterfaces ();
      for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
        {
          Ipv4InterfaceAddress ifAddr;
          if (!SelectPrimaryInterfaceAddress (context.ipv4, ifIndex, ifAddr))
            {
              continue;
            }

          context.app->SetInterfacePrefixRoutable (ifIndex, true);
        }

      context.app->SetInterfaceReachableAddresses (CollectReachableRoutesFromIpv4 (context));
    }
}

void
OspfAppHelper::Preload (NodeContainer c)
{
  std::vector<LsaRecord> proxiedLsaList;
  AreaLsaSeedMap lsaList;
  std::map<uint32_t, std::vector<AreaLink>> areaAdj;
  std::map<uint32_t, std::set<uint32_t>> areaMembers;
  const auto preloadNodes = CollectPreloadNodes (c);

  for (const auto &context : preloadNodes)
    {
      areaMembers[context.app->GetArea ()].insert (context.app->GetRouterId ().Get ());

      const uint32_t nIf = context.ipv4->GetNInterfaces ();
      for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
        {
          Ipv4InterfaceAddress ifAddr;
          if (!SelectPrimaryInterfaceAddress (context.ipv4, ifIndex, ifAddr))
            {
              continue;
            }
          context.app->SetInterfacePrefixRoutable (ifIndex, true);
        }

      context.app->SetInterfaceReachableAddresses (CollectReachableRoutesFromIpv4 (context));
      const auto l1Key =
          std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, context.app->GetRouterId ().Get (),
                           context.app->GetRouterId ().Get ());
      auto l1 = context.app->FetchLsa (l1Key);
      if (l1.second != nullptr)
        {
          lsaList[context.app->GetArea ()].emplace_back (l1.first.Copy (), l1.second->Copy ());
        }

      const auto adjacencies = CollectPointToPointAdjacencies (context);
      SeedRouterLsaAndNeighbors (context, adjacencies, areaAdj, lsaList);
    }

  proxiedLsaList = BuildProxiedLsas (areaAdj, areaMembers, lsaList);

  for (const auto &context : preloadNodes)
    {
      // Process area-leader LSAs
      bool isLeader = context.app->GetRouterId ().Get () == *areaMembers[context.app->GetArea ()].begin ();
      context.app->SetDoInitialize (false);
      context.app->SetAreaLeader (isLeader);
      context.app->InjectLsa (proxiedLsaList);
      context.app->InjectLsa (lsaList[context.app->GetArea ()]);
    }
}

Ptr<Application>
OspfAppHelper::InstallPriv (Ptr<Node> node, Ptr<OspfRouting> routing,
                            NetDeviceContainer devs) const
{
  Ptr<OspfApp> app = m_factory.Create<OspfApp> ();
  app->SetRouting (routing);
  routing->SetApp (app);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  app->SetRouterId (
      ipv4->GetAddress (1, 0).GetAddress ()); // default to the first interface address
  node->AddApplication (app);
  app->SetBoundNetDevices (devs);

  return app;
}

} // namespace ns3
