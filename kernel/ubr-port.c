#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>

#include <net/dsa.h>
#include <net/rtnetlink.h>

#include "ubr-private.h"

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

	ubr_vec_clear(&ubr->busy, p->ingress_cb.pidx);
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

	/* Allow egress on all ports execpt this one. */
	bitmap_fill(cb->vec.bitmap, UBR_MAX_PORTS);
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

	dev->priv_flags &= ~IFF_UBR_PORT;
	netdev_rx_handler_unregister(dev);
	dev_set_allmulti(dev, -1);
	dev_set_promiscuity(dev, -1);
	netdev_upper_dev_unlink(dev, ubr->dev);
	ubr_port_cleanup(p);
	return 0;
}
