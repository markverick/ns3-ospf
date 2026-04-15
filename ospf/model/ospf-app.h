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
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/net-device-container.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ospf-header.h"
#include "ns3/ospf-dbd.h"
#include "ns3/ospf-hello.h"
#include "ns3/ls-ack.h"
#include "ns3/ls-update.h"
#include "ns3/lsa-header.h"
#include "ns3/router-lsa.h"
#include "ns3/l1-summary-lsa.h"
#include "ns3/area-lsa.h"
#include "ns3/l2-summary-lsa.h"
#include "next-hop.h"
#include "ospf-routing.h"
#include "ospf-interface.h"
#include "unordered_map"
#include "queue"
#include "filesystem"

#include <array>
#include <iosfwd>
#include <memory>

namespace ns3 {
class OspfAppIo;
class OspfNeighborFsm;
class OspfLsaProcessor;
class OspfStateSerializer;
class OspfAppSockets;
class OspfAppLogging;
class OspfAppRng;
class OspfAreaLeaderController;
class Ipv4;
class Ipv4InterfaceAddress;
 
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
  using ReachableRoute = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>;
  using ReachableRouteList = std::vector<ReachableRoute>;

  enum AreaLeaderMode
  {
    AREA_LEADER_LOWEST_ROUTER_ID = 0,
    AREA_LEADER_STATIC = 1,
    AREA_LEADER_REACHABLE_LOWEST_ROUTER_ID = 2,
  };

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  OspfApp ();
  virtual ~OspfApp ();

  /**
   * \brief Enable/disable the OSPF protocol at runtime.
   *
   * Enable or disable protocol activity without destroying the application.
   */
  void Enable ();
  void Disable ();
  bool IsEnabled () const;

  /**
   * \brief Set a pointer to a routing table.
   * \param ipv4Routing OSPF forwarding plane
   */
  void SetRouting (Ptr<OspfRouting> ipv4Routing);

  /**
   * \brief Register network devices as OSPF interfaces.
   *
    * The selected devices are resolved to existing IPv4 interface indices on
    * the owning node. Every selected device must already be registered with
    * the node's IPv4 stack. Index 0 remains reserved for loopback to match the
    * attached IPv4 stack layout.
   *
   * \param devs Net device container to be registered
   */
  void SetBoundNetDevices (NetDeviceContainer devs);
  /**
   * \brief Enable or disable connected-prefix advertisement for one selected interface.
    * \param ifIndex IPv4 interface index on the owning node
    * \param enabled whether the interface prefix should be advertised
   */
  void SetInterfacePrefixRoutable (uint32_t ifIndex, bool enabled);
  /**
   * \brief Check whether connected-prefix advertisement is enabled for one interface.
    * \param ifIndex IPv4 interface index on the owning node
    * \returns true if the interface prefix is advertised
   */
  bool GetInterfacePrefixRoutable (uint32_t ifIndex) const;

  /**
   * \brief Add an explicitly injected reachable prefix.
    * \param ifIndex IPv4 interface index that owns the injected prefix
    * \param address network or host prefix to advertise
    * \param mask prefix mask
    * \param gateway next hop used for the local forwarding entry
    * \param metric advertised metric for the injected prefix
   */
  void AddReachableAddress (uint32_t ifIndex, Ipv4Address address, Ipv4Mask mask,
                            Ipv4Address gateway, uint32_t metric);
    /**
    * \brief Add an explicitly injected reachable prefix with a direct next hop.
    * \param ifIndex IPv4 interface index that owns the injected prefix
    * \param address network or host prefix to advertise
    * \param mask prefix mask
    */
  void AddReachableAddress (uint32_t ifIndex, Ipv4Address address, Ipv4Mask mask);
    /**
    * \brief Replace the injected prefix set.
    *
    * Each tuple stores `(ifIndex, network, mask, gateway, metric)` where
    * `ifIndex` is an IPv4 interface index on the owning node.
    *
    * \returns true if the injected prefix set changed
    */
  bool SetReachableAddresses (ReachableRouteList);

