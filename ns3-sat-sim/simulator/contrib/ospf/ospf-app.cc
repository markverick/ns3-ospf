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

#include "tuple"

#include "ospf-app.h"
#include "ospf-packet-helper.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OSPFApp");

NS_OBJECT_ENSURE_REGISTERED (OSPFApp);

TypeId
OSPFApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OSPFApp")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<OSPFApp> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&OSPFApp::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("HelloInterval", "OSPF Hello Interval",
                   TimeValue (Seconds(10)),
                   MakeTimeAccessor (&OSPFApp::m_helloInterval),
                   MakeTimeChecker ())
    .AddAttribute ("HelloAddress", "Broadcast address of Hello",
                   Ipv4AddressValue (Ipv4Address("224.0.0.5")),
                   MakeIpv4AddressAccessor (&OSPFApp::m_helloAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("NeighborTimeout", "Link is considered down when not receiving Hello until NeighborTimeout",
                   TimeValue (Seconds(30)),
                   MakeTimeAccessor (&OSPFApp::m_neighborTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("LSUInterval", "LSU Interval",
                   TimeValue (Seconds(30)),
                   MakeTimeAccessor (&OSPFApp::m_lsuInterval),
                   MakeTimeChecker ())
    .AddAttribute ("TTL", "Time To Live",
                   UintegerValue (64),
                   MakeUintegerAccessor (&OSPFApp::m_ttl),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&OSPFApp::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&OSPFApp::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&OSPFApp::m_txTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&OSPFApp::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
  ;
  return tid;
}

OSPFApp::OSPFApp ()
{
  NS_LOG_FUNCTION (this);
}

OSPFApp::~OSPFApp()
{
  NS_LOG_FUNCTION (this);
  m_sockets.clear();
}

void
OSPFApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
OSPFApp::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  for (uint32_t i = 1; i < m_boundDevices.GetN(); i++)
  {
    // Create sockets
    TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
    
    auto socket = Socket::CreateSocket (GetNode (), tid);
    // auto ipv4 = GetNode()->GetObject<Ipv4>();
    // Ipv4Address addr = ipv4->GetAddress(i, 0).GetAddress();
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny());
    if (socket->Bind (local) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
    socket->Connect (InetSocketAddress (m_helloAddress));
    socket->SetAllowBroadcast (true);
    socket->SetAttribute("Protocol", UintegerValue(89));
    socket->BindToNetDevice(m_boundDevices.Get(i));
    socket->SetRecvCallback (MakeCallback (&OSPFApp::HandleRead, this));
    m_sockets.emplace_back(socket);

  }
  ScheduleTransmitHello (Seconds (0.));
}

void 
OSPFApp::SetBoundNetDevices (NetDeviceContainer devs)
{
  NS_LOG_FUNCTION (this << devs.GetN());
  m_boundDevices = devs;
  m_lastHelloReceived.resize(devs.GetN());
  m_helloTimeouts.resize(devs.GetN());

  // Create interface database
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4> ();
  Ptr<OSPFInterface> ospfInterface= CreateObject<OSPFInterface> ();
  m_ospfInterfaces.emplace_back(ospfInterface);
  for (uint32_t i = 1; i < m_boundDevices.GetN(); i++) {
    auto sourceIp = ipv4->GetAddress(i, 0).GetAddress();
    auto mask = ipv4->GetAddress(i, 0).GetMask();
    Ptr<OSPFInterface> ospfInterface= CreateObject<OSPFInterface> (sourceIp, mask, m_helloInterval.GetSeconds());
    m_ospfInterfaces.emplace_back(ospfInterface);
  }
}

void 
OSPFApp::SetRouterId (Ipv4Address routerId)
{
  m_routerId = routerId;
}

void 
OSPFApp::ScheduleTransmitHello (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &OSPFApp::SendHello, this);
}

void 
OSPFApp::SendHello (void)
{
  if (m_sockets.empty()) return;

  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  NS_LOG_DEBUG("Router ID: " << m_routerId);


  Address localAddress;
  for (uint32_t i = 0; i < m_sockets.size(); i++)
  {
    auto socket = m_sockets[i];
    socket->GetSockName (localAddress);
    // call to the trace sinks before the packet is actually sent,
    // so that tags added to the packet can be sent as well
    Ptr<Packet> p = ConstructHelloPayload(Ipv4Address::ConvertFrom (m_routerId), m_areaId,
                    m_ospfInterfaces[i + 1]->GetMask(), m_helloInterval.GetSeconds(), m_neighborTimeout.GetSeconds());
    m_txTrace (p);
    if (Ipv4Address::IsMatchingType (m_helloAddress))
    {
      m_txTraceWithAddresses (p, localAddress, InetSocketAddress (Ipv4Address::ConvertFrom (m_helloAddress)));
    }
    socket->Connect (InetSocketAddress (m_helloAddress));
    // NS_LOG_DEBUG(socket->GetBoundNetDevice()->GetIfIndex());
    // InetSocketAddress sourceAddress = InetSocketAddress::ConvertFrom(localAddress);
    // NS_LOG_DEBUG("Source Address: " << sourceAddress.GetIpv4());
    socket->Send (p);
    if (Ipv4Address::IsMatchingType (m_helloAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S) << " client sent " << p->GetSize() << " bytes to " <<
                    m_helloAddress << " via interface " << i + 1 << " : " << m_ospfInterfaces[i+1]->GetAddress());
    }

  }
  if (!m_sockets.empty())
  {
    ScheduleTransmitHello (m_helloInterval);
  }
}

bool
OSPFApp::IsDeviceAlive(uint32_t ifIndex) {
  if (m_lastHelloReceived[ifIndex].IsZero() || Simulator::Now() - m_lastHelloReceived[ifIndex] > m_neighborTimeout) {
    return false;
  }
  return true;
}

void 
OSPFApp::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  for (auto socket : m_sockets)
  {
    socket->Close ();
    socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    socket = 0;
  }
  m_sockets.clear();
}

