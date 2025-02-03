/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
MIT License

Copyright (c) 2025 Sirapop Theeranantachai

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
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
 * \ingroup udpecho
 * \brief Create a server application which waits for input UDP packets
 *        and sends them back to the original sender.
 */
class OspfAppHelper
{
public:
  /**
   * Create OspfAppHelper which will make life easier for people trying
   * to set up simulations with echos.
   *
   * \param port The port the server will wait on for incoming packets
   */
  OspfAppHelper (uint16_t port);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * \param c The nodes on which to create the Applications.  The nodes
   *          are specified by a NodeContainer.
   *
   * Create one udp echo server application on each of the Nodes in the
   * NodeContainer.
   *
   * \returns The applications created, one Application per Node in the 
   *          NodeContainer.
   */
  ApplicationContainer Install (Ptr<Node> n) const;
  ApplicationContainer Install (Ptr<Node> n, std::vector<uint32_t> areas) const;
  ApplicationContainer Install (Ptr<Node> n, std::vector<uint32_t> areas, std::vector<uint32_t> metrices) const;
  ApplicationContainer Install (NodeContainer c) const;
  ApplicationContainer Install (NodeContainer c, std::vector<uint32_t> areas) const;
  ApplicationContainer Install (NodeContainer c, std::vector<uint32_t> areas, std::vector<uint32_t> metrices) const;

private:
  /**
   * Install an ns3::UdpEchoServer on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an UdpEchoServer will be installed.
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs) const;

  Ptr<Application> InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs, std::vector<uint32_t> areas) const;

  Ptr<Application> InstallPriv (Ptr<Node> node, Ptr<Ipv4StaticRouting> routing, NetDeviceContainer devs, std::vector<uint32_t> areas, std::vector<uint32_t> metrices) const;

  ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* OSPF_APP_HELPER_H */
