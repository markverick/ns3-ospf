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

#include "filesystem"
#include "tuple"
#include "unordered_set"

#include "ospf-app.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OspfApp");

NS_OBJECT_ENSURE_REGISTERED (OspfApp);

TypeId
OspfApp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::OspfApp")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<OspfApp> ()
          .AddAttribute ("HelloInterval", "OSPF Hello Interval", TimeValue (Seconds (10)),
                         MakeTimeAccessor (&OspfApp::m_helloInterval), MakeTimeChecker ())
          .AddAttribute ("HelloAddress", "Multicast address of Hello",
                         Ipv4AddressValue (Ipv4Address ("224.0.0.5")),
                         MakeIpv4AddressAccessor (&OspfApp::m_helloAddress),
                         MakeIpv4AddressChecker ())
          .AddAttribute ("LSAAddress", "Multicast address of LSAs",
                         Ipv4AddressValue (Ipv4Address ("224.0.0.6")),
                         MakeIpv4AddressAccessor (&OspfApp::m_lsaAddress),
                         MakeIpv4AddressChecker ())
          .AddAttribute (
              "RouterDeadInterval",
              "Link is considered down when not receiving Hello until RouterDeadInterval",
              TimeValue (Seconds (30)), MakeTimeAccessor (&OspfApp::m_routerDeadInterval),
              MakeTimeChecker ())
          .AddAttribute ("LSUInterval", "LSU Retransmission Interval", TimeValue (Seconds (5)),
                         MakeTimeAccessor (&OspfApp::m_rxmtInterval), MakeTimeChecker ())
          .AddAttribute ("DefaultArea", "Default area ID for interfaces", UintegerValue (0),
                         MakeUintegerAccessor (&OspfApp::m_areaId),
                         MakeUintegerChecker<uint32_t> ())
          .AddAttribute ("EnableAreaProxy", "Enable area proxy for area routing",
                         BooleanValue (true), MakeBooleanAccessor (&OspfApp::m_enableAreaProxy),
                         MakeBooleanChecker ())
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
                           "ns3::Packet::TwoAddressTracedCallback");
  return tid;
}

OspfApp::OspfApp ()
{
  NS_LOG_FUNCTION (this);
}

OspfApp::~OspfApp ()
{
  NS_LOG_FUNCTION (this);
  m_sockets.clear ();
  m_lsaSockets.clear ();
  m_helloSockets.clear ();
}

void
OspfApp::SetRouting (Ptr<Ipv4StaticRouting> routing)
{
  m_routing = routing;
}

void
OspfApp::SetBoundNetDevices (NetDeviceContainer devs)
{
  NS_LOG_FUNCTION (this << devs.GetN ());
  m_boundDevices = devs;
  m_lastHelloReceived.resize (devs.GetN ());
  m_helloTimeouts.resize (devs.GetN ());

  // Create interface database
  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();

  // Add loopbacks at index 0
  Ptr<OspfInterface> ospfInterface = Create<OspfInterface> ();
  m_ospfInterfaces.emplace_back (ospfInterface);

  // Add the rest of net devices
  for (uint32_t i = 1; i < m_boundDevices.GetN (); i++)
    {
      auto sourceIp = ipv4->GetAddress (i, 0).GetAddress ();
      auto mask = ipv4->GetAddress (i, 0).GetMask ();
      Ptr<OspfInterface> ospfInterface = Create<OspfInterface> (
          sourceIp, mask, m_helloInterval.GetSeconds (), m_routerDeadInterval.GetSeconds (),
          m_areaId, 1, m_boundDevices.Get (i)->GetMtu ());
      m_ospfInterfaces.emplace_back (ospfInterface);
    }
}

void
OspfApp::SetMask (Ipv4Mask mask)
{
  m_mask = mask;
}

void
OspfApp::SetAreas (uint32_t area)
{
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      m_ospfInterfaces[i]->SetArea (area);
    }
  m_areaId = area;
}

void
OspfApp::SetAreas (std::vector<uint32_t> areas)
{
  NS_ASSERT_MSG (areas.size () == m_ospfInterfaces.size (),
                 "The length of areas must match the number of interfaces");
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      m_ospfInterfaces[i]->SetArea (areas[i]);
    }
  // Loopback is the router area (only used for alt area)
  m_areaId = areas[0];
}

void
OspfApp::SetMetrices (std::vector<uint32_t> metrices)
{
  NS_ASSERT_MSG (metrices.size () == m_ospfInterfaces.size (),
                 "The length of metrices must match the number of interfaces");
  for (uint32_t i = 0; i < m_ospfInterfaces.size (); i++)
    {
      m_ospfInterfaces[i]->SetMetric (metrices[i]);
    }
}

void
OspfApp::SetRouterId (Ipv4Address routerId)
{
  m_routerId = routerId;
}

std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>>
OspfApp::GetLsdb ()
{
  return m_routerLsdb;
}

void
OspfApp::PrintLsdb ()
{
  if (m_routerLsdb.empty ())
    return;
  std::cout << "===== L1 Router ID: " << m_routerId << " Area ID: " << m_areaId
            << " =====" << std::endl;
  for (auto &pair : m_routerLsdb)
    {
      std::cout << "At t=" << Simulator::Now ().GetSeconds ()
                << " , Router: " << Ipv4Address (pair.first) << std::endl;
      std::cout << "  Neighbors: " << pair.second.second->GetNLink () << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          RouterLink link = pair.second.second->GetLink (i);
          std::cout << "  (" << Ipv4Address (link.m_linkData) << ", " << link.m_metric << ", "
                    << (uint32_t) (link.m_type) << ")" << std::endl;
        }
    }
  std::cout << std::endl;
}

void
OspfApp::PrintAreaLsdb ()
{
  if (m_areaLsdb.empty ())
    return;
  std::cout << "===== L2 Router ID: " << m_routerId << " Area ID: " << m_areaId
            << " =====" << std::endl;
  for (auto &pair : m_areaLsdb)
    {
      std::cout << "At t=" << Simulator::Now ().GetSeconds ()
                << " , Area: " << Ipv4Address (pair.first) << std::endl;
      std::cout << "  Neighbors: " << pair.second.second->GetNLink () << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          AreaLink link = pair.second.second->GetLink (i);
          std::cout << "  (" << Ipv4Address (link.m_areaId) << ", "
                    << Ipv4Address (link.m_ipAddress) << ", " << link.m_metric << ")" << std::endl;
        }
    }
  std::cout << std::endl;
}

void
OspfApp::PrintRouting (std::filesystem::path dirName, std::string filename)
{
  try
    {
      Ptr<OutputStreamWrapper> routingStream =
          Create<OutputStreamWrapper> (dirName / filename, std::ios::out);
      m_routing->PrintRoutingTable (routingStream);
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error: " << e.what () << std::endl;
    }
}

void
OspfApp::PrintAreas ()
{
  std::cout << "Area:";
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      std::cout << " " << m_ospfInterfaces[i]->GetArea ();
    }
  std::cout << std::endl;
}

uint32_t
OspfApp::GetLsdbHash ()
{
  std::stringstream ss;
  for (auto &pair : m_routerLsdb)
    {
      ss << Ipv4Address (pair.first) << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          RouterLink link = pair.second.second->GetLink (i);
          ss << "  (" << Ipv4Address (link.m_linkData) << Ipv4Address (link.m_metric) << ")"
             << std::endl;
        }
    }
  std::hash<std::string> hasher;
  return hasher (ss.str ());
}

