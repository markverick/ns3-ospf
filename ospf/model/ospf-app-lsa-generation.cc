/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {

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

Ptr<AreaLsa>
OspfApp::GetAreaLsa ()
{
  std::vector<AreaLink> allAreaLinks;
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

  if (!m_minLsInterval.IsZero ())
    {
      m_lastLsaOriginationTime[lsaKey] = Simulator::Now ();
    }

  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  m_seqNumbers[lsaKey]++;

  Ptr<RouterLsa> routerLsa = GetRouterLsa ();

  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + routerLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_routerLsdb[m_routerId.Get ()] = std::make_pair (lsaHeader, routerLsa);

  ScheduleUpdateL1ShortestPath ();

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

  if (!m_minLsInterval.IsZero ())
    {
      m_lastLsaOriginationTime[lsaKey] = Simulator::Now ();
    }

  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  m_seqNumbers[lsaKey]++;

  Ptr<L1SummaryLsa> l1SummaryLsa = GetL1SummaryLsa ();

  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + l1SummaryLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_l1SummaryLsdb[m_routerId.Get ()] = std::make_pair (lsaHeader, l1SummaryLsa);

  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  lsUpdate->AddLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
  FloodLsu (0, lsUpdate);

  UpdateRouting ();
}

bool
OspfApp::RecomputeAreaLsa ()
{
  NS_LOG_FUNCTION (this);

  Ptr<AreaLsa> areaLsa = GetAreaLsa ();

  if (m_areaLsdb.find (m_areaId) != m_areaLsdb.end () &&
      areaLsa->GetLinks () == m_areaLsdb[m_areaId].second->GetLinks ())
    {
      return false;
    }

  auto lsaKey = std::make_tuple (LsaHeader::LsType::AreaLSAs, m_areaId, m_routerId.Get ());

  if (!m_minLsInterval.IsZero ())
    {
      m_lastLsaOriginationTime[lsaKey] = Simulator::Now ();
    }

  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  m_seqNumbers[lsaKey]++;

  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + areaLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_areaLsdb[m_areaId] = std::make_pair (lsaHeader, areaLsa);

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
      if (lsa->GetRoutes () == summary->GetRoutes ())
        {
          return false;
        }
    }

  auto lsaKey = std::make_tuple (LsaHeader::LsType::L2SummaryLSAs, m_areaId, m_routerId.Get ());

  if (!m_minLsInterval.IsZero ())
    {
      m_lastLsaOriginationTime[lsaKey] = Simulator::Now ();
    }

  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  m_seqNumbers[lsaKey]++;

  LsaHeader lsaHeader (lsaKey);
  lsaHeader.SetLength (20 + summary->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  m_l2SummaryLsdb[m_areaId] = std::make_pair (lsaHeader, summary);

  Ptr<LsUpdate> lsUpdateSummary = Create<LsUpdate> ();
  lsUpdateSummary->AddLsa (m_l2SummaryLsdb[m_areaId]);
  FloodLsu (0, lsUpdateSummary);
  UpdateRouting ();
  return true;
}

} // namespace ns3
