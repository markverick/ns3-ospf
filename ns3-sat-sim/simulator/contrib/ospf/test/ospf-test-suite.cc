/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/basic-simulation.h"
#include "ns3/test.h"
#include "end-to-end-test.h"
#include "four-node-test.h"

using namespace ns3;

class OspfTestSuite : public TestSuite {
public:
    OspfTestSuite() : TestSuite("ospf", UNIT) {

        // Running it complete with reading in files etc.
        AddTestCase(new FourNodetTeest, TestCase::QUICK);

    }
};
static OspfTestSuite OspfTestSuite;
