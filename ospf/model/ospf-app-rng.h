/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_RNG_H
#define OSPF_APP_RNG_H

namespace ns3 {

class OspfApp;

class OspfAppRng
{
public:
  explicit OspfAppRng (OspfApp &app);

  void InitializeRandomVariables ();

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_RNG_H
