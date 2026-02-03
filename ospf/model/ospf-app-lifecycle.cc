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

  if (m_enabled)
    {
      Enable ();
    }
  // Will begin as an area leader if noone will
}

void
OspfApp::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  Disable ();
  m_lsaTimingLog.close ();
}

void
OspfApp::Enable ()
{
  m_enabled = true;
  if (m_protocolRunning)
    {
      return;
    }
  m_protocolRunning = true;

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
}

void
OspfApp::Disable ()
{
  m_enabled = false;
  if (!m_protocolRunning)
    {
      return;
    }
  m_protocolRunning = false;

  StopInterfaceSync ();
  m_helloEvent.Remove ();
  CancelHelloTimeouts ();
  CloseSockets ();

  if (m_resetStateOnDisable)
    {
      ResetStateForRestart ();
    }
}

bool
OspfApp::IsEnabled () const
{
  return m_protocolRunning;
}

void
OspfApp::FlushOspfRoutes ()
{
  if (m_routing == nullptr)
    {
      return;
    }
  while (m_routing->GetNRoutes () > m_boundDevices.GetN ())
    {
      m_routing->RemoveRoute (m_boundDevices.GetN ());
    }
}

void
OspfApp::ResetStateForRestart ()
{
  // Cancel internal timers beyond socket-related timeouts.
  m_updateL1ShortestPathTimeout.Remove ();
  m_updateL2ShortestPathTimeout.Remove ();
  m_areaLeaderBeginTimer.Remove ();

  // Stop retransmissions and neighbor-specific scheduled work.
  for (auto &iface : m_ospfInterfaces)
    {
      if (iface == nullptr)
        {
          continue;
        }

      for (auto &nbr : iface->GetNeighbors ())
        {
          if (nbr == nullptr)
            {
              continue;
            }
          nbr->ClearKeyedTimeouts ();
          nbr->RemoveTimeout ();
          nbr->SetState (OspfNeighbor::Down);
        }
      iface->ClearNeighbors ();
    }

  FlushOspfRoutes ();

  m_isAreaLeader = false;
  m_seqNumbers.clear ();

  m_routerLsdb.clear ();
  m_l1SummaryLsdb.clear ();
  m_nextHopToShortestBorderRouter.clear ();
  m_advertisingPrefixes.clear ();
  m_l1NextHop.clear ();
  m_l1Addresses.clear ();

  m_areaLsdb.clear ();
  m_l2SummaryLsdb.clear ();
  m_l2NextHop.clear ();

  // Cancel pending LSA regeneration events and clear throttling state
  for (auto &pair : m_pendingLsaRegeneration)
    {
      if (pair.second.IsRunning ())
        {
          Simulator::Cancel (pair.second);
        }
    }
  m_pendingLsaRegeneration.clear ();
  m_lastLsaOriginationTime.clear ();

  m_doInitialize = true;
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
