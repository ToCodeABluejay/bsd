/*	$OpenBSD: ip_esp.c,v 1.152 2017/11/08 16:29:20 visa Exp $ */
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 2001 Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include "pfsync.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <net/pfkeyv2.h>
#include <net/if_enc.h>

#if NPFSYNC > 0
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#include <crypto/cryptodev.h>
#include <crypto/xform.h>

#include "bpfilter.h"

void esp_output_cb(struct cryptop *);
void esp_input_cb(struct cryptop *);

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

/*
 * esp_attach() is called from the transformation initialization code.
 */
int
esp_attach(void)
{
	return 0;
}

/*
 * esp_init() is called when an SPI is being set up.
 */
int
esp_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
	struct enc_xform *txform = NULL;
	struct auth_hash *thash = NULL;
	struct cryptoini cria, crie, crin;

	if (!ii->ii_encalg && !ii->ii_authalg) {
		DPRINTF(("esp_init(): neither authentication nor encryption "
		    "algorithm given"));
		return EINVAL;
	}

	if (ii->ii_encalg) {
		switch (ii->ii_encalg) {
		case SADB_EALG_NULL:
			txform = &enc_xform_null;
			break;

		case SADB_EALG_3DESCBC:
			txform = &enc_xform_3des;
			break;

		case SADB_X_EALG_AES:
			txform = &enc_xform_aes;
			break;

		case SADB_X_EALG_AESCTR:
			txform = &enc_xform_aes_ctr;
			break;

		case SADB_X_EALG_AESGCM16:
			txform = &enc_xform_aes_gcm;
			break;

		case SADB_X_EALG_AESGMAC:
			txform = &enc_xform_aes_gmac;
			break;

		case SADB_X_EALG_CHACHA20POLY1305:
			txform = &enc_xform_chacha20_poly1305;
			break;

		case SADB_X_EALG_BLF:
			txform = &enc_xform_blf;
			break;

		case SADB_X_EALG_CAST:
			txform = &enc_xform_cast5;
			break;

		default:
			DPRINTF(("esp_init(): unsupported encryption "
			    "algorithm %d specified\n", ii->ii_encalg));
			return EINVAL;
		}

		if (ii->ii_enckeylen < txform->minkey) {
			DPRINTF(("esp_init(): keylength %d too small "
			    "(min length is %d) for algorithm %s\n",
			    ii->ii_enckeylen, txform->minkey, txform->name));
			return EINVAL;
		}

		if (ii->ii_enckeylen > txform->maxkey) {
			DPRINTF(("esp_init(): keylength %d too large "
			    "(max length is %d) for algorithm %s\n",
			    ii->ii_enckeylen, txform->maxkey, txform->name));
			return EINVAL;
		}

		if (ii->ii_encalg == SADB_X_EALG_AESGCM16 ||
		    ii->ii_encalg == SADB_X_EALG_AESGMAC) {
			switch (ii->ii_enckeylen) {
			case 20:
				ii->ii_authalg = SADB_X_AALG_AES128GMAC;
				break;
			case 28:
				ii->ii_authalg = SADB_X_AALG_AES192GMAC;
				break;
			case 36:
				ii->ii_authalg = SADB_X_AALG_AES256GMAC;
				break;
			}
			ii->ii_authkeylen = ii->ii_enckeylen;
			ii->ii_authkey = ii->ii_enckey;
		} else if (ii->ii_encalg == SADB_X_EALG_CHACHA20POLY1305) {
			ii->ii_authalg = SADB_X_AALG_CHACHA20POLY1305;
			ii->ii_authkeylen = ii->ii_enckeylen;
			ii->ii_authkey = ii->ii_enckey;
		}

		tdbp->tdb_encalgxform = txform;

		DPRINTF(("esp_init(): initialized TDB with enc algorithm %s\n",
		    txform->name));

		tdbp->tdb_ivlen = txform->ivsize;
	}

	if (ii->ii_authalg) {
		switch (ii->ii_authalg) {
		case SADB_AALG_MD5HMAC:
			thash = &auth_hash_hmac_md5_96;
			break;

		case SADB_AALG_SHA1HMAC:
			thash = &auth_hash_hmac_sha1_96;
			break;

		case SADB_X_AALG_RIPEMD160HMAC:
			thash = &auth_hash_hmac_ripemd_160_96;
			break;

		case SADB_X_AALG_SHA2_256:
			thash = &auth_hash_hmac_sha2_256_128;
			break;

		case SADB_X_AALG_SHA2_384:
			thash = &auth_hash_hmac_sha2_384_192;
			break;

		case SADB_X_AALG_SHA2_512:
			thash = &auth_hash_hmac_sha2_512_256;
			break;

		case SADB_X_AALG_AES128GMAC:
			thash = &auth_hash_gmac_aes_128;
			break;

		case SADB_X_AALG_AES192GMAC:
			thash = &auth_hash_gmac_aes_192;
			break;

		case SADB_X_AALG_AES256GMAC:
			thash = &auth_hash_gmac_aes_256;
			break;

		case SADB_X_AALG_CHACHA20POLY1305:
			thash = &auth_hash_chacha20_poly1305;
			break;

		default:
			DPRINTF(("esp_init(): unsupported authentication "
			    "algorithm %d specified\n", ii->ii_authalg));
			return EINVAL;
		}

		if (ii->ii_authkeylen != thash->keysize) {
			DPRINTF(("esp_init(): keylength %d doesn't match "
			    "algorithm %s keysize (%d)\n", ii->ii_authkeylen,
			    thash->name, thash->keysize));
			return EINVAL;
		}

		tdbp->tdb_authalgxform = thash;

		DPRINTF(("esp_init(): initialized TDB with hash algorithm %s\n",
		    thash->name));
	}

	tdbp->tdb_xform = xsp;
	tdbp->tdb_rpl = AH_HMAC_INITIAL_RPL;

	/* Initialize crypto session */
	if (tdbp->tdb_encalgxform) {
		/* Save the raw keys */
		tdbp->tdb_emxkeylen = ii->ii_enckeylen;
		tdbp->tdb_emxkey = malloc(tdbp->tdb_emxkeylen, M_XDATA,
		    M_WAITOK);
		memcpy(tdbp->tdb_emxkey, ii->ii_enckey, tdbp->tdb_emxkeylen);

		memset(&crie, 0, sizeof(crie));

		crie.cri_alg = tdbp->tdb_encalgxform->type;

		if (tdbp->tdb_authalgxform)
			crie.cri_next = &cria;
		else
			crie.cri_next = NULL;

		crie.cri_klen = ii->ii_enckeylen * 8;
		crie.cri_key = ii->ii_enckey;
		/* XXX Rounds ? */
	}

	if (tdbp->tdb_authalgxform) {
		/* Save the raw keys */
		tdbp->tdb_amxkeylen = ii->ii_authkeylen;
		tdbp->tdb_amxkey = malloc(tdbp->tdb_amxkeylen, M_XDATA,
		    M_WAITOK);
		memcpy(tdbp->tdb_amxkey, ii->ii_authkey, tdbp->tdb_amxkeylen);

		memset(&cria, 0, sizeof(cria));

		cria.cri_alg = tdbp->tdb_authalgxform->type;

		if ((tdbp->tdb_wnd > 0) && (tdbp->tdb_flags & TDBF_ESN)) {
			memset(&crin, 0, sizeof(crin));
			crin.cri_alg = CRYPTO_ESN;
			cria.cri_next = &crin;
		}

		cria.cri_klen = ii->ii_authkeylen * 8;
		cria.cri_key = ii->ii_authkey;
	}

	return crypto_newsession(&tdbp->tdb_cryptoid,
	    (tdbp->tdb_encalgxform ? &crie : &cria), 0);
}