void
OSPFApp::SetRouting (Ptr<Ipv4StaticRouting> routing) {
  m_routing = routing;
} 

void
OSPFApp::LinkDown (Ptr<OSPFInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp) {
  NS_LOG_FUNCTION (ospfInterface->GetAddress() << ospfInterface->GetNeighbors().size() << remoteRouterId << remoteIp);
  NeighberInterface neighbor(remoteRouterId, remoteIp);
  ospfInterface->RemoveNeighbor(neighbor);
  RefreshLSUTimer();
  NS_LOG_DEBUG("Interface " << ospfInterface->GetAddress() << " now has " << ospfInterface->GetNeighbors().size() << " neighbors");
}

void
OSPFApp::LinkUp (Ptr<OSPFInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp) {
  NS_LOG_FUNCTION (ospfInterface->GetAddress() << ospfInterface->GetNeighbors().size() << remoteRouterId << remoteIp);
  NeighberInterface neighbor(remoteRouterId, remoteIp);
  ospfInterface->AddNeighbor(neighbor);
  RefreshLSUTimer();
  NS_LOG_DEBUG("Interface " << ospfInterface->GetAddress() << " now has " << ospfInterface->GetNeighbors().size() << " neighbors");
}

void
OSPFApp::RefreshHelloTimer(uint32_t ifIndex, Ptr<OSPFInterface> ospfInterface, Ipv4Address remoteRouterId, Ipv4Address remoteIp)
{
// Refresh the timer
  if (m_helloTimeouts[ifIndex].IsRunning()) {
    m_helloTimeouts[ifIndex].Remove();
    m_helloTimeouts[ifIndex] = Simulator::Schedule(m_neighborTimeout, &OSPFApp::LinkDown, this, ospfInterface, remoteRouterId, remoteIp);
  } else {
    m_helloTimeouts[ifIndex] = Simulator::Schedule(m_neighborTimeout, &OSPFApp::LinkDown, this, ospfInterface, remoteRouterId, remoteIp);
  }
}

// Only flood all neighbors, neighbors will create another LSU to flood again.
// This is why TTL is not used from IP header
// Will repeat if this node is the originator
void
OSPFApp::FloodLSU(Ptr<Packet> lsuPayload, uint32_t inputIfIndex) {
  NS_LOG_FUNCTION (this << lsuPayload->GetSize() << inputIfIndex);
  for (uint32_t i = 0; i < m_sockets.size(); i++)
  {
    // Skip the incoming interface
    if (inputIfIndex == i + 1) continue;

    // Copy the LSU to be sent over
    auto p = lsuPayload->Copy();
    auto socket = m_sockets[i];
    Address localAddress;
    socket->GetSockName (localAddress);
    // call to the trace sinks before the packet is actually sent,
    // so that tags added to the packet can be sent as well
    m_txTrace (p);
    auto neighbors = m_ospfInterfaces[i + 1]->GetNeighbors();
    // Send to every neighbor (only 1 neighbor for point-to-point)
    for (auto n : neighbors) {
      // NS_LOG_DEBUG("Remote IP: " << n.remoteIpAddress);
      socket->Connect (InetSocketAddress (n.remoteIpAddress));
      socket->Send (p);
    }
  }
  if (!m_sockets.empty() && inputIfIndex == 0)
  {
    m_seqNumbers[m_routerId.Get()]++;
    m_lsuTimeout = Simulator::Schedule(m_lsuInterval, &OSPFApp::FloodLSU, this, CopyAndIncrementSeqNumber(lsuPayload), 0);
  }
}

