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
#include <linux/rtnetlink.h>

#include <libmnl/libmnl.h>
#include <sys/socket.h>

#include "ubr-netlink.h"
#include "cmdl.h"
#include "port.h"
#include "private.h"

#define PORT_OPTS "[pvid none|VID]"

static char *ifname;
static int ifindex;


static void cmd_port_set_help(struct cmdl *cmdl)
{
	printf("Usage: %s port IFNAME set %s\n", cmdl->argv[0], PORT_OPTS);
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

	if (parse_opts(opts, cmdl) < 0 || !ifname) {
		if (help_flag)
			(cmd->help)(cmdl);
		return -EINVAL;
	}

	nlh = msg_init(UBR_NL_PORT_SET);
	if (!nlh) {
		warnx("error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_PORT);
	mnl_attr_put_u32(nlh, UBR_NLA_PORT_IFINDEX, ifindex);

	opt = get_opt(opts, "pvid");
	if (opt && -1 != (val = atoi(opt->val)))
		mnl_attr_put_u16(nlh, UBR_NLA_PORT_PVID, (uint16_t)val);

	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}

static void cmd_port_attach_help(struct cmdl *cmdl)
{
	printf("Usage: %s port IFNAME attach %s\n",
	       cmdl->argv[0], PORT_OPTS);
}

static int cmd_port_attach(struct nlmsghdr *nlh, const struct cmd *cmd,
			   struct cmdl *cmdl, void *data)
{
	struct ifinfomsg *ifm;
	struct nlattr *linkinfo;
	int err;

	if (!ifname) {
		if (help_flag)
			cmd->help(cmdl);
		return -EINVAL;
	}

	nlh = msg_init2(RTM_SETLINK, NLM_F_REQUEST | NLM_F_ACK);
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_index  = ifindex;

	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "ubr");
	mnl_attr_nest_end(nlh, linkinfo);

	mnl_attr_put_u32(nlh, IFLA_MASTER, brindex);

	err = msg_query2(nlh, NULL, NULL);
	if (err)
		return err;

	if (more_opts(cmdl))
		return cmd_port_set(nlh, cmd, cmdl, data);

	return 0;
}

static void cmd_port_detach_help(struct cmdl *cmdl)
{
	printf("Usage: %s port IFNAME detach\n", cmdl->argv[0]);
}

static int cmd_port_detach(struct nlmsghdr *nlh, const struct cmd *cmd,
			   struct cmdl *cmdl, void *data)
{
	struct ifinfomsg *ifm;
	struct nlattr *linkinfo;

	if (!ifname) {
		if (help_flag)
			cmd->help(cmdl);
		return -EINVAL;
	}

	nlh = msg_init2(RTM_SETLINK, NLM_F_REQUEST | NLM_F_ACK);
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_index  = ifindex;

	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "ubr");
	mnl_attr_nest_end(nlh, linkinfo);

	mnl_attr_put_u32(nlh, IFLA_MASTER, 0);

	return msg_query2(nlh, NULL, NULL);
}

void cmd_port_help(struct cmdl *cmdl)
{
	printf("Usage: %s port PORT-LIST COMMAND [OPTS] ...\n"
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
		{ "attach",	cmd_port_attach,	cmd_port_attach_help },
		{ "detach",	cmd_port_detach,	cmd_port_detach_help },
		{ "set",	cmd_port_set,		cmd_port_set_help },
		{ NULL }
	};

	if (help_flag)
		goto cont;

	/* Read port name, required argument */
	ifname = shift_cmdl(cmdl);
	if (!ifname) {
		cmd_port_help(cmdl);
		return -EINVAL;
	}

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		warn("%s is not a valid interface", ifname);
		return -EINVAL;
	}

cont:
	return run_cmd(nlh, cmd, cmds, cmdl, NULL);
}
