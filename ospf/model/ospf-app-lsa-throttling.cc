/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"

namespace ns3 {

OspfApp::LsaThrottleStats
OspfApp::GetLsaThrottleStats () const
{
  LsaThrottleStats stats;
  stats.recomputeTriggers = m_lsaThrottleRecomputeTriggers;
  stats.immediate = m_lsaThrottleImmediate;
  stats.deferredScheduled = m_lsaThrottleDeferredScheduled;
  stats.suppressed = m_lsaThrottleSuppressed;
  stats.cancelledPending = m_lsaThrottleCancelledPending;
  return stats;
}

void
OspfApp::ResetLsaThrottleStats ()
{
  m_lsaThrottleRecomputeTriggers = 0;
  m_lsaThrottleImmediate = 0;
  m_lsaThrottleDeferredScheduled = 0;
  m_lsaThrottleSuppressed = 0;
  m_lsaThrottleCancelledPending = 0;
}

Time
OspfApp::GetLsaThrottleDelay (const LsaHeader::LsaKey &lsaKey)
{
  if (m_minLsInterval.IsZero ())
    {
      return Time (0);
    }

  auto lastIt = m_lastLsaOriginationTime.find (lsaKey);
  if (lastIt != m_lastLsaOriginationTime.end ())
    {
      Time elapsed = Simulator::Now () - lastIt->second;
      if (elapsed < m_minLsInterval)
        {
          return m_minLsInterval - elapsed;
        }
    }
  return Time (0);
}

void
OspfApp::CleanupThrottleEvent (const LsaHeader::LsaKey &lsaKey)
{
  auto it = m_pendingLsaRegeneration.find (lsaKey);
  if (it != m_pendingLsaRegeneration.end () && !it->second.IsRunning ())
    {
      m_pendingLsaRegeneration.erase (it);
    }
}

// Wrapper methods for bool-returning functions (needed for ns-3.35 Simulator::Schedule)
void
OspfApp::RecomputeAreaLsaWrapper ()
{
  RecomputeAreaLsa ();
}

void
OspfApp::RecomputeL2SummaryLsaWrapper ()
{
  RecomputeL2SummaryLsa ();
}

void
OspfApp::ThrottledRecomputeRouterLsa ()
{
  NS_LOG_FUNCTION (this);
  auto lsaKey =
      std::make_tuple (LsaHeader::LsType::RouterLSAs, m_routerId.Get (), m_routerId.Get ());

  CleanupThrottleEvent (lsaKey);
  if (m_enableLsaThrottleStats)
    {
      ++m_lsaThrottleRecomputeTriggers;
    }
  Time delay = GetLsaThrottleDelay (lsaKey);

  if (delay.IsZero ())
    {
      auto pendingIt = m_pendingLsaRegeneration.find (lsaKey);
      if (pendingIt != m_pendingLsaRegeneration.end ())
        {
          Simulator::Cancel (pendingIt->second);
          m_pendingLsaRegeneration.erase (pendingIt);
          if (m_enableLsaThrottleStats)
            {
              ++m_lsaThrottleCancelledPending;
            }
        }
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleImmediate;
        }
      RecomputeRouterLsa ();
    }
  else if (m_pendingLsaRegeneration.find (lsaKey) == m_pendingLsaRegeneration.end ())
    {
      NS_LOG_INFO ("Router-LSA throttled, deferring by " << delay.As (Time::MS));
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleDeferredScheduled;
        }
      m_pendingLsaRegeneration[lsaKey] =
          Simulator::Schedule (delay, &OspfApp::RecomputeRouterLsa, this);
    }
  else
    {
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleSuppressed;
        }
    }
}

