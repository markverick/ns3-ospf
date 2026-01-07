/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_NEIGHBOR_FSM_H
#define OSPF_APP_NEIGHBOR_FSM_H

#include "ns3/ipv4-header.h"
#include "ns3/nstime.h"
#include "ns3/ospf-header.h"
#include "ns3/ptr.h"

#include <cstdint>

namespace ns3 {

class OspfApp;
class OspfNeighbor;
class OspfHello;
class OspfDbd;

class OspfNeighborFsm
{
public:
  explicit OspfNeighborFsm (OspfApp &app);

  void HandleHello (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    Ptr<OspfHello> hello);
  void HandleDbd (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<OspfDbd> dbd);
  void HandleNegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);
  void HandleMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);
  void HandleSlaveDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd);

  void HelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void RefreshHelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  void FallbackToInit (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void FallbackToDown (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  void NegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, bool bitMS);
  void PollMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  void AdvanceToLoading (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void CompareAndSendLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);
  void SendNextLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

  void AdvanceToFull (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor);

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_NEIGHBOR_FSM_H
