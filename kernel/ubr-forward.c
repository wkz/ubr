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

void ubr_forward(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);
	struct ubr_dst *dst = NULL;
	bool allowed = true;

	bitmap_and(cb->dst, cb->dst, ubr->active, ubr->ports_max);

	if (cb->vlan_filter)
		allowed = ubr_vlan_ingress(ubr, skb);

	/* /\* uses regular SO_ATTACH_FILTER/BPF *\/ */
	/* if (unlikely(ubr_ctrl_ingress(ubr, skb))) */
	/* 	goto consumed; */

	/* if (allowed && cb->stp) */
	/* 	allowed = ubr_stp_ingress(ubr, skb); */

	if (unlikely(!allowed))
		goto drop;

	/* if (cb->fdb) { */
	/* 	if (cb->sa_learning) */
	/* 		ubr_fdb_learn(ubr, skb); */

	/* 	dst = ubr_fdb_lookup(ubr, skb); */
	/* } */

	if (!dst) {
		/* TODO: filter based on packet type, e.g. unknown
		 * unicast etc. */
		ubr_deliver(ubr, skb);
		return;
	}

	/* switch (dst->type) { */
	/* case UBR_DST_ONE: */
	/* 	if (!test_bit(dst->port, cb->dst)) */
	/* 		goto drop; */

	/* 	return ubr_deliver_one(ubr, skb, dst->port); */

	/* case UBR_DST_MANY: */
	/* 	bitmap_and(cb->dst, cb->dst, dst->ports, ubr->ports_max); */
	/* 	ubr_deliver(ubr, skb); */
	/* 	return; */
	/* } */

drop:
	kfree(skb);
/* consumed: */
	return;
}
