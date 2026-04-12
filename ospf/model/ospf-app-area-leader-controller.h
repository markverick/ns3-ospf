/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_AREA_LEADER_CONTROLLER_H
#define OSPF_APP_AREA_LEADER_CONTROLLER_H

#include <cstdint>

namespace ns3 {

class OspfApp;

class OspfAreaLeaderController
{
public:
  explicit OspfAreaLeaderController (OspfApp &app);

  void ScheduleInitialLeadershipAttempt ();
  void UpdateLeadershipEligibility ();

  void Begin ();
  void End ();

private:
  bool ShouldUseStaticLeader () const;
  bool ShouldUseReachableLowestLeader () const;
  bool HasLeadershipQuorum () const;
  bool IsStaticLeader () const;
  uint32_t SelectLeaderCandidateRouterId () const;
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_AREA_LEADER_CONTROLLER_H
