/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-lsa-processor.h"

namespace ns3 {

void
OspfApp::HandleLsr (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    Ptr<LsRequest> lsr)
{
  m_lsa->HandleLsr (ifIndex, ipHeader, ospfHeader, lsr);
}

void
OspfApp::HandleLsu (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader, Ptr<LsUpdate> lsu)
{
  m_lsa->HandleLsu (ifIndex, ipHeader, ospfHeader, lsu);
}

void
OspfApp::HandleLsa (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                    LsaHeader lsaHeader, Ptr<Lsa> lsa)
{
  NS_LOG_FUNCTION (this << ifIndex << Ipv4Address (ospfHeader.GetRouterId ())
                        << ipHeader.GetSource ());
  m_lsa->HandleLsa (ifIndex, ipHeader, ospfHeader, lsaHeader, lsa);
}

void
OspfApp::ProcessLsa (LsaHeader lsaHeader, Ptr<Lsa> lsa)
{
  NS_LOG_FUNCTION (this);
  m_lsa->ProcessLsa (lsaHeader, lsa);
}

void
OspfApp::ProcessLsa (std::pair<LsaHeader, Ptr<Lsa>> lsa)
{
  m_lsa->ProcessLsa (lsa);
}

void
OspfApp::HandleLsAck (uint32_t ifIndex, Ipv4Header ipHeader, OspfHeader ospfHeader,
                      Ptr<LsAck> lsAck)
{
  m_lsa->HandleLsAck (ifIndex, ipHeader, ospfHeader, lsAck);
}

std::pair<LsaHeader, Ptr<Lsa>>
OspfApp::FetchLsa (LsaHeader::LsaKey lsaKey)
{
  uint32_t lsId = std::get<1> (lsaKey);

  switch (std::get<0> (lsaKey))
    {
    case LsaHeader::RouterLSAs:
      {
        auto it = m_routerLsdb.find (lsId);
        if (it == m_routerLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: RouterLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    case LsaHeader::L1SummaryLSAs:
      {
        auto it = m_l1SummaryLsdb.find (lsId);
        if (it == m_l1SummaryLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: L1SummaryLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    case LsaHeader::AreaLSAs:
      {
        auto it = m_areaLsdb.find (lsId);
        if (it == m_areaLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: AreaLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    case LsaHeader::L2SummaryLSAs:
      {
        auto it = m_l2SummaryLsdb.find (lsId);
        if (it == m_l2SummaryLsdb.end ())
          {
            NS_LOG_WARN ("FetchLsa: L2SummaryLSA not found for lsId=" << Ipv4Address (lsId));
            return {LsaHeader (), nullptr};
          }
        return {it->second.first, it->second.second};
      }
    default:
      NS_LOG_WARN ("FetchLsa: unsupported LSA type " << static_cast<uint32_t> (std::get<0> (lsaKey)));
      return {LsaHeader (), nullptr};
    }
}

} // namespace ns3
