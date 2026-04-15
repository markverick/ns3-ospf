/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_TEST_UTILS_H
#define OSPF_TEST_UTILS_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-header.h"
#include "ns3/lsa.h"
#include "ns3/ospf-dbd.h"
#include "ns3/ospf-header.h"

#include "ns3/ospf-app-helper.h"
#include "ns3/ospf-app.h"
#include "ns3/ospf-routing.h"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>

namespace ns3 {

class OspfAppTestPeer
{
public:
  static void ReceiveDbd (OspfApp &app, uint32_t ifIndex, Ipv4Address remoteIp,
                          Ipv4Address remoteRouterId, uint32_t area, Ptr<OspfDbd> dbd)
  {
    Ipv4Header ipHeader;
    ipHeader.SetSource (remoteIp);

    OspfHeader ospfHeader;
    ospfHeader.SetRouterId (remoteRouterId.Get ());
    ospfHeader.SetArea (area);

    app.HandleDbd (ifIndex, ipHeader, ospfHeader, dbd);
  }

  static void ProcessLsa (const Ptr<OspfApp> &app, LsaHeader lsaHeader, Ptr<Lsa> lsa)
  {
    app->ProcessLsa (lsaHeader, lsa);
  }

  static bool HasOspfInterface (const Ptr<OspfApp> &app, uint32_t ifIndex)
  {
    return app != nullptr && app->HasOspfInterface (ifIndex);
  }

  static Ipv4Address GetInterfaceGateway (const Ptr<OspfApp> &app, uint32_t ifIndex)
  {
    if (app == nullptr)
      {
        return Ipv4Address::GetZero ();
      }

    auto ospfIf = app->GetOspfInterface (ifIndex);
    return ospfIf != nullptr ? ospfIf->GetGateway () : Ipv4Address::GetZero ();
  }

  static void TrackPendingLsaRegeneration (const Ptr<OspfApp> &app,
                                           const LsaHeader::LsaKey &lsaKey,
                                           EventId event,
                                           Time lastOriginationTime)
  {
    if (app != nullptr)
      {
        app->m_pendingLsaRegeneration[lsaKey] = event;
        app->m_lastLsaOriginationTime[lsaKey] = lastOriginationTime;
      }
  }

  static bool HasPendingLsaRegeneration (const Ptr<OspfApp> &app,
                                         const LsaHeader::LsaKey &lsaKey)
  {
    return app != nullptr && app->m_pendingLsaRegeneration.find (lsaKey) !=
                                 app->m_pendingLsaRegeneration.end ();
  }
};

} // namespace ns3