  /**
    * \brief Inject the connected prefixes of all selected interfaces onto one origin interface.
    * \param ifIndex IPv4 interface index used as the origin for injected prefixes
   */
  void AddAllReachableAddresses (uint32_t ifIndex);

  /**
   * \brief Remove addresses from the interface until empty or hitting localhost
    * \param ifIndex IPv4 interface index on the owning node
   */
  void ClearReachableAddresses (uint32_t ifIndex);

  /**
    * \brief Remove one injected reachable prefix.
    * \param ifIndex IPv4 interface index that owns the injected prefix
    * \param address network or host prefix to remove
    * \param mask prefix mask
   */
  void RemoveReachableAddress (uint32_t ifIndex, Ipv4Address address, Ipv4Mask mask);

  /**
    * \brief Set the default area used by this app's selected interfaces.
   * \param area the area ID
   */
  void SetArea (uint32_t area);

  /**
    * \brief Get the default area used by this app.
   * \return the area ID
   */
  uint32_t GetArea ();

  /**
   * \brief Set the area leader status
   * \param isLeader the status of area leader
   */
  void SetAreaLeader (bool isLeader);

  /**
   * \brief Set if LSAs are already preloaded
   * \param doInitialize the status
   */
  void SetDoInitialize (bool doInitialize);

  /**
   * \brief Get area mask.
   * \return the area mask
   */
  Ipv4Mask GetAreaMask ();

  /**
    * \brief Set interface metrics by IPv4 interface index.
    * \param metrices Routing metrics indexed by IPv4 interface index
   */
  void SetMetrices (std::vector<uint32_t> metrices);

  bool IsAreaLeader () const;

  /**
    * \brief Get the metric for one selected interface.
    * \param ifIndex IPv4 interface index on the owning node
   */
  uint32_t GetMetric (uint32_t ifIndex);

  /**
   * \brief Set router ID.
   * \param routerId Router ID of this router
   */
  void SetRouterId (Ipv4Address routerId);

  /**
   * \brief Get router ID.
   * \return routerId Router ID of this router
   */
  Ipv4Address GetRouterId ();

  /**
   * \brief Get LSDB; only use for testing/debugging
   */
  std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>> GetLsdb ();
  std::map<uint32_t, std::pair<LsaHeader, Ptr<L1SummaryLsa>>> GetL1SummaryLsdb ();
  std::map<uint32_t, std::pair<LsaHeader, Ptr<AreaLsa>>> GetAreaLsdb ();
  std::map<uint32_t, std::pair<LsaHeader, Ptr<L2SummaryLsa>>> GetL2SummaryLsdb ();
  uint64_t GetL1ShortestPathRunCount () const;
  uint64_t GetL2ShortestPathRunCount () const;

  struct ForwardingEntry
  {
    Ipv4Address network = Ipv4Address::GetZero ();
    Ipv4Mask mask = Ipv4Mask::GetZero ();
    Ipv4Address nextHop = Ipv4Address::GetZero ();
    uint32_t ifIndex = 0;
    uint32_t metric = 0;
  };

  bool LookupForwardingEntry (Ipv4Address destination, int32_t requiredIfIndex,
                              ForwardingEntry &entry) const;
  void PrintForwardingTable (std::ostream &os) const;
  /**
   * \brief Print Router LSDB
   */
  void PrintLsdb ();
  /**
   * \brief Print AS External LSDB
   */
  void PrintL1SummaryLsdb ();
  /**
   * \brief Print AreaLSDB
   */
  void PrintAreaLsdb ();
  /**
   * \brief Print SummaryLSDB
   */
  void PrintL2SummaryLsdb ();

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
  uint32_t GetL1SummaryLsdbHash ();

  /**
   * \brief Get Area LSDB hash for comparison.
   * \return Area LSDB hash
   */
  uint32_t GetAreaLsdbHash ();

  /**
   * \brief Get Summary LSDB hash for comparison.
   * \return Summary LSDB hash
   */
  uint32_t GetL2SummaryLsdbHash ();

  /**
   * \brief Print LSDB hash.
   */
  void PrintLsdbHash ();

