/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-lsa-processor.h"

namespace ns3 {
void
OspfApp::HandleLsr (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    Ptr<LsRequest> lsr)
{
  m_lsa->HandleLsr (ifIndex, ipHeader, ospfHeader, lsr);
}

void
OspfApp::HandleLsu (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsUpdate> lsu)
{
  m_lsa->HandleLsu (ifIndex, ipHeader, ospfHeader, lsu);
}

void
OspfApp::HandleLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    LsaHeader lsaHeader, Ptr<Lsa> lsa)
{
  NS_LOG_FUNCTION (this << ifIndex << Ipv4Address (ospfHeader.GetRouterId ())
                        << ipHeader.GetSource ());
  m_lsa->HandleLsa (ifIndex, ipHeader, ospfHeader, lsaHeader, lsa);
}

void
OspfApp::ProcessLsa (LsaHeader lsaHeader, Ptr<Lsa> lsa)
{
  NS_LOG_FUNCTION (this);
  m_lsa->ProcessLsa (lsaHeader, lsa);
}

void
OspfApp::ProcessLsa (std::pair<LsaHeader, Ptr<Lsa>> lsa)
{
  m_lsa->ProcessLsa (lsa);
}

void
OspfApp::HandleLsAck (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                      Ptr<LsAck> lsAck)
{
  m_lsa->HandleLsAck (ifIndex, ipHeader, ospfHeader, lsAck);
}

void
OspfApp::ProcessL1SummaryLsa (LsaHeader lsaHeader, Ptr<L1SummaryLsa> l1SummaryLsa)
{
  uint32_t lsId = lsaHeader.GetLsId ();

  // Filling in AS External LSDB
  NS_LOG_FUNCTION (this);
  m_l1SummaryLsdb[lsId] = std::make_pair (lsaHeader, l1SummaryLsa);

  if (m_enableAreaProxy)
    {
      // Update local L2 Summary LSDB entry if there's a change in L1 prefixes
      // This will flood L2 LSA if self is the area leader
      if (m_isAreaLeader)
        {
          RecomputeL2SummaryLsa ();
          if (m_enableLog)
            {
              std::string fullname = m_logDir + "/lsa_mapping.csv";
              auto mappingLog = std::ofstream (fullname, std::ios::app);
              auto l1Key = lsaHeader.GetKey ();
              auto l1KeyString = LsaHeader::GetKeyString (lsaHeader.GetSeqNum (), l1Key);
              auto l2Key = m_l2SummaryLsdb[m_areaId].first.GetKey ();
              auto l2KeyString =
                  LsaHeader::GetKeyString (m_l2SummaryLsdb[m_areaId].first.GetSeqNum (), l2Key);
              mappingLog << l1KeyString << "," << l2KeyString << std::endl;
              mappingLog.close ();
            }
        }
    }

  // Update routing table
  UpdateRouting ();
}

void
OspfApp::ProcessRouterLsa (LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa)
{
  uint32_t lsId = lsaHeader.GetLsId ();

  // Filling in Router LSDB
  NS_LOG_FUNCTION (this);
  m_routerLsdb[lsId] = std::make_pair (lsaHeader, routerLsa);
  if (m_enableAreaProxy)
    {
      // Update local Area LSDB entry if there's a change in area links
      // This will flood L2 LSA if self is the area leader
      if (m_isAreaLeader)
        {

          RecomputeAreaLsa ();
          if (m_enableLog)
            {
              std::string fullname = m_logDir + "/lsa_mapping.csv";
              auto mappingLog = std::ofstream (fullname, std::ios::app);
              auto l1Key = lsaHeader.GetKey ();
              auto l1KeyString = LsaHeader::GetKeyString (lsaHeader.GetSeqNum (), l1Key);
              auto l2Key = m_areaLsdb[m_areaId].first.GetKey ();
              auto l2KeyString =
                  LsaHeader::GetKeyString (m_areaLsdb[m_areaId].first.GetSeqNum (), l2Key);
              mappingLog << l1KeyString << "," << l2KeyString << std::endl;
              mappingLog.close ();
            }
        }

      // Start leadership begin timer if it's a leader (lowest router ID)
      if (m_routerLsdb.begin ()->first == m_routerId.Get ())
        {
          if (!m_isAreaLeader && !m_areaLeaderBeginTimer.IsRunning ())
            {
              m_areaLeaderBeginTimer = Simulator::Schedule (
                  m_routerDeadInterval + MilliSeconds (m_jitterRv->GetValue ()),
                  &OspfApp::AreaLeaderBegin, this);
            }
        }
      else
        {
          if (m_areaLeaderBeginTimer.IsRunning ())
            {
              m_areaLeaderBeginTimer.Remove ();
            }
          if (m_isAreaLeader)
            {
              AreaLeaderEnd ();
            }
        }
    }

  // Update routing table
  ScheduleUpdateL1ShortestPath ();
}

