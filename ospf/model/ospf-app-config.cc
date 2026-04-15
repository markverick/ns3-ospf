/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

#include "ns3/channel.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-address.h"

namespace ns3 {

void
OspfApp::SetRouting (Ptr<OspfRouting> routing)
{
  m_routing = routing;
}

int32_t
OspfApp::ResolveIpv4InterfaceIndex (Ptr<NetDevice> dev) const
{
  if (dev == nullptr)
    {
      return -1;
    }

  Ptr<Node> node = GetNode ();
  if (node == nullptr)
    {
      return -1;
    }

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4 == nullptr)
    {
      return -1;
    }

  return ipv4->GetInterfaceForDevice (dev);
}

Ipv4Address
OspfApp::SelectAutomaticRouterId () const
{
  Ptr<Node> node = GetNode ();
  if (node == nullptr)
    {
      return Ipv4Address::GetZero ();
    }

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4 == nullptr)
    {
      return Ipv4Address::GetZero ();
    }

  Ipv4InterfaceAddress ifAddr;
  const uint32_t limit = std::min (ipv4->GetNInterfaces (),
                                   static_cast<uint32_t> (m_boundInterfaceSelection.size ()));
  for (uint32_t ifIndex = 1; ifIndex < limit; ++ifIndex)
    {
      if (!m_boundInterfaceSelection[ifIndex])
        {
          continue;
        }

      if (SelectPrimaryInterfaceAddress (ipv4, ifIndex, ifAddr))
        {
          return ifAddr.GetAddress ();
        }
    }

  return Ipv4Address::GetZero ();
}

void
OspfApp::CancelPendingLsaRegeneration (const LsaHeader::LsaKey &lsaKey)
{
  auto pendingIt = m_pendingLsaRegeneration.find (lsaKey);
  if (pendingIt != m_pendingLsaRegeneration.end ())
    {
      if (pendingIt->second.IsRunning ())
        {
          Simulator::Cancel (pendingIt->second);
        }
      m_pendingLsaRegeneration.erase (pendingIt);
    }
  m_lastLsaOriginationTime.erase (lsaKey);
  m_seqNumbers.erase (lsaKey);
}

void
OspfApp::ClearPendingLsaRegenerationState ()
{
  for (auto &[lsaKey, event] : m_pendingLsaRegeneration)
    {
      if (event.IsRunning ())
        {
          Simulator::Cancel (event);
        }
      m_lastLsaOriginationTime.erase (lsaKey);
    }

  m_pendingLsaRegeneration.clear ();
}

void
OspfApp::ClearSelfOriginatedLsaStateForRouterId (Ipv4Address routerId)
{
  if (routerId == Ipv4Address::GetZero ())
    {
      return;
    }

  const auto routerKey =
      std::make_tuple (LsaHeader::LsType::RouterLSAs, routerId.Get (), routerId.Get ());
  const auto l1Key =
      std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, routerId.Get (), routerId.Get ());
  const auto areaKey = std::make_tuple (LsaHeader::LsType::AreaLSAs, m_areaId, routerId.Get ());
  const auto l2Key =
      std::make_tuple (LsaHeader::LsType::L2SummaryLSAs, m_areaId, routerId.Get ());

  CancelPendingLsaRegeneration (routerKey);
  CancelPendingLsaRegeneration (l1Key);
  CancelPendingLsaRegeneration (areaKey);
  CancelPendingLsaRegeneration (l2Key);

  m_routerLsdb.erase (routerId.Get ());
  m_l1SummaryLsdb.erase (routerId.Get ());

  auto areaIt = m_areaLsdb.find (m_areaId);
  if (areaIt != m_areaLsdb.end () && areaIt->second.first.GetAdvertisingRouter () == routerId.Get ())
    {
      m_areaLsdb.erase (areaIt);
    }

  auto l2It = m_l2SummaryLsdb.find (m_areaId);
  if (l2It != m_l2SummaryLsdb.end () && l2It->second.first.GetAdvertisingRouter () == routerId.Get ())
    {
      m_l2SummaryLsdb.erase (l2It);
    }
}

