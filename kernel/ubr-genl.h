#ifndef UBR_GENL_H_
#define UBR_GENL_H_

/* Netlink commands */
enum {
	UBR_NL_UNSPEC,
	UBR_NL_VLAN_ADD,

	__UBR_NL_CMD_MAX,
	UBR_NL_CMD_MAX = __UBR_NL_CMD_MAX - 1
};

enum {
	UBR_NLA_UNSPEC,
	UBR_NLA_VLAN,

	__UBR_NLA_MAX,
	UBR_NLA_MAX = __UBR_NLA_MAX - 1
};

enum {
	UBR_NLA_VLAN_UNSPEC,
	UBR_NLA_VLAN_VID,

	__UBR_NLA_VLAN_MAX,
	UBR_NLA_VLAN_MAX = __UBR_NLA_VLAN_MAX - 1
};

#endif /* UBR_GENL_H_ */
