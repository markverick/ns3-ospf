/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_LSA_PROCESSOR_H
#define OSPF_APP_LSA_PROCESSOR_H

#include "ns3/ipv4-header.h"
#include "ns3/lsa-header.h"
#include "ns3/ospf-header.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <utility>

namespace ns3 {

class Lsa;
class LsAck;
class LsRequest;
class LsUpdate;
class OspfApp;

class OspfLsaProcessor
{
public:
  explicit OspfLsaProcessor (OspfApp &app);

  void HandleLsr (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsRequest> lsr);
  void HandleLsu (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsUpdate> lsu);
  void HandleLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, LsaHeader lsaHeader,
                  Ptr<Lsa> lsa);
  void ProcessLsa (LsaHeader lsaHeader, Ptr<Lsa> lsa);
  void ProcessLsa (std::pair<LsaHeader, Ptr<Lsa>> lsa);
  void HandleLsAck (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsAck> lsAck);

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_LSA_PROCESSOR_H
