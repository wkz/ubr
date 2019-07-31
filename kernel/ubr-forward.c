#include <linux/etherdevice.h>
#include <linux/netdevice.h>

#include "ubr-private.h"

static void ubr_deliver_one(struct ubr *ubr, struct sk_buff *skb, int id)
{
	struct ethhdr *eth = eth_hdr(skb);

	skb->dev = ubr->ports[id].dev;

	if (id) {
		skb_push(skb, ETH_HLEN);
		dev_queue_xmit(skb);
	} else {
		if (ether_addr_equal(ubr->dev->dev_addr, eth->h_dest))
			skb->pkt_type = PACKET_HOST;

		netif_receive_skb(skb);
	}
}

static void ubr_deliver(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);
	struct sk_buff *cskb;
	int id, first;

	first = find_first_bit(cb->dst, ubr->ports_max);
	if (first == ubr->ports_max)
		goto drop;

	id = first + 1;
	for_each_set_bit_from(id, cb->dst, ubr->ports_max) {
		cskb = skb_clone(skb, GFP_ATOMIC);
		if (!cskb) {
			/* TODO: bump counter */
			goto drop;
		}

		ubr_deliver_one(ubr, cskb, id);
	}

	ubr_deliver_one(ubr, skb, first);
	return;

drop:
	kfree_skb(skb);
}

static bool ubr_dot1q_ingress(struct ubr *ubr, struct sk_buff *skb)
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
	return true;
}

void ubr_forward(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);

	if (cb->dot1q)
		if (!ubr_dot1q_ingress(ubr, skb))
			goto drop;
	else
		bitmap_and(cb->dst, cb->dst, ubr->active, ubr->ports_max);

	ubr_deliver(ubr, skb);
	return;

drop:
	kfree(skb);
}
