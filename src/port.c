/*
 * port.c	ubr port functionality.
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
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <linux/genetlink.h>

#include <libmnl/libmnl.h>
#include <sys/socket.h>

#include "ubr-netlink.h"
#include "cmdl.h"
#include "port.h"
#include "private.h"

#define PORT_OPTS "[pvid none|VID]"

static char *ifname;

static void cmd_port_set_help(struct cmdl *cmdl)
{
	fprintf(stderr, "Usage: %s port IFNAME set %s\n", cmdl->argv[0], PORT_OPTS);
}

static int cmd_port_set(struct nlmsghdr *nlh, const struct cmd *cmd,
			struct cmdl *cmdl, void *data)
{
	struct nlattr *attrs;
	struct opt opts[] = {
		{ "pvid",	OPT_KEYVAL,	NULL },
		{ NULL }
	};
	struct opt *opt;
	int val;

	if (parse_opts(opts, cmdl) < 0) {
		if (help_flag)
			(cmd->help)(cmdl);
		return -EINVAL;
	}

	nlh = msg_init(UBR_NL_PORT_SET);
	if (!nlh) {
		fprintf(stderr, "error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_PORT);
	mnl_attr_put_u32(nlh, UBR_NLA_PORT_IFINDEX, if_nametoindex(ifname));

	opt = get_opt(opts, "pvid");
	if (opt && -1 != (val = atoi(opt->val)))
		mnl_attr_put_u16(nlh, UBR_NLA_PORT_PVID, (uint16_t)val);

	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}

void cmd_port_help(struct cmdl *cmdl)
{
	fprintf(stderr,
		"Usage: %s port PORT-LIST COMMAND [OPTS] ...\n"
		"\n"
		"COMMANDS\n"
		" attach      Attach port(s) to bridge\n"
		" detach      Detach port(s) from bridge\n"
		" set         Set various port properties\n",
		cmdl->argv[0]);
}

int cmd_port(struct nlmsghdr *nlh, const struct cmd *cmd, struct cmdl *cmdl,
	       void *data)
{
	const struct cmd cmds[] = {
//		{ "attach",	cmd_port_attach,	cmd_port_attach_help },
//		{ "detach",	cmd_port_detach,	cmd_port_detach_help },
		{ "set",	cmd_port_set,		cmd_port_set_help },
		{ NULL }
	};

	/* Read port name, required argument */
	ifname = shift_cmdl(cmdl);
	if (!ifname) {
		cmd_port_help(cmdl);
		return -EINVAL;
	}

	return run_cmd(nlh, cmd, cmds, cmdl, NULL);
}