uint32_t
OspfApp::GetAreaLsdbHash ()
{
  std::stringstream ss;
  for (auto &pair : m_areaLsdb)
    {
      ss << Ipv4Address (pair.first) << std::endl;
      for (uint32_t i = 0; i < pair.second.second->GetNLink (); i++)
        {
          AreaLink link = pair.second.second->GetLink (i);
          ss << "  (" << Ipv4Address (link.m_areaId) << Ipv4Address (link.m_ipAddress)
             << Ipv4Address (link.m_metric) << ")" << std::endl;
        }
    }
  std::hash<std::string> hasher;
  return hasher (ss.str ());
}

void
OspfApp::PrintLsdbHash ()
{
  std::cout << GetLsdbHash () << std::endl;
}

void
OspfApp::PrintAreaLsdbHash ()
{
  std::cout << GetAreaLsdbHash () << std::endl;
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
  m_randomVariable->SetAttribute ("Min", DoubleValue (0.0)); // Minimum value in seconds
  m_randomVariable->SetAttribute ("Max", DoubleValue (0.005)); // Maximum value in seconds (5 ms)

  m_randomVariableSeq->SetAttribute ("Min", DoubleValue (0.0));
  m_randomVariableSeq->SetAttribute ("Max", DoubleValue ((1 << 16) * 1000)); // arbitrary number

  // Add local null sockets
  m_sockets.emplace_back (nullptr);
  m_helloSockets.emplace_back (nullptr);
  m_lsaSockets.emplace_back (nullptr);
  for (uint32_t i = 1; i < m_boundDevices.GetN (); i++)
    {
      // Create sockets
      TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");

      // auto ipv4 = GetNode()->GetObject<Ipv4>();
      // Ipv4Address addr = ipv4->GetAddress(i, 0).GetAddress();
      InetSocketAddress anySocketAddress (Ipv4Address::GetAny ());

      // For Hello, both bind and listen to m_helloAddress
      auto helloSocket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress helloSocketAddress (m_helloAddress);
      if (helloSocket->Bind (helloSocketAddress) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      helloSocket->Connect (helloSocketAddress);
      helloSocket->SetAllowBroadcast (true);
      helloSocket->SetAttribute ("Protocol", UintegerValue (89));
      helloSocket->BindToNetDevice (m_boundDevices.Get (i));
      helloSocket->SetRecvCallback (MakeCallback (&OspfApp::HandleRead, this));
      m_helloSockets.emplace_back (helloSocket);

      // For LSA, both bind and listen to m_lsaAddress
      auto lsaSocket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress lsaSocketAddress (m_lsaAddress);
      if (lsaSocket->Bind (lsaSocketAddress) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      lsaSocket->Connect (lsaSocketAddress);
      lsaSocket->SetAllowBroadcast (true);
      lsaSocket->SetAttribute ("Protocol", UintegerValue (89));
      lsaSocket->SetIpTtl (1);
      lsaSocket->BindToNetDevice (m_boundDevices.Get (i));
      m_lsaSockets.emplace_back (lsaSocket);

      // For unicast, such as LSA retransmission, bind to local address
      auto unicastSocket = Socket::CreateSocket (GetNode (), tid);
      if (unicastSocket->Bind (anySocketAddress) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      unicastSocket->SetAllowBroadcast (true);
      unicastSocket->SetAttribute ("Protocol", UintegerValue (89));
      lsaSocket->SetIpTtl (1); // Only allow local hop
      unicastSocket->BindToNetDevice (m_boundDevices.Get (i));
      unicastSocket->SetRecvCallback (MakeCallback (&OspfApp::HandleRead, this));
      m_sockets.emplace_back (unicastSocket);
    }
  // Start sending Hello
  ScheduleTransmitHello (Seconds (0.));

  m_isAreaLeader = false;
  // Will begin as an area leader if noone will
  if (m_enableAreaProxy)
    {
      m_areaLeaderBeginTimer =
          Simulator::Schedule (m_routerDeadInterval + Seconds (m_randomVariable->GetValue ()),
                               &OspfApp::AreaLeaderBegin, this);
    }
}

void
OspfApp::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  for (auto timer : m_helloTimeouts)
    {
      timer.Remove ();
    }
  for (uint32_t i = 1; i < m_sockets.size (); i++)
    {
      // Hello
      m_helloSockets[i]->Close ();
      m_helloSockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
      m_helloSockets[i] = 0;

      // LSA
      m_lsaSockets[i]->Close ();
      m_lsaSockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
      m_lsaSockets[i] = 0;

      // Unicast
      m_sockets[i]->Close ();
      m_sockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
      m_sockets[i] = 0;
    }
  m_helloSockets.clear ();
  m_lsaSockets.clear ();
  m_sockets.clear ();
}

void
OspfApp::ScheduleTransmitHello (Time dt)
{
  m_helloEvent =
      Simulator::Schedule (dt + Seconds (m_randomVariable->GetValue ()), &OspfApp::SendHello, this);
}

void
OspfApp::SendHello ()
{
  if (m_helloSockets.empty ())
    return;

  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_helloEvent.IsExpired ());

  Address helloSocketAddress;
  for (uint32_t i = 1; i < m_helloSockets.size (); i++)
    {
      auto socket = m_helloSockets[i];
      socket->GetSockName (helloSocketAddress);
      Ptr<Packet> p = ConstructHelloPacket (
          Ipv4Address::ConvertFrom (m_routerId), m_ospfInterfaces[i]->GetArea (),
          m_ospfInterfaces[i]->GetMask (), m_ospfInterfaces[i]->GetHelloInterval (),
          m_ospfInterfaces[i]->GetRouterDeadInterval (), m_ospfInterfaces[i]->GetNeighbors ());
      m_txTrace (p);
      if (Ipv4Address::IsMatchingType (m_helloAddress))
        {
          m_txTraceWithAddresses (p, helloSocketAddress,
                                  InetSocketAddress (Ipv4Address::ConvertFrom (m_helloAddress)));
        }
      socket->SendTo (p, 0, InetSocketAddress (Ipv4Address::ConvertFrom (m_helloAddress)));
      if (Ipv4Address::IsMatchingType (m_helloAddress))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S) << " client sent "
                                  << p->GetSize () << " bytes to " << m_helloAddress
                                  << " via interface " << i << " : "
                                  << m_ospfInterfaces[i]->GetAddress ());
        }
    }
  if (!m_helloSockets.empty ())
    {
      ScheduleTransmitHello (m_helloInterval);
    }
}

void
OspfApp::SendAck (uint32_t ifIndex, Ptr<Packet> ackPacket, Ipv4Address (originRouterId))
{
  if (m_lsaSockets.empty ())
    return;

  Address ackSocketAddress;
  auto socket = m_lsaSockets[ifIndex];
  socket->GetSockName (ackSocketAddress);
  m_txTrace (ackPacket);

  socket->SendTo (ackPacket, 0, InetSocketAddress (m_lsaAddress)); // TODO: Change to unicast later
  NS_LOG_INFO ("LS Ack sent via interface " << ifIndex << " : "
                                            << m_ospfInterfaces[ifIndex]->GetAddress ());
}

void
OspfApp::SendToNeighbor (uint32_t ifIndex, Ptr<Packet> packet, Ptr<OspfNeighbor> neighbor)
{
  auto socket = m_sockets[ifIndex];
  m_txTrace (packet);

  socket->SendTo (packet, 0, InetSocketAddress (neighbor->GetIpAddress ()));
  // NS_LOG_INFO ("Packet sent to via interface " << ifIndex << " : " << m_ospfInterfaces[ifIndex]->GetAddress());
}

