#ifndef __UBR_PRIVATE_H
#define __UBR_PRIVATE_H

#include <linux/bitmap.h>
#include <linux/slab.h>

#include <net/rtnetlink.h>
#include <net/genetlink.h>

/* TODO move to bits/socket.h */
#define PF_UBR PF_SNA
#define AF_UBR PF_UBR

/* TODO move to linux/if_ubr.h */
enum sockaddr_ubr_type {
	UBR_TYPE_IEEE_GROUP,
	UBR_TYPE_ETH_TYPE,
	UBR_TYPE_IPV4_UDP_BC,
};

#define UBR_FLAG_BYPASS_STG  (1 << 0)
#define UBR_FLAG_BYPASS_SA   (1 << 1)
#define UBR_FLAG_BYPASS_VLAN (1 << 2)

struct sockaddr_ubr_proto {
	enum sockaddr_ubr_type type;

	__u32 flags;
	union {
		  __u8 ieee_group;
		__be16 eth_type;
		__be16 udp_dport;
	};
};

struct sockaddr_ubr {
	__kernel_sa_family_t subr_family;

	int subr_ifindex;
	struct sockaddr_ubr_proto subr_proto;
};

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

#define ubr_vec_or(_vd, _vs) \
	__ubr_vec_bitmap_op(or, (_vd)->bitmap, (_vd)->bitmap, (_vs)->bitmap)

#define ubr_vec_zero(_v) \
	__ubr_vec_bitmap_op(zero, (_v)->bitmap)

#define ubr_vec_fill(_v) \
	__ubr_vec_bitmap_op(fill, (_v)->bitmap)

#define ubr_vec_set(_v, _bit)   set_bit((_bit), (_v)->bitmap)
#define ubr_vec_clear(_v, _bit) clear_bit((_bit), (_v)->bitmap)
#define ubr_vec_test(_v, _bit)  test_bit((_bit), (_v)->bitmap)

#define ubr_vec_foreach(_v, _bit) \
	for_each_set_bit(_bit, (_v)->bitmap, UBR_MAX_PORTS)


/* Control buffer */
struct ubr_cb {
	/* Ingress port index */
	u32 pidx:UBR_MAX_PORTS_SHIFT;

	/*
	 * Ingress filter results.  Setting these in a port's ingress_cb
	 * _disables_ the corresponding filter, i.e. the result is
	 * always treated as OK for all packets ingressing on that port.
	 */
	u32 vlan_ok:1;
	u32 stg_ok:1;
	u32 sa_ok:1;

	u32 sa_learning:1;

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

struct ubr_fdb_flush_op {
	u32 per_port:1;
	u32 pidx:UBR_MAX_PORTS_SHIFT;

	u32 per_vlan:1;
	u32 vid:16;

	u32 per_proto:1;
	u32 proto:8;

	u32 old:1;
};

enum ubr_addr_type {
	UBR_ADDR_MAC,
	UBR_ADDR_IP4,
	UBR_ADDR_IP6,
};

struct ubr_fdb_addr {
	u32 type:3;
	u32 vid:16;

	union {
		u8 mac[ETH_ALEN];
		__be32 ip4;
#if IS_ENABLED(CONFIG_IPV6)
		struct in6_addr ip6;
#endif
	};
};

enum ubr_fdb_proto {
	UBR_FDB_DYNAMIC,
	UBR_FDB_EXTERNAL,
	UBR_FDB_USER,
};

struct ubr_fdb_node {
	struct rhash_head rhnode;

	struct ubr_fdb_addr addr;
	struct ubr_vec vec;

	u32 proto:8;
	u32 offloaded:1;

	/* TODO on separate cache line like bridge? */
	unsigned long tstamp;

	struct rcu_head rcu;
};

struct ubr_fdb {
	struct rhashtable nodes;
	unsigned long ageing_timeout;
	struct delayed_work age_work;
};

struct ubr_vlan {
	struct ubr *ubr;
	struct hlist_node node;
	u16 vid;

	unsigned sa_learning:1;

	struct ubr_vec members;
	struct ubr_vec tagged;

