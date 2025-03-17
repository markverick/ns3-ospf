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
#include "ns3/ospf-header.h"
#include "ns3/ospf-dbd.h"
#include "ns3/ospf-hello.h"
#include "ns3/ls-ack.h"
#include "ns3/ls-update.h"
#include "ns3/lsa-header.h"
#include "ns3/router-lsa.h"
#include "ns3/as-external-lsa.h"
#include "ns3/area-lsa.h"
#include "ns3/summary-lsa.h"
#include "ospf-interface.h"
#include "unordered_map"
#include "queue"
#include "filesystem"

namespace ns3 {
/**
 * \ingroup applications 
 * \defgroup ospf Ospf
 */

/**
 * \ingroup ospf
 *
 * \brief An OSPF app, creates IP sockets and fills up routing table.
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

  /**
   * \brief Set a pointer to a routing table.
   * \param ipv4Routing Ipv4 routing table
   */
  void SetRouting (Ptr<Ipv4StaticRouting> ipv4Routing);

  /**
   * \brief Register network devices as OSPF interfaces.Abs
   * 
   * 0th index must be loopback TODO: make this more elegant #14
   * 
   * \param devs Net device container to beregisted
   */
  void SetBoundNetDevices (NetDeviceContainer devs);

  /**
   * \brief Set inteface areas.
   * \param areas a list of areas for each interface
   */
  void SetArea (uint32_t area, Ipv4Address address, Ipv4Mask mask);

  /**
   * \brief Set inteface metrices.
   * \param metrices Routing metrices to be registed
   */
  void SetMetrices (std::vector<uint32_t> metrices);

  /**
   * \brief Set router ID.
   * \param routerId Router ID of this router
   */
  void SetRouterId (Ipv4Address routerId);

  /**
   * \brief Get LSDB; only use for testing/debugging
   */
  std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>> GetLsdb ();
  /**
   * \brief Print Router LSDB
   */
  void PrintLsdb ();
  /**
   * \brief Print AS External LSDB
   */
  void PrintL1PrefixLsdb ();
  /**
   * \brief Print AreaLSDB
   */
  void PrintAreaLsdb ();
  /**
   * \brief Print SummaryLSDB
   */
  void PrintSummaryLsdb ();

  /**
   * \brief Print LSDB.
   * \param dirName directory name
   * \param dirName file name
   */
  void PrintRouting (std::filesystem::path dirName, std::string filename);

  /**
   * \brief Print interface areas.
   */
  void PrintAreas ();

  /**
   * \brief Get Router LSDB hash for comparison.
   * \return Router LSDB hash
   */
  uint32_t GetLsdbHash ();

  /**
   * \brief Get AS External LSDB hash for comparison.
   * \return Router LSDB hash
   */
  uint32_t GetL1PrefixLsdbHash ();

  /**
   * \brief Get Area LSDB hash for comparison.
   * \return Area LSDB hash
   */
  uint32_t GetAreaLsdbHash ();

  /**
   * \brief Get Summary LSDB hash for comparison.
   * \return Summary LSDB hash
   */
  uint32_t GetSummaryLsdbHash ();

  /**
   * \brief Print LSDB hash.
   */
  void PrintLsdbHash ();

  /**
   * \brief Print Area LSDB hash.
   */
  void PrintAreaLsdbHash ();

protected:
  virtual void DoDispose (void);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Helper to schedule hello transmission.
   * \param dt time to transmit
   */
  void ScheduleTransmitHello (Time dt);

  // Packet Helpers
  /**
   * \brief Send Hello message containing router IDs of where it has received Hello
   */
  void SendHello ();

  /**
   * \brief Send ACK.
   * \param ifIndex interface index
   * \param ackPacket Acknowledgement packet
   * \param remoteIp Destination IP address
   */
  void SendAck (uint32_t ifIndex, Ptr<Packet> ackPacket, Ipv4Address remoteIp);

  /**
   * \brief Send packet to neighbor via interface ifIndex.
   * \param ifIndex Interface index
   * \param packet Packet to sent
   * \param neighbor Neighbor to sent
   */
  void SendToNeighbor (uint32_t ifIndex, Ptr<Packet> packet, Ptr<OspfNeighbor> neighbor);

