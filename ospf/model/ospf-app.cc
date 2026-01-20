/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Sirapop Theeranantachai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Sirapop Theeranantachaoi <stheera@g.ucla.edu>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/header.h"
#include "ns3/random-variable-stream.h"
#include "ns3/core-module.h"
#include "ns3/ospf-packet-helper.h"

#include <memory>

#include "ospf-app.h"

#include "ospf-app-area-leader-controller.h"
#include "ospf-app-io-component.h"
#include "ospf-app-logging.h"
#include "ospf-app-lsa-processor.h"
#include "ospf-app-neighbor-fsm.h"
#include "ospf-app-rng.h"
#include "ospf-app-routing-engine.h"
#include "ospf-app-sockets.h"
#include "ospf-app-state-serializer.h"

namespace ns3 {

LogComponent g_log ("OspfApp", __FILE__);

NS_OBJECT_ENSURE_REGISTERED (OspfApp);

TypeId
OspfApp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::OspfApp")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<OspfApp> ()
          .AddAttribute ("HelloInterval", "OSPF Hello Interval", TimeValue (MilliSeconds (10000)),
                         MakeTimeAccessor (&OspfApp::m_helloInterval), MakeTimeChecker ())
          .AddAttribute ("InitialHelloDelay", "Initial Hello Delay", TimeValue (MilliSeconds (0)),
                         MakeTimeAccessor (&OspfApp::m_initialHelloDelay), MakeTimeChecker ())
          .AddAttribute ("HelloAddress", "Multicast address of Hello",
                         Ipv4AddressValue (Ipv4Address ("224.0.0.5")),
                         MakeIpv4AddressAccessor (&OspfApp::m_helloAddress),
                         MakeIpv4AddressChecker ())
          .AddAttribute ("LSAAddress", "Multicast address of LSAs",
                         Ipv4AddressValue (Ipv4Address ("224.0.0.6")),
                         MakeIpv4AddressAccessor (&OspfApp::m_lsaAddress),
                         MakeIpv4AddressChecker ())
          .AddAttribute ("LogDir", "Log Directory", StringValue ("results/"),
                         MakeStringAccessor (&OspfApp::m_logDir), MakeStringChecker ())
          .AddAttribute ("EnableLsaTimingLog", "Enable LSA timing logs for convergence analysis",
                         BooleanValue (false), MakeBooleanAccessor (&OspfApp::m_enableLsaTimingLog),
                         MakeBooleanChecker ())
          .AddAttribute ("EnablePacketLog", "Enable OSPF packet logging for overhead measurement",
                         BooleanValue (false), MakeBooleanAccessor (&OspfApp::m_enablePacketLog),
                         MakeBooleanChecker ())
          .AddAttribute ("IncludeHelloInPacketLog", "Include Hello packets in packet log",
                         BooleanValue (false), MakeBooleanAccessor (&OspfApp::m_includeHelloInPacketLog),
                         MakeBooleanChecker ())
          .AddAttribute (
              "RouterDeadInterval",
              "Link is considered down when not receiving Hello until RouterDeadInterval",
              TimeValue (MilliSeconds (30000)), MakeTimeAccessor (&OspfApp::m_routerDeadInterval),
              MakeTimeChecker ())
          .AddAttribute ("LSUInterval", "LSU Retransmission Interval", TimeValue (MilliSeconds (5000)),
                         MakeTimeAccessor (&OspfApp::m_rxmtInterval), MakeTimeChecker ())
          .AddAttribute ("DefaultArea", "Default area ID for router", UintegerValue (0),
                         MakeUintegerAccessor (&OspfApp::m_areaId),
                         MakeUintegerChecker<uint32_t> ())
          .AddAttribute ("AreaMask", "Area mask for the router",
                         Ipv4MaskValue (Ipv4Mask ("255.0.0.0")),
                         MakeIpv4MaskAccessor (&OspfApp::m_areaMask), MakeIpv4MaskChecker ())
          .AddAttribute ("EnableAreaProxy", "Enable area proxy for area routing",
                         BooleanValue (true), MakeBooleanAccessor (&OspfApp::m_enableAreaProxy),
                         MakeBooleanChecker ())
          .AddAttribute ("ShortestPathUpdateDelay", "Delay to re-calculate the shortest path",
                         TimeValue (Seconds (5)),
                         MakeTimeAccessor (&OspfApp::m_shortestPathUpdateDelay), MakeTimeChecker ())
          .AddAttribute ("MinLsInterval",
                         "Minimum interval between originating the same LSA (RFC 2328 MinLSInterval)",
                         TimeValue (Seconds (0)),
                         MakeTimeAccessor (&OspfApp::m_minLsInterval), MakeTimeChecker ())
          .AddAttribute ("AutoSyncInterfaces",
                         "If true, OSPF automatically tracks the node's Ipv4 interfaces (up/down/add/remove) and updates its bound interfaces accordingly.",
                         BooleanValue (false),
                         MakeBooleanAccessor (&OspfApp::m_autoSyncInterfaces),
                         MakeBooleanChecker ())
          .AddAttribute ("InterfaceSyncInterval",
                         "Polling interval for Ipv4 interface synchronization when AutoSyncInterfaces is enabled.",
                         TimeValue (MilliSeconds (200)),
                         MakeTimeAccessor (&OspfApp::m_interfaceSyncInterval),
                         MakeTimeChecker ())
            .AddAttribute ("ResetStateOnDisable",
                   "When Disable() is called, clear neighbor/LSDB state and remove OSPF-installed routes so Enable() behaves like a clean re-join",
                           BooleanValue (false),
                   MakeBooleanAccessor (&OspfApp::m_resetStateOnDisable),
                   MakeBooleanChecker ())
          .AddTraceSource ("Tx", "A new packet is created and is sent",
                           MakeTraceSourceAccessor (&OspfApp::m_txTrace),
                           "ns3::Packet::TracedCallback")
          .AddTraceSource ("Rx", "A packet has been received",
                           MakeTraceSourceAccessor (&OspfApp::m_rxTrace),
                           "ns3::Packet::TracedCallback")
          .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
                           MakeTraceSourceAccessor (&OspfApp::m_txTraceWithAddresses),
                           "ns3::Packet::TwoAddressTracedCallback")
          .AddTraceSource ("RxWithAddresses", "A packet has been received",
                           MakeTraceSourceAccessor (&OspfApp::m_rxTraceWithAddresses),
                           "ns3::Packet::TwoAddressTracedCallback");
  return tid;
}

OspfApp::OspfApp ()
  : m_io (std::make_unique<OspfAppIo> (*this)),
    m_neighborFsm (std::make_unique<OspfNeighborFsm> (*this)),
    m_lsa (std::make_unique<OspfLsaProcessor> (*this)),
    m_state (std::make_unique<OspfStateSerializer> (*this)),
    m_socketsMgr (std::make_unique<OspfAppSockets> (*this)),
    m_logging (std::make_unique<OspfAppLogging> (*this)),
    m_rng (std::make_unique<OspfAppRng> (*this)),
    m_areaLeader (std::make_unique<OspfAreaLeaderController> (*this)),
    m_routingEngine (std::make_unique<OspfRoutingEngine> (*this))
{
  NS_LOG_FUNCTION (this);
}

OspfApp::~OspfApp ()
{
  m_sockets.clear ();
  m_lsaSockets.clear ();
  m_helloSockets.clear ();
}

} // namespace ns3
