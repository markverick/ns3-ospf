/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {

void
OspfApp::UpdateRouting ()
{
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

  // Fill L1 routes
  for (auto &[remoteRouterId, nextHop] : m_l1NextHop)
    {
      if (m_l1SummaryLsdb.find (remoteRouterId) == m_l1SummaryLsdb.end ())
        {
          continue;
        }
      for (auto route : m_l1SummaryLsdb[remoteRouterId].second->GetRoutes ())
        {
          auto mask = Ipv4Mask (route.m_mask);
          auto dest = Ipv4Address (route.m_address);
          auto key = std::make_pair (dest.CombineMask (mask).Get (), mask.Get ());
          if (bestDest.find (key) == bestDest.end () ||
              nextHop.metric < std::get<2> (bestDest[key]))
            {
              bestDest[key] = std::make_tuple (nextHop.ipAddress, nextHop.ifIndex, nextHop.metric);
            }
        }
    }

  // Install L1 routes
  for (auto &[key, value] : bestDest)
    {
      auto &[dest, mask] = key;
      auto &[addr, ifIndex, metric] = value;
      m_routing->AddNetworkRouteTo (Ipv4Address (dest), Ipv4Mask (mask), addr, ifIndex, metric);
    }

  // Fill L2 routes (lower priority than L1)
  for (auto &[remoteAreaId, l2NextHop] : m_l2NextHop)
    {
      if (remoteAreaId == m_areaId)
        continue;
      if (m_l2SummaryLsdb.find (remoteAreaId) == m_l2SummaryLsdb.end ())
        {
          continue;
        }
      auto nextHop = m_nextHopToShortestBorderRouter[l2NextHop.first].second;
      nextHop.metric += l2NextHop.second;
      
      for (auto route : m_l2SummaryLsdb[remoteAreaId].second->GetRoutes ())
        {
          auto mask = Ipv4Mask (route.m_mask);
          auto dest = Ipv4Address (route.m_address);
          auto key = std::make_pair (dest.CombineMask (mask).Get (), mask.Get ());
          
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

  // Install L2 routes
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
