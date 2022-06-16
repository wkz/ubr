#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>

#include "ubr-private.h"

static void ubr_common_egress(struct ubr *ubr, struct sk_buff *skb, int pidx)
{
	struct ubr_cb *cb = ubr_cb(skb);
	int depth;

	if (cb->vlan_filtering) {
		if (ubr_vec_test(&cb->vlan->tagged, pidx))
			skb_vlan_push(skb, htons(ubr->vlan_proto), skb->vlan_tci);
		else
			__vlan_hwaccel_clear_tag(skb);
	}

	if (!__vlan_get_protocol(skb, skb->protocol, &depth)) {
		kfree_skb(skb);
		return;
	}

	skb_set_network_header(skb, depth);
}

static void ubr_deliver_up(struct ubr *ubr, struct sk_buff *skb)
{
	struct ethhdr *eth = eth_hdr(skb);

	skb->dev = ubr->dev;
	ubr_common_egress(ubr, skb, 0);

	if (ether_addr_equal(ubr->dev->dev_addr, eth->h_dest))
		skb->pkt_type = PACKET_HOST;

	netif_receive_skb(skb);
}

static void ubr_deliver_down(struct ubr *ubr, struct sk_buff *skb, int pidx)
{
	skb_push(skb, ETH_HLEN);
	skb->dev = ubr->ports[pidx].dev;
	ubr_common_egress(ubr, skb, pidx);

	dev_queue_xmit(skb);
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

		ubr_deliver_down(ubr, cskb, pidx);
	}

	if (first == 0)
		ubr_deliver_up(ubr, skb);
	else
		ubr_deliver_down(ubr, skb, first);

	return;
drop:
	kfree_skb(skb);
}

/* TODO */
static bool ubr_ctrl_ingress(struct ubr *ubr, struct sk_buff *skb) { return true; };
static bool ubr_stp_ingress(struct ubr *ubr, struct sk_buff *skb) { return true; };
static bool ubr_fdb_ingress(struct ubr *ubr, struct sk_buff *skb) { return true; };

bool ubr_forward(struct ubr *ubr, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);
	bool allow;

	/* All subsequent stages rely on the frame's VID being known,
	 * so this must run first.
	 */
	allow = ubr_vlan_ingress(ubr, skb);

	/* Then run stages can potentially classify the frame as
	 * control traffic to be trapped.
	 */
	allow &= !cb->ctrl && ubr_fdb_forward(&ubr->fdb, skb);
	allow &= !cb->ctrl && ubr_ctrl_ingress(ubr, skb);
	if (cb->ctrl) {
		skb->dev = ubr->dev;
		return true;
	}

	/* Now that forward/drop are the only remaining outcomes, we
	 * can lazily evaluate the remaining stages.
	 */
	if (allow &&
	    ubr_stp_ingress(ubr, skb) &&
	    ubr_fdb_ingress(ubr, skb))
		ubr_deliver(ubr, skb);
	else
		kfree_skb(skb);

	return false;
}
