/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-neighbor-fsm.h"

#include "ospf-app-private.h"

namespace ns3 {

OspfNeighborFsm::OspfNeighborFsm (OspfApp &app)
  : m_app (app)
{
}

void
OspfNeighborFsm::HandleHello (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                              Ptr<OspfHello> hello)
{

  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("Hello dropped due to invalid ifIndex: " << ifIndex);
      return;
    }

  // Get relevant interface
  Ptr<OspfInterface> ospfInterface = m_app.m_ospfInterfaces[ifIndex];

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
  NS_LOG_FUNCTION (&m_app << ifIndex << remoteRouterId << remoteIp);

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

  // At this point, the state must be at least Init.
  if (neighbor->GetState () == OspfNeighbor::Down)
    {
      NS_LOG_INFO ("Re-added timed out interface " << ifIndex);
      neighbor->SetState (OspfNeighbor::Init);
    }

  // Refresh last received hello time to Now()
  neighbor->RefreshLastHelloReceived ();

  // If the neighbor contains its router ID
  if (hello->IsNeighbor (m_app.m_routerId.Get ()))
    {
      // Two-way hello
      // Reset dead timeout
      RefreshHelloTimeout (ifIndex, neighbor);

      // Advance to two-way/exstart
      if (neighbor->GetState () == OspfNeighbor::Init)
        {
          // Advance to ExStart (skipped DR/BDR)
          NS_LOG_INFO ("Interface " << ifIndex << " is now bi-directional");
          neighbor->SetState (OspfNeighbor::ExStart);
          // Send DBD to negotiate master/slave and DD seq num, starting with self as a Master
          neighbor->SetDDSeqNum (m_app.m_randomVariableSeq->GetInteger ());
          NegotiateDbd (ifIndex, neighbor, true);
        }
    }
  else
    {
      // One-way hello
      if (neighbor->GetState () == OspfNeighbor::Init)
        {
          NS_LOG_INFO ("Interface " << ifIndex << " stays INIT");
        }
      else
        {
          NS_LOG_INFO ("Interface " << ifIndex << " falls back to INIT");
          FallbackToInit (ifIndex, neighbor);
        }
    }
}

void
OspfNeighborFsm::HandleDbd (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                            Ptr<OspfDbd> dbd)
{
  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("DBD dropped due to invalid ifIndex: " << ifIndex);
      return;
    }
  auto ospfInterface = m_app.m_ospfInterfaces[ifIndex];
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
  if (m_app.m_routerId.Get () == neighbor->GetRouterId ().Get ())
    {
      NS_LOG_ERROR ("Received DBD has the same router ID; drop the packet");
      return;
    }
  // Negotiation (ExStart)
  if (dbd->GetBitI ())
    {
      if (neighbor->GetState () > OspfNeighbor::ExStart)
        {
          NS_LOG_INFO ("DBD Dropped. Negotiation has already done " << neighbor->GetState ());
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
      if (m_app.m_routerId.Get () > neighbor->GetRouterId ().Get ())
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
      if (m_app.m_routerId.Get () < neighbor->GetRouterId ().Get ())
        {
          // TODO: Reset adjacency
          NS_LOG_ERROR ("Both neighbors cannot be slaves");
          return;
        }
      HandleSlaveDbd (ifIndex, neighbor, dbd);
    }
}

