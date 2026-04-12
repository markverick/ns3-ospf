/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {

namespace {

bool
IsUsableIpv4Interface (Ptr<Ipv4> ipv4, uint32_t ifIndex)
{
  return ipv4 != nullptr && ifIndex < ipv4->GetNInterfaces () && ipv4->IsUp (ifIndex) &&
         ipv4->GetNetDevice (ifIndex) != nullptr;
}

} // namespace

std::size_t
OspfApp::OwnerRefHash::operator() (const OwnerRef &owner) const
{
  const auto kindHash = std::hash<uint8_t> () (static_cast<uint8_t> (owner.kind));
  const auto idHash = std::hash<uint64_t> () (owner.id);
  return kindHash ^ (idHash + 0x9e3779b9u + (kindHash << 6) + (kindHash >> 2));
}

OspfApp::OwnerRef
OspfApp::MakeOwnerRef (OwnerKind kind, uint64_t id)
{
  return OwnerRef{kind, id};
}

uint64_t
OspfApp::MakeGatewayRouteOwnerId (uint32_t ifIndex, uint32_t gateway)
{
  return (static_cast<uint64_t> (ifIndex) << 32) | static_cast<uint64_t> (gateway);
}

uint32_t
OspfApp::PrefixLengthToMaskValue (uint8_t prefixLength)
{
  if (prefixLength == 0)
    {
      return 0;
    }
  if (prefixLength >= 32)
    {
      return 0xffffffffu;
    }
  return 0xffffffffu << (32 - prefixLength);
}

uint32_t
OspfApp::MaskNetwork (uint32_t address, uint8_t prefixLength)
{
  return address & PrefixLengthToMaskValue (prefixLength);
}

bool
OspfApp::IsOwnerPreferred (const OwnerRef &candidate, const OwnerRef &current)
{
  if (candidate.kind != current.kind)
    {
      return static_cast<uint8_t> (candidate.kind) < static_cast<uint8_t> (current.kind);
    }
  return candidate.id < current.id;
}

void
OspfApp::RemoveOwnedPrefixes (const OwnerRef &owner)
{
  auto ownerIt = m_ownerToPrefixes.find (owner);
  if (ownerIt == m_ownerToPrefixes.end ())
    {
      return;
    }

  for (const auto &prefix : ownerIt->second)
    {
      auto &bucket = m_prefixOwnersByLength[prefix.prefixLength];
      auto entryIt = bucket.find (prefix.network);
      if (entryIt == bucket.end ())
        {
          continue;
        }

      entryIt->second.candidates.erase (owner);
      if (entryIt->second.candidates.empty ())
        {
          bucket.erase (entryIt);
        }
    }

  m_ownerToPrefixes.erase (owner);
}

void
OspfApp::ReplaceOwnedPrefixes (const OwnerRef &owner, const std::set<SummaryRoute> &routes)
{
  RemoveOwnedPrefixes (owner);

  if (routes.empty ())
    {
      return;
    }

  auto &ownedPrefixes = m_ownerToPrefixes[owner];
  ownedPrefixes.reserve (routes.size ());
  PrefixKey lastPrefix;
  bool hasLastPrefix = false;

  for (const auto &route : routes)
    {
      const auto mask = Ipv4Mask (route.m_mask);
      const auto prefixLength = static_cast<uint8_t> (mask.GetPrefixLength ());
      const auto network = Ipv4Address (route.m_address).CombineMask (mask).Get ();
      const PrefixKey prefix{prefixLength, network};
      auto &entry = m_prefixOwnersByLength[prefixLength][network];
      auto metricIt = entry.candidates.find (owner);
      if (metricIt == entry.candidates.end ())
        {
          entry.candidates.emplace (owner, route.m_metric);
        }
      else
        {
          metricIt->second = std::min (metricIt->second, route.m_metric);
        }

      if (!hasLastPrefix || !(lastPrefix == prefix))
        {
          ownedPrefixes.push_back (prefix);
          lastPrefix = prefix;
          hasLastPrefix = true;
        }
    }
}

void
OspfApp::RebuildLocalInterfacePrefixOwners ()
{
  std::vector<OwnerRef> localOwners;
  localOwners.reserve (m_ownerToPrefixes.size ());
  for (const auto &[owner, prefixes] : m_ownerToPrefixes)
    {
      (void)prefixes;
      if (owner.kind == OwnerKind::Interface || owner.kind == OwnerKind::GatewayRoute)
        {
          localOwners.push_back (owner);
        }
    }

  for (const auto &owner : localOwners)
    {
      RemoveOwnedPrefixes (owner);
    }

  std::unordered_map<uint32_t, std::set<SummaryRoute>> routesByInterface;
  std::unordered_map<uint64_t, std::set<SummaryRoute>> routesByGateway;
  for (const auto &[ifIndex, dest, mask, addr, metric] : m_interfaceExternalRoutes)
    {
      (void)addr;
      routesByInterface[ifIndex].insert (SummaryRoute (dest, mask, metric));
    }

  for (const auto &[ifIndex, dest, mask, addr, metric] : m_injectedExternalRoutes)
    {
      const SummaryRoute route (dest, mask, metric);
      if (addr == Ipv4Address::GetAny ().Get () || addr == Ipv4Address::GetZero ().Get ())
        {
          routesByInterface[ifIndex].insert (route);
          continue;
        }

      routesByGateway[MakeGatewayRouteOwnerId (ifIndex, addr)].insert (route);
    }

  for (const auto &[ifIndex, routes] : routesByInterface)
    {
      ReplaceOwnedPrefixes (MakeOwnerRef (OwnerKind::Interface, ifIndex), routes);
    }

  for (const auto &[ownerId, routes] : routesByGateway)
    {
      ReplaceOwnedPrefixes (MakeOwnerRef (OwnerKind::GatewayRoute, ownerId), routes);
    }
}

void
OspfApp::RebuildPrefixOwnerTable ()
{
  for (auto &bucket : m_prefixOwnersByLength)
    {
      bucket.clear ();
    }
  m_ownerToPrefixes.clear ();

  RebuildLocalInterfacePrefixOwners ();

  for (const auto &[routerId, lsa] : m_l1SummaryLsdb)
    {
      const auto ownerId = lsa.first.GetAdvertisingRouter ();
      if (ownerId == m_routerId.Get ())
        {
          continue;
        }
      ReplaceOwnedPrefixes (MakeOwnerRef (OwnerKind::Router, ownerId), lsa.second->GetRoutes ());
    }

  for (const auto &[areaId, lsa] : m_l2SummaryLsdb)
    {
      if (areaId == m_areaId)
        {
          continue;
        }
      ReplaceOwnedPrefixes (MakeOwnerRef (OwnerKind::Area, areaId), lsa.second->GetRoutes ());
    }
}

void
OspfApp::RebuildOwnerResolutionTable ()
{
  m_ownerResolutionTable.clear ();

  Ptr<Ipv4> ipv4 = GetNode () != nullptr ? GetNode ()->GetObject<Ipv4> () : nullptr;
  for (const auto &[ifIndex, dest, mask, addr, metric] : m_interfaceExternalRoutes)
    {
      (void)dest;
      (void)mask;
      (void)addr;
      (void)metric;
      if (!IsUsableIpv4Interface (ipv4, ifIndex))
        {
          continue;
        }

      m_ownerResolutionTable[MakeOwnerRef (OwnerKind::Interface, ifIndex)] =
          ResolvedOwner{ifIndex, Ipv4Address::GetZero (), 0};
    }

  for (const auto &[ifIndex, dest, mask, addr, metric] : m_injectedExternalRoutes)
    {
      (void)dest;
      (void)mask;
      (void)metric;
      if (!IsUsableIpv4Interface (ipv4, ifIndex))
        {
          continue;
        }

      if (addr == Ipv4Address::GetAny ().Get () || addr == Ipv4Address::GetZero ().Get ())
        {
          m_ownerResolutionTable[MakeOwnerRef (OwnerKind::Interface, ifIndex)] =
              ResolvedOwner{ifIndex, Ipv4Address::GetZero (), 0};
          continue;
        }

      m_ownerResolutionTable[MakeOwnerRef (OwnerKind::GatewayRoute,
                                           MakeGatewayRouteOwnerId (ifIndex, addr))] =
          ResolvedOwner{ifIndex, Ipv4Address (addr), 0};
    }

  for (const auto &[remoteRouterId, nextHop] : m_l1NextHop)
    {
      m_ownerResolutionTable[MakeOwnerRef (OwnerKind::Router, remoteRouterId)] =
          ResolvedOwner{nextHop.ifIndex, nextHop.ipAddress, nextHop.metric};
    }

  for (const auto &[remoteAreaId, l2NextHop] : m_l2NextHop)
    {
      if (remoteAreaId == m_areaId)
        {
          continue;
        }

      auto borderIt = m_nextHopToShortestBorderRouter.find (l2NextHop.first);
      if (borderIt == m_nextHopToShortestBorderRouter.end ())
        {
          continue;
        }

      const auto &nextHop = borderIt->second.second;
      m_ownerResolutionTable[MakeOwnerRef (OwnerKind::Area, remoteAreaId)] = ResolvedOwner{
          nextHop.ifIndex,
          nextHop.ipAddress,
          nextHop.metric + l2NextHop.second};
    }
}

bool
OspfApp::TryResolveOwner (const OwnerRef &owner, ResolvedOwner &resolved) const
{
  auto it = m_ownerResolutionTable.find (owner);
  if (it == m_ownerResolutionTable.end ())
    {
      return false;
    }

  resolved = it->second;
  return true;
}

bool
OspfApp::LookupForwardingEntry (Ipv4Address destination, int32_t requiredIfIndex,
                                ForwardingEntry &entry) const
{
  Ptr<Ipv4> ipv4 = GetNode () != nullptr ? GetNode ()->GetObject<Ipv4> () : nullptr;

  for (int prefixLength = 32; prefixLength >= 0; --prefixLength)
    {
      const auto network = MaskNetwork (destination.Get (), static_cast<uint8_t> (prefixLength));
      const auto &bucket = m_prefixOwnersByLength[prefixLength];
      auto prefixIt = bucket.find (network);
      if (prefixIt == bucket.end ())
        {
          continue;
        }

      bool found = false;
      OwnerRef bestOwner;
      ResolvedOwner bestResolved;
      uint32_t bestMetric = 0;

      for (const auto &[owner, prefixMetric] : prefixIt->second.candidates)
        {
          ResolvedOwner resolved;
          if (!TryResolveOwner (owner, resolved) || !IsUsableIpv4Interface (ipv4, resolved.ifIndex))
            {
              continue;
            }

          if (requiredIfIndex >= 0 && resolved.ifIndex != static_cast<uint32_t> (requiredIfIndex))
            {
              continue;
            }

          const uint32_t totalMetric = resolved.pathMetric + prefixMetric;
          if (!found || totalMetric < bestMetric ||
              (totalMetric == bestMetric && IsOwnerPreferred (owner, bestOwner)))
            {
              found = true;
              bestOwner = owner;
              bestResolved = resolved;
              bestMetric = totalMetric;
            }
        }

      if (!found)
        {
          continue;
        }

      entry.network = Ipv4Address (network);
      entry.mask = Ipv4Mask (PrefixLengthToMaskValue (static_cast<uint8_t> (prefixLength)));
      entry.nextHop = bestResolved.nextHop;
      entry.ifIndex = bestResolved.ifIndex;
      entry.metric = bestMetric;
      return true;
    }

  return false;
}

void
OspfApp::PrintForwardingTable (std::ostream &os) const
{
  std::vector<std::pair<OwnerRef, ResolvedOwner>> owners (m_ownerResolutionTable.begin (),
                                                           m_ownerResolutionTable.end ());
  std::sort (owners.begin (), owners.end (), [] (const auto &lhs, const auto &rhs) {
    if (lhs.first.kind != rhs.first.kind)
      {
        return static_cast<uint8_t> (lhs.first.kind) < static_cast<uint8_t> (rhs.first.kind);
      }
    return lhs.first.id < rhs.first.id;
  });

  os << "  owner resolution\n";
  for (const auto &[owner, resolved] : owners)
    {
      const char *kind = owner.kind == OwnerKind::Interface
                             ? "if"
               : (owner.kind == OwnerKind::GatewayRoute
                 ? "gw"
                 : (owner.kind == OwnerKind::Router ? "router" : "area"));
      os << "    " << kind << "=" << owner.id << " via if=" << resolved.ifIndex;
      if (!resolved.nextHop.IsAny ())
        {
          os << " next-hop=" << resolved.nextHop;
        }
      os << " metric=" << resolved.pathMetric << "\n";
    }

  struct PrefixLine
  {
    uint8_t prefixLength;
    uint32_t network;
    std::vector<std::pair<OwnerRef, uint32_t>> candidates;
  };

  std::vector<PrefixLine> prefixes;
  for (uint32_t prefixLength = 0; prefixLength < m_prefixOwnersByLength.size (); ++prefixLength)
    {
      for (const auto &[network, entry] : m_prefixOwnersByLength[prefixLength])
        {
          PrefixLine line;
          line.prefixLength = static_cast<uint8_t> (prefixLength);
          line.network = network;
          line.candidates.assign (entry.candidates.begin (), entry.candidates.end ());
          std::sort (line.candidates.begin (), line.candidates.end (), [] (const auto &lhs,
                                                                           const auto &rhs) {
            if (lhs.first.kind != rhs.first.kind)
              {
                return static_cast<uint8_t> (lhs.first.kind) < static_cast<uint8_t> (rhs.first.kind);
              }
            return lhs.first.id < rhs.first.id;
          });
          prefixes.push_back (std::move (line));
        }
    }

  std::sort (prefixes.begin (), prefixes.end (), [] (const PrefixLine &lhs, const PrefixLine &rhs) {
    if (lhs.prefixLength != rhs.prefixLength)
      {
        return lhs.prefixLength > rhs.prefixLength;
      }
    return lhs.network < rhs.network;
  });

  os << "  prefix owners\n";
  for (const auto &line : prefixes)
    {
      os << "    " << Ipv4Address (line.network) << "/" << static_cast<uint32_t> (line.prefixLength);
      for (const auto &[owner, metric] : line.candidates)
        {
          const char *kind = owner.kind == OwnerKind::Interface
                                 ? "if"
                     : (owner.kind == OwnerKind::GatewayRoute
                       ? "gw"
                       : (owner.kind == OwnerKind::Router ? "router" : "area"));
          os << " [" << kind << "=" << owner.id << " metric=" << metric << "]";
        }
      os << "\n";
    }
}

uint64_t
OspfApp::GetL1ShortestPathRunCount () const
{
  return m_l1ShortestPathRunCount;
}

uint64_t
OspfApp::GetL2ShortestPathRunCount () const
{
  return m_l2ShortestPathRunCount;
}

void
OspfApp::UpdateRouting ()
{
  RebuildLocalInterfacePrefixOwners ();
  RebuildOwnerResolutionTable ();
}

void
OspfApp::ScheduleUpdateL1ShortestPath ()
{
  if (m_updateL1ShortestPathTimeout.IsRunning ())
    {
      return;
    }
  m_updateL1ShortestPathTimeout =
      Simulator::Schedule (m_shortestPathUpdateDelay, &OspfApp::UpdateL1ShortestPath, this);
}

void
OspfApp::ScheduleUpdateL2ShortestPath ()
{
  if (m_updateL2ShortestPathTimeout.IsRunning ())
    {
      return;
    }
  m_updateL2ShortestPathTimeout =
      Simulator::Schedule (m_shortestPathUpdateDelay, &OspfApp::UpdateL2ShortestPath, this);
}

} // namespace ns3
