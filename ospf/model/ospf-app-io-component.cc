/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-io-component.h"

#include "ospf-app-private.h"

namespace ns3 {

OspfAppIo::OspfAppIo (OspfApp &app)
  : m_app (app)
{
}

void
OspfAppIo::SendHello ()
{
  Address helloSocketAddress;
  for (uint32_t i = 1; i < m_app.m_helloSockets.size (); i++)
    {
      auto socket = m_app.m_helloSockets[i];
      socket->GetSockName (helloSocketAddress);
      Ptr<Packet> p = ConstructHelloPacket (
          Ipv4Address::ConvertFrom (m_app.m_routerId), m_app.m_ospfInterfaces[i]->GetArea (),
          m_app.m_ospfInterfaces[i]->GetMask (), m_app.m_ospfInterfaces[i]->GetHelloInterval (),
          m_app.m_ospfInterfaces[i]->GetRouterDeadInterval (),
          m_app.m_ospfInterfaces[i]->GetNeighbors ());
      m_app.m_txTrace (p);
      if (Ipv4Address::IsMatchingType (m_app.m_helloAddress))
        {
          m_app.m_txTraceWithAddresses (
              p, helloSocketAddress,
              InetSocketAddress (Ipv4Address::ConvertFrom (m_app.m_helloAddress)));
        }
      socket->Send (p, 0);
      if (Ipv4Address::IsMatchingType (m_app.m_helloAddress))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S) << " client sent "
                                  << p->GetSize () << " bytes to " << m_app.m_helloAddress
                                  << " via interface " << i << " : "
                                  << m_app.m_ospfInterfaces[i]->GetAddress ());
        }
    }
  if (!m_app.m_helloSockets.empty ())
    {
      m_app.ScheduleTransmitHello (m_app.m_helloInterval);
    }
}

void
OspfAppIo::SendAck (uint32_t ifIndex, Ptr<Packet> ackPacket, Ipv4Address remoteIp)
{
  Address ackSocketAddress;
  auto socket = m_app.m_sockets[ifIndex];
  socket->GetSockName (ackSocketAddress);
  m_app.m_txTrace (ackPacket);

  socket->SendTo (ackPacket, 0, InetSocketAddress (remoteIp));

  NS_LOG_INFO ("LS Ack sent via interface " << ifIndex << " : " << remoteIp);
}

void
OspfAppIo::SendToNeighbor (uint32_t ifIndex, Ptr<Packet> packet, Ptr<OspfNeighbor> neighbor)
{
  auto socket = m_app.m_sockets[ifIndex];
  m_app.m_txTrace (packet);

  socket->SendTo (packet->Copy (), 0, InetSocketAddress (neighbor->GetIpAddress ()));
}

void
OspfAppIo::SendToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                   Ptr<OspfNeighbor> neighbor)
{
  // No sockets to send
  if (m_app.m_sockets.empty ())
    {
      neighbor->ClearKeyedTimeouts ();
      return;
    }
  SendToNeighbor (ifIndex, packet, neighbor);
  if (neighbor->GetState () >= OspfNeighbor::TwoWay)
    {
      auto event = Simulator::Schedule (interval, &OspfApp::SendToNeighborInterval, &m_app,
                                        interval, ifIndex, packet, neighbor);
      neighbor->BindTimeout (event);
      return;
    }
  else
    {
      neighbor->RemoveTimeout ();
    }
}

void
OspfAppIo::SendToNeighborKeyedInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                        Ptr<OspfNeighbor> neighbor, LsaHeader::LsaKey lsaKey)
{
  // No sockets to send
  if (m_app.m_sockets.empty ())
    return;
  SendToNeighbor (ifIndex, packet, neighbor);
  // Retransmit only when the neighbor >= TwoWay (may end up being Full after propagation delay)
  if (neighbor->GetState () >= OspfNeighbor::TwoWay)
    {
      auto event = Simulator::Schedule (interval, &OspfApp::SendToNeighborKeyedInterval, &m_app,
                                        interval, ifIndex, packet, neighbor, lsaKey);
      neighbor->BindKeyedTimeout (lsaKey, event);
    }
  else
    {
      neighbor->RemoveKeyedTimeout (lsaKey);
    }
}

void
OspfAppIo::FloodLsu (uint32_t inputIfIndex, Ptr<LsUpdate> lsu)
{
  NS_ASSERT_MSG (lsu->GetNLsa () == 1,
                 "Only LSU with one LSA is allowed to flood (simplification)");

  auto lsaKey = lsu->GetLsaList ()[0].first.GetKey ();

  for (uint32_t i = 1; i < m_app.m_sockets.size (); i++)
    {
      // Skip the incoming interface
      if (inputIfIndex == i)
        continue;
      auto interface = m_app.m_ospfInterfaces[i];

      // Send to neighbors with multicast address (only 1 neighbor for point-to-point)
      auto neighbors = m_app.m_ospfInterfaces[i]->GetNeighbors ();
      for (auto neighbor : neighbors)
        {
          if (neighbor->GetState () < OspfNeighbor::TwoWay)
            {
              continue;
            }
          // Flood L1 LSAs to neighbors within the same area
          if (neighbor->GetArea () != m_app.m_areaId &&
              (lsu->GetLsaList ()[0].first.GetType () == LsaHeader::RouterLSAs ||
               lsu->GetLsaList ()[0].first.GetType () == LsaHeader::L1SummaryLSAs))
            {
              continue;
            }
          Ptr<Packet> packet = lsu->ConstructPacket ();
          EncapsulateOspfPacket (packet, m_app.m_routerId, interface->GetArea (),
                                 OspfHeader::OspfType::OspfLSUpdate);
          m_app.SendToNeighborKeyedInterval (
              m_app.m_rxmtInterval + MilliSeconds (m_app.m_jitterRv->GetValue ()), i, packet,
              neighbor, lsaKey);
        }
    }
}

void
OspfAppIo::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  packet = socket->RecvFrom (from);

  Ipv4Header ipHeader;
  OspfHeader ospfHeader;

  packet->PeekHeader (ipHeader);
  packet->RemoveHeader (ipHeader);

  packet->RemoveHeader (ospfHeader);

  // Drop irrelevant packets in multi-access
  if (ipHeader.GetDestination () != m_app.m_lsaAddress &&
      ipHeader.GetDestination () != m_app.m_helloAddress)
    {
      if (ipHeader.GetDestination () !=
          m_app.m_ospfInterfaces[socket->GetBoundNetDevice ()->GetIfIndex ()]->GetAddress ())
        {
          return;
        }
    }

  if (ospfHeader.GetType () == OspfHeader::OspfType::OspfHello)
    {
      Ptr<OspfHello> hello = Create<OspfHello> (packet);
      m_app.HandleHello (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, hello);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfDBD)
    {
      Ptr<OspfDbd> dbd = Create<OspfDbd> (packet);
      m_app.HandleDbd (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, dbd);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfLSRequest)
    {
      Ptr<LsRequest> lsr = Create<LsRequest> (packet);
      m_app.HandleLsr (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, lsr);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfLSUpdate)
    {
      Ptr<LsUpdate> lsu = Create<LsUpdate> (packet);
      m_app.HandleLsu (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, lsu);
    }
  else if (ospfHeader.GetType () == OspfHeader::OspfType::OspfLSAck)
    {
      Ptr<LsAck> lsAck = Create<LsAck> (packet);
      m_app.HandleLsAck (socket->GetBoundNetDevice ()->GetIfIndex (), ipHeader, ospfHeader, lsAck);
    }
  else
    {
      NS_FATAL_ERROR ("Unknown packet type");
    }
}

} // namespace ns3
