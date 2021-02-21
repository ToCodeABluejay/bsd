/* $OpenBSD: toeplitz.c,v 1.10 2021/02/21 02:37:38 dlg Exp $ */

/*
 * Copyright (c) 2009 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Sepherosa Ziehau <sepherosa@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 2019 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2020 Theo Buehler <tb@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <net/toeplitz.h>

/*
 * symmetric toeplitz
 */

static stoeplitz_key		stoeplitz_keyseed = STOEPLITZ_KEYSEED;
static struct stoeplitz_cache	stoeplitz_syskey_cache;
const struct stoeplitz_cache *const
				stoeplitz_cache = &stoeplitz_syskey_cache;

/* parity of n16: count (mod 2) of ones in the binary representation. */
int
parity(uint16_t n16)
{
	n16 = ((n16 & 0xaaaa) >> 1) ^ (n16 & 0x5555);
	n16 = ((n16 & 0xcccc) >> 2) ^ (n16 & 0x3333);
	n16 = ((n16 & 0xf0f0) >> 4) ^ (n16 & 0x0f0f);
	n16 = ((n16 & 0xff00) >> 8) ^ (n16 & 0x00ff);

	return (n16);
}

/*
 * The Toeplitz matrix obtained from a seed is invertible if and only if the
 * parity of the seed is 1. Generate such a seed uniformly at random.
 */
stoeplitz_key
stoeplitz_random_seed(void)
{
	stoeplitz_key seed;

	seed = arc4random() & UINT16_MAX;
	if (parity(seed) == 0)
		seed ^= 1;

	return (seed);
}

void
stoeplitz_init(void)
{
	stoeplitz_keyseed = stoeplitz_random_seed();
	stoeplitz_cache_init(&stoeplitz_syskey_cache, stoeplitz_keyseed);
}

#define NBSK (NBBY * sizeof(stoeplitz_key))

/*
 * The Toeplitz hash of a 16-bit number considered as a column vector over
 * the field with two elements is calculated as a matrix multiplication with
 * a 16x16 circulant Toeplitz matrix T generated by skey.
 *
 * The first eight columns H of T generate the remaining eight columns using
 * the byteswap operation J = swap16:  T = [H JH].  Thus, the Toeplitz hash of
 * n = [hi lo] is computed via the formula T * n = (H * hi) ^ swap16(H * lo).
 *
 * Therefore the results H * val for all values of a byte are cached in scache.
 */
void
stoeplitz_cache_init(struct stoeplitz_cache *scache, stoeplitz_key skey)
{
	uint16_t column[NBBY];
	unsigned int b, shift, val;

	bzero(column, sizeof(column));

	/* Calculate the first eight columns H of the Toeplitz matrix T. */
	for (b = 0; b < NBBY; ++b)
		column[b] = skey << b | skey >> (NBSK - b);

	/* Cache the results of H * val for all possible values of a byte. */
	for (val = 0; val < 256; ++val) {
		uint16_t res = 0;

		for (b = 0; b < NBBY; ++b) {
			shift = NBBY - b - 1;
			if (val & (1 << shift))
				res ^= column[b];
		}
		scache->bytes[val] = res;
	}
}

uint16_t
stoeplitz_hash_ip4(const struct stoeplitz_cache *scache,
    in_addr_t faddr, in_addr_t laddr)
{
	return (stoeplitz_hash_n32(scache, faddr ^ laddr));
}

uint16_t
stoeplitz_hash_ip4port(const struct stoeplitz_cache *scache,
    in_addr_t faddr, in_addr_t laddr, in_port_t fport, in_port_t lport)
{
	return (stoeplitz_hash_n32(scache, faddr ^ laddr ^ fport ^ lport));
}

#ifdef INET6
uint16_t
stoeplitz_hash_ip6(const struct stoeplitz_cache *scache,
    const struct in6_addr *faddr6, const struct in6_addr *laddr6)
{
	uint32_t n32 = 0;
	size_t i;

	for (i = 0; i < nitems(faddr6->s6_addr32); i++)
		n32 ^= faddr6->s6_addr32[i] ^ laddr6->s6_addr32[i];

	return (stoeplitz_hash_n32(scache, n32));
}

uint16_t
stoeplitz_hash_ip6port(const struct stoeplitz_cache *scache,
    const struct in6_addr *faddr6, const struct in6_addr *laddr6,
    in_port_t fport, in_port_t lport)
{
	uint32_t n32 = 0;
	size_t i;

	for (i = 0; i < nitems(faddr6->s6_addr32); i++)
		n32 ^= faddr6->s6_addr32[i] ^ laddr6->s6_addr32[i];

	n32 ^= fport ^ lport;

	return (stoeplitz_hash_n32(scache, n32));
}
#endif /* INET6 */

uint16_t
stoeplitz_hash_eaddr(const struct stoeplitz_cache *scache,
    const uint8_t ea[static 6])
{
	const uint16_t *ea16 = (const uint16_t *)ea;

	return (stoeplitz_hash_n16(scache, ea16[0] ^ ea16[1] ^ ea16[2]));
}

void
stoeplitz_to_key(void *key, size_t klen)
{
	uint8_t *k = key;
	uint16_t skey = htons(stoeplitz_keyseed);
	size_t i;

	KASSERT((klen % 2) == 0);

	for (i = 0; i < klen; i += sizeof(skey)) {
		k[i + 0] = skey >> 8;
		k[i + 1] = skey;
	}
}
