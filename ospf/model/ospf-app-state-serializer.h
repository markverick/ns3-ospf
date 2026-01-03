/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_APP_STATE_SERIALIZER_H
#define OSPF_APP_STATE_SERIALIZER_H

#include <filesystem>
#include <string>

namespace ns3 {

class OspfApp;

class OspfStateSerializer
{
public:
  explicit OspfStateSerializer (OspfApp &app);

  void ExportOspf (std::filesystem::path dirName, std::string nodeName);
  void ExportLsdb (std::filesystem::path dirName, std::string filename);
  void ExportNeighbors (std::filesystem::path dirName, std::string filename);
  void ExportMetadata (std::filesystem::path dirName, std::string filename);
  void ExportPrefixes (std::filesystem::path dirName, std::string filename);

  void ImportOspf (std::filesystem::path dirName, std::string nodeName);
  void ImportLsdb (std::filesystem::path dirName, std::string filename);
  void ImportNeighbors (std::filesystem::path dirName, std::string filename);
  void ImportMetadata (std::filesystem::path dirName, std::string filename);
  void ImportPrefixes (std::filesystem::path dirName, std::string filename);

private:
  OspfApp &m_app;
};

} // namespace ns3

#endif // OSPF_APP_STATE_SERIALIZER_H
