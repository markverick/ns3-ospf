/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {

void
OspfApp::ProcessL1SummaryLsa (LsaHeader lsaHeader, Ptr<L1SummaryLsa> l1SummaryLsa)
{
  uint32_t lsId = lsaHeader.GetLsId ();

  NS_LOG_FUNCTION (this);
  m_l1SummaryLsdb[lsId] = std::make_pair (lsaHeader, l1SummaryLsa);

  if (m_enableAreaProxy)
    {
      if (m_isAreaLeader)
        {
          ThrottledRecomputeL2SummaryLsa ();
          if (m_enableLsaTimingLog)
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

  UpdateRouting ();
}

void
OspfApp::ProcessRouterLsa (LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa)
{
  uint32_t lsId = lsaHeader.GetLsId ();

  NS_LOG_FUNCTION (this);
  m_routerLsdb[lsId] = std::make_pair (lsaHeader, routerLsa);
  
  if (m_enableAreaProxy)
    {
      if (m_isAreaLeader)
        {
          ThrottledRecomputeAreaLsa ();
          if (m_enableLsaTimingLog)
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
  uint32_t lsId = lsaHeader.GetLsId ();

  if (m_areaLsdb.find (lsId) == m_areaLsdb.end ())
    {
      m_areaLsdb[lsId] = std::make_pair (lsaHeader, areaLsa);
      ScheduleUpdateL2ShortestPath ();
      return;
    }

  if (lsaHeader.GetSeqNum () > m_areaLsdb[lsId].first.GetSeqNum ())
    {
      m_areaLsdb[lsId] = std::make_pair (lsaHeader, areaLsa);
      ScheduleUpdateL2ShortestPath ();
    }
  else if (lsaHeader.GetSeqNum () == m_areaLsdb[lsId].first.GetSeqNum () &&
           lsaHeader.GetAdvertisingRouter () < m_areaLsdb[lsId].first.GetAdvertisingRouter ())
    {
      m_areaLsdb[lsId] = std::make_pair (lsaHeader, areaLsa);
      ScheduleUpdateL2ShortestPath ();
    }
}

void
OspfApp::ProcessL2SummaryLsa (LsaHeader lsaHeader, Ptr<L2SummaryLsa> l2SummaryLsa)
{
  if (!m_enableAreaProxy)
    {
      return;
    }
  
  NS_LOG_FUNCTION (this);
  uint32_t lsId = lsaHeader.GetLsId ();

  if (m_l2SummaryLsdb.find (lsId) == m_l2SummaryLsdb.end ())
    {
      m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, l2SummaryLsa);
      UpdateRouting ();
      return;
    }

  if (lsaHeader.GetSeqNum () > m_l2SummaryLsdb[lsId].first.GetSeqNum ())
    {
      m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, l2SummaryLsa);
      UpdateRouting ();
    }
  else if (lsaHeader.GetSeqNum () == m_l2SummaryLsdb[lsId].first.GetSeqNum () &&
           lsaHeader.GetAdvertisingRouter () < m_l2SummaryLsdb[lsId].first.GetAdvertisingRouter ())
    {
      m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, l2SummaryLsa);
      UpdateRouting ();
    }
}

void
OspfApp::AreaLeaderBegin ()
{
  NS_LOG_FUNCTION (this);
  std::cout << "Area Leader Begin " << m_areaId << ", " << m_routerId << std::endl;
  m_isAreaLeader = true;
  
  RecomputeAreaLsa ();
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
