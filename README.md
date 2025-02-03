Simplified OSPF implementation on ns-3, focusing on scalability evaluation.

README is under construction

# Instruction
Put the folder into ns-3's contrib folder as "ospf". Use it as an Application.

Only tested on ns-3.35

Documentation is work-in-progress.

# What's currently working 
- Basic OSPF functionality for point-to-point routers
- Data synchronization and flooding procedures

# Examples
Run with ns-3 logging:
```
NS_LOG="OspfApp" ./waf --run=ospf-metric
```
PCAPs and routing tables can be found in `/results/<example>/`

Debug with gdb:
```
NS_LOG="OspfInterface:OspfNeighbor" ./waf --run=ospf-metric --gdb
```

# Major Simplifications
- LS Updates that are subject to flooding may only contain 1 LSA each.
   - LS Updates as implicit ACKs to LS Requests may contain LSAs up to the given MTU.
- LS Updates received in a short interval do not aggregate.

# Current Limitation
- No broadcast networks
- Net devices after OSFP app starts are not included for now.

# Planned
- Area routing
- Automatic registration of new net devices
- Writing tests for corner cases

# Not yet planned
3. Broadcast networks (Network-LSA)
