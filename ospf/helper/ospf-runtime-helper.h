#ifndef OSPF_RUNTIME_HELPER_H
#define OSPF_RUNTIME_HELPER_H

#include "ns3/node-container.h"
#include "ns3/ospf-app.h"
#include "ns3/network-module.h"
#include "ns3/ospf-runtime-helper.h"

namespace ns3 {

void
SetLinkDown (Ptr<NetDevice> nd)
{
  Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
  pem->SetRate (1.0);
  nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

void
SetLinkError (Ptr<NetDevice> nd)
{
  Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
  pem->SetRate (0.005);
  nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

void
SetLinkUp (Ptr<NetDevice> nd)
{
  Ptr<RateErrorModel> pem = CreateObject<RateErrorModel> ();
  pem->SetRate (0.0);
  nd->SetAttribute ("ReceiveErrorModel", PointerValue (pem));
}

void
VerifyNeighbor (NodeContainer allNodes, NodeContainer nodes)
{
  NS_ASSERT (nodes.GetN () > 0);
  bool match = true;
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
      for (auto &pair : app->GetLsdb ())
        {
          if (pair.second.second->GetNLink () !=
              allNodes.Get (app->GetNode ()->GetId ())->GetNDevices () - 1)
            {
              std::cout << "[" << Simulator::Now () << "] LSDB entry [" << Ipv4Address (pair.first)
                        << "] of node [" << i << "] is incorrect ("
                        << pair.second.second->GetNLink ()
                        << " != " << allNodes.Get (app->GetNode ()->GetId ())->GetNDevices () - 1
                        << ")" << std::endl;
              match = false;
              for (uint32_t j = 0; j < pair.second.second->GetNLink (); j++)
                {
                  std::cout << "  " << Ipv4Address (pair.second.second->GetLink (j).m_linkId);
                }
              std::cout << std::endl;
            }
        }
    }
  if (match)
    std::cout << "[" << Simulator::Now () << "] LSDB entries correct" << std::endl;
  return;
}

void
CompareLsdb (NodeContainer nodes)
{
  NS_ASSERT (nodes.GetN () > 0);
  Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (0)->GetApplication (0));
  uint32_t hash = app->GetLsdbHash ();

  for (uint32_t i = 1; i < nodes.GetN (); i++)
    {
      app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
      if (hash != app->GetLsdbHash ())
        {
          std::cout << "[" << Simulator::Now () << "] LSDBs mismatched" << std::endl;
          return;
        }
    }
  std::cout << "[" << Simulator::Now () << "] LSDBs matched" << std::endl;

  return;
}

void
CompareAreaLsdb (NodeContainer nodes)
{
  NS_ASSERT (nodes.GetN () > 0);
  Ptr<OspfApp> app = DynamicCast<OspfApp> (nodes.Get (0)->GetApplication (0));
  uint32_t hash = app->GetAreaLsdbHash ();

  for (uint32_t i = 1; i < nodes.GetN (); i++)
    {
      app = DynamicCast<OspfApp> (nodes.Get (i)->GetApplication (0));
      if (hash != app->GetAreaLsdbHash ())
        {
          std::cout << "[" << Simulator::Now () << "] Area LSDBs mismatched" << std::endl;
          return;
        }
    }
  std::cout << "[" << Simulator::Now () << "] Area LSDBs matched" << std::endl;

  return;
}

} // namespace ns3

#endif /* OSPF_RUNTIME_HELPER_H */
