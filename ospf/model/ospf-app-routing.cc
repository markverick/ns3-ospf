/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {

void
OspfApp::UpdateRouting ()
{
  // The forwarding plane performs hierarchical lookup directly from OSPF state.
  // Recompute is still triggered here so callers keep the same control-plane flow.
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
