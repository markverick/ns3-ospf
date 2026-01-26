/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_TEST_UTILS_H
#define OSPF_TEST_UTILS_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"

#include "ns3/ospf-app-helper.h"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>

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
  std::istringstream iss (table);
  for (std::string line; std::getline (iss, line);)
    {
      // Ipv4StaticRouting table lines begin with the destination.
      if (line.rfind (dst, 0) == 0 && line.find (gw) != std::string::npos)
        {
          return true;
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
  std::istringstream iss (table);
  for (std::string line; std::getline (iss, line);)
    {
      if (line.rfind (dst, 0) == 0)
        {
          return true;
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
