/*	$OpenBSD: if_etherbridge.c,v 1.1 2021/02/21 03:26:46 dlg Exp $ */

/*
 * Copyright (c) 2018, 2021 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/timeout.h>
#include <sys/pool.h>
#include <sys/tree.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/rtable.h>
#include <net/toeplitz.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

/* for bridge stuff */
#include <net/if_bridge.h>

#include <net/if_etherbridge.h>

static inline void	ebe_take(struct eb_entry *);
static inline void	ebe_rele(struct eb_entry *);
static void		ebe_free(void *);

static void		etherbridge_age(void *);

RBT_PROTOTYPE(eb_tree, eb_entry, ebe_tentry, ebt_cmp);

static struct pool	eb_entry_pool;

static inline int
eb_port_eq(struct etherbridge *eb, void *a, void *b)
{
	return ((*eb->eb_ops->eb_op_port_eq)(eb->eb_cookie, a, b));
}

static inline void *
eb_port_take(struct etherbridge *eb, void *port)
{
	return ((*eb->eb_ops->eb_op_port_take)(eb->eb_cookie, port));
}

static inline void
eb_port_rele(struct etherbridge *eb, void *port)
{
	return ((*eb->eb_ops->eb_op_port_rele)(eb->eb_cookie, port));
}

static inline size_t
eb_port_ifname(struct etherbridge *eb, char *dst, size_t len, void *port)
{
	return ((*eb->eb_ops->eb_op_port_ifname)(eb->eb_cookie, dst, len,
	    port));
}

static inline void
eb_port_sa(struct etherbridge *eb, struct sockaddr_storage *ss, void *port)
{
	(*eb->eb_ops->eb_op_port_sa)(eb->eb_cookie, ss, port);
}

