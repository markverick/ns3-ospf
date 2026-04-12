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
  RefreshInterfaceReachableRoutesFromIpv4 ();
  InitializeSockets ();

  // Start sending Hello
  ScheduleTransmitHello (m_initialHelloDelay);

  if (m_doInitialize)
    {
      // Create AS External LSA from Router ID for L1 routing prefix
      RecomputeL1SummaryLsaAndProcessSelf ();
      if (m_enableAreaProxy)
        {
          m_areaLeader->ScheduleInitialLeadershipAttempt ();
        }
    }
  else
    {
      // Preloaded/imported instances still need to re-originate current self state once sockets are up.
      RecomputeRouterLsaAndProcessSelf ();
      RecomputeL1SummaryLsaAndProcessSelf ();
      RebuildPrefixOwnerTable ();
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
  // No flat OSPF routes are installed anymore. Forwarding stops automatically
  // when the app is disabled because the OSPF routing protocol checks IsEnabled().
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
  m_externalRoutes.clear ();
  m_injectedExternalRoutes.clear ();
  for (auto &bucket : m_prefixOwnersByLength)
    {
      bucket.clear ();
    }
  m_ownerToPrefixes.clear ();
  m_ownerResolutionTable.clear ();
  m_l1ShortestPathRunCount = 0;
  m_l2ShortestPathRunCount = 0;

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
  RefreshAllOspfInterfaceStateFromIpv4 ();

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

  const bool changed = RefreshAllOspfInterfaceStateFromIpv4 ();
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