void
OspfNeighborFsm::HandleNegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  // Neighbor is Master
  if (m_app.m_routerId.Get () < neighbor->GetRouterId ().Get ())
    {
      NS_LOG_INFO ("Set to slave (" << m_app.m_routerId << " < " << neighbor->GetRouterId ()
                                    << ") with DD Seq Num: " << dbd->GetDDSeqNum ());
      // Match DD Seq Num with Master
      neighbor->SetDDSeqNum (dbd->GetDDSeqNum ());
      // Snapshot LSDB headers during Exchange for consistency
      for (const auto &pair : m_app.m_routerLsdb)
        {
          // L1 LSAs must not cross the area
          if (neighbor->GetArea () == m_app.m_areaId)
            {
              neighbor->AddDbdQueue (pair.second.first);
            }
        }
      for (const auto &pair : m_app.m_l1SummaryLsdb)
        {
          // L1 LSAs must not cross the area
          if (neighbor->GetArea () == m_app.m_areaId)
            {
              neighbor->AddDbdQueue (pair.second.first);
            }
        }
      for (const auto &pair : m_app.m_areaLsdb)
        {
          neighbor->AddDbdQueue (pair.second.first);
        }
      for (const auto &pair : m_app.m_l2SummaryLsdb)
        {
          neighbor->AddDbdQueue (pair.second.first);
        }
      NegotiateDbd (ifIndex, neighbor, false);
      neighbor->SetState (OspfNeighbor::Exchange);
    }
  else if (m_app.m_routerId.Get () > neighbor->GetRouterId ().Get () && !dbd->GetBitMS ())
    {
      NS_LOG_INFO ("Set to master (" << m_app.m_routerId << " > " << neighbor->GetRouterId ()
                                     << ") with DD Seq Num: " << neighbor->GetDDSeqNum ());
      // Snapshot LSDB headers during Exchange for consistency
      for (const auto &pair : m_app.m_routerLsdb)
        {
          if (neighbor->GetArea () == m_app.m_areaId)
            {
              neighbor->AddDbdQueue (pair.second.first);
            }
        }
      for (const auto &pair : m_app.m_l1SummaryLsdb)
        {
          if (neighbor->GetArea () == m_app.m_areaId)
            {
              neighbor->AddDbdQueue (pair.second.first);
            }
        }
      for (const auto &pair : m_app.m_areaLsdb)
        {
          neighbor->AddDbdQueue (pair.second.first);
        }
      for (const auto &pair : m_app.m_l2SummaryLsdb)
        {
          neighbor->AddDbdQueue (pair.second.first);
        }
      neighbor->SetState (OspfNeighbor::Exchange);
      PollMasterDbd (ifIndex, neighbor);
    }
  return;
}

void
OspfNeighborFsm::HandleMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("Master DBD dropped due to invalid ifIndex: " << ifIndex);
      return;
    }
  Ptr<OspfInterface> interface = m_app.m_ospfInterfaces[ifIndex];
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
  EncapsulateOspfPacket (packet, m_app.m_routerId, interface->GetArea (),
                         OspfHeader::OspfType::OspfDBD);
  m_app.SendToNeighbor (ifIndex, packet, neighbor);

  // Increase its own DD to expect for the next one
  neighbor->IncrementDDSeqNum ();

  if (!dbd->GetBitM () && neighbor->IsDbdQueueEmpty ())
    {
      AdvanceToLoading (ifIndex, neighbor);
    }
}

void
OspfNeighborFsm::HandleSlaveDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, Ptr<OspfDbd> dbd)
{
  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("Slave DBD dropped due to invalid ifIndex: " << ifIndex);
      return;
    }
  Ptr<OspfInterface> interface = m_app.m_ospfInterfaces[ifIndex];
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

// Hello Protocol
void
OspfNeighborFsm::HelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("Hello timeout ignored due to invalid ifIndex: " << ifIndex);
      return;
    }
  // Set the interface to down
  FallbackToDown (ifIndex, neighbor);

  NS_LOG_DEBUG ("Interface " << ifIndex << " has removed routerId: " << neighbor->GetRouterId ()
                             << ", remoteIp" << neighbor->GetIpAddress () << " neighbors");

  // Remove the neighbor for scalability (TODO: delay the removal)
  m_app.m_ospfInterfaces[ifIndex]->RemoveNeighbor (neighbor->GetRouterId (), neighbor->GetIpAddress ());
}

