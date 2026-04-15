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
      NS_LOG_ERROR ("Failed to open file for writing LSDB: " << fullname);
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  NS_LOG_INFO ("Exported " << lsUpdate->GetNLsa () << " LSAs: " << data.size () << " bytes to "
                            << fullname);
}

void
OspfStateSerializer::ExportNeighbors (std::filesystem::path dirName, std::string filename)
{
  if (m_app.m_ospfInterfaces.empty ())
    {
      NS_LOG_ERROR ("Cannot export neighbors without initialized interfaces");
      return;
    }

  // Export Neighbor Information
  // Serialize neighbors
  Buffer buffer;
  uint32_t totalNeighbors = 0;
  std::vector<uint32_t> activeIfIndices;
  for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
    {
      if (m_app.GetOspfInterface (i) != nullptr)
        {
          activeIfIndices.push_back (i);
        }
    }
  uint32_t serializedSize = 4; // number of interfaces
  for (uint32_t ifIndex : activeIfIndices)
    {
      auto ospfIf = m_app.GetOspfInterface (ifIndex);
      serializedSize += 4; // number of neighbors
      serializedSize += ospfIf->GetNeighbors ().size () * 12; // each neighbor is 12 bytes
      totalNeighbors += ospfIf->GetNeighbors ().size ();
    }
  buffer.AddAtEnd (serializedSize);
  Buffer::Iterator it = buffer.Begin ();

  it.WriteHtonU32 (activeIfIndices.size ());
  for (uint32_t ifIndex : activeIfIndices)
    {
      auto ospfIf = m_app.GetOspfInterface (ifIndex);
      it.WriteHtonU32 (ospfIf->GetNeighbors ().size ());
      for (auto n : ospfIf->GetNeighbors ())
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
      NS_LOG_ERROR ("Failed to open file for writing neighbor information: " << fullname);
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  NS_LOG_INFO ("Exported " << totalNeighbors << " neighbors: " << data.size () << " bytes to "
                            << fullname);
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
      NS_LOG_ERROR ("Failed to open file for writing metadata: " << fullname);
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  NS_LOG_INFO ("Exported metadata of " << data.size () << " bytes to " << fullname);
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
      NS_LOG_ERROR ("Failed to open file for writing external routes: " << fullname);
      return;
    }
  outFile.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  outFile.close ();
  NS_LOG_INFO ("Exported external routes of " << data.size () << " bytes to " << fullname);
}

void
OspfStateSerializer::ImportOspf (std::filesystem::path dirName, std::string nodeName)
{
  ImportMetadata (dirName, nodeName + ".meta");
  ImportLsdb (dirName, nodeName + ".lsdb");
  ImportNeighbors (dirName, nodeName + ".neighbors");
  ImportPrefixes (dirName, nodeName + ".prefixes");
  m_app.RebuildPrefixOwnerTable ();
  m_app.UpdateRouting ();
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
      NS_LOG_ERROR ("Failed to open file for reading LSDB: " << fullname);
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
      NS_LOG_ERROR ("Empty LSDB file: " << fullname);
      return;
    }

  const uint32_t consumed = lsUpdate->Deserialize (buffer.Begin ());
  if (consumed == 0 || consumed > data.size ())
    {
      NS_LOG_ERROR ("Malformed LSDB file (cannot deserialize LSU): " << fullname);
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
                NS_LOG_ERROR ("Malformed LSDB: RouterLSA payload type mismatch");
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
                NS_LOG_ERROR ("Malformed LSDB: L1SummaryLSA payload type mismatch");
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
                NS_LOG_ERROR ("Malformed LSDB: AreaLSA payload type mismatch");
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
                NS_LOG_ERROR ("Malformed LSDB: L2SummaryLSA payload type mismatch");
                return;
              }
            l2SummaryLsdb[lsId] = std::make_pair (lsaHeader, casted);
          }
          break;
        default:
          NS_LOG_ERROR ("Unsupported LSA Type");
          return;
        }
      seqNumbers[lsaHeader.GetKey ()] = lsaHeader.GetSeqNum ();
    }

  // Commit staged state.
  m_app.ClearPendingLsaRegenerationState ();
  m_app.m_routerLsdb = std::move (routerLsdb);
  m_app.m_l1SummaryLsdb = std::move (l1SummaryLsdb);
  m_app.m_areaLsdb = std::move (areaLsdb);
  m_app.m_l2SummaryLsdb = std::move (l2SummaryLsdb);
  m_app.m_seqNumbers = std::move (seqNumbers);

  NS_LOG_INFO ("Imported " << lsUpdate->GetNLsa () << " LSAs: " << data.size () << " bytes from "
                            << fullname);
}

