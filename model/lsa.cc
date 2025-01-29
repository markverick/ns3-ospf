/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2024 University of California, Los Angeles
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
 * Author: Sirapop Theeranantachai (stheera@g.ucla.edu)
 */

#include "ns3/assert.h"
#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/packet.h"
#include "lsa.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Lsa");

NS_OBJECT_ENSURE_REGISTERED (Lsa);

uint32_t 
Lsa::GetSerializedSize (void) const
{
	return 0;
}

TypeId 
Lsa::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Lsa")
    .SetGroupName ("Ospf")
    .AddConstructor<Lsa> ()
  ;
  return tid;
}
TypeId 
Lsa::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}

uint32_t
Lsa::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  return GetSerializedSize();
}

uint32_t
Lsa::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  return GetSerializedSize ();
}

uint32_t
Lsa::Deserialize (Ptr<Packet> packet)
{
  return GetSerializedSize ();
}


} // namespace ns3
