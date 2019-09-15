#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/timer.h>
#include <linux/export.h>

#include "ubr-netlink.h"
#include "ubr-private.h"

static const struct nla_policy ubr_nl_policy[UBR_NLA_MAX + 1] = {
	[UBR_NLA_UNSPEC]	= { .type = NLA_UNSPEC, },
	[UBR_NLA_VLAN]		= { .type = NLA_NESTED, },
};

static const struct genl_ops ubr_genl_ops[] = { {
		.cmd    = UBR_NL_VLAN_ADD,
		.doit   = ubr_vlan_nl_add_cmd,
		.dumpit = NULL,
	}, {
		.cmd    = UBR_NL_VLAN_DEL,
		.doit   = ubr_vlan_nl_del_cmd,
		.dumpit = NULL,
	}, {
		.cmd    = UBR_NL_VLAN_SET,
		.doit   = ubr_vlan_nl_set_cmd,
		.dumpit = NULL,
	}, {
		.cmd    = UBR_NL_VLAN_ATTACH,
		.doit   = ubr_vlan_nl_attach_cmd,
		.dumpit = NULL,
	}, {
		.cmd    = UBR_NL_VLAN_DETACH,
		.doit   = ubr_vlan_nl_detach_cmd,
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

