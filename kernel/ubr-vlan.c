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

	if (unlikely(!skb_vlan_tag_present(skb) &&
		     skb->protocol == ubr->vlan_proto)) {
		skb = skb_vlan_untag(skb);
		if (unlikely(!skb))
			return false;
	}

	if (unlikely(skb_vlan_tag_present(skb))) {
		if (unlikely(skb->vlan_proto != ubr->vlan_proto)) {
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

	if (unlikely(!cb->vlan))
		/* Untagged or priority tagged packet on port with no
		 * default vlan (PVID), or tagged packet with a VID
		 * not configured on this bridge. Drop. */
		return false;

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

	return 0;
}

int ubr_vlan_nl_attach_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	struct net_device *dev, *port;
	u32 ifindex, tagged = 0;
	u16 vid;
	int err;

	err = __get_vid(info, attrs, &vid);
	if (err)
		return err;

	err = __get_port(info, attrs, &ifindex, &tagged);
	if (err)
		return err;

	dev = ubr_netlink_dev(info);
	port = dev_get_by_index(genl_info_net(info), ifindex);
	if (!dev || !port)
		return -EINVAL;

	printk(KERN_NOTICE "Attach to VLAN %u, bridge %s, port %s ifindex %d %stagged\n",
	       vid, dev->name, port->name, ifindex, tagged ? "" : "not ");

	return 0;
}

int ubr_vlan_nl_detach_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[UBR_NLA_VLAN_MAX + 1];
	struct net_device *dev, *port;
	u32 ifindex;
	u16 vid;
	int err;

	err = __get_vid(info, attrs, &vid);
	if (err)
		return err;

	err = __get_port(info, attrs, &ifindex, NULL);
	if (err)
		return err;

	dev = ubr_netlink_dev(info);
	port = dev_get_by_index(genl_info_net(info), ifindex);
	if (!dev || !port)
		return -EINVAL;

	printk(KERN_NOTICE "Detach to VLAN %u, bridge %s, port %s ifindex %d\n",
	       vid, dev->name, port->name, ifindex);

	return 0;
}