void
OspfApp::SendToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                 Ptr<OspfNeighbor> neighbor)
{
  // NS_LOG_FUNCTION (this);
  // No sockets to send
  if (m_sockets.empty ())
    {
      neighbor->ClearKeyedTimeouts ();
      return;
    }
  SendToNeighbor (ifIndex, packet, neighbor);
  auto event = Simulator::Schedule (interval, &OspfApp::SendToNeighborInterval, this, interval,
                                    ifIndex, packet, neighbor);
  neighbor->BindTimeout (event);
}

void
OspfApp::SendToNeighborKeyedInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                      Ptr<OspfNeighbor> neighbor, LsaHeader::LsaKey lsaKey)
{
  // NS_LOG_FUNCTION (this);
  // No sockets to send
  if (m_sockets.empty ())
    return;
  SendToNeighbor (ifIndex, packet, neighbor);
  // Retransmit only when the neighbor >= TwoWay (may end up being Full after propagation delay)
  if (neighbor->GetState () >= OspfNeighbor::TwoWay)
    {
      auto event = Simulator::Schedule (interval, &OspfApp::SendToNeighborKeyedInterval, this,
                                        interval, ifIndex, packet, neighbor, lsaKey);
      neighbor->BindKeyedTimeout (lsaKey, event);
    }
  else
    {
      neighbor->RemoveKeyedTimeout (lsaKey);
    }
}

