ubr CLI
=======

```
UBR := ubr [dev <BR>]

UBR add [max_ports N] ...
UBR del

UBR port <PORT> attach [index auto|N] *1
UBR port <PORT> detach
UBR port <PORT> set *1 [learning on|off] [vlan-filtering on|off] [pvid none|VID]

UBR vlan <VID> add *2
UBR vlan <VID> del
UBR vlan <VID> attach <PORT> [tagged] [pvid]
UBR vlan <VID> detach <PORT>
UBR vlan <VID> set *2 [fdb auto|N] [stp 0|N] [learning on|off]

UBR fdb <FID> dst <GROUP>|<LLADDR> add [<PORT-LIST>]
UBR fdb <FID> dst <GROUP>|<LLADDR> del [<PORT-LIST>]
UBR fdb <FID> dst <GROUP>|<LLADDR> attach <PORT-LIST>
UBR fdb <FID> dst <GROUP>|<LLADDR> detach <PORT-LIST>

UBR stp <SID> port <PORT-LIST> set <STP-STATE>
--
ubr dev ubr0 add max-ports 64
ubr port eth0 attach
ubr port eth1 attach

ubr vlan 1 add
ubr vlan 1 attach eth0

ubr dev virubr0 vlan 1 add
```



JSON equivalent:

[{
    seqnum = N,
    type = "dev",
    name = "ubr0",
    
    add = {
		max-ports = 64;
    };
},
 ...

]



-> { seqnum = N, status = 0 }, ....
