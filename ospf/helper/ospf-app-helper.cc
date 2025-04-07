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
#include "ospf-app-helper.h"

#include <map>

namespace ns3 {

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
      devs.Add (n->GetDevice (j));
    }
  return InstallPriv (n, routing, devs);
}

void
OspfAppHelper::Preload (NodeContainer c)
{
  // Ipv4Address remoteRouterId, Ipv4Address remoteIp, uint32_t remoteAreaId,
  // OspfNeighbor::NeighborState state
  std::vector<std::pair<LsaHeader, Ptr<Lsa>>> proxiedLsaList;
  std::map<uint32_t, std::vector<std::pair<LsaHeader, Ptr<Lsa>>>> lsaList;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      auto app = DynamicCast<OspfApp> (node->GetApplication (0));
      auto numIf = node->GetNDevices ();

      // Neighbor Values
      Ipv4Address remoteRouterId;
      Ipv4Address remoteIp, selfIp;
      uint32_t remoteAreaId;
      Ptr<RouterLsa> routerLsa = Create<RouterLsa> ();
      for (uint32_t ifIndex = 0; ifIndex < numIf; ifIndex++)
        {
          Ptr<NetDevice> dev = node->GetDevice (numIf);
          if (!dev->IsPointToPoint ())
            {
              // TODO: Only support p2p for now
              continue;
            }
          selfIp = ipv4->GetAddress (dev->GetIfIndex (), 0).GetAddress ();
          Ptr<NetDevice> remoteDev;
          auto ch = DynamicCast<PointToPointChannel> (dev->GetChannel ());
          for (uint32_t j = 0; j < ch->GetNDevices (); j++)
            {
              remoteDev = ch->GetDevice (j);
              if (remoteDev != dev)
                {
                  // Set as a gateway
                  auto remoteIpv4 = remoteDev->GetNode ()->GetObject<Ipv4> ();
                  auto remoteApp = DynamicCast<OspfApp> (remoteDev->GetNode ()->GetApplication (0));
                  remoteRouterId = remoteApp->GetRouterId ();
                  remoteIp = remoteIpv4->GetAddress (remoteDev->GetIfIndex (), 0).GetAddress ();
                  remoteAreaId = remoteApp->GetArea ();
                  // RouterLink (uint32_t linkId, uint32_t linkData, uint8_t type, uint16_t metric)
                  routerLsa->AddLink (RouterLink (remoteRouterId.Get (), selfIp.Get (), 1,
                                                  app->GetMetric (ifIndex)));
                  break;
                }
            }
          app->AddNeighbor (ifIndex, Create<OspfNeighbor> (remoteRouterId, remoteIp, remoteAreaId,
                                                           OspfNeighbor::NeighborState::Full));
        }
      // RouterLsa
      LsaHeader lsaHeader (std::make_tuple (LsaHeader::LsType::RouterLSAs, app->GetArea (),
                                            app->GetRouterId ().Get ()));
      lsaHeader.SetLength (20 + routerLsa->GetSerializedSize ());
      lsaHeader.SetSeqNum (1);
      lsaList[app->GetArea ()].emplace_back (LsaHeader (), routerLsa);
    }
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      auto app = DynamicCast<OspfApp> (node->GetApplication (0));
      // Process area-leader LSAs
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