void
OspfNeighborFsm::RefreshHelloTimeout (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("Hello timeout refresh ignored due to invalid ifIndex: " << ifIndex);
      return;
    }
  uint32_t remoteIp = neighbor->GetIpAddress ().Get ();
  // Refresh the timer
  if (m_app.m_helloTimeouts[ifIndex].find (remoteIp) == m_app.m_helloTimeouts[ifIndex].end () ||
      m_app.m_helloTimeouts[ifIndex][remoteIp].IsRunning ())
    {
      m_app.m_helloTimeouts[ifIndex][remoteIp].Remove ();
    }
  m_app.m_helloTimeouts[ifIndex][remoteIp] =
      Simulator::Schedule (MilliSeconds (m_app.m_ospfInterfaces[ifIndex]->GetRouterDeadInterval ()) +
                   MilliSeconds (m_app.m_jitterRv->GetValue ()),
                           &OspfApp::HelloTimeout, &m_app, ifIndex, neighbor);
}

// Init
void
OspfNeighborFsm::FallbackToInit (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_INFO ("Move to Init");
  // TODO: Defer router lsa update until when the link is fully down
  neighbor->SetState (OspfNeighbor::Init);

  // Fill in the current Router LSDB
  m_app.RecomputeRouterLsa ();

  // Process the new LSA and generate/flood Area LSA if needed
  m_app.ProcessLsa (m_app.m_routerLsdb[m_app.m_routerId.Get ()]);

  // Clear timeouts
  neighbor->RemoveTimeout ();
  neighbor->ClearKeyedTimeouts ();
}

// Down
void
OspfNeighborFsm::FallbackToDown (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_INFO ("Hello timeout. Move to Down");
  neighbor->SetState (OspfNeighbor::Down);
  // Fill in the current Router LSDB
  m_app.RecomputeRouterLsa ();

  // Process the new LSA and generate/flood Area LSA if needed
  m_app.ProcessLsa (m_app.m_routerLsdb[m_app.m_routerId.Get ()]);

  // Clear timeouts
  neighbor->RemoveTimeout ();
  neighbor->ClearKeyedTimeouts ();
}

// ExStart
void
OspfNeighborFsm::NegotiateDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor, bool bitMS)
{
  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("Negotiate DBD aborted due to invalid ifIndex: " << ifIndex);
      return;
    }
  auto interface = m_app.m_ospfInterfaces[ifIndex];
  uint32_t ddSeqNum = neighbor->GetDDSeqNum ();
  NS_LOG_INFO ("DD Sequence Num (" << ddSeqNum << ") is generated to negotiate neighbor "
                                   << neighbor->GetNeighborString () << " via interface "
                                   << ifIndex);
  Ptr<OspfDbd> ospfDbd = Create<OspfDbd> (interface->GetMtu (), 0, 0, 1, 1, bitMS, ddSeqNum);
  Ptr<Packet> packet = ospfDbd->ConstructPacket ();
  EncapsulateOspfPacket (packet, m_app.m_routerId, interface->GetArea (),
                         OspfHeader::OspfType::OspfDBD);

  if (bitMS)
    {
      // Master keep sending DBD until stopped
      NS_LOG_INFO ("Router started advertising as master");
      m_app.SendToNeighborInterval (
          m_app.m_rxmtInterval + MilliSeconds (m_app.m_jitterRv->GetValue ()), ifIndex,
          packet, neighbor);
    }
  else
    {
      // Remove timeout
      neighbor->RemoveTimeout ();
      // Implicit ACK, replying with being slave
      NS_LOG_INFO ("Router responds as slave");
      m_app.SendToNeighbor (ifIndex, packet, neighbor);
    }
}