void
OspfApp::ProcessAreaLsa (LsaHeader lsaHeader, Ptr<AreaLsa> areaLsa)
{
  if (!m_enableAreaProxy)
    {
      return;
    }
  NS_LOG_FUNCTION (this);
  // LS ID ofType 5 LSA is the originating LSA
  uint32_t lsId = lsaHeader.GetLsId ();

  // If this is the first area-LSA for this area
  if (m_areaLsdb.find (lsId) == m_areaLsdb.end ())
    {
      m_areaLsdb[lsId] = std::make_pair (lsaHeader, areaLsa);
      ScheduleUpdateL2ShortestPath ();
      return;
    }
  // If the received areaLSA is newer and taking precedence
  if (lsaHeader.GetSeqNum () > m_areaLsdb[lsId].first.GetSeqNum ())
    {
      // Higher seq num
      m_areaLsdb[lsId] = std::make_pair (lsaHeader, areaLsa);
      ScheduleUpdateL2ShortestPath ();
    }
  else if (lsaHeader.GetSeqNum () == m_areaLsdb[lsId].first.GetSeqNum () &&
           lsaHeader.GetAdvertisingRouter () < m_areaLsdb[lsId].first.GetAdvertisingRouter ())
    {
      // Tiebreaker. Lower router ID becomes the leader
      m_areaLsdb[lsId] = std::make_pair (lsaHeader, areaLsa);
      ScheduleUpdateL2ShortestPath ();
    }
}

void
OspfApp::ProcessL2SummaryLsa (LsaHeader lsaHeader, Ptr<L2SummaryLsa> l2SummaryLsa)
{
  NS_LOG_FUNCTION (this);
  if (!m_enableAreaProxy)
    {
      return;
    }
  // Fill in the prefixes
  // LS ID of Summary LSA is the originating LSA
  uint32_t lsId = lsaHeader.GetLsId ();

  // If this is the first area-LSA for this area
  if (m_l2SummaryLsdb.find (lsId) == m_l2SummaryLsdb.end ())
    {
      m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, l2SummaryLsa);
      UpdateRouting ();
      return;
    }
  // If the received areaSummaryLSA is newer and taking precedence
  if (lsaHeader.GetSeqNum () > m_l2SummaryLsdb[lsId].first.GetSeqNum ())
    {
      // Higher seq num
      m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, l2SummaryLsa);
      UpdateRouting ();
    }
  else if (lsaHeader.GetSeqNum () == m_l2SummaryLsdb[lsId].first.GetSeqNum () &&
           lsaHeader.GetAdvertisingRouter () < m_l2SummaryLsdb[lsId].first.GetAdvertisingRouter ())
    {
      // Tiebreaker. Lower router ID becomes the leader
      m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, l2SummaryLsa);
      UpdateRouting ();
    }
}

