/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-area-leader-controller.h"
#include "ospf-app-logging.h"
#include "ospf-app-rng.h"
#include "ospf-app-sockets.h"

namespace ns3 {
void
OspfApp::DoDispose (void)
{
  // NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
OspfApp::InitializeLoggingIfEnabled ()
{
  m_logging->InitializeLoggingIfEnabled ();
}

void
OspfApp::InitializeRandomVariables ()
{
  m_rng->InitializeRandomVariables ();
}

void
OspfApp::InitializeSockets ()
{
  m_socketsMgr->InitializeSockets ();
}

void
OspfApp::CancelHelloTimeouts ()
{
  m_socketsMgr->CancelHelloTimeouts ();
}

void
OspfApp::CloseSockets ()
{
  m_socketsMgr->CloseSockets ();
}

void
OspfApp::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  InitializeLoggingIfEnabled ();
  InitializeRandomVariables ();
  InitializeSockets ();
  // Start sending Hello
  ScheduleTransmitHello (m_initialHelloDelay);

  if (m_doInitialize)
    {
      // Create AS External LSA from Router ID for L1 routing prefix
      RecomputeL1SummaryLsa ();
      // Process the new LSA and generate/flood L2 Summary LSA if needed
      ProcessLsa (m_l1SummaryLsdb[m_routerId.Get ()]);
      if (m_enableAreaProxy)
        {
          m_areaLeader->ScheduleInitialLeadershipAttempt ();
        }
    }
  else
    {
      UpdateL1ShortestPath ();
      UpdateL2ShortestPath ();
    }
  // Will begin as an area leader if noone will
}

void
OspfApp::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  CancelHelloTimeouts ();
  CloseSockets ();
  m_lsaTimingLog.close ();
}

void
OspfApp::ScheduleTransmitHello (Time dt)
{
  m_socketsMgr->ScheduleTransmitHello (dt);
}

} // namespace ns3