namespace ns3::ospf_test_utils {

inline std::string
ReadAll (const std::filesystem::path &path)
{
  std::ifstream in (path);
  std::stringstream ss;
  ss << in.rdbuf ();
  return ss.str ();
}

inline bool
HasRouteLine (const std::string &table, const std::string &dst, const std::string &gw)
{
  std::vector<std::string> matchingOwners;
  std::istringstream iss (table);
  for (std::string line; std::getline (iss, line);)
    {
      const auto firstNonSpace = line.find_first_not_of (" \t");
      const auto trimmed = firstNonSpace == std::string::npos ? std::string () : line.substr (firstNonSpace);
      if (trimmed.rfind (dst, 0) == 0 && trimmed.find (gw) != std::string::npos)
        {
          return true;
        }
      if (trimmed.find ("next-hop=" + gw) != std::string::npos)
        {
          const auto ownerEnd = trimmed.find (' ');
          if (ownerEnd != std::string::npos)
            {
              matchingOwners.push_back (trimmed.substr (0, ownerEnd));
            }
        }
    }

  if (matchingOwners.empty ())
    {
      return false;
    }

  std::istringstream prefixStream (table);
  for (std::string line; std::getline (prefixStream, line);)
    {
      const auto firstNonSpace = line.find_first_not_of (" \t");
      const auto trimmed = firstNonSpace == std::string::npos ? std::string () : line.substr (firstNonSpace);
      if (trimmed.rfind (dst, 0) != 0)
        {
          continue;
        }

      for (const auto &owner : matchingOwners)
        {
          if (trimmed.find ("[" + owner + " metric=") != std::string::npos)
            {
              return true;
            }
        }
    }

  return false;
}

inline bool
HasRouteLineViaAnyGateway (const std::string &table, const std::string &dst,
                           const std::initializer_list<std::string> &gws)
{
  for (const auto &gw : gws)
    {
      if (HasRouteLine (table, dst, gw))
        {
          return true;
        }
    }
  return false;
}

inline bool
HasRouteDest (const std::string &table, const std::string &dst)
{
  std::vector<std::string> resolvedOwners;
  std::istringstream iss (table);
  for (std::string line; std::getline (iss, line);)
    {
      const auto firstNonSpace = line.find_first_not_of (" \t");
      const auto trimmed = firstNonSpace == std::string::npos ? std::string () : line.substr (firstNonSpace);
      if (trimmed.rfind (dst, 0) == 0 && trimmed.find ('[') == std::string::npos)
        {
          return true;
        }
      const auto ownerEnd = trimmed.find (' ');
      if (ownerEnd != std::string::npos)
        {
          const auto owner = trimmed.substr (0, ownerEnd);
          if (owner.rfind ("if=", 0) == 0 || owner.rfind ("router=", 0) == 0 ||
                owner.rfind ("area=", 0) == 0 || owner.rfind ("gw=", 0) == 0)
            {
              resolvedOwners.push_back (owner);
            }
        }
    }

  if (resolvedOwners.empty ())
    {
      return false;
    }

  std::istringstream prefixStream (table);
  for (std::string line; std::getline (prefixStream, line);)
    {
      const auto firstNonSpace = line.find_first_not_of (" \t");
      const auto trimmed = firstNonSpace == std::string::npos ? std::string () : line.substr (firstNonSpace);
      if (trimmed.rfind (dst, 0) != 0)
        {
          continue;
        }

      for (const auto &owner : resolvedOwners)
        {
          if (trimmed.find ("[" + owner + " metric=") != std::string::npos)
            {
              return true;
            }
        }
    }

  return false;
}

inline std::string
Ipv4ToString (Ipv4Address addr)
{
  std::ostringstream os;
  os << addr;
  return os.str ();
}

inline uint32_t
Ipv4IfIndex (Ptr<Node> node, Ptr<NetDevice> dev)
{
  NS_ABORT_MSG_IF (node == nullptr, "Ipv4IfIndex requires a node");
  NS_ABORT_MSG_IF (dev == nullptr, "Ipv4IfIndex requires a net device");

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  NS_ABORT_MSG_IF (ipv4 == nullptr, "Ipv4IfIndex requires an Ipv4 stack");

  const int32_t ifIndex = ipv4->GetInterfaceForDevice (dev);
  NS_ABORT_MSG_IF (ifIndex < 0, "NetDevice is not registered with the node Ipv4 stack");
  return static_cast<uint32_t> (ifIndex);
}

struct StaticRouteMatch
{
  Ipv4Address network;
  Ipv4Mask mask;
  Ipv4Address gateway;
  uint32_t interface;
};

inline std::optional<StaticRouteMatch>
FindStaticRoute (Ptr<Node> node, Ipv4Address network, Ipv4Mask mask)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4 == nullptr)
    {
      return std::nullopt;
    }

  Ipv4StaticRoutingHelper helper;
  Ptr<Ipv4StaticRouting> routing = helper.GetStaticRouting (ipv4);
  if (routing == nullptr)
    {
      return std::nullopt;
    }

  for (uint32_t i = 0; i < routing->GetNRoutes (); ++i)
    {
      Ipv4RoutingTableEntry entry = routing->GetRoute (i);
      if (entry.IsNetwork () && entry.GetDestNetwork () == network &&
          entry.GetDestNetworkMask () == mask)
        {
          return StaticRouteMatch{entry.GetDestNetwork (), entry.GetDestNetworkMask (),
                                  entry.GetGateway (), entry.GetInterface ()};
        }
    }

  return std::nullopt;
}

inline Ptr<OspfApp>
GetOspfApp (Ptr<Node> node)
{
  if (node == nullptr)
    {
      return nullptr;
    }

  for (uint32_t i = 0; i < node->GetNApplications (); ++i)
    {
      auto app = DynamicCast<OspfApp> (node->GetApplication (i));
      if (app != nullptr)
        {
          return app;
        }
    }

  return nullptr;
}

inline Ptr<OspfRouting>
GetOspfRouting (Ptr<Node> node)
{
  if (node == nullptr)
    {
      return nullptr;
    }

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4 == nullptr)
    {
      return nullptr;
    }

  return Ipv4RoutingHelper::GetRouting<OspfRouting> (ipv4->GetRoutingProtocol ());
}

inline Ptr<Ipv4Route>
LookupOspfRoute (Ptr<Node> node, Ipv4Address destination, Ptr<NetDevice> oif = nullptr,
                 Socket::SocketErrno *sockerrOut = nullptr)
{
  auto routing = GetOspfRouting (node);
  if (routing == nullptr)
    {
      if (sockerrOut != nullptr)
        {
          *sockerrOut = Socket::ERROR_NOROUTETOHOST;
        }
      return nullptr;
    }

  Ipv4Header header;
  header.SetDestination (destination);
  Socket::SocketErrno sockerr = Socket::ERROR_NOTERROR;
  auto route = routing->RouteOutput (Create<Packet> (), header, oif, sockerr);
  if (sockerrOut != nullptr)
    {
      *sockerrOut = sockerr;
    }
  return route;
}

inline void
ConfigureFastColdStart (OspfAppHelper &ospf)
{
  ospf.SetAttribute ("HelloAddress", Ipv4AddressValue (Ipv4Address ("224.0.0.5")));
  ospf.SetAttribute ("ShortestPathUpdateDelay", TimeValue (MilliSeconds (50)));

  // Fast timings for test runtime (cold start).
  ospf.SetAttribute ("InitialHelloDelay", TimeValue (Seconds (0)));
  ospf.SetAttribute ("HelloInterval", TimeValue (MilliSeconds (200)));
  ospf.SetAttribute ("RouterDeadInterval", TimeValue (MilliSeconds (600)));
  ospf.SetAttribute ("LSUInterval", TimeValue (MilliSeconds (500)));
}

} // namespace ns3::ospf_test_utils

#endif // OSPF_TEST_UTILS_H
