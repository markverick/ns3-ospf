/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-routing-engine.h"

#include "ospf-app-private.h"

namespace ns3 {

OspfRoutingEngine::OspfRoutingEngine (OspfApp &app)
  : m_app (app)
{
}

void
OspfRoutingEngine::UpdateRouting ()
{
  // Remove old route
  while (m_app.m_routing->GetNRoutes () > m_app.m_boundDevices.GetN ())
    {
      m_app.m_routing->RemoveRoute (m_app.m_boundDevices.GetN ());
    }

  std::map<std::pair<uint32_t, uint32_t>, std::tuple<Ipv4Address, uint32_t, uint32_t>> bestDest,
      l2BestDest;
  // Fill in local routes
  for (auto &[ifIndex, dest, mask, addr, metric] : m_app.m_externalRoutes)
    {
      bestDest[std::make_pair (dest, mask)] =
          std::make_tuple (Ipv4Address::GetZero (), ifIndex, metric);
    }

  for (auto &[remoteRouterId, nextHop] : m_app.m_l1NextHop)
    {
      if (m_app.m_l1SummaryLsdb.find (remoteRouterId) == m_app.m_l1SummaryLsdb.end ())
        {
          continue;
        }
      for (auto route : m_app.m_l1SummaryLsdb[remoteRouterId].second->GetRoutes ())
        {
          auto mask = Ipv4Mask (route.m_mask);
          auto dest = Ipv4Address (route.m_address);
          auto key = std::make_pair (dest.CombineMask (mask).Get (), mask.Get ());
          if (bestDest.find (key) == bestDest.end () || nextHop.metric < std::get<2> (bestDest[key]))
            {
              bestDest[key] =
                  std::make_tuple (nextHop.ipAddress, nextHop.ifIndex, nextHop.metric);
            }
        }
    }
  // Fill L1 in the routing table
  for (auto &[key, value] : bestDest)
    {
      auto &[dest, mask] = key;
      auto &[addr, ifIndex, metric] = value;
      m_app.m_routing->AddNetworkRouteTo (Ipv4Address (dest), Ipv4Mask (mask), addr, ifIndex, metric);
    }

  // Fill L2 in the routing table (less priority)
  for (auto &[remoteAreaId, l2NextHop] : m_app.m_l2NextHop)
    {
      if (remoteAreaId == m_app.m_areaId)
        continue;
      if (m_app.m_l2SummaryLsdb.find (remoteAreaId) == m_app.m_l2SummaryLsdb.end ())
        {
          continue;
        }
      auto lsa = m_app.m_l2SummaryLsdb[remoteAreaId].second;
      auto nextHop = m_app.m_nextHopToShortestBorderRouter[l2NextHop.first].second;
      nextHop.metric += l2NextHop.second;
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
      m_app.m_routing->AddNetworkRouteTo (Ipv4Address (dest), Ipv4Mask (mask), addr, ifIndex, metric);
    }
}

void
OspfRoutingEngine::ScheduleUpdateL1ShortestPath ()
{
  // Can update at least once in m_shortestPathUpdateDelay
  if (m_app.m_updateL1ShortestPathTimeout.IsRunning ())
    {
      return;
    }
  m_app.m_updateL1ShortestPathTimeout =
      Simulator::Schedule (m_app.m_shortestPathUpdateDelay, &OspfApp::UpdateL1ShortestPath, &m_app);
}

void
OspfRoutingEngine::UpdateL1ShortestPath ()
{
  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;
  NS_LOG_FUNCTION (&m_app);

  // Clear existing next-hop data
  m_app.m_l1NextHop.clear ();

  // Dijkstra
  while (!pq.empty ())
    {
      pq.pop ();
    }
  distanceTo.clear ();
  uint32_t u, v, w;
  pq.emplace (0, m_app.m_routerId.Get ());
  distanceTo[m_app.m_routerId.Get ()] = 0;
  while (!pq.empty ())
    {
      std::tie (w, u) = pq.top ();
      pq.pop ();
      if (m_app.m_routerLsdb.find (u) == m_app.m_routerLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_app.m_routerLsdb[u].second->GetNLink (); i++)
        {
          v = m_app.m_routerLsdb[u].second->GetLink (i).m_linkId;
          auto metric = m_app.m_routerLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }

  for (auto &[remoteRouterId, routerLsa] : m_app.m_routerLsdb)
    {
      (void)routerLsa;
      // No reachable path
      if (prevHop.find (remoteRouterId) == prevHop.end ())
        {
          continue;
        }

      // Find the first hop
      v = remoteRouterId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_app.m_routerId.Get ())
            {
              break;
            }
          v = prevHop[v];
        }

      // Find the next hop's IP and interface index
      uint32_t ifIndex = 0;
      Ipv4Address ipAddress;
      for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
        {
          auto neighbors = m_app.m_ospfInterfaces[i]->GetNeighbors ();
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
      NS_ASSERT (ifIndex > 0);

      m_app.m_l1NextHop[remoteRouterId] = NextHop (ifIndex, ipAddress, distanceTo[remoteRouterId]);
    }

  if (m_app.m_enableAreaProxy)
    {
      // Getting exit routers
      m_app.m_nextHopToShortestBorderRouter.clear ();
      for (auto &[remoteRouterId, lsa] : m_app.m_routerLsdb)
        {
          auto links = lsa.second->GetCrossAreaLinks ();
          // Skip self router
          if (m_app.m_routerId.Get () == remoteRouterId)
            {
              continue;
            }
          if (m_app.m_l1NextHop.find (remoteRouterId) == m_app.m_l1NextHop.end ())
            continue;
          for (auto link : links)
            {
              if (m_app.m_nextHopToShortestBorderRouter.find (link.m_areaId) ==
                      m_app.m_nextHopToShortestBorderRouter.end () ||
                  m_app.m_nextHopToShortestBorderRouter[link.m_areaId].second.metric >
                      m_app.m_l1NextHop[remoteRouterId].metric + link.m_metric)
                {
                  m_app.m_nextHopToShortestBorderRouter[link.m_areaId] =
                      std::make_pair (remoteRouterId, m_app.m_l1NextHop[remoteRouterId]);
                  m_app.m_nextHopToShortestBorderRouter[link.m_areaId].second.metric += link.m_metric;
                }
            }
        }
      // Fill nextHopToShortestBorderRouter for itself
      if (m_app.m_routerLsdb.find (m_app.m_routerId.Get ()) != m_app.m_routerLsdb.end ())
        {
          for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
            {
              for (auto neighbor : m_app.m_ospfInterfaces[i]->GetNeighbors ())
                {
                  if (neighbor->GetState () < OspfNeighbor::TwoWay)
                    {
                      continue;
                    }
                  if (neighbor->GetArea () != m_app.m_areaId)
                    {
                      if (m_app.m_nextHopToShortestBorderRouter.find (neighbor->GetArea ()) ==
                              m_app.m_nextHopToShortestBorderRouter.end () ||
                          m_app.m_nextHopToShortestBorderRouter[neighbor->GetArea ()].second.metric >
                              m_app.m_ospfInterfaces[i]->GetMetric ())
                        {
                          m_app.m_nextHopToShortestBorderRouter[neighbor->GetArea ()] =
                              std::make_pair (m_app.m_routerId.Get (),
                                              NextHop (i, neighbor->GetIpAddress (),
                                                       m_app.m_ospfInterfaces[i]->GetMetric ()));
                        }
                    }
                }
            }
        }
    }
  UpdateRouting ();
}

void
OspfRoutingEngine::ScheduleUpdateL2ShortestPath ()
{
  // Can update at least once in m_shortestPathUpdateDelay
  if (m_app.m_updateL2ShortestPathTimeout.IsRunning ())
    {
      return;
    }
  m_app.m_updateL2ShortestPathTimeout =
      Simulator::Schedule (m_app.m_shortestPathUpdateDelay, &OspfApp::UpdateL2ShortestPath, &m_app);
}

void
OspfRoutingEngine::UpdateL2ShortestPath ()
{
  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;
  NS_LOG_FUNCTION (&m_app);

  // Clear existing next-hop data
  m_app.m_l2NextHop.clear ();

  // Dijkstra
  while (!pq.empty ())
    {
      pq.pop ();
    }
  distanceTo.clear ();
  uint32_t u, v, w;
  pq.emplace (0, m_app.m_areaId);
  distanceTo[m_app.m_areaId] = 0;
  while (!pq.empty ())
    {
      std::tie (w, u) = pq.top ();
      pq.pop ();
      if (m_app.m_areaLsdb.find (u) == m_app.m_areaLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_app.m_areaLsdb[u].second->GetNLink (); i++)
        {
          v = m_app.m_areaLsdb[u].second->GetLink (i).m_areaId;
          auto metric = m_app.m_areaLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }

  // Find the shortest paths and the next hops
  for (auto &[remoteAreaId, areaLsa] : m_app.m_areaLsdb)
    {
      (void)areaLsa;
      // No reachable path
      if (prevHop.find (remoteAreaId) == prevHop.end ())
        {
          continue;
        }

      // Find the first hop
      v = remoteAreaId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_app.m_areaId)
            {
              break;
            }
          v = prevHop[v];
        }

      // Fill in the next hop and prefixes data
      for (uint32_t i = 0; i < m_app.m_areaLsdb[remoteAreaId].second->GetNLink (); i++)
        {
          m_app.m_l2NextHop[remoteAreaId] = std::make_pair (v, distanceTo[remoteAreaId]);
        }
    }
  UpdateRouting ();
}

} // namespace ns3
