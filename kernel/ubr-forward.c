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

static void ubr_deliver_many(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);
	struct sk_buff *cskb;
	int id, first;

	first = find_first_bit(cb->dst.ports, ubr->ports_max);

	id = first + 1;
	for_each_set_bit_from(id, cb->dst.ports, ubr->ports_max) {
		cskb = skb_clone(skb, GFP_ATOMIC);
		if (!cskb) {
			/* TODO: bump counter */
			goto err_free;
		}

		ubr_deliver_one(ubr, cskb, id);
	}

	ubr_deliver_one(ubr, skb, first);
	return;

err_free:
	kfree_skb(skb);
}

static void ubr_deliver(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);

	switch (cb->dst.type) {
	case UBR_DST_DROP:
		kfree_skb(skb);
		break;
	case UBR_DST_ONE:
		ubr_deliver_one(ubr, skb, cb->dst.port);
		break;
	case UBR_DST_MANY:
		ubr_deliver_many(ubr, skb);
		break;
	}
}

void ubr_forward(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);

	if (!ubr_dst_and(ubr, &cb->dst, ubr->active))
		goto drop;

	ubr_deliver(ubr, skb);
	return;
drop:
	kfree_skb(skb);
}
