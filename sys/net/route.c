/*	$OpenBSD: route.c,v 1.375 2018/06/11 08:48:54 mpi Exp $	*/
/*	$NetBSD: route.c,v 1.14 1996/02/13 22:00:46 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1980, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)route.c	8.2 (Berkeley) 11/15/93
 */

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed at the Information
 *	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/timeout.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/pool.h>
#include <sys/atomic.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#endif

#ifdef BFD
#include <net/bfd.h>
#endif

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

/* Give some jitter to hash, to avoid synchronization between routers. */
static uint32_t		rt_hashjitter;

extern unsigned int	rtmap_limit;

struct cpumem *		rtcounters;
int			rttrash;	/* routes not in table but not freed */
int			ifatrash;	/* ifas not in ifp list but not free */

struct pool		rtentry_pool;	/* pool for rtentry structures */
struct pool		rttimer_pool;	/* pool for rttimer structures */

void	rt_timer_init(void);
int	rt_setgwroute(struct rtentry *, u_int);
void	rt_putgwroute(struct rtentry *);
int	rtflushclone1(struct rtentry *, void *, u_int);
void	rtflushclone(unsigned int, struct rtentry *);
int	rt_ifa_purge_walker(struct rtentry *, void *, unsigned int);
struct rtentry *rt_match(struct sockaddr *, uint32_t *, int, unsigned int);
int	rt_clone(struct rtentry **, struct sockaddr *, unsigned int);
struct sockaddr *rt_plentosa(sa_family_t, int, struct sockaddr_in6 *);

#ifdef DDB
void	db_print_sa(struct sockaddr *);
void	db_print_ifa(struct ifaddr *);
int	db_show_rtentry(struct rtentry *, void *, unsigned int);
#endif

#define	LABELID_MAX	50000

struct rt_label {
	TAILQ_ENTRY(rt_label)	rtl_entry;
	char			rtl_name[RTLABEL_LEN];
	u_int16_t		rtl_id;
	int			rtl_ref;
};

TAILQ_HEAD(rt_labels, rt_label)	rt_labels = TAILQ_HEAD_INITIALIZER(rt_labels);

void
route_init(void)
{
	rtcounters = counters_alloc(rts_ncounters);

	pool_init(&rtentry_pool, sizeof(struct rtentry), 0, IPL_SOFTNET, 0,
	    "rtentry", NULL);

	while (rt_hashjitter == 0)
		rt_hashjitter = arc4random();

#ifdef BFD
	bfdinit();
#endif
}

/*
 * Returns 1 if the (cached) ``rt'' entry is still valid, 0 otherwise.
 */
int
rtisvalid(struct rtentry *rt)
{
	if (rt == NULL)
		return (0);

	if (!ISSET(rt->rt_flags, RTF_UP))
		return (0);

	if (ISSET(rt->rt_flags, RTF_GATEWAY)) {
		KASSERT(rt->rt_gwroute != NULL);
		KASSERT(!ISSET(rt->rt_gwroute->rt_flags, RTF_GATEWAY));
		if (!ISSET(rt->rt_gwroute->rt_flags, RTF_UP))
			return (0);
	}

	return (1);
}

/*
 * Do the actual lookup for rtalloc(9), do not use directly!
 *
 * Return the best matching entry for the destination ``dst''.
 *
 * "RT_RESOLVE" means that a corresponding L2 entry should
 *   be added to the routing table and resolved (via ARP or
 *   NDP), if it does not exist.
 */
struct rtentry *
rt_match(struct sockaddr *dst, uint32_t *src, int flags, unsigned int tableid)
{
	struct rtentry		*rt = NULL;

	rt = rtable_match(tableid, dst, src);
	if (rt == NULL) {
		rtstat_inc(rts_unreach);
		return (NULL);
	}

	if (ISSET(rt->rt_flags, RTF_CLONING) && ISSET(flags, RT_RESOLVE))
		rt_clone(&rt, dst, tableid);

	rt->rt_use++;
	return (rt);
}

int
rt_clone(struct rtentry **rtp, struct sockaddr *dst, unsigned int rtableid)
{
	struct rt_addrinfo	 info;
	struct rtentry		*rt = *rtp;
	int			 error = 0;

	memset(&info, 0, sizeof(info));
	info.rti_info[RTAX_DST] = dst;

	/*
	 * The priority of cloned route should be different
	 * to avoid conflict with /32 cloning routes.
	 *
	 * It should also be higher to let the ARP layer find
	 * cloned routes instead of the cloning one.
	 */
	KERNEL_LOCK();
	error = rtrequest(RTM_RESOLVE, &info, rt->rt_priority - 1, &rt,
	    rtableid);
	KERNEL_UNLOCK();
	if (error) {
		rtm_miss(RTM_MISS, &info, 0, RTP_NONE, 0, error, rtableid);
	} else {
		/* Inform listeners of the new route */
		rtm_send(rt, RTM_ADD, 0, rtableid);
		rtfree(*rtp);
		*rtp = rt;
	}
	return (error);
}

/*
 * Originated from bridge_hash() in if_bridge.c
 */
#define mix(a, b, c) do {						\
	a -= b; a -= c; a ^= (c >> 13);					\
	b -= c; b -= a; b ^= (a << 8);					\
	c -= a; c -= b; c ^= (b >> 13);					\
	a -= b; a -= c; a ^= (c >> 12);					\
	b -= c; b -= a; b ^= (a << 16);					\
	c -= a; c -= b; c ^= (b >> 5);					\
	a -= b; a -= c; a ^= (c >> 3);					\
	b -= c; b -= a; b ^= (a << 10);					\
	c -= a; c -= b; c ^= (b >> 15);					\
} while (0)

