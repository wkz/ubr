#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/rhashtable.h>
#include <linux/slab.h>

#include "ubr-private.h"


struct kmem_cache *ubr_fdb_cache __read_mostly;

static const struct rhashtable_params ubr_rht_params = {
	.head_offset = offsetof(struct ubr_fdb_node, rhnode),
	.key_offset = offsetof(struct ubr_fdb_node, addr),
	.key_len = sizeof(struct ubr_fdb_addr),
	.automatic_shrinking = true,
};

static void ubr_fdb_node_delete_rcu(struct rcu_head *rcu)
{
	struct ubr_fdb_node *node = container_of(rcu, struct ubr_fdb_node, rcu);

	kmem_cache_free(ubr_fdb_cache, node);
}

static inline bool ubr_fdb_node_is_old(struct ubr_fdb *fdb,
				       struct ubr_fdb_node *node)
{
	return (node->proto == UBR_FDB_DYNAMIC) &&
		time_is_before_jiffies(node->tstamp + fdb->ageing_timeout);
}

int ubr_fdb_flush(struct ubr_fdb *fdb, struct ubr_fdb_flush_op op)
{
	struct rhashtable_iter iter;
	struct ubr_fdb_node *node;

	rhashtable_walk_enter(&fdb->nodes, &iter);
	rhashtable_walk_start(&iter);

	while ((node = rhashtable_walk_next(&iter))) {
		if (IS_ERR(node)) {
			if (PTR_ERR(node) == -EAGAIN)
				continue;

			break;
		}

		if (op.per_port && !ubr_vec_test(&node->vec, op.pidx))
			continue;

		if (op.per_vlan && node->addr.vid != op.vid)
			continue;

		if (op.per_proto) {
			if (node->proto != op.proto)
				continue;

			if (op.proto == UBR_FDB_DYNAMIC &&
			    op.old && !ubr_fdb_node_is_old(fdb, node))
				continue;
		}

		rhashtable_remove_fast(&fdb->nodes, &node->rhnode,
				       ubr_rht_params);
		call_rcu(&node->rcu, ubr_fdb_node_delete_rcu);
	}

	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	return IS_ERR(node) ? PTR_ERR(node) : 0;
}

static void ubr_fdb_age(struct work_struct *work)
{
	struct ubr_fdb *fdb = container_of(work, struct ubr_fdb, age_work.work);
	struct ubr_fdb_flush_op op = {
		.per_proto = true,
		.proto = UBR_FDB_DYNAMIC,
		.old = true,
	};

	ubr_fdb_flush(fdb, op);
	queue_delayed_work(system_long_wq, &fdb->age_work,
			   fdb->ageing_timeout / 2);
}

static void ubr_fdb_learn(struct ubr_fdb *fdb, struct sk_buff *skb)
{
	struct ubr_fdb_node *node;
	struct ubr_fdb_addr key = {};
	struct ubr_cb *cb = ubr_cb(skb);

	if (unlikely(!is_valid_ether_addr(eth_hdr(skb)->h_source)))
		return;

	key.type = UBR_ADDR_MAC;
	key.vid = cb->vlan->vid;
	ether_addr_copy(key.mac, eth_hdr(skb)->h_source);

	node = rhashtable_lookup(&fdb->nodes, &key, ubr_rht_params);
	if (likely(node)) {
		if (node->proto != UBR_FDB_DYNAMIC)
			return;

		if (unlikely(!ubr_vec_test(&node->vec, cb->pidx))) {
			/* TODO counter: station moved */
			ubr_vec_zero(&node->vec);
			ubr_vec_set(&node->vec, cb->pidx);
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
	node->tstamp = jiffies;
	ubr_vec_set(&node->vec, cb->pidx);

	if (rhashtable_lookup_insert_fast(&fdb->nodes, &node->rhnode,
					  ubr_rht_params)) {
		kmem_cache_free(ubr_fdb_cache, node);
		/* TODO counter: learn insert fail */
		return;
	}
}

void ubr_fdb_forward(struct ubr_fdb *fdb, struct sk_buff *skb)
{
	struct ubr_fdb_node *node;
	struct ubr_fdb_addr key = {};
	struct ubr_vec *filter;
	struct ubr_cb *cb = ubr_cb(skb);

	if (cb->sa_learning && cb->vlan->sa_learning)
		ubr_fdb_learn(fdb, skb);

	key.vid = cb->vlan->vid;
	if (unlikely(is_multicast_ether_addr(eth_hdr(skb)->h_dest))) {
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			key.type = UBR_ADDR_IP4;
			key.ip4 = ip_hdr(skb)->daddr;
			goto lookup;
#if IS_ENABLED(CONFIG_IPV6)
		case htons(ETH_P_IPV6):
			key.type = UBR_ADDR_IP6;
			key.ip6 = ipv6_hdr(skb)->daddr;
			goto lookup;
#endif
		}
	}

	key.type = UBR_ADDR_MAC;
	ether_addr_copy(key.mac, eth_hdr(skb)->h_dest);
lookup:
	node = rhashtable_lookup(&fdb->nodes, &key, ubr_rht_params);
	if (likely(node)) {
		if (unlikely(ubr_fdb_node_is_old(fdb, node)))
			goto flood;

		filter = &node->vec;

		if (unlikely(key.type == UBR_ADDR_IP4 ||
			     key.type == UBR_ADDR_IP6)) {
			struct ubr_vec grp_n_routers = node->vec;

			ubr_vec_or(&grp_n_routers, &cb->vlan->routers);
			ubr_vec_and(&cb->vec, &grp_n_routers);
			return;
		}
	} else {
	flood:
		if (is_multicast_ether_addr(eth_hdr(skb)->h_dest)) {
			if (is_broadcast_ether_addr(eth_hdr(skb)->h_dest))
				filter = &cb->vlan->bcflood;
			else
				filter = &cb->vlan->mcflood;
		} else {
			filter = &cb->vlan->ucflood;
		}
	}

	ubr_vec_and(&cb->vec, filter);
}

int ubr_fdb_newlink(struct ubr_fdb *fdb)
{
	int err;

	fdb->ageing_timeout = msecs_to_jiffies(300 * MSEC_PER_SEC);

	err = rhashtable_init(&fdb->nodes, &ubr_rht_params);
	if (err)
		return err;

	INIT_DELAYED_WORK(&fdb->age_work, ubr_fdb_age);

	queue_delayed_work(system_long_wq, &fdb->age_work,
			   fdb->ageing_timeout / 2);

	return 0;
}

void ubr_fdb_dellink(struct ubr_fdb *fdb)
{
	struct ubr_fdb_flush_op op = {};

	cancel_delayed_work_sync(&fdb->age_work);

	ubr_fdb_flush(fdb, op);
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
