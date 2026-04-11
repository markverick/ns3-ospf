/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-routing.h"

#include "ospf-app.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"

#include <limits>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfRouting");
NS_OBJECT_ENSURE_REGISTERED (OspfRouting);

TypeId
OspfRouting::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::OspfRouting")
          .SetParent<Ipv4RoutingProtocol> ()
          .SetGroupName ("Ospf")
          .AddConstructor<OspfRouting> ();
  return tid;
}

OspfRouting::OspfRouting ()
{
}

OspfRouting::~OspfRouting ()
{
}

void
OspfRouting::SetApp (Ptr<OspfApp> app)
{
  m_app = app;
}

bool
OspfRouting::IsActive () const
{
  return m_ipv4 != nullptr && m_app != nullptr && m_app->IsEnabled ();
}

bool
OspfRouting::MatchesOutputInterface (uint32_t ifIndex, Ptr<NetDevice> oif) const
{
  return oif == nullptr || m_ipv4->GetNetDevice (ifIndex) == oif;
}

void
OspfRouting::ConsiderCandidate (CandidateRoute &best, Ipv4Address network, Ipv4Mask mask,
                                Ipv4Address gateway, uint32_t ifIndex, uint32_t metric,
                                RouteClass routeClass) const
{
  const auto prefixLength = mask.GetPrefixLength ();
  const auto bestPrefixLength = best.mask.GetPrefixLength ();

  if (!best.found || prefixLength > bestPrefixLength ||
      (prefixLength == bestPrefixLength && routeClass < best.routeClass) ||
      (prefixLength == bestPrefixLength && routeClass == best.routeClass && metric < best.metric))
    {
      best.found = true;
      best.network = network;
      best.mask = mask;
      best.gateway = gateway;
      best.ifIndex = ifIndex;
      best.metric = metric;
      best.routeClass = routeClass;
    }
}

Ptr<Ipv4Route>
OspfRouting::Lookup (Ipv4Address destination, Ptr<NetDevice> oif) const
{
  NS_LOG_FUNCTION (this << destination << oif);

  if (!IsActive ())
    {
      return nullptr;
    }

  CandidateRoute best;

  if (destination.IsLocalMulticast ())
    {
      if (oif == nullptr)
        {
          return nullptr;
        }
      const auto interface = m_ipv4->GetInterfaceForDevice (oif);
      if (interface < 0)
        {
          return nullptr;
        }
      best.found = true;
      best.ifIndex = static_cast<uint32_t> (interface);
      best.metric = 0;
      best.routeClass = ROUTE_LOCAL;
    }

  for (const auto &[ifIndex, dest, mask, addr, metric] : m_app->m_externalRoutes)
    {
      (void)addr;
      const Ipv4Mask routeMask (mask);
      const Ipv4Address routeNetwork (dest);
      if (!routeMask.IsMatch (destination, routeNetwork) || !MatchesOutputInterface (ifIndex, oif))
        {
          continue;
        }
      ConsiderCandidate (best, routeNetwork, routeMask, Ipv4Address::GetZero (), ifIndex, metric,
                         ROUTE_LOCAL);
    }

  for (const auto &[remoteRouterId, nextHop] : m_app->m_l1NextHop)
    {
      auto l1It = m_app->m_l1SummaryLsdb.find (remoteRouterId);
      if (l1It == m_app->m_l1SummaryLsdb.end () || !MatchesOutputInterface (nextHop.ifIndex, oif))
        {
          continue;
        }

      for (const auto &route : l1It->second.second->GetRoutes ())
        {
          const Ipv4Mask routeMask (route.m_mask);
          const Ipv4Address routeNetwork = Ipv4Address (route.m_address).CombineMask (routeMask);
          if (!routeMask.IsMatch (destination, routeNetwork))
            {
              continue;
            }
          ConsiderCandidate (best, routeNetwork, routeMask, nextHop.ipAddress, nextHop.ifIndex,
                             nextHop.metric, ROUTE_L1);
        }
    }

  for (const auto &[remoteAreaId, l2NextHop] : m_app->m_l2NextHop)
    {
      if (remoteAreaId == m_app->m_areaId)
        {
          continue;
        }
      auto l2It = m_app->m_l2SummaryLsdb.find (remoteAreaId);
      auto borderIt = m_app->m_nextHopToShortestBorderRouter.find (l2NextHop.first);
      if (l2It == m_app->m_l2SummaryLsdb.end () ||
          borderIt == m_app->m_nextHopToShortestBorderRouter.end ())
        {
          continue;
        }

      const auto &nextHop = borderIt->second.second;
      if (!MatchesOutputInterface (nextHop.ifIndex, oif))
        {
          continue;
        }

      for (const auto &route : l2It->second.second->GetRoutes ())
        {
          const Ipv4Mask routeMask (route.m_mask);
          const Ipv4Address routeNetwork = Ipv4Address (route.m_address).CombineMask (routeMask);
          if (!routeMask.IsMatch (destination, routeNetwork))
            {
              continue;
            }

          const uint32_t totalMetric = nextHop.metric + l2NextHop.second + route.m_metric;
          ConsiderCandidate (best, routeNetwork, routeMask, nextHop.ipAddress, nextHop.ifIndex,
                             totalMetric, ROUTE_L2);
        }
    }

  if (!best.found)
    {
      return nullptr;
    }

  auto route = Create<Ipv4Route> ();
  route->SetDestination (destination);
  const auto target = best.gateway.IsAny () ? destination : best.gateway;
  route->SetSource (m_ipv4->SourceAddressSelection (best.ifIndex, target));
  route->SetGateway (best.gateway);
  route->SetOutputDevice (m_ipv4->GetNetDevice (best.ifIndex));
  return route;
}