void
OspfApp::UpdateRouterId (Ipv4Address routerId)
{
  if (m_routerId == routerId)
    {
      return;
    }

  ClearSelfOriginatedLsaStateForRouterId (m_routerId);
  m_routerId = routerId;
  RebuildPrefixOwnerTable ();
  UpdateRouting ();
}

void
OspfApp::EnsureInterfacePolicySize (uint32_t nIf)
{
  if (m_boundInterfaceSelection.size () < nIf)
    {
      m_boundInterfaceSelection.resize (nIf, false);
    }
  if (m_advertiseInterfacePrefixes.size () < nIf)
    {
      m_advertiseInterfacePrefixes.resize (nIf, false);
    }
  if (m_interfaceMetrics.size () < nIf)
    {
      m_interfaceMetrics.resize (nIf, 1);
    }
}

uint32_t
OspfApp::GetConfiguredInterfaceMetric (uint32_t ifIndex) const
{
  return ifIndex < m_interfaceMetrics.size () ? m_interfaceMetrics[ifIndex] : 1;
}

Ptr<OspfInterface>
OspfApp::GetOspfInterface (uint32_t ifIndex) const
{
  return ifIndex < m_ospfInterfaces.size () ? m_ospfInterfaces[ifIndex] : nullptr;
}

Ptr<NetDevice>
OspfApp::GetNetDeviceForInterface (uint32_t ifIndex) const
{
  Ptr<Node> node = GetNode ();
  if (node == nullptr)
    {
      return nullptr;
    }

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4 == nullptr || ifIndex >= ipv4->GetNInterfaces ())
    {
      return nullptr;
    }

  return ipv4->GetNetDevice (ifIndex);
}

std::vector<uint32_t>
OspfApp::CollectSelectedInterfaces (Ptr<Ipv4> ipv4, NetDeviceContainer devs) const
{
  std::vector<uint32_t> selectedInterfaces;
  if (ipv4 == nullptr)
    {
      return selectedInterfaces;
    }

  selectedInterfaces.reserve (devs.GetN ());
  for (uint32_t i = 0; i < devs.GetN (); ++i)
    {
      Ptr<NetDevice> dev = devs.Get (i);
      if (dev == nullptr)
        {
          continue;
        }

      int32_t ifIndex = ResolveIpv4InterfaceIndex (dev);
      NS_ABORT_MSG_IF (ifIndex < 0,
                       "Selected OSPF device is not registered with the node IPv4 stack");
      selectedInterfaces.push_back (static_cast<uint32_t> (ifIndex));
    }

  return selectedInterfaces;
}

void
OspfApp::ApplyBoundInterfaceSelection (Ptr<Ipv4> ipv4,
                                       const std::vector<uint32_t> &selectedInterfaces)
{
  const uint32_t nIf = ipv4->GetNInterfaces ();
  EnsureInterfacePolicySize (nIf);
  m_boundInterfaceSelection.assign (nIf, false);
  for (uint32_t ifIndex : selectedInterfaces)
    {
      if (ifIndex < m_boundInterfaceSelection.size ())
        {
          m_boundInterfaceSelection[ifIndex] = ifIndex != 0;
        }
    }

  m_lastHelloReceived.assign (nIf, Time ());
  m_helloTimeouts.clear ();
  m_helloTimeouts.resize (nIf);
}

void
OspfApp::RestoreAdvertisedInterfacePrefixes (
    const std::vector<uint32_t> &selectedInterfaces,
    const std::vector<bool> &previousAdvertiseInterfacePrefixes)
{
  EnsureInterfacePolicySize (m_boundInterfaceSelection.size ());
  m_advertiseInterfacePrefixes.assign (m_boundInterfaceSelection.size (), false);
  for (uint32_t ifIndex : selectedInterfaces)
    {
      if (ifIndex < previousAdvertiseInterfacePrefixes.size () &&
          previousAdvertiseInterfacePrefixes[ifIndex])
        {
          m_advertiseInterfacePrefixes[ifIndex] = true;
        }
    }
}

