Simplified OSPF module for ns-3
===============================
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.18843774.svg)](https://doi.org/10.5281/zenodo.18843774)
[![CI](https://github.com/markverick/ns3-ospf/actions/workflows/main.yml/badge.svg)](https://github.com/markverick/ns3-ospf/actions/workflows/main.yml)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)

This project is an external module for the [ns-3 network simulator](https://www.nsnam.org/), implementing basic functionality of the Open Shortest Path First Version 2 (OSPFv2) routing protocol as defined in [RFC 2328](https://datatracker.ietf.org/doc/rfc2328/). This module is targeted for ns-3 version 3.35.

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
- Two-step forwarding lookup that separates prefix ownership from next-hop resolution
- Prefix injection, including explicit local gateway injection
- Per-interface control over whether directly attached interface prefixes are advertised
- Area leader policies: static, lowest-router-ID, and reachable-lowest with quorum
- Import/Export functionality of the OSPF state (LSA, neighbor, leadership)

### Forwarding Architecture
The forwarding plane is intentionally split into two stages instead of flattening every learned prefix directly into a single precomputed next-hop table.

1. Prefix lookup selects the best owner for the destination prefix.
   - Local interface-derived prefixes are owned per interface.
   - Locally injected prefixes with an explicit gateway are owned as gateway routes.
   - Remote intra-area prefixes are owned by the advertising router.
   - Remote inter-area prefixes are owned by the advertising area.
2. Owner resolution maps that owner to the current forwarding decision.
   - Interface owners resolve directly to an output interface.
   - Gateway-route owners resolve to the configured local interface and explicit gateway.
   - Router owners resolve through the current L1 shortest path state.
   - Area owners resolve through the current border-router and L2 shortest path state.

This design keeps prefix churn decoupled from SPF recomputation. Changes to advertised prefixes update the prefix-owner tables without forcing a shortest-path rerun unless the underlying topology or router/area reachability changes.

### Prefix Advertisement Model
The module distinguishes between two kinds of locally advertised reachability:

- Interface-derived reachability: prefixes collected from enabled Ipv4 interfaces.
- Injected reachability: additional prefixes supplied explicitly through the OSPF application API.

Injected prefixes can either be direct (`Ipv4Address::GetAny ()` or zero gateway) or explicit-gateway routes. Explicit-gateway injections remain distinct in the forwarding plane and are not collapsed into ordinary interface-owned routes.

By default, interface prefixes are not advertised just because an interface exists. They must either be enabled per interface or provided through the helper that configures reachable prefixes from interfaces.

### Area Leader Behavior
When Area Proxy is enabled, an area leader originates Area-LSAs and L2 Summary-LSAs for the area. The implementation supports three leader policies:

- `AreaLeaderMode=LowestRouterId`: the lowest known router ID leads.
- `AreaLeaderMode=Static`: the configured router ID leads.
- `AreaLeaderMode=ReachableLowestRouterId`: the lowest currently reachable router ID leads, but only while the router still has quorum.

Reachable-lowest mode is designed to step down during minority partitions. When a leader steps down, it explicitly originates empty Area and L2 Summary LSAs so inter-area withdrawals are propagated to peers instead of being removed only from local state.

### Major Simplifications
- Link State Updates subject to flooding contain only one Link State Advertisement (LSA) each.
   - Link State Updates serving as implicit acknowledgments to Link State Requests may contain multiple LSAs, up to the specified MTU.
- Link State Updates received within a short time window are not aggregated.
- LSA freshness is modeled primarily by sequence number; the module does not implement the full OSPF age/checksum tie-break machinery.
- Authentication is not implemented.
- Inter-AS routing is not implemented.

### Current Limitations
- Does not implement Designated Router (DR) and Backup Designated Router (BDR).
- Network devices added after the OSPF application starts are not dynamically registered unless `AutoSyncInterfaces` is enabled.
- Area leadership quorum is inferred from the router LSDB and currently reachable routers rather than a separately configured area membership database.

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

Recommended starting points:

- `ospf-two-step-gateway`: shows the two-step forwarding model when a router injects a prefix through an explicit local gateway. The routing dump includes both the remote router-owned view and the local `gw=` owner resolution.
- `ospf-area-leader`: shows inter-area summary origination with `AreaLeaderMode=ReachableLowestRouterId` and deterministic router IDs so the elected leaders are easy to inspect in the logs and dumps.
- `ospf-grid-n-prefix-update`: keeps exercising dynamic injected-prefix churn and is still the best stress example for repeated local prefix updates.

When routing-table printing is enabled, the dump includes both the traditional route view and the internal forwarding-plane state. The forwarding section lists:

- `owner resolution`: the current mapping from interface/router/area/gateway owners to output interface and next hop.
- `prefix owners`: the current owner candidates and metrics for each advertised prefix.

This is useful when debugging two-step lookup behavior, injected gateways, or inter-area proxy summaries.

For debugging with gdb:
```
NS_LOG="OspfInterface:OspfNeighbor" ./waf --run=ospf-metric --gdb
```
### Reporting Issues
If you would like to report an issue, you can open a new issue in the GitLab issue tracker. Before creating a new issue, please check if the problem that you are facing was already reported and contribute to the discussion, if necessary.
