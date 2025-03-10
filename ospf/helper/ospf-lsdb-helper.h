#ifndef OSPF_LSDB_HELPER_H
#define OSPF_LSDB_HELPER_H

#include "ns3/node-container.h"
#include "ns3/ospf-app.h"

namespace ns3 {

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

#endif /* OSPF_LSDB_HELPER_H */