void
OSPFApp::RefreshLSUTimer() {
  NS_LOG_FUNCTION (this);
  if (m_lsuTimeout.IsRunning()) {
    m_lsuTimeout.Remove();
  }
  // Generate shared advertisements data
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > advertisements;
  for (auto interface : m_ospfInterfaces) {
    auto ifAds = interface->GetLSAdvertisement();
    for (auto a : ifAds) {
      advertisements.emplace_back(a);
    }
  }
  if (m_seqNumbers.find(m_routerId.Get()) == m_seqNumbers.end()) {
    m_seqNumbers[m_routerId.Get()] = 0;
  }
  Ptr<Packet> p = ConstructLSUPayload(m_routerId, m_areaId, m_seqNumbers[m_routerId.Get()] + 1, m_ttl, advertisements);
  m_lsdb[m_routerId.Get()] = advertisements;
  UpdateRouting();
  FloodLSU(p, 0);
  // m_lsuTimeout = Simulator::Schedule(m_lsuInterval, &OSPFApp::FloodLSU, this, p, 0);
}

// std::vector<Ptr<OSPFInterface> > ospfInterfaces
void 
OSPFApp::HandleHello (uint32_t ifIndex, Ipv4Address remoteRouterId, Ipv4Address remoteIp)
{
  NS_LOG_FUNCTION (this << ifIndex << remoteRouterId << remoteIp);

  // Skip checking for invalid/corrupted packet
  // New Neighbor
  Ptr<OSPFInterface> ospfInterface = m_ospfInterfaces[ifIndex];
  if (m_lastHelloReceived[ifIndex].IsZero()) {
    NS_LOG_INFO("New neighbor detected from interface " << ifIndex);
    
    // Add neighbor to the interface
    LinkUp (ospfInterface, remoteRouterId, remoteIp);
    // m_lsdb
  }
  // Neighbor Tiemout
  if (Simulator::Now() - m_lastHelloReceived[ifIndex] > m_neighborTimeout) {
    NS_LOG_INFO("Re-added timed out interface " << ifIndex);
    LinkUp (ospfInterface, remoteRouterId, remoteIp);
  }
  // Update last hello received
  m_lastHelloReceived[ifIndex] = Simulator::Now();

  RefreshHelloTimer(ifIndex, ospfInterface, remoteRouterId, remoteIp);
}

void 
OSPFApp::HandleLSU (uint32_t ifIndex, Ptr<Packet> lsuPayload)
{
  NS_LOG_FUNCTION(this << ifIndex << lsuPayload->GetSize());
  // Prepare buffer
  auto payloadSize = lsuPayload->GetSize();
  uint8_t *buffer = new uint8_t[payloadSize];
  lsuPayload->CopyData(buffer, payloadSize);

  // If the LSU was originally generated by the receiving router, the packet is dropped.
  Ipv4Address originRouterId = Ipv4Address::Deserialize(buffer + 4);
  uint32_t originRouterId_ = originRouterId.Get();
  if (originRouterId == m_routerId) {
    NS_LOG_INFO("LSU is dropped, received LSU has originated here");
    return;
  }
  uint16_t seqNum = static_cast<int>(buffer[24] << 8) + static_cast<int>(buffer[25]);

  // If the sequence number equals or less than that of the last packet received from the
  // originating router, the packet is dropped.
  if (m_seqNumbers.find(originRouterId_) == m_seqNumbers.end()) {
    m_seqNumbers[originRouterId_] = 0;
  }
  if (seqNum <= m_seqNumbers[originRouterId_]) {
    NS_LOG_INFO("LSU is dropped, received sequence number <= stored sequence number");
    return;
  }

  // Filling in lsdb
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t> > lsAdvertisements;
  // std::cout << "Read advertisements: " << std::endl;
  for (uint32_t i = 32; i < payloadSize; i+=12) {
    const auto& [subnet, mask, remoteRouterId] = GetAdvertisement(buffer + i);
    // std::cout << "  [" << (i) << "](" << subnet << ", " << mask << ", " << remoteRouterId << ")" << std::endl;
    lsAdvertisements.emplace_back(std::make_tuple(subnet.Get(), mask.Get(), remoteRouterId.Get()));
  }
  if (m_lsdb.find(originRouterId_) != m_lsdb.end() && m_lsdb[originRouterId_] == lsAdvertisements) {
    // If the packet contents are equivalent
    // to the contents of the packet last received from the originating router, the
    // database entry is updated with the new sequence number.
    NS_LOG_INFO("Receive the same lsdb entry, sequence number updated");
    m_seqNumbers[originRouterId_] = seqNum;
  }
  else if (m_lsdb.find(originRouterId_) == m_lsdb.end()) {

    // If the LSU is from a router
    // not currently in the database, the packets contents are used to update the database
    // and recompute the forwarding table.
    NS_LOG_INFO("Receive a new lsdb entry");
    m_lsdb[originRouterId_] = lsAdvertisements;
    m_seqNumbers[originRouterId_] = seqNum;
    UpdateRouting();
  }
  else if (m_lsdb.find(originRouterId_) != m_lsdb.end()) {
    // Finally, if the LSU data is for a router
    // currently in the database but the information has changed, the LSU is used to update
    // the database, and forwarding table is recomputed.
    NS_LOG_INFO("Update the lsdb entry");
    m_lsdb[originRouterId_] = lsAdvertisements;
    m_seqNumbers[originRouterId_] = seqNum;
    UpdateRouting();
  }
  auto p = CopyAndDecrementTtl(lsuPayload);
  if (p) {
    FloodLSU(p, ifIndex);
  }
}

