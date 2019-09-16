/*
 * cmdl.h	Framework for handling command line options.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Richard Alpe <richard.alpe@ericsson.com>
 */

#ifndef _UBR_CMDL_H
#define _UBR_CMDL_H

#include <libmnl/libmnl.h>

extern int help_flag;

enum {
	OPT_KEY		= (1 << 0),
	OPT_KEYVAL		= (1 << 1),
};

struct cmdl {
	int optind;
	int argc;
	char **argv;
};

struct cmd {
	const char *cmd;
	int (*func)(struct nlmsghdr *nlh, const struct cmd *cmd,
		    struct cmdl *cmdl, void *data);
	void (*help)(struct cmdl *cmdl);
};

struct opt {
	const char *key;
	uint16_t flag;
	char *val;
};

struct opt *get_opt(struct opt *opts, char *key);
bool has_opt(struct opt *opts, char *key);
bool more_opts(struct cmdl *cmdl);
int parse_opts(struct opt *opts, struct cmdl *cmdl);
char *shift_cmdl(struct cmdl *cmdl);

int run_cmd(struct nlmsghdr *nlh, const struct cmd *caller,
	    const struct cmd *cmds, struct cmdl *cmdl, void *data);

const struct cmd *find_cmd(const struct cmd *cmds, char *str);

#endif