Ipv4Address
OspfApp::ResolvePointToPointGateway (Ptr<NetDevice> dev) const
{
  if (dev == nullptr || !dev->IsPointToPoint ())
    {
      return Ipv4Address::GetBroadcast ();
    }

  auto ch = DynamicCast<Channel> (dev->GetChannel ());
  if (ch == nullptr)
    {
      return Ipv4Address::GetZero ();
    }

  for (uint32_t j = 0; j < ch->GetNDevices (); ++j)
    {
      Ptr<NetDevice> remoteDev = ch->GetDevice (j);
      if (remoteDev == nullptr || remoteDev == dev)
        {
          continue;
        }

      auto remoteIpv4 = remoteDev->GetNode ()->GetObject<Ipv4> ();
      Ipv4InterfaceAddress remoteIfAddr;
      const int32_t remoteIfIndex =
          remoteIpv4 != nullptr ? remoteIpv4->GetInterfaceForDevice (remoteDev) : -1;
      if (remoteIfIndex >= 0 &&
          SelectPrimaryInterfaceAddress (remoteIpv4, static_cast<uint32_t> (remoteIfIndex),
                                         remoteIfAddr))
        {
          return remoteIfAddr.GetAddress ();
        }
      break;
    }

  return Ipv4Address::GetZero ();
}

void
OspfApp::RebuildSelectedOspfInterfaces (Ptr<Ipv4> ipv4)
{
  const uint32_t nIf = ipv4->GetNInterfaces ();
  EnsureInterfacePolicySize (nIf);
  m_ospfInterfaces.clear ();
  m_ospfInterfaces.resize (nIf);
  m_ospfInterfaces[0] = Create<OspfInterface> ();

  for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
    {
      if (!HasOspfInterface (ifIndex))
        {
          continue;
        }

      Ipv4InterfaceAddress ifAddr;
      const bool hasAddr = SelectPrimaryInterfaceAddress (ipv4, ifIndex, ifAddr);
      const auto sourceIp = hasAddr ? ifAddr.GetAddress () : Ipv4Address::GetAny ();
      const auto mask = hasAddr ? ifAddr.GetMask () : Ipv4Mask (0xffffffff);
      Ptr<NetDevice> dev = GetNetDeviceForInterface (ifIndex);
      NS_ABORT_MSG_IF (dev == nullptr,
                       "Selected OSPF IPv4 interface has no backing net device");
      Ptr<OspfInterface> ospfInterface = Create<OspfInterface> (
          sourceIp, mask, m_helloInterval.GetMilliSeconds (), m_routerDeadInterval.GetMilliSeconds (),
          m_areaId, GetConfiguredInterfaceMetric (ifIndex), dev->GetMtu ());
      ospfInterface->SetGateway (ResolvePointToPointGateway (dev));
      m_ospfInterfaces[ifIndex] = ospfInterface;
    }
}

void
OspfApp::StopProtocolForInterfaceRebind ()
{
  m_helloEvent.Remove ();
  CancelHelloTimeouts ();
  CloseSockets ();

  for (uint32_t ifIndex = 1; ifIndex < m_ospfInterfaces.size (); ++ifIndex)
    {
      if (GetOspfInterface (ifIndex) != nullptr)
        {
          HandleInterfaceDown (ifIndex);
        }
    }
}

void
OspfApp::RestartProtocolAfterInterfaceRebind ()
{
  RefreshInterfaceReachableRoutesFromIpv4 ();
  InitializeSockets ();
  ScheduleTransmitHello (MilliSeconds (0));
  ThrottledRecomputeRouterLsa ();
  ThrottledRecomputeL1SummaryLsa ();
}

void
OspfApp::SetBoundNetDevices (NetDeviceContainer devs)
{
  NS_LOG_FUNCTION (this << devs.GetN ());
  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
  NS_ABORT_MSG_IF (ipv4 == nullptr, "SetBoundNetDevices requires an Ipv4 object on the node");

  const bool wasRunning = m_protocolRunning;
  const auto previousAdvertiseInterfacePrefixes = m_advertiseInterfacePrefixes;

  if (wasRunning)
    {
      StopProtocolForInterfaceRebind ();
    }

  const auto selectedInterfaces = CollectSelectedInterfaces (ipv4, devs);
  ApplyBoundInterfaceSelection (ipv4, selectedInterfaces);
  RestoreAdvertisedInterfacePrefixes (selectedInterfaces, previousAdvertiseInterfacePrefixes);
  RebuildSelectedOspfInterfaces (ipv4);

  if (!m_manualRouterId)
    {
      UpdateRouterId (SelectAutomaticRouterId ());
    }

  if (wasRunning)
    {
      RestartProtocolAfterInterfaceRebind ();
    }
}

