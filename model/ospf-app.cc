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

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/header.h"
#include "ns3/random-variable-stream.h"
#include "ns3/core-module.h"
#include "ns3/ospf-packet-helper.h"
#include "ospf-header.h"
#include "lsa-header.h"
#include "router-lsa.h"
#include "ospf-hello.h"
#include "ospf-interface.h"
#include "ospf-neighbor.h"

#include "filesystem"
#include "tuple"

#include "ospf-app.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfApp");

NS_OBJECT_ENSURE_REGISTERED (OspfApp);

TypeId
OspfApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OspfApp")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<OspfApp> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&OspfApp::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("HelloInterval", "OSPF Hello Interval",
                   TimeValue (Seconds(10)),
                   MakeTimeAccessor (&OspfApp::m_helloInterval),
                   MakeTimeChecker ())
    .AddAttribute ("HelloAddress", "Multicast address of Hello",
                   Ipv4AddressValue (Ipv4Address("224.0.0.5")),
                   MakeIpv4AddressAccessor (&OspfApp::m_helloAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("LSAAddress", "Multicast address of LSAs",
                   Ipv4AddressValue (Ipv4Address("224.0.0.6")),
                   MakeIpv4AddressAccessor (&OspfApp::m_lsaAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("RouterDeadInterval", "Link is considered down when not receiving Hello until RouterDeadInterval",
                   TimeValue (Seconds(30)),
                   MakeTimeAccessor (&OspfApp::m_routerDeadInterval),
                   MakeTimeChecker ())
    .AddAttribute ("LSUInterval", "LSU Retransmission Interval",
                   TimeValue (Seconds(5)),
                   MakeTimeAccessor (&OspfApp::m_rxmtInterval),
                   MakeTimeChecker ())
    .AddAttribute ("TTL", "Time To Live",
                   UintegerValue (64),
                   MakeUintegerAccessor (&OspfApp::m_ttl),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("DefaultArea", "Default area ID for interfaces",
                   UintegerValue (0),
                   MakeUintegerAccessor (&OspfApp::m_areaId),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&OspfApp::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&OspfApp::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&OspfApp::m_txTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&OspfApp::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
  ;
  return tid;
}

OspfApp::OspfApp ()
{
  NS_LOG_FUNCTION (this);
}

OspfApp::~OspfApp()
{
  NS_LOG_FUNCTION (this);
  m_sockets.clear();
  m_lsaSockets.clear();
  m_helloSockets.clear();
}

void
OspfApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
OspfApp::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  // Generate random variables
  m_randomVariable->SetAttribute("Min", DoubleValue(0.0)); // Minimum value in seconds
  m_randomVariable->SetAttribute("Max", DoubleValue(0.005)); // Maximum value in seconds (5 ms)

  // Add local null sockets
  m_sockets.emplace_back(nullptr);
  m_helloSockets.emplace_back(nullptr);
  m_lsaSockets.emplace_back(nullptr);
  for (uint32_t i = 1; i < m_boundDevices.GetN(); i++)
  {
    // Create sockets
    TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
    
    // auto ipv4 = GetNode()->GetObject<Ipv4>();
    // Ipv4Address addr = ipv4->GetAddress(i, 0).GetAddress();
    InetSocketAddress anySocketAddress(Ipv4Address::GetAny());

    // For Hello, both bind and listen to m_helloAddress
    auto helloSocket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress helloSocketAddress(m_helloAddress);
    if (helloSocket->Bind (anySocketAddress) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
    helloSocket->Connect (anySocketAddress);
    helloSocket->SetAllowBroadcast (true);
    helloSocket->SetAttribute("Protocol", UintegerValue(89));
    helloSocket->BindToNetDevice(m_boundDevices.Get(i));
    helloSocket->SetRecvCallback (MakeCallback (&OspfApp::HandleRead, this));
    m_helloSockets.emplace_back(helloSocket);

    // For LSA, both bind and listen to m_lsaAddress
    auto lsaSocket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress lsaSocketAddress(m_lsaAddress);
    if (lsaSocket->Bind (anySocketAddress) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
    lsaSocket->Connect (anySocketAddress);
    lsaSocket->SetAllowBroadcast (true);
    lsaSocket->SetAttribute("Protocol", UintegerValue(89));
    lsaSocket->SetIpTtl(1);
    lsaSocket->BindToNetDevice(m_boundDevices.Get(i));
    m_lsaSockets.emplace_back(lsaSocket);

    // For unicast, such as LSA retransmission, bind to local address
    auto unicastSocket = Socket::CreateSocket (GetNode (), tid);
    if (unicastSocket->Bind (anySocketAddress) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
    unicastSocket->SetAllowBroadcast (true);
    unicastSocket->SetAttribute("Protocol", UintegerValue(89));
    unicastSocket->BindToNetDevice(m_boundDevices.Get(i));
    unicastSocket->SetRecvCallback (MakeCallback (&OspfApp::HandleRead, this));
    m_sockets.emplace_back(unicastSocket);
  }
  ScheduleTransmitHello (Seconds (0.));
}

void 
OspfApp::SetBoundNetDevices (NetDeviceContainer devs)
{
  NS_LOG_FUNCTION (this << devs.GetN());
  m_boundDevices = devs;
  m_lastHelloReceived.resize(devs.GetN());
  m_helloTimeouts.resize(devs.GetN());
  m_lsuTimeouts.resize(devs.GetN());

  // Create interface database
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4> ();

  // Add loopbacks at index 0
  Ptr<OspfInterface> ospfInterface= Create<OspfInterface> ();
  m_ospfInterfaces.emplace_back(ospfInterface);

  // Add the rest of net devices
  for (uint32_t i = 1; i < m_boundDevices.GetN(); i++) {
    auto sourceIp = ipv4->GetAddress(i, 0).GetAddress();
    auto mask = ipv4->GetAddress(i, 0).GetMask();
    Ptr<OspfInterface> ospfInterface = Create<OspfInterface> (sourceIp, mask, m_helloInterval.GetSeconds(), m_routerDeadInterval.GetSeconds(), m_areaId, 1);
    m_ospfInterfaces.emplace_back(ospfInterface);
  }
}

void
OspfApp::SetMetrices (std::vector<uint32_t> metrices) {
  NS_ASSERT_MSG(metrices.size() == m_ospfInterfaces.size(),
              "The length of metrices must match the number of interfaces");
  for (uint32_t i = 0; i < m_ospfInterfaces.size(); i++) {
    m_ospfInterfaces[i]->SetMetric(metrices[i]);
  }
}

void
OspfApp::SetAreas (std::vector<uint32_t> areas) {
  NS_ASSERT_MSG(areas.size() == m_ospfInterfaces.size(),
              "The length of areas must match the number of interfaces");
  for (uint32_t i = 1; i < m_ospfInterfaces.size(); i++) {
    m_ospfInterfaces[i]->SetArea(areas[i]);
  }
  // Loopback is the router area (only used for alt area)
  m_areaId = areas[0];
}


void 
OspfApp::SetRouterId (Ipv4Address routerId)
{
  m_routerId = routerId;
}

void 
OspfApp::ScheduleTransmitHello (Time dt)
{
  m_helloEvent = Simulator::Schedule (dt + Seconds(m_randomVariable->GetValue()), &OspfApp::SendHello, this);
}

void 
OspfApp::SendHello ()
{
  if (m_helloSockets.empty()) return;

  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_helloEvent.IsExpired ());

  Address helloSocketAddress;
  for (uint32_t i = 1; i < m_helloSockets.size(); i++)
  {
    auto socket = m_helloSockets[i];
    socket->GetSockName (helloSocketAddress);
    Ptr<Packet> p = ConstructHelloPacket(Ipv4Address::ConvertFrom (m_routerId), m_ospfInterfaces[i]->GetArea(),
                    m_ospfInterfaces[i]->GetMask(), m_ospfInterfaces[i]->GetHelloInterval(), m_ospfInterfaces[i]->GetRouterDeadInterval(),
                    m_ospfInterfaces[i]->GetNeighbors());
    // call to the trace sinks before the packet is actually sent,
    // so that tags added to the packet can be sent as well
    m_txTrace (p);
    if (Ipv4Address::IsMatchingType (m_helloAddress))
    {
      m_txTraceWithAddresses (p, helloSocketAddress, InetSocketAddress (Ipv4Address::ConvertFrom (m_helloAddress)));
    }
    // NS_LOG_DEBUG(socket->GetBoundNetDevice()->GetIfIndex());
    // InetSocketAddress sourceAddress = InetSocketAddress::ConvertFrom(localAddress);
    // NS_LOG_DEBUG("Source Address: " << sourceAddress.GetIpv4());
    socket->SendTo (p, 0, InetSocketAddress (Ipv4Address::ConvertFrom (m_helloAddress)));
    if (Ipv4Address::IsMatchingType (m_helloAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S) << " client sent " << p->GetSize() << " bytes to " <<
                    m_helloAddress << " via interface " << i << " : " << m_ospfInterfaces[i]->GetAddress());
    }

  }
  if (!m_helloSockets.empty())
  {
    ScheduleTransmitHello (m_helloInterval);
  }
}

void 
OspfApp::SendAck (uint32_t ifIndex, Ptr<Packet> ackPayload, Ipv4Address originRouterId)
{
  NS_LOG_FUNCTION (this << ifIndex << originRouterId);
  if (m_lsaSockets.empty()) return;


  Address ackSocketAddress;
  auto socket = m_lsaSockets[ifIndex];
  socket->GetSockName (ackSocketAddress);
  m_txTrace (ackPayload);

  socket->SendTo (ackPayload, 0, InetSocketAddress (m_lsaAddress));
  NS_LOG_INFO ("ack sent to " << originRouterId << " via interface " << ifIndex << " : " << m_ospfInterfaces[ifIndex]->GetAddress());
}

void 
OspfApp::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  m_lsuTimeouts.clear();
  for (auto timer : m_helloTimeouts) {
    timer.Remove();
  }
  for (uint32_t i = 1; i < m_sockets.size(); i++)
  {
    // Hello
    m_helloSockets[i]->Close ();
    m_helloSockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    m_helloSockets[i] = 0;

    // LSA
    m_lsaSockets[i]->Close ();
    m_lsaSockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    m_lsaSockets[i] = 0;

    // Unicast
    m_sockets[i]->Close ();
    m_sockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    m_sockets[i] = 0;
  }
  m_helloSockets.clear();
  m_lsaSockets.clear();
  m_sockets.clear();
}

void
OspfApp::SetRouting (Ptr<Ipv4StaticRouting> routing) {
  m_routing = routing;
} 

void
OspfApp::HelloTimeout (Ptr<OspfInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp) {
  auto neighbor = ospfInterface->GetNeighbor(remoteRouterId, remoteIp);
  NS_ASSERT(neighbor != nullptr);
  // Set the interface to down, which keeps hello going
  neighbor->SetState(OspfNeighbor::Down);
  AdjacencyUpdate();
  NS_LOG_DEBUG("Interface " << ospfInterface->GetAddress() << " has removed routerId: "
              << remoteRouterId << ", remoteIp" << remoteIp << " neighbors");
}

void
OspfApp::RefreshHelloTimeout(uint32_t ifIndex, Ptr<OspfInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp)
{
  // Refresh the timer
  if (m_helloTimeouts[ifIndex].IsRunning()) {
    m_helloTimeouts[ifIndex].Remove();
  }
  m_helloTimeouts[ifIndex] = Simulator::Schedule(Seconds(ospfInterface->GetRouterDeadInterval()) + Seconds(m_randomVariable->GetValue()),
                                                    &OspfApp::HelloTimeout, this, ospfInterface, remoteRouterId, remoteIp);
}

// Only flood all neighbors, neighbors will create another LSU to flood again.
// This is why TTL is not used from IP header
// Will repeat if this node is the originator
void
OspfApp::SendLSU(uint32_t ifIndex, Ptr<Packet> lsuPacket, uint32_t flags, Ipv4Address routerId, Ipv4Address toAddress) {
  auto p = lsuPacket->Copy();
  Ptr<Socket> socket = m_lsaSockets[ifIndex];
  m_txTrace (p);
  socket->SendTo (p, 0, InetSocketAddress (toAddress));
  // Schedule for retries
  m_lsuTimeouts[ifIndex][routerId.Get()] =
    Simulator::Schedule(m_rxmtInterval + Seconds(m_randomVariable->GetValue()),
                        &OspfApp::LSUTimeout, this, ifIndex, lsuPacket, flags, routerId, toAddress);
}

// Retransmit missing ACK
void
OspfApp::LSUTimeout(uint32_t ifIndex, Ptr<Packet> lsuPacket, uint32_t flags, Ipv4Address routerId, Ipv4Address toAddress) {
  // Sockets are closed
  if (m_lsaSockets.empty()) {
    return;
  }

  NS_LOG_INFO("Retry missing ACK from interface " << ifIndex << " from address " << toAddress);
  SendLSU(ifIndex, lsuPacket, flags, routerId, toAddress);
}

void
OspfApp::FloodLSU(Ptr<Packet> lsuPacket, uint32_t inputIfIndex) {
  if (m_lsaSockets.empty()) {
    NS_LOG_INFO ("No sockets to flood LSU");
    return;
  }
  NS_LOG_FUNCTION (this << lsuPacket->GetSize() << inputIfIndex);
  for (uint32_t i = 1; i < m_lsaSockets.size(); i++)
  {
    // Skip the incoming interface
    if (inputIfIndex == i) continue;
    
    // Send to neighbors with multicast address (only 1 neighbor for point-to-point)
    auto neighbors = m_ospfInterfaces[i]->GetNeighbors();
    for (auto neighbor : neighbors) {
      SendLSU(i, lsuPacket, 0, neighbor->GetRouterId(), neighbor->GetIpAddress());
    }     
  }
}

// Generate Local Router LSA with all areas
Ptr<RouterLsa>
OspfApp::GetRouterLsa() {
  // <neighbor's router ID, router's IP address, interface metric>
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > allLinks;
  for (uint32_t i = 1; i < m_ospfInterfaces.size(); i++) {
    auto links = m_ospfInterfaces[i]->GetActiveNeighborLinks();
    for (auto l : links) {
      allLinks.emplace_back(l.first, l.second, m_ospfInterfaces[i]->GetMetric());
    }
  }
  NS_LOG_INFO("LSA Created with " << allLinks.size() << " links");
  return ConstructRouterLsa(allLinks);
}

// Generate the local router LSA for a specific area
Ptr<RouterLsa>
OspfApp::GetRouterLsa(uint32_t areaId) {
  // <neighbor's router ID, router's IP address, interface metric>
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > allLinks;
  for (uint32_t i = 1; i < m_ospfInterfaces.size(); i++) {
    auto links = m_ospfInterfaces[i]->GetActiveNeighborLinks(areaId);
    for (auto l : links) {
      allLinks.emplace_back(l.first, l.second, m_ospfInterfaces[i]->GetMetric());
    }
  }
  NS_LOG_INFO("LSA Created with " << allLinks.size() << " links");
  return ConstructRouterLsa(allLinks);
}

void
OspfApp::AdjacencyUpdate() {
  NS_LOG_FUNCTION (this);

  // Initialize seq number to zero if new
  if (m_seqNumbers.find(m_routerId.Get()) == m_seqNumbers.end()) {
    m_seqNumbers[m_routerId.Get()] = 0;
  }

  // Construct an LSU packet
  Ptr<RouterLsa> routerLsa = GetRouterLsa();

  // Assign routerLsa to its router LSDB
  m_routerLsdb[m_routerId.Get()] = routerLsa;

  // routerLsa->Print(std::cout);

  // Construct an LSU packet
  Ptr<Packet> p = ConstructLSUPacket(m_routerId, m_areaId, ++m_seqNumbers[m_routerId.Get()], routerLsa);

  // Update routing according to the updated LSDB
  UpdateRouting();
  
  // Flood the network with multicast IP
  FloodLSU(p, 0);
  // m_lsuTimeout = Simulator::Schedule(m_rxmtInterval + Seconds(m_randomVariable->GetValue()), &OspfApp::FloodLSU, this, p, 0);
}

// std::vector<Ptr<OSPFInterface> > ospfInterfaces
void 
OspfApp::HandleHello (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<OspfHello> hello)
{
  
  // Get relevant interface
  Ptr<OspfInterface> ospfInterface = m_ospfInterfaces[ifIndex];

  // Check if the paremeters match
  if (hello->GetHelloInterval() != ospfInterface->GetHelloInterval()) {
    NS_LOG_ERROR("Hello interval does not match " << hello->GetHelloInterval() << " != " << ospfInterface->GetHelloInterval());
    return;
  }
  if (hello->GetRouterDeadInterval() != ospfInterface->GetRouterDeadInterval()) {
    NS_LOG_ERROR("Router Interval does not match " << hello->GetRouterDeadInterval() << " != " << ospfInterface->GetRouterDeadInterval());
    return;
  }

  Ipv4Address remoteRouterId = Ipv4Address(ospfHeader.GetRouterId());
  Ipv4Address remoteIp = ipHeader.GetSource();
  NS_LOG_FUNCTION(this << ifIndex << remoteRouterId << remoteIp);

  Ptr<OspfNeighbor> neighbor;

  // Add a new neighbor if interface hasn't registered the neighbor
  if (!ospfInterface->IsNeighbor(remoteRouterId, remoteIp)) {
    neighbor = ospfInterface->AddNeighbor(remoteRouterId, remoteIp, ospfHeader.GetArea(), OspfNeighbor::Init);
    NS_LOG_INFO("New neighbor from area " << ospfHeader.GetArea() << " detected from interface " << ifIndex);
  } else {
    neighbor = ospfInterface->GetNeighbor(remoteRouterId, remoteIp);
    // Check if received Hello has different area ID
    if (neighbor->GetArea() != ospfHeader.GetArea()) {
      NS_LOG_WARN("Received Hello and the stored neighbor have different area IDs, replacing with the Hello");
      neighbor->SetArea(ospfHeader.GetArea());
    }
  }

  if (neighbor->GetState() == OspfNeighbor::Down) {
    NS_LOG_INFO("Re-added timed out interface " << ifIndex);
    neighbor->SetState(OspfNeighbor::Init);
  }

  // Refresh last received hello time to Now()
  neighbor->RefreshLastHelloReceived();
  RefreshHelloTimeout(ifIndex, ospfInterface, remoteRouterId, remoteIp);

  // Advance to two-way/exstart
  if (neighbor->GetState() == OspfNeighbor::Init) {
    // Advance to ExStart if it's bidirectional (TODO: two-way for broadcast networks)
    if (hello->IsNeighbor(m_routerId.Get())) {
      NS_LOG_INFO("Interface " << ifIndex << " is now bi-directional");
      neighbor->SetState(OspfNeighbor::Full); // TODO: Change to ExStart and add Data Sync
      // Adjacency formed, start sending updates
      AdjacencyUpdate();
    }
  }
}

void 
OspfApp::HandleLSAck (uint32_t ifIndex, OspfHeader ospfHeader, std::vector<LsaHeader> lsaHeaders)
{
  NS_LOG_FUNCTION (this << ifIndex << lsaHeaders.size());

  for (auto lsaHeader : lsaHeaders) {
    // Remote timeout if the sequence numbers match
    if (lsaHeader.GetSeqNum() == m_seqNumbers[m_routerId.Get()]) {
      m_lsuTimeouts[ifIndex][ospfHeader.GetRouterId()].Remove();
    }
  }
}

void 
OspfApp::HandleRouterLSU (uint32_t ifIndex, OspfHeader ospfHeader, LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa)
{
  NS_LOG_FUNCTION(this << ifIndex);
  // If the LSU was originally generated by the receiving router, the packet is dropped.
  uint32_t originRouterId = ospfHeader.GetRouterId();
  if (originRouterId == m_routerId.Get()) {
    NS_LOG_INFO("LSU is dropped, received LSU has originated here");
    return;
  }
  uint16_t seqNum = lsaHeader.GetSeqNum();

  // If the sequence number equals or less than that of the last packet received from the
  // originating router, the packet is dropped.
  if (m_seqNumbers.find(originRouterId) == m_seqNumbers.end()) {
    m_seqNumbers[originRouterId] = 0;
  }
  if (seqNum <= m_seqNumbers[originRouterId]) {
    // std::cout << "recv seqNum: "<< seqNum << ", stored seqNum: " << m_seqNumbers[originRouterId] << std::endl;
    NS_LOG_INFO("LSU is dropped: received sequence number <= stored sequence number");
    // Send direct ACK once receiving duplicated sequence number
    auto ackPacket = ConstructLSAckPacket(m_routerId, m_areaId, lsaHeader);
    NS_LOG_DEBUG("Sending ACK [" << ackPacket->GetSize() << "] from " << m_routerId << " to interface " << ifIndex);
    SendAck(ifIndex, ackPacket, Ipv4Address(originRouterId));
    return;
  }

  // Filling in lsdb
  NS_LOG_INFO("Update the lsdb entry");
  m_routerLsdb[originRouterId] = routerLsa;
  m_seqNumbers[originRouterId] = seqNum;

  // Update routing table
  UpdateRouting();

  // Flood LSU to all interfaces except for receiving one
  Ptr<Packet> p = ConstructLSUPacket(ospfHeader, lsaHeader, routerLsa);
  FloodLSU(p, ifIndex);

  // Send ACK to sender
  auto ackPacket = ConstructLSAckPacket(m_routerId, m_areaId, lsaHeader);
  SendAck(ifIndex, ackPacket, Ipv4Address(originRouterId));
  NS_LOG_DEBUG("Sending ACK [" << ackPacket->GetSize() << "] from " << m_routerId << " to interface " << ifIndex);
}

void
OspfApp::PrintLsdb() {
  if (m_routerLsdb.empty()) return;
  std::cout << "===== Router ID: " << m_routerId << " =====" << std::endl;
  for (auto& pair : m_routerLsdb) {
    std::cout << "At t=" << Simulator::Now().GetSeconds() << " , Router: " << Ipv4Address(pair.first) << std::endl;
    std::cout << "  Neighbors: " << pair.second->GetNLink() << std::endl;
    for (uint32_t i = 0; i < pair.second->GetNLink(); i++) {
      RouterLink link = pair.second->GetLink(i);
      std::cout << "  (" << Ipv4Address(link.m_linkData) << ", " << link.m_metric <<  ")" << std::endl;
    }
  }
  std::cout << std::endl;
}

void
OspfApp::PrintRouting(std::filesystem::path dirName, std::string filename) {
  try {
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (dirName / filename, std::ios::out);
    m_routing->PrintRoutingTable(routingStream);
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

void
OspfApp::PrintAreas() {
  std::cout << "Area:";
  for (uint32_t i = 1; i < m_ospfInterfaces.size(); i++) {
    std::cout << " " << m_ospfInterfaces[i]->GetArea();
  }
  std::cout << std::endl;
}

uint32_t
OspfApp::GetLsdbHash() {
  std::stringstream ss;
  for (auto& pair : m_routerLsdb) {
    ss << Ipv4Address(pair.first) << std::endl;
    ss << Ipv4Address(pair.first) << std::endl;
    for (uint32_t i = 0; i < pair.second->GetNLink(); i++) {
      RouterLink link = pair.second->GetLink(i);
      ss << "  (" << Ipv4Address(link.m_linkData) << Ipv4Address(link.m_metric) <<  ")" << std::endl;
    }
  }
  std::hash<std::string> hasher;
  return hasher(ss.str());
}

void
OspfApp::PrintLsdbHash() {
  std::cout << GetLsdbHash() << std::endl;
}

void
OspfApp::UpdateRouting() {
  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  // <distance, next hop>
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t> >, std::greater<std::pair<uint32_t, uint32_t>> > pq; 
  NS_LOG_FUNCTION(this);

  // Remove old route
  // std::cout << "Number of Route: " << m_boundDevices.GetN() << std::endl;
  while (m_routing->GetNRoutes() > m_boundDevices.GetN()) {
    m_routing->RemoveRoute(m_boundDevices.GetN());
  }

  // Dijkstra
  while (!pq.empty()) {
    pq.pop();
  }
  distanceTo.clear();
  uint32_t u, v, w;
  pq.emplace(0, m_routerId.Get());
  distanceTo[m_routerId.Get()] = 0;
  while(!pq.empty()) {
    std::tie(w, u) = pq.top();
    pq.pop();
    // Skip if the lsdb doesn't have any neighbors for that router
    if (m_routerLsdb.find(u) == m_routerLsdb.end()) continue;

    for (uint32_t i = 0; i < m_routerLsdb[u]->GetNLink(); i++) {
      v = m_routerLsdb[u]->GetLink(i).m_linkId;
      auto metric = m_routerLsdb[u]->GetLink(i).m_metric;
      if (distanceTo.find(v) == distanceTo.end() || w + metric < distanceTo[v]) {
        distanceTo[v] = w + metric;
        prevHop[v] = u;
        pq.emplace(w + metric, v);
      }
    }
  }
  // std::cout << "node: " << GetNode()->GetId() << std::endl;
  // Shortest path information for each subnet -- <mask, nextHop, interface, metric>
  std::map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> > routingEntries;
  for (auto& [remoteRouterId, routerLsa] : m_routerLsdb) {
    // std::cout << "  destination: " << Ipv4Address(remoteRouterId) << std::endl;

    // No reachable path
    if (prevHop.find(remoteRouterId) == prevHop.end()) {
      // std::cout << "    no route" << std::endl;
      continue;
    }

    // Find the first hop
    v = remoteRouterId;
    while (prevHop.find(v) != prevHop.end()) {
      if (prevHop[v] == m_routerId.Get()) {
        break;
      }
      v = prevHop[v];
    }

    // Find the next hop's IP and interface index
    uint32_t ifIndex = 0;
    for (uint32_t i = 1; i < m_ospfInterfaces.size(); i++) {
      auto neighbors = m_ospfInterfaces[i]->GetNeighbors();
      for (auto n : neighbors) {
        if (n->GetRouterId().Get() == v) {
          ifIndex = i;
          break;
        }
      }
      if (ifIndex) break;
    }
    NS_ASSERT(ifIndex > 0);
    for(uint32_t i = 0; i < routerLsa->GetNLink(); i++) {
      NS_LOG_DEBUG("Add route: " << Ipv4Address(routerLsa->GetLink(i).m_linkData) << ", "
                    << ifIndex << ", " << distanceTo[remoteRouterId]);
      m_routing->AddHostRouteTo(Ipv4Address(routerLsa->GetLink(i).m_linkData), ifIndex, distanceTo[remoteRouterId]);
    }
    
  }
}

void 
OspfApp::HandleRead (Ptr<Socket> socket)
{
  // NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  packet = socket->RecvFrom (from);

  // NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S) << " server received " << packet->GetSize () << " bytes from " <<
  //                      InetSocketAddress::ConvertFrom (from).GetIpv4 ());
  // Update last received hello message
  Ipv4Header ipHeader;
  OspfHeader ospfHeader;
  
  packet->PeekHeader(ipHeader);
  // NS_LOG_DEBUG("Packet Size: " << packet->GetSize() << ", Header Size: " << sizeof (ipHeader) << ", Payload Size " << ipHeader.GetPayloadSize());
  packet->RemoveHeader(ipHeader);

  packet->RemoveHeader(ospfHeader);

  
  Ipv4Address remoteRouterId;
  // NS_LOG_INFO("Packet Type: " << ospfHeader.OspfTypeToString(ospfHeader.GetType()));
  if (ospfHeader.GetType() == OspfHeader::OspfType::OspfHello) {
    Ptr<OspfHello> hello = Create<OspfHello>(packet);
    HandleHello(socket->GetBoundNetDevice()->GetIfIndex(), ipHeader, ospfHeader, hello);
  } else if (ospfHeader.GetType() == OspfHeader::OspfType::OspfLSUpdate) {
    // Read LSU packet and handle the payload
    LsaHeader lsaHeader;
    packet->RemoveHeader(lsaHeader);
    Ptr<RouterLsa> routerLsa = Create<RouterLsa>(packet);
    HandleRouterLSU(socket->GetBoundNetDevice()->GetIfIndex(), ospfHeader, lsaHeader, routerLsa);
  } else if (ospfHeader.GetType() == OspfHeader::OspfType::OspfLSAck) {
    // Strip lsa headers until the payload runs out
    std::vector<LsaHeader> lsaHeaders;
    while (packet->GetSize() > 0) {
      LsaHeader lsaHeader;
      packet->RemoveHeader(lsaHeader);
      lsaHeaders.emplace_back(lsaHeader);
    }
    HandleLSAck(socket->GetBoundNetDevice()->GetIfIndex(), ospfHeader, lsaHeaders);
  } else {
    NS_LOG_ERROR("Unknown packet type");
  }
  // if (packet->PeekHeader())
}

} // Namespace ns3
