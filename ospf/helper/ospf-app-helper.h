/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Sirapop Theeranantachai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Sirapop Theeranantachaoi <stheera@g.ucla.edu>
 */
#ifndef OSPF_APP_HELPER_H
#define OSPF_APP_HELPER_H

#include <stdint.h>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "ns3/application-container.h"
#include "ns3/area-lsa.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"

namespace ns3 {

class OspfRouting;
class OspfApp;
class Ipv4;
class Ipv4InterfaceAddress;
class Lsa;
class LsaHeader;

/**
 * \ingroup ospf
 * \brief Helper for installing and configuring ns3::OspfApp instances.
 */
class OspfAppHelper
{
public:
  /**
   * Create an OspfAppHelper.
   */
  OspfAppHelper ();

  /**
   * Record an attribute to be set in each Application after it is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
    * Create one OSPF application on the given node.
   *
    * The app binds to the node's existing IPv4 interfaces. Devices that are not
    * already registered with the node's IPv4 stack are ignored by this overload.
    *
    * \param n The node on which to create the Application.
   * \returns The application created.
   */
    ApplicationContainer Install (Ptr<Node> n) const;

  /**
   * Create one OSPF application on the given node bound only to the selected
   * interfaces.
   *
    * The selected devices must already be registered with the node's IPv4
    * stack. They are resolved to the node's IPv4 interface indices and only
    * those interfaces participate in OSPF.
   *
   * \param n The node on which to create the Application.
   * \param devs Interfaces that OSPF should use on the node.
   * \returns The application created.
   */
  ApplicationContainer Install (Ptr<Node> n, NetDeviceContainer devs) const;

  /**
   * Create one OSPF application on each node in the NodeContainer using an
  * node's existing IPv4 interfaces.
   *
   * \param c The nodes on which to create the Applications.
   * \returns The applications created, one per node.
   */
  ApplicationContainer Install (NodeContainer c) const;

  /**
   * Populate each node's reachable prefixes from its configured IPv4
   * interfaces.
   *
   * This is intended for scenarios/tests that want nodes to advertise connected
   * interface networks without relying on Preload().
   */
  void ConfigureReachablePrefixesFromInterfaces (NodeContainer c) const;

  /**
   * Seed neighbors, self-originated LSAs, and area-proxy LSAs for deterministic
   * startup in point-to-point OSPF topologies.
   */
  void Preload (NodeContainer c);

private:
  struct PreloadNodeContext;
  struct PointToPointAdjacency;
  using LsaRecord = std::pair<LsaHeader, Ptr<Lsa>>;
  using AreaLsaSeedMap = std::map<uint32_t, std::vector<LsaRecord>>;

  static Ptr<OspfRouting> GetOrCreateRouting (Ptr<Node> node);
  static Ptr<OspfApp> FindOspfApp (const Ptr<Node> &node);
  static bool HasSelectedInterface (const Ptr<OspfApp> &app, uint32_t ifIndex);
  static bool SelectPrimaryInterfaceAddress (Ptr<Ipv4> ipv4, uint32_t ifIndex,
                                             Ipv4InterfaceAddress &out);
  static NetDeviceContainer CollectIpv4BoundDevices (Ptr<Ipv4> ipv4);
  static void PopulateSelectedReachableRoutes (const Ptr<OspfApp> &app, Ptr<Ipv4> ipv4);
  static std::vector<PreloadNodeContext> CollectPreloadNodes (NodeContainer c);
  static std::vector<PointToPointAdjacency> CollectPointToPointAdjacencies (
      const PreloadNodeContext &context);
  static void SeedRouterLsaAndNeighbors (const PreloadNodeContext &context,
                                         const std::vector<PointToPointAdjacency> &adjacencies,
                                         std::map<uint32_t, std::vector<AreaLink>> &areaAdj,
                                         AreaLsaSeedMap &lsaList);
  static bool SeedPreloadNode (const PreloadNodeContext &context,
                               std::map<uint32_t, std::set<uint32_t>> &areaMembers,
                               std::map<uint32_t, std::vector<AreaLink>> &areaAdj,
                               AreaLsaSeedMap &lsaList);
  static void ApplyPreloadedLsas (const std::vector<PreloadNodeContext> &preloadNodes,
                                  const std::map<uint32_t, std::set<uint32_t>> &areaMembers,
                                  const AreaLsaSeedMap &lsaList,
                                  const std::vector<LsaRecord> &proxiedLsaList);

  /**
    * Install an ns3::OspfApp on the node configured with all the
    * attributes set with SetAttribute().
   *
    * \param node The node on which an OspfApp will be installed.
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node, Ptr<OspfRouting> routing,
                                NetDeviceContainer devs) const;

  ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* OSPF_APP_HELPER_H */