// Exchange
void
OspfNeighborFsm::PollMasterDbd (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("Poll master DBD aborted due to invalid ifIndex: " << ifIndex);
      return;
    }
  auto interface = m_app.m_ospfInterfaces[ifIndex];
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
  EncapsulateOspfPacket (packet, m_app.m_routerId, interface->GetArea (),
                         OspfHeader::OspfType::OspfDBD);

  // Keep sending DBD until receiving corresponding DBD from slave
  NS_LOG_INFO ("Master start polling for DBD with LSAs");
  m_app.SendToNeighborInterval (
      m_app.m_rxmtInterval + MilliSeconds (m_app.m_jitterRv->GetValue ()), ifIndex, packet,
      neighbor);
}

// Loading
void
OspfNeighborFsm::AdvanceToLoading (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_INFO ("Database exchange is done. Advance to Loading");
  neighbor->SetState (OspfNeighbor::Loading);
  neighbor->RemoveTimeout ();
  CompareAndSendLsr (ifIndex, neighbor);
}

void
OspfNeighborFsm::CompareAndSendLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  std::vector<LsaHeader> localLsaHeaders;
  for (auto &[remoteRouterId, lsa] : m_app.m_routerLsdb)
    {
      if (neighbor->GetArea () == m_app.m_areaId)
        {
          localLsaHeaders.emplace_back (lsa.first);
        }
    }
  for (auto &[remoteRouterId, lsa] : m_app.m_l1SummaryLsdb)
    {
      if (neighbor->GetArea () == m_app.m_areaId)
        {
          localLsaHeaders.emplace_back (lsa.first);
        }
    }
  for (auto &[remoteAreaId, lsa] : m_app.m_areaLsdb)
    {
      localLsaHeaders.emplace_back (lsa.first);
    }
  for (auto &[remoteAreaId, lsa] : m_app.m_l2SummaryLsdb)
    {
      localLsaHeaders.emplace_back (lsa.first);
    }
  NS_LOG_INFO ("Number of local LSAs: " << localLsaHeaders.size ());
  neighbor->AddOutdatedLsaKeysToQueue (localLsaHeaders);
  NS_LOG_INFO ("Number of outdated LSA: " << neighbor->GetLsrQueueSize ());
  SendNextLsr (ifIndex, neighbor);
}

void
OspfNeighborFsm::SendNextLsr (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  if (neighbor->IsLsrQueueEmpty ())
    {
      NS_LOG_INFO ("Number of outdated LSA: " << neighbor->GetLsrQueueSize ());
      AdvanceToFull (ifIndex, neighbor);
      return;
    }

  if (ifIndex >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[ifIndex] == nullptr)
    {
      NS_LOG_WARN ("LSR send aborted due to invalid ifIndex: " << ifIndex);
      return;
    }
  auto interface = m_app.m_ospfInterfaces[ifIndex];
  std::vector<LsaHeader::LsaKey> lsaKeys = neighbor->PopMaxMtuFromLsrQueue (interface->GetMtu ());
  Ptr<LsRequest> lsRequest = Create<LsRequest> (lsaKeys);
  Ptr<Packet> packet = lsRequest->ConstructPacket ();
  EncapsulateOspfPacket (packet, m_app.m_routerId, interface->GetArea (),
                         OspfHeader::OspfType::OspfLSRequest);
  neighbor->SetLastLsrSent (lsRequest);
  m_app.SendToNeighborInterval (
      m_app.m_rxmtInterval + MilliSeconds (m_app.m_jitterRv->GetValue ()), ifIndex, packet,
      neighbor);
}

// Full
void
OspfNeighborFsm::AdvanceToFull (uint32_t ifIndex, Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_INFO ("LSR Queue is empty. Loading is done. Advance to FULL");
  neighbor->SetState (OspfNeighbor::Full);
  // Remove data sync timeout
  neighbor->RemoveTimeout ();

  // Fill in the current Router LSDB
  m_app.RecomputeRouterLsa ();

  // Process the new LSA and generate/flood Area LSA if needed
  m_app.ProcessLsa (m_app.m_routerLsdb[m_app.m_routerId.Get ()]);
}

} // namespace ns3