/*
 * Paranoia.
 */
int
esp_zeroize(struct tdb *tdbp)
{
	int err;

	if (tdbp->tdb_amxkey) {
		explicit_bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
		free(tdbp->tdb_amxkey, M_XDATA, tdbp->tdb_amxkeylen);
		tdbp->tdb_amxkey = NULL;
	}

	if (tdbp->tdb_emxkey) {
		explicit_bzero(tdbp->tdb_emxkey, tdbp->tdb_emxkeylen);
		free(tdbp->tdb_emxkey, M_XDATA, tdbp->tdb_emxkeylen);
		tdbp->tdb_emxkey = NULL;
	}

	err = crypto_freesession(tdbp->tdb_cryptoid);
	tdbp->tdb_cryptoid = 0;
	return err;
}

#define MAXBUFSIZ (AH_ALEN_MAX > ESP_MAX_IVS ? AH_ALEN_MAX : ESP_MAX_IVS)

/*
 * ESP input processing, called (eventually) through the protocol switch.
 */
int
esp_input(struct mbuf *m, struct tdb *tdb, int skip, int protoff)
{
	struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
	struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
	struct cryptodesc *crde = NULL, *crda = NULL;
	struct cryptop *crp;
	struct tdb_crypto *tc;
	int plen, alen, hlen;
	u_int32_t btsx, esn;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	/* Determine the ESP header length */
	hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen; /* "new" ESP */
	alen = esph ? esph->authsize : 0;
	plen = m->m_pkthdr.len - (skip + hlen + alen);
	if (plen <= 0) {
		DPRINTF(("%s: invalid payload length\n", __func__));
		espstat_inc(esps_badilen);
		m_freem(m);
		return EINVAL;
	}

	if (espx) {
		/*
		 * Verify payload length is multiple of encryption algorithm
		 * block size.
		 */
		if (plen & (espx->blocksize - 1)) {
			DPRINTF(("%s: payload of %d octets not a multiple of %d"
			    " octets, SA %s/%08x\n", __func__,
			    plen, espx->blocksize, ipsp_address(&tdb->tdb_dst,
			    buf, sizeof(buf)), ntohl(tdb->tdb_spi)));
			espstat_inc(esps_badilen);
			m_freem(m);
			return EINVAL;
		}
	}

	/* Replay window checking, if appropriate -- no value commitment. */
	if (tdb->tdb_wnd > 0) {
		m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		    (unsigned char *) &btsx);
		btsx = ntohl(btsx);

		switch (checkreplaywindow(tdb, btsx, &esn, 0)) {
		case 0: /* All's well */
			break;
		case 1:
			m_freem(m);
			DPRINTF(("%s: replay counter wrapped for SA %s/%08x\n",
			    __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_wrap);
			return EACCES;
		case 2:
			m_freem(m);
			DPRINTF(("%s: old packet received in SA %s/%08x\n",
			    __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_replay);
			return EACCES;
		case 3:
			m_freem(m);
			DPRINTF(("%s: duplicate packet received"
			    " in SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_replay);
			return EACCES;
		default:
			m_freem(m);
			DPRINTF(("%s: bogus value from"
			    " checkreplaywindow() in SA %s/%08x\n",
			    __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_replay);
			return EACCES;
		}
	}

	/* Update the counters */
	tdb->tdb_cur_bytes += m->m_pkthdr.len - skip - hlen - alen;
	espstat_add(esps_ibytes, m->m_pkthdr.len - skip - hlen - alen);

	/* Hard expiration */
	if ((tdb->tdb_flags & TDBF_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))	{
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		m_freem(m);
		return ENXIO;
	}

	/* Notify on soft expiration */
	if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES;       /* Turn off checking */
	}

	/* Get crypto descriptors */
	crp = crypto_getreq(esph && espx ? 2 : 1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("%s: failed to acquire crypto descriptors\n", __func__));
		espstat_inc(esps_crypto);
		return ENOBUFS;
	}

	/* Get IPsec-specific opaque pointer */
	if (esph == NULL)
		tc = malloc(sizeof(*tc), M_XDATA, M_NOWAIT | M_ZERO);
	else
		tc = malloc(sizeof(*tc) + alen, M_XDATA, M_NOWAIT | M_ZERO);
	if (tc == NULL)	{
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		espstat_inc(esps_crypto);
		return ENOBUFS;
	}

	if (esph) {
		crda = &crp->crp_desc[0];
		crde = &crp->crp_desc[1];

		/* Authentication descriptor */
		crda->crd_skip = skip;
		crda->crd_inject = m->m_pkthdr.len - alen;

		crda->crd_alg = esph->type;
		crda->crd_key = tdb->tdb_amxkey;
		crda->crd_klen = tdb->tdb_amxkeylen * 8;

		if ((tdb->tdb_wnd > 0) && (tdb->tdb_flags & TDBF_ESN)) {
			esn = htonl(esn);
			memcpy(crda->crd_esn, &esn, 4);
			crda->crd_flags |= CRD_F_ESN;
		}

		if (espx &&
		    (espx->type == CRYPTO_AES_GCM_16 ||
		     espx->type == CRYPTO_CHACHA20_POLY1305))
			crda->crd_len = hlen - tdb->tdb_ivlen;
		else
			crda->crd_len = m->m_pkthdr.len - (skip + alen);

		/* Copy the authenticator */
		m_copydata(m, m->m_pkthdr.len - alen, alen, (caddr_t)(tc + 1));
	} else
		crde = &crp->crp_desc[0];

	/* Crypto operation descriptor */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t)m;
	crp->crp_callback = esp_input_cb;
	crp->crp_sid = tdb->tdb_cryptoid;
	crp->crp_opaque = (caddr_t)tc;

	/* These are passed as-is to the callback */
	tc->tc_skip = skip;
	tc->tc_protoff = protoff;
	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = tdb->tdb_sproto;
	tc->tc_rdomain = tdb->tdb_rdomain;
	tc->tc_dst = tdb->tdb_dst;

	/* Decryption descriptor */
	if (espx) {
		crde->crd_skip = skip + hlen;
		crde->crd_inject = skip + hlen - tdb->tdb_ivlen;
		crde->crd_alg = espx->type;
		crde->crd_key = tdb->tdb_emxkey;
		crde->crd_klen = tdb->tdb_emxkeylen * 8;
		/* XXX Rounds ? */

		if (crde->crd_alg == CRYPTO_AES_GMAC)
			crde->crd_len = 0;
		else
			crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
	}

	return crypto_dispatch(crp);
}

/*
 * ESP input callback, called directly by the crypto driver.
 */
void
esp_input_cb(struct cryptop *crp)
{
	u_int8_t lastthree[3], aalg[AH_HMAC_MAX_HASHLEN];
	int hlen, roff, skip, protoff;
	struct mbuf *m1, *mo, *m;
	struct auth_hash *esph;
	struct tdb_crypto *tc;
	struct tdb *tdb;
	u_int32_t btsx, esn;
	caddr_t ptr;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	tc = (struct tdb_crypto *) crp->crp_opaque;
	skip = tc->tc_skip;
	protoff = tc->tc_protoff;

	m = (struct mbuf *) crp->crp_buf;
	if (m == NULL) {
		/* Shouldn't happen... */
		free(tc, M_XDATA, 0);
		crypto_freereq(crp);
		espstat_inc(esps_crypto);
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		return;
	}

	NET_LOCK();

	tdb = gettdb(tc->tc_rdomain, tc->tc_spi, &tc->tc_dst, tc->tc_proto);
	if (tdb == NULL) {
		free(tc, M_XDATA, 0);
		espstat_inc(esps_notdb);
		DPRINTF(("%s: TDB is expired while in crypto", __func__));
		goto baddone;
	}

	esph = (struct auth_hash *) tdb->tdb_authalgxform;

	/* Check for crypto errors */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (tdb->tdb_cryptoid != 0)
				tdb->tdb_cryptoid = crp->crp_sid;
			NET_UNLOCK();
			crypto_dispatch(crp);
			return;
		}
		free(tc, M_XDATA, 0);
		espstat_inc(esps_noxform);
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		goto baddone;
	}

	/* If authentication was performed, check now. */
	if (esph != NULL) {
		/* Copy the authenticator from the packet */
		m_copydata(m, m->m_pkthdr.len - esph->authsize,
		    esph->authsize, aalg);

		ptr = (caddr_t) (tc + 1);

		/* Verify authenticator */
		if (timingsafe_bcmp(ptr, aalg, esph->authsize)) {
			free(tc, M_XDATA, 0);
			DPRINTF(("%s: authentication "
			    "failed for packet in SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf,
				sizeof(buf)), ntohl(tdb->tdb_spi)));
			espstat_inc(esps_badauth);
			goto baddone;
		}

		/* Remove trailing authenticator */
		m_adj(m, -(esph->authsize));
	}
	free(tc, M_XDATA, 0);

	/* Replay window checking, if appropriate */
	if (tdb->tdb_wnd > 0) {
		m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		    (unsigned char *) &btsx);
		btsx = ntohl(btsx);

		switch (checkreplaywindow(tdb, btsx, &esn, 1)) {
		case 0: /* All's well */
#if NPFSYNC > 0
			pfsync_update_tdb(tdb,0);
#endif
			break;

		case 1:
			DPRINTF(("%s: replay counter wrapped for SA %s/%08x\n",
			    __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_wrap);
			goto baddone;
		case 2:
			DPRINTF(("%s: old packet received in SA %s/%08x\n",
			    __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_replay);
			goto baddone;
		case 3:
			DPRINTF(("%s: duplicate packet received"
			    " in SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_replay);
			goto baddone;
		default:
			DPRINTF(("%s: bogus value from"
			    " checkreplaywindow() in SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_replay);
			goto baddone;
		}
	}

	/* Release the crypto descriptors */
	crypto_freereq(crp);

	/* Determine the ESP header length */
	hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen;

	/* Find beginning of ESP header */
	m1 = m_getptr(m, skip, &roff);
	if (m1 == NULL)	{
		espstat_inc(esps_hdrops);
		NET_UNLOCK();
		DPRINTF(("%s: bad mbuf chain, SA %s/%08x\n", __func__,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		m_freem(m);
		return;
	}

	/* Remove the ESP header and IV from the mbuf. */
	if (roff == 0) {
		/* The ESP header was conveniently at the beginning of the mbuf */
		m_adj(m1, hlen);
		if (!(m1->m_flags & M_PKTHDR))
			m->m_pkthdr.len -= hlen;
	} else if (roff + hlen >= m1->m_len) {
		/*
		 * Part or all of the ESP header is at the end of this mbuf, so
		 * first let's remove the remainder of the ESP header from the
		 * beginning of the remainder of the mbuf chain, if any.
		 */
		if (roff + hlen > m1->m_len) {
			/* Adjust the next mbuf by the remainder */
			m_adj(m1->m_next, roff + hlen - m1->m_len);

			/* The second mbuf is guaranteed not to have a pkthdr */
			m->m_pkthdr.len -= (roff + hlen - m1->m_len);
		}

		/* Now, let's unlink the mbuf chain for a second...*/
		mo = m1->m_next;
		m1->m_next = NULL;

		/* ...and trim the end of the first part of the chain...sick */
		m_adj(m1, -(m1->m_len - roff));
		if (!(m1->m_flags & M_PKTHDR))
			m->m_pkthdr.len -= (m1->m_len - roff);

		/* Finally, let's relink */
		m1->m_next = mo;
	} else {
		/*
		 * The ESP header lies in the "middle" of the mbuf...do an
		 * overlapping copy of the remainder of the mbuf over the ESP
		 * header.
		 */
		memmove(mtod(m1, u_char *) + roff, 
		    mtod(m1, u_char *) + roff + hlen,
		    m1->m_len - (roff + hlen));
		m1->m_len -= hlen;
		m->m_pkthdr.len -= hlen;
	}

	/* Save the last three bytes of decrypted data */
	m_copydata(m, m->m_pkthdr.len - 3, 3, lastthree);

	/* Verify pad length */
	if (lastthree[1] + 2 > m->m_pkthdr.len - skip) {
		espstat_inc(esps_badilen);
		NET_UNLOCK();
		DPRINTF(("%s: invalid padding length %d for packet in "
		    "SA %s/%08x\n", __func__, lastthree[1],
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		m_freem(m);
		return;
	}

	/* Verify correct decryption by checking the last padding bytes */
	if ((lastthree[1] != lastthree[0]) && (lastthree[1] != 0)) {
		espstat_inc(esps_badenc);
		NET_UNLOCK();
		DPRINTF(("%s: decryption failed for packet in SA %s/%08x\n",
		    __func__, ipsp_address(&tdb->tdb_dst, buf,
		    sizeof(buf)), ntohl(tdb->tdb_spi)));
		m_freem(m);
		return;
	}

	/* Trim the mbuf chain to remove the trailing authenticator and padding */
	m_adj(m, -(lastthree[1] + 2));

	/* Restore the Next Protocol field */
	m_copyback(m, protoff, sizeof(u_int8_t), lastthree + 2, M_NOWAIT);

	/* Back to generic IPsec input processing */
	ipsec_common_input_cb(m, tdb, skip, protoff);
	NET_UNLOCK();
	return;

 baddone:
	NET_UNLOCK();

	m_freem(m);

	crypto_freereq(crp);
}

/*
 * ESP output routine, called by ipsp_process_packet().
 */
int
esp_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
    int protoff)
{
	struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
	struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
	int ilen, hlen, rlen, padding, blks, alen, roff;
	u_int32_t replay;
	struct mbuf *mi, *mo = (struct mbuf *) NULL;
	struct tdb_crypto *tc;
	unsigned char *pad;
	u_int8_t prot;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif
	struct cryptodesc *crde = NULL, *crda = NULL;
	struct cryptop *crp;
#if NBPFILTER > 0
	struct ifnet *encif;

	if ((encif = enc_getif(tdb->tdb_rdomain, tdb->tdb_tap)) != NULL) {
		encif->if_opackets++;
		encif->if_obytes += m->m_pkthdr.len;

		if (encif->if_bpf) {
			struct enchdr hdr;

			memset(&hdr, 0, sizeof(hdr));

			hdr.af = tdb->tdb_dst.sa.sa_family;
			hdr.spi = tdb->tdb_spi;
			if (espx)
				hdr.flags |= M_CONF;
			if (esph)
				hdr.flags |= M_AUTH;

			bpf_mtap_hdr(encif->if_bpf, (char *)&hdr,
			    ENC_HDRLEN, m, BPF_DIRECTION_OUT, NULL);
		}
	}
#endif

	hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen;

	rlen = m->m_pkthdr.len - skip; /* Raw payload length. */
	if (espx)
		blks = MAX(espx->blocksize, 4);
	else
		blks = 4; /* If no encryption, we have to be 4-byte aligned. */

	padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;

	alen = esph ? esph->authsize : 0;
	espstat_inc(esps_output);

	switch (tdb->tdb_dst.sa.sa_family) {
	case AF_INET:
		/* Check for IP maximum packet size violations. */
		if (skip + hlen + rlen + padding + alen > IP_MAXPACKET)	{
			DPRINTF(("%s: packet in SA %s/%08x got too big\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			m_freem(m);
			espstat_inc(esps_toobig);
			return EMSGSIZE;
		}
		break;

#ifdef INET6
	case AF_INET6:
		/* Check for IPv6 maximum packet size violations. */
		if (skip + hlen + rlen + padding + alen > IPV6_MAXPACKET) {
			DPRINTF(("%s: packet in SA %s/%08x got too big\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			m_freem(m);
			espstat_inc(esps_toobig);
			return EMSGSIZE;
		}
		break;
#endif /* INET6 */

	default:
		DPRINTF(("%s: unknown/unsupported protocol family %d, "
		    "SA %s/%08x\n", __func__, tdb->tdb_dst.sa.sa_family,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		m_freem(m);
		espstat_inc(esps_nopf);
		return EPFNOSUPPORT;
	}

	/* Update the counters. */
	tdb->tdb_cur_bytes += m->m_pkthdr.len - skip;
	espstat_add(esps_obytes, m->m_pkthdr.len - skip);

	/* Hard byte expiration. */
	if (tdb->tdb_flags & TDBF_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		m_freem(m);
		return EINVAL;
	}

	/* Soft byte expiration. */
	if (tdb->tdb_flags & TDBF_SOFT_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES;    /* Turn off checking. */
	}

	/*
	 * Loop through mbuf chain; if we find a readonly mbuf,
	 * copy the packet.
	 */
	mi = m;
	while (mi != NULL && !M_READONLY(mi))
		mi = mi->m_next;

	if (mi != NULL)	{
		struct mbuf *n = m_dup_pkt(m, 0, M_DONTWAIT);

		if (n == NULL) {
			DPRINTF(("%s: bad mbuf chain, SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			espstat_inc(esps_hdrops);
			m_freem(m);
			return ENOBUFS;
		}

		m_freem(m);
		m = n;
	}

	/* Inject ESP header. */
	mo = m_makespace(m, skip, hlen, &roff);
	if (mo == NULL) {
		DPRINTF(("%s: failed to inject ESP header for SA %s/%08x\n",
		    __func__, ipsp_address(&tdb->tdb_dst, buf,
		    sizeof(buf)), ntohl(tdb->tdb_spi)));
		m_freem(m);
		espstat_inc(esps_hdrops);
		return ENOBUFS;
	}

	/* Initialize ESP header. */
	memcpy(mtod(mo, caddr_t) + roff, (caddr_t) &tdb->tdb_spi,
	    sizeof(u_int32_t));
	tdb->tdb_rpl++;
	replay = htonl((u_int32_t)tdb->tdb_rpl);
	memcpy(mtod(mo, caddr_t) + roff + sizeof(u_int32_t), (caddr_t) &replay,
	    sizeof(u_int32_t));

#if NPFSYNC > 0
	pfsync_update_tdb(tdb,1);
#endif

	/*
	 * Add padding -- better to do it ourselves than use the crypto engine,
	 * although if/when we support compression, we'd have to do that.
	 */
	mo = m_makespace(m, m->m_pkthdr.len, padding + alen, &roff);
	if (mo == NULL) {
		DPRINTF(("%s: m_makespace() failed for SA %s/%08x\n", __func__,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		m_freem(m);
		return ENOBUFS;
	}
	pad = mtod(mo, caddr_t) + roff;

	/* Apply self-describing padding */
	for (ilen = 0; ilen < padding - 2; ilen++)
		pad[ilen] = ilen + 1;

	/* Fix padding length and Next Protocol in padding itself. */
	pad[padding - 2] = padding - 2;
	m_copydata(m, protoff, sizeof(u_int8_t), pad + padding - 1);

	/* Fix Next Protocol in IPv4/IPv6 header. */
	prot = IPPROTO_ESP;
	m_copyback(m, protoff, sizeof(u_int8_t), &prot, M_NOWAIT);

	/* Get crypto descriptors. */
	crp = crypto_getreq(esph && espx ? 2 : 1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("%s: failed to acquire crypto descriptors\n",
		    __func__));
		espstat_inc(esps_crypto);
		return ENOBUFS;
	}

	if (espx) {
		crde = &crp->crp_desc[0];
		crda = &crp->crp_desc[1];

		/* Encryption descriptor. */
		crde->crd_skip = skip + hlen;
		crde->crd_flags = CRD_F_ENCRYPT;
		crde->crd_inject = skip + hlen - tdb->tdb_ivlen;

		/* Encryption operation. */
		crde->crd_alg = espx->type;
		crde->crd_key = tdb->tdb_emxkey;
		crde->crd_klen = tdb->tdb_emxkeylen * 8;
		/* XXX Rounds ? */

		if (crde->crd_alg == CRYPTO_AES_GMAC)
			crde->crd_len = 0;
		else
			crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
	} else
		crda = &crp->crp_desc[0];

	/* IPsec-specific opaque crypto info. */
	tc = malloc(sizeof(*tc), M_XDATA, M_NOWAIT | M_ZERO);
	if (tc == NULL) {
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		espstat_inc(esps_crypto);
		return ENOBUFS;
	}

	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = tdb->tdb_sproto;
	tc->tc_rdomain = tdb->tdb_rdomain;
	tc->tc_dst = tdb->tdb_dst;

	/* Crypto operation descriptor. */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length. */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t)m;
	crp->crp_callback = esp_output_cb;
	crp->crp_opaque = (caddr_t)tc;
	crp->crp_sid = tdb->tdb_cryptoid;

	if (esph) {
		/* Authentication descriptor. */
		crda->crd_skip = skip;
		crda->crd_inject = m->m_pkthdr.len - alen;

		/* Authentication operation. */
		crda->crd_alg = esph->type;
		crda->crd_key = tdb->tdb_amxkey;
		crda->crd_klen = tdb->tdb_amxkeylen * 8;

		if ((tdb->tdb_wnd > 0) && (tdb->tdb_flags & TDBF_ESN)) {
			u_int32_t esn;

			esn = htonl((u_int32_t)(tdb->tdb_rpl >> 32));
			memcpy(crda->crd_esn, &esn, 4);
			crda->crd_flags |= CRD_F_ESN;
		}

		if (espx &&
		    (espx->type == CRYPTO_AES_GCM_16 ||
		     espx->type == CRYPTO_CHACHA20_POLY1305))
			crda->crd_len = hlen - tdb->tdb_ivlen;
		else
			crda->crd_len = m->m_pkthdr.len - (skip + alen);
	}

	return crypto_dispatch(crp);
}

/*
 * ESP output callback, called directly by the crypto driver.
 */
void
esp_output_cb(struct cryptop *crp)
{
	struct tdb_crypto *tc;
	struct tdb *tdb;
	struct mbuf *m;

	tc = (struct tdb_crypto *) crp->crp_opaque;

	m = (struct mbuf *) crp->crp_buf;
	if (m == NULL) {
		/* Shouldn't happen... */
		free(tc, M_XDATA, 0);
		crypto_freereq(crp);
		espstat_inc(esps_crypto);
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		return;
	}


	NET_LOCK();

	tdb = gettdb(tc->tc_rdomain, tc->tc_spi, &tc->tc_dst, tc->tc_proto);
	if (tdb == NULL) {
		free(tc, M_XDATA, 0);
		espstat_inc(esps_notdb);
		DPRINTF(("%s: TDB is expired while in crypto\n", __func__));
		goto baddone;
	}

	/* Check for crypto errors. */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (tdb->tdb_cryptoid != 0)
				tdb->tdb_cryptoid = crp->crp_sid;
			NET_UNLOCK();
			crypto_dispatch(crp);
			return;
		}
		free(tc, M_XDATA, 0);
		espstat_inc(esps_noxform);
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		goto baddone;
	}
	free(tc, M_XDATA, 0);

	/* Release crypto descriptors. */
	crypto_freereq(crp);

	/* Call the IPsec input callback. */
	if (ipsp_process_done(m, tdb))
		espstat_inc(esps_outfail);
	NET_UNLOCK();
	return;

 baddone:
	NET_UNLOCK();

	m_freem(m);

	crypto_freereq(crp);
}

#define SEEN_SIZE	howmany(TDB_REPLAYMAX, 32)

/*
 * return 0 on success
 * return 1 for counter == 0
 * return 2 for very old packet
 * return 3 for packet within current window but already received
 */
int
checkreplaywindow(struct tdb *tdb, u_int32_t seq, u_int32_t *seqh, int commit)
{
	u_int32_t	tl, th, wl;
	u_int32_t	packet, window = TDB_REPLAYMAX - TDB_REPLAYWASTE;
	int		idx, esn = tdb->tdb_flags & TDBF_ESN;

	tl = (u_int32_t)tdb->tdb_rpl;
	th = (u_int32_t)(tdb->tdb_rpl >> 32);

	/* Zero SN is not allowed */
	if ((esn && seq == 0 && tl <= AH_HMAC_INITIAL_RPL && th == 0) ||
	    (!esn && seq == 0))
		return (1);

	if (th == 0 && tl < window)
		window = tl;
	/* Current replay window starts here */
	wl = tl - window + 1;

	idx = (seq % TDB_REPLAYMAX) / 32;
	packet = 1 << (31 - (seq & 31));

	/*
	 * We keep the high part intact when:
	 * 1) the SN is within [wl, 0xffffffff] and the whole window is
	 *    within one subspace;
	 * 2) the SN is within [0, wl) and window spans two subspaces.
	 */
	if ((tl >= window - 1 && seq >= wl) ||
	    (tl <  window - 1 && seq <  wl)) {
		*seqh = th;
		if (seq > tl) {
			if (commit) {
				if (seq - tl > window)
					memset(tdb->tdb_seen, 0,
					    sizeof(tdb->tdb_seen));
				else {
					int i = (tl % TDB_REPLAYMAX) / 32;

					while (i != idx) {
						i = (i + 1) % SEEN_SIZE;
						tdb->tdb_seen[i] = 0;
					}
				}
				tdb->tdb_seen[idx] |= packet;
				tdb->tdb_rpl = ((u_int64_t)*seqh << 32) | seq;
			}
		} else {
			if (tl - seq >= window)
				return (2);
			if (tdb->tdb_seen[idx] & packet)
				return (3);
			if (commit)
				tdb->tdb_seen[idx] |= packet;
		}
		return (0);
	}

	/* Can't wrap if not doing ESN */
	if (!esn)
		return (2);

	/*
	 * SN is within [wl, 0xffffffff] and wl is within
	 * [0xffffffff-window, 0xffffffff].  This means we got a SN
	 * which is within our replay window, but in the previous
	 * subspace.
	 */
	if (tl < window - 1 && seq >= wl) {
		if (tdb->tdb_seen[idx] & packet)
			return (3);
		*seqh = th - 1;
		if (commit)
			tdb->tdb_seen[idx] |= packet;
		return (0);
	}

	/*
	 * SN has wrapped and the last authenticated SN is in the old
	 * subspace.
	 */
	*seqh = th + 1;
	if (*seqh == 0)		/* Don't let high bit to wrap */
		return (1);
	if (commit) {
		if (seq - tl > window)
			memset(tdb->tdb_seen, 0, sizeof(tdb->tdb_seen));
		else {
			int i = (tl % TDB_REPLAYMAX) / 32;

			while (i != idx) {
				i = (i + 1) % SEEN_SIZE;
				tdb->tdb_seen[i] = 0;
			}
		}
		tdb->tdb_seen[idx] |= packet;
		tdb->tdb_rpl = ((u_int64_t)*seqh << 32) | seq;
	}

	return (0);
}
