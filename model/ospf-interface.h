/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
MIT License

Copyright (c) 2025 Sirapop Theeranantachai

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#ifndef OSPF_INTERFACE_H
#define OSPF_INTERFACE_H

#include "ns3/log.h"
#include "ns3/address.h"
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
#include "ospf-neighbor.h"
#include "algorithm"

namespace ns3 {

class OspfInterface: public Object
{
public:
  OspfInterface();
  OspfInterface(Ipv4Address ipAddress, Ipv4Mask ipMask, uint16_t helloInterval,
                uint32_t routerDeadInterval, uint32_t area, uint32_t metric, uint32_t mtu);

  ~OspfInterface();

  Ipv4Address GetAddress();
  void SetAddress(Ipv4Address ipAddress);

  Ipv4Mask GetMask();
  void SetMask(Ipv4Mask ipMask);

  uint32_t GetMetric();
  void SetMetric(uint32_t metric);

  uint32_t GetArea();
  void SetArea(uint32_t area);

  uint32_t GetMtu();
  void SetMtu(uint32_t mtu);

  uint16_t GetHelloInterval();
  void SetHelloInterval(uint16_t helloInterval);

  uint32_t GetRouterDeadInterval();
  void SetRouterDeadInterval(uint32_t routerDeadInterval);

  Ptr<OspfNeighbor> GetNeighbor(Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  std::vector<Ptr<OspfNeighbor> > GetNeighbors();

  Ptr<OspfNeighbor> AddNeighbor(Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  Ptr<OspfNeighbor> AddNeighbor(Ipv4Address remoteRouterId, Ipv4Address remoteIp, uint32_t remoteAreaId, OspfNeighbor::NeighborState state);
  

  bool RemoveNeighbor(Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  bool IsNeighbor(Ipv4Address remoteRouterId, Ipv4Address remoteIp);

  //  Vector of <neighbor's routerIds, its own interface ipAddress>
  std::vector<std::pair<uint32_t, uint32_t> > GetNeighborLinks();
  std::vector<std::pair<uint32_t, uint32_t> > GetNeighborLinks(uint32_t areaId);
  std::vector<std::pair<uint32_t, uint32_t> > GetActiveNeighborLinks();
  std::vector<std::pair<uint32_t, uint32_t> > GetActiveNeighborLinks(uint32_t areaId);


  private:
    Ipv4Address m_ipAddress;
    Ipv4Mask m_ipMask;
    uint16_t m_helloInterval;
    uint32_t m_routerDeadInterval;
    uint32_t m_area;
    uint32_t m_metric;
    uint32_t m_mtu;
    std::vector<Ptr<OspfNeighbor> > m_neighbors;
};
}

#endif /* OSPF_INTERFACE_H */