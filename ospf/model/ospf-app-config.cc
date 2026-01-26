/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

#include "ns3/channel.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-address.h"

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

bool
OspfApp::SelectPrimaryInterfaceAddress (Ptr<Ipv4> ipv4, uint32_t ifIndex, Ipv4InterfaceAddress &out)
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

void
OspfApp::HandleInterfaceDown (uint32_t ifIndex)
{
  if (ifIndex >= m_ospfInterfaces.size () || m_ospfInterfaces[ifIndex] == nullptr)
    {
      return;
    }

  auto interface = m_ospfInterfaces[ifIndex];
  auto neighbors = interface->GetNeighbors ();
  for (auto n : neighbors)
    {
      FallbackToDown (ifIndex, n);
    }
  interface->ClearNeighbors ();
}

bool
OspfApp::SyncInterfacesFromIpv4 ()
{
  if (!m_autoSyncInterfaces)
    {
      return false;
    }

  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
  if (ipv4 == nullptr)
    {
      NS_LOG_WARN ("AutoSyncInterfaces enabled but node has no Ipv4 object");
      return false;
    }

  const uint32_t nIf = ipv4->GetNInterfaces ();
  if (nIf == 0)
    {
      return false;
    }

  bool changed = false;

  NetDeviceContainer newDevices;
  for (uint32_t i = 0; i < nIf; ++i)
    {
      newDevices.Add (ipv4->GetNetDevice (i));
    }

  if (m_boundDevices.GetN () != newDevices.GetN ())
    {
      changed = true;
    }
  else
    {
      for (uint32_t i = 0; i < newDevices.GetN (); ++i)
        {
          if (m_boundDevices.Get (i) != newDevices.Get (i))
            {
              changed = true;
              break;
            }
        }
    }
  m_boundDevices = newDevices;

  if (m_lastHelloReceived.size () != nIf)
    {
      m_lastHelloReceived.resize (nIf);
      changed = true;
    }
  if (m_helloTimeouts.size () != nIf)
    {
      m_helloTimeouts.resize (nIf);
      changed = true;
    }

  if (m_ospfInterfaces.size () != nIf)
    {
      m_ospfInterfaces.resize (nIf);
      changed = true;
    }

  if (m_ospfInterfaces[0] == nullptr)
    {
      m_ospfInterfaces[0] = Create<OspfInterface> ();
      changed = true;
    }

  for (uint32_t i = 1; i < nIf; ++i)
    {
      if (m_ospfInterfaces[i] == nullptr)
        {
          m_ospfInterfaces[i] = Create<OspfInterface> ();
          changed = true;
        }

      Ipv4InterfaceAddress ifAddr;
      const bool hasAddr = SelectPrimaryInterfaceAddress (ipv4, i, ifAddr);
      const auto ip = hasAddr ? ifAddr.GetAddress () : Ipv4Address::GetAny ();
      const auto mask = hasAddr ? ifAddr.GetMask () : Ipv4Mask (0xffffffff);
      const bool isUp = ipv4->IsUp (i) && hasAddr;

      auto ospfIf = m_ospfInterfaces[i];
      if (ospfIf->GetAddress () != ip)
        {
          ospfIf->SetAddress (ip);
          changed = true;
        }
      if (ospfIf->GetMask () != mask)
        {
          ospfIf->SetMask (mask);
          changed = true;
        }

      const bool wasUp = ospfIf->IsUp ();
      if (wasUp && !isUp)
        {
          HandleInterfaceDown (i);
          changed = true;
        }
      if (wasUp != isUp)
        {
          ospfIf->SetUp (isUp);
          changed = true;
        }

      // Keep common parameters aligned with app attributes.
      ospfIf->SetHelloInterval (m_helloInterval.GetMilliSeconds ());
      ospfIf->SetRouterDeadInterval (m_routerDeadInterval.GetMilliSeconds ());
      ospfIf->SetArea (m_areaId);
      ospfIf->SetMetric (1);
      if (m_boundDevices.Get (i) != nullptr)
        {
          ospfIf->SetMtu (m_boundDevices.Get (i)->GetMtu ());
        }

      // Gateway selection (only meaningful for point-to-point).
      Ipv4Address gw = Ipv4Address::GetBroadcast ();
      if (m_boundDevices.Get (i) != nullptr && m_boundDevices.Get (i)->IsPointToPoint ())
        {
          auto dev = m_boundDevices.Get (i);
          Ptr<Channel> ch = DynamicCast<Channel> (dev->GetChannel ());
          if (ch != nullptr)
            {
              for (uint32_t j = 0; j < ch->GetNDevices (); j++)
                {
                  Ptr<NetDevice> remoteDev = ch->GetDevice (j);
                  if (remoteDev != nullptr && remoteDev != dev)
                    {
                      auto remoteIpv4 = remoteDev->GetNode ()->GetObject<Ipv4> ();
                      Ipv4InterfaceAddress remoteIfAddr;
                      if (SelectPrimaryInterfaceAddress (remoteIpv4, remoteDev->GetIfIndex (),
                                                         remoteIfAddr))
                        {
                          gw = remoteIfAddr.GetAddress ();
                        }
                      break;
                    }
                }
            }
        }
      if (ospfIf->GetGateway () != gw)
        {
          ospfIf->SetGateway (gw);
          changed = true;
        }
    }

  return changed;
}

void
OspfApp::AddReachableAddress (uint32_t ifIndex, Ipv4Address dest, Ipv4Mask mask,
                              Ipv4Address gateway, uint32_t metric)
{
  // Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  // ipv4->AddAddress (0, Ipv4InterfaceAddress (dest, mask));
  // std::cout << "Inject: " << ifIndex << ", " << dest << ", " << mask << ", " << gateway << ", " << metric << std::endl;
  m_externalRoutes.emplace_back (ifIndex, dest.Get (), mask.Get (), gateway.Get (), metric);
  ThrottledRecomputeL1SummaryLsa ();
  // Process the new LSA and generate/flood L2 Summary LSA if needed
  ProcessLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
}

void
OspfApp::AddReachableAddress (uint32_t ifIndex, Ipv4Address address, Ipv4Mask mask)
{
  m_externalRoutes.emplace_back (ifIndex, address.Get (), mask.Get (),
                                 Ipv4Address::GetAny ().Get (), 0);
  ThrottledRecomputeL1SummaryLsa ();
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
      ThrottledRecomputeL1SummaryLsa ();
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
  ThrottledRecomputeL1SummaryLsa ();
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
  if (metrices.size () != m_ospfInterfaces.size ())
    {
      NS_LOG_ERROR ("Ignoring SetMetrices: expected " << m_ospfInterfaces.size () << " entries, got "
                                                      << metrices.size ());
      return;
    }
  for (uint32_t i = 0; i < m_ospfInterfaces.size (); i++)
    {
      m_ospfInterfaces[i]->SetMetric (metrices[i]);
    }
}

uint32_t
OspfApp::GetMetric (uint32_t ifIndex)
{
  if (ifIndex >= m_ospfInterfaces.size () || m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("GetMetric called with invalid ifIndex: " << ifIndex);
      return 0;
    }
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
  if (ifIndex >= m_ospfInterfaces.size () || m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("AddNeighbor ignored due to invalid ifIndex: " << ifIndex);
      return;
    }

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