// LSA
// Fetch LSA from LSDB
std::pair<LsaHeader, Ptr<Lsa>>
OspfApp::FetchLsa (LsaHeader::LsaKey lsaKey)
{
  uint32_t lsId = std::get<1> (lsaKey);

  switch (std::get<0> (lsaKey))
    {
    case LsaHeader::RouterLSAs:
      {
        auto it = m_routerLsdb.find (lsId);
        if (it == m_routerLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: RouterLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    case LsaHeader::L1SummaryLSAs:
      {
        auto it = m_l1SummaryLsdb.find (lsId);
        if (it == m_l1SummaryLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: L1SummaryLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    case LsaHeader::AreaLSAs:
      {
        auto it = m_areaLsdb.find (lsId);
        if (it == m_areaLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: AreaLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    case LsaHeader::L2SummaryLSAs:
      {
        auto it = m_l2SummaryLsdb.find (lsId);
        if (it == m_l2SummaryLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: L2SummaryLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    default:
      NS_LOG_WARN ("FetchLsa: unsupported LSA type " << static_cast<uint32_t> (std::get<0> (lsaKey)));
      return {LsaHeader (), nullptr};
    }
}
// Generate Local AS External LSA
Ptr<L1SummaryLsa>
OspfApp::GetL1SummaryLsa ()
{
  Ptr<L1SummaryLsa> l1SummaryLsa = Create<L1SummaryLsa> ();
  for (auto &[ifIndex, dest, mask, addr, metric] : m_externalRoutes)
    {
      (void)ifIndex;
      (void)addr;
      l1SummaryLsa->AddRoute (SummaryRoute (dest, mask, metric));
    }
  return l1SummaryLsa;
}
// Generate Local Router LSA with all areas
Ptr<RouterLsa>
OspfApp::GetRouterLsa ()
{
  std::vector<RouterLink> allLinks;
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      std::vector<RouterLink> links = m_ospfInterfaces[i]->GetActiveRouterLinks ();
      for (auto l : links)
        {
          allLinks.emplace_back (l);
        }
    }
  NS_LOG_INFO ("Router-LSA Created with " << allLinks.size () << " active links");
  return ConstructRouterLsa (allLinks);
}

// Generate Local Area LSA for the leader
Ptr<AreaLsa>
OspfApp::GetAreaLsa ()
{
  // <area neighbor ID, leader's IP address, interface metric>
  std::vector<AreaLink> allAreaLinks;
  // Read Router LSDB and extract cross-area links
  for (auto &[remoteRouterId, routerLsa] : m_routerLsdb)
    {
      auto crossAreaLinks = routerLsa.second->GetCrossAreaLinks ();
      for (auto l : crossAreaLinks)
        {
          allAreaLinks.emplace_back (l);
        }
    }
  NS_LOG_INFO ("Area-LSA Created with " << allAreaLinks.size () << " active links");
  return ConstructAreaLsa (allAreaLinks);
}

void
OspfApp::RecomputeRouterLsa ()
{
  NS_LOG_FUNCTION (this);

  auto lsaKey =
      std::make_tuple (LsaHeader::LsType::RouterLSAs, m_routerId.Get (), m_routerId.Get ());
  // Initialize seq number to zero if new
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // Increment a seq number
  m_seqNumbers[lsaKey]++;

  // Construct its router LSA
  Ptr<RouterLsa> routerLsa = GetRouterLsa ();

  // Assign routerLsa to its router LSDB
  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + routerLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_routerLsdb[m_routerId.Get ()] = std::make_pair (lsaHeader, routerLsa);

  // Update routing according to the updated LSDB
  ScheduleUpdateL1ShortestPath ();

  // Flood to neighbor
  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  lsUpdate->AddLsa (m_routerLsdb[m_routerId.Get ()]);
  FloodLsu (0, lsUpdate);
}

void
OspfApp::RecomputeL1SummaryLsa ()
{
  NS_LOG_FUNCTION (this);

  auto lsaKey =
      std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, m_routerId.Get (), m_routerId.Get ());
  // Initialize seq number to zero if new
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // Increment a seq number
  m_seqNumbers[lsaKey]++;

  // Construct its router LSA
  Ptr<L1SummaryLsa> l1SummaryLsa = GetL1SummaryLsa ();

  // Assign routerLsa to its router LSDB
  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + l1SummaryLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_l1SummaryLsdb[m_routerId.Get ()] = std::make_pair (lsaHeader, l1SummaryLsa);

  // Create its LSU packet containing its own L1 prefixes and flood
  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  lsUpdate->AddLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
  FloodLsu (0, lsUpdate);

  // Update routing according to the updated LSDB
  UpdateRouting ();
}

// Recompute Area LSA
bool
OspfApp::RecomputeAreaLsa ()
{
  NS_LOG_FUNCTION (this);

  // Construct its area LSA
  Ptr<AreaLsa> areaLsa = GetAreaLsa ();

  // Do not update Area LSDB if it still reflects the current cross-border links in Router LSDB
  if (m_areaLsdb.find (m_areaId) != m_areaLsdb.end () &&
      areaLsa->GetLinks () == m_areaLsdb[m_areaId].second->GetLinks ())
    {
      return false;
    }

  auto lsaKey = std::make_tuple (LsaHeader::LsType::AreaLSAs, m_areaId, m_routerId.Get ());

  // Initialize seq number to zero if new
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // Increment a seq number
  m_seqNumbers[lsaKey]++;

  // Assign areaLsa to its area LSDB
  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + areaLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_areaLsdb[m_areaId] = std::make_pair (lsaHeader, areaLsa);

  // Flood LSA
  Ptr<LsUpdate> lsUpdateArea = Create<LsUpdate> ();
  lsUpdateArea->AddLsa (m_areaLsdb[m_areaId]);
  FloodLsu (0, lsUpdateArea);
  ScheduleUpdateL2ShortestPath ();
  return true;
}

bool
OspfApp::RecomputeL2SummaryLsa ()
{
  NS_LOG_FUNCTION (this);

  // Construct its area LSA
  Ptr<L2SummaryLsa> summary = Create<L2SummaryLsa> ();
  for (auto &[routerId, l1SummaryLsa] : m_l1SummaryLsdb)
    {
      for (auto route : l1SummaryLsa.second->GetRoutes ())
        {
          summary->AddRoute (SummaryRoute (route.m_address, route.m_mask, route.m_metric));
        }
    }
  if (m_l2SummaryLsdb.find (m_areaId) != m_l2SummaryLsdb.end ())
    {
      auto &[header, lsa] = m_l2SummaryLsdb[m_areaId];
      // Will not generate a new prefix LSA when the content doesn't change
      if (lsa->GetRoutes () == summary->GetRoutes ())
        {
          return false;
        }
    }

  auto lsaKey = std::make_tuple (LsaHeader::LsType::L2SummaryLSAs, m_areaId, m_routerId.Get ());

  // Initialize seq number to zero if new
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // Increment a seq number
  m_seqNumbers[lsaKey]++;

  // Assign areaLsa to its area LSDB
  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + summary->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_l2SummaryLsdb[m_areaId] = std::make_pair (lsaHeader, summary);

  // Flood LSA
  Ptr<LsUpdate> lsUpdateSummary = Create<LsUpdate> ();
  lsUpdateSummary->AddLsa (m_l2SummaryLsdb[m_areaId]);
  FloodLsu (0, lsUpdateSummary);
  UpdateRouting ();
  return true;
}

void
OspfApp::UpdateRouting ()
{
  // Remove old route
  // std::cout << "Number of Route: " << m_boundDevices.GetN() << std::endl;
  while (m_routing->GetNRoutes () > m_boundDevices.GetN ())
    {
      m_routing->RemoveRoute (m_boundDevices.GetN ());
    }

  std::map<std::pair<uint32_t, uint32_t>, std::tuple<Ipv4Address, uint32_t, uint32_t>> bestDest,
      l2BestDest;
  // Fill in local routes
  for (auto &[ifIndex, dest, mask, addr, metric] : m_externalRoutes)
    {
      bestDest[std::make_pair (dest, mask)] =
          std::make_tuple (Ipv4Address::GetZero (), ifIndex, metric);
    }

  for (auto &[remoteRouterId, nextHop] : m_l1NextHop)
    {
      if (m_l1SummaryLsdb.find (remoteRouterId) == m_l1SummaryLsdb.end ())
        {
          continue;
        }
      // auto n = m_l1SummaryLsdb[remoteRouterId].second->GetNRoutes ();
      for (auto route : m_l1SummaryLsdb[remoteRouterId].second->GetRoutes ())
        {
          auto mask = Ipv4Mask (route.m_mask);
          auto dest = Ipv4Address (route.m_address);
          auto key = std::make_pair (dest.CombineMask (mask).Get (), mask.Get ());
          if (bestDest.find (key) == bestDest.end () ||
              nextHop.metric < std::get<2> (bestDest[key]))
            {
              // TODO: Do ECMP
              bestDest[key] = std::make_tuple (nextHop.ipAddress, nextHop.ifIndex, nextHop.metric);
            }
        }
    }
  // Fill L1 in the routing table
  for (auto &[key, value] : bestDest)
    {
      auto &[dest, mask] = key;
      auto &[addr, ifIndex, metric] = value;
      m_routing->AddNetworkRouteTo (Ipv4Address (dest), Ipv4Mask (mask), addr, ifIndex, metric);
    }
  // Fill L2 in the routing table (less priority)
  for (auto &[remoteAreaId, l2NextHop] : m_l2NextHop)
    {
      if (remoteAreaId == m_areaId)
        continue;
      if (m_l2SummaryLsdb.find (remoteAreaId) == m_l2SummaryLsdb.end ())
        {
          continue;
        }
      auto header = m_l2SummaryLsdb[remoteAreaId].first;
      auto lsa = m_l2SummaryLsdb[remoteAreaId].second;
      auto nextHop = m_nextHopToShortestBorderRouter[l2NextHop.first].second;
      nextHop.metric += l2NextHop.second;
      // Ipv4Address network = Ipv4Address(header.GetAdvertisingRouter()).CombineMask (lsa->GetMask());
      for (auto route : lsa->GetRoutes ())
        {
          auto mask = Ipv4Mask (route.m_mask);
          auto dest = Ipv4Address (route.m_address);
          auto key = std::make_pair (dest.CombineMask (mask).Get (), mask.Get ());
          // Don't compete with L1
          if (bestDest.find (key) != bestDest.end ())
            continue;
          if (l2BestDest.find (key) == l2BestDest.end () ||
              nextHop.metric + route.m_metric < std::get<2> (l2BestDest[key]))
            {
              // TODO: Do ECMP
              l2BestDest[key] = std::make_tuple (nextHop.ipAddress, nextHop.ifIndex,
                                                 nextHop.metric + route.m_metric);
            }
        }
    }
  // Fill L2 in the routing table
  for (auto &[key, value] : l2BestDest)
    {
      auto &[dest, mask] = key;
      auto &[addr, ifIndex, metric] = value;
      m_routing->AddNetworkRouteTo (Ipv4Address (dest), Ipv4Mask (mask), addr, ifIndex, metric);
    }
}
void
OspfApp::ScheduleUpdateL1ShortestPath ()
{
  // Can update at least once in m_shortestPathUpdateDelay
  if (m_updateL1ShortestPathTimeout.IsRunning ())
    {
      return;
    }
  m_updateL1ShortestPathTimeout =
      Simulator::Schedule (m_shortestPathUpdateDelay, &OspfApp::UpdateL1ShortestPath, this);
}

void
OspfApp::UpdateL1ShortestPath ()
{
  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  // <distance, next hop>
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;
  NS_LOG_FUNCTION (this);

  // Clear existing next-hop data
  m_l1NextHop.clear ();

  // Dijkstra
  while (!pq.empty ())
    {
      pq.pop ();
    }
  distanceTo.clear ();
  uint32_t u, v, w;
  pq.emplace (0, m_routerId.Get ());
  distanceTo[m_routerId.Get ()] = 0;
  while (!pq.empty ())
    {
      std::tie (w, u) = pq.top ();
      pq.pop ();
      // Skip if the lsdb doesn't have any neighbors for that router
      if (m_routerLsdb.find (u) == m_routerLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_routerLsdb[u].second->GetNLink (); i++)
        {
          v = m_routerLsdb[u].second->GetLink (i).m_linkId;
          auto metric = m_routerLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }
  // std::cout << "node: " << GetNode()->GetId() << std::endl;
  // Shortest path information for each subnet -- <mask, nextHop, interface, metric>
  std::map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> routingEntries;
  for (auto &[remoteRouterId, routerLsa] : m_routerLsdb)
    {
      // std::cout << "  destination: " << Ipv4Address(remoteRouterId) << std::endl;

      // No reachable path
      if (prevHop.find (remoteRouterId) == prevHop.end ())
        {
          // std::cout << "    no route" << std::endl;
          continue;
        }

      // Find the first hop
      v = remoteRouterId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_routerId.Get ())
            {
              break;
            }
          v = prevHop[v];
        }

      // Find the next hop's IP and interface index
      uint32_t ifIndex = 0;
      Ipv4Address ipAddress;
      for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
        {
          auto neighbors = m_ospfInterfaces[i]->GetNeighbors ();
          for (auto n : neighbors)
            {
              if (n->GetState () < OspfNeighbor::Full)
                {
                  continue;
                }
              if (n->GetRouterId ().Get () == v)
                {
                  ifIndex = i;
                  ipAddress = n->GetIpAddress ();
                  break;
                }
            }
          if (ifIndex)
            break;
        }
      if (ifIndex == 0)
        {
          NS_LOG_WARN ("No FULL neighbor found for next-hop routerId="
                       << Ipv4Address (v) << "; skipping next-hop computation");
          continue;
        }

      // Fill in the next hop and prefixes data
      m_l1NextHop[remoteRouterId] = NextHop (ifIndex, ipAddress, distanceTo[remoteRouterId]);
    }
  if (m_enableAreaProxy)
    {
      // Getting exit routers
      m_nextHopToShortestBorderRouter.clear ();
      for (auto &[remoteRouterId, lsa] : m_routerLsdb)
        {
          auto links = lsa.second->GetCrossAreaLinks ();
          // Skip self router
          if (m_routerId.Get () == remoteRouterId)
            {
              continue;
            }
          if (m_l1NextHop.find (remoteRouterId) == m_l1NextHop.end ())
            continue;
          for (auto link : links)
            {
              if (m_nextHopToShortestBorderRouter.find (link.m_areaId) ==
                      m_nextHopToShortestBorderRouter.end () ||
                  m_nextHopToShortestBorderRouter[link.m_areaId].second.metric >
                      m_l1NextHop[remoteRouterId].metric + link.m_metric)
                {
                  m_nextHopToShortestBorderRouter[link.m_areaId] =
                      std::make_pair (remoteRouterId, m_l1NextHop[remoteRouterId]);
                  m_nextHopToShortestBorderRouter[link.m_areaId].second.metric += link.m_metric;
                }
            }
        }
      // Fill nextHopToShortestBorderRouter for itself
      // Currently will take the loweest metric even when having a direct link to the exit router
      if (m_routerLsdb.find (m_routerId.Get ()) != m_routerLsdb.end ())
        {
          auto lsaHeader = m_routerLsdb[m_routerId.Get ()].first;
          auto routerLsa = m_routerLsdb[m_routerId.Get ()].second;
          for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
            {
              for (auto neighbor : m_ospfInterfaces[i]->GetNeighbors ())
                {
                  if (neighbor->GetState () < OspfNeighbor::TwoWay)
                    {
                      continue;
                    }
                  if (neighbor->GetArea () != m_areaId)
                    {
                      if (m_nextHopToShortestBorderRouter.find (neighbor->GetArea ()) ==
                              m_nextHopToShortestBorderRouter.end () ||
                          m_nextHopToShortestBorderRouter[neighbor->GetArea ()].second.metric >
                              m_ospfInterfaces[i]->GetMetric ())
                        {
                          m_nextHopToShortestBorderRouter[neighbor->GetArea ()] = std::make_pair (
                              m_routerId.Get (), NextHop (i, neighbor->GetIpAddress (),
                                                          m_ospfInterfaces[i]->GetMetric ()));
                        }
                    }
                }
            }
        }
    }
  UpdateRouting ();
}

void
OspfApp::ScheduleUpdateL2ShortestPath ()
{
  // Can update at least once in m_shortestPathUpdateDelay
  if (m_updateL2ShortestPathTimeout.IsRunning ())
    {
      return;
    }
  m_updateL2ShortestPathTimeout =
      Simulator::Schedule (m_shortestPathUpdateDelay, &OspfApp::UpdateL2ShortestPath, this);
}

void
OspfApp::UpdateL2ShortestPath ()
{
  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  // <distance, next hop>
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;
  NS_LOG_FUNCTION (this);

  // Clear existing next-hop data
  m_l2NextHop.clear ();

  // Dijkstra
  while (!pq.empty ())
    {
      pq.pop ();
    }
  distanceTo.clear ();
  uint32_t u, v, w;
  pq.emplace (0, m_areaId);
  distanceTo[m_areaId] = 0;
  while (!pq.empty ())
    {
      std::tie (w, u) = pq.top ();
      pq.pop ();
      // Skip if the lsdb doesn't have any neighbors for that router
      if (m_areaLsdb.find (u) == m_areaLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_areaLsdb[u].second->GetNLink (); i++)
        {
          v = m_areaLsdb[u].second->GetLink (i).m_areaId;
          auto metric = m_areaLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }
  // std::cout << "node: " << GetNode()->GetId() << std::endl;
  // Find the shortest paths and the next hops
  for (auto &[remoteAreaId, areaLsa] : m_areaLsdb)
    {
      // No reachable path
      if (prevHop.find (remoteAreaId) == prevHop.end ())
        {
          // std::cout << "    no route" << std::endl;
          continue;
        }

      // Find the first hop
      v = remoteAreaId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_areaId)
            {
              break;
            }
          v = prevHop[v];
        }

      // Fill in the next hop and prefixes data
      for (uint32_t i = 0; i < areaLsa.second->GetNLink (); i++)
        {
          // NS_LOG_DEBUG ("Add route: " << Ipv4Address (routerLsa.second->GetLink (i).m_linkData)
          // << ", " << ifIndex << ", " << distanceTo[areaId]);
          m_l2NextHop[remoteAreaId] = std::make_pair (v, distanceTo[remoteAreaId]);
        }
    }
  UpdateRouting ();
}

// Area
void
OspfApp::AreaLeaderBegin ()
{
  NS_LOG_FUNCTION (this);
  std::cout << "Area Leader Begin " << m_areaId << ", " << m_routerId << std::endl;
  m_isAreaLeader = true;
  // Area Leader Logic -- start flooding Area-LSA and Summary-LSA-Area
  // Flood LSU with Area-LSAs to all interfaces
  RecomputeAreaLsa ();

  // Flood Area Summary LSA for L2 routing prefix
  RecomputeL2SummaryLsa ();
}


void
OspfApp::AreaLeaderEnd ()
{
  NS_LOG_FUNCTION (this);
  m_isAreaLeader = false;
  // TODO: Area Leader Logic -- stop flooding Area-LSA and Summary-LSA-Area
}

} // namespace ns3
