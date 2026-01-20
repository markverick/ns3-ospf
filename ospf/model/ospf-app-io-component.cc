/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-io-component.h"

#include "ospf-app-private.h"
#include "ospf-app-logging.h"
#include "packets/ospf-header.h"
#include "lsa/lsa-header.h"

namespace ns3 {

namespace {
/**
 * Classify LSA type as L1 or L2
 */
std::string
ClassifyLsaLevel (uint8_t lsaType)
{
  switch (lsaType)
    {
    case LsaHeader::RouterLSAs:
    case LsaHeader::NetworkLSAs:
    case LsaHeader::L1SummaryLSAs:
      return "L1";
    case LsaHeader::SummaryLSAsIP:
    case LsaHeader::SummaryLSAsASBR:
    case LsaHeader::ASExternalLSAs:
    case LsaHeader::AreaLSAs:
    case LsaHeader::L2SummaryLSAs:
      return "L2";
    default:
      return "";
    }
}

/**
 * Extract LSA level from packet payload for LSU (type 4) or LSAck (type 5)
 */
std::string
ExtractLsaLevelFromPacket (Ptr<Packet> packet, uint8_t ospfType)
{
  if (ospfType != OspfHeader::OspfLSUpdate && ospfType != OspfHeader::OspfLSAck)
    {
      return "";
    }

  // Make a copy to avoid modifying original
  Ptr<Packet> copy = packet->Copy ();
  
  // Remove OSPF header (24 bytes) to get to payload
  OspfHeader ospfHeader;
  if (copy->RemoveHeader (ospfHeader) == 0)
    {
      return "";
    }

  uint32_t payloadSize = copy->GetSize ();
  if (payloadSize < 8)
    {
      return "";
    }

  // Read first few bytes of payload
  uint8_t buffer[12];
  uint32_t bytesToRead = std::min (payloadSize, 12u);
  copy->CopyData (buffer, bytesToRead);

  if (ospfType == OspfHeader::OspfLSUpdate)
    {
      // LSU format: num_lsas (4 bytes), then LSA headers
      // LSA header: LS age(2), options(1), LS type(1), ...
      if (bytesToRead >= 8)
        {
          uint8_t lsaType = buffer[7]; // offset 4 + 3
          return ClassifyLsaLevel (lsaType);
        }
    }
  else if (ospfType == OspfHeader::OspfLSAck)
    {
      // LSAck format: LSA header(s) directly
      // LSA header: LS age(2), options(1), LS type(1), ...
      if (bytesToRead >= 4)
        {
          uint8_t lsaType = buffer[3];
          return ClassifyLsaLevel (lsaType);
        }
    }

  return "";
}
} // anonymous namespace

OspfAppIo::OspfAppIo (OspfApp &app)
  : m_app (app)
{
}

void
OspfAppIo::SendHello ()
{
  if (!m_app.IsEnabled ())
    {
      return;
    }
  Address helloSocketAddress;
  for (uint32_t i = 1; i < m_app.m_helloSockets.size (); i++)
    {
      auto socket = m_app.m_helloSockets[i];
      if (socket == nullptr)
        {
          continue;
        }
      if (i >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[i] == nullptr)
        {
          continue;
        }
      socket->GetSockName (helloSocketAddress);
      Ptr<Packet> p = ConstructHelloPacket (
          Ipv4Address::ConvertFrom (m_app.m_routerId), m_app.m_ospfInterfaces[i]->GetArea (),
          m_app.m_ospfInterfaces[i]->GetMask (), m_app.m_ospfInterfaces[i]->GetHelloInterval (),
          m_app.m_ospfInterfaces[i]->GetRouterDeadInterval (),
          m_app.m_ospfInterfaces[i]->GetNeighbors ());
      m_app.m_txTrace (p);

      // Log Hello packet (type 1, no LSA level)
      if (m_app.m_enablePacketLog)
        {
          m_app.m_logging->LogPacketTx (p->GetSize (), OspfHeader::OspfHello, "");
        }

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
  if (ifIndex >= m_app.m_sockets.size () || m_app.m_sockets[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("SendAck dropped (no socket) ifIndex=" << ifIndex);
      return;
    }
  auto socket = m_app.m_sockets[ifIndex];
  socket->GetSockName (ackSocketAddress);
  m_app.m_txTrace (ackPacket);

  // Log LSAck packet (type 5, extract LSA level)
  if (m_app.m_enablePacketLog)
    {
      std::string lsaLevel = ExtractLsaLevelFromPacket (ackPacket, OspfHeader::OspfLSAck);
      m_app.m_logging->LogPacketTx (ackPacket->GetSize (), OspfHeader::OspfLSAck, lsaLevel);
    }

  socket->SendTo (ackPacket, 0, InetSocketAddress (remoteIp));

  NS_LOG_INFO ("LS Ack sent via interface " << ifIndex << " : " << remoteIp);
}

void
OspfAppIo::SendToNeighbor (uint32_t ifIndex, Ptr<Packet> packet, Ptr<OspfNeighbor> neighbor)
{
  if (ifIndex >= m_app.m_sockets.size () || m_app.m_sockets[ifIndex] == nullptr)
    {
      return;
    }
  auto socket = m_app.m_sockets[ifIndex];
  m_app.m_txTrace (packet);

  // Log packet for overhead measurement (replaces PCAP)
  if (m_app.m_enablePacketLog)
    {
      OspfHeader ospfHeader;
      packet->PeekHeader (ospfHeader);
      uint8_t ospfType = static_cast<uint8_t> (ospfHeader.GetType ());
      std::string lsaLevel = ExtractLsaLevelFromPacket (packet, ospfType);
      m_app.m_logging->LogPacketTx (packet->GetSize (), ospfType, lsaLevel);
    }

  socket->SendTo (packet->Copy (), 0, InetSocketAddress (neighbor->GetIpAddress ()));
}

void
OspfAppIo::SendToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                   Ptr<OspfNeighbor> neighbor)
{
  // No sockets to send
  if (m_app.m_sockets.empty () || ifIndex >= m_app.m_sockets.size () || m_app.m_sockets[ifIndex] == nullptr)
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
  if (m_app.m_sockets.empty () || ifIndex >= m_app.m_sockets.size () || m_app.m_sockets[ifIndex] == nullptr)
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
  if (lsu == nullptr)
    {
      NS_LOG_WARN ("FloodLsu: null LSU");
      return;
    }

  const uint32_t nLsa = lsu->GetNLsa ();
  auto lsaList = lsu->GetLsaList ();
  if (nLsa != 1 || lsaList.size () != 1)
    {
      NS_LOG_WARN ("FloodLsu: dropping LSU with nLsa=" << nLsa
                                                       << " (expected exactly 1)");
      return;
    }

  auto lsaKey = lsaList[0].first.GetKey ();

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
              (lsaList[0].first.GetType () == LsaHeader::RouterLSAs ||
               lsaList[0].first.GetType () == LsaHeader::L1SummaryLSAs))
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
  if (packet->RemoveHeader (ipHeader) == 0)
    {
      NS_LOG_WARN ("Dropping packet: missing IPv4 header");
      return;
    }

  if (packet->RemoveHeader (ospfHeader) == 0)
    {
      NS_LOG_WARN ("Dropping packet: missing/invalid OSPF header");
      return;
    }

  if (ospfHeader.GetPayloadSize () > packet->GetSize ())
    {
      NS_LOG_WARN ("Dropping packet: OSPF declared payload exceeds available bytes");
      return;
    }
  if (ospfHeader.GetPayloadSize () < packet->GetSize ())
    {
      packet->RemoveAtEnd (packet->GetSize () - ospfHeader.GetPayloadSize ());
    }

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
      NS_LOG_WARN ("Dropping packet: unknown OSPF packet type");
      return;
    }
}

} // namespace ns3