int
etherbridge_init(struct etherbridge *eb, const char *name,
    const struct etherbridge_ops *ops, void *cookie)
{
	size_t i;

	if (eb_entry_pool.pr_size == 0) {
		pool_init(&eb_entry_pool, sizeof(struct eb_entry),
		    0, IPL_SOFTNET, 0, "ebepl", NULL);
	}

	eb->eb_table = mallocarray(ETHERBRIDGE_TABLE_SIZE,
	    sizeof(*eb->eb_table), M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (eb->eb_table == NULL)
		return (ENOMEM);

	eb->eb_name = name;
	eb->eb_ops = ops;
	eb->eb_cookie = cookie;

	mtx_init(&eb->eb_lock, IPL_SOFTNET);
	RBT_INIT(eb_tree, &eb->eb_tree);

	eb->eb_num = 0;
	eb->eb_max = 100;
	eb->eb_max_age = 240;
	timeout_set(&eb->eb_tmo_age, etherbridge_age, eb);

	for (i = 0; i < ETHERBRIDGE_TABLE_SIZE; i++) {
		struct eb_list *ebl = &eb->eb_table[i];
		SMR_TAILQ_INIT(ebl);
	}

	return (0);
}

int
etherbridge_up(struct etherbridge *eb)
{
	etherbridge_age(eb);

	return (0);
}

int
etherbridge_down(struct etherbridge *eb)
{
	smr_barrier();

	return (0);
}

void
etherbridge_destroy(struct etherbridge *eb)
{
	struct eb_entry *ebe, *nebe;

	/* XXX assume that nothing will calling etherbridge_map now */

	timeout_del_barrier(&eb->eb_tmo_age);

	free(eb->eb_table, M_DEVBUF,
	    ETHERBRIDGE_TABLE_SIZE * sizeof(*eb->eb_table));

	RBT_FOREACH_SAFE(ebe, eb_tree, &eb->eb_tree, nebe) {
		RBT_REMOVE(eb_tree, &eb->eb_tree, ebe);
		ebe_free(ebe);
	}
}

static struct eb_list *
etherbridge_list(struct etherbridge *eb, const struct ether_addr *ea)
{
	uint16_t hash = stoeplitz_eaddr(ea->ether_addr_octet);
	hash &= ETHERBRIDGE_TABLE_MASK;
	return (&eb->eb_table[hash]);
}

static struct eb_entry *
ebl_find(struct eb_list *ebl, const struct ether_addr *ea)
{
	struct eb_entry *ebe;

	SMR_TAILQ_FOREACH(ebe, ebl, ebe_lentry) {
		if (ETHER_IS_EQ(ea, &ebe->ebe_addr))
			return (ebe);
	}

	return (NULL);
}

static inline void
ebl_insert(struct eb_list *ebl, struct eb_entry *ebe)
{
	SMR_TAILQ_INSERT_TAIL_LOCKED(ebl, ebe, ebe_lentry);
}

static inline void
ebl_remove(struct eb_list *ebl, struct eb_entry *ebe)
{
	SMR_TAILQ_REMOVE_LOCKED(ebl, ebe, ebe_lentry);
}

static inline int
ebt_cmp(const struct eb_entry *aebe, const struct eb_entry *bebe)
{
	return (memcmp(&aebe->ebe_addr, &bebe->ebe_addr,
	    sizeof(aebe->ebe_addr)));
}

RBT_GENERATE(eb_tree, eb_entry, ebe_tentry, ebt_cmp);

static inline struct eb_entry *
ebt_insert(struct etherbridge *eb, struct eb_entry *ebe)
{
	return (RBT_INSERT(eb_tree, &eb->eb_tree, ebe));
}

static inline void
ebt_replace(struct etherbridge *eb, struct eb_entry *oebe,
    struct eb_entry *nebe)
{
	struct eb_entry *rvebe;

	RBT_REMOVE(eb_tree, &eb->eb_tree, oebe);
	rvebe = RBT_INSERT(eb_tree, &eb->eb_tree, nebe);
	KASSERTMSG(rvebe == NULL, "ebt_replace eb %p nebe %p rvebe %p",
	    eb, nebe, rvebe);
}

static inline void
ebt_remove(struct etherbridge *eb, struct eb_entry *ebe)
{
	RBT_REMOVE(eb_tree, &eb->eb_tree, ebe);
}

static inline void
ebe_take(struct eb_entry *ebe)
{
	refcnt_take(&ebe->ebe_refs);
}

static void
ebe_rele(struct eb_entry *ebe)
{
	if (refcnt_rele(&ebe->ebe_refs))
		smr_call(&ebe->ebe_smr_entry, ebe_free, ebe);
}

static void
ebe_free(void *arg)
{
	struct eb_entry *ebe = arg;
	struct etherbridge *eb = ebe->ebe_etherbridge;

	eb_port_rele(eb, ebe->ebe_port);
	pool_put(&eb_entry_pool, ebe);
}

void *
etherbridge_resolve(struct etherbridge *eb, const struct ether_addr *ea)
{
	struct eb_list *ebl = etherbridge_list(eb, ea);
	struct eb_entry *ebe;

	SMR_ASSERT_CRITICAL();

	ebe = ebl_find(ebl, ea);
	if (ebe != NULL) {
		if (ebe->ebe_type == EBE_DYNAMIC) {
			int diff = getuptime() - ebe->ebe_age;
			if (diff > eb->eb_max_age)
				return (NULL);
		}

		return (ebe->ebe_port);
	}

	return (NULL);
}

void
etherbridge_map(struct etherbridge *eb, void *port,
    const struct ether_addr *ea)
{
	struct eb_list *ebl;
	struct eb_entry *oebe, *nebe;
	unsigned int num;
	void *nport;
	int new = 0;

	if (ETHER_IS_MULTICAST(ea->ether_addr_octet) ||
	    ETHER_IS_EQ(ea->ether_addr_octet, etheranyaddr))
		return;

	ebl = etherbridge_list(eb, ea);

	smr_read_enter();
	oebe = ebl_find(ebl, ea);
	if (oebe == NULL)
		new = 1;
	else {
		oebe->ebe_age = getuptime();

		/* does this entry need to be replaced? */
		if (oebe->ebe_type == EBE_DYNAMIC &&
		    !eb_port_eq(eb, oebe->ebe_port, port)) {
			new = 1;
			ebe_take(oebe);
		} else
			oebe = NULL;
	}
	smr_read_leave();

	if (!new)
		return;

	nport = eb_port_take(eb, port);
	if (nport == NULL) {
		/* XXX should we remove the old one and flood? */
		return;
	}

	nebe = pool_get(&eb_entry_pool, PR_NOWAIT);
	if (nebe == NULL) {
		/* XXX should we remove the old one and flood? */
		eb_port_rele(eb, nport);
		return;
	}

	smr_init(&nebe->ebe_smr_entry);
	refcnt_init(&nebe->ebe_refs);
	nebe->ebe_etherbridge = eb;

	nebe->ebe_addr = *ea;
	nebe->ebe_port = nport;
	nebe->ebe_type = EBE_DYNAMIC;
	nebe->ebe_age = getuptime();

	mtx_enter(&eb->eb_lock);
	num = eb->eb_num + (oebe == NULL);
	if (num <= eb->eb_max && ebt_insert(eb, nebe) == oebe) {
		/* we won, do the update */
		ebl_insert(ebl, nebe);

		if (oebe != NULL) {
			ebl_remove(ebl, oebe);
			ebt_replace(eb, oebe, nebe);

			/* take the table reference away */
			if (refcnt_rele(&oebe->ebe_refs)) {
				panic("%s: eb %p oebe %p refcnt",
				    __func__, eb, oebe);
			}
		}

		nebe = NULL;
		eb->eb_num = num;
	}
	mtx_leave(&eb->eb_lock);

	if (nebe != NULL) {
		/*
		 * the new entry didnt make it into the
		 * table, so it can be freed directly.
		 */
		ebe_free(nebe);
	}

	if (oebe != NULL) {
		/*
		 * the old entry could be referenced in
		 * multiple places, including an smr read
		 * section, so release it properly.
		 */
		ebe_rele(oebe);
	}
}

static void
etherbridge_age(void *arg)
{
	struct etherbridge *eb = arg;
	struct eb_entry *ebe, *nebe;
	struct eb_queue ebq = TAILQ_HEAD_INITIALIZER(ebq);
	int diff;
	unsigned int now = getuptime();
	size_t i;

	timeout_add_sec(&eb->eb_tmo_age, 100);

	for (i = 0; i < ETHERBRIDGE_TABLE_SIZE; i++) {
		struct eb_list *ebl = &eb->eb_table[i];
#if 0
		if (SMR_TAILQ_EMPTY(ebl));
			continue;
#endif

		mtx_enter(&eb->eb_lock); /* don't block map too much */
		SMR_TAILQ_FOREACH_SAFE_LOCKED(ebe, ebl, ebe_lentry, nebe) {
			if (ebe->ebe_type != EBE_DYNAMIC)
				continue;

			diff = now - ebe->ebe_age;
			if (diff < eb->eb_max_age)
				continue;

			ebl_remove(ebl, ebe);
			ebt_remove(eb, ebe);
			eb->eb_num--;

			/* we own the tables ref now */

			TAILQ_INSERT_TAIL(&ebq, ebe, ebe_qentry);
		}
		mtx_leave(&eb->eb_lock);
	}

	TAILQ_FOREACH_SAFE(ebe, &ebq, ebe_qentry, nebe) {
		TAILQ_REMOVE(&ebq, ebe, ebe_qentry);
		ebe_rele(ebe);
	}
}

void
etherbridge_detach_port(struct etherbridge *eb, void *port)
{
	struct eb_entry *ebe, *nebe;
	struct eb_queue ebq = TAILQ_HEAD_INITIALIZER(ebq);
	size_t i;

	for (i = 0; i < ETHERBRIDGE_TABLE_SIZE; i++) {
		struct eb_list *ebl = &eb->eb_table[i];

		mtx_enter(&eb->eb_lock); /* don't block map too much */
		SMR_TAILQ_FOREACH_SAFE_LOCKED(ebe, ebl, ebe_lentry, nebe) {
			if (!eb_port_eq(eb, ebe->ebe_port, port))
				continue;

			ebl_remove(ebl, ebe);
			ebt_remove(eb, ebe);
			eb->eb_num--;

			/* we own the tables ref now */

			TAILQ_INSERT_TAIL(&ebq, ebe, ebe_qentry);
		}
		mtx_leave(&eb->eb_lock);
	}

	smr_barrier(); /* try and do it once for all the entries */

	TAILQ_FOREACH_SAFE(ebe, &ebq, ebe_qentry, nebe) {
		TAILQ_REMOVE(&ebq, ebe, ebe_qentry);
		if (refcnt_rele(&ebe->ebe_refs))
			ebe_free(ebe);
	}
}

void
etherbridge_flush(struct etherbridge *eb, uint32_t flags)
{
	struct eb_entry *ebe, *nebe;
	struct eb_queue ebq = TAILQ_HEAD_INITIALIZER(ebq);
	size_t i;

	for (i = 0; i < ETHERBRIDGE_TABLE_SIZE; i++) {
		struct eb_list *ebl = &eb->eb_table[i];

		mtx_enter(&eb->eb_lock); /* don't block map too much */
		SMR_TAILQ_FOREACH_SAFE_LOCKED(ebe, ebl, ebe_lentry, nebe) {
			if (flags == IFBF_FLUSHDYN &&
			    ebe->ebe_type != EBE_DYNAMIC)
				continue;

			ebl_remove(ebl, ebe);
			ebt_remove(eb, ebe);
			eb->eb_num--;

			/* we own the tables ref now */

			TAILQ_INSERT_TAIL(&ebq, ebe, ebe_qentry);
		}
		mtx_leave(&eb->eb_lock);
	}

	smr_barrier(); /* try and do it once for all the entries */

	TAILQ_FOREACH_SAFE(ebe, &ebq, ebe_qentry, nebe) {
		TAILQ_REMOVE(&ebq, ebe, ebe_qentry);
		if (refcnt_rele(&ebe->ebe_refs))
			ebe_free(ebe);
	}
}

int
etherbridge_rtfind(struct etherbridge *eb, struct ifbaconf *baconf)
{
	struct eb_entry *ebe;
	struct ifbareq bareq;
	caddr_t buf;
	size_t len, nlen;
	time_t age, now = getuptime();
	int error;

	if (baconf->ifbac_len == 0) {
		/* single read is atomic */
		baconf->ifbac_len = eb->eb_num * sizeof(bareq);
		return (0);
	}

	buf = malloc(baconf->ifbac_len, M_TEMP, M_WAITOK|M_CANFAIL);
	if (buf == NULL)
		return (ENOMEM);
	len = 0;

	mtx_enter(&eb->eb_lock);
	RBT_FOREACH(ebe, eb_tree, &eb->eb_tree) {
		nlen = len + sizeof(bareq);
		if (nlen > baconf->ifbac_len)
			break;

		strlcpy(bareq.ifba_name, eb->eb_name,
		    sizeof(bareq.ifba_name));
		eb_port_ifname(eb,
		    bareq.ifba_ifsname, sizeof(bareq.ifba_ifsname),
		    ebe->ebe_port);
		memcpy(&bareq.ifba_dst, &ebe->ebe_addr,
		    sizeof(bareq.ifba_dst));

		memset(&bareq.ifba_dstsa, 0, sizeof(bareq.ifba_dstsa));
		eb_port_sa(eb, &bareq.ifba_dstsa, ebe->ebe_port);

		switch (ebe->ebe_type) {
		case EBE_DYNAMIC:
			age = now - ebe->ebe_age;
			bareq.ifba_age = MIN(age, 0xff);
			bareq.ifba_flags = IFBAF_DYNAMIC;
			break;
		case EBE_STATIC:
			bareq.ifba_age = 0;
			bareq.ifba_flags = IFBAF_STATIC;
			break;
		}

		memcpy(buf + len, &bareq, sizeof(bareq));
                len = nlen;
        }
	nlen = baconf->ifbac_len;
	baconf->ifbac_len = eb->eb_num * sizeof(bareq);
	mtx_leave(&eb->eb_lock);

	error = copyout(buf, baconf->ifbac_buf, len);
	free(buf, M_TEMP, nlen);

        return (error);
}

int
etherbridge_set_max(struct etherbridge *eb, struct ifbrparam *bparam)
{
	if (bparam->ifbrp_csize < 1 ||
	    bparam->ifbrp_csize > 4096) /* XXX */
		return (EINVAL);

	/* commit */
	eb->eb_max = bparam->ifbrp_csize;

	return (0);
}

int
etherbridge_get_max(struct etherbridge *eb, struct ifbrparam *bparam)
{
	bparam->ifbrp_csize = eb->eb_max;

	return (0);
}

int
etherbridge_set_tmo(struct etherbridge *eb, struct ifbrparam *bparam)
{
	if (bparam->ifbrp_ctime < 8 ||
	    bparam->ifbrp_ctime > 3600)
		return (EINVAL);

	/* commit */
	eb->eb_max_age = bparam->ifbrp_ctime;

	return (0);
}

int
etherbridge_get_tmo(struct etherbridge *eb, struct ifbrparam *bparam)
{
	bparam->ifbrp_ctime = eb->eb_max_age;

	return (0);
}
