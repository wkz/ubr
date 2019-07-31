#ifndef __UBR_PRIVATE_H
#define __UBR_PRIVATE_H

#include <linux/bitmap.h>
#include <linux/slab.h>

#include <net/rtnetlink.h>

/* struct ubr_addr { */
/* 	/\* ETH_P_IP , ETH_P_IPV6, ETH_P_ALL *\/ */
/* 	__be16 proto; */
/* 	union { */
/* 		__be32          ip4; */
/* 		struct in6_addr ip6; */
/* 		u8              mac[ETH_ALEN]; */
/* 	} */
/* }; */

/* struct ubr_dst { */
/* 	struct rhash_head rhnode; */
/* 	struct ubr_addr addr; */

/* 	struct ubr_portvec target; */
/* }; */

/* struct ubr_fdb { */
/* 	struct rhash_head rhnode; */
/* 	u16 fid; */

/* 	refcount_t refcnt; */
	
/* 	struct rhashtable *db; */
/* }; */

/* enum ubr_stp_state { */
/* 	UBR_STP_BLOCKING, */
/* 	UBR_STP_LISTENING, */
/* 	UBR_STP_LEARNING, */
/* 	UBR_STP_FORWARDING */
/* }; */

/* struct ubr_stpdb { */
/* 	struct rhash_head rhnode; */
/* 	u16 sid; */

/* 	refcount_t refcnt; */

/* 	struct ubr_portvec state[2]; */
/* }; */

/* static inline enum ubr_stp_state */
/* ubr_stpdb_get(const struct ubr_stpdb *stpdb, int port) */
/* { */
/* 	return !!test_bit(port, stpdb->state[0].v) | */
/* 		(!!test_bit(port, stpdb->state[1].v) << 1) */
/* } */

struct ubr_vlan {
	struct hlist_node node;
	u16 vid;

	unsigned long *members;
	unsigned long *tagged;

	/* struct ubr_fdb *fdb; */
	/* struct ubr_sdb *sdb; */

	struct rcu_head rcu;
};


struct ubr_switchdev_domain {
	unsigned long *members;
};

struct ubr_cb;

struct ubr_port {
	struct ubr *ubr;
	struct net_device *dev;
	int id;

	struct ubr_cb *ingress_cb;

	/* struct ubr_vlan *pvlan; */
	/* enum ubr_vlan_mode vlan_mode:1; */

	struct rcu_head rcu;
};

static inline struct ubr_port *ubr_port_get_rcu(const struct net_device *dev)
{
	return rcu_dereference(dev->rx_handler_data);
}

static inline struct ubr_port *ubr_port_get_rtnl(const struct net_device *dev)
{
	return rtnl_dereference(dev->rx_handler_data);
}

struct ubr {
	struct net_device *dev;

	struct ubr_port* ports;
	size_t ports_max;

	unsigned long *active;
	u16 vlan_proto;

	DECLARE_HASHTABLE(vlans, 8);
	/* struct rhashtable *vlans; */
	/* struct rhashtable *fdbs; */
	/* struct rhashtable *stpdbs; */
};

struct ubr_cb {
	unsigned dot1q:1;
	unsigned learn:1;

	struct ubr_vlan *vlan;

	unsigned long dst[0];
};
#define ubr_cb(_skb) ((struct ubr_cb *)(_skb)->cb)

#define ubr_vec_foreach(_ubr, _vec, _id) \
	for_each_set_bit(_id, (_vec), (_ubr)->ports_max)

#define ubr_port_foreach(_ubr, _id) \
	ubr_vec_foreach(_ubr, (_ubr)->active, _id)

#define ubr_vec_size(_ubr) \
	(BITS_TO_LONGS((_ubr)->ports_max) * sizeof(unsigned long))

#define ubr_vec_sizeof(_ubr, _obj) \
	(sizeof(_obj) + ubr_vec_size(_ubr))

static inline void *ubr_alloc_with_vec(struct ubr *ubr, size_t size, gfp_t flags)
{
	size += ubr_vec_size(ubr);
	return kmalloc(size, flags);
}

static inline void *ubr_zalloc_with_vec(struct ubr *ubr, size_t size, gfp_t flags)
{
	return ubr_alloc_with_vec(ubr, size, flags | __GFP_ZERO);
}

static inline unsigned long *ubr_zalloc_vec(struct ubr *ubr, gfp_t flags)
{
	return ubr_alloc_with_vec(ubr, 0, flags | __GFP_ZERO);
}


/* ubr-dev.c */
void ubr_update_headroom(struct ubr *ubr, struct net_device *new_dev);

/* ubr-forward.c */
void ubr_forward(struct ubr *ubr, struct sk_buff *skb);

/* ubr-port.c */
struct ubr_port *ubr_port_init(struct ubr *ubr, int id, struct net_device *dev);

int ubr_port_add(struct ubr *ubr, struct net_device *dev,
		 struct netlink_ext_ack *extack);
int ubr_port_del(struct ubr *ubr, struct net_device *dev);

/* ubr-vlan.c */
struct ubr_vlan *ubr_vlan_get(struct ubr *ubr, u16 vid);

int ubr_vlan_port_add(struct ubr *ubr, u16 vid, int id, bool tagged);
int ubr_vlan_port_del(struct ubr *ubr, u16 vid, int id);

int ubr_vlan_add(struct ubr *ubr, u16 vid);
int ubr_vlan_del(struct ubr *ubr, u16 vid);

int ubr_vlan_init(struct ubr *ubr);

#endif	/* __UBR_PRIVATE_H */