bool
OspfApp::HasOspfInterface (uint32_t ifIndex) const
{
  return ifIndex > 0 && ifIndex < m_boundInterfaceSelection.size () &&
         m_boundInterfaceSelection[ifIndex];
}

void
OspfApp::SetInterfacePrefixRoutable (uint32_t ifIndex, bool enabled)
{
  if (enabled && !HasOspfInterface (ifIndex))
    {
      NS_LOG_WARN ("Ignoring SetInterfacePrefixRoutable on unselected ifIndex: " << ifIndex);
      if (ifIndex < m_advertiseInterfacePrefixes.size ())
        {
          m_advertiseInterfacePrefixes[ifIndex] = false;
        }
      return;
    }

  if (ifIndex >= m_advertiseInterfacePrefixes.size ())
    {
      EnsureInterfacePolicySize (ifIndex + 1);
    }

  if (m_advertiseInterfacePrefixes[ifIndex] == enabled)
    {
      return;
    }

  m_advertiseInterfacePrefixes[ifIndex] = enabled;
  if (enabled)
    {
      Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
      RefreshOspfInterfaceStateFromIpv4 (ipv4, ifIndex);
    }
  if (m_protocolRunning)
    {
      RefreshReachableRoutesAndAdvertise ();
    }
}

bool
OspfApp::GetInterfacePrefixRoutable (uint32_t ifIndex) const
{
  return ifIndex < m_advertiseInterfacePrefixes.size () && m_advertiseInterfacePrefixes[ifIndex];
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

OspfApp::ReachableRouteList
OspfApp::CollectInterfaceReachableRoutesFromIpv4 (Ptr<Ipv4> ipv4) const
{
  ReachableRouteList interfaceRoutes;
  if (ipv4 == nullptr)
    {
      return interfaceRoutes;
    }

  const uint32_t nIf = ipv4->GetNInterfaces ();
  for (uint32_t ifIndex = 1; ifIndex < nIf; ++ifIndex)
    {
      if (!HasOspfInterface (ifIndex))
        {
          continue;
        }

      if (!GetInterfacePrefixRoutable (ifIndex))
        {
          continue;
        }

      Ipv4InterfaceAddress ifAddr;
      if (!SelectPrimaryInterfaceAddress (ipv4, ifIndex, ifAddr))
        {
          continue;
        }

      const auto addr = ifAddr.GetAddress ();
      const auto mask = ifAddr.GetMask ();
      const auto dest = addr.CombineMask (mask);
      interfaceRoutes.emplace_back (ifIndex, dest.Get (), mask.Get (), addr.Get (), 1);
    }

  return interfaceRoutes;
}

bool
OspfApp::RefreshOspfInterfaceStateFromIpv4 (Ptr<Ipv4> ipv4, uint32_t ifIndex)
{
  if (ipv4 == nullptr || ifIndex >= ipv4->GetNInterfaces ())
    {
      return false;
    }

  if (m_lastHelloReceived.size () <= ifIndex)
    {
      m_lastHelloReceived.resize (ifIndex + 1);
    }
  if (m_helloTimeouts.size () <= ifIndex)
    {
      m_helloTimeouts.resize (ifIndex + 1);
    }
  if (m_ospfInterfaces.size () <= ifIndex)
    {
      m_ospfInterfaces.resize (ifIndex + 1);
    }
  EnsureInterfacePolicySize (ifIndex + 1);

  bool changed = false;
  if (m_ospfInterfaces[ifIndex] == nullptr)
    {
      m_ospfInterfaces[ifIndex] = Create<OspfInterface> ();
      changed = true;
    }

  Ipv4InterfaceAddress ifAddr;
  const bool hasAddr = SelectPrimaryInterfaceAddress (ipv4, ifIndex, ifAddr);
  const auto ip = hasAddr ? ifAddr.GetAddress () : Ipv4Address::GetAny ();
  const auto mask = hasAddr ? ifAddr.GetMask () : Ipv4Mask (0xffffffff);
  const bool isUp = ipv4->IsUp (ifIndex) && hasAddr;

  auto ospfIf = m_ospfInterfaces[ifIndex];
  const bool identityChanged = ospfIf->GetAddress () != ip || ospfIf->GetMask () != mask;
  const bool wasUp = ospfIf->IsUp ();

  if (wasUp && (identityChanged || !isUp))
    {
      HandleInterfaceDown (ifIndex);
      ospfIf->SetUp (false);
      changed = true;
    }

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
  if (wasUp != isUp)
    {
      ospfIf->SetUp (isUp);
      changed = true;
    }
  else if (ospfIf->IsUp () != isUp)
    {
      ospfIf->SetUp (isUp);
      changed = true;
    }

  ospfIf->SetHelloInterval (m_helloInterval.GetMilliSeconds ());
  ospfIf->SetRouterDeadInterval (m_routerDeadInterval.GetMilliSeconds ());
  ospfIf->SetArea (m_areaId);
  ospfIf->SetMetric (GetConfiguredInterfaceMetric (ifIndex));

  Ptr<NetDevice> dev = ipv4->GetNetDevice (ifIndex);
  if (dev != nullptr)
    {
      ospfIf->SetMtu (dev->GetMtu ());
    }

  Ipv4Address gw = Ipv4Address::GetBroadcast ();
  if (dev != nullptr && dev->IsPointToPoint ())
    {
      gw = ResolvePointToPointGateway (dev);
    }
  if (ospfIf->GetGateway () != gw)
    {
      ospfIf->SetGateway (gw);
      changed = true;
    }

  return changed;
}

void
OspfApp::InitializeSplitReachableRoutesFromCurrentState ()
{
  if ((!m_interfaceExternalRoutes.empty () || !m_injectedExternalRoutes.empty ()) ||
      m_externalRoutes.empty ())
    {
      return;
    }

  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
  ReachableRouteList currentInterfaceRoutes = CollectInterfaceReachableRoutesFromIpv4 (ipv4);

  m_interfaceExternalRoutes = currentInterfaceRoutes;

  for (const auto &route : m_externalRoutes)
    {
      if (std::find (m_interfaceExternalRoutes.begin (), m_interfaceExternalRoutes.end (), route) ==
          m_interfaceExternalRoutes.end ())
        {
          m_injectedExternalRoutes.emplace_back (route);
        }
    }
}

bool
OspfApp::ApplyAdvertisedReachableRoutes ()
{
  ReachableRouteList advertisedRoutes = m_interfaceExternalRoutes;
  advertisedRoutes.insert (advertisedRoutes.end (), m_injectedExternalRoutes.begin (),
                           m_injectedExternalRoutes.end ());

  if (m_externalRoutes == advertisedRoutes)
    {
      return false;
    }

  m_externalRoutes = std::move (advertisedRoutes);
  UpdateRouting ();
  return true;
}

void
OspfApp::RefreshInterfaceReachableRoutesFromIpv4 ()
{
  InitializeSplitReachableRoutesFromCurrentState ();

  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
  ReachableRouteList interfaceRoutes = CollectInterfaceReachableRoutesFromIpv4 (ipv4);

  if (m_interfaceExternalRoutes != interfaceRoutes)
    {
      m_interfaceExternalRoutes = std::move (interfaceRoutes);
    }

  ApplyAdvertisedReachableRoutes ();
}

void
OspfApp::RefreshReachableRoutesAndAdvertise ()
{
  RefreshInterfaceReachableRoutesFromIpv4 ();
  ThrottledRecomputeL1SummaryLsa ();
}

void
OspfApp::HandleInterfaceDown (uint32_t ifIndex)
{
  auto interface = GetOspfInterface (ifIndex);
  if (interface == nullptr)
    {
      return;
    }
  auto neighbors = interface->GetNeighbors ();
  for (auto n : neighbors)
    {
      FallbackToDown (ifIndex, n);
    }
  interface->ClearNeighbors ();
}

void
OspfApp::ReplaceNeighbors (const std::vector<std::vector<Ptr<OspfNeighbor>>> &neighbors)
{
  CancelHelloTimeouts ();

  for (uint32_t i = 1; i < m_ospfInterfaces.size (); ++i)
    {
      if (auto ospfIf = GetOspfInterface (i))
        {
          for (auto &neighbor : ospfIf->GetNeighbors ())
            {
              neighbor->RemoveTimeout ();
              neighbor->ClearKeyedTimeouts ();
            }
          ospfIf->ClearNeighbors ();
        }

      for (auto &neighbor : neighbors[i])
        {
          if (auto ospfIf = GetOspfInterface (i))
            {
              neighbor->RefreshLastHelloReceived ();
              RefreshHelloTimeout (i, neighbor);
              ospfIf->AddNeighbor (neighbor);
            }
        }
    }
}

void
OspfApp::HandleLocalInterfaceEvent ()
{
  const bool changed = RefreshAllOspfInterfaceStateFromIpv4 ();
  if (!changed || !m_protocolRunning)
    {
      return;
    }

  CancelHelloTimeouts ();
  CloseSockets ();
  InitializeSockets ();

  m_helloEvent.Remove ();
  ScheduleTransmitHello (MilliSeconds (0));

  ThrottledRecomputeRouterLsa ();
  ThrottledRecomputeL1SummaryLsa ();
}

bool
OspfApp::RefreshAllOspfInterfaceStateFromIpv4 ()
{
  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
  if (ipv4 == nullptr)
    {
      NS_LOG_WARN ("RefreshAllOspfInterfaceStateFromIpv4 called but node has no Ipv4 object");
      return false;
    }

  const uint32_t nIf = ipv4->GetNInterfaces ();
  if (nIf == 0)
    {
      return false;
    }

  bool changed = false;
  EnsureInterfacePolicySize (nIf);

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
      if (!HasOspfInterface (i))
        {
          if (m_ospfInterfaces[i] != nullptr)
            {
              HandleInterfaceDown (i);
              m_ospfInterfaces[i] = nullptr;
              changed = true;
            }
          continue;
        }

      if (RefreshOspfInterfaceStateFromIpv4 (ipv4, i))
        {
          changed = true;
        }
    }

  if (!m_manualRouterId)
    {
      const auto selectedRouterId = SelectAutomaticRouterId ();
      if (m_routerId != selectedRouterId)
        {
          UpdateRouterId (selectedRouterId);
          changed = true;
        }
    }

  const auto oldExternalRoutes = m_externalRoutes;
  RefreshInterfaceReachableRoutesFromIpv4 ();
  if (m_externalRoutes != oldExternalRoutes)
    {
      changed = true;
    }

  return changed;
}

