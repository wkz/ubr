/*
 * msg.h	Messaging (netlink) helper functions.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Richard Alpe <richard.alpe@ericsson.com>
 *		Joachim Nilsson <troglobit@gmail.com>
 */

#ifndef UBR_MSG_H_
#define UBR_MSG_H_

#include <stdlib.h>

struct nlmsghdr *msg_init(int cmd);

int msg_doit    (struct nlmsghdr *nlh, mnl_cb_t callback, void *data);
int msg_dumpit  (struct nlmsghdr *nlh, mnl_cb_t callback, void *data);

int parse_attrs (const struct nlattr *attr, void *data);

#endif /* UBR_MSG_H_ */
