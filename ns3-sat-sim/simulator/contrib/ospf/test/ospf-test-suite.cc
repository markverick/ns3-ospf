/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/test.h"
#include "four-node-test.h"

using namespace ns3;

// TODO: Unfinished Tests
class OspfTestSuite : public TestSuite {
public:
    OspfTestSuite() : TestSuite("ospf", UNIT) {

        // Running it complete with reading in files etc.
        AddTestCase(new FourNodeTestCase, TestCase::QUICK);

    }
};
