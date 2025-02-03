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

#ifndef OSPF_APP_H
#define OSPF_APP_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/random-variable-stream.h"
#include "ospf-header.h"
#include "lsa-header.h"
#include "router-lsa.h"
#include "ospf-dbd.h"
#include "ospf-hello.h"
#include "ls-ack.h"
#include "ls-update.h"
#include "ospf-interface.h"
#include "unordered_map"
#include "queue"
#include "filesystem"

namespace ns3 {
/**
 * \ingroup applications 
 * \defgroup ospfapp OspfApp
 */

/**
 * \ingroup ospfapp
 *
 * \brief An OSPF app, creates IP sockets and fills up routing table
 */
class OspfApp : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  OspfApp ();
  virtual ~OspfApp ();

  // Accessor for Static Routing
  void SetRouting (Ptr<Ipv4StaticRouting>);

  // Set net device to be bound for multicast address
  void SetBoundNetDevices (NetDeviceContainer devs);

  // Set interface area
  void SetAreas (std::vector<uint32_t> areas);

  // Set interface metrices
  void SetMetrices (std::vector<uint32_t> metrices);

  // Add neighbor to an existing interface (for multiaccess networks)
  void AddInterfaceNeighbor (uint32_t ifIndex, Ipv4Address destIp, Ipv4Address nextHopIp);

  // Set router id
  void SetRouterId (Ipv4Address routerId);

  // Print LSDB
  void PrintLsdb ();

  // Print Routing Table
  void PrintRouting (std::filesystem::path dirName, std::string filename);

  // Print Interface Areas
  void PrintAreas ();

  // Get LSDB hash (Lazy)
  uint32_t GetLsdbHash ();
  void PrintLsdbHash ();

protected:
  virtual void DoDispose (void);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTransmitHello (Time dt);

  // Packet Helpers
  void SendHello ();
  void SendAck (uint32_t ifIndex, Ptr<Packet> ackPacket, Ipv4Address remoteIp);
  void SendToNeighbor (uint32_t ifIndex, Ptr<Packet> packet, Ptr<OspfNeighbor> neighbor);
  void SendToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                               Ptr<OspfNeighbor> neighbor);
  void SendLsuToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                  Ptr<OspfNeighbor> neighbor, LsaHeader::LsaKey lsaKey);
  void FloodLsu (uint32_t inputIfIndex, Ptr<LsUpdate> lsu);

  // Timeouts
  void LsuTimeout (uint32_t ifIndex, Ptr<Packet> lsuPacket, uint32_t flags,
                   std::tuple<uint8_t, uint32_t, uint32_t> lsaKey, Ipv4Address toAddress);
  void HelloTimeout (Ptr<OspfInterface> ospfInterface, Ipv4Address remoteRouterId,
                     Ipv4Address remoteIp);

  // Packet Handlers
  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);

  void HandleHello (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    Ptr<OspfHello> hello);

  void HandleDbd (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<OspfDbd> dbd);
  void HandleNegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);
  void HandleMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);
  void HandleSlaveDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);

  void HandleLsr (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsRequest> lsr);

  void HandleLsu (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsUpdate> lsu);

  void HandleRouterLsu (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                        LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa);

  void HandleLsAck (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsAck> lsAck);

  // LSA
  Ptr<RouterLsa> GetRouterLsa (); // Generate local Router-LSA based on adjacencies
  Ptr<RouterLsa> GetRouterLsa (uint32_t areaId);
  void RecomputeRouterLsa (); // Recompute local Router-LSA and inject to LSDB

  void UpdateRouting (); // Update routing table based on LSDB

  // Hello
  void RefreshHelloTimeout (uint32_t ifIndex, Ptr<OspfInterface> ospfInterface,
                            Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  // ExStart
  void NegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, bool bitMS);

  // Exchange
  void SendMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void PollMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  // Loading
  void AdvanceToLoading (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void CompareAndSendLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void SendNextLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void SatisfyLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  // Full
  void AdvanceToFull (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  std::vector<Ptr<Socket>> m_sockets; //!< Unicast socket
  std::vector<Ptr<Socket>> m_helloSockets; //!< Hello multicast socket
  std::vector<Ptr<Socket>> m_lsaSockets; //!< LSA multicast socket
  Address m_local; //!< local multicast address

  // For OSPF
  // Attributes
  Ipv4Address m_routerId; // eth0
  NetDeviceContainer m_boundDevices;
  uint32_t m_areaId; // Only used for default value and for alt area and

  // Randomization
  Ptr<UniformRandomVariable> m_randomVariable = CreateObject<UniformRandomVariable> ();
  Ptr<UniformRandomVariable> m_randomVariableSeq = CreateObject<UniformRandomVariable> ();

  // Hello
  Time m_helloInterval; //!< Hello Interval
  Ipv4Address m_helloAddress; //!< Address of multicast hello message
  std::vector<Time> m_lastHelloReceived;
  std::vector<EventId> m_helloTimeouts;
  Time m_routerDeadInterval;
  EventId m_helloEvent; //!< Event to send the next hello packet

  // Interface
  std::vector<Ptr<OspfInterface>> m_ospfInterfaces; // router interfaces

  // Routing
  Ptr<Ipv4StaticRouting> m_routing; // routing table

  // LSA
  Time m_rxmtInterval; // retransmission timer
  Ipv4Address m_lsaAddress; //!< multicast address for LSA
  std::map<LsaHeader::LsaKey, uint16_t> m_seqNumbers; // sequence number of stored LSA
  std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>>
      m_routerLsdb; // LSDB for each remote router ID

  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet>> m_txTrace;

  /// Callbacks for tracing the packet Rx events
  TracedCallback<Ptr<const Packet>> m_rxTrace;

  /// Callbacks for tracing the packet Tx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_txTraceWithAddresses;

  /// Callbacks for tracing the packet Rx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;
};

} // namespace ns3

#endif /* OSPF_APP_H */
