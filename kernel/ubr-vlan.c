#include <linux/if_vlan.h>

#include "ubr-private.h"

static struct ubr_vlan *ubr_vlan_get(struct ubr *ubr, u16 vid)
{
	struct ubr_vlan *v;

	hash_for_each_possible(ubr->vlans, v, node, vid) {
		if (v->vid == vid)
			return v;
	}

	return NULL;
}

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
		cb->vlan = ubr_vlan_get(ubr, vid);

	if (unlikely(!cb->vlan))
		/* Untagged or priority tagged packet on port with no
		 * default vlan (PVID), or tagged packet with a VID
		 * not configured on this bridge. Drop. */
		return false;

	bitmap_and(cb->dst, cb->dst, cb->vlan->members, ubr->ports_max);
 	return test_bit(cb->id, cb->vlan->members);
}

int ubr_vlan_port_add(struct ubr *ubr, u16 vid, int id, bool tagged)
{
	struct ubr_vlan *v;

	v = ubr_vlan_get(ubr, vid);
	if (!v)
		return -ESRCH;

	set_bit(id, v->members);

	if (tagged)
		set_bit(id, v->tagged);
		
	return 0;
}

int ubr_vlan_port_del(struct ubr *ubr, u16 vid, int id)
{
	struct ubr_vlan *v;

	v = ubr_vlan_get(ubr, vid);
	if (!v)
		return -ESRCH;

	clear_bit(id, v->members);
	clear_bit(id, v->tagged);		
	return 0;
}

static int ubr_vlan_insert(struct ubr *ubr, struct ubr_vlan *v)
{
	struct ubr_vlan *search;

	hash_for_each_possible(ubr->vlans, search, node, v->vid) {
		if (search->vid == v->vid)
			return -EBUSY;
	}

	hash_add(ubr->vlans, &v->node, v->vid);
	return 0;
}

int ubr_vlan_add(struct ubr *ubr, u16 vid)
{
	struct ubr_vlan *v;
	int err = -ENOMEM;

	v = kzalloc(sizeof(*v), 0);
	if (!v)
		goto err;

	v->members = ubr_zalloc_vec(ubr, 0);
	if (!v->members)
		goto err_free_v;

	v->tagged = ubr_zalloc_vec(ubr, 0);
	if (!v->tagged)
		goto err_free_members;

	v->vid = vid;

	err = ubr_vlan_insert(ubr, v);
	if (err)
		goto err_free_tagged;

	return 0;

err_free_tagged:
	kfree(v->tagged);
err_free_members:
	kfree(v->members);
err_free_v:
	kfree(v);
err:
	return err;
}

int ubr_vlan_init(struct ubr *ubr)
{
	hash_init(ubr->vlans);
	return 0;
}