void
OspfStateSerializer::ImportNeighbors (std::filesystem::path dirName, std::string filename)
{
  // Import Neighbor Information
  std::string fullname = dirName / filename;
  std::ifstream inFile (fullname, std::ios::binary);
  if (!inFile)
    {
      NS_LOG_ERROR ("Failed to open file for reading neighbor information: " << fullname);
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
      NS_LOG_ERROR ("Truncated neighbors data: missing interface count");
      return;
    }

  std::vector<uint32_t> activeIfIndices;
  for (uint32_t i = 1; i < m_app.m_ospfInterfaces.size (); i++)
    {
      if (m_app.GetOspfInterface (i) != nullptr)
        {
          activeIfIndices.push_back (i);
        }
    }

  if (nInterfaces != activeIfIndices.size ())
    {
      NS_LOG_ERROR ("Numbers of selected OSPF interfaces do not match");
      return;
    }

  uint32_t nNeighbors = 0;
  uint32_t routerId = 0;
  uint32_t ipAddress = 0;
  uint32_t areaId = 0;
  uint32_t totalNeighbors = 0;
  std::vector<std::vector<Ptr<OspfNeighbor>>> importedNeighbors (m_app.m_ospfInterfaces.size ());
  for (uint32_t ordinal = 0; ordinal < activeIfIndices.size (); ++ordinal)
    {
      const uint32_t ifIndex = activeIfIndices[ordinal];
      if (!TryReadNtohU32 (it, nNeighbors))
        {
          NS_LOG_ERROR ("Truncated neighbors data: missing neighbor count");
          return;
        }

      totalNeighbors += nNeighbors;
      for (uint32_t j = 0; j < nNeighbors; j++)
        {
          if (!TryReadNtohU32 (it, routerId) || !TryReadNtohU32 (it, ipAddress) ||
              !TryReadNtohU32 (it, areaId))
            {
              NS_LOG_ERROR ("Truncated neighbors data: missing neighbor entry");
              return;
            }
          auto neighbor = Create<OspfNeighbor> (Ipv4Address (routerId), Ipv4Address (ipAddress),
                                                areaId, OspfNeighbor::Full);
          importedNeighbors[ifIndex].push_back (neighbor);
        }
    }

  m_app.ReplaceNeighbors (importedNeighbors);

  NS_LOG_INFO ("Imported " << totalNeighbors << " neighbors: " << data.size () << " bytes from "
                            << fullname);
}

void
OspfStateSerializer::ImportMetadata (std::filesystem::path dirName, std::string filename)
{
  // Import Additional Information
  std::string fullname = dirName / filename;
  std::ifstream inFile (fullname, std::ios::binary);
  if (!inFile)
    {
      NS_LOG_ERROR ("Failed to open file for reading additional information: " << fullname);
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
      NS_LOG_ERROR ("Truncated metadata: missing area-leader field");
      return;
    }
  m_app.m_isAreaLeader = (isLeader != 0);

  NS_LOG_INFO ("Imported metadata of " << data.size () << " bytes from " << fullname);
}

void
OspfStateSerializer::ImportPrefixes (std::filesystem::path dirName, std::string filename)
{
  // Import External Routes
  std::string fullname = dirName / filename;
  std::ifstream inFile (fullname, std::ios::binary);
  if (!inFile)
    {
      NS_LOG_ERROR ("Failed to open file for reading external routes: " << fullname);
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
      NS_LOG_ERROR ("Truncated external routes: missing route count");
      return;
    }
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t c = 0;
  uint32_t d = 0;
  uint32_t e = 0;
  OspfApp::ReachableRouteList importedRoutes;
  for (uint32_t i = 0; i < routeNum; i++)
    {
      if (!TryReadNtohU32 (it, a) || !TryReadNtohU32 (it, b) || !TryReadNtohU32 (it, c) ||
          !TryReadNtohU32 (it, d) || !TryReadNtohU32 (it, e))
        {
          NS_LOG_ERROR ("Truncated external routes: missing route entry");
          return;
        }
      importedRoutes.emplace_back (a, b, c, d, e);
    }

  m_app.m_externalRoutes = std::move (importedRoutes);
  m_app.m_interfaceExternalRoutes.clear ();
  m_app.m_injectedExternalRoutes.clear ();
  m_app.InitializeSplitReachableRoutesFromCurrentState ();

  NS_LOG_INFO ("Imported external routes of " << data.size () << " bytes from " << fullname);
}

} // namespace ns3
