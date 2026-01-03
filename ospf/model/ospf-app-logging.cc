/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-logging.h"

#include "ospf-app-private.h"

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
}

} // namespace ns3
