/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-private.h"
#include "ospf-app-state-serializer.h"

namespace ns3 {
void
OspfApp::PrintLsaTiming (uint32_t seqNum, LsaHeader::LsaKey lsaKey, Time time)
{
  auto str = LsaHeader::GetKeyString (seqNum, lsaKey);

  m_lsaTimingLog << time.GetNanoSeconds () << "," << str << std::endl;
}

// Import Export
void
OspfApp::ExportOspf (std::filesystem::path dirName, std::string nodeName)
{
  m_state->ExportOspf (dirName, nodeName);
}
void
OspfApp::ExportLsdb (std::filesystem::path dirName, std::string filename)
{
  m_state->ExportLsdb (dirName, filename);
}

void
OspfApp::ExportNeighbors (std::filesystem::path dirName, std::string filename)
{
  m_state->ExportNeighbors (dirName, filename);
}

void
OspfApp::ExportMetadata (std::filesystem::path dirName, std::string filename)
{
  m_state->ExportMetadata (dirName, filename);
}

void
OspfApp::ExportPrefixes (std::filesystem::path dirName, std::string filename)
{
  m_state->ExportPrefixes (dirName, filename);
}

void
OspfApp::ImportOspf (std::filesystem::path dirName, std::string nodeName)
{
  m_state->ImportOspf (dirName, nodeName);
}

void
OspfApp::ImportLsdb (std::filesystem::path dirName, std::string filename)
{
  m_state->ImportLsdb (dirName, filename);
}
void
OspfApp::ImportNeighbors (std::filesystem::path dirName, std::string filename)
{
  m_state->ImportNeighbors (dirName, filename);
}

void
OspfApp::ImportMetadata (std::filesystem::path dirName, std::string filename)
{
  m_state->ImportMetadata (dirName, filename);
}

void
OspfApp::ImportPrefixes (std::filesystem::path dirName, std::string filename)
{
  m_state->ImportPrefixes (dirName, filename);
}

} // namespace ns3
