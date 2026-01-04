/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-sockets.h"

#include "ospf-app-private.h"

namespace ns3 {

OspfAppSockets::OspfAppSockets (OspfApp &app)
  : m_app (app)
{
}

void
OspfAppSockets::InitializeSockets ()
{
  // Add local null sockets
  m_app.m_sockets.emplace_back (nullptr);
  m_app.m_helloSockets.emplace_back (nullptr);
  m_app.m_lsaSockets.emplace_back (nullptr);
  for (uint32_t i = 1; i < m_app.m_boundDevices.GetN (); i++)
    {
      // In auto-sync mode, skip sockets for interfaces that are currently down or missing.
      if (m_app.m_autoSyncInterfaces)
        {
          if (i >= m_app.m_ospfInterfaces.size () || m_app.m_ospfInterfaces[i] == nullptr ||
              !m_app.m_ospfInterfaces[i]->IsUp ())
            {
              m_app.m_helloSockets.emplace_back (nullptr);
              m_app.m_lsaSockets.emplace_back (nullptr);
              m_app.m_sockets.emplace_back (nullptr);
              continue;
            }
        }

      // Create sockets
      TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");

      InetSocketAddress anySocketAddress (Ipv4Address::GetAny ());

      // For Hello, both bind and listen to m_helloAddress
      auto helloSocket = Socket::CreateSocket (m_app.GetNode (), tid);
      InetSocketAddress helloSocketAddress (m_app.m_helloAddress);
      if (helloSocket->Bind (helloSocketAddress) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      helloSocket->Connect (helloSocketAddress);
      helloSocket->SetAllowBroadcast (true);
      helloSocket->SetAttribute ("Protocol", UintegerValue (89));
      helloSocket->SetIpTtl (1);
      helloSocket->BindToNetDevice (m_app.m_boundDevices.Get (i));
      helloSocket->SetRecvCallback (MakeCallback (&OspfApp::HandleRead, &m_app));
      m_app.m_helloSockets.emplace_back (helloSocket);

      // For LSA, both bind and listen to m_lsaAddress
      auto lsaSocket = Socket::CreateSocket (m_app.GetNode (), tid);
      InetSocketAddress lsaSocketAddress (m_app.m_lsaAddress);
      if (lsaSocket->Bind (lsaSocketAddress) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      lsaSocket->Connect (lsaSocketAddress);
      lsaSocket->SetAllowBroadcast (true);
      lsaSocket->SetAttribute ("Protocol", UintegerValue (89));
      lsaSocket->SetIpTtl (1);
      lsaSocket->BindToNetDevice (m_app.m_boundDevices.Get (i));
      lsaSocket->SetRecvCallback (MakeCallback (&OspfApp::HandleRead, &m_app));
      m_app.m_lsaSockets.emplace_back (lsaSocket);

      // For unicast, such as LSA retransmission, bind to local address
      auto unicastSocket = Socket::CreateSocket (m_app.GetNode (), tid);
      if (unicastSocket->Bind (anySocketAddress) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      unicastSocket->SetAllowBroadcast (true);
      unicastSocket->SetAttribute ("Protocol", UintegerValue (89));
      unicastSocket->SetIpTtl (1); // Only allow local hop
      unicastSocket->BindToNetDevice (m_app.m_boundDevices.Get (i));
      unicastSocket->SetRecvCallback (MakeCallback (&OspfApp::HandleRead, &m_app));
      m_app.m_sockets.emplace_back (unicastSocket);
    }
}

void
OspfAppSockets::CancelHelloTimeouts ()
{
  for (auto &timeouts : m_app.m_helloTimeouts)
    {
      for (auto &timer : timeouts)
        {
          timer.second.Remove ();
        }
      timeouts.clear ();
    }
}

void
OspfAppSockets::CloseSockets ()
{
  for (uint32_t i = 1; i < m_app.m_sockets.size (); i++)
    {
      // Hello
      if (m_app.m_helloSockets[i] != nullptr)
        {
          m_app.m_helloSockets[i]->Close ();
          m_app.m_helloSockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
        }
      m_app.m_helloSockets[i] = nullptr;

      // LSA
      if (m_app.m_lsaSockets[i] != nullptr)
        {
          m_app.m_lsaSockets[i]->Close ();
          m_app.m_lsaSockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
        }
      m_app.m_lsaSockets[i] = nullptr;

      // Unicast
      if (m_app.m_sockets[i] != nullptr)
        {
          m_app.m_sockets[i]->Close ();
          m_app.m_sockets[i]->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
        }
      m_app.m_sockets[i] = nullptr;
    }
  m_app.m_helloSockets.clear ();
  m_app.m_lsaSockets.clear ();
  m_app.m_sockets.clear ();
}

void
OspfAppSockets::ScheduleTransmitHello (Time dt)
{
  m_app.m_helloEvent =
      Simulator::Schedule (dt + MilliSeconds (m_app.m_jitterRv->GetValue ()), &OspfApp::SendHello, &m_app);
}

} // namespace ns3
