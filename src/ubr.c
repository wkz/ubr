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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include "vlan.h"
#include "cmdl.h"

int help_flag;

static void about(struct cmdl *cmdl)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS] COMMAND [ARGS] ...\n"
		"\n"
		"Options:\n"
		" -h, --help  Print help for last given command\n"
		" -j, --json  Read JSON from STDIN, or write to STDOUT\n"
		"\n"
		"Commands:\n"
		" fdb         Manage the forwarding (MAC) database\n"
		" port        Manage bridge ports\n"
		" vlan        Manage bridge VLANs\n",
		cmdl->argv[0]);
}

int main(int argc, char *argv[])
{
	int i;
	int res;
	struct cmdl cmdl;
	const struct cmd cmd = {"ubr", NULL, about};
	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	const struct cmd cmds[] = {
		{ "vlan",	cmd_vlan,	cmd_vlan_help},
		{ NULL }
	};

	do {
		int option_index = 0;

		i = getopt_long(argc, argv, "h", long_options, &option_index);

		switch (i) {
		case 'h':
			/*
			 * We want the help for the last command, so we flag
			 * here in order to print later.
			 */
			help_flag = 1;
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
