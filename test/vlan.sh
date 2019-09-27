#!/bin/sh
# Verify VLAN separation
# - Three ports:
#   - p1 untagged VLAN 1
#   - p2 untagged VLAN 2
#   - p3 tagged VLAN 1,2 and PVID = 42
# - Inject VLAN 1 tagged traffic on p3
#   - Expect: traffic to be seen on p1 and not on p2
# - Inject VLAN 2 tagged traffic on p3
#   - Expect: traffic to be seen on p2 and not on p1
# - Inject untagged traffic on p3
#   - Expect: no traffic to be seen on p1 or p2
# - Inject untagged traffic on p1
#   - Expect VLAN 1 tagged traffic on p3 and no traffic on p2