	struct ubr_vec routers;
	struct ubr_vec mcflood;
	struct ubr_vec bcflood;
	struct ubr_vec ucflood;

	/* struct ubr_stp *stp; */

	struct rcu_head rcu;
};

/*
 * Note: While PVID is a VLAN which does not (yet) exist in the bridge,
 *       the ingress.cb->vlan is NULL.  This is a poor mans filtering
 *       for VLAN trunk ports.  When the VLAN is added, or removed, all
 *       ports with a PVID matching that VLAN must be updated.
 */
struct ubr_port {
	struct net_device *dev;
	struct ubr_cb ingress_cb;
	u16 pvid;

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

struct ubr_sks {
	atomic_t ieee_groups;
	atomic_t udp_dports;
	atomic_t eth_types;
};

struct ubr {
	struct net_device *dev;

	struct ubr_vec active;
	u16 vlan_proto;

	struct ubr_sks sks;

	DECLARE_HASHTABLE(vlans, 8);
	DECLARE_HASHTABLE(stps, 8);

	struct ubr_fdb fdb;

	struct ubr_vec  busy;
	struct ubr_port ports[UBR_MAX_PORTS];
};
#define ubr_from_port(_port) \
	container_of((_port), struct ubr, ports[(_port)->ingress_cb.pidx])

/* ubr-dev.c */
void ubr_update_headroom(struct ubr *ubr, struct net_device *new_dev);
struct ubr *ubr_from_dev(struct net_device *dev);


/* ubr-fdb.c */
void ubr_fdb_forward(struct ubr_fdb *fdb, struct sk_buff *skb);

int  ubr_fdb_newlink(struct ubr_fdb *fdb);
void ubr_fdb_dellink(struct ubr_fdb *fdb);

int __init ubr_fdb_cache_init(void);
void       ubr_fdb_cache_fini(void);

int ubr_fdb_nl_flush_cmd(struct sk_buff *skb, struct genl_info *info);


/* ubr-forward.c */
void ubr_forward(struct ubr *ubr, struct sk_buff *skb);


/* ubr-netlink.c */
int ubr_netlink_init(const struct net_device_ops *ops);
int ubr_netlink_exit(void);

struct net_device *ubr_netlink_dev(struct genl_info *info);


/* ubr-port.c */
struct ubr_port *ubr_port_init(struct ubr *ubr, unsigned idx, struct net_device *dev);

int ubr_port_add(struct ubr *ubr, struct net_device *dev,
		 struct netlink_ext_ack *extack);
int ubr_port_del(struct ubr *ubr, struct net_device *dev);

int ubr_port_find(struct ubr *ubr, struct net_device *dev);

int ubr_port_nl_set_cmd(struct sk_buff *skb, struct genl_info *info);


/* ubr-sk.c */
bool ubr_sk_ingress(struct ubr *ubr, struct sk_buff *skb);

int __init  ubr_sk_init(void);
void __exit ubr_sk_fini(void);


/* ubr-vlan.c */
bool ubr_vlan_ingress(struct ubr *ubr, struct sk_buff *skb);

int ubr_vlan_port_add(struct ubr_vlan *vlan, unsigned idx, bool tagged);
int ubr_vlan_port_del(struct ubr_vlan *vlan, unsigned idx);

struct ubr_vlan *ubr_vlan_find(struct ubr *ubr, u16 vid);
int              ubr_vlan_del (struct ubr_vlan *vlan);
struct ubr_vlan *ubr_vlan_new (struct ubr *ubr, u16 vid, u16 fid, u16 sid);

int  ubr_vlan_newlink(struct ubr *ubr);
void ubr_vlan_dellink(struct ubr *ubr);

int ubr_vlan_nl_add_cmd(struct sk_buff *skb, struct genl_info *info);
int ubr_vlan_nl_del_cmd(struct sk_buff *skb, struct genl_info *info);
int ubr_vlan_nl_set_cmd(struct sk_buff *skb, struct genl_info *info);

int ubr_vlan_nl_attach_cmd(struct sk_buff *skb, struct genl_info *info);
int ubr_vlan_nl_detach_cmd(struct sk_buff *skb, struct genl_info *info);

#endif	/* __UBR_PRIVATE_H */
