#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>

#include <net/dsa.h>
#include <net/rtnetlink.h>

#include "ubr-private.h"

rx_handler_result_t ubr_port_lower_recv(struct sk_buff **pskb)
{
	return RX_HANDLER_CONSUMED;
}

static void __ubr_port_cleanup(struct rcu_head *head)
{
	struct ubr_port *p = container_of(head, struct ubr_port, rcu);
	struct ubr *ubr = p->ubr;

	dev_put(ubr->port_devs[p->id]);
	ubr->port_devs[p->id] = NULL;
	memset(p, 0, sizeof(*p));
}

void ubr_port_cleanup(struct ubr_port *p)
{
	clear_bit(p->id, p->ubr->ports_busy);
	call_rcu(&p->rcu, __ubr_port_cleanup);
}

struct ubr_port *ubr_port_init(struct ubr *ubr, int id, struct net_device *dev)
{
	struct ubr_port *p = &ubr->ports[id];

	if (dev != ubr->dev)
		dev_hold(dev);

	p->ubr = ubr;
	p->id  = id;
	ubr->port_devs[id] = dev;

	/* err = ubr_switchdev_port_init(p); */
	/* if (err) */
	/* 	return err; */

	wmb();
	set_bit(id, ubr->ports_busy);
	ubr->ports_now++;

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
	int err, id;

	err = __ubr_port_add_allowed(dev, extack);
	if (err)
		return err;

	id = find_first_zero_bit(ubr->ports_busy, ubr->ports_max);
	if (id == ubr->ports_max) {
		NL_SET_ERR_MSG(extack, "Maximum number of ports reached");
		return -EBUSY;
	}

	p = ubr_port_init(ubr, id, dev);
	if (IS_ERR(p))
		return PTR_ERR(p);

	ubr_update_headroom(ubr, dev);
	netdev_update_features(ubr->dev);
	call_netdevice_notifiers(NETDEV_JOIN, dev);

	err = netdev_master_upper_dev_link(dev, ubr->dev, NULL, NULL, extack);
	if (err)
		goto err_uninit;

	err = dev_set_allmulti(dev, 1);
	if (err)
		goto err_unlink;

	err = netdev_rx_handler_register(dev, ubr_port_lower_recv, p);
	if (err)
		goto err_clear_allmulti;

	return 0;

err_clear_allmulti:
	dev_set_allmulti(dev, -1);
err_unlink:
	netdev_upper_dev_unlink(dev, ubr->dev);
err_uninit:
	clear_bit(id, ubr->ports_busy);
	__ubr_port_cleanup(&p->rcu);
	return err;
}

int ubr_port_del(struct ubr *ubr, struct net_device *dev)
{
	/* struct net_bridge_port *p; */
	/* bool changed_addr; */

	/* p = br_port_get_rtnl(dev); */
	/* if (!p || p->br != br) */
	/* 	return -EINVAL; */

	/* /\* Since more than one interface can be attached to a bridge, */
	/*  * there still maybe an alternate path for netconsole to use; */
	/*  * therefore there is no reason for a NETDEV_RELEASE event. */
	/*  *\/ */
	/* del_nbp(p); */

	/* br_mtu_auto_adjust(br); */
	/* br_set_gso_limits(br); */

	/* spin_lock_bh(&br->lock); */
	/* changed_addr = br_stp_recalculate_bridge_id(br); */
	/* spin_unlock_bh(&br->lock); */

	/* if (changed_addr) */
	/* 	call_netdevice_notifiers(NETDEV_CHANGEADDR, br->dev); */

	/* netdev_update_features(br->dev); */

	return 0;
}
