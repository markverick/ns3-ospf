/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-io-component.h"

namespace ns3 {
void
OspfApp::SendHello ()
{
  if (m_helloSockets.empty ())
    return;

  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_helloEvent.IsExpired ());

  m_io->SendHello ();
}

void
OspfApp::SendAck (uint32_t ifIndex, Ptr<Packet> ackPacket, Ipv4Address remoteIp)
{
  if (m_sockets.empty ())
    return;

  m_io->SendAck (ifIndex, ackPacket, remoteIp);
}

void
OspfApp::SendToNeighbor (uint32_t ifIndex, Ptr<Packet> packet, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_FUNCTION (this << m_routerId << ifIndex << neighbor->GetIpAddress ()
                        << neighbor->GetIpAddress () << neighbor->GetState ());

  m_io->SendToNeighbor (ifIndex, packet, neighbor);
}

void
OspfApp::SendToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                 Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_FUNCTION (this << ifIndex << neighbor->GetIpAddress ());

  m_io->SendToNeighborInterval (interval, ifIndex, packet, neighbor);
}

void
OspfApp::SendToNeighborKeyedInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                      Ptr<OspfNeighbor> neighbor, LsaHeader::LsaKey lsaKey)
{
  NS_LOG_FUNCTION (this << ifIndex << neighbor->GetIpAddress () << std::get<0> (lsaKey)
                        << std::get<1> (lsaKey) << std::get<2> (lsaKey));

  m_io->SendToNeighborKeyedInterval (interval, ifIndex, packet, neighbor, lsaKey);
}

void
OspfApp::FloodLsu (uint32_t inputIfIndex, Ptr<LsUpdate> lsu)
{
  if (m_sockets.empty ())
    {
      NS_LOG_INFO ("No sockets to flood LSU");
      return;
    }
  NS_ASSERT_MSG (lsu->GetNLsa () == 1,
                 "Only LSU with one LSA is allowed to flood (simplification)");
  NS_LOG_FUNCTION (this << inputIfIndex << lsu->GetNLsa ());

  m_io->FloodLsu (inputIfIndex, lsu);
}

void
OspfApp::HandleRead (Ptr<Socket> socket)
{

  m_io->HandleRead (socket);
}

} // namespace ns3