  /**
   * \brief Send packet to neighbor via interface ifIndex, retx enabled.
   * 
   * Only allow one timeout per neighbor
   * 
   * \param interval Retransmission interval
   * \param ifIndex Interface index
   * \param packet Packet to sent
   * \param neighbor Neighbor to sent
   */
  void SendToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                               Ptr<OspfNeighbor> neighbor);

  /**
   * \brief Send packet to neighbor via interface ifIndex, retx enabled.
   * 
   * Only allow one timeout per LSA key
   * 
   * \param interval Retransmission interval
   * \param ifIndex Interface index
   * \param packet Packet to sent
   * \param neighbor Neighbor to sent
   * \param lsaKey LSA unique identifier
   */
  void SendToNeighborKeyedInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                    Ptr<OspfNeighbor> neighbor, LsaHeader::LsaKey lsaKey);

  /**
   * \brief Flood LSUs to every interface except the incoming interface.
   * \param inputIfIndex Input interface ID
   * \param lsu LS Update packet
   */
  void FloodLsu (uint32_t inputIfIndex, Ptr<LsUpdate> lsu);

  // Packet Handler
  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);
  /**
   * \brief Handle a Hello packet.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param hello Hello payload
   */
  void HandleHello (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    Ptr<OspfHello> hello);
  /**
   * \brief Handle Database Description.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param dbd Database Description Payload
   */
  void HandleDbd (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<OspfDbd> dbd);
  /**
   * \brief Handle Negotiate DBD during ExStart.
   * \param ifIndex Interface index
   * \param neighbor OSPF Neighbor
   * \param dbd Database Description Payload
   */
  void HandleNegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);
  /**
   * \brief Handle DBD from Master during Exchange.
   * \param ifIndex Interface index
   * \param neighbor OSPF Neighbor
   * \param dbd Database Description Payload
   */
  void HandleMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);
  /**
   * \brief Handle DBD from Slave during Exchange.
   * \param ifIndex Interface index
   * \param neighbor OSPF Neighbor
   * \param dbd Database Description Payload
   */
  void HandleSlaveDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);
  /**
   * \brief Handle LS Request during Loading.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsr LS Request Payload
   */
  void HandleLsr (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsRequest> lsr);
  /**
   * \brief Handle LS Update during Loading and Full.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsu LS Update Payload
   */
  void HandleLsu (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsUpdate> lsu);

  /**
   * \brief Handle LSA.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsu LS Update Payload
   */
  void HandleLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, LsaHeader lsaHeader,
                  Ptr<Lsa> lsa);
  /**
   * \brief Process LSA.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsa LS Advertisement
   */

  // TODO: seperate processor as objects
  void ProcessLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                   LsaHeader lsaHeader, Ptr<Lsa> lsa);
  /**
   * \brief Process Router-LSA during Full.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsaHeader LSA Header
   * \param asExternalLsa AS External LSA Payload
   */
  void ProcessAsExternalLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                             LsaHeader lsaHeader, Ptr<AsExternalLsa> asExternalLsa);
  /**
   * \brief Process Router-LSA during Full.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsaHeader LSA Header
   * \param routerLsa Router LSA Payload
   */
  void ProcessRouterLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                         LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa);
  /**
   * \brief Process Area-LSA.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsaHeader LSA Header
   * \param areaLsa Area LSA Payload
   */
  void ProcessAreaLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                       LsaHeader lsaHeader, Ptr<AreaLsa> areaLsa);
  /**
   * \brief Process Summary-LSA (Area).
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsaHeader LSA Header
   * \param summaryLsa Area Summary LSA Payload
   */
  void ProcessAreaSummaryLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                              LsaHeader lsaHeader, Ptr<SummaryLsa> summaryLsa);
  /**
   * \brief Process LS Acknowledge as a response for LS Update during Full.
   * \param ifIndex Interface index
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsAck LS Acknowledge Payload
   */
  void HandleLsAck (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsAck> lsAck);

  // Link State Advertisement
  /**
   * \brief Generate local Router-LSA based on adjacencies (Full)
   * \return Router-LSA for this router
   */
  Ptr<AsExternalLsa> GetAsExternalLsa ();
  /**
   * \brief Generate local Router-LSA based on adjacencies (Full)
   * \return Router-LSA for this router
   */
  Ptr<RouterLsa> GetRouterLsa ();
  /**
   * \brief Generate local Area-LSA based on L2 adjacencies (Full)
   * \return Router-LSA for this router
   */
  Ptr<AreaLsa> GetAreaLsa ();
  /**
   * \brief Generate Summary Area-LSA for its area
   * \return Summary-LSA containing all prefixes in the areas
   */
  Ptr<SummaryLsa> GetSummaryLsa ();
  /**
   * \brief Generate local Router-LSA based on adjacencies (Full), filtered by areaId
   * \param areaId Area ID to filter
   * \return Router-LSA for this router
   */
  Ptr<RouterLsa> GetRouterLsa (uint32_t areaId);
  /**
   * \brief Recompute local Router-LSA, increment its Sequence Number, and inject to Router LSDB
   */
  void RecomputeRouterLsa ();
  /**
   * \brief Recompute local Area-LSA, increment its Sequence Number, and inject to Area LSDB
   */
  void RecomputeAreaLsa ();
  /**
   * \brief Recompute Area Summary-LSA, increment its Sequence Number, and inject to Area LSDB
   */
  void RecomputeSummaryLsa ();
  /**
   * \brief Update routing table based on shortest paths and prefixes
   */
  void UpdateRouting ();

  /**
   * \brief Update shortest paths and prefixes for L1
   */
  void UpdateL1ShortestPath ();

  /**
   * \brief Update shortest paths and prefixes for L2
   */
  void UpdateL2ShortestPath ();

  // Hello Protocol
  /**
   * \brief A timeout event for Hello, triggered after not receiving Hello for RouterDeadInterval
   * \param ospfInterface OSPF Interface
   * \param remoteRouterId Destination router ID
   * \param remoteIp Destination IP address
   */
  void HelloTimeout (uint32_t ifIndex, Ptr<OspfInterface> ospfInterface, Ipv4Address remoteRouterId,
                     Ipv4Address remoteIp);
  /**
   * \brief Refresh the Hello timeout, triggered after receiving Hello.
   * \param ifIndex Interface index
   * \param ospfInterface OSPF Interface TODO: Redundant
   * \param remoteRouterId Destination router ID
   * \param remoteIp Destination IP address
   */
  void RefreshHelloTimeout (uint32_t ifIndex, Ptr<OspfInterface> ospfInterface,
                            Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  // Down
  /**
   * \brief Move the neighbor state to Down.
   * 
   * Move the neighbor state to Down, flooding LSAs to update
   * its neighbors
   * 
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void AdvanceToDown (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  // ExStart
  /**
   * \brief Negotiate for Master/Slave, sending DBDs with no LSA header.
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   * \param bitMS true if it wants to be Master
   */
  void NegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, bool bitMS);

  // Exchange
  /**
   * \brief Master sends its DBD to Slave, retx enabled.
   * 
   * Only one outstanding DBD is allowed. The next DBD is sent
   * upon receiving DBD from slave as implicit acknowledgement.
   * 
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void PollMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  // Loading
  /**
   * \brief Move the neighbor state to Loading.
   * 
   * Move the neighbor state to Loading, sending LS Request
   * for any outdated or missing LSAs.
   * 
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void AdvanceToLoading (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  /**
   * \brief Compare and start sending LS Requests.
   * 
   * Compare the LSDB with its neighbor, and send the first LS Request.
   * 
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void CompareAndSendLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  /**
   * \brief Send the next LSR or advance to Full if the queue is empty.
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void SendNextLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  // Full
  /**
   * \brief LSDBs are synchronized, advancing to Full.
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void AdvanceToFull (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  std::vector<Ptr<Socket>> m_sockets; //!< Unicast socket
  std::vector<Ptr<Socket>> m_helloSockets; //!< Hello multicast socket
  std::vector<Ptr<Socket>> m_lsaSockets; //!< LSA multicast socket
  Address m_local; //!< local multicast address

  // Area
  /**
   * \brief Begin as an area leader
   */
  void AreaLeaderBegin ();

  /**
   * \brief End a role as an area leader
   */
  void AreaLeaderEnd ();

  // For OSPF
  // Attributes
  Ipv4Address m_routerId;
  Ipv4Mask m_areaMask; // Area masks
  NetDeviceContainer m_boundDevices;
  uint32_t m_areaId; // Only used for default value and for alt area and

  // Randomization
  // For a small time jitter
  Ptr<UniformRandomVariable> m_randomVariable = CreateObject<UniformRandomVariable> ();
  // For DD Sequence Number
  Ptr<UniformRandomVariable> m_randomVariableSeq = CreateObject<UniformRandomVariable> ();

  // Hello
  Time m_helloInterval; //!< Hello Interval
  Ipv4Address m_helloAddress; //!< Address of multicast hello message
  std::vector<Time> m_lastHelloReceived; //!< Times of last hello received
  std::vector<EventId> m_helloTimeouts; //!< Timeout Events of not receiving Hello
  Time m_routerDeadInterval; //!< Router Dead Interval for Hello to become Down
  EventId m_helloEvent; //!< Event to send the next hello packet

  // Interface
  std::vector<Ptr<OspfInterface>> m_ospfInterfaces; // !< Router interfaces

  // Routing
  Ptr<Ipv4StaticRouting> m_routing; // !< Routing table
  std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>
      m_l1NextHop; //!< <next hop, distance> to routers
  std::unordered_map<uint32_t, std::vector<uint32_t>> m_l1Addresses; //!< Addresses for L1 routers

  // Area
  std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>
      m_l2NextHop; //!< <next hop, distance> to areas
  bool m_isAreaLeader;

  // LSA
  bool m_enableAreaProxy; // True if Proxied L2 LSAs are generated
  Time m_rxmtInterval; // retransmission timer
  EventId m_areaLeaderBeginTimer; // area leadership begin timer
  Ipv4Address m_lsaAddress; //!< multicast address for LSA
  std::map<LsaHeader::LsaKey, uint16_t> m_seqNumbers; // sequence number of stored LSA

  // L1 LSDB
  std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>>
      m_routerLsdb; // LSDB for each remote router ID
  std::map<uint32_t, std::pair<LsaHeader, Ptr<AsExternalLsa>>>
      m_asExternalLsdb; // LSDB for each remote router ID
  std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>
      m_nextHopToShortestBorderRouter; // <next hop, metric>

  // L2 LSDB
  std::map<uint32_t, std::pair<LsaHeader, Ptr<AreaLsa>>> m_areaLsdb; // LSDB for each remote area ID
  std::map<uint32_t, std::pair<LsaHeader, Ptr<SummaryLsa>>>
      m_summaryLsdb; // LSDB for summary prefixes

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
