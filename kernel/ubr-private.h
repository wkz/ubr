#ifndef __UBR_PRIVATE_H
#define __UBR_PRIVATE_H

#include <linux/bitmap.h>
#include <linux/slab.h>

#include <net/rtnetlink.h>

/* TODO move to linux/netdevice.h */
#define IFF_UBR_PORT (1 << 31)
static inline bool netif_is_ubr_port(const struct net_device *dev)
{
	return dev->priv_flags & IFF_UBR_PORT;
}

#define UBR_MAX_PORTS_SHIFT 8
#define UBR_MAX_PORTS      (1 << UBR_MAX_PORTS_SHIFT)

/* Port vector */
struct ubr_vec {
	unsigned long bitmap[BITS_TO_LONGS(UBR_MAX_PORTS)];
};

#define __ubr_vec_bitmap_op(_op, ...) \
	bitmap_ ## _op(__VA_ARGS__, UBR_MAX_PORTS)

#define ubr_vec_and(_vd, _vs) \
	__ubr_vec_bitmap_op(and, (_vd)->bitmap, (_vd)->bitmap, (_vs)->bitmap)

#define ubr_vec_set(_v, _bit)   set_bit((_bit), (_v)->bitmap)
#define ubr_vec_clear(_v, _bit) clear_bit((_bit), (_v)->bitmap)
#define ubr_vec_test(_v, _bit)  test_bit((_bit), (_v)->bitmap)

#define ubr_vec_foreach(_v, _bit) \
	for_each_set_bit(_bit, (_v)->bitmap, UBR_MAX_PORTS)


/* Control buffer */
struct ubr_cb {
	/* Ingress port index */
	unsigned pidx:UBR_MAX_PORTS_SHIFT;

	/* Ingress filter results. Setting these in a port's
	 * ingress_cb will _disable_ the corresponding filter,
	 * i.e. the result is always treated as ok for all packets
	 * ingressing on that port. */
	unsigned vlan_ok:1;
	unsigned stp_ok:1;
	unsigned sa_ok:1;
	
	unsigned sa_learning:1;

	struct ubr_vlan *vlan;

	struct ubr_vec vec;
};
#define ubr_cb(_skb) ((struct ubr_cb *)(_skb)->cb)


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

/* enum ubr_addr_type { */
/* 	UBR_ADDR_MAC, */
/* 	UBR_ADDR_IP4, */
/* 	UBR_ADDR_IP6, */
/* }; */

/* struct ubr_addr { */
/* 	enum ubr_addr_type type; */
/* 	union { */
/* 		u8 mac[ETH_ALEN]; */
/* 		be32 ip4; */
/* 		struct in6_addr ip6; */
/* 	} */
/* }; */

/* struct ubr_dst { */
/* 	struct rhash_head node; */

/* 	struct ubr_addr addr; */

/* 	unsigned vector:1; */
/* 	unsigned offload:1; */
/* }; */

/* struct ubr_dst_port { */
/* 	unsigned port:UBR_MAX_PORTS_SHIFT; */
/* 	unsigned dynamic:1; */
/* 	unsigned external:1; */

/* 	struct ubr_dst dst; */

/* 	unsigned long age; */
/* }; */

/* struct ubr_dst_vector { */
/* 	struct ubr_vector vector; */
/* 	struct ubr_dst dst; */
/* }; */

/* struct ubr_fdb { */
/* 	struct hlist_node node; */
/* 	u16 fid; */

/* 	struct rhashtable dsts; */
/* } */

struct ubr_vlan {
	struct ubr *ubr;
	struct hlist_node node;
	u16 vid;

	unsigned sa_learning:1;

	struct ubr_vec members;
	struct ubr_vec tagged;

	/* struct ubr_fdb *fdb; */
	/* struct ubr_stp *stp; */

	struct rcu_head rcu;
};

struct ubr_port {
	struct net_device *dev;
	struct ubr_cb ingress_cb;

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

	struct ubr_vec active;
	u16 vlan_proto;

	DECLARE_HASHTABLE(vlans, 8);
	DECLARE_HASHTABLE(fdbs, 8);
	DECLARE_HASHTABLE(stps, 8);

	struct ubr_vec  busy;
	struct ubr_port ports[UBR_MAX_PORTS];
};
#define ubr_from_port(_port) \
	container_of((_port), struct ubr, ports[(_port)->ingress_cb.pidx])


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
