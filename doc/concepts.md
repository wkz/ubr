ubr
===

CLI
---

The base use-case is of course in an interactive setting where the
user configures the desired ports, vlans etc. Output shall thus be
optimized for human consumption, i.e. `top` over `ip link`. If the
output is to be parsed by another program or script, ubr should
support JSON output which can then be processed with `jq` or similar.

In addition, we should support a batch mode in which ubr would read
commands in JSON form from stdin, maybe as JSON-RPC, and write result
objects to stdout.

### Grammar

```
# Allow dev to be inferred where possible. Either because the command is
# operating on a port which is attached to an ubr, or if there is only
# one ubr on the box.
UBR := ubr [-j|--json] [dev BR]

# Whitespace is the only feasible delimitor since [,;:] are all allowed
# in interface names. The only exception is /, but that feels very
# idiosyncratic. So lists have to be quoted unfortunately.
PORT-LIST := PORT [PORT [...]]

UBR add [max_ports N] ...
UBR del

UBR port PORT-LIST attach [index auto|N] PORT-SETTINGS
UBR port PORT-LIST detach
UBR port PORT-LIST set PORT-SETTINGS

PORT-SETTINGS :=
	pvid none|VID
	filter-vlans on|off
	filter-source on|off
	learning on|off
	flood-unicast on|off
	flood-multicast on|off
	flood-broadcast on|off

UBR vlan VID add [protocol N] VLAN-SETTINGS
UBR vlan VID del
UBR vlan VID attach PORT-LIST [tagged]
UBR vlan VID detach PORT-LIST
UBR vlan VID set VLAN-SETTINGS

VLAN-SETTINGS :=
	[learning on|off]
	[fdb auto|N]
	[stg N]

UBR fdb FID dst GROUP|LLADDR add [PORT-LIST] [protocol N]
UBR fdb FID dst GROUP|LLADDR del [PORT-LIST]
UBR fdb FID dst GROUP|LLADDR attach PORT-LIST
UBR fdb FID dst GROUP|LLADDR detach PORT-LIST

# Spanning Tree Group seems to be the most used term.
UBR stg <SID> port <PORT-LIST> set <STP-STATE>

STP-STATE := blocking|listening|learning|forwarding
```

### Example

```
ubr dev ubr0 add
ubr port "eth0 eth1" attach

ubr vlan 1 add
ubr vlan 2 add

ubr vlan 1 attach ubr0 tagged
ubr vlan 1 attach eth0 pvid

ubr vlan 2 attach ubr0 tagged
ubr vlan 2 attach eth1 pvid
```
