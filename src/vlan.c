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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

#include <linux/genetlink.h>

#include <libmnl/libmnl.h>
#include <sys/socket.h>

#include "ubr-genl.h"
#include "cmdl.h"
#include "vlan.h"

static uint16_t vid = -1;


static void cmd_vlan_add_help(struct cmdl *cmdl)
{
	fprintf(stderr, "Usage: %s vlan VID add [protocol VLAN-PROTO] [set VLAN-SETTINGS]\n",
		cmdl->argv[0]);
}

static int cmd_vlan_add(struct nlmsghdr *nlh, const struct cmd *cmd,
			  struct cmdl *cmdl, void *data)
{
	uint16_t vid;
	int err;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlattr *attrs;

	if (!(nlh = msg_init(buf, UBR_NL_VLAN_ADD))) {
		fprintf(stderr, "error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_VLAN);
	mnl_attr_put_u16(nlh, UBR_NLA_VLAN_VID, (uint16_t)vid);

	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}

void cmd_vlan_help(struct cmdl *cmdl)
{
	fprintf(stderr,
		"Usage: %s vlan COMMAND [ARGS] ...\n"
		"\n"
		"COMMANDS\n"
		" VID add		- Add VLAN to bridge\n"
		" VID del		- Remove VLAN from bridge\n"
		" VID attach        - Attach port(s) to VLAN\n"
		" VID detach        - Detach port(s) from VLAN\n"
		" VID set           - Set various VLAN properties\n",
		cmdl->argv[0]);
}

int cmd_vlan(struct nlmsghdr *nlh, const struct cmd *cmd, struct cmdl *cmdl,
	       void *data)
{
	const struct cmd cmds[] = {
		{ "add",	cmd_vlan_add,		cmd_vlan_add_help },
		{ NULL }
	};
	char *arg;

	arg = shift_cmdl(cmdl);
	vid = atoi(arg);
	if (vid < 1 || vid > 4095) {
		fprintf(stderr, "error, invalid VLAN %s\n", arg);
		return -EINVAL;
	}

	return run_cmd(nlh, cmd, cmds, cmdl, NULL);
}
