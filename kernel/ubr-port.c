#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>

#include <net/dsa.h>
#include <net/rtnetlink.h>

#include "ubr-netlink.h"
#include "ubr-private.h"

const struct nla_policy ubr_nl_port_policy[UBR_NLA_PORT_MAX + 1] = {
	[UBR_NLA_PORT_UNSPEC]   = { .type = NLA_UNSPEC },
	[UBR_NLA_PORT_IFINDEX]  = { .type = NLA_U32    },
	[UBR_NLA_PORT_PVID]     = { .type = NLA_U16    },
};

static rx_handler_result_t ubr_port_rx_handler(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct ubr_port *p = ubr_port_get_rcu(skb->dev);
	struct ubr_cb *cb = ubr_cb(skb);

	memcpy(cb, &p->ingress_cb, sizeof(*cb));
	ubr_forward(ubr_from_port(p), skb);

	return RX_HANDLER_CONSUMED;
}

static void __ubr_port_cleanup(struct rcu_head *head)
{
	struct ubr_port *p = container_of(head, struct ubr_port, rcu);

	memset(p, 0, sizeof(*p));
}

void ubr_port_cleanup(struct ubr_port *p)
{
	struct ubr *ubr = ubr_from_port(p);
	unsigned pidx = p->ingress_cb.pidx;

	printk(KERN_NOTICE "Clearing pidx %d from bridge %s\n", pidx, ubr->dev->name);
	ubr_vec_clear(&ubr->busy, pidx);
	call_rcu(&p->rcu, __ubr_port_cleanup);
}

struct ubr_port *ubr_port_init(struct ubr *ubr, unsigned pidx, struct net_device *dev)
{
	struct ubr_port *p = &ubr->ports[pidx];
	struct ubr_cb *cb = &p->ingress_cb;

	p->dev = dev;
	cb->pidx = pidx;

	/* Disable all ingress filtering. */
	cb->vlan_ok = 1;
	cb->stp_ok = 1;
	cb->sa_ok = 1;

	cb->sa_learning = 1;

	/* Allow egress on all ports execpt this one. */
	ubr_vec_fill(&cb->vec);
	ubr_vec_clear(&cb->vec, pidx);

	/* Put all ports in VLAN 0. */
	cb->vlan = ubr_vlan_find(ubr, 0);
	ubr_vlan_port_add(cb->vlan, pidx, 0);

	/* err = ubr_switchdev_port_init(p); */
	/* if (err) */
	/* 	return err; */

	smp_wmb();
	ubr_vec_set(&ubr->busy, pidx);

	return p;
}

static int __ubr_port_add_allowed(struct net_device *dev,
				  struct netlink_ext_ack *extack)
{
	if (dev->flags & IFF_LOOPBACK) {
		NL_SET_ERR_MSG(extack, "Interface is loopback");
		return -EINVAL;
	}

	if (dev->type != ARPHRD_ETHER ||
	    dev->addr_len != ETH_ALEN ||
	    !is_valid_ether_addr(dev->dev_addr)) {
		NL_SET_ERR_MSG(extack, "Non-Ethernet interface");
		return -EINVAL;
	}

	if (netdev_uses_dsa(dev)) {
		NL_SET_ERR_MSG(extack, "DSA master interface");
		return -EBUSY;
	}

	if (netdev_master_upper_dev_get(dev)) {
		NL_SET_ERR_MSG(extack, "Interface already has a master");
		return -EBUSY;
	}

	if (dev->priv_flags & IFF_DONT_BRIDGE) {
		NL_SET_ERR_MSG(extack, "Interface does not allow bridging");
		return -EOPNOTSUPP;
	}

	return 0;
}

int ubr_port_add(struct ubr *ubr, struct net_device *dev,
		 struct netlink_ext_ack *extack)
{
	struct ubr_port *p;
	int err, pidx;

	printk(KERN_NOTICE "Adding port %s to bridge %s ...\n", dev->name, ubr->dev->name);
	err = __ubr_port_add_allowed(dev, extack);
	if (err)
		return err;

	pidx = find_first_zero_bit(ubr->busy.bitmap, UBR_MAX_PORTS);
	if (pidx == UBR_MAX_PORTS) {
		NL_SET_ERR_MSG(extack, "Maximum number of ports reached");
		return -EBUSY;
	}

