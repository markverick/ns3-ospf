/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-state-serializer.h"

#include "ospf-app-private.h"

namespace ns3 {

OspfStateSerializer::OspfStateSerializer (OspfApp &app)
  : m_app (app)
{
}

void
OspfStateSerializer::ExportOspf (std::filesystem::path dirName, std::string nodeName)
{
  ExportMetadata (dirName, nodeName + ".meta");
  ExportLsdb (dirName, nodeName + ".lsdb");
  ExportNeighbors (dirName, nodeName + ".neighbors");
  ExportPrefixes (dirName, nodeName + ".prefixes");
}

void
OspfStateSerializer::ExportLsdb (std::filesystem::path dirName, std::string filename)
{
  // Export LSDBs
  // Pack it in a giant LS Update
  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  for (auto &[lsId, lsa] : m_app.m_routerLsdb)
    {
      lsUpdate->AddLsa (lsa);
    }
  for (auto &[lsId, lsa] : m_app.m_l1SummaryLsdb)
    {
      lsUpdate->AddLsa (lsa);
    }
  for (auto &[lsId, lsa] : m_app.m_areaLsdb)
    {
      lsUpdate->AddLsa (lsa);
    }
  for (auto &[lsId, lsa] : m_app.m_l2SummaryLsdb)
    {
      lsUpdate->AddLsa (lsa);
    }

  // Serialize into a buffer
  Buffer buffer;
  buffer.AddAtEnd (lsUpdate->GetSerializedSize ());
  lsUpdate->Serialize (buffer.Begin ());

  // Convert buffer to vector<uint8_t>
  std::vector<uint8_t> data (buffer.GetSize ());
  auto it = buffer.Begin ();
  it.Read (data.data (), buffer.GetSize ());

  // Write LSU to the file
  std::string fullname = dirName / filename;
  std::ofstream outFile (fullname);
  if (!outFile)
    {
      std::cerr << "Failed to open file for writing LSDB: " << fullname << std::endl;
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  std::cout << "Exported " << lsUpdate->GetNLsa () << " LSAs : " << data.size () << " bytes to "
            << fullname << std::endl;
}

void
OspfStateSerializer::ExportNeighbors (std::filesystem::path dirName, std::string filename)
{
  // Export Neighbor Information
  // Serialize neighbors
  Buffer buffer;
  uint32_t totalNeighbors = 0;
  uint32_t serializedSize = 4; // number of interfaces
  for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
    {
      serializedSize += 4; // number of neighbors
      serializedSize +=
          m_app.m_ospfInterfaces[i]->GetNeighbors ().size () * 12; // each neighbor is 12 bytes
      totalNeighbors += m_app.m_ospfInterfaces[i]->GetNeighbors ().size ();
    }
  buffer.AddAtEnd (serializedSize);
  Buffer::Iterator it = buffer.Begin ();

  it.WriteHtonU32 (m_app.m_ospfInterfaces.size () - 1);
  for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
    {
      it.WriteHtonU32 (m_app.m_ospfInterfaces[i]->GetNeighbors ().size ());
      for (auto n : m_app.m_ospfInterfaces[i]->GetNeighbors ())
        {
          it.WriteHtonU32 (n->GetRouterId ().Get ());
          it.WriteHtonU32 (n->GetIpAddress ().Get ());
          it.WriteHtonU32 (n->GetArea ());
        }
    }

  // Convert buffer to vector<uint8_t>
  std::vector<uint8_t> data (buffer.GetSize ());
  it = buffer.Begin ();
  it.Read (data.data (), buffer.GetSize ());

  // Write neighbors to the file
  std::string fullname = dirName / filename;
  std::ofstream outFile (fullname);
  if (!outFile)
    {
      std::cerr << "Failed to open file for writing neighbor information: " << fullname
                << std::endl;
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  std::cout << "Exported " << totalNeighbors << " neighbors : " << data.size () << " bytes to "
            << fullname << std::endl;
}

void
OspfStateSerializer::ExportMetadata (std::filesystem::path dirName, std::string filename)
{
  // Export additional Information
  Buffer buffer;
  uint32_t serializedSize = 4; // isLeader
  buffer.AddAtEnd (serializedSize);
  Buffer::Iterator it = buffer.Begin ();

  it.WriteHtonU32 (m_app.m_isAreaLeader);

  // Convert buffer to vector<uint8_t>
  std::vector<uint8_t> data (buffer.GetSize ());
  it = buffer.Begin ();
  it.Read (data.data (), buffer.GetSize ());

  // Write metadata to the file
  std::string fullname = dirName / filename;
  std::ofstream outFile (fullname);
  if (!outFile)
    {
      std::cerr << "Failed to open file for writing neighbor information: " << fullname
                << std::endl;
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  std::cout << "Exported metadata of " << data.size () << " bytes to " << fullname << std::endl;
}

void
OspfStateSerializer::ExportPrefixes (std::filesystem::path dirName, std::string filename)
{
  // Export external routes
  Buffer buffer;
  uint32_t serializedSize = 4 + m_app.m_externalRoutes.size () * 5 * 4; // numRoutes + 5x u32
  buffer.AddAtEnd (serializedSize);
  Buffer::Iterator it = buffer.Begin ();

  it.WriteHtonU32 (m_app.m_externalRoutes.size ());
  for (auto &[a, b, c, d, e] : m_app.m_externalRoutes)
    {
      it.WriteHtonU32 (a);
      it.WriteHtonU32 (b);
      it.WriteHtonU32 (c);
      it.WriteHtonU32 (d);
      it.WriteHtonU32 (e);
    }

  // Convert buffer to vector<uint8_t>
  std::vector<uint8_t> data (buffer.GetSize ());
  it = buffer.Begin ();
  it.Read (data.data (), buffer.GetSize ());

  // Write prefixes to the file
  std::string fullname = dirName / filename;
  std::ofstream outFile (fullname);
  if (!outFile)
    {
      std::cerr << "Failed to open file for writing external routes: " << fullname << std::endl;
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  std::cout << "Exported external routes of " << data.size () << " bytes to " << fullname
            << std::endl;
}

void
OspfStateSerializer::ImportOspf (std::filesystem::path dirName, std::string nodeName)
{
  ImportMetadata (dirName, nodeName + ".meta");
  ImportLsdb (dirName, nodeName + ".lsdb");
  ImportNeighbors (dirName, nodeName + ".neighbors");
  ImportPrefixes (dirName, nodeName + ".prefixes");
  m_app.m_doInitialize = false;
}

void
OspfStateSerializer::ImportLsdb (std::filesystem::path dirName, std::string filename)
{
  // Import LSDBs
  std::string fullname = dirName / filename;
  std::ifstream inFile (fullname, std::ios::binary);
  if (!inFile)
    {
      std::cerr << "Failed to open file for reading LSDB: " << fullname << std::endl;
      return;
    }

  // Read file into vector
  std::vector<uint8_t> data ((std::istreambuf_iterator<char> (inFile)),
                             std::istreambuf_iterator<char> ());

  // Create buffer and allocate space
  Buffer buffer;
  buffer.AddAtEnd (data.size ());

  // Write data into buffer
  auto it = buffer.Begin ();
  it.Write (data.data (), data.size ());

  Ptr<LsUpdate> lsUpdate = Create<LsUpdate> ();
  lsUpdate->Deserialize (buffer.Begin ());

  for (auto &[lsaHeader, lsa] : lsUpdate->GetLsaList ())
    {
      auto lsId = lsaHeader.GetLsId ();
      switch (lsaHeader.GetType ())
        {
        case LsaHeader::RouterLSAs:
          m_app.m_routerLsdb[lsId] = std::make_pair (lsaHeader, DynamicCast<RouterLsa> (lsa));
          break;
        case LsaHeader::L1SummaryLSAs:
          m_app.m_l1SummaryLsdb[lsId] = std::make_pair (lsaHeader, DynamicCast<L1SummaryLsa> (lsa));
          break;
        case LsaHeader::AreaLSAs:
          m_app.m_areaLsdb[lsId] = std::make_pair (lsaHeader, DynamicCast<AreaLsa> (lsa));
          break;
        case LsaHeader::L2SummaryLSAs:
          m_app.m_l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, DynamicCast<L2SummaryLsa> (lsa));
          break;
        default:
          std::cerr << "Unsupported LSA Type" << std::endl;
        }
      m_app.m_seqNumbers[lsaHeader.GetKey ()] = lsaHeader.GetSeqNum ();
    }

  std::cout << "Imported " << lsUpdate->GetNLsa () << " LSAs : " << data.size () << " bytes from "
            << fullname << std::endl;
}

void
OspfStateSerializer::ImportNeighbors (std::filesystem::path dirName, std::string filename)
{
  // Import Neighbor Information
  std::string fullname = dirName / filename;
  std::ifstream inFile (fullname, std::ios::binary);
  if (!inFile)
    {
      std::cerr << "Failed to open file for reading neighbor information: " << fullname
                << std::endl;
      return;
    }

  // Read file into vector
  std::vector<uint8_t> data ((std::istreambuf_iterator<char> (inFile)),
                             std::istreambuf_iterator<char> ());

  // Create buffer and allocate space
  Buffer buffer;
  buffer.AddAtEnd (data.size ());

  // Write data into buffer
  auto it = buffer.Begin ();
  it.Write (data.data (), data.size ());

  it = buffer.Begin ();
  uint32_t nInterfaces = it.ReadNtohU32 ();
  NS_ASSERT_MSG (nInterfaces + 1 == m_app.m_ospfInterfaces.size (),
                 "Numbers of bound interfaces do not match");

  uint32_t nNeighbors, routerId, ipAddress, areaId, totalNeighbors = 0;
  for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
    {
      nNeighbors = it.ReadNtohU32 ();
      totalNeighbors += nNeighbors;
      for (uint32_t j = 0; j < nNeighbors; j++)
        {
          routerId = it.ReadNtohU32 ();
          ipAddress = it.ReadNtohU32 ();
          areaId = it.ReadNtohU32 ();
          auto neighbor = Create<OspfNeighbor> (Ipv4Address (routerId), Ipv4Address (ipAddress),
                                                areaId, OspfNeighbor::Full);
          neighbor->RefreshLastHelloReceived ();
          m_app.RefreshHelloTimeout (i, neighbor);
          m_app.m_ospfInterfaces[i]->AddNeighbor (neighbor);
        }
    }

  std::cout << "Imported " << totalNeighbors << " neighbors : " << data.size () << " bytes from "
            << fullname << std::endl;
}

void
OspfStateSerializer::ImportMetadata (std::filesystem::path dirName, std::string filename)
{
  // Import Additional Information
  std::string fullname = dirName / filename;
  std::ifstream inFile (fullname, std::ios::binary);
  if (!inFile)
    {
      std::cerr << "Failed to open file for reading additional information: " << fullname
                << std::endl;
      return;
    }

  // Read file into vector
  std::vector<uint8_t> data ((std::istreambuf_iterator<char> (inFile)),
                             std::istreambuf_iterator<char> ());

  // Create buffer and allocate space
  Buffer buffer;
  buffer.AddAtEnd (data.size ());

  // Write data into buffer
  auto it = buffer.Begin ();
  it.Write (data.data (), data.size ());

  it = buffer.Begin ();
  m_app.m_isAreaLeader = it.ReadNtohU32 ();

  std::cout << "Imported metadata of " << data.size () << " bytes from " << fullname << std::endl;
}

void
OspfStateSerializer::ImportPrefixes (std::filesystem::path dirName, std::string filename)
{
  // Import External Routes
  std::string fullname = dirName / filename;
  std::ifstream inFile (fullname, std::ios::binary);
  if (!inFile)
    {
      std::cerr << "Failed to open file for reading external routes: " << fullname << std::endl;
      return;
    }

  // Read file into vector
  std::vector<uint8_t> data ((std::istreambuf_iterator<char> (inFile)),
                             std::istreambuf_iterator<char> ());

  // Create buffer and allocate space
  Buffer buffer;
  buffer.AddAtEnd (data.size ());

  // Write data into buffer
  auto it = buffer.Begin ();
  it.Write (data.data (), data.size ());

  it = buffer.Begin ();
  uint32_t routeNum = it.ReadNtohU32 ();
  uint32_t a, b, c, d, e;
  for (uint32_t i = 0; i < routeNum; i++)
    {
      a = it.ReadNtohU32 ();
      b = it.ReadNtohU32 ();
      c = it.ReadNtohU32 ();
      d = it.ReadNtohU32 ();
      e = it.ReadNtohU32 ();
      m_app.m_externalRoutes.emplace_back (a, b, c, d, e);
    }

  std::cout << "Imported external routes of " << data.size () << " bytes from " << fullname
            << std::endl;
}

} // namespace ns3
