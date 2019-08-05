#ifndef __UBR_PRIVATE_H
#define __UBR_PRIVATE_H

#include <linux/bitmap.h>
#include <linux/slab.h>

#include <net/rtnetlink.h>

#define UBR_MAX_PORTS_SHIFT 8
#define UBR_MAX_PORTS      (1 << UBR_MAX_PORTS_SHIFT)

/* enum ubr_stp_state { */
/* 	UBR_STP_BLOCKING, */
/* #define	UBR_STP_LISTENING UBR_STP_BLOCKING */
/* 	UBR_STP_LEARNING, */
/* 	UBR_STP_FORWARDING */
/* }; */

/* struct ubr_stp { */
/* 	struct hlist_node node; */
/* 	u16 sid; */

/* 	unsigned long *forwarding; */
/* 	unsigned long *learning; */

/* 	struct rcu_head rcu; */
/* 	refcount_t refcount; */
/* }; */


struct ubr_vlan {
	struct ubr *ubr;
	struct hlist_node node;
	u16 vid;

	unsigned sa_learning:1;

	unsigned long *members;
	unsigned long *tagged;

	/* struct ubr_fdb *fdb; */
	/* struct ubr_stp *stp; */

	struct rcu_head rcu;
};


struct ubr_cb;

struct ubr_port {
	struct ubr *ubr;
	struct net_device *dev;
	struct ubr_cb *ingress_cb;

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
	DECLARE_HASHTABLE(fdbs, 8);
	DECLARE_HASHTABLE(stps, 8);
};

struct ubr_cb {
	/* Ingress port index */
	unsigned idx:UBR_MAX_PORTS_SHIFT;

	/* Ingress filter results. Setting these in a port's
	 * ingress_cb will _disable_ the corresponding filter,
	 * i.e. the result is assumed to be ok. */
	unsigned vlan_ok:1;
	unsigned stp_ok:1;
	unsigned sa_ok:1;
	
	unsigned sa_learning:1;

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
struct ubr_port *ubr_port_init(struct ubr *ubr, unsigned idx, struct net_device *dev);

int ubr_port_add(struct ubr *ubr, struct net_device *dev,
		 struct netlink_ext_ack *extack);
int ubr_port_del(struct ubr *ubr, struct net_device *dev);

/* ubr-vlan.c */
bool ubr_vlan_ingress(struct ubr *ubr, struct sk_buff *skb);

int ubr_vlan_port_add(struct ubr_vlan *vlan, unsigned idx, bool tagged);
int ubr_vlan_port_del(struct ubr_vlan *vlan, unsigned idx);

struct ubr_vlan *ubr_vlan_find(struct ubr *ubr, u16 vid);
int              ubr_vlan_del (struct ubr_vlan *vlan);
struct ubr_vlan *ubr_vlan_new (struct ubr *ubr, u16 vid, u16 fid, u16 sid);

int ubr_vlan_init(struct ubr *ubr);

#endif	/* __UBR_PRIVATE_H */
