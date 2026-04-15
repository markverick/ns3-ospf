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
OspfRouting::IsInterfaceUsable (uint32_t ifIndex) const
{
  return m_ipv4 != nullptr && ifIndex < m_ipv4->GetNInterfaces () && m_ipv4->IsUp (ifIndex) &&
         m_ipv4->GetNetDevice (ifIndex) != nullptr;
}

bool
OspfRouting::MatchesOutputInterface (uint32_t ifIndex, Ptr<NetDevice> oif) const
{
  return IsInterfaceUsable (ifIndex) &&
         (oif == nullptr || m_ipv4->GetNetDevice (ifIndex) == oif);
}

Ptr<Ipv4Route>
OspfRouting::Lookup (Ipv4Address destination, Ptr<NetDevice> oif) const
{
  NS_LOG_FUNCTION (this << destination << oif);

  if (!IsActive ())
    {
      return nullptr;
    }

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
      if (!IsInterfaceUsable (static_cast<uint32_t> (interface)))
        {
          return nullptr;
        }
      auto route = Create<Ipv4Route> ();
      route->SetDestination (destination);
      route->SetSource (
          m_ipv4->SourceAddressSelection (static_cast<uint32_t> (interface), destination));
      route->SetGateway (Ipv4Address::GetZero ());
      route->SetOutputDevice (m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)));
      return route;
    }

  int32_t requiredIfIndex = -1;
  if (oif != nullptr)
    {
      requiredIfIndex = m_ipv4->GetInterfaceForDevice (oif);
      if (requiredIfIndex < 0)
        {
          return nullptr;
        }
    }

  OspfApp::ForwardingEntry entry;
  if (!m_app->LookupForwardingEntry (destination, requiredIfIndex, entry) ||
      !MatchesOutputInterface (entry.ifIndex, oif))
    {
      return nullptr;
    }

  auto route = Create<Ipv4Route> ();
  route->SetDestination (destination);
  const auto target = entry.nextHop.IsAny () ? destination : entry.nextHop;
  route->SetSource (m_ipv4->SourceAddressSelection (entry.ifIndex, target));
  route->SetGateway (entry.nextHop);
  route->SetOutputDevice (m_ipv4->GetNetDevice (entry.ifIndex));
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
  if (m_app != nullptr)
    {
      m_app->HandleLocalInterfaceEvent ();
    }
}

void
OspfRouting::NotifyInterfaceDown (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
  if (m_app != nullptr)
    {
      m_app->HandleLocalInterfaceEvent ();
    }
}

void
OspfRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address.GetLocal ());
  if (m_app != nullptr)
    {
      m_app->HandleLocalInterfaceEvent ();
    }
}

void
OspfRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address.GetLocal ());
  if (m_app != nullptr)
    {
      m_app->HandleLocalInterfaceEvent ();
    }
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

  m_app->PrintForwardingTable (*os);
}

void
OspfRouting::DoDispose (void)
{
  m_app = nullptr;
  m_ipv4 = nullptr;
  Ipv4RoutingProtocol::DoDispose ();
}

} // namespace ns3