void
OspfApp::AddReachableAddress (uint32_t ifIndex, Ipv4Address dest, Ipv4Mask mask,
                              Ipv4Address gateway, uint32_t metric)
{
  InitializeSplitReachableRoutesFromCurrentState ();
  m_injectedExternalRoutes.emplace_back (ifIndex, dest.Get (), mask.Get (), gateway.Get (), metric);
  RefreshReachableRoutesAndAdvertise ();
}

void
OspfApp::AddReachableAddress (uint32_t ifIndex, Ipv4Address address, Ipv4Mask mask)
{
  InitializeSplitReachableRoutesFromCurrentState ();
  m_injectedExternalRoutes.emplace_back (ifIndex, address.Get (), mask.Get (),
                                         Ipv4Address::GetAny ().Get (), 0);
  RefreshReachableRoutesAndAdvertise ();
}

bool
OspfApp::SetReachableAddresses (ReachableRouteList reachableAddresses)
{
  InitializeSplitReachableRoutesFromCurrentState ();
  if (m_injectedExternalRoutes != reachableAddresses)
    {
      m_injectedExternalRoutes = std::move (reachableAddresses);
      ApplyAdvertisedReachableRoutes ();
      ThrottledRecomputeL1SummaryLsa ();
      return true;
    }
  return false;
}

