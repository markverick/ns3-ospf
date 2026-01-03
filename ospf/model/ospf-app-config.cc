/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {
void
OspfApp::SetRouting (Ptr<Ipv4StaticRouting> routing)
{
  m_routing = routing;
}

void
OspfApp::SetBoundNetDevices (NetDeviceContainer devs)
{
  NS_LOG_FUNCTION (this << devs.GetN ());
  m_boundDevices = devs;
  m_lastHelloReceived.resize (devs.GetN ());
  m_helloTimeouts.resize (devs.GetN ());

  // Create interface database
  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();

  // Add loopbacks at index 0
  Ptr<OspfInterface> ospfInterface = Create<OspfInterface> ();
  m_ospfInterfaces.emplace_back (ospfInterface);

  // Add the rest of net devices
  for (uint32_t i = 1; i < m_boundDevices.GetN (); i++)
    {
      auto sourceIp = ipv4->GetAddress (i, 0).GetAddress ();
      auto mask = ipv4->GetAddress (i, 0).GetMask ();
      Ptr<OspfInterface> ospfInterface = Create<OspfInterface> (
          sourceIp, mask, m_helloInterval.GetMilliSeconds (), m_routerDeadInterval.GetMilliSeconds (),
          m_areaId, 1, m_boundDevices.Get (i)->GetMtu ());

      // Set default routes
      if (m_boundDevices.Get (i)->IsPointToPoint ())
        {

          // Get remote IP address
          auto dev = m_boundDevices.Get (i);
          auto ch = DynamicCast<Channel> (dev->GetChannel ());
          Ptr<NetDevice> remoteDev;
          for (uint32_t j = 0; j < ch->GetNDevices (); j++)
            {
              remoteDev = ch->GetDevice (j);
              if (remoteDev != dev)
                {
                  // Set as a gateway
                  auto remoteIpv4 = remoteDev->GetNode ()->GetObject<Ipv4> ();
                  // std::cout << " ! Num IF: " << ipv4->GetNInterfaces () << " / " << remoteDev->GetIfIndex () << std::endl;
                  // std::cout << " !! GW : " << sourceIp << " -> " << remoteIpv4->GetAddress(remoteDev->GetIfIndex (), 0).GetAddress () << std::endl;
                  ospfInterface->SetGateway (
                      remoteIpv4->GetAddress (remoteDev->GetIfIndex (), 0).GetAddress ());
                  break;
                }
            }
        }
      else
        {
          ospfInterface->SetGateway (Ipv4Address::GetBroadcast ());
        }
      m_ospfInterfaces.emplace_back (ospfInterface);
    }
}

void
OspfApp::AddReachableAddress (uint32_t ifIndex, Ipv4Address dest, Ipv4Mask mask,
                              Ipv4Address gateway, uint32_t metric)
{
  // Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  // ipv4->AddAddress (0, Ipv4InterfaceAddress (dest, mask));
  // std::cout << "Inject: " << ifIndex << ", " << dest << ", " << mask << ", " << gateway << ", " << metric << std::endl;
  m_externalRoutes.emplace_back (ifIndex, dest.Get (), mask.Get (), gateway.Get (), metric);
  RecomputeL1SummaryLsa ();
  // Process the new LSA and generate/flood L2 Summary LSA if needed
  ProcessLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
}

void
OspfApp::AddReachableAddress (uint32_t ifIndex, Ipv4Address address, Ipv4Mask mask)
{
  m_externalRoutes.emplace_back (ifIndex, address.Get (), mask.Get (),
                                 Ipv4Address::GetAny ().Get (), 0);
  RecomputeL1SummaryLsa ();
  // Process the new LSA and generate/flood L2 Summary LSA if needed
  ProcessLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
}

bool
OspfApp::SetReachableAddresses (
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> reachableAddresses)
{
  // Flood when the content is different
  if (m_externalRoutes != reachableAddresses)
    {
      m_externalRoutes = std::move (reachableAddresses);
      RecomputeL1SummaryLsa ();
      // Process the new LSA and generate/flood L2 Summary LSA if needed
      ProcessLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
      return true;
    }
  return false;
}

void
OspfApp::AddAllReachableAddresses (uint32_t ifIndex)
{
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  for (uint32_t i = 1; i < m_boundDevices.GetN (); i++)
    {
      if (ifIndex == i)
        continue;
      AddReachableAddress (
          ifIndex, m_ospfInterfaces[i]->GetAddress ().CombineMask (m_ospfInterfaces[i]->GetMask ()),
          m_ospfInterfaces[i]->GetMask (), m_ospfInterfaces[i]->GetAddress (), 0);
    }
  RecomputeL1SummaryLsa ();
  // Process the new LSA and generate/flood L2 Summary LSA if needed
  ProcessLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
}


void
OspfApp::ClearReachableAddresses (uint32_t ifIndex)
{
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  while (ipv4->GetNAddresses (ifIndex))
    {
      uint32_t addrIndex = ipv4->GetNAddresses (ifIndex) - 1;
      if (ipv4->GetAddress (ifIndex, addrIndex).GetAddress ().IsLocalhost ())
        {
          break;
        }
      ipv4->RemoveAddress (ifIndex, addrIndex);
    }
}

void
OspfApp::RemoveReachableAddress (uint32_t ifIndex, Ipv4Address address, Ipv4Mask mask)
{
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  ipv4->RemoveAddress (0, address);
}

void
OspfApp::SetArea (uint32_t area)
{
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      m_ospfInterfaces[i]->SetArea (area);
    }
  m_areaId = area;
}

