#include <linux/etherdevice.h>
#include <linux/netdevice.h>

#include "ubr-private.h"

static void ubr_deliver_one(struct ubr *ubr, struct sk_buff *skb, int pidx)
{
	struct ethhdr *eth = eth_hdr(skb);
	
	skb->dev = ubr->ports[pidx].dev;

	if (pidx) {
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
	int pidx, first;

	first = find_first_bit(cb->vec.bitmap, UBR_MAX_PORTS);
	if (first == UBR_MAX_PORTS)
		goto drop;

	pidx = first + 1;
	for_each_set_bit_from(pidx, cb->vec.bitmap, UBR_MAX_PORTS) {
		cskb = skb_clone(skb, GFP_ATOMIC);
		if (!cskb) {
			/* TODO: bump counter */
			goto drop;
		}

		ubr_deliver_one(ubr, cskb, pidx);
	}

	ubr_deliver_one(ubr, skb, first);
	return;

drop:
	kfree_skb(skb);
}

void ubr_forward(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);

	/* Run all enabled ingress checks and record the results, but
	 * don't drop filtered frames until ubr sockets have had a
	 * chance to trap them. */
	if (!cb->vlan_ok)
		cb->vlan_ok = ubr_vlan_ingress(ubr, skb);

	/* if (!cb->stp_ok) */
	/* 	cb->stp_ok = ubr_stp_ingress(ubr, skb); */

	/* if (!cb->sa_ok) */
	/* 	cb->sa_ok = ubr_fdb_ingress(ubr, skb); */

	/* Check if there is an ubr socket that want's to consume this
	 * packet. */
	/* if (ubr_ctrl_ingress(ubr, skb)) */
	/* 	goto consumed; */

	/* No filtered packets allowed beyond this point. */
	if (unlikely(!(cb->vlan_ok && cb->stp_ok && cb->sa_ok)))
		goto drop;

	ubr_vec_and(&cb->vec, &cb->vlan->members);

	ubr_fdb_forward(&ubr->fdb, skb);

	ubr_deliver(ubr, skb);
	return;
drop:
	kfree(skb);
/* consumed: */
	return;
}
