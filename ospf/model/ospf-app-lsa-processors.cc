/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-lsa-processor.h"
#include "ospf-app-area-leader-controller.h"

namespace ns3 {

namespace {

bool
ShouldReplaceLsdbEntry (const LsaHeader &incoming, const LsaHeader &current)
{
  return incoming.GetSeqNum () > current.GetSeqNum () ||
         (incoming.GetSeqNum () == current.GetSeqNum () &&
          incoming.GetAdvertisingRouter () < current.GetAdvertisingRouter ());
}

} // namespace

void
OspfLsaProcessor::ProcessL1SummaryLsa (LsaHeader lsaHeader, Ptr<L1SummaryLsa> l1SummaryLsa)
{
  uint32_t lsId = lsaHeader.GetLsId ();
  const auto advertisingRouter = lsaHeader.GetAdvertisingRouter ();

  NS_LOG_FUNCTION (&m_app);
  auto it = m_app.m_l1SummaryLsdb.find (lsId);
  if (it != m_app.m_l1SummaryLsdb.end () && !ShouldReplaceLsdbEntry (lsaHeader, it->second.first))
    {
      return;
    }

  m_app.m_l1SummaryLsdb[lsId] = std::make_pair (lsaHeader, l1SummaryLsa);

  if (advertisingRouter != m_app.m_routerId.Get ())
    {
      m_app.ReplaceOwnedPrefixes (OspfApp::MakeOwnerRef (OspfApp::OwnerKind::Router,
                                                          advertisingRouter),
                                  l1SummaryLsa->GetRoutes ());
    }
  else
    {
      m_app.RemoveOwnedPrefixes (OspfApp::MakeOwnerRef (OspfApp::OwnerKind::Router,
                                                        advertisingRouter));
    }

  if (m_app.m_enableAreaProxy)
    {
      if (m_app.m_isAreaLeader)
        {
          m_app.ThrottledRecomputeL2SummaryLsa ();
          if (m_app.m_enableLsaTimingLog)
            {
              std::string fullname = m_app.m_logDir + "/lsa_mapping.csv";
              auto mappingLog = std::ofstream (fullname, std::ios::app);
              auto l1Key = lsaHeader.GetKey ();
              auto l1KeyString = LsaHeader::GetKeyString (lsaHeader.GetSeqNum (), l1Key);
              auto l2Key = m_app.m_l2SummaryLsdb[m_app.m_areaId].first.GetKey ();
              auto l2KeyString =
                  LsaHeader::GetKeyString (m_app.m_l2SummaryLsdb[m_app.m_areaId].first.GetSeqNum (),
                                           l2Key);
              mappingLog << l1KeyString << "," << l2KeyString << std::endl;
              mappingLog.close ();
            }
        }
    }

  m_app.UpdateRouting ();
}

void
OspfLsaProcessor::ProcessRouterLsa (LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa)
{
  uint32_t lsId = lsaHeader.GetLsId ();

  NS_LOG_FUNCTION (&m_app);
  auto it = m_app.m_routerLsdb.find (lsId);
  if (it != m_app.m_routerLsdb.end () && !ShouldReplaceLsdbEntry (lsaHeader, it->second.first))
    {
      return;
    }

  m_app.m_routerLsdb[lsId] = std::make_pair (lsaHeader, routerLsa);
  
  if (m_app.m_enableAreaProxy)
    {
      if (m_app.m_isAreaLeader)
        {
          m_app.ThrottledRecomputeAreaLsa ();
          if (m_app.m_enableLsaTimingLog)
            {
              std::string fullname = m_app.m_logDir + "/lsa_mapping.csv";
              auto mappingLog = std::ofstream (fullname, std::ios::app);
              auto l1Key = lsaHeader.GetKey ();
              auto l1KeyString = LsaHeader::GetKeyString (lsaHeader.GetSeqNum (), l1Key);
              auto l2Key = m_app.m_areaLsdb[m_app.m_areaId].first.GetKey ();
              auto l2KeyString =
                  LsaHeader::GetKeyString (m_app.m_areaLsdb[m_app.m_areaId].first.GetSeqNum (),
                                           l2Key);
              mappingLog << l1KeyString << "," << l2KeyString << std::endl;
              mappingLog.close ();
            }
        }

      if (m_app.m_areaLeaderMode != OspfApp::AREA_LEADER_REACHABLE_LOWEST_ROUTER_ID)
        {
          m_app.m_areaLeader->UpdateLeadershipEligibility ();
        }
    }

  m_app.ScheduleUpdateL1ShortestPath ();
}

void
OspfLsaProcessor::ProcessAreaLsa (LsaHeader lsaHeader, Ptr<AreaLsa> areaLsa)
{
  if (!m_app.m_enableAreaProxy)
    {
      return;
    }
  
  NS_LOG_FUNCTION (&m_app);
  uint32_t lsId = lsaHeader.GetLsId ();

  auto it = m_app.m_areaLsdb.find (lsId);
  if (it == m_app.m_areaLsdb.end ())
    {
      m_app.m_areaLsdb[lsId] = std::make_pair (lsaHeader, areaLsa);
      m_app.ScheduleUpdateL2ShortestPath ();
      return;
    }

  if (ShouldReplaceLsdbEntry (lsaHeader, it->second.first))
    {
      it->second = std::make_pair (lsaHeader, areaLsa);
      m_app.ScheduleUpdateL2ShortestPath ();
    }
}

void
OspfLsaProcessor::ProcessL2SummaryLsa (LsaHeader lsaHeader, Ptr<L2SummaryLsa> l2SummaryLsa)
{
  if (!m_app.m_enableAreaProxy)
    {
      return;
    }
  
  NS_LOG_FUNCTION (&m_app);
  uint32_t lsId = lsaHeader.GetLsId ();

  auto updateOwnedPrefixes = [this, lsId, l2SummaryLsa] {
    if (lsId != m_app.m_areaId)
      {
        m_app.ReplaceOwnedPrefixes (OspfApp::MakeOwnerRef (OspfApp::OwnerKind::Area, lsId),
                                    l2SummaryLsa->GetRoutes ());
      }
    else
      {
        m_app.RemoveOwnedPrefixes (OspfApp::MakeOwnerRef (OspfApp::OwnerKind::Area, lsId));
      }
  };

  auto it = m_app.m_l2SummaryLsdb.find (lsId);
  if (it == m_app.m_l2SummaryLsdb.end ())
    {
      m_app.m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, l2SummaryLsa);
      updateOwnedPrefixes ();
      m_app.UpdateRouting ();
      return;
    }

  if (ShouldReplaceLsdbEntry (lsaHeader, it->second.first))
    {
      it->second = std::make_pair (lsaHeader, l2SummaryLsa);
      updateOwnedPrefixes ();
      m_app.UpdateRouting ();
    }
}

void
OspfApp::AreaLeaderBegin ()
{
  NS_LOG_FUNCTION (this);
  m_areaLeader->Begin ();
}

void
OspfApp::AreaLeaderEnd ()
{
  NS_LOG_FUNCTION (this);
  m_areaLeader->End ();
}

} // namespace ns3
