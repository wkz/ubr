
#include <net/sock.h>

#include "ubr-private.h"


struct ubr_sk {
	struct sock sk;

	struct ubr *ubr;
	enum sockaddr_ubr_proto proto;
	union {
		__u8   ieee_group;
		__be16 eth_type;
		__be16 udp_dport;
	} filter;
};

static inline struct ubr_sk *ubr_sk(struct sock *sk)
{
	return (struct ubr_sk *)sk;
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

	if (len < sizeof(*subr) || subr->subr_family != PF_UBR)
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
	usk->proto = (__force __be16)protocol;

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