bool
OspfApp::SetInterfaceReachableAddresses (ReachableRouteList reachableAddresses)
{
  InitializeSplitReachableRoutesFromCurrentState ();
  if (m_interfaceExternalRoutes != reachableAddresses)
    {
      m_interfaceExternalRoutes = std::move (reachableAddresses);
      ApplyAdvertisedReachableRoutes ();
      ThrottledRecomputeL1SummaryLsa ();
      return true;
    }
  return false;
}

void
OspfApp::AddAllReachableAddresses (uint32_t ifIndex)
{
  InitializeSplitReachableRoutesFromCurrentState ();
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      auto ospfIf = GetOspfInterface (i);
      if (ospfIf == nullptr)
        {
          continue;
        }
      if (ifIndex == i)
        continue;
      m_injectedExternalRoutes.emplace_back (
          ifIndex, ospfIf->GetAddress ().CombineMask (ospfIf->GetMask ()).Get (),
          ospfIf->GetMask ().Get (), ospfIf->GetAddress ().Get (), 0);
    }
  RefreshReachableRoutesAndAdvertise ();
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
  InitializeSplitReachableRoutesFromCurrentState ();

  const auto exact = address.Get ();
  const auto network = address.CombineMask (mask).Get ();
  const auto maskValue = mask.Get ();

  auto newEnd = std::remove_if (m_injectedExternalRoutes.begin (), m_injectedExternalRoutes.end (),
                                [ifIndex, exact, network, maskValue] (const auto &route) {
                                  const auto routeIfIndex = std::get<0> (route);
                                  const auto routeDest = std::get<1> (route);
                                  const auto routeMask = std::get<2> (route);
                                  return routeIfIndex == ifIndex && routeMask == maskValue &&
                                         (routeDest == exact || routeDest == network);
                                });

  if (newEnd == m_injectedExternalRoutes.end ())
    {
      return;
    }

  m_injectedExternalRoutes.erase (newEnd, m_injectedExternalRoutes.end ());
  RefreshReachableRoutesAndAdvertise ();
}

