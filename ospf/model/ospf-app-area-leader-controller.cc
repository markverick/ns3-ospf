/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-area-leader-controller.h"

#include "ospf-app-private.h"

namespace ns3 {

bool
OspfAreaLeaderController::ShouldUseStaticLeader () const
{
  return m_app.m_areaLeaderMode == OspfApp::AREA_LEADER_STATIC;
}

bool
OspfAreaLeaderController::ShouldUseReachableLowestLeader () const
{
  return m_app.m_areaLeaderMode == OspfApp::AREA_LEADER_REACHABLE_LOWEST_ROUTER_ID;
}

bool
OspfAreaLeaderController::HasLeadershipQuorum () const
{
  if (!ShouldUseReachableLowestLeader ())
    {
      return true;
    }

  const uint32_t knownRouters =
      std::max (1u, static_cast<uint32_t> (m_app.m_routerLsdb.size ())); 
  const uint32_t reachableRouters = 1u + static_cast<uint32_t> (m_app.m_l1NextHop.size ());
  const uint32_t quorum = knownRouters / 2u + 1u;
  return reachableRouters >= quorum;
}

bool
OspfAreaLeaderController::IsStaticLeader () const
{
  return m_app.m_staticAreaLeaderRouterId != Ipv4Address::GetZero () &&
         m_app.m_staticAreaLeaderRouterId == m_app.m_routerId;
}

uint32_t
OspfAreaLeaderController::SelectLeaderCandidateRouterId () const
{
  uint32_t candidate = m_app.m_routerId.Get ();

  if (ShouldUseReachableLowestLeader ())
    {
      for (const auto &[routerId, nextHop] : m_app.m_l1NextHop)
        {
          (void)nextHop;
          candidate = std::min (candidate, routerId);
        }
      return candidate;
    }

  if (m_app.m_routerLsdb.empty ())
    {
      return candidate;
    }

  return std::min (candidate, m_app.m_routerLsdb.begin ()->first);
}

OspfAreaLeaderController::OspfAreaLeaderController (OspfApp &app)
  : m_app (app)
{
}

void
OspfAreaLeaderController::ScheduleInitialLeadershipAttempt ()
{
  m_app.m_isAreaLeader = false;

  if (ShouldUseStaticLeader ())
    {
      if (IsStaticLeader ())
        {
          Begin ();
        }
      return;
    }

  m_app.m_areaLeaderBeginTimer =
  Simulator::Schedule (m_app.m_routerDeadInterval + MilliSeconds (m_app.m_jitterRv->GetValue ()),
                           &OspfApp::AreaLeaderBegin, &m_app);
}

void
OspfAreaLeaderController::UpdateLeadershipEligibility ()
{
  if (ShouldUseStaticLeader ())
    {
      if (m_app.m_areaLeaderBeginTimer.IsRunning ())
        {
          m_app.m_areaLeaderBeginTimer.Remove ();
        }

      if (IsStaticLeader ())
        {
          Begin ();
        }
      else if (m_app.m_isAreaLeader)
        {
          m_app.AreaLeaderEnd ();
        }
      return;
    }

  if (!HasLeadershipQuorum ())
    {
      if (m_app.m_areaLeaderBeginTimer.IsRunning ())
        {
          m_app.m_areaLeaderBeginTimer.Remove ();
        }
      if (m_app.m_isAreaLeader)
        {
          m_app.AreaLeaderEnd ();
        }
      return;
    }

  if (SelectLeaderCandidateRouterId () == m_app.m_routerId.Get ())
    {
      if (!m_app.m_isAreaLeader && !m_app.m_areaLeaderBeginTimer.IsRunning ())
        {
          m_app.m_areaLeaderBeginTimer = Simulator::Schedule (
              m_app.m_routerDeadInterval + MilliSeconds (m_app.m_jitterRv->GetValue ()),
              &OspfApp::AreaLeaderBegin, &m_app);
        }
    }
  else
    {
      if (m_app.m_areaLeaderBeginTimer.IsRunning ())
        {
          m_app.m_areaLeaderBeginTimer.Remove ();
        }
      if (m_app.m_isAreaLeader)
        {
          m_app.AreaLeaderEnd ();
        }
    }
}

void
OspfAreaLeaderController::Begin ()
{
  NS_LOG_FUNCTION (&m_app);
  if (m_app.m_isAreaLeader)
    {
      return;
    }

  m_app.m_isAreaLeader = true;
  m_app.RecomputeAreaLsa ();
  m_app.RecomputeL2SummaryLsa ();
}

void
OspfAreaLeaderController::End ()
{
  NS_LOG_FUNCTION (&m_app);
  if (!m_app.m_isAreaLeader)
    {
      return;
    }

  m_app.m_isAreaLeader = false;

  const auto areaLsaKey =
      std::make_tuple (LsaHeader::LsType::AreaLSAs, m_app.m_areaId, m_app.m_routerId.Get ());
  auto pendingArea = m_app.m_pendingLsaRegeneration.find (areaLsaKey);
  if (pendingArea != m_app.m_pendingLsaRegeneration.end ())
    {
      pendingArea->second.Cancel ();
      m_app.m_pendingLsaRegeneration.erase (pendingArea);
    }

  const auto summaryLsaKey = std::make_tuple (LsaHeader::LsType::L2SummaryLSAs,
                                              m_app.m_areaId,
                                              m_app.m_routerId.Get ());
  auto pendingSummary = m_app.m_pendingLsaRegeneration.find (summaryLsaKey);
  if (pendingSummary != m_app.m_pendingLsaRegeneration.end ())
    {
      pendingSummary->second.Cancel ();
      m_app.m_pendingLsaRegeneration.erase (pendingSummary);
    }

    Ptr<AreaLsa> emptyArea = Create<AreaLsa> ();
    Ptr<L2SummaryLsa> emptySummary = Create<L2SummaryLsa> ();
    m_app.OriginateAreaLsa (emptyArea);
    m_app.OriginateL2SummaryLsa (emptySummary);
}

} // namespace ns3
