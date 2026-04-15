/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_HELPER_TEST_UTILS_H
#define OSPF_APP_HELPER_TEST_UTILS_H

#include "ns3/ipv4-address.h"
#include "ns3/l1-summary-lsa.h"
#include "ns3/l2-summary-lsa.h"
#include "ns3/lsa-header.h"
#include "ns3/ospf-app.h"
#include "ns3/router-lsa.h"

#include <set>
#include <tuple>

namespace ns3::ospf_app_helper_test_utils {

inline bool
HasSummaryRoute (const std::set<SummaryRoute> &routes, Ipv4Address address, Ipv4Mask mask,
                 uint32_t metric)
{
  return routes.find (SummaryRoute (address.Get (), mask.Get (), metric)) != routes.end ();
}

inline Ptr<L1SummaryLsa>
FetchSelfL1Summary (const Ptr<OspfApp> &app)
{
  const auto key =
      std::make_tuple (LsaHeader::LsType::L1SummaryLSAs, app->GetRouterId ().Get (),
                       app->GetRouterId ().Get ());
  auto pair = app->FetchLsa (key);
  if (pair.second == nullptr)
    {
      return nullptr;
    }
  return DynamicCast<L1SummaryLsa> (pair.second);
}

inline Ptr<L2SummaryLsa>
FetchAreaL2Summary (const Ptr<OspfApp> &app, uint32_t advertisingRouter)
{
  const auto key =
      std::make_tuple (LsaHeader::LsType::L2SummaryLSAs, app->GetArea (), advertisingRouter);
  auto pair = app->FetchLsa (key);
  if (pair.second == nullptr)
    {
      return nullptr;
    }
  return DynamicCast<L2SummaryLsa> (pair.second);
}

inline Ptr<RouterLsa>
FetchSelfRouterLsa (const Ptr<OspfApp> &app)
{
  const auto key =
      std::make_tuple (LsaHeader::LsType::RouterLSAs, app->GetRouterId ().Get (),
                       app->GetRouterId ().Get ());
  auto pair = app->FetchLsa (key);
  if (pair.second == nullptr)
    {
      return nullptr;
    }
  return DynamicCast<RouterLsa> (pair.second);
}

} // namespace ns3::ospf_app_helper_test_utils

#endif // OSPF_APP_HELPER_TEST_UTILS_H