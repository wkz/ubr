#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/timer.h>
#include <linux/export.h>
#include <net/genetlink.h>

#include "ubr-netlink.h"

static const struct nla_policy ubr_nl_policy[UBR_NLA_MAX + 1] = {
	[UBR_NLA_UNSPEC]	= { .type = NLA_UNSPEC, },
	[UBR_NLA_VLAN]		= { .type = NLA_NESTED, },
};

const struct nla_policy ubr_nl_vlan_policy[UBR_NLA_VLAN_MAX + 1] = {
	[UBR_NLA_VLAN_UNSPEC]	= { .type = NLA_UNSPEC },
	[UBR_NLA_VLAN_VID]	= { .type = NLA_U16    },
};


static int ubr_vlan_add_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	u16 vid;
	int err;

	if (!info->attrs || !info->attrs[UBR_NLA_VLAN])
		return -EINVAL;

	err = nla_parse_nested(attrs, UBR_NLA_VLAN_MAX,
			       info->attrs[UBR_NLA_VLAN],
			       ubr_nl_vlan_policy, info->extack);
	if (err)
		return err;

	if (!attrs[UBR_NLA_VLAN_VID])
		return -EINVAL;

	vid = nla_get_u16(attrs[UBR_NLA_VLAN_VID]);
	printk(KERN_NOTICE "Add VLAN %u, hello\n", vid);

	return 0;
}

static const struct genl_ops ubr_genl_ops[] = { {
		.cmd    = UBR_NL_VLAN_ADD,
		.doit   = ubr_vlan_add_cmd,
		.dumpit = NULL,
	},
};

static struct genl_family family = {
	.name     = "ubr",
	.version  = 1,
	.maxattr  = 2,		/* XXX */
	.netnsok  = false,
	.module   = THIS_MODULE,
	.ops      = ubr_genl_ops,
	.n_ops    = ARRAY_SIZE(ubr_genl_ops),
};

int ubr_nlmsg_parse(const struct nlmsghdr *nlh, struct nlattr ***attr)
{
	u32 maxattr = family.maxattr;

	*attr = genl_family_attrbuf(&family);
	if (!*attr)
		return -EOPNOTSUPP;

	return nlmsg_parse(nlh, GENL_HDRLEN, *attr, maxattr, ubr_nl_policy, NULL);
}

int ubr_netlink_init(void)
{
	int err;

	err = genl_register_family(&family);
	if (err)
		goto err;

	return 0;
err:
	return err;
}

int ubr_netlink_exit(void)
{
	return genl_unregister_family(&family);
}

