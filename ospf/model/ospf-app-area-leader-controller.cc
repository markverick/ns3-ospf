/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-area-leader-controller.h"

#include "ospf-app-private.h"

namespace ns3 {

OspfAreaLeaderController::OspfAreaLeaderController (OspfApp &app)
  : m_app (app)
{
}

void
OspfAreaLeaderController::ScheduleInitialLeadershipAttempt ()
{
  m_app.m_isAreaLeader = false;
  m_app.m_areaLeaderBeginTimer =
  Simulator::Schedule (m_app.m_routerDeadInterval + MilliSeconds (m_app.m_jitterRv->GetValue ()),
                           &OspfApp::AreaLeaderBegin, &m_app);
}

void
OspfAreaLeaderController::UpdateLeadershipEligibility ()
{
  // Start leadership begin timer if it's a leader (lowest router ID)
  if (m_app.m_routerLsdb.begin ()->first == m_app.m_routerId.Get ())
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
  std::cout << "Area Leader Begin " << m_app.m_areaId << ", " << m_app.m_routerId << std::endl;
  m_app.m_isAreaLeader = true;
  // Area Leader Logic -- start flooding Area-LSA and Summary-LSA-Area
  // Flood LSU with Area-LSAs to all interfaces
  m_app.RecomputeAreaLsa ();

  // Flood Area Summary LSA for L2 routing prefix
  m_app.RecomputeL2SummaryLsa ();
}

void
OspfAreaLeaderController::End ()
{
  NS_LOG_FUNCTION (&m_app);
  m_app.m_isAreaLeader = false;
  // TODO: Area Leader Logic -- stop flooding Area-LSA and Summary-LSA-Area
}

} // namespace ns3
