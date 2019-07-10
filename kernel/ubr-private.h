#ifndef __UBR_PRIVATE_H
#define __UBR_PRIVATE_H

#include <linux/bitmap.h>


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

/* struct ubr_vlan { */
/* 	struct rhash_head rhnode; */
/* 	u16 vid; */

/* 	struct ubr_portvec member; */
/* 	struct ubr_portvec tagged; */
/* 	struct ubr_portvec allmulti; */

/* 	struct ubr_fdb *fdb; */
/* 	struct ubr_sdb *sdb; */
/* }; */

/* enum ubr_vlan_mode { */
/* 	UBR_VLAN_TRANSPARENT, */
/* 	UBR_VLAN_SECURE, */
/* }; */

struct ubr_switchdev_domain {
	struct ubr_v *members;
};

struct ubr_port {
	struct ubr *ubr;
	int id;

	/* struct ubr_vlan *pvlan; */
	/* enum ubr_vlan_mode vlan_mode:1; */

	struct rcu_head rcu;
};

struct ubr {
	struct net_device *dev;

	struct net_device **port_devs;
	struct ubr_port* ports;
	size_t ports_max;
	size_t ports_now;

	unsigned long *ports_busy;
	/* struct rhashtable *vlans; */
	/* struct rhashtable *fdbs; */
	/* struct rhashtable *stpdbs; */
};

#define ubr_foreach(_ubr, _mask, _id) \
	for_each_set_bit(_id, (_mask), (_ubr)->ports_max)

#define ubr_port_foreach(_ubr, _id) \
	ubr_foreach(_ubr, (_ubr)->ports_busy, _id)

static inline unsigned long *ubr_mask_alloc(struct ubr *ubr)
{
	return bitmap_zalloc(ubr->ports_max, 0);
}

static inline void ubr_mask_free(unsigned long *mask)
{
	bitmap_free(mask);
}


/* ubr-dev.c */
void ubr_update_headroom(struct ubr *ubr, struct net_device *new_dev);

/* ubr-port.c */
struct ubr_port *ubr_port_init(struct ubr *ubr, int id, struct net_device *dev);

int ubr_port_add(struct ubr *ubr, struct net_device *dev,
		 struct netlink_ext_ack *extack);
int ubr_port_del(struct ubr *ubr, struct net_device *dev);

#endif	/* __UBR_PRIVATE_H */