void
OspfApp::FloodLsu (uint32_t inputIfIndex, Ptr<LsUpdate> lsu)
{
  if (m_lsaSockets.empty ())
    {
      NS_LOG_INFO ("No sockets to flood LSU");
      return;
    }
  NS_ASSERT_MSG (lsu->GetNLsa () == 1,
                 "Only LSU with one LSA is allowed to flood (simplification)");
  NS_LOG_FUNCTION (this << inputIfIndex << lsu->GetNLsa ());

  auto lsaKey = lsu->GetLsaList ()[0].first.GetKey ();

  for (uint32_t i = 1; i < m_lsaSockets.size (); i++)
    {
      // Skip the incoming interface
      if (inputIfIndex == i)
        continue;
      auto interface = m_ospfInterfaces[i];

      // Send to neighbors with multicast address (only 1 neighbor for point-to-point)
      auto neighbors = m_ospfInterfaces[i]->GetNeighbors ();
      for (auto neighbor : neighbors)
        {
          // Flood L1 LSAs to neighbors within the same area
          if (lsu->GetLsaList ()[0].first.GetType () == LsaHeader::RouterLSAs &&
              neighbor->GetArea () != this->m_areaId)
            {
              continue;
            }
          Ptr<Packet> packet = lsu->ConstructPacket ();
          EncapsulateOspfPacket (packet, m_routerId, interface->GetArea (),
                                 OspfHeader::OspfType::OspfLSUpdate);
          SendToNeighborKeyedInterval (m_rxmtInterval + Seconds (m_randomVariable->GetValue ()), i,
                                       packet, neighbor, lsaKey);
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

  packet->PeekHeader (ipHeader);
  // NS_LOG_DEBUG("Packet Size: " << packet->GetSize() << ", Header Size: " << sizeof (ipHeader) << ", Payload Size " << ipHeader.GetPayloadSize());
  packet->RemoveHeader (ipHeader);

  packet->RemoveHeader (ospfHeader);

  Ipv4Address remoteRouterId;
  // NS_LOG_INFO("Packet Type: " << ospfHeader.OspfTypeToString(ospfHeader.GetType()));
  if (ospfHeader.GetType () == OspfHeader::OspfType::OspfHello)
    {
      Ptr<OspfHello> hello = Create<OspfHello> (packet);
      HandleHello (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, hello);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfDBD)
    {
      Ptr<OspfDbd> dbd = Create<OspfDbd> (packet);
      HandleDbd (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, dbd);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfLSRequest)
    {
      Ptr<LsRequest> lsr = Create<LsRequest> (packet);
      HandleLsr (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, lsr);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfLSUpdate)
    {
      Ptr<LsUpdate> lsu = Create<LsUpdate> (packet);
      HandleLsu (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, lsu);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfLSAck)
    {
      Ptr<LsAck> lsAck = Create<LsAck> (packet);
      HandleLsAck (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, lsAck);
    }
  else
    {
      NS_LOG_ERROR ("Unknown packet type");
    }
  // if (packet->PeekHeader())
}

void
OspfApp::HandleHello (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                      Ptr<OspfHello> hello)
{

  // Get relevant interface
  Ptr<OspfInterface> ospfInterface = m_ospfInterfaces[ifIndex];

  // Check if the paremeters match
  if (hello->GetHelloInterval () != ospfInterface->GetHelloInterval ())
    {
      NS_LOG_ERROR ("Hello interval does not match "
                    << hello->GetHelloInterval () << " != " << ospfInterface->GetHelloInterval ());
      return;
    }
  if (hello->GetRouterDeadInterval () != ospfInterface->GetRouterDeadInterval ())
    {
      NS_LOG_ERROR ("Router Interval does not match " << hello->GetRouterDeadInterval () << " != "
                                                      << ospfInterface->GetRouterDeadInterval ());
      return;
    }

  Ipv4Address remoteRouterId = Ipv4Address (ospfHeader.GetRouterId ());
  Ipv4Address remoteIp = ipHeader.GetSource ();
  NS_LOG_FUNCTION (this << ifIndex << remoteRouterId << remoteIp);

  Ptr<OspfNeighbor> neighbor;

  // Add a new neighbor if interface hasn't registered the neighbor
  if (!ospfInterface->IsNeighbor (remoteRouterId, remoteIp))
    {
      neighbor = ospfInterface->AddNeighbor (remoteRouterId, remoteIp, ospfHeader.GetArea (),
                                             OspfNeighbor::Init);
      NS_LOG_INFO ("New neighbor from area " << ospfHeader.GetArea () << " detected from interface "
                                             << ifIndex);
    }
  else
    {
      neighbor = ospfInterface->GetNeighbor (remoteRouterId, remoteIp);
      // Check if received Hello has different area ID
      if (neighbor->GetArea () != ospfHeader.GetArea ())
        {
          NS_LOG_WARN ("Received Hello and the stored neighbor have different area IDs, replacing "
                       "with the Hello");
          neighbor->SetArea (ospfHeader.GetArea ());
        }
    }

  if (neighbor->GetState () == OspfNeighbor::Down)
    {
      NS_LOG_INFO ("Re-added timed out interface " << ifIndex);
      neighbor->SetState (OspfNeighbor::Init);
    }

  // Refresh last received hello time to Now()
  neighbor->RefreshLastHelloReceived ();
  RefreshHelloTimeout (ifIndex, ospfInterface, remoteRouterId, remoteIp);

  // Advance to two-way/exstart
  if (neighbor->GetState () == OspfNeighbor::Init)
    {
      // Advance to ExStart if it's bidirectional (TODO: two-way for broadcast networks)
      NS_LOG_INFO ("Interface " << ifIndex << " is stuck at init");
      if (hello->IsNeighbor (m_routerId.Get ()))
        {
          NS_LOG_INFO ("Interface " << ifIndex << " is now bi-directional");
          neighbor->SetState (OspfNeighbor::ExStart);
          // Send DBD to negotiate master/slave and DD seq num, starting with self as a Master
          neighbor->SetDDSeqNum (m_randomVariableSeq->GetInteger ());
          NegotiateDbd (ifIndex, neighbor, true);
        }
    }
}

void
OspfApp::HandleDbd (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<OspfDbd> dbd)
{
  auto ospfInterface = m_ospfInterfaces[ifIndex];
  Ptr<OspfNeighbor> neighbor =
      ospfInterface->GetNeighbor (Ipv4Address (ospfHeader.GetRouterId ()), ipHeader.GetSource ());
  if (neighbor == nullptr)
    {
      NS_LOG_WARN ("Received DBD when neighbor (" << Ipv4Address (ospfHeader.GetRouterId ()) << ", "
                                                  << ipHeader.GetSource ()
                                                  << ") has not been formed");
      return;
    }
  if (neighbor->GetState () < OspfNeighbor::NeighborState::ExStart)
    {
      NS_LOG_INFO ("Received DBD when two-way adjacency hasn't formed yet");
      return;
    }
  if (m_routerId.Get () == neighbor->GetRouterId ().Get ())
    {
      NS_LOG_ERROR ("Received DBD has the same router ID; drop the packet");
      return;
    }
  // Negotiation (ExStart)
  if (dbd->GetBitI ())
    {
      if (neighbor->GetState () > OspfNeighbor::ExStart)
        {
          NS_LOG_INFO ("DBD Dropped. Negotiation has already done");
          return;
        }
      // Receive Negotiate DBD
      HandleNegotiateDbd (ifIndex, neighbor, dbd);
      return;
    }
  if (neighbor->GetState () < OspfNeighbor::Exchange)
    {
      NS_LOG_INFO ("Neighbor must be at least Exchange to start processing DBD");
      return;
    }
  if (dbd->GetBitI ())
    {
      NS_LOG_ERROR ("Bit I must be set to 1 only when both M and MS set to 1");
      return;
    }
  // bitI = 0 : DBD includes LSA headers
  if (dbd->GetBitMS ())
    {
      // Self is slave. Neighbor is Master
      if (m_routerId.Get () > neighbor->GetRouterId ().Get ())
        {
          // TODO: Reset adjacency
          NS_LOG_ERROR ("Both neighbors cannot be masters");
          return;
        }
      HandleMasterDbd (ifIndex, neighbor, dbd);
      // CompareAndSendLSRequests(dbd->GetLsaHeaders());
    }
  else
    {
      // Self is Master. Neighbor is Slave
      if (m_routerId.Get () < neighbor->GetRouterId ().Get ())
        {
          // TODO: Reset adjacency
          NS_LOG_ERROR ("Both neighbors cannot be slaves");
          return;
        }
      HandleSlaveDbd (ifIndex, neighbor, dbd);
    }
}

void
OspfApp::HandleNegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  // Neighbor is Master
  if (m_routerId.Get () < neighbor->GetRouterId ().Get ())
    {
      NS_LOG_INFO ("Set to slave (" << m_routerId << " < " << neighbor->GetRouterId ()
                                    << ") with DD Seq Num: " << dbd->GetDDSeqNum ());
      // Match DD Seq Num with Master
      neighbor->SetDDSeqNum (dbd->GetDDSeqNum ());
      // Snapshot LSDB headers during Exchange for consistency
      for (const auto &pair : m_routerLsdb)
        {
          // L1 LSAs must not cross the area
          if (neighbor->GetArea () == this->m_areaId)
            {
              neighbor->AddDbdQueue (pair.second.first);
            }
        }
      NegotiateDbd (ifIndex, neighbor, false);
      neighbor->SetState (OspfNeighbor::Exchange);
    }
  else if (m_routerId.Get () > neighbor->GetRouterId ().Get () && !dbd->GetBitMS ())
    {
      NS_LOG_INFO ("Set to master (" << m_routerId << " > " << neighbor->GetRouterId ()
                                     << ") with DD Seq Num: " << neighbor->GetDDSeqNum ());
      // Snapshot LSDB headers during Exchange for consistency
      for (const auto &pair : m_routerLsdb)
        {
          if (neighbor->GetArea () == this->m_areaId)
            {
              neighbor->AddDbdQueue (pair.second.first);
            }
        }
      neighbor->SetState (OspfNeighbor::Exchange);
      PollMasterDbd (ifIndex, neighbor);
    }
  return;
}

void
OspfApp::HandleMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  Ptr<OspfInterface> interface = m_ospfInterfaces[ifIndex];
  if (dbd->GetDDSeqNum () < neighbor->GetDDSeqNum () ||
      dbd->GetDDSeqNum () > neighbor->GetDDSeqNum () + 1)
    {
      // Drop the packet if out of order
      NS_LOG_ERROR ("DD sequence number is out-of-order " << neighbor->GetDDSeqNum () << " <> "
                                                          << dbd->GetDDSeqNum ());
      return;
    }
  Ptr<OspfDbd> dbdResponse;
  if (dbd->GetDDSeqNum () == neighbor->GetDDSeqNum () + 1)
    {
      NS_LOG_INFO ("Received duplicated DBD from Master");
      // Already received this DD seq Num; send the last DBD
      dbdResponse = neighbor->GetLastDbdSent ();
    }
  else
    {
      NS_LOG_INFO ("Received new DBD from Master");
      // Process neighbor DBD
      auto masterLsaHeaders = dbd->GetLsaHeaders ();
      for (auto header : masterLsaHeaders)
        {
          neighbor->InsertLsaKey (header);
        }

      // Generate its own next DBD, echoing DD seq num of the master
      auto slaveLsaHeaders = neighbor->PopMaxMtuFromDbdQueue (interface->GetMtu ());
      dbdResponse = Create<OspfDbd> (interface->GetMtu (), 0, 0, 0, 1, 0, dbd->GetDDSeqNum ());
      if (neighbor->IsDbdQueueEmpty ())
        {
          // No (M)ore packets (the last DBD)
          dbdResponse->SetBitM (0);
        }
      for (auto header : slaveLsaHeaders)
        {
          dbdResponse->AddLsaHeader (header);
        }
    }
  Ptr<Packet> packet = dbdResponse->ConstructPacket ();
  EncapsulateOspfPacket (packet, m_routerId, interface->GetArea (), OspfHeader::OspfType::OspfDBD);
  SendToNeighbor (ifIndex, packet, neighbor);

  // Increase its own DD to expect for the next one
  neighbor->IncrementDDSeqNum ();

  if (!dbd->GetBitM () && neighbor->IsDbdQueueEmpty ())
    {
      AdvanceToLoading (ifIndex, neighbor);
    }
}

void
OspfApp::HandleSlaveDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  Ptr<OspfInterface> interface = m_ospfInterfaces[ifIndex];
  if (dbd->GetDDSeqNum () != neighbor->GetDDSeqNum ())
    {
      // Out-of-order
      NS_LOG_ERROR ("DD sequence number is out-of-order");
      return;
    }
  // Process neighbor DBD
  NS_LOG_INFO ("Received DBD response [" << dbd->GetNLsaHeaders () << "] from slave");
  auto lsaHeaders = dbd->GetLsaHeaders ();
  for (auto header : lsaHeaders)
    {
      neighbor->InsertLsaKey (header);
    }
  // No more LSAs
  if (!dbd->GetBitM () && neighbor->IsDbdQueueEmpty ())
    {
      AdvanceToLoading (ifIndex, neighbor);
      return;
    }
  // Increment neighbor's seq num and poll more LSAs
  neighbor->IncrementDDSeqNum ();
  PollMasterDbd (ifIndex, neighbor);
}

