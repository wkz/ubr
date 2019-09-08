/*
 * vlan.c	ubr VLAN functionality.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Richard Alpe <richard.alpe@ericsson.com>
 *		Joachim Nilsson <troglobit@gmail.com>
 */

#ifndef _UBR_VLAN_H
#define _UBR_VLAN_H

#include "cmdl.h"
#include "msg.h"

/* Netlink commands */
enum {
	UBR_NL_UNSPEC,
	UBR_NL_VLAN_ADD,

	__UBR_NL_CMD_MAX,
	UBR_NL_CMD_MAX = __UBR_NL_CMD_MAX - 1
};

/*  */
enum {
	UBR_NLA_VLAN,

	__UBR_NLA_MAX,
	UBR_NLA_MAX = __UBR_NLA_MAX - 1
};

enum {
	UBR_NLA_VLAN_VID,

	__UBR_NLA_VLAN_MAX,
	UBR_NLA_VLAN_MAX = __UBR_NLA_VLAN_MAX - 1
};

extern int help_flag;

int cmd_vlan(struct nlmsghdr *nlh, const struct cmd *cmd, struct cmdl *cmdl, void *data);
void cmd_vlan_help(struct cmdl *cmdl);

#endif
