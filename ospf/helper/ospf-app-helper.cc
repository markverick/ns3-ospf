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

NS_LOG_COMPONENT_DEFINE ("OspfAppHelper");

struct OspfAppHelper::PreloadNodeContext
{
  Ptr<Node> node;
  Ptr<Ipv4> ipv4;
  Ptr<OspfApp> app;
};

struct OspfAppHelper::PointToPointAdjacency
{
  uint32_t ifIndex = 0;
  Ipv4Address selfIp;
  Ipv4Address remoteRouterId;
  Ipv4Address remoteIp;
  uint32_t remoteAreaId = 0;
  uint16_t metric = 0;
};

namespace {

using LsaRecord = std::pair<LsaHeader, Ptr<Lsa>>;
using AreaLsaSeedMap = std::map<uint32_t, std::vector<LsaRecord>>;

uint16_t
ToLinkMetric (uint32_t metric, uint32_t ifIndex)
{
  NS_ABORT_MSG_IF (metric > std::numeric_limits<uint16_t>::max (),
                   "OSPF preload metric " << metric << " on interface " << ifIndex
                                           << " exceeds RouterLink/AreaLink encoding width");
  return static_cast<uint16_t> (metric);
}

LsaRecord
MakeLsaRecord (LsaHeader::LsType type, uint32_t lsId, uint32_t advertisingRouter, Ptr<Lsa> lsa)
{
  LsaHeader header (std::make_tuple (type, lsId, advertisingRouter));
  header.SetLength (20 + lsa->GetSerializedSize ());
  header.SetSeqNum (1);
  return {header, lsa};
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

Ptr<OspfRouting>
OspfAppHelper::GetOrCreateRouting (Ptr<Node> node)
{
  Ptr<Ipv4> ipv4 = node != nullptr ? node->GetObject<Ipv4> () : nullptr;
  NS_ABORT_MSG_IF (ipv4 == nullptr, "OspfAppHelper requires an Ipv4 stack on the node");

  Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4->GetRoutingProtocol ();
  Ptr<OspfRouting> routing = Ipv4RoutingHelper::GetRouting<OspfRouting> (routingProtocol);
  if (routing != nullptr)
    {
      return routing;
    }

  auto listRouting = DynamicCast<Ipv4ListRouting> (routingProtocol);
  NS_ABORT_MSG_IF (listRouting == nullptr,
                   "OspfAppHelper requires Ipv4ListRouting on the node IPv4 stack");

  routing = CreateObject<OspfRouting> ();
  listRouting->AddRoutingProtocol (routing, 10);
  return routing;
}

Ptr<OspfApp>
OspfAppHelper::FindOspfApp (const Ptr<Node> &node)
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
OspfAppHelper::HasSelectedInterface (const Ptr<OspfApp> &app, uint32_t ifIndex)
{
  return app != nullptr && app->HasOspfInterface (ifIndex);
}

bool
OspfAppHelper::SelectPrimaryInterfaceAddress (Ptr<Ipv4> ipv4, uint32_t ifIndex,
                                              Ipv4InterfaceAddress &out)
{
  return OspfApp::SelectPrimaryInterfaceAddress (ipv4, ifIndex, out);
}

NetDeviceContainer
OspfAppHelper::CollectIpv4BoundDevices (Ptr<Ipv4> ipv4)
{
  NetDeviceContainer devs;
  if (ipv4 == nullptr)
    {
      return devs;
    }

  for (uint32_t ifIndex = 0; ifIndex < ipv4->GetNInterfaces (); ++ifIndex)
    {
      Ptr<NetDevice> dev = ipv4->GetNetDevice (ifIndex);
      if (dev != nullptr)
        {
          devs.Add (dev);
        }
    }

  return devs;
}

void
OspfAppHelper::PopulateSelectedReachableRoutes (const Ptr<OspfApp> &app, Ptr<Ipv4> ipv4)
{
  if (app == nullptr || ipv4 == nullptr)
    {
      return;
    }

  OspfApp::ReachableRouteList reachable;
  const uint32_t nIf = ipv4->GetNInterfaces ();
  for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
    {
      if (!HasSelectedInterface (app, ifIndex))
        {
          continue;
        }

      Ipv4InterfaceAddress ifAddr;
      if (!SelectPrimaryInterfaceAddress (ipv4, ifIndex, ifAddr))
        {
          continue;
        }

      app->SetInterfacePrefixRoutable (ifIndex, true);
      const auto addr = ifAddr.GetAddress ();
      const auto mask = ifAddr.GetMask ();
      reachable.emplace_back (ifIndex, addr.CombineMask (mask).Get (), mask.Get (), addr.Get (),
                              1);
    }

  app->SetInterfaceReachableAddresses (std::move (reachable));
}

std::vector<OspfAppHelper::PreloadNodeContext>
OspfAppHelper::CollectPreloadNodes (NodeContainer c)
{
  std::vector<PreloadNodeContext> preloadNodes;
  preloadNodes.reserve (c.GetN ());

  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ptr<OspfApp> app = FindOspfApp (node);
      if (ipv4 == nullptr || app == nullptr)
        {
          continue;
        }
      preloadNodes.push_back ({node, ipv4, app});
    }

  return preloadNodes;
}

std::vector<OspfAppHelper::PointToPointAdjacency>
OspfAppHelper::CollectPointToPointAdjacencies (const PreloadNodeContext &context)
{
  std::vector<PointToPointAdjacency> adjacencies;
  const uint32_t nIf = context.ipv4->GetNInterfaces ();
  for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
    {
      if (!HasSelectedInterface (context.app, ifIndex))
        {
          continue;
        }

      Ptr<NetDevice> dev = context.ipv4->GetNetDevice (ifIndex);
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

          Ptr<Node> remoteNode = remoteDev->GetNode ();
          Ptr<Ipv4> remoteIpv4 = remoteNode != nullptr ? remoteNode->GetObject<Ipv4> () : nullptr;
          Ptr<OspfApp> remoteApp = remoteNode != nullptr ? FindOspfApp (remoteNode) : nullptr;
          if (remoteIpv4 == nullptr || remoteApp == nullptr ||
              remoteApp->GetRouterId () == Ipv4Address::GetZero ())
            {
              continue;
            }

          const int32_t remoteIfIndex = remoteIpv4->GetInterfaceForDevice (remoteDev);
          if (remoteIfIndex <= 0 ||
              !HasSelectedInterface (remoteApp, static_cast<uint32_t> (remoteIfIndex)))
            {
              continue;
            }

          Ipv4InterfaceAddress remoteIfAddr;
          if (!SelectPrimaryInterfaceAddress (remoteIpv4,
                                              static_cast<uint32_t> (remoteIfIndex),
                                              remoteIfAddr))
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
OspfAppHelper::SeedRouterLsaAndNeighbors (const PreloadNodeContext &context,
                                          const std::vector<PointToPointAdjacency> &adjacencies,
                                          std::map<uint32_t, std::vector<AreaLink>> &areaAdj,
                                          AreaLsaSeedMap &lsaList)
{
  Ptr<RouterLsa> routerLsa = Create<RouterLsa> ();
  for (const auto &adjacency : adjacencies)
    {
      context.app->AddNeighbor (adjacency.ifIndex,
                                Create<OspfNeighbor> (adjacency.remoteRouterId,
                                                      adjacency.remoteIp,
                                                      adjacency.remoteAreaId,
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
          routerLsa->AddLink (RouterLink (adjacency.remoteAreaId,
                                          adjacency.selfIp.Get (),
                                          5,
                                          adjacency.metric));
          areaAdj[context.app->GetArea ()].emplace_back (
              AreaLink (adjacency.remoteAreaId, adjacency.selfIp.Get (), adjacency.metric));
        }
    }

  lsaList[context.app->GetArea ()].emplace_back (
      MakeLsaRecord (LsaHeader::LsType::RouterLSAs,
                     context.app->GetRouterId ().Get (),
                     context.app->GetRouterId ().Get (),
                     routerLsa));
}

bool
OspfAppHelper::SeedPreloadNode (const PreloadNodeContext &context,
                                std::map<uint32_t, std::set<uint32_t>> &areaMembers,
                                std::map<uint32_t, std::vector<AreaLink>> &areaAdj,
                                AreaLsaSeedMap &lsaList)
{
  if (context.app->GetRouterId () == Ipv4Address::GetZero ())
    {
      NS_LOG_WARN ("Skipping OSPF preload for node with unset router ID");
      return false;
    }

  areaMembers[context.app->GetArea ()].insert (context.app->GetRouterId ().Get ());
  PopulateSelectedReachableRoutes (context.app, context.ipv4);

  const auto l1Key =
      std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, context.app->GetRouterId ().Get (),
                       context.app->GetRouterId ().Get ());
  auto l1 = context.app->FetchLsa (l1Key);
  if (l1.second != nullptr)
    {
      lsaList[context.app->GetArea ()].emplace_back (l1.first.Copy (), l1.second->Copy ());
    }

  SeedRouterLsaAndNeighbors (context, CollectPointToPointAdjacencies (context), areaAdj, lsaList);
  return true;
}

void
OspfAppHelper::ApplyPreloadedLsas (const std::vector<PreloadNodeContext> &preloadNodes,
                                   const std::map<uint32_t, std::set<uint32_t>> &areaMembers,
                                   const AreaLsaSeedMap &lsaList,
                                   const std::vector<LsaRecord> &proxiedLsaList)
{
  for (const auto &context : preloadNodes)
    {
      auto membersIt = areaMembers.find (context.app->GetArea ());
      if (membersIt == areaMembers.end () || membersIt->second.empty ())
        {
          continue;
        }

      const bool isLeader =
          context.app->GetRouterId ().Get () == *membersIt->second.begin ();
      context.app->SetDoInitialize (false);
      context.app->SetAreaLeader (isLeader);
      context.app->InjectLsa (proxiedLsaList);

      auto lsaIt = lsaList.find (context.app->GetArea ());
      if (lsaIt != lsaList.end ())
        {
          context.app->InjectLsa (lsaIt->second);
        }
    }
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
  Ptr<Ipv4> ipv4 = n != nullptr ? n->GetObject<Ipv4> () : nullptr;
  NS_ABORT_MSG_IF (ipv4 == nullptr, "OspfAppHelper requires an Ipv4 stack on the node");

  return InstallPriv (n, GetOrCreateRouting (n), CollectIpv4BoundDevices (ipv4));
}

ApplicationContainer
OspfAppHelper::Install (Ptr<Node> n, NetDeviceContainer devs) const
{
  return InstallPriv (n, GetOrCreateRouting (n), devs);
}

void
OspfAppHelper::ConfigureReachablePrefixesFromInterfaces (NodeContainer c) const
{
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ptr<OspfApp> app = FindOspfApp (node);
      if (ipv4 == nullptr || app == nullptr)
        {
          continue;
        }
      PopulateSelectedReachableRoutes (app, ipv4);
    }
}

void
OspfAppHelper::Preload (NodeContainer c)
{
  AreaLsaSeedMap lsaList;
  std::map<uint32_t, std::vector<AreaLink>> areaAdj;
  std::map<uint32_t, std::set<uint32_t>> areaMembers;
  const auto preloadNodes = CollectPreloadNodes (c);
  std::vector<PreloadNodeContext> seededNodes;
  seededNodes.reserve (preloadNodes.size ());

  for (const auto &context : preloadNodes)
    {
      if (SeedPreloadNode (context, areaMembers, areaAdj, lsaList))
        {
          seededNodes.push_back (context);
        }
    }

  ApplyPreloadedLsas (seededNodes,
                      areaMembers,
                      lsaList,
                      BuildProxiedLsas (areaAdj, areaMembers, lsaList));
}

Ptr<Application>
OspfAppHelper::InstallPriv (Ptr<Node> node, Ptr<OspfRouting> routing,
                            NetDeviceContainer devs) const
{
  Ptr<OspfApp> app = m_factory.Create<OspfApp> ();
  app->SetRouting (routing);
  routing->SetApp (app);
  node->AddApplication (app);
  app->SetBoundNetDevices (devs);

  return app;
}

} // namespace ns3
