/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_LOGGING_H
#define OSPF_APP_LOGGING_H

#include <cstdint>
#include <string>

namespace ns3 {

class OspfApp;

class OspfAppLogging
{
public:
  explicit OspfAppLogging (OspfApp &app);

  void InitializeLoggingIfEnabled ();

  /**
   * Log an OSPF packet transmission (replaces PCAP capture for overhead measurement)
   * \param size Packet size in bytes
   * \param ospfType OSPF packet type (1=Hello, 2=DBD, 3=LSReq, 4=LSU, 5=LSAck)
   * \param lsaLevel LSA level ("L1", "L2", or "" for Hello/unknown)
   */
  void LogPacketTx (uint32_t size, uint8_t ospfType, const std::string &lsaLevel);

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_LOGGING_H
