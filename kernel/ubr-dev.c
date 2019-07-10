#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/netdevice.h>

#include <net/rtnetlink.h>

#include "ubr-private.h"

void ubr_update_headroom(struct ubr *ubr, struct net_device *new_dev)
{
	int id;

	ubr->dev->needed_headroom = max_t(unsigned short,
					  ubr->dev->needed_headroom,
					  netdev_get_fwd_headroom(new_dev));

	ubr_port_foreach(ubr, id) {
		netdev_set_rx_headroom(ubr->port_devs[id],
				       ubr->dev->needed_headroom);
	}
}

static int ubr_add_slave(struct net_device *dev, struct net_device *slave_dev,
			struct netlink_ext_ack *extack)

{
	struct ubr *ubr = netdev_priv(dev);

	return ubr_port_add(ubr, slave_dev, extack);
}

static int ubr_del_slave(struct net_device *dev, struct net_device *slave_dev)
{
	struct ubr *ubr = netdev_priv(dev);

	return ubr_port_del(ubr, slave_dev);
}

static const struct net_device_ops ubr_dev_ops = {
	/* .ndo_open		 = br_dev_open, */
	/* .ndo_stop		 = br_dev_stop, */
	/* .ndo_init		 = br_dev_init, */
	/* .ndo_uninit		 = br_dev_uninit, */
	/* .ndo_start_xmit		 = br_dev_xmit, */
	/* .ndo_get_stats64	 = br_get_stats64, */
	/* .ndo_set_mac_address	 = br_set_mac_address, */
	/* .ndo_set_rx_mode	 = br_dev_set_multicast_list, */
	/* .ndo_change_rx_flags	 = br_dev_change_rx_flags, */
	/* .ndo_change_mtu		 = br_change_mtu, */
	/* .ndo_do_ioctl		 = br_dev_ioctl, */

	.ndo_add_slave		 = ubr_add_slave,
	.ndo_del_slave		 = ubr_del_slave,
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

	/* TODO configurable ports_max */
	ubr->ports_max = 256;

	err = register_netdevice(dev);
	if (err)
		return err;

	err = -ENOMEM;

	ubr->ports = devm_kcalloc(&dev->dev, ubr->ports_max,
				  sizeof(*ubr->ports), 0);
	if (!ubr->ports)
		goto err_unregister;

	ubr->port_devs = devm_kcalloc(&dev->dev, ubr->ports_max,
				      sizeof(*ubr->port_devs), 0);
	if (!ubr->port_devs)
		goto err_unregister;

	ubr->ports_busy = ubr_mask_alloc(ubr);
	if (!ubr->ports_busy)
		goto err_unregister;

	ubr_port_init(ubr, 0, dev);
	return 0;

err_unregister:
	unregister_netdevice(dev);
	return err;
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
	/* .dellink		= ubr_dev_delete, */
	/* .get_size		= ubr_get_size, */
	/* .fill_info		= ubr_fill_info, */

	/* .slave_maxtype		= IFLA_UBRPORT_MAX, */
	/* .slave_policy		= ubr_port_policy, */
	/* .slave_changelink	= ubr_port_changelink, */
	/* .get_slave_size		= ubr_port_get_slave_size, */
	/* .fill_slave_info	= ubr_port_fill_slave_info, */
};

static int __init ubr_module_init(void)
{
	int err;

	pr_err("ubr hello\n");

	err = rtnl_link_register(&ubr_link_ops);
	if (err)
		return err;

	pr_err("ubr ready\n");
	return 0;
}

static void __exit ubr_module_cleanup(void)
{
	pr_err("ubr bye\n");

	rtnl_link_unregister(&ubr_link_ops);
}

module_init(ubr_module_init);
module_exit(ubr_module_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tobias Waldekranz");
MODULE_DESCRIPTION("The Unassuming Bridge");
MODULE_ALIAS_RTNL_LINK("ubr");
