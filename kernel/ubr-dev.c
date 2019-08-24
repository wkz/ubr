#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/netdevice.h>

#include <net/rtnetlink.h>

#include "ubr-private.h"

void ubr_update_headroom(struct ubr *ubr, struct net_device *new_dev)
{
	unsigned pidx;

	ubr->dev->needed_headroom = max_t(unsigned short,
					  ubr->dev->needed_headroom,
					  netdev_get_fwd_headroom(new_dev));

	ubr_vec_foreach(&ubr->busy, pidx) {
		netdev_set_rx_headroom(ubr->ports[pidx].dev,
				       ubr->dev->needed_headroom);
	}
}

netdev_tx_t ubr_ndo_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ubr *ubr = netdev_priv(dev);
	struct ubr_cb *cb = ubr_cb(skb);

	skb_pull(skb, ETH_HLEN);

	memcpy(cb, &ubr->ports[0].ingress_cb, sizeof(*cb));
	ubr_forward(ubr, skb);
	return NETDEV_TX_OK;
}

static int ubr_ndo_add_slave(struct net_device *dev,
			     struct net_device *slave_dev,
			     struct netlink_ext_ack *extack)

{
	struct ubr *ubr = netdev_priv(dev);

	return ubr_port_add(ubr, slave_dev, extack);
}

static int ubr_ndo_del_slave(struct net_device *dev,
			     struct net_device *slave_dev)
{
	struct ubr *ubr = netdev_priv(dev);

	return ubr_port_del(ubr, slave_dev);
}

static const struct net_device_ops ubr_dev_ops = {
	/* .ndo_open		 = br_dev_open, */
	/* .ndo_stop		 = br_dev_stop, */
	/* .ndo_init		 = br_dev_init, */
	/* .ndo_uninit		 = br_dev_uninit, */
	.ndo_start_xmit		 = ubr_ndo_start_xmit,
	/* .ndo_get_stats64	 = br_get_stats64, */
	/* .ndo_set_mac_address	 = br_set_mac_address, */
	/* .ndo_set_rx_mode	 = br_dev_set_multicast_list, */
	/* .ndo_change_rx_flags	 = br_dev_change_rx_flags, */
	/* .ndo_change_mtu		 = br_change_mtu, */
	/* .ndo_do_ioctl		 = br_dev_ioctl, */

	.ndo_add_slave		 = ubr_ndo_add_slave,
	.ndo_del_slave		 = ubr_ndo_del_slave,
	/* .ndo_fix_features        = br_fix_features, */
	/* .ndo_fdb_add		 = br_fdb_add, */
	/* .ndo_fdb_del		 = br_fdb_delete, */
	/* .ndo_fdb_dump		 = br_fdb_dump, */
	/* .ndo_bridge_getlink	 = br_getlink, */
	/* .ndo_bridge_setlink	 = br_setlink, */
	/* .ndo_bridge_dellink	 = br_dellink, */
	/* .ndo_features_check	 = passthru_features_check, */
};

static struct device_type ubr_dev_type = {
	.name	= "ubr",
};

void ubr_dev_setup(struct net_device *dev)
{
	eth_hw_addr_random(dev);
	ether_setup(dev);

	dev->netdev_ops = &ubr_dev_ops;
	SET_NETDEV_DEVTYPE(dev, &ubr_dev_type);

	dev->features = NETIF_F_NETNS_LOCAL | NETIF_F_HW_VLAN_CTAG_TX;
	dev->hw_features = NETIF_F_HW_VLAN_CTAG_TX;
}

static int ubr_dev_newlink(struct net *src_net, struct net_device *dev,
			   struct nlattr *tb[], struct nlattr *data[],
			   struct netlink_ext_ack *extack)
{
	struct ubr *ubr = netdev_priv(dev);
	int err;

	memset(ubr, 0, sizeof(*ubr));

