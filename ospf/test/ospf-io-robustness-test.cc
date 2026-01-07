/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/test.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ls-ack.h"
#include "ns3/ls-request.h"
#include "ns3/ls-update.h"
#include "ns3/ospf-app.h"
#include "ns3/ospf-header.h"

#include "../model/ospf-app-lsa-processor.h"

namespace ns3 {

class OspfInvalidIfIndexNoCrashTestCase : public TestCase
{
public:
  OspfInvalidIfIndexNoCrashTestCase ()
    : TestCase ("Handlers drop invalid ifIndex without crashing")
  {
  }

  void
  DoRun () override
  {
    Ptr<OspfApp> app = CreateObject<OspfApp> ();
    OspfLsaProcessor lsa (*PeekPointer (app));

    Ipv4Header ipHeader;
    ipHeader.SetSource (Ipv4Address ("10.0.0.2"));

    OspfHeader ospfHeader;
    ospfHeader.SetRouterId (Ipv4Address ("10.0.0.1").Get ());

    constexpr uint32_t badIfIndex = 999999;

    lsa.HandleLsr (badIfIndex, ipHeader, ospfHeader, Create<LsRequest> ());
    lsa.HandleLsu (badIfIndex, ipHeader, ospfHeader, Create<LsUpdate> ());
    lsa.HandleLsAck (badIfIndex, ipHeader, ospfHeader, Create<LsAck> ());

    NS_TEST_ASSERT_MSG_EQ (true, true, "no crash");
  }
};

class OspfIoRobustnessTestSuite : public TestSuite
{
public:
  OspfIoRobustnessTestSuite ()
    : TestSuite ("ospf-io-robustness", Type::UNIT)
  {
    AddTestCase (new OspfInvalidIfIndexNoCrashTestCase (), TestCase::QUICK);
  }
};

static OspfIoRobustnessTestSuite g_ospfIoRobustnessTestSuite;

} // namespace ns3