int
rt_hash(struct rtentry *rt, struct sockaddr *dst, uint32_t *src)
{
	uint32_t a, b, c;

	if (src == NULL || !rtisvalid(rt) || !ISSET(rt->rt_flags, RTF_MPATH))
		return (-1);

	a = b = 0x9e3779b9;
	c = rt_hashjitter;

	switch (dst->sa_family) {
	case AF_INET:
	    {
		struct sockaddr_in *sin;

		if (!ipmultipath)
			return (-1);

		sin = satosin(dst);
		a += sin->sin_addr.s_addr;
		b += (src != NULL) ? src[0] : 0;
		mix(a, b, c);
		break;
	    }
#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sin6;

		if (!ip6_multipath)
			return (-1);

		sin6 = satosin6(dst);
		a += sin6->sin6_addr.s6_addr32[0];
		b += sin6->sin6_addr.s6_addr32[2];
		c += (src != NULL) ? src[0] : 0;
		mix(a, b, c);
		a += sin6->sin6_addr.s6_addr32[1];
		b += sin6->sin6_addr.s6_addr32[3];
		c += (src != NULL) ? src[1] : 0;
		mix(a, b, c);
		a += sin6->sin6_addr.s6_addr32[2];
		b += sin6->sin6_addr.s6_addr32[1];
		c += (src != NULL) ? src[2] : 0;
		mix(a, b, c);
		a += sin6->sin6_addr.s6_addr32[3];
		b += sin6->sin6_addr.s6_addr32[0];
		c += (src != NULL) ? src[3] : 0;
		mix(a, b, c);
		break;
	    }
#endif /* INET6 */
	}

	return (c & 0xffff);
}

/*
 * Allocate a route, potentially using multipath to select the peer.
 */
struct rtentry *
rtalloc_mpath(struct sockaddr *dst, uint32_t *src, unsigned int rtableid)
{
	return (rt_match(dst, src, RT_RESOLVE, rtableid));
}

/*
 * Look in the routing table for the best matching entry for
 * ``dst''.
 *
 * If a route with a gateway is found and its next hop is no
 * longer valid, try to cache it.
 */
struct rtentry *
rtalloc(struct sockaddr *dst, int flags, unsigned int rtableid)
{
	return (rt_match(dst, NULL, flags, rtableid));
}

/*
 * Cache the route entry corresponding to a reachable next hop in
 * the gateway entry ``rt''.
 */
int
rt_setgwroute(struct rtentry *rt, u_int rtableid)
{
	struct rtentry *prt, *nhrt;
	unsigned int rdomain = rtable_l2(rtableid);
	int error;

	NET_ASSERT_LOCKED();

	KASSERT(ISSET(rt->rt_flags, RTF_GATEWAY));

	/* If we cannot find a valid next hop bail. */
	nhrt = rt_match(rt->rt_gateway, NULL, RT_RESOLVE, rdomain);
	if (nhrt == NULL)
		return (ENOENT);

	/* Next hop entry must be on the same interface. */
	if (nhrt->rt_ifidx != rt->rt_ifidx) {
		struct sockaddr_in6	sa_mask;

		/*
		 * If we found a non-L2 entry on a different interface
		 * there's nothing we can do.
		 */
		if (!ISSET(nhrt->rt_flags, RTF_LLINFO)) {
			rtfree(nhrt);
			return (EHOSTUNREACH);
		}

		/*
		 * We found a L2 entry, so we might have multiple
		 * RTF_CLONING routes for the same subnet.  Query
		 * the first route of the multipath chain and iterate
		 * until we find the correct one.
		 */
		prt = rtable_lookup(rdomain, rt_key(nhrt->rt_parent),
		    rt_plen2mask(nhrt->rt_parent, &sa_mask), NULL, RTP_ANY);
		rtfree(nhrt);

		while (prt != NULL && prt->rt_ifidx != rt->rt_ifidx)
			prt = rtable_iterate(prt);

		/* We found nothing or a non-cloning MPATH route. */
		if (prt == NULL || !ISSET(prt->rt_flags, RTF_CLONING)) {
			rtfree(prt);
			return (EHOSTUNREACH);
		}

		error = rt_clone(&prt, rt->rt_gateway, rdomain);
		if (error) {
			rtfree(prt);
			return (error);
		}
		nhrt = prt;
	}

	/*
	 * Next hop must be reachable, this also prevents rtentry
	 * loops for example when rt->rt_gwroute points to rt.
	 */
	if (ISSET(nhrt->rt_flags, RTF_CLONING|RTF_GATEWAY)) {
		rtfree(nhrt);
		return (ENETUNREACH);
	}

	/* Next hop is valid so remove possible old cache. */
	rt_putgwroute(rt);
	KASSERT(rt->rt_gwroute == NULL);

	/*
	 * If the MTU of next hop is 0, this will reset the MTU of the
	 * route to run PMTUD again from scratch.
	 */
	if (!ISSET(rt->rt_locks, RTV_MTU) && (rt->rt_mtu > nhrt->rt_mtu))
		rt->rt_mtu = nhrt->rt_mtu;

	/*
	 * To avoid reference counting problems when writting link-layer
	 * addresses in an outgoing packet, we ensure that the lifetime
	 * of a cached entry is greater that the bigger lifetime of the
	 * gateway entries it is pointed by.
	 */
	nhrt->rt_flags |= RTF_CACHED;
	nhrt->rt_cachecnt++;

	rt->rt_gwroute = nhrt;

	return (0);
}

/*
 * Invalidate the cached route entry of the gateway entry ``rt''.
 */
void
rt_putgwroute(struct rtentry *rt)
{
	struct rtentry *nhrt = rt->rt_gwroute;

	NET_ASSERT_LOCKED();

	if (!ISSET(rt->rt_flags, RTF_GATEWAY) || nhrt == NULL)
		return;

	KASSERT(ISSET(nhrt->rt_flags, RTF_CACHED));
	KASSERT(nhrt->rt_cachecnt > 0);

	--nhrt->rt_cachecnt;
	if (nhrt->rt_cachecnt == 0)
		nhrt->rt_flags &= ~RTF_CACHED;

	rtfree(rt->rt_gwroute);
	rt->rt_gwroute = NULL;
}

void
rtref(struct rtentry *rt)
{
	atomic_inc_int(&rt->rt_refcnt);
}

void
rtfree(struct rtentry *rt)
{
	int		 refcnt;

	if (rt == NULL)
		return;

	refcnt = (int)atomic_dec_int_nv(&rt->rt_refcnt);
	if (refcnt <= 0) {
		KASSERT(!ISSET(rt->rt_flags, RTF_UP));
		KASSERT(!RT_ROOT(rt));
		atomic_dec_int(&rttrash);
		if (refcnt < 0) {
			printf("rtfree: %p not freed (neg refs)\n", rt);
			return;
		}

		KERNEL_LOCK();
		rt_timer_remove_all(rt);
		ifafree(rt->rt_ifa);
		rtlabel_unref(rt->rt_labelid);
#ifdef MPLS
		if (rt->rt_flags & RTF_MPLS)
			free(rt->rt_llinfo, M_TEMP, sizeof(struct rt_mpls));
#endif
		free(rt->rt_gateway, M_RTABLE, ROUNDUP(rt->rt_gateway->sa_len));
		free(rt_key(rt), M_RTABLE, rt_key(rt)->sa_len);
		KERNEL_UNLOCK();

		pool_put(&rtentry_pool, rt);
	}
}

