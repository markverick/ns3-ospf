/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_ROUTING_ENGINE_H
#define OSPF_APP_ROUTING_ENGINE_H

namespace ns3 {

class OspfApp;

class OspfRoutingEngine
{
public:
  explicit OspfRoutingEngine (OspfApp &app);

  void UpdateRouting ();
  void ScheduleUpdateL1ShortestPath ();
  void UpdateL1ShortestPath ();
  void ScheduleUpdateL2ShortestPath ();
  void UpdateL2ShortestPath ();

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_ROUTING_ENGINE_H