void
OspfApp::SetArea (uint32_t area)
{
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      if (auto ospfIf = GetOspfInterface (i))
        {
          ospfIf->SetArea (area);
        }
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

bool
OspfApp::IsAreaLeader () const
{
  return m_isAreaLeader;
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
  Ptr<Ipv4> ipv4 = GetNode () != nullptr ? GetNode ()->GetObject<Ipv4> () : nullptr;
  const uint32_t nIf = ipv4 != nullptr ? ipv4->GetNInterfaces () : m_ospfInterfaces.size ();
  if (metrices.size () != nIf)
    {
      NS_LOG_ERROR ("Ignoring SetMetrices: expected " << nIf << " entries, got "
                                                      << metrices.size ());
      return;
    }

  EnsureInterfacePolicySize (nIf);
  m_interfaceMetrics = std::move (metrices);

  for (uint32_t i = 0; i < m_ospfInterfaces.size (); i++)
    {
      if (auto ospfIf = GetOspfInterface (i))
        {
          ospfIf->SetMetric (GetConfiguredInterfaceMetric (i));
        }
    }
}

uint32_t
OspfApp::GetMetric (uint32_t ifIndex)
{
  auto ospfIf = GetOspfInterface (ifIndex);
  if (ospfIf == nullptr)
    {
      if (ifIndex < m_interfaceMetrics.size ())
        {
          return m_interfaceMetrics[ifIndex];
        }
      NS_LOG_WARN ("GetMetric called with invalid ifIndex: " << ifIndex);
      return 0;
    }
  return ospfIf->GetMetric ();
}

void
OspfApp::SetRouterId (Ipv4Address routerId)
{
  UpdateRouterId (routerId);
  m_manualRouterId = true;
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
      if (auto ospfIf = GetOspfInterface (i))
        {
          std::cout << " " << ospfIf->GetArea ();
        }
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
  Ptr<OspfInterface> ospfInterface = GetOspfInterface (ifIndex);
  if (ospfInterface == nullptr)
    {
      NS_LOG_WARN ("AddNeighbor ignored due to invalid ifIndex: " << ifIndex);
      return;
    }
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