void
OspfApp::HandleLsr (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    Ptr<LsRequest> lsr)
{
  auto interface = m_ospfInterfaces[ifIndex];
  Ptr<OspfNeighbor> neighbor =
      interface->GetNeighbor (Ipv4Address (ospfHeader.GetRouterId ()), ipHeader.GetSource ());

  if (neighbor->GetState () < OspfNeighbor::Loading)
    {
      NS_LOG_WARN ("Received LSR when the state is not at least Loading");
    }
  // Construct LS Update as implicit ACK based on received lsr
  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  for (auto &[remoteRouterId, routerLsa] : m_routerLsdb)
    {
      if (lsr->HasLsaKey (routerLsa.first.GetKey ()))
        {
          lsUpdate->AddLsa (routerLsa.first, routerLsa.second);
        }
    }
  if (lsUpdate->GetNLsa () != lsr->GetNLsaKeys ())
    {
      NS_LOG_WARN ("LSDB does not contain some LSAs in LS Request");
    }
  NS_LOG_INFO ("Received LSR (" << lsr->GetNLsaKeys () << ") from interface: " << ifIndex);
  Ptr<Packet> packet = lsUpdate->ConstructPacket ();
  EncapsulateOspfPacket (packet, m_routerId, interface->GetArea (),
                         OspfHeader::OspfType::OspfLSUpdate);
  // Implicit Ack, only send once
  SendToNeighbor (ifIndex, packet, neighbor);
}

void
OspfApp::HandleLsu (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsUpdate> lsu)
{
  auto receivedLsa = lsu->GetLsaList ();
  for (auto &[lsaHeader, lsa] : receivedLsa)
    {
      // Handle LSA and send ACK when appropriate
      HandleLsa (ifIndex, ipHeader, ospfHeader, lsaHeader, lsa);
    }
}

void
OspfApp::HandleLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    LsaHeader lsaHeader, Ptr<Lsa> lsa)
{
  auto interface = m_ospfInterfaces[ifIndex];
  Ptr<OspfNeighbor> neighbor =
      interface->GetNeighbor (Ipv4Address (ospfHeader.GetRouterId ()), ipHeader.GetSource ());

  uint32_t advertisingRouter = lsaHeader.GetAdvertisingRouter ();
  uint16_t seqNum = lsaHeader.GetSeqNum ();
  LsaHeader::LsaKey lsaKey = lsaHeader.GetKey ();

  auto ackPacket = ConstructLSAckPacket (m_routerId, m_areaId, lsaHeader);

  // If the LSA was originally generated by the receiving router, the packet is dropped.
  // Ack is sent
  if (advertisingRouter == m_routerId.Get ())
    {
      NS_LOG_INFO ("LSU is dropped, received LSU has originated here");
      SendAck (ifIndex, ackPacket, neighbor->GetRouterId ());
      return;
    }

  // Initialize local seqNum if it does not exist
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // If the sequence number equals or less than that of the last packet received from the
  // originating router, the packet is dropped and ACK is sent.
  if (seqNum <= m_seqNumbers[lsaKey])
    {
      // std::cout << "recv seqNum: "<< seqNum << ", stored seqNum: " << m_seqNumbers[originRouterId] << std::endl;
      NS_LOG_INFO ("LSU is dropped: received sequence number <= stored sequence number");
      // Send direct ACK once receiving duplicated sequence number
      // NS_LOG_DEBUG ("Sending ACK [" << ackPacket->GetSize () << "] from " << m_routerId
      //                               << " to interface " << ifIndex);
      SendAck (ifIndex, ackPacket, neighbor->GetRouterId ());
      return;
    }

  // Update seq num
  m_seqNumbers[lsaKey] = seqNum;

  // Process LSA
  ProcessLsa (ifIndex, ipHeader, ospfHeader, lsaHeader, lsa);

  // Satisfy LSR
  bool isLsrSatisfied = false;
  if (neighbor->GetState () == OspfNeighbor::Loading)
    {
      auto lastLsr = neighbor->GetLastLsrSent ();
      if (lastLsr->HasLsaKey (lsaHeader.GetKey ()))
        {
          isLsrSatisfied = true;
          // If LSU is an implicit ACK to LSR
          lastLsr->RemoveLsaKey (lsaHeader.GetKey ());
          if (lastLsr->IsLsaKeyEmpty ())
            {
              SendNextLsr (ifIndex, neighbor);
            }
        }
    }
  if (!isLsrSatisfied)
    {
      SendAck (ifIndex, ackPacket, neighbor->GetRouterId ());
    }

  // Flood the network
  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  lsUpdate->AddLsa (lsaHeader, lsa);
  FloodLsu (ifIndex, lsUpdate);
}

void
OspfApp::ProcessLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                     LsaHeader lsaHeader, Ptr<Lsa> lsa)
{
  switch (lsaHeader.GetType ())
    {
    case LsaHeader::RouterLSAs:
      ProcessRouterLsa (ifIndex, ipHeader, ospfHeader, lsaHeader, DynamicCast<RouterLsa> (lsa));
      break;
    case LsaHeader::AreaLSAs:
      ProcessAreaLsa (ifIndex, ipHeader, ospfHeader, lsaHeader, DynamicCast<AreaLsa> (lsa));
      break;
    case LsaHeader::SummaryLSAsArea:
      ProcessAreaSummaryLsa (ifIndex, ipHeader, ospfHeader, lsaHeader,
                             DynamicCast<SummaryLsa> (lsa));
      break;
    default:
      NS_LOG_WARN ("Received unsupport LSA type in received LS Update");
      break;
    }
}

void
OspfApp::HandleLsAck (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                      Ptr<LsAck> lsAck)
{
  auto interface = m_ospfInterfaces[ifIndex];
  Ptr<OspfNeighbor> neighbor =
      interface->GetNeighbor (Ipv4Address (ospfHeader.GetRouterId ()), ipHeader.GetSource ());
  auto lsaHeaders = lsAck->GetLsaHeaders ();
  NS_LOG_FUNCTION (this << ifIndex << lsaHeaders.size ());

  for (auto lsaHeader : lsaHeaders)
    {
      // Remove timeout if the stored seq num have been satisfied
      if (lsaHeader.GetSeqNum () <= m_seqNumbers[lsaHeader.GetKey ()])
        {
          bool isRemoved = neighbor->RemoveKeyedTimeout (lsaHeader.GetKey ());
          if (isRemoved)
            {
              NS_LOG_INFO ("Removed key (advertising router): "
                           << Ipv4Address (lsaHeader.GetAdvertisingRouter ())
                           << " from the retx timer");
            }
          else
            {
              NS_LOG_INFO ("Key: " << lsaHeader.GetAdvertisingRouter ()
                                   << " does not exist in the retx timer");
            }
        }
    }
}

void
OspfApp::ProcessRouterLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                           LsaHeader lsaHeader, Ptr<RouterLsa> routerLsa)
{
  uint32_t advertisingRouter = lsaHeader.GetAdvertisingRouter ();

  // Filling in Router LSDB
  NS_LOG_FUNCTION (this);
  m_routerLsdb[advertisingRouter] = std::make_pair (lsaHeader, routerLsa);

  if (m_enableAreaProxy)
    {
      // Update local Area LSDB entry if there's a change in area links
      // This will flood L2 LSA if self is the area leader
      if (m_isAreaLeader)
        {
          RecomputeAreaLsa ();
        }

      // Reset area leadership begin timer if it's a leader (lowest router ID)
      if (m_areaLeaderBeginTimer.IsRunning ())
        {
          m_areaLeaderBeginTimer.Remove ();
        }
      if (m_routerLsdb.begin ()->first == m_routerId.Get ())
        {
          if (!m_isAreaLeader)
            {
              m_areaLeaderBeginTimer = Simulator::Schedule (
                  m_routerDeadInterval + Seconds (m_randomVariable->GetValue ()),
                  &OspfApp::AreaLeaderBegin, this);
            }
        }
      else
        {
          AreaLeaderEnd ();
        }
    }

  // Update routing table
  UpdateL1ShortestPath ();

  //
}

