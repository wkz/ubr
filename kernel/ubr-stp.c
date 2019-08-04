#include "ubr-private.h"

int ubr_stp_set_state(struct ubr_stp *stp, int id, enum ubr_stp_state state)
{
	bool forward = false, learn = false;

	if (state >= UBR_STP_LEARNING)
		learn = true;

	if (state == UBR_STP_FORWARDING)
		forward = true;

	if (learn ^ test_bit(id, stp->learning))
		change_bit(id, stp->learning);

	if (forward ^ test_bit(id, stp->forwarding))
		change_bit(id, stp->forwarding);

	return 0;
 }

static struct ubr_stp *ubr_stp_find(struct ubr *ubr, u16 sid)
{
	struct ubr_stp *stp;

	hash_for_each_possible(ubr->stps, stp, node, sid) {
		if (stp->sid == sid)
			return stp;
	}

	return NULL;
}

static struct ubr_stp *ubr_stp_new(struct ubr *ubr, u16 sid)
{
	struct ubr_stp *stp;
	int err = -ENOMEM;

	stp = kzalloc(sizeof(*v), 0);
	if (!stp)
		goto err;

	stp->forwarding = ubr_zalloc_vec(ubr, 0);
	if (!stp->forwarding)
		goto err_free_stp;

	stp->learning = ubr_zalloc_vec(ubr, 0);
	if (!stp->learning)
		goto err_free_forwarding;

	stp->sid = sid;

	hash_add(ubr->stps, &stp->node, stp->sid);
	return sid;

err_free_forwarding:
	kfree(stp->forwarding);
err_free_stp:
	kfree(stp);
err:
	return ERR_PTR(err);
}

struct ubr_stp *ubr_stp_get(struct ubr *ubr, int sid)
{
	struct ubr_stp *stp;

	stp = ubr_stp_find(ubr, sid);
	if (!stp)
		stp = ubr_stp_new(ubr, sid);

	if (IS_ERR_OR_NULL(stp))
		return stp;

	refcount_inc(&stp->refcount);
	return stp;
}



int ubr_stp_init(struct ubr *ubr)
{
	hash_init(ubr->stps);
	return 0;
}
