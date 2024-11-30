/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2024 University of California, Los Angeles
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
 * Author: Sirapop Theeranantachai (stheera@g.ucla.edu)
 */

#ifndef OSPF_APP_H
#define OSPF_APP_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ospf-interface.h"
#include "unordered_map"
#include "queue"

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup applications 
 * \defgroup udpecho UdpEcho
 */

/**
 * \ingroup udpecho
 * \brief A Udp Echo server
 *
 * Every packet received is sent back.
 */
class OSPFApp : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  OSPFApp ();

  // Accessor for Static Routing
  void SetRouting(Ptr<Ipv4StaticRouting>);

  // Set net device to be bound for multicast address
  void SetBoundNetDevices (NetDeviceContainer devs);
  void SetBoundNetDevices (NetDeviceContainer devs, std::vector<uint32_t> areas);

  // Set router id
  void SetRouterId (Ipv4Address routerId);

  // Print LSDB
  void PrintLSDB();

  // Get LSDB hash (Lazy)
  uint32_t GetLSDBHash();
  void PrintLSDBHash();

  virtual ~OSPFApp ();

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  virtual void ScheduleTransmitHello (Time dt);
  virtual void SendHello ();
  virtual void FloodLSU (Ptr<Packet> p, uint32_t inputIfIndex);
  virtual void LinkDown (Ptr<OSPFInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp);
  virtual void LinkUp (Ptr<OSPFInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);

  bool IsDeviceAlive(uint32_t ifIndex);

  void HandleHello (uint32_t ifIndex, Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  void HandleLSU (uint32_t ifIndex, Ptr<Packet> packet);

  void UpdateRouting ();

  void RefreshHelloTimer(uint32_t ifIndex, Ptr<OSPFInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  void RefreshLSUTimer();

  uint16_t m_port; //!< Port on which we listen for incoming packets.
  std::vector<Ptr<Socket>> m_sockets; //!< IPv4 Socket
  Address m_local; //!< local multicast address
  EventId m_sendEvent; //!< Event to send the next packet

  // For OSPF
  // Attributes
  Ipv4Address m_routerId; // eth0
  NetDeviceContainer m_boundDevices;
  uint32_t m_areaId;

  // Randomization
  Ptr<UniformRandomVariable> m_randomVariable = CreateObject<UniformRandomVariable>();

  // Hello
  Time m_helloInterval; //!< Hello Interval
  Ipv4Address m_helloAddress; //!< Address of hello message
  std::vector<Time> m_lastHelloReceived;
  std::vector<EventId> m_helloTimeouts;
  Time m_neighborTimeout;

  // LSU
  Time m_lsuInterval;
  uint16_t m_ttl;
  Ptr<Ipv4StaticRouting> m_routing;
  std::vector<Ptr<OSPFInterface> > m_ospfInterfaces;
  EventId m_lsuTimeout;
  std::map<uint32_t, uint32_t> m_seqNumbers; 
  std::map<uint32_t, std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > > m_lsdb; // adjacency list of [routerId] -> subnet, mask, remoteRouterId

  // Routing

  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet> > m_txTrace;

  /// Callbacks for tracing the packet Rx events
  TracedCallback<Ptr<const Packet> > m_rxTrace;

  /// Callbacks for tracing the packet Tx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_txTraceWithAddresses;

  /// Callbacks for tracing the packet Rx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;

};

} // namespace ns3

#endif /* OSPF_APP_H */