uint32_t
OspfApp::GetArea ()
{
  return m_areaId;
}

void
OspfApp::SetAreaLeader (bool isLeader)
{
  m_isAreaLeader = isLeader;
}

void
OspfApp::SetDoInitialize (bool doInitialize)
{
  m_doInitialize = doInitialize;
}

Ipv4Mask
OspfApp::GetAreaMask ()
{
  return m_areaMask;
}

void
OspfApp::SetMetrices (std::vector<uint32_t> metrices)
{
  NS_ASSERT_MSG (metrices.size () == m_ospfInterfaces.size (),
                 "The length of metrices must match the number of interfaces");
  for (uint32_t i = 0; i < m_ospfInterfaces.size (); i++)
    {
      m_ospfInterfaces[i]->SetMetric (metrices[i]);
    }
}

uint32_t
OspfApp::GetMetric (uint32_t ifIndex)
{
  return m_ospfInterfaces[ifIndex]->GetMetric ();
}

void
OspfApp::SetRouterId (Ipv4Address routerId)
{
  m_routerId = routerId;
}

Ipv4Address
OspfApp::GetRouterId ()
{
  return m_routerId;
}

std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>>
OspfApp::GetLsdb ()
{
  return m_routerLsdb;
}

std::map<uint32_t, std::pair<LsaHeader, Ptr<L1SummaryLsa>>>
OspfApp::GetL1SummaryLsdb ()
{
  return m_l1SummaryLsdb;
}

std::map<uint32_t, std::pair<LsaHeader, Ptr<AreaLsa>>>
OspfApp::GetAreaLsdb ()
{
  return m_areaLsdb;
}

std::map<uint32_t, std::pair<LsaHeader, Ptr<L2SummaryLsa>>>
OspfApp::GetL2SummaryLsdb ()
{
  return m_l2SummaryLsdb;
}

void
OspfApp::PrintLsdb ()
{
  if (m_routerLsdb.empty ())
    return;
  std::cout << "==== [ " << m_routerId << " : " << m_areaId << "] Router LSDB"
            << " =====" << std::endl;
  for (auto &pair : m_routerLsdb)
    {
      std::cout << "  At t=" << Simulator::Now ().GetSeconds ()
                << " , Router: " << Ipv4Address (pair.first) << std::endl;
      std::cout << "    Neighbors: " << pair.second.second->GetNLink () << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          RouterLink link = pair.second.second->GetLink (i);
          std::cout << "    (" << Ipv4Address (link.m_linkId) << ", "
                    << Ipv4Address (link.m_linkData) << ", " << link.m_metric << ", "
                    << (uint32_t) (link.m_type) << ")" << std::endl;
        }
    }
  std::cout << std::endl;
}

void
OspfApp::PrintL1SummaryLsdb ()
{
  if (m_l1SummaryLsdb.empty ())
    return;
  std::cout << "==== [ " << m_routerId << " : " << m_areaId << "] L1 Summary LSDB"
            << " =====" << std::endl;
  for (auto &pair : m_l1SummaryLsdb)
    {
      std::cout << "  At t=" << Simulator::Now ().GetSeconds ()
                << " , Router: " << Ipv4Address (pair.first) << std::endl;
      auto lsa = pair.second.second;
      for (auto route : lsa->GetRoutes ())
        {
          std::cout << "    (" << Ipv4Address (route.m_address) << ", " << Ipv4Mask (route.m_mask)
                    << ", " << route.m_metric << ")" << std::endl;
        }
    }
  std::cout << std::endl;
}

