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
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv4-static-routing-helper.h"

namespace ns3 {

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
    * \param n The node on which to create the Application.
    * \returns The application created.
   */
  ApplicationContainer Install (Ptr<Node> n) const;

    /**
    * Create one OSPF application on each node in the NodeContainer.
    *
    * \param c The nodes on which to create the Applications.
    * \returns The applications created, one per node.
    */
  ApplicationContainer Install (NodeContainer c) const;

  /**
   * Populate each node's reachable prefixes from its configured IPv4
   * point-to-point interfaces.
   *
   * This is intended for scenarios/tests that want nodes to advertise connected
   * interface networks without relying on Preload().
   */
  void ConfigureReachablePrefixesFromInterfaces (NodeContainer c) const;

  void Preload (NodeContainer c);

private:
  /**
  * Install an ns3::OspfApp on the node configured with all the
  * attributes set with SetAttribute().
   *
  * \param node The node on which an OspfApp will be installed.
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing,
                                NetDeviceContainer devs) const;

  ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* OSPF_APP_HELPER_H */
