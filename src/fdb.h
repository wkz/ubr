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

#ifndef UBR_FDB_H_
#define UBR_FDB_H_

#include "cmdl.h"
#include "msg.h"

int cmd_fdb(struct nlmsghdr *nlh, const struct cmd *cmd, struct cmdl *cmdl, void *data);
void cmd_fdb_help(struct cmdl *cmdl);

#endif /* UBR_FDB_H_ */