  /**
   * \brief Print Area LSDB hash.
   */
  void PrintAreaLsdbHash ();

  /**
   * \brief Drive DBD reception through the real handler; intended for testing/debugging.
   */
  void ReceiveDbdForTesting (uint32_t ifIndex, Ipv4Address remoteIp,
                             Ipv4Address remoteRouterId, uint32_t area, Ptr<OspfDbd> dbd);

  /**
   * \brief AddNeighbor
  */
  void AddNeighbor (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  /**
   * \brief InjectLsa
  */
  void InjectLsa (std::vector<std::pair<LsaHeader, Ptr<Lsa>>> lsaList);

  // Import Export
  /**
   * \brief Export both LSDB and neighbors in one-go.
   * \param dirName directory name
   * \param nodeName node name
  */
  void ExportOspf (std::filesystem::path dirName, std::string nodeName);
  /**
   * \brief Export LSDB.
   * \param dirName directory name
   * \param filename file name
  */
  void ExportLsdb (std::filesystem::path dirName, std::string filename);

  /**
   * \brief Export neighbor information.
   * \param dirName directory name
   * \param filename file name
  */
  void ExportNeighbors (std::filesystem::path dirName, std::string filename);

  /**
   * \brief Export additional information such as area leader.
   * \param dirName directory name
   * \param filename file name
  */
  void ExportMetadata (std::filesystem::path dirName, std::string filename);

  /**
   * \brief Export injected prefix information.
   * \param dirName directory name
   * \param filename file name
  */
  void ExportPrefixes (std::filesystem::path dirName, std::string filename);

  /**
   * \brief Export both LSDB and neighbors in one-go.
   * \param dirName directory name
   * \param lsdbName lsdb file name
   * \param neighborName neighbor file name
  */
  void ImportOspf (std::filesystem::path dirName, std::string nodeName);
  /**
  * \brief Import LSDB.
  * \param dirName directory name
  * \param nodeName node name
 */
  void ImportLsdb (std::filesystem::path dirName, std::string filename);

  /**
  * \brief Import neighbor information.
  * \param dirName directory name
  * \param filename file name
 */
  void ImportNeighbors (std::filesystem::path dirName, std::string filename);

  /**
    * \brief Import additional information such as area leader.
    * \param dirName directory name
    * \param filename file name
  */
  void ImportMetadata (std::filesystem::path dirName, std::string filename);

  /**
    * \brief Import injected prefix informatino.
    * \param dirName directory name
    * \param filename file name
  */
  void ImportPrefixes (std::filesystem::path dirName, std::string filename);

  /**
   * \brief Fetch an LSA by key from the local LSDB.
   * \return <header, payload> or <default header, nullptr> if missing.
   */
  std::pair<LsaHeader, Ptr<Lsa>> FetchLsa (LsaHeader::LsaKey lsaKey);

  struct LsaThrottleStats
  {
    uint64_t recomputeTriggers = 0; //!< Number of calls into ThrottledRecompute*()
    uint64_t immediate = 0; //!< Recompute executed immediately (no throttling delay)
    uint64_t deferredScheduled = 0; //!< A deferred recompute event was scheduled
    uint64_t suppressed = 0; //!< A trigger was suppressed because a deferred recompute was already pending
    uint64_t cancelledPending = 0; //!< A pending deferred recompute was cancelled because we could run immediately
  };

  /**
   * \brief Return current LSA throttling statistics.
   *
   * Stats are only collected when the EnableLsaThrottleStats attribute is true.
   */
  LsaThrottleStats GetLsaThrottleStats () const;

  /**
   * \brief Reset LSA throttling statistics to zero.
   */
  void ResetLsaThrottleStats ();

protected:
  virtual void DoDispose (void);

private:
  enum class OwnerKind : uint8_t
  {
    Interface = 0,
      GatewayRoute = 1,
      Router = 2,
      Area = 3,
  };

  struct OwnerRef
  {
    OwnerKind kind = OwnerKind::Interface;
    uint64_t id = 0;

    bool operator== (const OwnerRef &other) const
    {
      return kind == other.kind && id == other.id;
    }
  };

  struct OwnerRefHash
  {
    std::size_t operator() (const OwnerRef &owner) const;
  };

  struct PrefixKey
  {
    uint8_t prefixLength = 0;
    uint32_t network = 0;

    bool operator== (const PrefixKey &other) const
    {
      return prefixLength == other.prefixLength && network == other.network;
    }
  };

  struct PrefixOwnerEntry
  {
    std::unordered_map<OwnerRef, uint32_t, OwnerRefHash> candidates;
  };

  struct ResolvedOwner
  {
    uint32_t ifIndex = 0;
    Ipv4Address nextHop = Ipv4Address::GetZero ();
    uint32_t pathMetric = 0;
  };

  static OwnerRef MakeOwnerRef (OwnerKind kind, uint64_t id);
  static uint64_t MakeGatewayRouteOwnerId (uint32_t ifIndex, uint32_t gateway);
  static uint32_t PrefixLengthToMaskValue (uint8_t prefixLength);
  static uint32_t MaskNetwork (uint32_t address, uint8_t prefixLength);
  static bool IsOwnerPreferred (const OwnerRef &candidate, const OwnerRef &current);
  void RebuildPrefixOwnerTable ();
  void RebuildLocalInterfacePrefixOwners ();
  void RebuildOwnerResolutionTable ();
  void ReplaceOwnedPrefixes (const OwnerRef &owner, const std::set<SummaryRoute> &routes);
  void RemoveOwnedPrefixes (const OwnerRef &owner);
  bool TryResolveOwner (const OwnerRef &owner, ResolvedOwner &resolved) const;

  void ResetStateForRestart ();
  void FlushOspfRoutes ();
  friend class OspfAppTestPeer;
  friend class OspfAppIo;
  friend class OspfNeighborFsm;
  friend class OspfLsaProcessor;
  friend class OspfStateSerializer;
  friend class OspfAppSockets;
  friend class OspfAppLogging;
  friend class OspfAppRng;
  friend class OspfAreaLeaderController;

  std::unique_ptr<OspfAppIo> m_io;
  std::unique_ptr<OspfNeighborFsm> m_neighborFsm;
  std::unique_ptr<OspfLsaProcessor> m_lsa;
  std::unique_ptr<OspfStateSerializer> m_state;
  std::unique_ptr<OspfAppSockets> m_socketsMgr;
  std::unique_ptr<OspfAppLogging> m_logging;
  std::unique_ptr<OspfAppRng> m_rng;
  std::unique_ptr<OspfAreaLeaderController> m_areaLeader;

  friend class OspfRouting;
  friend class OspfAppHelper;
  friend class OspfStateSerializer;
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void InitializeLoggingIfEnabled ();
  void InitializeRandomVariables ();
  void InitializeSockets ();
  void CancelHelloTimeouts ();
  void CloseSockets ();

  // Optional: keep OSPF interface state in sync with Ipv4 interfaces.
  void StartInterfaceSyncIfEnabled ();
  void StopInterfaceSync ();
  void InterfaceSyncTick ();
  void EnsureInterfacePolicySize (uint32_t nIf);
  int32_t ResolveIpv4InterfaceIndex (Ptr<NetDevice> dev) const;
  std::vector<uint32_t> CollectSelectedInterfaces (Ptr<Ipv4> ipv4,
                                                   NetDeviceContainer devs) const;
  void ApplyBoundInterfaceSelection (Ptr<Ipv4> ipv4,
                                     const std::vector<uint32_t> &selectedInterfaces);
  void RestoreAdvertisedInterfacePrefixes (
      const std::vector<uint32_t> &selectedInterfaces,
      const std::vector<bool> &previousAdvertiseInterfacePrefixes);
  void CancelPendingLsaRegeneration (const LsaHeader::LsaKey &lsaKey);
  void ClearPendingLsaRegenerationState ();
  void ClearSelfOriginatedLsaStateForRouterId (Ipv4Address routerId);
  void UpdateRouterId (Ipv4Address routerId);
  void RebuildSelectedOspfInterfaces (Ptr<Ipv4> ipv4);
  void StopProtocolForInterfaceRebind ();
  void RestartProtocolAfterInterfaceRebind ();
  bool RefreshAllOspfInterfaceStateFromIpv4 ();
  Ipv4Address SelectAutomaticRouterId () const;
  uint32_t GetConfiguredInterfaceMetric (uint32_t ifIndex) const;
  bool HasOspfInterface (uint32_t ifIndex) const;
  Ptr<OspfInterface> GetOspfInterface (uint32_t ifIndex) const;
  Ptr<NetDevice> GetNetDeviceForInterface (uint32_t ifIndex) const;
  static bool SelectPrimaryInterfaceAddress (Ptr<Ipv4> ipv4, uint32_t ifIndex,
                                             Ipv4InterfaceAddress &out);
  Ipv4Address ResolvePointToPointGateway (Ptr<NetDevice> dev) const;
  bool RefreshOspfInterfaceStateFromIpv4 (Ptr<Ipv4> ipv4, uint32_t ifIndex);
  ReachableRouteList CollectInterfaceReachableRoutesFromIpv4 (Ptr<Ipv4> ipv4) const;
  void HandleLocalInterfaceEvent ();
  void RefreshInterfaceReachableRoutesFromIpv4 ();
  void InitializeSplitReachableRoutesFromCurrentState ();
  bool ApplyAdvertisedReachableRoutes ();
  bool SetInterfaceReachableAddresses (ReachableRouteList);
  void RefreshReachableRoutesAndAdvertise ();
  void HandleInterfaceDown (uint32_t ifIndex);
  void ReplaceNeighbors (const std::vector<std::vector<Ptr<OspfNeighbor>>> &neighbors);

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
   * \brief Print LSA
   * \param seqNum Sequence Number
   * \param lsaKey LSA Key
   * \param time Time
   */
  void PrintLsaTiming (uint32_t seqNum, LsaHeader::LsaKey lsaKey, Time time);
  /**
   * \brief Process LSA.
   * \param ipHeader IPv4 Header
   * \param ospfHeader OSPF Header
   * \param lsa LS Advertisement
   */
  void ProcessLsa (LsaHeader lsaHeader, Ptr<Lsa> lsa);
  void ProcessLsa (std::pair<LsaHeader, Ptr<Lsa>> lsa);
  /**
   * \brief Process LS Acknowledge as a response for LS Update during Full.
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
  Ptr<L1SummaryLsa> GetL1SummaryLsa ();
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
  Ptr<L2SummaryLsa> GetL2SummaryLsa ();
  void OriginateAreaLsa (Ptr<AreaLsa> areaLsa);
  void OriginateL2SummaryLsa (Ptr<L2SummaryLsa> summary);
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
   * \brief Throttled version of RecomputeRouterLsa that respects MinLsInterval
   */
  void ThrottledRecomputeRouterLsa ();
  void RecomputeRouterLsaAndProcessSelf ();
  /**
   * \brief Recompute L1 Summary-LSA, increment its Sequence Number, and inject to L1 Summary LSDB
   */
  void RecomputeL1SummaryLsa ();
  /**
   * \brief Throttled version of RecomputeL1SummaryLsa that respects MinLsInterval
   */
  void ThrottledRecomputeL1SummaryLsa ();
  void RecomputeL1SummaryLsaAndProcessSelf ();
  /**
   * \brief Recompute local Area-LSA, increment its Sequence Number, and inject to Area LSDB
   */
  bool RecomputeAreaLsa ();
  /**
   * \brief Throttled version of RecomputeAreaLsa that respects MinLsInterval
   */
  void ThrottledRecomputeAreaLsa ();
  /**
   * \brief Recompute Area Summary-LSA, increment its Sequence Number, and inject to L2 Summary LSDB
   */
  bool RecomputeL2SummaryLsa ();
  /**
   * \brief Throttled version of RecomputeL2SummaryLsa that respects MinLsInterval
   */
  void ThrottledRecomputeL2SummaryLsa ();

  /**
   * \brief Check if LSA should be throttled and get delay
   * \param lsaKey the LSA key
   * \return Time::Zero() if should proceed immediately, otherwise delay until next allowed origination
   */
  Time GetLsaThrottleDelay (const LsaHeader::LsaKey &lsaKey);

  /**
   * \brief Clean up completed throttle events for an LSA key
   * \param lsaKey the LSA key to clean
   */
  void CleanupThrottleEvent (const LsaHeader::LsaKey &lsaKey);

  /**
   * \brief Wrapper for RecomputeAreaLsa (void return type for Simulator::Schedule)
   */
  void RecomputeAreaLsaWrapper ();

  /**
   * \brief Wrapper for RecomputeL2SummaryLsa (void return type for Simulator::Schedule)
   */
  void RecomputeL2SummaryLsaWrapper ();
  /**
   * \brief Update routing table based on shortest paths and prefixes
   */
  void UpdateRouting ();

  /**
   * \brief Schedule to update shortest paths and prefixes for L1
   */
  void ScheduleUpdateL1ShortestPath ();

  /**
   * \brief Update shortest paths and prefixes for L1
   */
  void UpdateL1ShortestPath ();

  /**
   * \brief Schedule to update shortest paths and prefixes for L2
   */
  void ScheduleUpdateL2ShortestPath ();

  /**
   * \brief Update shortest paths and prefixes for L2
   */
  void UpdateL2ShortestPath ();

  // Hello Protocol
  /**
   * \brief A timeout event for Hello, triggered after not receiving Hello for RouterDeadInterval
   * \param ifIndex Interface index
   * \param neighbor Ospf neighbor
   */
  void HelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  /**
   * \brief Refresh the Hello timeout, triggered after receiving Hello.
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void RefreshHelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

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
  void FallbackToDown (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  // Init
  /**
   * \brief Move the neighbor state to Init.
    *
   * Move the neighbor state to Init, flooding LSAs to update
    * its neighbors. Router-LSA updates are currently emitted immediately
    * instead of being deferred until the dead interval expires.
    *
   * \param ifIndex Interface index
   * \param neighbor OSPF neighbor
   */
  void FallbackToInit (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

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

  bool m_doInitialize = true;

  bool m_enabled = true;
  bool m_protocolRunning = false;
  bool m_resetStateOnDisable = false;

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
  bool m_manualRouterId = false;
  Ipv4Mask m_areaMask; // Area masks
  std::vector<bool> m_boundInterfaceSelection;
  std::vector<uint32_t> m_interfaceMetrics;
  uint32_t m_areaId; // Only used for default value and for alt area and
  std::string m_logDir; //!< Log directory
  bool m_enableLsaTimingLog; //!< Enable LSA timing logs
  std::ofstream m_lsaTimingLog; //!< LSA timing log file
  bool m_enablePacketLog; //!< Enable OSPF packet logging
  bool m_includeHelloInPacketLog; //!< Include Hello packets in packet log
  std::ofstream m_packetLog; //!< OSPF packet log file

  // Randomization
  // For a small time jitter (used for hello/timeout/retx scheduling jitter)
  Ptr<UniformRandomVariable> m_jitterRv = CreateObject<UniformRandomVariable> ();
  // For DD Sequence Number
  Ptr<UniformRandomVariable> m_randomVariableSeq = CreateObject<UniformRandomVariable> ();

  // Hello
  Time m_helloInterval; //!< Hello Interval
  Time m_initialHelloDelay; //!< Delay before the first hello
  Ipv4Address m_helloAddress; //!< Address of multicast hello message
  std::vector<Time> m_lastHelloReceived; //!< Times of last hello received
  std::vector<std::map<uint32_t, EventId>>
      m_helloTimeouts; //!< Timeout Events of not receiving Hello, per interface, per neighbor
  Time m_routerDeadInterval; //!< Router Dead Interval for Hello to become Down
  EventId m_helloEvent; //!< Event to send the next hello packet

  // Interface auto-tracking (opt-in)
  bool m_autoSyncInterfaces = false;
  Time m_interfaceSyncInterval = MilliSeconds (200);
  EventId m_interfaceSyncEvent;

  // Interface
  std::vector<Ptr<OspfInterface>> m_ospfInterfaces; // !< Router interfaces
  std::vector<bool> m_advertiseInterfacePrefixes;

  // Routing
  Ptr<OspfRouting> m_routing; // !< OSPF forwarding plane
  std::unordered_map<uint32_t, NextHop> m_l1NextHop; //!< Next Hopto routers
  std::unordered_map<uint32_t, std::vector<uint32_t>> m_l1Addresses; //!< Addresses for L1 routers
  Time m_shortestPathUpdateDelay; // !< Shortest path before shortest path calculation
  // Interface-derived prefixes, normally sourced from the node's active Ipv4 interfaces.
    ReachableRouteList m_interfaceExternalRoutes;
  // Effective advertised prefixes used by the lookup and LSA code.
    ReachableRouteList m_externalRoutes;
  // Additional advertised prefixes explicitly injected by the caller and not derived from Ipv4 interfaces.
    ReachableRouteList m_injectedExternalRoutes;
  std::array<std::unordered_map<uint32_t, PrefixOwnerEntry>, 33> m_prefixOwnersByLength;
  std::unordered_map<OwnerRef, std::vector<PrefixKey>, OwnerRefHash> m_ownerToPrefixes;
  std::unordered_map<OwnerRef, ResolvedOwner, OwnerRefHash> m_ownerResolutionTable;
  uint64_t m_l1ShortestPathRunCount = 0;
  uint64_t m_l2ShortestPathRunCount = 0;

  // Area
  std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>
      m_l2NextHop; //!< <next hop, distance> to areas
  bool m_isAreaLeader = false;
  AreaLeaderMode m_areaLeaderMode = AREA_LEADER_REACHABLE_LOWEST_ROUTER_ID;
  Ipv4Address m_staticAreaLeaderRouterId = Ipv4Address::GetZero ();

  // LSA
  bool m_enableAreaProxy; // True if Proxied L2 LSAs are generated
  Time m_rxmtInterval; // retransmission timer
  EventId m_areaLeaderBeginTimer; // area leadership begin timer
  Ipv4Address m_lsaAddress; //!< multicast address for LSA
  std::map<LsaHeader::LsaKey, uint16_t> m_seqNumbers; // sequence number of stored LSA

  // LSA Throttling (RFC 2328 MinLSInterval)
  Time m_minLsInterval; //!< Minimum interval between originating the same LSA
  std::map<LsaHeader::LsaKey, Time> m_lastLsaOriginationTime; //!< Last origination time per LSA key
  std::map<LsaHeader::LsaKey, EventId> m_pendingLsaRegeneration; //!< Pending regeneration events

  bool m_enableLsaThrottleStats = false;
  uint64_t m_lsaThrottleRecomputeTriggers = 0;
  uint64_t m_lsaThrottleImmediate = 0;
  uint64_t m_lsaThrottleDeferredScheduled = 0;
  uint64_t m_lsaThrottleSuppressed = 0;
  uint64_t m_lsaThrottleCancelledPending = 0;

  // L1 LSDB
  std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>>
      m_routerLsdb; // LSDB for each remote router ID
  std::map<uint32_t, std::pair<LsaHeader, Ptr<L1SummaryLsa>>>
      m_l1SummaryLsdb; // LSDB for each remote router ID
  std::unordered_map<uint32_t, std::pair<uint32_t, NextHop>>
      m_nextHopToShortestBorderRouter; // next hop
  std::vector<uint32_t> m_advertisingPrefixes;
  EventId m_updateL1ShortestPathTimeout; // timeout to update the L1 shortest path

  // L2 LSDB
  std::map<uint32_t, std::pair<LsaHeader, Ptr<AreaLsa>>> m_areaLsdb; // LSDB for each remote area ID
  std::map<uint32_t, std::pair<LsaHeader, Ptr<L2SummaryLsa>>>
      m_l2SummaryLsdb; // LSDB for summary prefixes
  EventId m_updateL2ShortestPathTimeout; // timeout to update the L2 shortest path

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
