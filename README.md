Simplified OSPF implementation on ns-3, focusing on scalability evaluation.

# Instruction
Put the folder into ns-3's contrib folder as "ospf". Use it as an Application.

Only tested on ns-3.35

Documentation is work-in-progress.

# Notes
1. Skip Database Description (DBD) and LS Request implementation. All nodes start at clean slate (for now)
2. LSAs are simplified. Need to add header to support multiple types and variable-size.
3. Multi-access is not fully tested/implemented.