	p = ubr_port_init(ubr, pidx, dev);
	if (IS_ERR(p))
		return PTR_ERR(p);

	ubr_update_headroom(ubr, dev);
	netdev_update_features(ubr->dev);
	call_netdevice_notifiers(NETDEV_JOIN, dev);

	err = netdev_master_upper_dev_link(dev, ubr->dev, NULL, NULL, extack);
	if (err)
		goto err_uninit;

	err = dev_set_promiscuity(dev, 1);
	if (err)
		goto err_unlink;

	err = dev_set_allmulti(dev, 1);
	if (err)
		goto err_clear_promisc;

	err = netdev_rx_handler_register(dev, ubr_port_rx_handler, p);
	if (err)
		goto err_clear_allmulti;

	dev->priv_flags |= IFF_UBR_PORT;

	return 0;

err_clear_allmulti:
	dev_set_allmulti(dev, -1);
err_clear_promisc:
	dev_set_promiscuity(dev, -1);
err_unlink:
	netdev_upper_dev_unlink(dev, ubr->dev);
err_uninit:
	ubr_vec_clear(&ubr->busy, pidx);
	__ubr_port_cleanup(&p->rcu);

	return err;
}

int ubr_port_del(struct ubr *ubr, struct net_device *dev)
{
	struct ubr_port *p = ubr_port_get_rtnl(dev);

	printk(KERN_NOTICE "Removing port %s from bridge %s ...\n", dev->name, ubr->dev->name);
	dev->priv_flags &= ~IFF_UBR_PORT;
	netdev_rx_handler_unregister(dev);
	dev_set_allmulti(dev, -1);
	dev_set_promiscuity(dev, -1);
	netdev_upper_dev_unlink(dev, ubr->dev);
	ubr_port_cleanup(p);

	return 0;
}

int ubr_port_find(struct ubr *ubr, struct net_device *dev)
{
	unsigned pidx;

	if (!ubr || !dev)
		return -EINVAL;

	ubr_vec_foreach(&ubr->busy, pidx) {
		if (ubr->ports[pidx].dev == dev)
			return pidx;
	}

	return -ENOENT;
}

static int __get_port(struct genl_info *info, struct nlattr **attrs,
		      u32 *port)
{
	int err;

	if (!info->attrs || !info->attrs[UBR_NLA_PORT])
		return -EINVAL;

	err = nla_parse_nested(attrs, UBR_NLA_PORT_MAX,
			       info->attrs[UBR_NLA_PORT],
			       ubr_nl_port_policy, info->extack);
	if (err)
		return err;

	if (!attrs[UBR_NLA_PORT_IFINDEX])
		return -EINVAL;

	*port = nla_get_u32(attrs[UBR_NLA_PORT_IFINDEX]);

	return 0;
}

int ubr_port_nl_set_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_PORT_MAX + 1];
	struct net_device *dev, *port;
	struct ubr_port *p;
	struct ubr_cb *cb;
	struct ubr *ubr;
	int ifindex;
	int pidx;
	u16 pvid = 0;
	int err;

	err = __get_port(info, attrs, &ifindex);
	if (err)
		return err;
	port = dev_get_by_index(genl_info_net(info), ifindex);

	if (attrs[UBR_NLA_PORT_PVID])
		pvid = nla_get_u16(attrs[UBR_NLA_PORT_PVID]);

	dev = ubr_netlink_dev(info);
	if (!dev)
		return -EINVAL;

	ubr = netdev_priv(dev);
	if (!ubr)
		goto err;

	pidx = ubr_port_find(ubr, port);
	if (pidx < 0 || pidx >= UBR_MAX_PORTS)
		goto err;

	dev_put(dev);

	p = &ubr->ports[pidx];
	p->pvid = pvid;

	cb = &p->ingress_cb;
	cb->vlan = ubr_vlan_find(ubr, pvid);

	/* Enable VLAN filtering when PVID is set. */
	if (pvid != 0)
		cb->vlan_ok = 0;
	else
		cb->vlan_ok = 1;

	return 0;
err:
	dev_put(dev);
	return -EINVAL;
}
