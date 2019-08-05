#include <linux/if_vlan.h>

#include "ubr-private.h"

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

 	return test_bit(cb->idx, cb->vlan->members);
}

int ubr_vlan_port_add(struct ubr_vlan *vlan, unsigned idx, bool tagged)
{
	if (tagged) {
		set_bit(idx, vlan->tagged);
		smp_wmb();
	}

	set_bit(idx, vlan->members);
	return 0;
}

int ubr_vlan_port_del(struct ubr_vlan *vlan, unsigned idx)
{
	clear_bit(idx, vlan->members);

	smp_wmb();

	clear_bit(idx, vlan->tagged);		
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
	kfree(vlan->tagged);
	kfree(vlan->members);
	kfree(vlan);
}

int ubr_vlan_del(struct ubr_vlan *vlan)
{
	bitmap_zero(vlan->members, vlan->ubr->ports_max);
	call_rcu(&vlan->rcu, ubr_vlan_del_rcu);
	return 0;
}

struct ubr_vlan *ubr_vlan_new(struct ubr *ubr, u16 vid, u16 fid, u16 sid)
{
	struct ubr_vlan *vlan;
	int err = -ENOMEM;

	vlan = ubr_vlan_find(ubr, vid);
	if (vlan) {
		err = -EBUSY;
		goto err;
	}

	vlan = kzalloc(sizeof(*vlan), 0);
	if (!vlan)
		goto err;

	vlan->ubr = ubr;
	vlan->vid = vid;

	vlan->members = ubr_zalloc_vec(ubr, 0);
	if (!vlan->members)
		goto err;

	vlan->tagged = ubr_zalloc_vec(ubr, 0);
	if (!vlan->tagged)
		goto err;

	/* vlan->fdb = ubr_fdb_get(ubr, fid); */
	/* if (IS_ERR(vlan->fdb)) { */
	/* 	err = PTR_ERR(vlan->fdb); */
	/* 	vlan->fdb = NULL; */
	/* 	goto err; */
	/* } */

	hash_add(ubr->vlans, &vlan->node, vlan->vid);
	return vlan;

err:
	/* if (vlan && vlan->fdb) */
	/* 	ubr_fdb_put(vlan->fdb); */
	if (vlan && vlan->tagged)
		kfree(vlan->tagged);
	if (vlan && vlan->members)
		kfree(vlan->members);
	if (vlan)
		kfree(vlan);

	return ERR_PTR(err);
}

int ubr_vlan_init(struct ubr *ubr)
{
	struct ubr_vlan *vlan0;

	vlan0 = ubr_vlan_new(ubr, 0, 0, 0);
	if (IS_ERR(vlan0))
		return PTR_ERR(vlan0);

	return 0;
}
