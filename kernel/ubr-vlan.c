#include <linux/if_vlan.h>

#include "ubr-netlink.h"
#include "ubr-private.h"

const struct nla_policy ubr_nl_vlan_policy[UBR_NLA_VLAN_MAX + 1] = {
	[UBR_NLA_VLAN_UNSPEC]   = { .type = NLA_UNSPEC },
	[UBR_NLA_VLAN_VID]      = { .type = NLA_U16    },
	[UBR_NLA_VLAN_PORT]     = { .type = NLA_U32    },
	[UBR_NLA_VLAN_TAGGED]   = { .type = NLA_U32    }, /* XXX: bool */
	[UBR_NLA_VLAN_LEARNING] = { .type = NLA_U32    }, /* XXX: bool */
};

bool ubr_vlan_ingress(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);
	bool tagged = false;
	u16 vid = 0;

	/*
	 * This looks weird, but skb_vlan_tag_present() looks for any
	 * already offloaded VLAN information, not for 0x8100 in the
	 * payload after the source address.  So, if there is no VLAN
	 * info extracted already from the frame we do that here.
	 */
	if (unlikely(!skb_vlan_tag_present(skb) &&
		     skb->protocol == ubr->vlan_proto)) {
		skb = skb_vlan_untag(skb);
		if (unlikely(!skb))
			return false;
	}

	/*
	 * If we end up with VLAN information we handle that here.
	 * First, the VLAN proto field may be something odd, e.g.
	 * an S-tag instead of a C-tag, then we convert to C-tag.
	 * Second, if the frame was a sane C-tag, extract the VID
	 * for further processing below.
	 */
	if (unlikely(skb_vlan_tag_present(skb))) {
		if (unlikely(htons(skb->vlan_proto) != ubr->vlan_proto)) {
			skb = vlan_insert_tag_set_proto(skb, skb->vlan_proto,
							skb_vlan_tag_get(skb));
			if (unlikely(!skb))
				return false;

			skb_reset_mac_len(skb);
		} else {
			vid = skb_vlan_tag_get_id(skb);
			tagged = vid ? true : false;
		}
	}

	if (tagged)
		cb->vlan = ubr_vlan_find(ubr, vid);

	/*
	 * Untagged or priority tagged packet on port with no default
	 * vlan (PVID), or tagged packet with a VID not configured on
	 * this bridge, or PVID does not have a VLAN (yet). Drop.
	 */
	if (unlikely(!cb->vlan))
		return false;

	if (!tagged)
		__vlan_hwaccel_put_tag(skb, htons(ubr->vlan_proto), cb->vlan->vid);

 	return ubr_vec_test(&cb->vlan->members, cb->pidx);
}

int ubr_vlan_port_add(struct ubr_vlan *vlan, unsigned pidx, bool tagged)
{
	if (tagged) {
		ubr_vec_set(&vlan->tagged, pidx);
		smp_wmb();
	}

	ubr_vec_set(&vlan->members, pidx);

	return 0;
}

int ubr_vlan_port_del(struct ubr_vlan *vlan, unsigned pidx)
{
	ubr_vec_clear(&vlan->members, pidx);

	smp_wmb();

	ubr_vec_clear(&vlan->tagged, pidx);

	return 0;
}

struct ubr_vlan *ubr_vlan_find(struct ubr *ubr, u16 vid)
{
	struct ubr_vlan *vlan;

	hash_for_each_possible(ubr->vlans, vlan, node, vid) {
		if (vlan->vid == vid)
			return vlan;
	}

	return NULL;
}

static void ubr_vlan_del_rcu(struct rcu_head *head)
{
	struct ubr_vlan *vlan = container_of(head, struct ubr_vlan, rcu);

	/* ubr_fdb_put(vlan->fdb); */
	kfree(vlan);
}

int ubr_vlan_del(struct ubr_vlan *vlan)
{
	ubr_vec_zero(&vlan->members);
	call_rcu(&vlan->rcu, ubr_vlan_del_rcu);

	return 0;
}

struct ubr_vlan *ubr_vlan_new(struct ubr *ubr, u16 vid, u16 fid, u16 sid)
{
	struct ubr_vlan *vlan;
	int err = -ENOMEM;

	vlan = ubr_vlan_find(ubr, vid);
	if (vlan)
		return ERR_PTR(-EBUSY);

	vlan = kzalloc(sizeof(*vlan), 0);
	if (!vlan)
		goto err;

	vlan->ubr = ubr;
	vlan->vid = vid;
	vlan->sa_learning = 1;

	/* Flood unknown traffic by default. */
	ubr_vec_fill(&vlan->mcflood);
	ubr_vec_fill(&vlan->bcflood);
	ubr_vec_fill(&vlan->ucflood);

	hash_add(ubr->vlans, &vlan->node, vlan->vid);

	return vlan;

err:
	if (vlan)
		kfree(vlan);

	return ERR_PTR(err);
}

int ubr_vlan_newlink(struct ubr *ubr)
{
	struct ubr_vlan *vlan0;

	vlan0 = ubr_vlan_new(ubr, 0, 0, 0);
	if (IS_ERR(vlan0))
		return PTR_ERR(vlan0);

	return 0;
}

void ubr_vlan_dellink(struct ubr *ubr)
{
	/* TODO: cleanup all vlans */
}

static int __get_vid(struct genl_info *info, struct nlattr **attrs, u16 *vid)
{
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

	*vid = nla_get_u16(attrs[UBR_NLA_VLAN_VID]);

	return 0;
}

