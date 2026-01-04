/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-area-leader-controller.h"
#include "ospf-app-logging.h"
#include "ospf-app-rng.h"
#include "ospf-app-sockets.h"

#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-address.h"

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

  StartInterfaceSyncIfEnabled ();
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

  StopInterfaceSync ();
  CancelHelloTimeouts ();
  CloseSockets ();
  m_lsaTimingLog.close ();
}

void
OspfApp::ScheduleTransmitHello (Time dt)
{
  m_socketsMgr->ScheduleTransmitHello (dt);
}

void
OspfApp::StartInterfaceSyncIfEnabled ()
{
  if (!m_autoSyncInterfaces)
    {
      return;
    }

  // Seed from current Ipv4 interfaces before sockets are created.
  SyncInterfacesFromIpv4 ();

  if (m_interfaceSyncInterval.IsZero ())
    {
      return;
    }
  m_interfaceSyncEvent = Simulator::Schedule (m_interfaceSyncInterval, &OspfApp::InterfaceSyncTick, this);
}

void
OspfApp::StopInterfaceSync ()
{
  m_interfaceSyncEvent.Remove ();
}

void
OspfApp::InterfaceSyncTick ()
{
  if (!m_autoSyncInterfaces)
    {
      return;
    }

  const bool changed = SyncInterfacesFromIpv4 ();
  if (changed)
    {
      // Rebind sockets to match the current interface set.
      CancelHelloTimeouts ();
      CloseSockets ();
      InitializeSockets ();

      // Restart hello scheduling against the new socket set.
      m_helloEvent.Remove ();
      ScheduleTransmitHello (MilliSeconds (0));
    }

  if (!m_interfaceSyncInterval.IsZero ())
    {
      m_interfaceSyncEvent =
          Simulator::Schedule (m_interfaceSyncInterval, &OspfApp::InterfaceSyncTick, this);
    }
}

} // namespace ns3
