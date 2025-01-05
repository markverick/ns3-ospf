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
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ospf-app.h"
#include "ospf-app-helper.h"

namespace ns3 {

OspfAppHelper::OspfAppHelper (uint16_t port)
{
  m_factory.SetTypeId (OspfApp::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
}

void 
OspfAppHelper::SetAttribute (
  std::string name, 
  const AttributeValue &value)
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
    Ptr<Ipv4> ipv4 = (*i)->GetObject<Ipv4> ();
    Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (ipv4);
    NetDeviceContainer devs;
    for (uint32_t j = 0; j < (*i)->GetNDevices(); j++) {
      devs.Add((*i)->GetDevice(j));
    }
    apps.Add (InstallPriv (*i, routing, devs));
  }
  return apps;
}

void
OspfAppHelper::InstallGateway (NodeContainer c, std::vector<uint32_t> ifIndices, Ipv4Address nextHopIp) const
{
  InstallGateway(c, ifIndices, Ipv4Address("0.0.0.0"), Ipv4Mask("0.0.0.0"), nextHopIp);
  return;
}

void
OspfAppHelper::InstallGateway (NodeContainer c, std::vector<uint32_t> ifIndices, Ipv4Address destIp, Ipv4Mask mask, Ipv4Address nextHopIp) const
{
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<OspfApp> ospfApp;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    ospfApp = DynamicCast<OspfApp>((*i)->GetApplication(0));
    for (uint32_t j = 0; j < ifIndices.size(); j++) {
      ospfApp->SetOSPFGateway(ifIndices[j], destIp, mask, nextHopIp);
    }
  }
  return;
}

ApplicationContainer
OspfAppHelper::Install (NodeContainer c, std::vector<uint32_t> areas) const
{
  ApplicationContainer apps;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    Ptr<Ipv4> ipv4 = (*i)->GetObject<Ipv4> ();
    Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (ipv4);
    NetDeviceContainer devs;
    for (uint32_t j = 0; j < (*i)->GetNDevices(); j++) {
      devs.Add((*i)->GetDevice(j));
    }
    apps.Add (InstallPriv (*i, routing, devs, areas));
  }
  return apps;
}

Ptr<Application>
OspfAppHelper::InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs) const
{
  Ptr<OspfApp> app = m_factory.Create<OspfApp> ();
  app->SetRouting(routing);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  app->SetRouterId(ipv4->GetAddress(1, 0).GetAddress()); //eth0
  node->AddApplication (app);
  app->SetBoundNetDevices(devs);

  return app;
}

Ptr<Application>
OspfAppHelper::InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs, std::vector<uint32_t> areas) const
{
  Ptr<OspfApp> app = m_factory.Create<OspfApp> ();
  app->SetRouting(routing);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  app->SetRouterId(ipv4->GetAddress(1, 0).GetAddress()); //eth0
  node->AddApplication (app);
  app->SetBoundNetDevices(devs, areas);

  return app;
}

} // namespace ns3
