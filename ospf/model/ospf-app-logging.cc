/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-logging.h"

#include "ospf-app-private.h"
#include "ns3/simulator.h"

namespace ns3 {

OspfAppLogging::OspfAppLogging (OspfApp &app)
  : m_app (app)
{
}

void
OspfAppLogging::InitializeLoggingIfEnabled ()
{
  if (m_app.m_enableLog)
    {
      std::string fullname =
          m_app.m_logDir + "/lsa-timings/" + std::to_string (m_app.GetNode ()->GetId ()) + ".csv";
      std::filesystem::path pathObj (fullname);
      std::filesystem::path dir = pathObj.parent_path ();
      if (!dir.empty () && !std::filesystem::exists (dir))
        {
          std::filesystem::create_directories (dir);
        }
      m_app.m_lsaTimingLog = std::ofstream (fullname, std::ios::trunc);
      m_app.m_lsaTimingLog << "timestamp,lsa_key" << std::endl;

      fullname = m_app.m_logDir + "/lsa_mapping.csv";
      ;
      auto mappingLog = std::ofstream (fullname, std::ios::trunc);
      mappingLog << "l1_key,l2_key" << std::endl;
      mappingLog.close ();
    }

  // Initialize packet logging (replaces PCAP for overhead measurement)
  if (m_app.m_enablePacketLog)
    {
      std::string fullname =
          m_app.m_logDir + "/ospf-packets/" + std::to_string (m_app.GetNode ()->GetId ()) + ".csv";
      std::filesystem::path pathObj (fullname);
      std::filesystem::path dir = pathObj.parent_path ();
      if (!dir.empty () && !std::filesystem::exists (dir))
        {
          std::filesystem::create_directories (dir);
        }
      m_app.m_packetLog = std::ofstream (fullname, std::ios::trunc);
      m_app.m_packetLog << "timestamp,size,type,lsa_level" << std::endl;
    }
}

void
OspfAppLogging::LogPacketTx (uint32_t size, uint8_t ospfType, const std::string &lsaLevel)
{
  if (m_app.m_enablePacketLog && m_app.m_packetLog.is_open ())
    {
      // Format: timestamp,size,type,lsa_level
      // timestamp is in seconds (matching PCAP format)
      m_app.m_packetLog << Simulator::Now ().GetSeconds () << ","
                        << size << ","
                        << static_cast<int> (ospfType) << ","
                        << lsaLevel << std::endl;
    }
}

} // namespace ns3
