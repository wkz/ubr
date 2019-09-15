/*
 * msg.c	Messaging (netlink) helper functions.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Richard Alpe <richard.alpe@ericsson.com>
 *		Joachim Nilsson <troglobit@gmail.com>
 */

#include <err.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <net/if.h>

#include <linux/genetlink.h>
#include <libmnl/libmnl.h>

#include "ubr-netlink.h"
#include "private.h"
#include "msg.h"

static char   *buf = NULL;
static size_t  len = 0;


int parse_attrs(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	tb[type] = attr;

	return MNL_CB_OK;
}

static int family_id_cb(const struct nlmsghdr *nlh, void *data)
{
	struct genlmsghdr *genl = mnl_nlmsg_get_payload(nlh);
	struct nlattr *tb[CTRL_ATTR_MAX + 1] = {};
	int *id = data;

	mnl_attr_parse(nlh, sizeof(*genl), parse_attrs, tb);
	if (!tb[CTRL_ATTR_FAMILY_ID])
		return MNL_CB_ERROR;

	*id = mnl_attr_get_u16(tb[CTRL_ATTR_FAMILY_ID]);

	return MNL_CB_OK;
}

static struct mnl_socket *msg_send(struct nlmsghdr *nlh)
{
	struct mnl_socket *nl;
	int ret;

	nl = mnl_socket_open(NETLINK_GENERIC);
	if (nl == NULL) {
		perror("mnl_socket_open");
		return NULL;
	}

	ret = mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID);
	if (ret < 0) {
		perror("mnl_socket_bind");
		return NULL;
	}

	ret = mnl_socket_sendto(nl, nlh, nlh->nlmsg_len);
	if (ret < 0) {
		perror("mnl_socket_send");
		return NULL;
	}

	return nl;
}

static int msg_recv(struct mnl_socket *nl, mnl_cb_t callback, void *data, int seq)
{
	unsigned int portid;
	int ret;

	portid = mnl_socket_get_portid(nl);

	ret = mnl_socket_recvfrom(nl, buf, len);
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, callback, data);
		if (ret <= 0)
			break;
		ret = mnl_socket_recvfrom(nl, buf, len);
	}

	return ret;
}

static int msg_query(struct nlmsghdr *nlh, mnl_cb_t callback, void *data)
{
	unsigned int seq;
	struct mnl_socket *nl;
	int ret;

	seq = time(NULL);
	nlh->nlmsg_seq = seq;

	nl = msg_send(nlh);
	if (!nl)
		return -ENOTSUP;

	ret = msg_recv(nl, callback, data, seq);
	mnl_socket_close(nl);

	return ret;
}

static int get_family(void)
{
	struct genlmsghdr *genl;
	struct nlmsghdr *nlh;
	int nl_family;
	int err;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= GENL_ID_CTRL;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

	genl = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	genl->cmd = CTRL_CMD_GETFAMILY;
	genl->version = 1;

	mnl_attr_put_u16(nlh, CTRL_ATTR_FAMILY_ID, GENL_ID_CTRL);
	mnl_attr_put_strz(nlh, CTRL_ATTR_FAMILY_NAME, "ubr");

	err = msg_query(nlh, family_id_cb, &nl_family);
	if (err)
		return err;

	return nl_family;
}

int msg_doit(struct nlmsghdr *nlh, mnl_cb_t callback, void *data)
{
	int err;

	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	return msg_query(nlh, callback, data);
}

int msg_dumpit(struct nlmsghdr *nlh, mnl_cb_t callback, void *data)
{
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	return msg_query(nlh, callback, data);
}

static void msg_exit(void)
{
	free(buf);
}

struct nlmsghdr *msg_init(int cmd)
{
	struct genlmsghdr *genl;
	struct nlmsghdr *nlh;
	int family;

	if (!buf) {
		len = MNL_SOCKET_BUFFER_SIZE;
		buf = calloc(1, len);
		if (!buf)
			return NULL;

		atexit(msg_exit);
	}

	family = get_family();
	if (family <= 0) {
		warn("Unable to get ubr nl family id (module loaded?)");
		return NULL;
	}

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= family;

	genl = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	genl->cmd = cmd;
	genl->version = 1;

	mnl_attr_put_u32(nlh, UBR_NLA_IFINDEX, if_nametoindex(bridge));

	return nlh;
}