void
OspfApp::ProcessAreaLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                         LsaHeader lsaHeader, Ptr<AreaLsa> areaLsa)
{
  if (!m_enableAreaProxy)
    {
      return;
    }
  NS_LOG_FUNCTION (this);
  // LS ID ofType 5 LSA is the originating LSA
  uint32_t areaId = lsaHeader.GetLsId ();

  // If this is the first area-LSA for this area
  if (m_areaLsdb.find (areaId) == m_areaLsdb.end ())
    {
      m_areaLsdb[areaId] = std::make_pair (lsaHeader, areaLsa);
      return;
    }
  // If the received areaLSA is newer and taking precedence
  if (lsaHeader.GetSeqNum () > m_areaLsdb[areaId].first.GetSeqNum ())
    {
      // Higher seq num
      m_areaLsdb[areaId] = std::make_pair (lsaHeader, areaLsa);
    }
  else if (lsaHeader.GetSeqNum () == m_areaLsdb[areaId].first.GetSeqNum () &&
           lsaHeader.GetAdvertisingRouter () < m_areaLsdb[areaId].first.GetAdvertisingRouter ())
    {
      // Tiebreaker. Lower router ID becomes the leader
      m_areaLsdb[areaId] = std::make_pair (lsaHeader, areaLsa);
    }
}

void
OspfApp::ProcessAreaSummaryLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                                LsaHeader lsaHeader, Ptr<SummaryLsa> summaryLsa)
{
  NS_LOG_FUNCTION (this);
  if (!m_enableAreaProxy)
    {
      return;
    }
  // Fill in the prefixes
  // LS ID of Summary LSA is the originating LSA
  uint32_t areaId = lsaHeader.GetLsId ();

  // If the area leader receives LSAs with higher seq num from other nodes
  if (areaId == m_areaId && m_isAreaLeader &&
      lsaHeader.GetSeqNum () >= m_seqNumbers[lsaHeader.GetKey ()])
    {
      // Recompute and flood with the new seq num
      RecomputeSummaryLsa ();
      Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
      lsUpdate->AddLsa (m_summaryLsdb[m_areaId]);
      FloodLsu (0, lsUpdate);
      return;
    }
  m_summaryLsdb[areaId] = std::make_pair (lsaHeader, summaryLsa);
}

// LSA
// Generate Local Router LSA with all areas
Ptr<RouterLsa>
OspfApp::GetRouterLsa ()
{
  std::vector<RouterLink> allLinks;
  for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
    {
      std::vector<RouterLink> links = m_ospfInterfaces[i]->GetActiveRouterLinks ();
      for (auto l : links)
        {
          allLinks.emplace_back (l);
        }
    }
  NS_LOG_INFO ("Router-LSA Created with " << allLinks.size () << " active links");
  return ConstructRouterLsa (allLinks);
}

// Generate Local Area LSA for the leader
Ptr<AreaLsa>
OspfApp::GetAreaLsa ()
{
  // <area neighbor ID, leader's IP address, interface metric>
  std::vector<AreaLink> allAreaLinks;
  // Read Router LSDB and extract cross-area links
  for (auto &[remoteRouterId, routerLsa] : m_routerLsdb)
    {
      auto crossAreaLinks = routerLsa.second->GetCrossAreaLinks ();
      for (auto l : crossAreaLinks)
        {
          allAreaLinks.emplace_back (l);
        }
    }
  NS_LOG_INFO ("Area-LSA Created with " << allAreaLinks.size () << " active links");
  return ConstructAreaLsa (allAreaLinks);
}