	ubr->dev = dev;
	ubr->vlan_proto = ETH_P_8021Q;
	hash_init(ubr->vlans);
	hash_init(ubr->fdbs);
	hash_init(ubr->stps);

	err = ubr_vlan_init(ubr);
	if (err)
		return err;

	ubr_port_init(ubr, 0, dev);

	return 	register_netdevice(dev);
}

void ubr_dev_dellink(struct net_device *dev, struct list_head *head)
{
	struct ubr *ubr = netdev_priv(dev);
	int pidx;

	ubr_vec_foreach(&ubr->busy, pidx) {
		if (!pidx)
			continue;

		ubr_port_del(ubr, ubr->ports[pidx].dev);
	}

	/* TODO cleanup VLAN db */

	unregister_netdevice_queue(ubr->dev, head);
}

struct rtnl_link_ops ubr_link_ops __read_mostly = {
	.kind			= "ubr",
	.priv_size		= sizeof(struct ubr),
	.setup			= ubr_dev_setup,
	/* .maxtype		= IFLA_UBR_MAX, */
	/* .policy			= ubr_policy, */
	/* .validate		= ubr_validate, */
	.newlink		= ubr_dev_newlink,
	/* .changelink		= ubr_dev_changelink, */
	.dellink		= ubr_dev_dellink,
	/* .get_size		= ubr_get_size, */
	/* .fill_info		= ubr_fill_info, */

	/* .slave_maxtype		= IFLA_UBRPORT_MAX, */
	/* .slave_policy		= ubr_port_policy, */
	/* .slave_changelink	= ubr_port_changelink, */
	/* .get_slave_size		= ubr_port_get_slave_size, */
	/* .fill_slave_info	= ubr_port_fill_slave_info, */
};


static int ubr_device_event(struct notifier_block *unused, unsigned long event,
			    void *data)
{
	struct net_device *dev = netdev_notifier_info_to_dev(data);
	struct ubr_port *p;

	if (!netif_is_ubr_port(dev))
		return NOTIFY_DONE;

	p = ubr_port_get_rtnl(dev);

	switch (event) {
	/* case NETDEV_CHANGEMTU: */
	/* 	ubr_mtu_auto_adjust(ubr); */
	/* 	break; */

	/* case NETDEV_CHANGE: */
	/* 	ubr_port_carrier_check(p, &notified); */
	/* 	break; */

	/* case NETDEV_FEAT_CHANGE: */
	/* 	netdev_update_features(ubr->dev); */
	/* 	break; */

	case NETDEV_UNREGISTER:
		ubr_port_del(ubr_from_port(p), dev);
		break;

	case NETDEV_PRE_TYPE_CHANGE:
		/* Forbid underlaying device to change its type. */
		return NOTIFY_BAD;

	/* case NETDEV_RESEND_IGMP: */
	/* 	/\* Propagate to master device *\/ */
	/* 	call_netdevice_notifiers(event, ubr->dev); */
	/* 	break; */
	}

	return NOTIFY_DONE;
}

static struct notifier_block ubr_device_notifier = {
	.notifier_call = ubr_device_event
};

static int __init ubr_module_init(void)
{
	int err;

	BUILD_BUG_ON(sizeof(struct ubr_cb) > FIELD_SIZEOF(struct sk_buff, cb));

	err = register_netdevice_notifier(&ubr_device_notifier);
	if (err)
		goto err;

	err = rtnl_link_register(&ubr_link_ops);
	if (err)
		goto err_unreg_notifier;

	return 0;

err_unreg_notifier:
	unregister_netdevice_notifier(&ubr_device_notifier);
err:
	return err;
}

static void __exit ubr_module_cleanup(void)
{
	rtnl_link_unregister(&ubr_link_ops);
	unregister_netdevice_notifier(&ubr_device_notifier);
}

module_init(ubr_module_init);
module_exit(ubr_module_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tobias Waldekranz");
MODULE_DESCRIPTION("The Unassuming Bridge");
MODULE_ALIAS_RTNL_LINK("ubr");
