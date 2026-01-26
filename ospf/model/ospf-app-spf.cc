/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {

void
OspfApp::UpdateL1ShortestPath ()
{
  NS_LOG_FUNCTION (this);

  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;

  m_l1NextHop.clear ();

  // Dijkstra's algorithm
  distanceTo[m_routerId.Get ()] = 0;
  pq.emplace (0, m_routerId.Get ());

  while (!pq.empty ())
    {
      auto [w, u] = pq.top ();
      pq.pop ();

      if (m_routerLsdb.find (u) == m_routerLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_routerLsdb[u].second->GetNLink (); i++)
        {
          uint32_t v = m_routerLsdb[u].second->GetLink (i).m_linkId;
          auto metric = m_routerLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }

  // Compute next hops for each destination
  for (auto &[remoteRouterId, routerLsa] : m_routerLsdb)
    {
      if (prevHop.find (remoteRouterId) == prevHop.end ())
        {
          continue;
        }

      // Find first hop
      uint32_t v = remoteRouterId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_routerId.Get ())
            {
              break;
            }
          v = prevHop[v];
        }

      // Find next hop's IP and interface
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

      m_l1NextHop[remoteRouterId] = NextHop (ifIndex, ipAddress, distanceTo[remoteRouterId]);
    }

  // Compute border router next hops for L2 routing
  if (m_enableAreaProxy)
    {
      m_nextHopToShortestBorderRouter.clear ();
      
      for (auto &[remoteRouterId, lsa] : m_routerLsdb)
        {
          if (m_routerId.Get () == remoteRouterId)
            {
              continue;
            }
          if (m_l1NextHop.find (remoteRouterId) == m_l1NextHop.end ())
            continue;

          auto links = lsa.second->GetCrossAreaLinks ();
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

      // Check for direct cross-area links
      if (m_routerLsdb.find (m_routerId.Get ()) != m_routerLsdb.end ())
        {
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
OspfApp::UpdateL2ShortestPath ()
{
  NS_LOG_FUNCTION (this);

  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;

  m_l2NextHop.clear ();

  // Dijkstra's algorithm for L2
  distanceTo[m_areaId] = 0;
  pq.emplace (0, m_areaId);

  while (!pq.empty ())
    {
      auto [w, u] = pq.top ();
      pq.pop ();

      if (m_areaLsdb.find (u) == m_areaLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_areaLsdb[u].second->GetNLink (); i++)
        {
          uint32_t v = m_areaLsdb[u].second->GetLink (i).m_areaId;
          auto metric = m_areaLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }

  // Compute next hops for each remote area
  for (auto &[remoteAreaId, areaLsa] : m_areaLsdb)
    {
      if (prevHop.find (remoteAreaId) == prevHop.end ())
        {
          continue;
        }

      // Find first hop
      uint32_t v = remoteAreaId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_areaId)
            {
              break;
            }
          v = prevHop[v];
        }

      // Store next hop information
      for (uint32_t i = 0; i < areaLsa.second->GetNLink (); i++)
        {
          m_l2NextHop[remoteAreaId] = std::make_pair (v, distanceTo[remoteAreaId]);
        }
    }

  UpdateRouting ();
}

} // namespace ns3
