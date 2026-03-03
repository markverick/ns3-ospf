Simplified OSPF module for ns-3
===============================
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.18843774.svg)](https://doi.org/10.5281/zenodo.18843774)
[![CI](https://github.com/markverick/ns3-ospf/actions/workflows/main.yml/badge.svg)](https://github.com/markverick/ns3-ospf/actions/workflows/main.yml)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)

This project is an external module for the [ns-3 network simulator](https://www.nsnam.org/), implementing basic functionality of the Open Shortest Path First Version 2 (OSPFv2) routing protocol as defined in [RFC 2328](https://datatracker.ietf.org/doc/rfc2328/). This module is targetted for ns-3 version 3.35.

## License

This software is licensed under the terms of the GNU General Public License v2.0 only (GPL-2.0-only).
See the LICENSE file for more details.

## Table of Contents:

1) [Overview](#overview)
2) [Instruction](#instruction)
3) [Continuous Integration](#continuous-integration)
4) [Citation](#citation)
5) [Reporting Issues](#reporting-issues)


## Overview
### What's currently implemented 
- Basic OSPF functionality for point-to-point routers and multi-access networks
- Data synchronization and flooding procedures
- Area Proxy implementation (RFC 9666) for OSPF
- LSA separation between router reachability and prefix advertisement (Similar to OSPFv3)
- Prefix Injection
- Import/Export functionality of the OSPF state (LSA, neighbor, leadership)

### Major Simplifications
- Link State Updates subject to flooding contain only one Link State Advertisement (LSA) each.
   - Link State Updates serving as implicit acknowledgments to Link State Requests may contain multiple LSAs, up to the specified MTU.
- Link State Updates received within a short time window are not aggregated.
- Authentication is not implemented.
- Inter-AS routing is not implemented.

### Current Limitations
- Does not implement Designated Router (DR) and Backup Designated Router (BDR).
- Network devices added after the OSPF application starts are not dynamically registered unless `AutoSyncInterfaces` is enabled.

## Continuous Integration

This repository includes a GitHub Actions workflow that builds ns-3 and runs all
OSPF test suites (`ospf-*`):

- Workflow: `.github/workflows/main.yml`

To run the same OSPF test suites locally from an ns-3 checkout (after placing
this repository at `ns-3-dev/contrib/ospf`):

```sh
./waf configure --enable-tests --enable-asserts -d debug
./waf

./test.py --nowaf --list > test-suites.txt
ospf_suites=$(awk 'NR>2 {print $2}' test-suites.txt | grep -E '^ospf-' | sort -u)
suite_args=""
for s in $ospf_suites; do suite_args="$suite_args -s $s"; done

# shellcheck disable=SC2086
./test.py --nowaf $suite_args -v
```

## Citation

If you use this software in research, please cite the Zenodo record:

Theeranantachai, S. (2026). *ns3-ospf* (v1.0.0) [Computer software]. Zenodo. https://doi.org/10.5281/zenodo.18843774

BibTeX:

```bibtex
@software{theeranantachai_2026_ns3_ospf,
   author    = {Theeranantachai, Sirapop},
   title     = {ns3-ospf},
   version   = {v1.0.0},
   publisher = {Zenodo},
   year      = {2026},
   doi       = {10.5281/zenodo.18843774},
   url       = {https://doi.org/10.5281/zenodo.18843774}
}
```

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
