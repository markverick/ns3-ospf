/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-neighbor-fsm.h"

namespace ns3 {
void
OspfApp::HandleHello (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                      Ptr<OspfHello> hello)
{

  m_neighborFsm->HandleHello (ifIndex, ipHeader, ospfHeader, hello);
}

void
OspfApp::HandleDbd (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<OspfDbd> dbd)
{

  m_neighborFsm->HandleDbd (ifIndex, ipHeader, ospfHeader, dbd);
}

void
OspfApp::HandleNegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  m_neighborFsm->HandleNegotiateDbd (ifIndex, neighbor, dbd);
}

void
OspfApp::HandleMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  m_neighborFsm->HandleMasterDbd (ifIndex, neighbor, dbd);
}

void
OspfApp::HandleSlaveDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  m_neighborFsm->HandleSlaveDbd (ifIndex, neighbor, dbd);
}

// Hello Protocol
void
OspfApp::HelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->HelloTimeout (ifIndex, neighbor);
}

void
OspfApp::RefreshHelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->RefreshHelloTimeout (ifIndex, neighbor);
}

// Init
void
OspfApp::FallbackToInit (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->FallbackToInit (ifIndex, neighbor);
}

// Down
void
OspfApp::FallbackToDown (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->FallbackToDown (ifIndex, neighbor);
}

// ExStart
void
OspfApp::NegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, bool bitMS)
{
  m_neighborFsm->NegotiateDbd (ifIndex, neighbor, bitMS);
}

// Exchange
void
OspfApp::PollMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->PollMasterDbd (ifIndex, neighbor);
}

// Loading
void
OspfApp::AdvanceToLoading (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->AdvanceToLoading (ifIndex, neighbor);
}
void
OspfApp::CompareAndSendLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->CompareAndSendLsr (ifIndex, neighbor);
}
void
OspfApp::SendNextLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->SendNextLsr (ifIndex, neighbor);
}

// Full
void
OspfApp::AdvanceToFull (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  m_neighborFsm->AdvanceToFull (ifIndex, neighbor);
}

} // namespace ns3