Ptr<Ipv4Route>
OspfRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                          Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << p << header << oif << sockerr);

  auto route = Lookup (header.GetDestination (), oif);
  sockerr = route != nullptr ? Socket::ERROR_NOTERROR : Socket::ERROR_NOROUTETOHOST;
  return route;
}

bool
OspfRouting::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                         UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                         LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header << idev);

  if (m_ipv4 == nullptr)
    {
      return false;
    }

  const auto iif = m_ipv4->GetInterfaceForDevice (idev);
  NS_ASSERT (iif >= 0);

  if (header.GetDestination ().IsMulticast ())
    {
      return false;
    }

  if (m_ipv4->IsDestinationAddress (header.GetDestination (), static_cast<uint32_t> (iif)))
    {
      if (!lcb.IsNull ())
        {
          lcb (p, header, static_cast<uint32_t> (iif));
          return true;
        }
      return false;
    }

  if (!IsActive ())
    {
      return false;
    }

  if (!m_ipv4->IsForwarding (static_cast<uint32_t> (iif)))
    {
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }

  auto route = Lookup (header.GetDestination (), nullptr);
  if (route == nullptr)
    {
      return false;
    }

  ucb (route, p, header);
  return true;
}

void
OspfRouting::NotifyInterfaceUp (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
}

void
OspfRouting::NotifyInterfaceDown (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
}

void
OspfRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address.GetLocal ());
}

void
OspfRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address.GetLocal ());
}

void
OspfRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);
  NS_ASSERT (m_ipv4 == nullptr);
  m_ipv4 = ipv4;
}

void
OspfRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  (void)unit;
  auto os = stream->GetStream ();
  *os << "Node: " << (m_app != nullptr ? m_app->GetNode ()->GetId () : std::numeric_limits<uint32_t>::max ())
      << ", Time: " << Simulator::Now ().GetSeconds () << "s\n";
  *os << "OSPF two-step forwarding state\n";

  if (!IsActive ())
    {
      *os << "  inactive\n";
      return;
    }

  *os << "  connected prefixes\n";
  for (const auto &[ifIndex, dest, mask, addr, metric] : m_app->m_externalRoutes)
    {
      *os << "    " << Ipv4Address (dest) << "/" << Ipv4Mask (mask).GetPrefixLength ()
          << " if=" << ifIndex << " src=" << Ipv4Address (addr) << " metric=" << metric << "\n";
    }

  *os << "  intra-area summaries\n";
  for (const auto &[remoteRouterId, nextHop] : m_app->m_l1NextHop)
    {
      auto l1It = m_app->m_l1SummaryLsdb.find (remoteRouterId);
      if (l1It == m_app->m_l1SummaryLsdb.end ())
        {
          continue;
        }
      for (const auto &route : l1It->second.second->GetRoutes ())
        {
          *os << "    " << Ipv4Address (route.m_address).CombineMask (Ipv4Mask (route.m_mask))
              << "/" << Ipv4Mask (route.m_mask).GetPrefixLength () << " via router "
              << Ipv4Address (remoteRouterId) << " next-hop=" << nextHop.ipAddress
              << " if=" << nextHop.ifIndex << " metric=" << nextHop.metric << "\n";
        }
    }

  *os << "  inter-area summaries\n";
  for (const auto &[remoteAreaId, l2NextHop] : m_app->m_l2NextHop)
    {
      auto l2It = m_app->m_l2SummaryLsdb.find (remoteAreaId);
      auto borderIt = m_app->m_nextHopToShortestBorderRouter.find (l2NextHop.first);
      if (l2It == m_app->m_l2SummaryLsdb.end () || borderIt == m_app->m_nextHopToShortestBorderRouter.end ())
        {
          continue;
        }
      const auto &nextHop = borderIt->second.second;
      for (const auto &route : l2It->second.second->GetRoutes ())
        {
          *os << "    " << Ipv4Address (route.m_address).CombineMask (Ipv4Mask (route.m_mask))
              << "/" << Ipv4Mask (route.m_mask).GetPrefixLength () << " via area " << remoteAreaId
              << " border-next-hop=" << nextHop.ipAddress << " if=" << nextHop.ifIndex
              << " metric=" << (nextHop.metric + l2NextHop.second + route.m_metric) << "\n";
        }
    }
}

void
OspfRouting::DoDispose (void)
{
  m_app = nullptr;
  m_ipv4 = nullptr;
  Ipv4RoutingProtocol::DoDispose ();
}

} // namespace ns3