void
OspfApp::RecomputeRouterLsa ()
{
  NS_LOG_FUNCTION (this);

  auto lsaKey =
      std::make_tuple (LsaHeader::LsType::RouterLSAs, m_routerId.Get (), m_routerId.Get ());
  // Initialize seq number to zero if new
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // Increment a seq number
  m_seqNumbers[lsaKey]++;

  // Construct its router LSA
  Ptr<RouterLsa> routerLsa = GetRouterLsa ();

  // Assign routerLsa to its router LSDB
  LsaHeader lsaHeader;
  lsaHeader.SetType (LsaHeader::LsType::RouterLSAs);
  lsaHeader.SetLength (20 + routerLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  lsaHeader.SetLsId (m_routerId.Get ());
  lsaHeader.SetAdvertisingRouter (m_routerId.Get ());
  m_routerLsdb[m_routerId.Get ()] = std::make_pair (lsaHeader, routerLsa);

  // Update routing according to the updated LSDB
  UpdateL1ShortestPath ();
}

// Recompute Area LSA
void
OspfApp::RecomputeAreaLsa ()
{
  NS_LOG_FUNCTION (this);

  // Construct its area LSA
  Ptr<AreaLsa> areaLsa = GetAreaLsa ();

  // Do not update Area LSDB if it still reflects the current cross-border links in Router LSDB
  if (m_areaLsdb.find (m_areaId) != m_areaLsdb.end () &&
      areaLsa->GetLinks () == m_areaLsdb[m_areaId].second->GetLinks ())
    {
      return;
    }

  auto lsaKey = std::make_tuple (LsaHeader::LsType::AreaLSAs, m_areaId, m_routerId.Get ());

  // Initialize seq number to zero if new
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // Increment a seq number
  m_seqNumbers[lsaKey]++;

  // Assign areaLsa to its area LSDB
  LsaHeader lsaHeader;
  lsaHeader.SetType (LsaHeader::LsType::AreaLSAs);
  lsaHeader.SetLength (20 + areaLsa->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  lsaHeader.SetLsId (m_areaId);
  lsaHeader.SetAdvertisingRouter (m_routerId.Get ());
  m_areaLsdb[m_areaId] = std::make_pair (lsaHeader, areaLsa);

  // Flood LSA if it's the area leader
  if (m_isAreaLeader)
    {
      // Area-LSAs
      Ptr<LsUpdate> lsUpdateArea = Create<LsUpdate> ();
      lsUpdateArea->AddLsa (m_areaLsdb[m_areaId]);
      FloodLsu (0, lsUpdateArea);

      // Area Summary LSA
      RecomputeSummaryLsa ();
      Ptr<LsUpdate> lsUpdateSummary = Create<LsUpdate> ();
      lsUpdateSummary->AddLsa (m_summaryLsdb[m_areaId]);
      FloodLsu (0, lsUpdateSummary);
    }
}

void
OspfApp::RecomputeSummaryLsa ()
{
  NS_LOG_FUNCTION (this);

  // Construct its area LSA
  Ptr<SummaryLsa> summary = ConstructSummaryLsa (m_l2Prefixes[m_areaId]);

  auto lsaKey = std::make_tuple (LsaHeader::LsType::SummaryLSAsArea, m_areaId, m_routerId.Get ());

  // Initialize seq number to zero if new
  if (m_seqNumbers.find (lsaKey) == m_seqNumbers.end ())
    {
      m_seqNumbers[lsaKey] = 0;
    }

  // Increment a seq number
  m_seqNumbers[lsaKey]++;

  // Assign areaLsa to its area LSDB
  LsaHeader lsaHeader;
  lsaHeader.SetType (LsaHeader::LsType::SummaryLSAsArea);
  lsaHeader.SetLength (20 + summary->GetSerializedSize ());
  lsaHeader.SetSeqNum (m_seqNumbers[lsaKey]);
  lsaHeader.SetLsId (m_areaId);
  lsaHeader.SetAdvertisingRouter (m_routerId.Get ());
  m_summaryLsdb[m_areaId] = std::make_pair (lsaHeader, summary);
}

void
OspfApp::UpdateRouting ()
{
  // Remove old route
  // std::cout << "Number of Route: " << m_boundDevices.GetN() << std::endl;
  while (m_routing->GetNRoutes () > m_boundDevices.GetN ())
    {
      m_routing->RemoveRoute (m_boundDevices.GetN ());
    }
  for (auto &[remoteRouterId, nextHop] : m_l1NextHop)
    {
      for (auto addr : m_l1Addresses[remoteRouterId])
        {
          m_routing->AddHostRouteTo (Ipv4Address (addr), nextHop.first, nextHop.second);
        }
    }
  for (auto &[remoteRouterId, nextHop] : m_l2NextHop)
    {
      for (auto addr : m_l1Addresses[remoteRouterId])
        {
          m_routing->AddHostRouteTo (Ipv4Address (addr), nextHop.first, nextHop.second);
        }
    }
}
void
OspfApp::UpdateL1ShortestPath ()
{
  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  // <distance, next hop>
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;
  NS_LOG_FUNCTION (this);

  // Clear existing next-hop data
  m_l1NextHop.clear ();
  m_l1Addresses.clear ();
  if (m_l2Prefixes.find (m_areaId) != m_l2Prefixes.end ())
    {
      m_l2Prefixes[m_areaId].clear ();
    }

  // Dijkstra
  while (!pq.empty ())
    {
      pq.pop ();
    }
  distanceTo.clear ();
  uint32_t u, v, w;
  pq.emplace (0, m_routerId.Get ());
  distanceTo[m_routerId.Get ()] = 0;
  while (!pq.empty ())
    {
      std::tie (w, u) = pq.top ();
      pq.pop ();
      // Skip if the lsdb doesn't have any neighbors for that router
      if (m_routerLsdb.find (u) == m_routerLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_routerLsdb[u].second->GetNLink (); i++)
        {
          v = m_routerLsdb[u].second->GetLink (i).m_linkId;
          auto metric = m_routerLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }
  // std::cout << "node: " << GetNode()->GetId() << std::endl;
  // Shortest path information for each subnet -- <mask, nextHop, interface, metric>
  std::map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> routingEntries;
  for (auto &[remoteRouterId, routerLsa] : m_routerLsdb)
    {
      // std::cout << "  destination: " << Ipv4Address(remoteRouterId) << std::endl;

      // No reachable path
      if (prevHop.find (remoteRouterId) == prevHop.end ())
        {
          // std::cout << "    no route" << std::endl;
          continue;
        }

      // Find the first hop
      v = remoteRouterId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_routerId.Get ())
            {
              break;
            }
          v = prevHop[v];
        }

      // Find the next hop's IP and interface index
      uint32_t ifIndex = 0;
      for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
        {
          auto neighbors = m_ospfInterfaces[i]->GetNeighbors ();
          for (auto n : neighbors)
            {
              if (n->GetRouterId ().Get () == v)
                {
                  ifIndex = i;
                  break;
                }
            }
          if (ifIndex)
            break;
        }
      NS_ASSERT (ifIndex > 0);

      // Fill in the next hop and prefixes data
      for (uint32_t i = 0; i < routerLsa.second->GetNLink (); i++)
        {
          // NS_LOG_DEBUG ("Add route: " << Ipv4Address (routerLsa.second->GetLink (i).m_linkData)
          // << ", " << ifIndex << ", " << distanceTo[remoteRouterId]);
          m_l1NextHop[remoteRouterId] = std::make_pair (ifIndex, distanceTo[remoteRouterId]);
          m_l1Addresses[remoteRouterId].emplace_back (routerLsa.second->GetLink (i).m_linkData);
          // Cost of proxied prefixes are always zero
          m_l2Prefixes[m_areaId].emplace_back (
              SummaryPrefix (routerLsa.second->GetLink (i).m_linkData, 0));
          // m_routing->AddHostRouteTo (Ipv4Address (routerLsa.second->GetLink (i).m_linkData),
          //                            ifIndex, distanceTo[remoteRouterId]);
        }
    }
  UpdateRouting ();
}

void
OspfApp::UpdateL2ShortestPath ()
{
  std::unordered_map<uint32_t, uint32_t> distanceTo;
  std::unordered_map<uint32_t, uint32_t> prevHop;
  // <distance, next hop>
  std::priority_queue<std::pair<uint32_t, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>,
                      std::greater<std::pair<uint32_t, uint32_t>>>
      pq;
  NS_LOG_FUNCTION (this);

  // Clear existing next-hop data
  m_l2NextHop.clear ();

  // Dijkstra
  while (!pq.empty ())
    {
      pq.pop ();
    }
  distanceTo.clear ();
  uint32_t u, v, w;
  pq.emplace (0, m_areaId);
  distanceTo[m_areaId] = 0;
  while (!pq.empty ())
    {
      std::tie (w, u) = pq.top ();
      pq.pop ();
      // Skip if the lsdb doesn't have any neighbors for that router
      if (m_areaLsdb.find (u) == m_areaLsdb.end ())
        continue;

      for (uint32_t i = 0; i < m_areaLsdb[u].second->GetNLink (); i++)
        {
          v = m_areaLsdb[u].second->GetLink (i).m_areaId;
          auto metric = m_areaLsdb[u].second->GetLink (i).m_metric;
          if (distanceTo.find (v) == distanceTo.end () || w + metric < distanceTo[v])
            {
              distanceTo[v] = w + metric;
              prevHop[v] = u;
              pq.emplace (w + metric, v);
            }
        }
    }
  // std::cout << "node: " << GetNode()->GetId() << std::endl;
  // Find the shortest paths and the next hops
  for (auto &[remoteAreaId, areaLsa] : m_areaLsdb)
    {
      // No reachable path
      if (prevHop.find (remoteAreaId) == prevHop.end ())
        {
          // std::cout << "    no route" << std::endl;
          continue;
        }

      // Find the first hop
      v = remoteAreaId;
      while (prevHop.find (v) != prevHop.end ())
        {
          if (prevHop[v] == m_areaId)
            {
              break;
            }
          v = prevHop[v];
        }

      // Find the next hop's IP and interface index
      uint32_t ifIndex = 0;
      for (uint32_t i = 1; i < m_ospfInterfaces.size (); i++)
        {
          auto neighbors = m_ospfInterfaces[i]->GetNeighbors ();
          for (auto n : neighbors)
            {
              if (n->GetRouterId ().Get () == v)
                {
                  ifIndex = i;
                  break;
                }
            }
          if (ifIndex)
            break;
        }
      NS_ASSERT (ifIndex > 0);

      // Fill in the next hop and prefixes data
      for (uint32_t i = 0; i < areaLsa.second->GetNLink (); i++)
        {
          // NS_LOG_DEBUG ("Add route: " << Ipv4Address (routerLsa.second->GetLink (i).m_linkData)
          // << ", " << ifIndex << ", " << distanceTo[areaId]);
          m_l2NextHop[remoteAreaId] =
              std::make_pair (areaLsa.second->GetLink (i).m_ipAddress, distanceTo[remoteAreaId]);
        }
    }
  UpdateRouting ();
}

// Hello Protocol
void
OspfApp::HelloTimeout (uint32_t ifIndex, Ptr<OspfInterface> ospfInterface,
                       Ipv4Address remoteRouterId, Ipv4Address remoteIp)
{
  auto neighbor = ospfInterface->GetNeighbor (remoteRouterId, remoteIp);
  NS_ASSERT (neighbor != nullptr);
  // Set the interface to down, which keeps hello going
  AdvanceToDown (ifIndex, neighbor);
  NS_LOG_DEBUG ("Interface " << ospfInterface->GetAddress () << " has removed routerId: "
                             << remoteRouterId << ", remoteIp" << remoteIp << " neighbors");
}

void
OspfApp::RefreshHelloTimeout (uint32_t ifIndex, Ptr<OspfInterface> ospfInterface,
                              Ipv4Address remoteRouterId, Ipv4Address remoteIp)
{
  // Refresh the timer
  if (m_helloTimeouts[ifIndex].IsRunning ())
    {
      m_helloTimeouts[ifIndex].Remove ();
    }
  m_helloTimeouts[ifIndex] = Simulator::Schedule (
      Seconds (ospfInterface->GetRouterDeadInterval ()) + Seconds (m_randomVariable->GetValue ()),
      &OspfApp::HelloTimeout, this, ifIndex, ospfInterface, remoteRouterId, remoteIp);
}

// Down
void
OspfApp::AdvanceToDown (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_INFO ("Hello timeout. Move to Down");
  neighbor->SetState (OspfNeighbor::Down);
  RecomputeRouterLsa ();
  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  lsUpdate->AddLsa (m_routerLsdb[m_routerId.Get ()]);
  neighbor->RemoveTimeout ();
  neighbor->ClearKeyedTimeouts ();
  FloodLsu (0, lsUpdate);
}

// ExStart
void
OspfApp::NegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, bool bitMS)
{
  auto interface = m_ospfInterfaces[ifIndex];
  uint32_t ddSeqNum = neighbor->GetDDSeqNum ();
  NS_LOG_INFO ("DD Sequence Num (" << ddSeqNum << ") is generated to negotiate neighbor "
                                   << neighbor->GetNeighborString () << " via interface "
                                   << ifIndex);
  Ptr<OspfDbd> ospfDbd = Create<OspfDbd> (interface->GetMtu (), 0, 0, 1, 1, bitMS, ddSeqNum);
  Ptr<Packet> packet = ospfDbd->ConstructPacket ();
  EncapsulateOspfPacket (packet, m_routerId, interface->GetArea (), OspfHeader::OspfType::OspfDBD);

  if (bitMS)
    {
      // Master keep sending DBD until stopped
      NS_LOG_INFO ("Router started advertising as master");
      SendToNeighborInterval (m_rxmtInterval + Seconds (m_randomVariable->GetValue ()), ifIndex,
                              packet, neighbor);
    }
  else
    {
      // Remove timeout
      neighbor->RemoveTimeout ();
      // Implicit ACK, replying with being slave
      NS_LOG_INFO ("Router responds as slave");
      SendToNeighbor (ifIndex, packet, neighbor);
    }
}

