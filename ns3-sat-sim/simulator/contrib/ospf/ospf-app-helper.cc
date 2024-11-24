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
#include "ospf-app-helper.h"
#include "ospf-app.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/ipv4-static-routing-helper.h"

namespace ns3 {

OSPFAppHelper::OSPFAppHelper (uint16_t port)
{
  m_factory.SetTypeId (OSPFApp::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
}

void 
OSPFAppHelper::SetAttribute (
  std::string name, 
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
OSPFAppHelper::Install (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs) const
{
  return ApplicationContainer (InstallPriv (node, routing, devs));
}

ApplicationContainer
OSPFAppHelper::Install (std::string nodeName, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node, routing, devs));
}

ApplicationContainer
OSPFAppHelper::Install (NodeContainer c) const
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

Ptr<Application>
OSPFAppHelper::InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs) const
{
  Ptr<OSPFApp> app = m_factory.Create<OSPFApp> ();
  app->SetRouting(routing);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  app->SetRouterId(ipv4->GetAddress(1, 0).GetAddress()); //eth0
  node->AddApplication (app);
  app->SetBoundNetDevices(devs);

  return app;
}

} // namespace ns3
