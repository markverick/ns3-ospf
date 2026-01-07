/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-app-state-serializer.h"

#include "ospf-app-private.h"

namespace ns3 {

namespace {

bool
TryReadNtohU32 (Buffer::Iterator &it, uint32_t &out)
{
  if (it.GetRemainingSize () < 4)
    {
      return false;
    }
  out = it.ReadNtohU32 ();
  return true;
}

} // namespace

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
  std::ofstream outFile (fullname, std::ios::binary);
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
  if (m_app.m_ospfInterfaces.empty ())
    {
      std::cerr << "Cannot export neighbors without initialized interfaces" << std::endl;
      return;
    }

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
  std::ofstream outFile (fullname, std::ios::binary);
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
  std::ofstream outFile (fullname, std::ios::binary);
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
  std::ofstream outFile (fullname, std::ios::binary);
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
  if (data.empty ())
    {
      std::cerr << "Empty LSDB file: " << fullname << std::endl;
      return;
    }

  const uint32_t consumed = lsUpdate->Deserialize (buffer.Begin ());
  if (consumed == 0 || consumed > data.size ())
    {
      std::cerr << "Malformed LSDB file (cannot deserialize LSU): " << fullname << std::endl;
      return;
    }

  // Build a local staging area so we don't partially mutate state.
  std::map<uint32_t, std::pair<LsaHeader, Ptr<RouterLsa>>> routerLsdb;
  std::map<uint32_t, std::pair<LsaHeader, Ptr<L1SummaryLsa>>> l1SummaryLsdb;
  std::map<uint32_t, std::pair<LsaHeader, Ptr<AreaLsa>>> areaLsdb;
  std::map<uint32_t, std::pair<LsaHeader, Ptr<L2SummaryLsa>>> l2SummaryLsdb;
  std::map<LsaHeader::LsaKey, uint16_t> seqNumbers;

  for (auto &[lsaHeader, lsa] : lsUpdate->GetLsaList ())
    {
      auto lsId = lsaHeader.GetLsId ();
      switch (lsaHeader.GetType ())
        {
        case LsaHeader::RouterLSAs:
          {
            auto casted = DynamicCast<RouterLsa> (lsa);
            if (!casted)
              {
                std::cerr << "Malformed LSDB: RouterLSA payload type mismatch" << std::endl;
                return;
              }
            routerLsdb[lsId] = std::make_pair (lsaHeader, casted);
          }
          break;
        case LsaHeader::L1SummaryLSAs:
          {
            auto casted = DynamicCast<L1SummaryLsa> (lsa);
            if (!casted)
              {
                std::cerr << "Malformed LSDB: L1SummaryLSA payload type mismatch" << std::endl;
                return;
              }
            l1SummaryLsdb[lsId] = std::make_pair (lsaHeader, casted);
          }
          break;
        case LsaHeader::AreaLSAs:
          {
            auto casted = DynamicCast<AreaLsa> (lsa);
            if (!casted)
              {
                std::cerr << "Malformed LSDB: AreaLSA payload type mismatch" << std::endl;
                return;
              }
            areaLsdb[lsId] = std::make_pair (lsaHeader, casted);
          }
          break;
        case LsaHeader::L2SummaryLSAs:
          {
            auto casted = DynamicCast<L2SummaryLsa> (lsa);
            if (!casted)
              {
                std::cerr << "Malformed LSDB: L2SummaryLSA payload type mismatch" << std::endl;
                return;
              }
            l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, casted);
          }
          break;
        default:
          std::cerr << "Unsupported LSA Type" << std::endl;
          return;
        }
      seqNumbers[lsaHeader.GetKey ()] = lsaHeader.GetSeqNum ();
    }

  // Commit staged state.
  m_app.m_routerLsdb.insert (routerLsdb.begin (), routerLsdb.end ());
  m_app.m_l1SummaryLsdb.insert (l1SummaryLsdb.begin (), l1SummaryLsdb.end ());
  m_app.m_areaLsdb.insert (areaLsdb.begin (), areaLsdb.end ());
  m_app.m_l2SummaryLsdb.insert (l2SummaryLsdb.begin (), l2SummaryLsdb.end ());
  m_app.m_seqNumbers.insert (seqNumbers.begin (), seqNumbers.end ());

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
  uint32_t nInterfaces = 0;
  if (!TryReadNtohU32 (it, nInterfaces))
    {
      std::cerr << "Truncated neighbors data: missing interface count" << std::endl;
      return;
    }
  if (nInterfaces + 1 != m_app.m_ospfInterfaces.size ())
    {
      std::cerr << "Numbers of bound interfaces do not match" << std::endl;
      return;
    }

  uint32_t nNeighbors = 0;
  uint32_t routerId = 0;
  uint32_t ipAddress = 0;
  uint32_t areaId = 0;
  uint32_t totalNeighbors = 0;
  for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
    {
      if (!TryReadNtohU32 (it, nNeighbors))
        {
          std::cerr << "Truncated neighbors data: missing neighbor count" << std::endl;
          return;
        }
      totalNeighbors += nNeighbors;
      for (uint32_t j = 0; j < nNeighbors; j++)
        {
          if (!TryReadNtohU32 (it, routerId) || !TryReadNtohU32 (it, ipAddress) ||
              !TryReadNtohU32 (it, areaId))
            {
              std::cerr << "Truncated neighbors data: missing neighbor entry" << std::endl;
              return;
            }
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
  uint32_t isLeader = 0;
  if (!TryReadNtohU32 (it, isLeader))
    {
      std::cerr << "Truncated metadata: missing area-leader field" << std::endl;
      return;
    }
  m_app.m_isAreaLeader = (isLeader != 0);

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
  uint32_t routeNum = 0;
  if (!TryReadNtohU32 (it, routeNum))
    {
      std::cerr << "Truncated external routes: missing route count" << std::endl;
      return;
    }
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t c = 0;
  uint32_t d = 0;
  uint32_t e = 0;
  for (uint32_t i = 0; i < routeNum; i++)
    {
      if (!TryReadNtohU32 (it, a) || !TryReadNtohU32 (it, b) || !TryReadNtohU32 (it, c) ||
          !TryReadNtohU32 (it, d) || !TryReadNtohU32 (it, e))
        {
          std::cerr << "Truncated external routes: missing route entry" << std::endl;
          return;
        }
      m_app.m_externalRoutes.emplace_back (a, b, c, d, e);
    }

  std::cout << "Imported external routes of " << data.size () << " bytes from " << fullname
            << std::endl;
}

} // namespace ns3
