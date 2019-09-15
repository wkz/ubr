/*
 * ubr		Frontend for the unassuming bridge.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Richard Alpe <richard.alpe@ericsson.com>
 *		Joachim Nilsson <troglobit@gmail.com>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <linux/rtnetlink.h>

#include "private.h"
#include "cmdl.h"
#include "port.h"
#include "vlan.h"

char *bridge    = "ubr0";
int   help_flag = 0;

int atob(const char *str)
{
	struct {
		const char *str;
		size_t len;
		int val;
	} alt[] = {
		{ "off",   3, 0 },
		{ "on",    2, 1 },
		{ "false", 5, 0 },
		{ "true",  3, 1 },
	};

	if (!str) {
		errno = EINVAL;
		return -1;
	}

	for (size_t i = 0; i < NELEMS(alt); i++) {
		if (!strncasecmp(alt[i].str, str, alt[i].len))
			return alt[i].val;
	}

	return -1;
}

static void cmd_add_help(struct cmdl *cmdl)
{
	fprintf(stderr, "Usage: %s -i NAME add", cmdl->argv[0]);
}

static int cmd_add(struct nlmsghdr *nlh, const struct cmd *cmd,
		   struct cmdl *cmdl, void *data)
{
	struct ifinfomsg *ifm;
	struct nlattr *linkinfo;

	nlh = msg_init2(RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL);
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;

	mnl_attr_put_str(nlh, IFLA_IFNAME, bridge);
	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "ubr");
	mnl_attr_nest_end(nlh, linkinfo);

	return msg_query2(nlh, NULL, NULL);
}

static void cmd_del_help(struct cmdl *cmdl)
{
	fprintf(stderr, "Usage: %s -i NAME del", cmdl->argv[0]);
}

static int cmd_del(struct nlmsghdr *nlh, const struct cmd *cmd,
		   struct cmdl *cmdl, void *data)
{
	struct ifinfomsg *ifm;
	struct nlattr *linkinfo;

	nlh = msg_init2(RTM_DELLINK, NLM_F_REQUEST | NLM_F_ACK);
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;

	mnl_attr_put_str(nlh, IFLA_IFNAME, bridge);
	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "ubr");
	mnl_attr_nest_end(nlh, linkinfo);

	return msg_query2(nlh, NULL, NULL);
}

static void about(struct cmdl *cmdl)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS] COMMAND [ARGS] ...\n"
		"\n"
		"Options:\n"
		" -i, --iface BRIDGE  Bridge interface to manage, default ubr0\n"
		" -h, --help          Show help for last given command\n"
		" -j, --json          Read JSON from STDIN, or write to STDOUT\n"
		"\n"
		"Commands:\n"
		" add                 Create a new bridge\n"
		" del                 Delete a bridge\n"
		" fdb                 Manage forwarding (MAC) database\n"
		" port PORT           Manage bridge ports\n"
		" vlan VID            Manage bridge VLANs\n",
		cmdl->argv[0]);
}

int main(int argc, char *argv[])
{
	int i;
	int res;
	struct cmdl cmdl;
	const struct cmd cmd = {"ubr", NULL, about};
	struct option long_options[] = {
		{ "help",  no_argument,       0, 'h' },
		{ "iface", required_argument, 0, 'i' },
		{ 0, 0, 0, 0 }
	};
	const struct cmd cmds[] = {
		{ "add",        cmd_add,        cmd_add_help  },
		{ "del",        cmd_del,        cmd_del_help  },
		{ "port",       cmd_port,       cmd_port_help },
		{ "vlan",       cmd_vlan,       cmd_vlan_help },
		{ NULL }
	};

	do {
		int option_index = 0;

		i = getopt_long(argc, argv, "hi:", long_options, &option_index);
		switch (i) {
		case 'h':
			/*
			 * We want the help for the last command, so we flag
			 * here in order to print later.
			 */
			help_flag = 1;
			break;

		case 'i':
			bridge = optarg;
			break;

		case -1:
			/* End of options */
			break;
		default:
			/* Invalid option, error msg is printed by getopts */
			return 1;
		}
	} while (i != -1);

	cmdl.optind = optind;
	cmdl.argc = argc;
	cmdl.argv = argv;

	if ((res = run_cmd(NULL, &cmd, cmds, &cmdl, NULL)) != 0)
		return 1;

	return 0;
}
