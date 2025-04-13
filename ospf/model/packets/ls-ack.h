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

#ifndef LS_ACK_H
#define LS_ACK_H

#include "ns3/object.h"
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/ospf-interface.h"
#include "ns3/lsa-header.h"

namespace ns3 {
/**
 * \ingroup ospf
 *
 * \brief LS Acknowledge Object
 */

class LsAck : public Object
{
public:
  /**
   * \brief Construct a LS Acknowledge Object
   */

  LsAck ();
  LsAck (std::vector<LsaHeader> lsaHeaders);
  LsAck (Ptr<Packet> packet);

  void AddLsaHeader (LsaHeader lsaHeader);
  void ClearLsaHeaders (void);
  bool HasLsaHeader (LsaHeader lsaHeader);

  LsaHeader GetLsaHeader (uint32_t index);
  std::vector<LsaHeader> GetLsaHeaders ();
  uint32_t GetNLsaHeaders ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual uint32_t Serialize (Buffer::Iterator start) const;
  virtual Ptr<Packet> ConstructPacket () const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t Deserialize (Ptr<Packet> packet);

private:
  std::vector<LsaHeader> m_lsaHeaders; //storing neighbor's router ID
};

} // namespace ns3

#endif /* LS_ACK_H */