void
OspfApp::PrintAreaLsdb ()
{
  if (m_areaLsdb.empty ())
    return;
  std::cout << "==== [ " << m_routerId << " : " << m_areaId << "] Area LSDB"
            << " =====" << std::endl;
  for (auto &pair : m_areaLsdb)
    {
      std::cout << "  At t=" << Simulator::Now ().GetSeconds () << " , Area: " << pair.first
                << std::endl;
      std::cout << "    Neighbors: " << pair.second.second->GetNLink () << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          AreaLink link = pair.second.second->GetLink (i);
          std::cout << "    (" << link.m_areaId << ", " << Ipv4Address (link.m_ipAddress) << ", "
                    << link.m_metric << ")" << std::endl;
        }
    }
  std::cout << std::endl;
}

void
OspfApp::PrintL2SummaryLsdb ()
{
  if (m_l2SummaryLsdb.empty ())
    return;
  std::cout << "==== [ " << m_routerId << " : " << m_areaId << "] L2 Summary LSDB"
            << " =====" << std::endl;
  for (auto &pair : m_l2SummaryLsdb)
    {
      std::cout << "  At t=" << Simulator::Now ().GetSeconds () << " , Area: " << pair.first
                << std::endl;
      auto lsa = pair.second.second;
      for (auto route : lsa->GetRoutes ())
        {
          std::cout << "    (" << Ipv4Address (route.m_address) << ", " << Ipv4Mask (route.m_mask)
                    << ", " << route.m_metric << ")" << std::endl;
        }
    }
  std::cout << std::endl;
}

void
OspfApp::PrintRouting (std::filesystem::path dirName, std::string filename)
{
  try
    {
      Ptr<OutputStreamWrapper> routingStream =
          Create<OutputStreamWrapper> (dirName / filename, std::ios::out);
      m_routing->PrintRoutingTable (routingStream);
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error: " << e.what () << std::endl;
    }
}

void
OspfApp::PrintAreas ()
{
  std::cout << "Area:";
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      std::cout << " " << m_ospfInterfaces[i]->GetArea ();
    }
  std::cout << std::endl;
}

uint32_t
OspfApp::GetLsdbHash ()
{
  std::stringstream ss;
  for (auto &pair : m_routerLsdb)
    {
      ss << Ipv4Address (pair.first) << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          RouterLink link = pair.second.second->GetLink (i);
          ss << "  (" << Ipv4Address (link.m_linkData) << Ipv4Address (link.m_metric) << ")"
             << std::endl;
        }
    }
  std::hash<std::string> hasher;
  return hasher (ss.str ());
}

uint32_t
OspfApp::GetL1SummaryLsdbHash ()
{
  std::stringstream ss;
  for (auto &pair : m_l1SummaryLsdb)
    {
      ss << Ipv4Address (pair.first) << std::endl;
      Ptr<L1SummaryLsa> lsa = pair.second.second;
      for (auto route : lsa->GetRoutes ())
        {
          ss << "    (" << Ipv4Address (route.m_address) << ", " << Ipv4Mask (route.m_mask) << ", "
             << route.m_metric << ")" << std::endl;
        }
    }
  std::hash<std::string> hasher;
  return hasher (ss.str ());
}

uint32_t
OspfApp::GetAreaLsdbHash ()
{
  std::stringstream ss;
  for (auto &pair : m_areaLsdb)
    {
      ss << Ipv4Address (pair.first) << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          AreaLink link = pair.second.second->GetLink (i);
          ss << "  (" << link.m_areaId << link.m_ipAddress << link.m_metric << ")" << std::endl;
        }
    }
  std::hash<std::string> hasher;
  return hasher (ss.str ());
}

uint32_t
OspfApp::GetL2SummaryLsdbHash ()
{
  std::stringstream ss;
  for (auto &pair : m_l2SummaryLsdb)
    {
      ss << Ipv4Address (pair.first) << std::endl;
      Ptr<L2SummaryLsa> lsa = pair.second.second;
      for (auto route : lsa->GetRoutes ())
        {
          ss << "    (" << Ipv4Address (route.m_address) << ", " << Ipv4Mask (route.m_mask) << ", "
             << route.m_metric << ")" << std::endl;
        }
    }
  std::hash<std::string> hasher;
  return hasher (ss.str ());
}

void
OspfApp::PrintLsdbHash ()
{
  std::cout << GetLsdbHash () << std::endl;
}

void
OspfApp::PrintAreaLsdbHash ()
{
  std::cout << GetAreaLsdbHash () << std::endl;
}

void
OspfApp::AddNeighbor (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  Ptr<OspfInterface> ospfInterface = m_ospfInterfaces[ifIndex];
  ospfInterface->AddNeighbor (neighbor);
}

void
OspfApp::InjectLsa (std::vector<std::pair<LsaHeader, Ptr<Lsa>>> lsaList)
{
  for (auto &[header, lsa] : lsaList)
    {
      ProcessLsa (header.Copy (), lsa->Copy ());
    }
}

} // namespace ns3
