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

extern int help_flag;

int cmd_vlan(struct nlmsghdr *nlh, const struct cmd *cmd, struct cmdl *cmdl, void *data);
void cmd_vlan_help(struct cmdl *cmdl);

#endif
