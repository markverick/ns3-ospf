/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_ROUTING_H
#define OSPF_ROUTING_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-address.h"

namespace ns3 {

class OspfApp;
class Ipv4Route;
class OutputStreamWrapper;

class OspfRouting : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId (void);

  OspfRouting ();
  ~OspfRouting () override;

  void SetApp (Ptr<OspfApp> app);

  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                              Socket::SocketErrno &sockerr) override;
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                   LocalDeliverCallback lcb, ErrorCallback ecb) override;
  void NotifyInterfaceUp (uint32_t interface) override;
  void NotifyInterfaceDown (uint32_t interface) override;
  void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address) override;
  void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address) override;
  void SetIpv4 (Ptr<Ipv4> ipv4) override;
  void PrintRoutingTable (Ptr<OutputStreamWrapper> stream,
                          Time::Unit unit = Time::S) const override;

protected:
  void DoDispose (void) override;

private:
  Ptr<Ipv4Route> Lookup (Ipv4Address destination, Ptr<NetDevice> oif) const;
  bool IsInterfaceUsable (uint32_t ifIndex) const;
  bool MatchesOutputInterface (uint32_t ifIndex, Ptr<NetDevice> oif) const;
  bool IsActive () const;

  Ptr<Ipv4> m_ipv4;
  Ptr<OspfApp> m_app;
};

} // namespace ns3

#endif /* OSPF_ROUTING_H */
