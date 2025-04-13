Simplified OSPF module for ns-3
===============================
This project is an external module for the [ns-3 network simulator](https://www.nsnam.org/), implementing basic functionality of the Open Shortest Path First Version 2 (OSPFv2) routing protocol as defined in [RFC 2328](https://datatracker.ietf.org/doc/rfc2328/). This module is targetted for ns-3 version 3.35.

## License

This software is licensed under the terms of the GNU General Public License v2.0 only (GPL-2.0-only).
See the LICENSE file for more details.

## Table of Contents:

1) [Overview](#overview)
2) [Instruction](#instruction)
3) [Reporting Issues](#reporting-issues)


## Overview
### What's currently implemented 
- Basic OSPF functionality for point-to-point routers
- Data synchronization and flooding procedures
- Area Proxy implementation (RFC 9666) for OSPF

### Major Simplifications
- Link State Updates subject to flooding contain only one Link State Advertisement (LSA) each.
   - Link State Updates serving as implicit acknowledgments to Link State Requests may contain multiple LSAs, up to the specified MTU.
- Link State Updates received within a short time window are not aggregated.
- Authentication is not implemented.
- Inter-AS routing is not implemented.

### Current Limitations
- Broadcast networks are not supported.
- Network devices added after the OSPF application starts are not dynamically registered.

## Instruction
### Installing ns-3
Ensure you have ns-3 version 3.35 installed. You can follow this [installation guide](https://www.nsnam.org/docs/release/3.35/tutorial/singlehtml/index.html) to set up prerequisites.

Clone and checkout ns-3 version 3.35:
```
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev
git checkout -b ns-3.35-branch ns-3.35
```
### Adding the OSPF Module
Place this repository in ns-3's `/contrib` directory as `/contrib/ospf`:
```
cd contrib
git clone git@github.com:markverick/ns3-ospf.git ospf
```
### Running Examples
Run an example with ns-3 logging:
```
NS_LOG="OspfApp" ./waf --run=ospf-metric
```
Resulting PCAP files and routing tables can be found in `/results/<example>/`.

For debugging with gdb:
```
NS_LOG="OspfInterface:OspfNeighbor" ./waf --run=ospf-metric --gdb
```
### Reporting Issues
If you would like to report an issue, you can open a new issue in the GitLab issue tracker. Before creating a new issue, please check if the problem that you are facing was already reported and contribute to the discussion, if necessary.