void
ifafree(struct ifaddr *ifa)
{
	if (ifa == NULL)
		panic("ifafree");
	if (ifa->ifa_refcnt == 0) {
		ifatrash--;
		free(ifa, M_IFADDR, 0);
	} else
		ifa->ifa_refcnt--;
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 */
void
rtredirect(struct sockaddr *dst, struct sockaddr *gateway,
    struct sockaddr *src, struct rtentry **rtp, unsigned int rdomain)
{
	struct rtentry		*rt;
	int			 error = 0;
	enum rtstat_counters	 stat = rts_ncounters;
	struct rt_addrinfo	 info;
	struct ifaddr		*ifa;
	unsigned int		 ifidx = 0;
	int			 flags = RTF_GATEWAY|RTF_HOST;
	uint8_t			 prio = RTP_NONE;

	NET_ASSERT_LOCKED();

	/* verify the gateway is directly reachable */
	rt = rtalloc(gateway, 0, rdomain);
	if (!rtisvalid(rt) || ISSET(rt->rt_flags, RTF_GATEWAY)) {
		rtfree(rt);
		error = ENETUNREACH;
		goto out;
	}
	ifidx = rt->rt_ifidx;
	ifa = rt->rt_ifa;
	rtfree(rt);
	rt = NULL;

	rt = rtable_lookup(rdomain, dst, NULL, NULL, RTP_ANY);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) \
	((a1)->sa_len == (a2)->sa_len && \
	 bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (rt != NULL && (!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway, rdomain) != NULL ||
	    (gateway->sa_family = AF_INET &&
	    in_broadcast(satosin(gateway)->sin_addr, rdomain)))
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if (rt == NULL)
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface.
	 */
	if (ISSET(rt->rt_flags, RTF_GATEWAY)) {
		if (!ISSET(rt->rt_flags, RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
create:
			rtfree(rt);
			flags |= RTF_DYNAMIC;
			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			info.rti_info[RTAX_GATEWAY] = gateway;
			info.rti_ifa = ifa;
			info.rti_flags = flags;
			rt = NULL;
			error = rtrequest(RTM_ADD, &info, RTP_DEFAULT, &rt,
			    rdomain);
			if (error == 0) {
				flags = rt->rt_flags;
				prio = rt->rt_priority;
			}
			stat = rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			prio = rt->rt_priority;
			stat = rts_newgateway;
			rt_setgate(rt, gateway, rdomain);
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
out:
	if (error)
		rtstat_inc(rts_badredirect);
	else if (stat != rts_ncounters)
		rtstat_inc(stat);
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_AUTHOR] = src;
	rtm_miss(RTM_REDIRECT, &info, flags, prio, ifidx, error, rdomain);
}

/*
 * Delete a route and generate a message
 */
int
rtdeletemsg(struct rtentry *rt, struct ifnet *ifp, u_int tableid)
{
	int			error;
	struct rt_addrinfo	info;
	struct sockaddr_in6	sa_mask;

	KASSERT(rt->rt_ifidx == ifp->if_index);

	/*
	 * Request the new route so that the entry is not actually
	 * deleted.  That will allow the information being reported to
	 * be accurate (and consistent with route_output()).
	 */
	memset(&info, 0, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	if (!ISSET(rt->rt_flags, RTF_HOST))
		info.rti_info[RTAX_NETMASK] = rt_plen2mask(rt, &sa_mask);
	error = rtrequest_delete(&info, rt->rt_priority, ifp, &rt, tableid);
	rtm_send(rt, RTM_DELETE, error, tableid);
	if (error == 0)
		rtfree(rt);
	return (error);
}

static inline int
rtequal(struct rtentry *a, struct rtentry *b)
{
	if (a == b)
		return 1;

	if (memcmp(rt_key(a), rt_key(b), rt_key(a)->sa_len) == 0 &&
	    rt_plen(a) == rt_plen(b))
		return 1;
	else
		return 0;
}

int
rtflushclone1(struct rtentry *rt, void *arg, u_int id)
{
	struct rtentry *cloningrt = arg;
	struct ifnet *ifp;
	int error;

	if (!ISSET(rt->rt_flags, RTF_CLONED))
		return 0;

	/* Cached route must stay alive as long as their parent are alive. */
	if (ISSET(rt->rt_flags, RTF_CACHED) && (rt->rt_parent != cloningrt))
		return 0;

	if (!rtequal(rt->rt_parent, cloningrt))
		return 0;
	/*
	 * This happens when an interface with a RTF_CLONING route is
	 * being detached.  In this case it's safe to bail because all
	 * the routes are being purged by rt_ifa_purge().
	 */
	ifp = if_get(rt->rt_ifidx);
	if (ifp == NULL)
	        return 0;

	error = rtdeletemsg(rt, ifp, id);
	if (error == 0)
		error = EAGAIN;

	if_put(ifp);
	return error;
}

void
rtflushclone(unsigned int rtableid, struct rtentry *parent)
{

#ifdef DIAGNOSTIC
	if (!parent || (parent->rt_flags & RTF_CLONING) == 0)
		panic("rtflushclone: called with a non-cloning route");
#endif
	rtable_walk(rtableid, rt_key(parent)->sa_family, rtflushclone1, parent);
}

int
rtrequest_delete(struct rt_addrinfo *info, u_int8_t prio, struct ifnet *ifp,
    struct rtentry **ret_nrt, u_int tableid)
{
	struct rtentry	*rt;
	int		 error;

	NET_ASSERT_LOCKED();

	if (!rtable_exists(tableid))
		return (EAFNOSUPPORT);
	rt = rtable_lookup(tableid, info->rti_info[RTAX_DST],
	    info->rti_info[RTAX_NETMASK], info->rti_info[RTAX_GATEWAY], prio);
	if (rt == NULL)
		return (ESRCH);

	/* Make sure that's the route the caller want to delete. */
	if (ifp != NULL && ifp->if_index != rt->rt_ifidx) {
		rtfree(rt);
		return (ESRCH);
	}

#ifdef BFD
	if (ISSET(rt->rt_flags, RTF_BFD))
		bfdclear(rt);
#endif

	error = rtable_delete(tableid, info->rti_info[RTAX_DST],
	    info->rti_info[RTAX_NETMASK], rt);
	if (error != 0) {
		rtfree(rt);
		return (ESRCH);
	}

	/* Release next hop cache before flushing cloned entries. */
	rt_putgwroute(rt);

	/* Clean up any cloned children. */
	if (ISSET(rt->rt_flags, RTF_CLONING))
		rtflushclone(tableid, rt);

	rtfree(rt->rt_parent);
	rt->rt_parent = NULL;

	rt->rt_flags &= ~RTF_UP;

	KASSERT(ifp->if_index == rt->rt_ifidx);
	ifp->if_rtrequest(ifp, RTM_DELETE, rt);

	atomic_inc_int(&rttrash);

	if (ret_nrt != NULL)
		*ret_nrt = rt;
	else
		rtfree(rt);

	return (0);
}

int
rtrequest(int req, struct rt_addrinfo *info, u_int8_t prio,
    struct rtentry **ret_nrt, u_int tableid)
{
	struct ifnet		*ifp;
	struct rtentry		*rt, *crt;
	struct ifaddr		*ifa;
	struct sockaddr		*ndst;
	struct sockaddr_rtlabel	*sa_rl, sa_rl2;
	struct sockaddr_dl	 sa_dl = { sizeof(sa_dl), AF_LINK };
	int			 dlen, error;
#ifdef MPLS
	struct sockaddr_mpls	*sa_mpls;
#endif

	NET_ASSERT_LOCKED();

	if (!rtable_exists(tableid))
		return (EAFNOSUPPORT);
	if (info->rti_flags & RTF_HOST)
		info->rti_info[RTAX_NETMASK] = NULL;
	switch (req) {
	case RTM_DELETE:
		return (EINVAL);

	case RTM_RESOLVE:
		if (ret_nrt == NULL || (rt = *ret_nrt) == NULL)
			return (EINVAL);
		if ((rt->rt_flags & RTF_CLONING) == 0)
			return (EINVAL);
		KASSERT(rt->rt_ifa->ifa_ifp != NULL);
		info->rti_ifa = rt->rt_ifa;
		info->rti_flags = rt->rt_flags | (RTF_CLONED|RTF_HOST);
		info->rti_flags &= ~(RTF_CLONING|RTF_CONNECTED|RTF_STATIC);
		info->rti_info[RTAX_GATEWAY] = sdltosa(&sa_dl);
		info->rti_info[RTAX_LABEL] =
		    rtlabel_id2sa(rt->rt_labelid, &sa_rl2);
		/* FALLTHROUGH */

	case RTM_ADD:
		if (info->rti_ifa == NULL)
			return (EINVAL);
		ifa = info->rti_ifa;
		ifp = ifa->ifa_ifp;
		if (prio == 0)
			prio = ifp->if_priority + RTP_STATIC;

		dlen = info->rti_info[RTAX_DST]->sa_len;
		ndst = malloc(dlen, M_RTABLE, M_NOWAIT);
		if (ndst == NULL)
			return (ENOBUFS);

		if (info->rti_info[RTAX_NETMASK] != NULL)
			rt_maskedcopy(info->rti_info[RTAX_DST], ndst,
			    info->rti_info[RTAX_NETMASK]);
		else
			memcpy(ndst, info->rti_info[RTAX_DST], dlen);

		rt = pool_get(&rtentry_pool, PR_NOWAIT | PR_ZERO);
		if (rt == NULL) {
			free(ndst, M_RTABLE, dlen);
			return (ENOBUFS);
		}

		rt->rt_refcnt = 1;
		rt->rt_flags = info->rti_flags | RTF_UP;
		rt->rt_priority = prio;	/* init routing priority */
		LIST_INIT(&rt->rt_timer);

		/* Check the link state if the table supports it. */
		if (rtable_mpath_capable(tableid, ndst->sa_family) &&
		    !ISSET(rt->rt_flags, RTF_LOCAL) &&
		    (!LINK_STATE_IS_UP(ifp->if_link_state) ||
		    !ISSET(ifp->if_flags, IFF_UP))) {
			rt->rt_flags &= ~RTF_UP;
			rt->rt_priority |= RTP_DOWN;
		}

		if (info->rti_info[RTAX_LABEL] != NULL) {
			sa_rl = (struct sockaddr_rtlabel *)
			    info->rti_info[RTAX_LABEL];
			rt->rt_labelid = rtlabel_name2id(sa_rl->sr_label);
		}

#ifdef MPLS
		/* We have to allocate additional space for MPLS infos */
		if (info->rti_flags & RTF_MPLS &&
		    (info->rti_info[RTAX_SRC] != NULL ||
		    info->rti_info[RTAX_DST]->sa_family == AF_MPLS)) {
			struct rt_mpls *rt_mpls;

			sa_mpls = (struct sockaddr_mpls *)
			    info->rti_info[RTAX_SRC];

			rt->rt_llinfo = malloc(sizeof(struct rt_mpls),
			    M_TEMP, M_NOWAIT|M_ZERO);

			if (rt->rt_llinfo == NULL) {
				free(ndst, M_RTABLE, dlen);
				pool_put(&rtentry_pool, rt);
				return (ENOMEM);
			}

			rt_mpls = (struct rt_mpls *)rt->rt_llinfo;

			if (sa_mpls != NULL)
				rt_mpls->mpls_label = sa_mpls->smpls_label;

			rt_mpls->mpls_operation = info->rti_mpls;

			/* XXX: set experimental bits */

			rt->rt_flags |= RTF_MPLS;
		} else
			rt->rt_flags &= ~RTF_MPLS;
#endif

		ifa->ifa_refcnt++;
		rt->rt_ifa = ifa;
		rt->rt_ifidx = ifp->if_index;
		/*
		 * Copy metrics and a back pointer from the cloned
		 * route's parent.
		 */
		if (ISSET(rt->rt_flags, RTF_CLONED)) {
			rtref(*ret_nrt);
			rt->rt_parent = *ret_nrt;
			rt->rt_rmx = (*ret_nrt)->rt_rmx;
		}

		/*
		 * We must set rt->rt_gateway before adding ``rt'' to
		 * the routing table because the radix MPATH code use
		 * it to (re)order routes.
		 */
		if ((error = rt_setgate(rt, info->rti_info[RTAX_GATEWAY],
		    tableid))) {
			ifafree(ifa);
			rtfree(rt->rt_parent);
			rt_putgwroute(rt);
			free(rt->rt_gateway, M_RTABLE, 0);
			free(ndst, M_RTABLE, dlen);
			pool_put(&rtentry_pool, rt);
			return (error);
		}

		error = rtable_insert(tableid, ndst,
		    info->rti_info[RTAX_NETMASK], info->rti_info[RTAX_GATEWAY],
		    rt->rt_priority, rt);
		if (error != 0 &&
		    (crt = rtable_match(tableid, ndst, NULL)) != NULL) {
			/* overwrite cloned route */
			if (ISSET(crt->rt_flags, RTF_CLONED)) {
				struct ifnet *cifp;

				cifp = if_get(crt->rt_ifidx);
				KASSERT(cifp != NULL);
				rtdeletemsg(crt, cifp, tableid);
				if_put(cifp);

				error = rtable_insert(tableid, ndst,
				    info->rti_info[RTAX_NETMASK],
				    info->rti_info[RTAX_GATEWAY],
				    rt->rt_priority, rt);
			}
			rtfree(crt);
		}
		if (error != 0) {
			ifafree(ifa);
			rtfree(rt->rt_parent);
			rt_putgwroute(rt);
			free(rt->rt_gateway, M_RTABLE, 0);
			free(ndst, M_RTABLE, dlen);
			pool_put(&rtentry_pool, rt);
			return (EEXIST);
		}
		ifp->if_rtrequest(ifp, req, rt);

		if_group_routechange(info->rti_info[RTAX_DST],
			info->rti_info[RTAX_NETMASK]);

		if (ret_nrt != NULL)
			*ret_nrt = rt;
		else
			rtfree(rt);
		break;
	}

	return (0);
}

int
rt_setgate(struct rtentry *rt, struct sockaddr *gate, u_int rtableid)
{
	int glen = ROUNDUP(gate->sa_len);
	struct sockaddr *sa;

	if (rt->rt_gateway == NULL || glen != ROUNDUP(rt->rt_gateway->sa_len)) {
		sa = malloc(glen, M_RTABLE, M_NOWAIT);
		if (sa == NULL)
			return (ENOBUFS);
		if (rt->rt_gateway != NULL) {
			free(rt->rt_gateway, M_RTABLE,
			    ROUNDUP(rt->rt_gateway->sa_len));
		}
		rt->rt_gateway = sa;
	}
	memmove(rt->rt_gateway, gate, glen);

	if (ISSET(rt->rt_flags, RTF_GATEWAY))
		return (rt_setgwroute(rt, rtableid));

	return (0);
}

/*
 * Return the route entry containing the next hop link-layer
 * address corresponding to ``rt''.
 */
struct rtentry *
rt_getll(struct rtentry *rt)
{
	if (ISSET(rt->rt_flags, RTF_GATEWAY)) {
		KASSERT(rt->rt_gwroute != NULL);
		return (rt->rt_gwroute);
	}

	return (rt);
}

void
rt_maskedcopy(struct sockaddr *src, struct sockaddr *dst,
    struct sockaddr *netmask)
{
	u_char	*cp1 = (u_char *)src;
	u_char	*cp2 = (u_char *)dst;
	u_char	*cp3 = (u_char *)netmask;
	u_char	*cplim = cp2 + *cp3;
	u_char	*cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

int
rt_ifa_add(struct ifaddr *ifa, int flags, struct sockaddr *dst)
{
	struct ifnet		*ifp = ifa->ifa_ifp;
	struct rtentry		*rt;
	struct sockaddr_rtlabel	 sa_rl;
	struct rt_addrinfo	 info;
	unsigned int		 rtableid = ifp->if_rdomain;
	uint8_t			 prio = ifp->if_priority + RTP_STATIC;
	int			 error;

	memset(&info, 0, sizeof(info));
	info.rti_ifa = ifa;
	info.rti_flags = flags | RTF_MPATH;
	info.rti_info[RTAX_DST] = dst;
	if (flags & RTF_LLINFO)
		info.rti_info[RTAX_GATEWAY] = sdltosa(ifp->if_sadl);
	else
		info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
	info.rti_info[RTAX_LABEL] = rtlabel_id2sa(ifp->if_rtlabelid, &sa_rl);

#ifdef MPLS
	if ((flags & RTF_MPLS) == RTF_MPLS) {
		info.rti_mpls = MPLS_OP_POP;
		/* MPLS routes only exist in rdomain 0 */
		rtableid = 0;
	}
#endif /* MPLS */

	if ((flags & RTF_HOST) == 0)
		info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;

	if (flags & (RTF_LOCAL|RTF_BROADCAST))
		prio = RTP_LOCAL;

	if (flags & RTF_CONNECTED)
		prio = ifp->if_priority + RTP_CONNECTED;

	error = rtrequest(RTM_ADD, &info, prio, &rt, rtableid);
	if (error == 0) {
		/*
		 * A local route is created for every address configured
		 * on an interface, so use this information to notify
		 * userland that a new address has been added.
		 */
		if (flags & RTF_LOCAL)
			rtm_addr(RTM_NEWADDR, ifa);
		rtm_send(rt, RTM_ADD, 0, rtableid);
		rtfree(rt);
	}
	return (error);
}

int
rt_ifa_del(struct ifaddr *ifa, int flags, struct sockaddr *dst)
{
	struct ifnet		*ifp = ifa->ifa_ifp;
	struct rtentry		*rt;
	struct mbuf		*m = NULL;
	struct sockaddr		*deldst;
	struct rt_addrinfo	 info;
	struct sockaddr_rtlabel	 sa_rl;
	unsigned int		 rtableid = ifp->if_rdomain;
	uint8_t			 prio = ifp->if_priority + RTP_STATIC;
	int			 error;

#ifdef MPLS
	if ((flags & RTF_MPLS) == RTF_MPLS)
		/* MPLS routes only exist in rdomain 0 */
		rtableid = 0;
#endif /* MPLS */

	if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
		m = m_get(M_DONTWAIT, MT_SONAME);
		if (m == NULL)
			return (ENOBUFS);
		deldst = mtod(m, struct sockaddr *);
		rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
		dst = deldst;
	}

	memset(&info, 0, sizeof(info));
	info.rti_ifa = ifa;
	info.rti_flags = flags;
	info.rti_info[RTAX_DST] = dst;
	if ((flags & RTF_LLINFO) == 0)
		info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
	info.rti_info[RTAX_LABEL] = rtlabel_id2sa(ifp->if_rtlabelid, &sa_rl);

	if ((flags & RTF_HOST) == 0)
		info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;

	if (flags & (RTF_LOCAL|RTF_BROADCAST))
		prio = RTP_LOCAL;

	if (flags & RTF_CONNECTED)
		prio = ifp->if_priority + RTP_CONNECTED;

	error = rtrequest_delete(&info, prio, ifp, &rt, rtableid);
	if (error == 0) {
		rtm_send(rt, RTM_DELETE, 0, rtableid);
		if (flags & RTF_LOCAL)
			rtm_addr(RTM_DELADDR, ifa);
		rtfree(rt);
	}
	m_free(m);

	return (error);
}

/*
 * Add ifa's address as a local rtentry.
 */
int
rt_ifa_addlocal(struct ifaddr *ifa)
{
	struct rtentry *rt;
	u_int flags = RTF_HOST|RTF_LOCAL;
	int error = 0;

	/*
	 * If the configured address correspond to the magical "any"
	 * address do not add a local route entry because that might
	 * corrupt the routing tree which uses this value for the
	 * default routes.
	 */
	switch (ifa->ifa_addr->sa_family) {
	case AF_INET:
		if (satosin(ifa->ifa_addr)->sin_addr.s_addr == INADDR_ANY)
			return (0);
		break;
#ifdef INET6
	case AF_INET6:
		if (IN6_ARE_ADDR_EQUAL(&satosin6(ifa->ifa_addr)->sin6_addr,
		    &in6addr_any))
			return (0);
		break;
#endif
	default:
		break;
	}

	if (!ISSET(ifa->ifa_ifp->if_flags, (IFF_LOOPBACK|IFF_POINTOPOINT)))
		flags |= RTF_LLINFO;

	/* If there is no local entry, allocate one. */
	rt = rtalloc(ifa->ifa_addr, 0, ifa->ifa_ifp->if_rdomain);
	if (rt == NULL || ISSET(rt->rt_flags, flags) != flags)
		error = rt_ifa_add(ifa, flags, ifa->ifa_addr);
	rtfree(rt);

	return (error);
}

/*
 * Remove local rtentry of ifa's addresss if it exists.
 */
int
rt_ifa_dellocal(struct ifaddr *ifa)
{
	struct rtentry *rt;
	u_int flags = RTF_HOST|RTF_LOCAL;
	int error = 0;

	/*
	 * We do not add local routes for such address, so do not bother
	 * removing them.
	 */
	switch (ifa->ifa_addr->sa_family) {
	case AF_INET:
		if (satosin(ifa->ifa_addr)->sin_addr.s_addr == INADDR_ANY)
			return (0);
		break;
#ifdef INET6
	case AF_INET6:
		if (IN6_ARE_ADDR_EQUAL(&satosin6(ifa->ifa_addr)->sin6_addr,
		    &in6addr_any))
			return (0);
		break;
#endif
	default:
		break;
	}

	if (!ISSET(ifa->ifa_ifp->if_flags, (IFF_LOOPBACK|IFF_POINTOPOINT)))
		flags |= RTF_LLINFO;

	/*
	 * Before deleting, check if a corresponding local host
	 * route surely exists.  With this check, we can avoid to
	 * delete an interface direct route whose destination is same
	 * as the address being removed.  This can happen when removing
	 * a subnet-router anycast address on an interface attached
	 * to a shared medium.
	 */
	rt = rtalloc(ifa->ifa_addr, 0, ifa->ifa_ifp->if_rdomain);
	if (rt != NULL && ISSET(rt->rt_flags, flags) == flags)
		error = rt_ifa_del(ifa, flags, ifa->ifa_addr);
	rtfree(rt);

	return (error);
}

/*
 * Remove all addresses attached to ``ifa''.
 */
void
rt_ifa_purge(struct ifaddr *ifa)
{
	struct ifnet		*ifp = ifa->ifa_ifp;
	unsigned int		 rtableid;
	int			 i;

	KASSERT(ifp != NULL);

	for (rtableid = 0; rtableid < rtmap_limit; rtableid++) {
		/* skip rtables that are not in the rdomain of the ifp */
		if (rtable_l2(rtableid) != ifp->if_rdomain)
			continue;
		for (i = 1; i <= AF_MAX; i++) {
			rtable_walk(rtableid, i, rt_ifa_purge_walker, ifa);
		}
	}
}

int
rt_ifa_purge_walker(struct rtentry *rt, void *vifa, unsigned int rtableid)
{
	struct ifaddr		*ifa = vifa;
	struct ifnet		*ifp = ifa->ifa_ifp;
	int			 error;

	if (rt->rt_ifa != ifa)
		return (0);

	if ((error = rtdeletemsg(rt, ifp, rtableid))) {
		return (error);
	}

	return (EAGAIN);

}

/*
 * Route timer routines.  These routes allow functions to be called
 * for various routes at any time.  This is useful in supporting
 * path MTU discovery and redirect route deletion.
 *
 * This is similar to some BSDI internal functions, but it provides
 * for multiple queues for efficiency's sake...
 */

LIST_HEAD(, rttimer_queue)	rttimer_queue_head;
static int			rt_init_done = 0;

#define RTTIMER_CALLOUT(r)	{					\
	if (r->rtt_func != NULL) {					\
		(*r->rtt_func)(r->rtt_rt, r);				\
	} else {							\
		struct ifnet *ifp;					\
									\
		ifp = if_get(r->rtt_rt->rt_ifidx);			\
		if (ifp != NULL) 					\
			rtdeletemsg(r->rtt_rt, ifp, r->rtt_tableid);	\
		if_put(ifp);						\
	}								\
}

/*
 * Some subtle order problems with domain initialization mean that
 * we cannot count on this being run from rt_init before various
 * protocol initializations are done.  Therefore, we make sure
 * that this is run when the first queue is added...
 */

void
rt_timer_init(void)
{
	static struct timeout	rt_timer_timeout;

	if (rt_init_done)
		panic("rt_timer_init: already initialized");

	pool_init(&rttimer_pool, sizeof(struct rttimer), 0, IPL_SOFTNET, 0,
	    "rttmr", NULL);

	LIST_INIT(&rttimer_queue_head);
	timeout_set_proc(&rt_timer_timeout, rt_timer_timer, &rt_timer_timeout);
	timeout_add_sec(&rt_timer_timeout, 1);
	rt_init_done = 1;
}

struct rttimer_queue *
rt_timer_queue_create(u_int timeout)
{
	struct rttimer_queue	*rtq;

	if (rt_init_done == 0)
		rt_timer_init();

	if ((rtq = malloc(sizeof(*rtq), M_RTABLE, M_NOWAIT|M_ZERO)) == NULL)
		return (NULL);

	rtq->rtq_timeout = timeout;
	rtq->rtq_count = 0;
	TAILQ_INIT(&rtq->rtq_head);
	LIST_INSERT_HEAD(&rttimer_queue_head, rtq, rtq_link);

	return (rtq);
}

void
rt_timer_queue_change(struct rttimer_queue *rtq, long timeout)
{
	rtq->rtq_timeout = timeout;
}

void
rt_timer_queue_destroy(struct rttimer_queue *rtq)
{
	struct rttimer	*r;

	NET_ASSERT_LOCKED();

	while ((r = TAILQ_FIRST(&rtq->rtq_head)) != NULL) {
		LIST_REMOVE(r, rtt_link);
		TAILQ_REMOVE(&rtq->rtq_head, r, rtt_next);
		RTTIMER_CALLOUT(r);
		pool_put(&rttimer_pool, r);
		if (rtq->rtq_count > 0)
			rtq->rtq_count--;
		else
			printf("rt_timer_queue_destroy: rtq_count reached 0\n");
	}

	LIST_REMOVE(rtq, rtq_link);
	free(rtq, M_RTABLE, sizeof(*rtq));
}

unsigned long
rt_timer_queue_count(struct rttimer_queue *rtq)
{
	return (rtq->rtq_count);
}

void
rt_timer_remove_all(struct rtentry *rt)
{
	struct rttimer	*r;

	while ((r = LIST_FIRST(&rt->rt_timer)) != NULL) {
		LIST_REMOVE(r, rtt_link);
		TAILQ_REMOVE(&r->rtt_queue->rtq_head, r, rtt_next);
		if (r->rtt_queue->rtq_count > 0)
			r->rtt_queue->rtq_count--;
		else
			printf("rt_timer_remove_all: rtq_count reached 0\n");
		pool_put(&rttimer_pool, r);
	}
}

int
rt_timer_add(struct rtentry *rt, void (*func)(struct rtentry *,
    struct rttimer *), struct rttimer_queue *queue, u_int rtableid)
{
	struct rttimer	*r;
	long		 current_time;

	current_time = time_uptime;
	rt->rt_expire = time_uptime + queue->rtq_timeout;

	/*
	 * If there's already a timer with this action, destroy it before
	 * we add a new one.
	 */
	LIST_FOREACH(r, &rt->rt_timer, rtt_link) {
		if (r->rtt_func == func) {
			LIST_REMOVE(r, rtt_link);
			TAILQ_REMOVE(&r->rtt_queue->rtq_head, r, rtt_next);
			if (r->rtt_queue->rtq_count > 0)
				r->rtt_queue->rtq_count--;
			else
				printf("rt_timer_add: rtq_count reached 0\n");
			pool_put(&rttimer_pool, r);
			break;  /* only one per list, so we can quit... */
		}
	}

	r = pool_get(&rttimer_pool, PR_NOWAIT | PR_ZERO);
	if (r == NULL)
		return (ENOBUFS);

	r->rtt_rt = rt;
	r->rtt_time = current_time;
	r->rtt_func = func;
	r->rtt_queue = queue;
	r->rtt_tableid = rtableid;
	LIST_INSERT_HEAD(&rt->rt_timer, r, rtt_link);
	TAILQ_INSERT_TAIL(&queue->rtq_head, r, rtt_next);
	r->rtt_queue->rtq_count++;

	return (0);
}

void
rt_timer_timer(void *arg)
{
	struct timeout		*to = (struct timeout *)arg;
	struct rttimer_queue	*rtq;
	struct rttimer		*r;
	long			 current_time;

	current_time = time_uptime;

	NET_LOCK();
	LIST_FOREACH(rtq, &rttimer_queue_head, rtq_link) {
		while ((r = TAILQ_FIRST(&rtq->rtq_head)) != NULL &&
		    (r->rtt_time + rtq->rtq_timeout) < current_time) {
			LIST_REMOVE(r, rtt_link);
			TAILQ_REMOVE(&rtq->rtq_head, r, rtt_next);
			RTTIMER_CALLOUT(r);
			pool_put(&rttimer_pool, r);
			if (rtq->rtq_count > 0)
				rtq->rtq_count--;
			else
				printf("rt_timer_timer: rtq_count reached 0\n");
		}
	}
	NET_UNLOCK();

	timeout_add_sec(to, 1);
}

u_int16_t
rtlabel_name2id(char *name)
{
	struct rt_label		*label, *p;
	u_int16_t		 new_id = 1;

	if (!name[0])
		return (0);

	TAILQ_FOREACH(label, &rt_labels, rtl_entry)
		if (strcmp(name, label->rtl_name) == 0) {
			label->rtl_ref++;
			return (label->rtl_id);
		}

	/*
	 * to avoid fragmentation, we do a linear search from the beginning
	 * and take the first free slot we find. if there is none or the list
	 * is empty, append a new entry at the end.
	 */
	TAILQ_FOREACH(p, &rt_labels, rtl_entry) {
		if (p->rtl_id != new_id)
			break;
		new_id = p->rtl_id + 1;
	}
	if (new_id > LABELID_MAX)
		return (0);

	label = malloc(sizeof(*label), M_RTABLE, M_NOWAIT|M_ZERO);
	if (label == NULL)
		return (0);
	strlcpy(label->rtl_name, name, sizeof(label->rtl_name));
	label->rtl_id = new_id;
	label->rtl_ref++;

	if (p != NULL)	/* insert new entry before p */
		TAILQ_INSERT_BEFORE(p, label, rtl_entry);
	else		/* either list empty or no free slot in between */
		TAILQ_INSERT_TAIL(&rt_labels, label, rtl_entry);

	return (label->rtl_id);
}

const char *
rtlabel_id2name(u_int16_t id)
{
	struct rt_label	*label;

	TAILQ_FOREACH(label, &rt_labels, rtl_entry)
		if (label->rtl_id == id)
			return (label->rtl_name);

	return (NULL);
}

struct sockaddr *
rtlabel_id2sa(u_int16_t labelid, struct sockaddr_rtlabel *sa_rl)
{
	const char	*label;

	if (labelid == 0 || (label = rtlabel_id2name(labelid)) == NULL)
		return (NULL);

	bzero(sa_rl, sizeof(*sa_rl));
	sa_rl->sr_len = sizeof(*sa_rl);
	sa_rl->sr_family = AF_UNSPEC;
	strlcpy(sa_rl->sr_label, label, sizeof(sa_rl->sr_label));

	return ((struct sockaddr *)sa_rl);
}

void
rtlabel_unref(u_int16_t id)
{
	struct rt_label	*p, *next;

	if (id == 0)
		return;

	TAILQ_FOREACH_SAFE(p, &rt_labels, rtl_entry, next) {
		if (id == p->rtl_id) {
			if (--p->rtl_ref == 0) {
				TAILQ_REMOVE(&rt_labels, p, rtl_entry);
				free(p, M_RTABLE, sizeof(*p));
			}
			break;
		}
	}
}

void
rt_if_track(struct ifnet *ifp)
{
	int i;
	u_int tid;

	for (tid = 0; tid < rtmap_limit; tid++) {
		/* skip rtables that are not in the rdomain of the ifp */
		if (rtable_l2(tid) != ifp->if_rdomain)
			continue;
		for (i = 1; i <= AF_MAX; i++) {
			if (!rtable_mpath_capable(tid, i))
				continue;

			rtable_walk(tid, i, rt_if_linkstate_change, ifp);
		}
	}
}

int
rt_if_linkstate_change(struct rtentry *rt, void *arg, u_int id)
{
	struct ifnet *ifp = arg;
	struct sockaddr_in6 sa_mask;
	int error;

	if (rt->rt_ifidx != ifp->if_index)
		return (0);

	/* Local routes are always usable. */
	if (rt->rt_flags & RTF_LOCAL) {
		rt->rt_flags |= RTF_UP;
		return (0);
	}

	if (LINK_STATE_IS_UP(ifp->if_link_state) && ifp->if_flags & IFF_UP) {
		if (ISSET(rt->rt_flags, RTF_UP))
			return (0);

		/* bring route up */
		rt->rt_flags |= RTF_UP;
		error = rtable_mpath_reprio(id, rt_key(rt),
		    rt_plen2mask(rt, &sa_mask), rt->rt_priority & RTP_MASK, rt);
	} else {
		/*
		 * Remove redirected and cloned routes (mainly ARP)
		 * from down interfaces so we have a chance to get
		 * new routes from a better source.
		 */
		if (ISSET(rt->rt_flags, RTF_CLONED|RTF_DYNAMIC) &&
		    !ISSET(rt->rt_flags, RTF_CACHED|RTF_BFD)) {
			if ((error = rtdeletemsg(rt, ifp, id)))
				return (error);
			return (EAGAIN);
		}

		if (!ISSET(rt->rt_flags, RTF_UP))
			return (0);

		/* take route down */
		rt->rt_flags &= ~RTF_UP;
		error = rtable_mpath_reprio(id, rt_key(rt),
		    rt_plen2mask(rt, &sa_mask), rt->rt_priority | RTP_DOWN, rt);
	}
	if_group_routechange(rt_key(rt), rt_plen2mask(rt, &sa_mask));

	return (error);
}

struct sockaddr *
rt_plentosa(sa_family_t af, int plen, struct sockaddr_in6 *sa_mask)
{
	struct sockaddr_in	*sin = (struct sockaddr_in *)sa_mask;
#ifdef INET6
	struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *)sa_mask;
#endif

	KASSERT(plen >= 0 || plen == -1);

	if (plen == -1)
		return (NULL);

	memset(sa_mask, 0, sizeof(*sa_mask));

	switch (af) {
	case AF_INET:
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(struct sockaddr_in);
		in_prefixlen2mask(&sin->sin_addr, plen);
		break;
#ifdef INET6
	case AF_INET6:
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		in6_prefixlen2mask(&sin6->sin6_addr, plen);
		break;
#endif /* INET6 */
	default:
		return (NULL);
	}

	return ((struct sockaddr *)sa_mask);
}

struct sockaddr *
rt_plen2mask(struct rtentry *rt, struct sockaddr_in6 *sa_mask)
{
	return (rt_plentosa(rt_key(rt)->sa_family, rt_plen(rt), sa_mask));
}

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_output.h>

void
db_print_sa(struct sockaddr *sa)
{
	int len;
	u_char *p;

	if (sa == NULL) {
		db_printf("[NULL]");
		return;
	}

	p = (u_char *)sa;
	len = sa->sa_len;
	db_printf("[");
	while (len > 0) {
		db_printf("%d", *p);
		p++;
		len--;
		if (len)
			db_printf(",");
	}
	db_printf("]\n");
}

void
db_print_ifa(struct ifaddr *ifa)
{
	if (ifa == NULL)
		return;
	db_printf("  ifa_addr=");
	db_print_sa(ifa->ifa_addr);
	db_printf("  ifa_dsta=");
	db_print_sa(ifa->ifa_dstaddr);
	db_printf("  ifa_mask=");
	db_print_sa(ifa->ifa_netmask);
	db_printf("  flags=0x%x, refcnt=%d, metric=%d\n",
	    ifa->ifa_flags, ifa->ifa_refcnt, ifa->ifa_metric);
}

/*
 * Function to pass to rtalble_walk().
 * Return non-zero error to abort walk.
 */
int
db_show_rtentry(struct rtentry *rt, void *w, unsigned int id)
{
	db_printf("rtentry=%p", rt);

	db_printf(" flags=0x%x refcnt=%d use=%llu expire=%lld rtableid=%u\n",
	    rt->rt_flags, rt->rt_refcnt, rt->rt_use, rt->rt_expire, id);

	db_printf(" key="); db_print_sa(rt_key(rt));
	db_printf(" plen=%d", rt_plen(rt));
	db_printf(" gw="); db_print_sa(rt->rt_gateway);
	db_printf(" ifidx=%u ", rt->rt_ifidx);
	db_printf(" ifa=%p\n", rt->rt_ifa);
	db_print_ifa(rt->rt_ifa);

	db_printf(" gwroute=%p llinfo=%p\n", rt->rt_gwroute, rt->rt_llinfo);
	return (0);
}

/*
 * Function to print all the route trees.
 * Use this from ddb:  "call db_show_arptab"
 */
int
db_show_arptab(void)
{
	db_printf("Route tree for AF_INET\n");
	rtable_walk(0, AF_INET, db_show_rtentry, NULL);
	return (0);
}
#endif /* DDB */
