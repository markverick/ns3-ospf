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
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ospf-app.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/ospf-neighbor.h"
#include "ns3/lsa.h"
#include "ns3/area-lsa.h"
#include "ospf-app-helper.h"

#include <map>
#include <set>
#include <tuple>

namespace ns3 {

namespace {

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
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
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
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (ipv4);
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

      std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> reachable;
      for (uint32_t ifIndex = 1; ifIndex < node->GetNDevices (); ++ifIndex)
        {
          Ptr<NetDevice> dev = node->GetDevice (ifIndex);
          if (dev == nullptr || !dev->IsPointToPoint ())
            {
              continue;
            }
          const auto ifAddr = ipv4->GetAddress (ifIndex, 0);
          const auto addr = ifAddr.GetAddress ();
          const auto mask = ifAddr.GetMask ();
          const auto dest = addr.CombineMask (mask);
          reachable.emplace_back (ifIndex, dest.Get (), mask.Get (), addr.Get (), 1);
        }
      app->SetReachableAddresses (std::move (reachable));
    }
}

void
OspfAppHelper::Preload (NodeContainer c)
{
  // Ipv4Address remoteRouterId, Ipv4Address remoteIp, uint32_t remoteAreaId,
  // OspfNeighbor::NeighborState state
  std::vector<std::pair<LsaHeader, Ptr<Lsa>>> proxiedLsaList;
  std::map<uint32_t, std::vector<std::pair<LsaHeader, Ptr<Lsa>>>> lsaList;
  std::map<uint32_t, std::vector<AreaLink>> areaAdj;
  std::map<uint32_t, std::set<uint32_t>> areaMembers;
  std::map<uint32_t, Ipv4Mask> areaMasks;
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

      areaMembers[app->GetArea ()].insert (app->GetRouterId ().Get ());
      areaMasks[app->GetArea ()] = app->GetAreaMask ();

      // Neighbor Values
      Ipv4Address remoteRouterId;
      Ipv4Address remoteIp, selfIp;
      uint32_t remoteAreaId;

      // Router LSA and Neighbor Status
      Ptr<RouterLsa> routerLsa = Create<RouterLsa> ();
      std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> reachable;
      auto numIf = node->GetNDevices ();
      for (uint32_t ifIndex = 0; ifIndex < numIf; ifIndex++)
        {
          Ptr<NetDevice> dev = node->GetDevice (ifIndex);
          if (!dev->IsPointToPoint ())
            {
              // TODO: Only support p2p for now
              continue;
            }
          auto selfIfAddr = ipv4->GetAddress (dev->GetIfIndex (), 0);
          selfIp = selfIfAddr.GetAddress ();

          // Populate reachable (advertised) prefixes via SetReachableAddresses().
          const auto mask = selfIfAddr.GetMask ();
          const auto dest = selfIp.CombineMask (mask);
          // Tuple is (ifIndex, dest, mask, gateway, metric).
          reachable.emplace_back (dev->GetIfIndex (), dest.Get (), mask.Get (), selfIp.Get (), 1);

          Ptr<NetDevice> remoteDev;
          auto ch = DynamicCast<Channel> (dev->GetChannel ());
          for (uint32_t j = 0; j < ch->GetNDevices (); j++)
            {
              remoteDev = ch->GetDevice (j);
              if (remoteDev != dev)
                {
                  // Set as a gateway
                  auto remoteIpv4 = remoteDev->GetNode ()->GetObject<Ipv4> ();
                  if (remoteIpv4 == nullptr)
                    {
                      continue;
                    }
                  auto remoteApp = FindOspfApp (remoteDev->GetNode ());
                  if (remoteApp == nullptr)
                    {
                      continue;
                    }
                  remoteRouterId = remoteApp->GetRouterId ();
                  remoteIp = remoteIpv4->GetAddress (remoteDev->GetIfIndex (), 0).GetAddress ();
                  remoteAreaId = remoteApp->GetArea ();

                  // Add neighbor with status FULL
                  app->AddNeighbor (ifIndex,
                                    Create<OspfNeighbor> (remoteRouterId, remoteIp, remoteAreaId,
                                                          OspfNeighbor::NeighborState::Init));
                  // RouterLink (uint32_t linkId, uint32_t linkData, uint8_t type, uint16_t metric)
                  // Router LSA
                  if (remoteAreaId == app->GetArea ())
                    {
                      // Intra-area Link
                      routerLsa->AddLink (RouterLink (remoteRouterId.Get (), selfIp.Get (), 1,
                                                      app->GetMetric (ifIndex)));
                    }
                  else
                    {
                      // Inter-area Link
                      routerLsa->AddLink (
                          RouterLink (remoteAreaId, selfIp.Get (), 5, app->GetMetric (ifIndex)));
                      areaAdj[app->GetArea ()].emplace_back (
                          AreaLink (remoteAreaId, selfIp.Get (), app->GetMetric (ifIndex)));
                    }
                  break;
                }
            }
        }
      LsaHeader routerLsaHeader (std::make_tuple (
          LsaHeader::LsType::RouterLSAs, app->GetRouterId ().Get (), app->GetRouterId ().Get ()));
      routerLsaHeader.SetLength (20 + routerLsa->GetSerializedSize ());
      routerLsaHeader.SetSeqNum (1);
      lsaList[app->GetArea ()].emplace_back (routerLsaHeader, routerLsa);

      // Configure reachable prefixes, then retrieve the app-generated L1SummaryLSA for seeding.
      app->SetReachableAddresses (std::move (reachable));

      const auto l1Key =
          std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, app->GetRouterId ().Get (),
                           app->GetRouterId ().Get ());
      auto l1 = app->FetchLsa (l1Key);
      if (l1.second != nullptr)
        {
          lsaList[app->GetArea ()].emplace_back (l1.first.Copy (), l1.second->Copy ());
        }
    }

  // Proxied LSA
  for (auto &[areaId, adj] : areaAdj)
    {
      Ptr<AreaLsa> areaLsa = Create<AreaLsa> ();
      for (auto areaLink : adj)
        {
          areaLsa->AddLink (areaLink);
        }
      // Area LSA
      LsaHeader areaLsaHeader (
          std::make_tuple (LsaHeader::LsType::AreaLSAs, areaId, *areaMembers[areaId].begin ()));
      areaLsaHeader.SetLength (20 + areaLsa->GetSerializedSize ());
      areaLsaHeader.SetSeqNum (1);
      proxiedLsaList.emplace_back (areaLsaHeader, areaLsa);

      // TODO: Preload L2 area summary
    }
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

      // Process area-leader LSAs
      bool isLeader = app->GetRouterId ().Get () == *areaMembers[app->GetArea ()].begin ();
      app->SetDoInitialize (false);
      app->SetAreaLeader (isLeader);
      app->InjectLsa (proxiedLsaList);
      app->InjectLsa (lsaList[app->GetArea ()]);
    }
}

Ptr<Application>
OspfAppHelper::InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing,
                            NetDeviceContainer devs) const
{
  Ptr<OspfApp> app = m_factory.Create<OspfApp> ();
  app->SetRouting (routing);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  app->SetRouterId (
      ipv4->GetAddress (1, 0).GetAddress ()); // default to the first interface address
  node->AddApplication (app);
  app->SetBoundNetDevices (devs);

  return app;
}

} // namespace ns3
