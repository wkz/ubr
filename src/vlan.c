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
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <linux/genetlink.h>

#include <libmnl/libmnl.h>
#include <sys/socket.h>

#include "ubr-netlink.h"
#include "cmdl.h"
#include "vlan.h"
#include "private.h"

#define VLAN_OPTS "[learning on|off] "


static uint16_t vid = 0;


static void cmd_vlan_add_help(struct cmdl *cmdl)
{
	printf("Usage: %s vlan VID add [protocol VLAN-PROTO] "
	       "[set %s]\n", cmdl->argv[0], VLAN_OPTS);
}

static int cmd_vlan_add(struct nlmsghdr *nlh, const struct cmd *cmd,
			  struct cmdl *cmdl, void *data)
{
	struct nlattr *attrs;
	int err;

	if (!vid) {
		if (help_flag)
			cmd->help(cmdl);
		return -EINVAL;
	}

	nlh = msg_init(UBR_NL_VLAN_ADD);
	if (!nlh) {
		warnx("error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_VLAN);
	mnl_attr_put_u16(nlh, UBR_NLA_VLAN_VID, (uint16_t)vid);
	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}

static void cmd_vlan_del_help(struct cmdl *cmdl)
{
	printf("Usage: %s vlan VID del\n", cmdl->argv[0]);
}

static int cmd_vlan_del(struct nlmsghdr *nlh, const struct cmd *cmd,
			  struct cmdl *cmdl, void *data)
{
	struct nlattr *attrs;
	int err;

	if (!vid) {
		if (help_flag)
			cmd->help(cmdl);
		return -EINVAL;
	}

	nlh = msg_init(UBR_NL_VLAN_DEL);
	if (!nlh) {
		warnx("error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_VLAN);
	mnl_attr_put_u16(nlh, UBR_NLA_VLAN_VID, (uint16_t)vid);
	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}

static void cmd_vlan_attach_help(struct cmdl *cmdl)
{
	printf("Usage: %s vlan VID attach PORT-LIST [tagged]\n",
	       cmdl->argv[0]);
}

static int cmd_vlan_attach(struct nlmsghdr *nlh, const struct cmd *cmd,
			   struct cmdl *cmdl, void *data)
{
	struct nlattr *attrs;
	struct opt opts[] = {
		{ "tagged",		OPT_KEY,	NULL },
		{ NULL }
	};
	struct opt *opt;
	char *ifname;
	int ifindex;
	int tagged = 0;
	int err;

	if (!vid) {
	err:
		if (help_flag)
			(cmd->help)(cmdl);
		return -EINVAL;
	}

	/* Read port name(s), required argument */
	ifname = shift_cmdl(cmdl);
	if (!ifname) {
		cmd->help(cmdl);
		return -EINVAL;
	}

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		warn("%s is not a valid interface", ifname);
		return -EINVAL;
	}

	if (parse_opts(opts, cmdl) < 0)
		goto err;

	nlh = msg_init(UBR_NL_VLAN_ATTACH);
	if (!nlh) {
		warnx("error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_VLAN);
	mnl_attr_put_u16(nlh, UBR_NLA_VLAN_VID, (uint16_t)vid);
	tagged = has_opt(opts, "tagged");
	printf("Adding port %s %stagged to bridge %s\n", ifname, tagged ? "" : "not ", bridge);
	mnl_attr_put_u32(nlh, UBR_NLA_VLAN_TAGGED, tagged);
	mnl_attr_put_u32(nlh, UBR_NLA_VLAN_PORT, ifindex);
	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}


static void cmd_vlan_detach_help(struct cmdl *cmdl)
{
	printf("Usage: %s vlan VID detach PORT-LIST [tagged]",
	       cmdl->argv[0]);
}

static int cmd_vlan_detach(struct nlmsghdr *nlh, const struct cmd *cmd,
			   struct cmdl *cmdl, void *data)
{
	struct nlattr *attrs;
	char *ifname;
	int ifindex;
	int err;

	if (!vid) {
		if (help_flag)
			cmd->help(cmdl);
		return -EINVAL;
	}

	/* Read port name(s), required argument */
	ifname = shift_cmdl(cmdl);
	if (!ifname) {
		cmd->help(cmdl);
		return -EINVAL;
	}

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		warn("%s is not a valid interface", ifname);
		return -EINVAL;
	}

	nlh = msg_init(UBR_NL_VLAN_DETACH);
	if (!nlh) {
		warnx("error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_VLAN);
	mnl_attr_put_u16(nlh, UBR_NLA_VLAN_VID, (uint16_t)vid);
	mnl_attr_put_u32(nlh, UBR_NLA_VLAN_PORT, ifindex);
	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}

static void cmd_vlan_set_help(struct cmdl *cmdl)
{
	printf("Usage: %s vlan VID set %s\n", cmdl->argv[0], VLAN_OPTS);
}

static int cmd_vlan_set(struct nlmsghdr *nlh, const struct cmd *cmd,
			struct cmdl *cmdl, void *data)
{
	struct nlattr *attrs;
	struct opt opts[] = {
		{ "learning",		OPT_KEYVAL,	NULL },
		{ NULL }
	};
	struct opt *opt;
	int val;

	if (!vid || parse_opts(opts, cmdl) < 0) {
		if (help_flag)
			(cmd->help)(cmdl);
		return -EINVAL;
	}

	nlh = msg_init(UBR_NL_VLAN_SET);
	if (!nlh) {
		warnx("error, message initialisation failed\n");
		return -1;
	}

	attrs = mnl_attr_nest_start(nlh, UBR_NLA_VLAN);
	mnl_attr_put_u16(nlh, UBR_NLA_VLAN_VID, (uint16_t)vid);

	opt = get_opt(opts, "learning");
	if (opt && -1 != (val = atob(opt->val)))
		mnl_attr_put_u32(nlh, UBR_NLA_VLAN_LEARNING, val);

	mnl_attr_nest_end(nlh, attrs);

	return msg_doit(nlh, NULL, NULL);
}

void cmd_vlan_help(struct cmdl *cmdl)
{
	printf("Usage: %s vlan VID COMMAND [OPTS] ...\n"
	       "\n"
	       "COMMANDS\n"
	       " add         Add VLAN to bridge\n"
	       " del         Remove VLAN from bridge\n"
	       " attach      Attach port(s) to VLAN\n"
	       " detach      Detach port(s) from VLAN\n"
	       " set         Set various VLAN properties\n",
	       cmdl->argv[0]);
}

int cmd_vlan(struct nlmsghdr *nlh, const struct cmd *cmd, struct cmdl *cmdl,
	       void *data)
{
	const struct cmd cmds[] = {
		{ "add",	cmd_vlan_add,		cmd_vlan_add_help },
		{ "del",	cmd_vlan_del,		cmd_vlan_del_help },
		{ "attach",	cmd_vlan_attach,	cmd_vlan_attach_help },
		{ "detach",	cmd_vlan_detach,	cmd_vlan_detach_help },
		{ "set",	cmd_vlan_set,		cmd_vlan_set_help },
		{ NULL }
	};
	char *arg;
	int val;

	if (help_flag)
		goto cont;

	/* Read VLAN id, required argument */
	arg = shift_cmdl(cmdl);
	if (!arg) {
		cmd_vlan_help(cmdl);
		return -EINVAL;
	}

	val = atoi(arg);
	if (val < 0 || val > UINT16_MAX)
		val = 0;

	vid = (uint16_t)val;
	if (vid < 1 || vid > 4095) {
		warnx("error, invalid VLAN %s\n", arg);
		return -EINVAL;
	}

cont:
	return run_cmd(nlh, cmd, cmds, cmdl, NULL);
}