// Exchange
void
OspfApp::PollMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  auto interface = m_ospfInterfaces[ifIndex];
  uint32_t ddSeqNum = neighbor->GetDDSeqNum ();

  Ptr<OspfDbd> ospfDbd = Create<OspfDbd> (interface->GetMtu (), 0, 0, 0, 1, 1, ddSeqNum);
  std::vector<LsaHeader> lsaHeaders = neighbor->PopMaxMtuFromDbdQueue (interface->GetMtu ());
  if (neighbor->IsDbdQueueEmpty ())
    {
      // No (M)ore packets (the last DBD)
      ospfDbd->SetBitM (0);
    }
  for (auto header : lsaHeaders)
    {
      ospfDbd->AddLsaHeader (header);
    }
  Ptr<Packet> packet = ospfDbd->ConstructPacket ();
  EncapsulateOspfPacket (packet, m_routerId, interface->GetArea (), OspfHeader::OspfType::OspfDBD);

  // Keep sending DBD until receiving corresponding DBD from slave
  NS_LOG_INFO ("Master start polling for DBD with LSAs");
  SendToNeighborInterval (m_rxmtInterval + Seconds (m_randomVariable->GetValue ()), ifIndex, packet,
                          neighbor);
}

// Loading
void
OspfApp::AdvanceToLoading (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_INFO ("Database exchange is done. Advance to Loading");
  auto interface = m_ospfInterfaces[ifIndex];
  neighbor->SetState (OspfNeighbor::Loading);
  neighbor->RemoveTimeout ();
  CompareAndSendLsr (ifIndex, neighbor);
}
void
OspfApp::CompareAndSendLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  auto interface = m_ospfInterfaces[ifIndex];
  std::vector<LsaHeader> localLsaHeaders;
  for (auto &[remoteRouterId, routerLsa] : m_routerLsdb)
    {
      localLsaHeaders.emplace_back (routerLsa.first);
    }
  NS_LOG_INFO ("Number of local LSAs: " << localLsaHeaders.size ());
  neighbor->AddOutdatedLsaKeysToQueue (localLsaHeaders);
  NS_LOG_INFO ("Number of outdated LSA: " << neighbor->GetLsrQueueSize () << " / "
                                          << neighbor->GetLsrQueueSize ());
  SendNextLsr (ifIndex, neighbor);
}
void
OspfApp::SendNextLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  if (neighbor->IsLsrQueueEmpty ())
    {
      NS_LOG_INFO ("Number of outdated LSA: " << neighbor->GetLsrQueueSize ());
      AdvanceToFull (ifIndex, neighbor);
      return;
    }
  auto interface = m_ospfInterfaces[ifIndex];
  std::vector<LsaHeader::LsaKey> lsaKeys = neighbor->PopMaxMtuFromLsrQueue (interface->GetMtu ());
  Ptr<LsRequest> lsRequest = Create<LsRequest> (lsaKeys);
  Ptr<Packet> packet = lsRequest->ConstructPacket ();
  EncapsulateOspfPacket (packet, m_routerId, interface->GetArea (),
                         OspfHeader::OspfType::OspfLSRequest);
  neighbor->SetLastLsrSent (lsRequest);
  SendToNeighborInterval (m_rxmtInterval + Seconds (m_randomVariable->GetValue ()), ifIndex, packet,
                          neighbor);
}

// Full
void
OspfApp::AdvanceToFull (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_INFO ("LSR Queue is empty. Loading is done. Advance to FULL");
  auto interface = m_ospfInterfaces[ifIndex];
  neighbor->SetState (OspfNeighbor::Full);
  // Remove data sync timeout
  neighbor->RemoveTimeout ();

  RecomputeRouterLsa ();

  // Create its LSU packet containing its own links
  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  lsUpdate->AddLsa (m_routerLsdb[m_routerId.Get ()]);

  // Flood its Router-LSA to all neighbors

  FloodLsu (0, lsUpdate);
}

// Area
void
OspfApp::AreaLeaderBegin ()
{
  NS_LOG_FUNCTION (this);
  std::cout << "Area Leader Begin " << m_areaId << ", " << m_routerId << std::endl;
  m_isAreaLeader = true;
  // TODO: Area Leader Logic -- start flooding Area-LSA and Summary-LSA-Area
  // Flood LSU with Area-LSAs to all interfaces
  RecomputeAreaLsa ();
}

void
OspfApp::AreaLeaderEnd ()
{
  NS_LOG_FUNCTION (this);
  // TODO: Area Leader Logic -- start flooding Area-LSA and Summary-LSA-Area
}

} // Namespace ns3
