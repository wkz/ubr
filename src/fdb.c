/*
 * fdb.c	ubr FDB functionality.
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
#include "fdb.h"
#include "private.h"

static void cmd_fdb_flush_help(struct cmdl *cmdl)
{
	fprintf(stderr, "Usage: %s fdb flush\n", cmdl->argv[0]);
}

static int cmd_fdb_flush(struct nlmsghdr *nlh, const struct cmd *cmd,
			 struct cmdl *cmdl, void *data)
{
	struct nlattr *attrs;
	int val;

	nlh = msg_init(UBR_NL_FDB_FLUSH);
	if (!nlh) {
		fprintf(stderr, "error, message initialisation failed\n");
		return -1;
	}

	return msg_doit(nlh, NULL, NULL);
}

void cmd_fdb_help(struct cmdl *cmdl)
{
	fprintf(stderr,
		"Usage: %s fdb COMMAND [OPTS] ...\n"
		"\n"
		"COMMANDS\n"
		" flush       Set flush forwarding database\n",
		cmdl->argv[0]);
}

int cmd_fdb(struct nlmsghdr *nlh, const struct cmd *cmd, struct cmdl *cmdl,
	       void *data)
{
	const struct cmd cmds[] = {
		{ "flush",	cmd_fdb_flush,		cmd_fdb_flush_help },
		{ NULL }
	};

	return run_cmd(nlh, cmd, cmds, cmdl, NULL);
}
