/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_LOGGING_H
#define OSPF_APP_LOGGING_H

namespace ns3 {

class OspfApp;

class OspfAppLogging
{
public:
  explicit OspfAppLogging (OspfApp &app);

  void InitializeLoggingIfEnabled ();

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_LOGGING_H
