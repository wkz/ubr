#include <linux/etherdevice.h>
#include <linux/rhashtable.h>
#include <linux/slab.h>

#include "ubr-private.h"


struct kmem_cache *ubr_fdb_cache __read_mostly;

static const struct rhashtable_params ubr_rht_params = {
	.head_offset = offsetof(struct ubr_fdb_node, rhnode),
	.key_offset = offsetof(struct ubr_fdb_node, addr),
	.key_len = sizeof(struct ubr_fdb_addr),
	.automatic_shrinking = true,
	.locks_mul = 1,
};

void ubr_fdb_learn(struct ubr_fdb *fdb, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);
	struct ethhdr *eth = eth_hdr(skb);
	struct ubr_fdb_node *node;
	struct ubr_fdb_addr key = {};

	if (unlikely(!is_valid_ether_addr(eth->h_source)))
		return;

	key.type = UBR_ADDR_MAC;
	key.vid = cb->vlan->vid;
	ether_addr_copy(key.mac, eth->h_source);

	node = rhashtable_lookup(&fdb->nodes, &key, ubr_rht_params);
	if (likely(node)) {
		if (node->proto != UBR_FDB_DYNAMIC)
			return;

		if (unlikely(node->pidx != cb->pidx)) {
			/* TODO counter: station moved */
			node->pidx = cb->pidx;
		}

		node->tstamp = jiffies;
		return;
	}

	node = kmem_cache_zalloc(ubr_fdb_cache, GFP_ATOMIC);
	if (unlikely(!node)) {
		/* TODO counter: learn no memory */
		return;
	}

	node->proto = UBR_FDB_DYNAMIC;
	node->addr = key;
	node->pidx = cb->pidx;
	node->tstamp = jiffies;

	if (rhashtable_lookup_insert_fast(&fdb->nodes, &node->rhnode,
					  ubr_rht_params)) {
		kmem_cache_free(ubr_fdb_cache, node);
		/* TODO counter: learn insert fail */
		return;
	}
}

int ubr_fdb_newlink(struct ubr_fdb *fdb)
{
	fdb->ageing_timeout = 300;

	return rhashtable_init(&fdb->nodes, &ubr_rht_params);
}

void ubr_fdb_dellink(struct ubr_fdb *fdb)
{
	/* TODO */
	/* ubr_fdb_flush(fdb, 0, NULL); */
	rhashtable_destroy(&fdb->nodes);
}

int __init ubr_fdb_cache_init(void)
{
	ubr_fdb_cache = kmem_cache_create("ubr-fdb",
					  sizeof(struct ubr_fdb_node), 0,
					  SLAB_HWCACHE_ALIGN, NULL);

	return ubr_fdb_cache ? 0 : -ENOMEM;
}

void ubr_fdb_cache_fini(void)
{
	kmem_cache_destroy(ubr_fdb_cache);
}