static int __get_port(struct genl_info *info, struct nlattr **attrs,
		      u32 *port, u32 *tagged)
{
	if (!attrs[UBR_NLA_VLAN_PORT])
		return -EINVAL;

	*port = nla_get_u32(attrs[UBR_NLA_VLAN_PORT]);

	if (tagged && attrs[UBR_NLA_VLAN_TAGGED])
		*tagged = nla_get_u32(attrs[UBR_NLA_VLAN_TAGGED]);

	return 0;
}

static int __get_learning(struct genl_info *info, struct nlattr **attrs,
			  u32 *learning)
{
	if (!attrs[UBR_NLA_VLAN_LEARNING])
		return -EINVAL;

	*learning = nla_get_u32(attrs[UBR_NLA_VLAN_LEARNING]);

	return 0;
}

static int __get_bridge_vlan_port(struct genl_info *info,
				  struct nlattr **attrs,
				  struct ubr **ubr,
				  struct ubr_vlan **vlan,
				  u16 *vid,
				  struct net_device **port,
				  u32 *ifindex,
				  u32 *tagged)
{
	struct net_device *dev = ubr_netlink_dev(info);
	u16 tmp1;
	u32 tmp2;
	int err;

	if (!vid)
		vid = &tmp1;
	if (!ifindex)
		ifindex = &tmp2;

	if (!dev)
		return -EINVAL;

	err = __get_vid(info, attrs, vid);
	if (err)
		goto err;

	err = __get_port(info, attrs, ifindex, tagged);
	if (err)
		goto err;

	*port = dev_get_by_index(genl_info_net(info), *ifindex);
	if (!*port) {
		err = -EINVAL;
		goto err;
	}

	*ubr = netdev_priv(dev);
	if (!ubr) {
		err = -EINVAL;
		goto err2;
	}

	*vlan = ubr_vlan_find(*ubr, *vid);
	if (!ubr) {
		err = -ENOENT;
		goto err2;
	}

	dev_put(dev);
	return 0;

err2:
	dev_put(*port);
err:
	dev_put(dev);
	return err;
}

int ubr_vlan_nl_add_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	struct net_device *dev;
	struct ubr_vlan *vlan;
	struct ubr *ubr;
	u16 vid;
	int err;

	err = __get_vid(info, attrs, &vid);
	if (err)
		return err;

	dev = ubr_netlink_dev(info);
	if (!dev)
		return -EINVAL;

	printk(KERN_NOTICE "Add VLAN %u to %s, hello\n", vid, dev->name);

	ubr = netdev_priv(dev);
	dev_put(dev);

	vlan = ubr_vlan_new(ubr, vid, 0, 0);
	if (IS_ERR(vlan))
		return PTR_ERR(vlan);

	return 0;
}

int ubr_vlan_nl_del_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	struct net_device *dev;
	struct ubr_vlan *vlan;
	struct ubr *ubr;
	u16 vid;
	int err;

	err = __get_vid(info, attrs, &vid);
	if (err)
		return err;

	dev = ubr_netlink_dev(info);
	if (!dev)
		return -EINVAL;

	printk(KERN_NOTICE "Del VLAN %u from %s, hello\n", vid, dev->name);

	ubr = netdev_priv(dev);
	dev_put(dev);
	vlan = ubr_vlan_find(ubr, vid);
	if (!vlan)
		return -ENOENT;

	return ubr_vlan_del(vlan);
}

int ubr_vlan_nl_set_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	struct net_device *dev;
	char flags[42] = "";
	u32 learning = -1;
	u16 vid;
	int err;

	err = __get_vid(info, attrs, &vid);
	if (err)
		return err;
	err = __get_learning(info, attrs, &learning);
	if (!err)
		snprintf(flags, sizeof(flags), " learning %s", learning ? "on" : "off");

	dev = ubr_netlink_dev(info);
	printk(KERN_NOTICE "Set VLAN %u%s on %s, hello\n", vid, flags, dev->name);
	dev_put(dev);

	return 0;
}

int ubr_vlan_nl_attach_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	struct net_device *port;
	struct ubr_vlan *vlan;
	struct ubr *ubr;
	u32 ifindex, tagged = 0;
	u16 vid;
	int err, pidx;

	err = __get_bridge_vlan_port(info, attrs, &ubr, &vlan, &vid,
				     &port, &ifindex, &tagged);
	if (err)
		return err;

	pidx = ubr_port_find(ubr, port);
	if (pidx < 0) {
		err = -ENODEV;
		goto err;
	}

	printk(KERN_NOTICE "Attach pidx %d to VLAN %u, bridge %s, port %s ifindex %d %stagged\n",
	       pidx, vid, ubr->dev->name, port->name, ifindex, tagged ? "" : "not ");
	dev_put(port);

	return ubr_vlan_port_add(vlan, pidx, tagged);
err:
	dev_put(port);
	return err;
}

int ubr_vlan_nl_detach_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	struct net_device *port;
	struct ubr_vlan *vlan;
	struct ubr *ubr;
	u32 ifindex;
	int err, pidx;

	err = __get_bridge_vlan_port(info, attrs, &ubr, &vlan, NULL,
				     &port, &ifindex, NULL);
	if (err)
		return err;

	pidx = ubr_port_find(ubr, port);
	if (pidx < 0)
		goto err;

	printk(KERN_NOTICE "Detach pidx %d from VLAN %u, bridge %s, port %s ifindex %d\n",
	       pidx, vlan->vid, ubr->dev->name, port->name, ifindex);
	dev_put(port);

	return ubr_vlan_port_del(vlan, pidx);
err:
	dev_put(port);
	return err;
}
