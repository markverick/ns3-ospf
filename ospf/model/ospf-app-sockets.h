/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_SOCKETS_H
#define OSPF_APP_SOCKETS_H

#include "ns3/nstime.h"

namespace ns3 {

class OspfApp;

class OspfAppSockets
{
public:
  explicit OspfAppSockets (OspfApp &app);

  void InitializeSockets ();
  void CancelHelloTimeouts ();
  void CloseSockets ();
  void ScheduleTransmitHello (Time dt);

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_SOCKETS_H