void
OSPFApp::PrintLSDB() {
  for (const auto& pair : m_lsdb) {
    std::cout << "Router: " << Ipv4Address(pair.first) << std::endl;
    std::cout << "  Neighbors:" << std::endl;

    for (const auto& tup : pair.second) {
        uint32_t val1, val2, val3;
        std::tie(val1, val2, val3) = tup;
        std::cout << "  (" << Ipv4Address(val1) << ", " << Ipv4Mask(val2) << ", " << Ipv4Address(val3) << ")" << std::endl;
    }
  }
  std::cout << std::endl;
}

uint32_t
OSPFApp::GetLSDBHash() {
  std::stringstream ss;
  for (const auto& pair : m_lsdb) {
    ss << Ipv4Address(pair.first) << std::endl;
    ss << std::endl;

    for (const auto& tup : pair.second) {
        uint32_t val1, val2, val3;
        std::tie(val1, val2, val3) = tup;
        ss << Ipv4Address(val1) << "," << Ipv4Mask(val2) << "," << Ipv4Address(val3) << std::endl;
    }
  }
  std::hash<std::string> hasher;
  return hasher(ss.str());
}

void
OSPFApp::PrintLSDBHash() {
  std::cout << GetLSDBHash() << std::endl;
}

void
OSPFApp::UpdateRouting() {
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

  // Disjkstra
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
    for (uint32_t i = 0; i < m_lsdb[u].size(); i++) {
      v = std::get<2>(m_lsdb[u][i]);
      if (distanceTo.find(v) == distanceTo.end() || w + 1 < distanceTo[v]) {
        distanceTo[v] = w + 1;
        prevHop[v] = u;
        pq.emplace(w + 1, v);
      }
    }
  }
  // std::cout << "node: " << GetNode()->GetId() << std::endl; 
  for (const auto& [remoteRouterId, remoteNeighbors] : m_lsdb) {
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

    // Iterate over the vector of tuples <subnet, mask, neighbor's router ID>
    // Check which neighbors is its best next hop
    for (uint32_t i = 1; i < m_ospfInterfaces.size(); i++) {
        if (m_ospfInterfaces[i]->isNeighbor(Ipv4Address(v))) {
          // std::cout << "    route added: (" << Ipv4Address(remoteRouterId) << ", " << i << ", " << distanceTo[remoteRouterId] << ")" << std::endl;
          m_routing->AddHostRouteTo(Ipv4Address(remoteRouterId), i, distanceTo[remoteRouterId]);
        }
    }
  }
  // std::cout << std::endl;
}

void 
OSPFApp::HandleRead (Ptr<Socket> socket)
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
  
  packet->PeekHeader(ipHeader);
  Ipv4Address remoteIp = ipHeader.GetSource();
  // NS_LOG_DEBUG("Packet Size: " << packet->GetSize() << ", Header Size: " << sizeof (ipHeader) << ", Payload Size " << ipHeader.GetPayloadSize());
  packet->RemoveHeader(ipHeader);
  uint32_t payloadSize = packet->GetSize();

  uint8_t *buffer = new uint8_t[payloadSize];
  packet->CopyData(buffer, payloadSize);
  int type = static_cast<int>(buffer[1]);
  Ipv4Address remoteRouterId;
  if (type == 0x01) {
    remoteRouterId = Ipv4Address::Deserialize(buffer + 4);
    HandleHello(socket->GetBoundNetDevice()->GetIfIndex(), remoteRouterId, remoteIp);
  } else if (type == 0x04) {
    HandleLSU(socket->GetBoundNetDevice()->GetIfIndex(), packet);
  } else {
    NS_LOG_ERROR("Unknown packet type");
  }
  // if (packet->PeekHeader())
  delete[] buffer;
}

} // Namespace ns3
