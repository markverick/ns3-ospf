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
    apps.Add (Install(*i));
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
  for (uint32_t j = 0; j < n->GetNDevices(); j++) {
    devs.Add(n->GetDevice(j));
  }
  return InstallPriv (n, routing, devs);
}

ApplicationContainer
OspfAppHelper::Install (Ptr<Node> n, std::vector<uint32_t> areas) const
{
  Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (ipv4);
  NetDeviceContainer devs;
  for (uint32_t j = 0; j < n->GetNDevices(); j++) {
    devs.Add(n->GetDevice(j));
  }
  return InstallPriv (n, routing, devs, areas);
}

ApplicationContainer
OspfAppHelper::Install (Ptr<Node> n, std::vector<uint32_t> areas, std::vector<uint32_t> metrices) const
{
  Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (ipv4);
  NetDeviceContainer devs;
  for (uint32_t j = 0; j < n->GetNDevices(); j++) {
    devs.Add(n->GetDevice(j));
  }
  return InstallPriv (n, routing, devs, areas, metrices);
}

// Set all nodes' metrices and areas (only useful in something uniform like a grid topology)
ApplicationContainer
OspfAppHelper::Install (NodeContainer c, std::vector<uint32_t> areas) const
{
  ApplicationContainer apps;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    apps.Add (Install(*i, areas));
  }
  return apps;
}

// Set all nodes' metrices and areas with the same values
// (only useful in something uniform like a grid topology)
ApplicationContainer
OspfAppHelper::Install (NodeContainer c, std::vector<uint32_t> areas, std::vector<uint32_t> metrices) const
{
  // 
  ApplicationContainer apps;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    apps.Add (Install(*i, areas, metrices));
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
OspfAppHelper::InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing,
                            NetDeviceContainer devs, std::vector<uint32_t> areas) const
{
  Ptr<OspfApp> app = m_factory.Create<OspfApp> ();
  app->SetRouting(routing);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  app->SetRouterId(ipv4->GetAddress(1, 0).GetAddress()); //eth0
  node->AddApplication (app);
  app->SetBoundNetDevices(devs);
  if (!areas.empty())  app->SetAreas(areas);

  return app;
}

Ptr<Application>
OspfAppHelper::InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing,
                            NetDeviceContainer devs, std::vector<uint32_t> areas,
                            std::vector<uint32_t> metrices) const
{
  Ptr<OspfApp> app = m_factory.Create<OspfApp> ();
  app->SetRouting(routing);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  app->SetRouterId(ipv4->GetAddress(1, 0).GetAddress()); //eth0
  node->AddApplication (app);
  app->SetBoundNetDevices(devs);
  if (!areas.empty())  app->SetAreas(areas);
  if (!metrices.empty()) app->SetMetrices(metrices);

  return app;
}

} // namespace ns3
