/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-rng.h"

#include "ospf-app-private.h"

namespace ns3 {

OspfAppRng::OspfAppRng (OspfApp &app)
  : m_app (app)
{
}

void
OspfAppRng::InitializeRandomVariables ()
{
  // Generate random variables
  m_app.m_jitterRv->SetAttribute ("Min", DoubleValue (0.0)); // Minimum value in seconds
  m_app.m_jitterRv->SetAttribute ("Max", DoubleValue (5.0)); // Maximum value in seconds (5 ms)

  m_app.m_randomVariableSeq->SetAttribute ("Min", DoubleValue (0.0));
  m_app.m_randomVariableSeq->SetAttribute ("Max", DoubleValue ((1 << 16) * 1000)); // arbitrary number
}

} // namespace ns3
