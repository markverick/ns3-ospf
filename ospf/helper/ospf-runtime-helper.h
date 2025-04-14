#ifndef OSPF_RUNTIME_HELPER_H
#define OSPF_RUNTIME_HELPER_H

#include "ns3/node-container.h"
#include "ns3/ospf-app.h"
#include "ns3/network-module.h"
#include "ns3/pointer.h"

namespace ns3 {

void SetLinkDown (Ptr<NetDevice> nd);

void SetLinkError (Ptr<NetDevice> nd);

void SetLinkUp (Ptr<NetDevice> nd);

void VerifyNeighbor (NodeContainer allNodes, NodeContainer nodes);

void CompareLsdb (NodeContainer nodes);

void CompareL1PrefixLsdb (NodeContainer nodes);

void CompareAreaLsdb (NodeContainer nodes);

void CompareSummaryLsdb (NodeContainer nodes);

} // namespace ns3

#endif /* OSPF_RUNTIME_HELPER_H */
