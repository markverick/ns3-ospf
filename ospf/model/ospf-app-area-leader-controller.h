/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_AREA_LEADER_CONTROLLER_H
#define OSPF_APP_AREA_LEADER_CONTROLLER_H

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
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_AREA_LEADER_CONTROLLER_H
