/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_IO_COMPONENT_H
#define OSPF_APP_IO_COMPONENT_H

#include "ns3/address.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/lsa-header.h"

#include <cstdint>

namespace ns3 {

class Packet;
class Socket;
class OspfNeighbor;
class LsUpdate;
class OspfApp;

class OspfAppIo
{
public:
  explicit OspfAppIo (OspfApp &app);

  void SendHello ();
  void SendAck (uint32_t ifIndex, Ptr<Packet> ackPacket, Ipv4Address remoteIp);
  void SendToNeighbor (uint32_t ifIndex, Ptr<Packet> packet, Ptr<OspfNeighbor> neighbor);
  void SendToNeighborInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                               Ptr<OspfNeighbor> neighbor);
  void SendToNeighborKeyedInterval (Time interval, uint32_t ifIndex, Ptr<Packet> packet,
                                    Ptr<OspfNeighbor> neighbor, LsaHeader::LsaKey lsaKey);
  void FloodLsu (uint32_t inputIfIndex, Ptr<LsUpdate> lsu);
  void HandleRead (Ptr<Socket> socket);

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_IO_COMPONENT_H
