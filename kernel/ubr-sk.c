#include <linux/ip.h>
#include <linux/udp.h>

#include <net/sock.h>

#include "ubr-private.h"


struct ubr_sk {
	struct sock sk;

	struct ubr *ubr;
	struct sockaddr_ubr_proto proto;
};

static inline struct ubr_sk *ubr_sk(struct sock *sk)
{
	return (struct ubr_sk *)sk;
}

static void ubr_sk_rcv(struct ubr_sk *usk, struct sk_buff *skb)
{
	/* TODO */
	kfree_skb(skb);
}

static inline bool ubr_sk_eligible(struct ubr_sk *usk, struct sk_buff *skb)
{
	struct ubr_cb *cb = ubr_cb(skb);
	u32 flags = usk->proto.flags;

	return  (cb->vlan_ok || (flags & UBR_FLAG_BYPASS_VLAN)) &&
		(cb->stg_ok  || (flags & UBR_FLAG_BYPASS_STG)) &&
		(cb->sa_ok   || (flags & UBR_FLAG_BYPASS_SA));
		
}

struct ubr_sk *ubr_sk_lookup(struct ubr *ubr, enum sockaddr_ubr_type type, u32 key)
{
	/* TODO */
	return NULL;
}

bool ubr_sk_ingress(struct ubr *ubr, struct sk_buff *skb)
{
	struct ethhdr *eth = eth_hdr(skb);	
	struct ubr_sk *usk;

	if (skb->pkt_type == PACKET_MULTICAST &&
	    atomic_read(&ubr->sks.ieee_groups) &&
	    is_link_local_ether_addr(eth->h_dest)) {
		usk = ubr_sk_lookup(ubr, UBR_TYPE_IEEE_GROUP, eth->h_dest[5]);
		if (!usk)
			/* IEEE Multicast can't possibly match any
			 * other socket. */
			return false;
	} else if (skb->pkt_type == PACKET_BROADCAST &&
		   atomic_read(&ubr->sks.udp_dports) &&
		   skb->protocol == htons(ETH_P_IP) &&
		   ip_hdr(skb)->protocol == IPPROTO_UDP) {
		usk = ubr_sk_lookup(ubr, UBR_TYPE_IPV4_UDP_BC,
				    udp_hdr(skb)->dest);
	}

	if (!usk && atomic_read(&ubr->sks.eth_types))
		usk = ubr_sk_lookup(ubr, UBR_TYPE_ETH_TYPE, skb->protocol);

	if (!usk || !ubr_sk_eligible(usk, skb))
		return false;
	
	ubr_sk_rcv(usk, skb);
	return true;
}

static int ubr_sk_enable(struct ubr_sk *usk)
{
	return 0;
}

static int ubr_sk_bind(struct socket *sock, struct sockaddr *addr,
		       int addr_len)
{
	struct sockaddr_ubr *subr = (struct sockaddr_ubr *)addr;
	struct net_device *dev;
	struct sock *sk = sock->sk;
	struct ubr_sk *usk = ubr_sk(sk);
	struct ubr *ubr;
	int err = 0;

	if (addr_len < sizeof(*subr) || subr->subr_family != PF_UBR)
		return -EINVAL;

	dev = dev_get_by_index(sock_net(sk), subr->subr_ifindex);
	if (!dev)
		return -ENODEV;

	ubr = ubr_from_dev(dev);
	if (!ubr)
		return -ENODEV;

	lock_sock(sk);

	if (usk->ubr) {
		err = -EBUSY;
		goto unlock;
	}

	usk->ubr = ubr;
	usk->proto = subr->subr_proto;
	err = ubr_sk_enable(usk);
	if (err)
		goto unlock;

	return 0;

unlock:
	release_sock(sk);
	return err;
}

static int ubr_sk_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (!sk)
		return 0;

	sock_orphan(sk);
	release_sock(sk);
	sock_put(sk);
	return 0;
}

static const struct proto_ops ubr_sk_ops = {
	.family     = PF_UBR,
	.owner      = THIS_MODULE,

	.bind       = ubr_sk_bind,
	.release    = ubr_sk_release,
	/* .getname    = ubr_sk_getname, */

	/* .ioctl      = ubr_sk_ioctl, */
	/* .setsockopt = ubr_sk_setsockopt, */
	/* .getsockopt = ubr_sk_getsockopt, */

	/* .poll       = ubr_sk_poll, */
	/* .recvmsg    = ubr_sk_recvmsg, */
	/* .sendmsg    = ubr_sk_sendmsg, */

	.accept     = sock_no_accept,
	.connect    = sock_no_connect,
	.gettstamp  = sock_gettstamp,
	.listen     = sock_no_listen,
	.sendpage   = sock_no_sendpage,
	.shutdown   = sock_no_shutdown,
	.socketpair = sock_no_socketpair,
};

static struct proto ubr_sk_proto = {
	.name	  = "ubr",
	.owner	  = THIS_MODULE,
	.obj_size = sizeof(struct ubr_sk),
};

static int ubr_sk_create(struct net *net, struct socket *sock, int protocol,
			 int kern)
{
	struct sock *sk;
	struct ubr_sk *usk;

	if (!ns_capable(net->user_ns, CAP_NET_RAW))
		return -EPERM;

	if (sock->type != SOCK_RAW)
		return -ESOCKTNOSUPPORT;

	sock->state = SS_UNCONNECTED;

	sk = sk_alloc(net, PF_UBR, GFP_KERNEL, &ubr_sk_proto, kern);
	if (sk == NULL)
		return -ENOBUFS;

	sock->ops = &ubr_sk_ops;
	sock_init_data(sock, sk);

	usk = (struct ubr_sk *)sk;
	/* init_completion(&po->skb_completion); */
	sk->sk_family = PF_UBR;

	/* err = ubr_sk_alloc_pending(po); */
	/* if (err) */
	/* 	goto out2; */

	/* ubr_sk_cached_dev_reset(po); */

	/* sk->sk_destruct = ubr_sk_sock_destruct; */
	sk_refcnt_debug_inc(sk);

	/* spin_lock_init(&po->bind_lock); */
	/* mutex_init(&po->pg_vec_lock); */
	/* po->rollover = NULL; */
	/* po->prot_hook.func = ubr_sk_rcv; */
	/* po->prot_hook.af_ubr_sk_priv = sk; */

	/* if (proto) { */
	/* 	po->prot_hook.type = proto; */
	/* 	__register_prot_hook(sk); */
	/* } */

	/* mutex_lock(&net->ubr_sk.sklist_lock); */
	/* sk_add_node_tail_rcu(sk, &net->ubr_sk.sklist); */
	/* mutex_unlock(&net->ubr_sk.sklist_lock); */

	preempt_disable();
	sock_prot_inuse_add(net, &ubr_sk_proto, 1);
	preempt_enable();
	return 0;
}

static const struct net_proto_family ubr_sk_family_ops = {
	.family = PF_UBR,
	.owner  = THIS_MODULE,
	.create = ubr_sk_create,
};

int __init ubr_sk_init(void)
{
	int err;

	err = proto_register(&ubr_sk_proto, 0);
	if (err)
		goto err;

	err = sock_register(&ubr_sk_family_ops);
	if (err)
		goto err_unreg_proto;
	return 0;

err_unreg_proto:
	proto_unregister(&ubr_sk_proto);
err:
	return err;
}

void __exit ubr_sk_fini(void)
{
	sock_unregister(PF_UBR);
	proto_unregister(&ubr_sk_proto);
}