void
OspfApp::ThrottledRecomputeL1SummaryLsa ()
{
  NS_LOG_FUNCTION (this);
  auto lsaKey =
      std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, m_routerId.Get (), m_routerId.Get ());

  CleanupThrottleEvent (lsaKey);
  if (m_enableLsaThrottleStats)
    {
      ++m_lsaThrottleRecomputeTriggers;
    }
  Time delay = GetLsaThrottleDelay (lsaKey);

  if (delay.IsZero ())
    {
      auto pendingIt = m_pendingLsaRegeneration.find (lsaKey);
      if (pendingIt != m_pendingLsaRegeneration.end ())
        {
          Simulator::Cancel (pendingIt->second);
          m_pendingLsaRegeneration.erase (pendingIt);
          if (m_enableLsaThrottleStats)
            {
              ++m_lsaThrottleCancelledPending;
            }
        }
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleImmediate;
        }
      RecomputeL1SummaryLsa ();
    }
  else if (m_pendingLsaRegeneration.find (lsaKey) == m_pendingLsaRegeneration.end ())
    {
      NS_LOG_INFO ("L1Summary-LSA throttled, deferring by " << delay.As (Time::MS));
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleDeferredScheduled;
        }
      m_pendingLsaRegeneration[lsaKey] =
          Simulator::Schedule (delay, &OspfApp::RecomputeL1SummaryLsa, this);
    }
  else
    {
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleSuppressed;
        }
    }
}

void
OspfApp::ThrottledRecomputeAreaLsa ()
{
  NS_LOG_FUNCTION (this);
  auto lsaKey = std::make_tuple (LsaHeader::LsType::AreaLSAs, m_areaId, m_routerId.Get ());

  CleanupThrottleEvent (lsaKey);
  if (m_enableLsaThrottleStats)
    {
      ++m_lsaThrottleRecomputeTriggers;
    }
  Time delay = GetLsaThrottleDelay (lsaKey);

  if (delay.IsZero ())
    {
      auto pendingIt = m_pendingLsaRegeneration.find (lsaKey);
      if (pendingIt != m_pendingLsaRegeneration.end ())
        {
          Simulator::Cancel (pendingIt->second);
          m_pendingLsaRegeneration.erase (pendingIt);
          if (m_enableLsaThrottleStats)
            {
              ++m_lsaThrottleCancelledPending;
            }
        }
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleImmediate;
        }
      RecomputeAreaLsa ();
    }
  else if (m_pendingLsaRegeneration.find (lsaKey) == m_pendingLsaRegeneration.end ())
    {
      NS_LOG_INFO ("Area-LSA throttled, deferring by " << delay.As (Time::MS));
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleDeferredScheduled;
        }
      m_pendingLsaRegeneration[lsaKey] =
          Simulator::Schedule (delay, &OspfApp::RecomputeAreaLsaWrapper, this);
    }
  else
    {
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleSuppressed;
        }
    }
}

void
OspfApp::ThrottledRecomputeL2SummaryLsa ()
{
  NS_LOG_FUNCTION (this);
  auto lsaKey = std::make_tuple (LsaHeader::LsType::L2SummaryLSAs, m_areaId, m_routerId.Get ());

  CleanupThrottleEvent (lsaKey);
  if (m_enableLsaThrottleStats)
    {
      ++m_lsaThrottleRecomputeTriggers;
    }
  Time delay = GetLsaThrottleDelay (lsaKey);

  if (delay.IsZero ())
    {
      auto pendingIt = m_pendingLsaRegeneration.find (lsaKey);
      if (pendingIt != m_pendingLsaRegeneration.end ())
        {
          Simulator::Cancel (pendingIt->second);
          m_pendingLsaRegeneration.erase (pendingIt);
          if (m_enableLsaThrottleStats)
            {
              ++m_lsaThrottleCancelledPending;
            }
        }
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleImmediate;
        }
      RecomputeL2SummaryLsa ();
    }
  else if (m_pendingLsaRegeneration.find (lsaKey) == m_pendingLsaRegeneration.end ())
    {
      NS_LOG_INFO ("L2Summary-LSA throttled, deferring by " << delay.As (Time::MS));
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleDeferredScheduled;
        }
      m_pendingLsaRegeneration[lsaKey] =
          Simulator::Schedule (delay, &OspfApp::RecomputeL2SummaryLsaWrapper, this);
    }
  else
    {
      if (m_enableLsaThrottleStats)
        {
          ++m_lsaThrottleSuppressed;
        }
    }
}

} // namespace ns3
