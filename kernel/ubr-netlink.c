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
	[UBR_NLA_IFINDEX]	= { .type = NLA_U32,    },
	[UBR_NLA_VLAN]		= { .type = NLA_NESTED, },
	[UBR_NLA_PORT]		= { .type = NLA_NESTED, },
};

static const struct genl_ops ubr_genl_ops[] = { {
		.cmd    = UBR_NL_VLAN_ADD,
		.doit   = ubr_vlan_nl_add_cmd,
		.policy = ubr_nl_policy,
	}, {
		.cmd    = UBR_NL_VLAN_DEL,
		.doit   = ubr_vlan_nl_del_cmd,
		.policy = ubr_nl_policy,
	}, {
		.cmd    = UBR_NL_VLAN_SET,
		.doit   = ubr_vlan_nl_set_cmd,
		.policy = ubr_nl_policy,
	}, {
		.cmd    = UBR_NL_VLAN_ATTACH,
		.doit   = ubr_vlan_nl_attach_cmd,
		.policy = ubr_nl_policy,
	}, {
		.cmd    = UBR_NL_VLAN_DETACH,
		.doit   = ubr_vlan_nl_detach_cmd,
		.policy = ubr_nl_policy,
	}, {
		.cmd    = UBR_NL_PORT_SET,
		.doit   = ubr_port_nl_set_cmd,
		.policy = ubr_nl_policy,
	},
};

static struct genl_family family = {
	.name     = "ubr",
	.version  = 1,
	.maxattr  = 10,		/* XXX */
	.netnsok  = false,
	.module   = THIS_MODULE,
	.ops      = ubr_genl_ops,
	.n_ops    = ARRAY_SIZE(ubr_genl_ops),
};

static const struct net_device_ops *devops = NULL;


int ubr_nlmsg_parse(const struct nlmsghdr *nlh, struct nlattr ***attr)
{
	u32 maxattr = family.maxattr;

	*attr = genl_family_attrbuf(&family);
	if (!*attr)
		return -EOPNOTSUPP;

	return nlmsg_parse(nlh, GENL_HDRLEN, *attr, maxattr, ubr_nl_policy, NULL);
}

struct net_device *ubr_netlink_dev(struct genl_info *info)
{
	struct net_device *dev;
	int ifindex;

	if (!info->attrs[UBR_NLA_IFINDEX])
		return NULL;

	ifindex = nla_get_u32(info->attrs[UBR_NLA_IFINDEX]);
	printk(KERN_NOTICE "Got UBR ifindex %d\n", ifindex);
	dev = dev_get_by_index(genl_info_net(info), ifindex);
	if (!dev || dev->netdev_ops != devops) {
		if (dev)
			dev_put(dev);
		return NULL;
	}

	return dev;
}

int ubr_netlink_init(const struct net_device_ops *ops)
{
	int err;

	err = genl_register_family(&family);
	if (err)
		goto err;

	/* Used to match against bridge interface from userspace */
	devops = ops;

	return 0;
err:
	return err;
}

int ubr_netlink_exit(void)
{
	return genl_unregister_family(&family);
}

