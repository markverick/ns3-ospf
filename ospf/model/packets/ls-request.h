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

#ifndef LS_REQUEST_H
#define LS_REQUEST_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/lsa-header.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief LS Request Object
 */

class LsRequest : public Object
{
public:
  /**
   * \brief Construct a LS Request Object
   */

  LsRequest ();
  LsRequest (std::vector<LsaHeader::LsaKey> lsaKeys);
  LsRequest (Ptr<Packet> packet);

  void AddLsaKey (LsaHeader::LsaKey lsaKey);
  bool RemoveLsaKey (LsaHeader::LsaKey lsaKey);
  bool IsLsaKeyEmpty ();
  void ClearLsaKeys (void);
  bool HasLsaKey (LsaHeader::LsaKey lsaKey);

  LsaHeader::LsaKey GetLsaKey (uint32_t index);
  std::vector<LsaHeader::LsaKey> GetLsaKeys ();
  uint32_t GetNLsaKeys ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual uint32_t Serialize (Buffer::Iterator start) const;
  virtual Ptr<Packet> ConstructPacket () const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t Deserialize (Ptr<Packet> packet);

private:
  std::vector<LsaHeader::LsaKey> m_lsaKeys; // storing LSA keys to request
};

} // namespace ns3

#endif /* LS_REQUEST_H */
