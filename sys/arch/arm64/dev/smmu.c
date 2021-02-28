/* $OpenBSD: smmu.c,v 1.1 2021/02/28 21:39:31 patrick Exp $ */
/*
 * Copyright (c) 2008-2009,2014-2016 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2021 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>
#include <sys/pool.h>
#include <sys/atomic.h>

#include <machine/bus.h>

#include <uvm/uvm_extern.h>
#include <arm64/vmparam.h>
#include <arm64/pmap.h>

#include <dev/pci/pcivar.h>
#include <arm64/dev/smmuvar.h>
#include <arm64/dev/smmureg.h>

struct smmuvp0 {
	uint64_t l0[VP_IDX0_CNT];
	struct smmuvp1 *vp[VP_IDX0_CNT];
};

struct smmuvp1 {
	uint64_t l1[VP_IDX1_CNT];
	struct smmuvp2 *vp[VP_IDX1_CNT];
};

struct smmuvp2 {
	uint64_t l2[VP_IDX2_CNT];
	struct smmuvp3 *vp[VP_IDX2_CNT];
};

struct smmuvp3 {
	uint64_t l3[VP_IDX3_CNT];
	struct pte_desc *vp[VP_IDX3_CNT];
};

CTASSERT(sizeof(struct smmuvp0) == sizeof(struct smmuvp1));
CTASSERT(sizeof(struct smmuvp0) == sizeof(struct smmuvp2));
CTASSERT(sizeof(struct smmuvp0) == sizeof(struct smmuvp3));

struct pte_desc {
	LIST_ENTRY(pte_desc) pted_pv_list;
	uint64_t pted_pte;
	struct smmu_domain *pted_dom;
	vaddr_t pted_va;
};

uint32_t smmu_gr0_read_4(struct smmu_softc *, bus_size_t);
void smmu_gr0_write_4(struct smmu_softc *, bus_size_t, uint32_t);
uint32_t smmu_gr1_read_4(struct smmu_softc *, bus_size_t);
void smmu_gr1_write_4(struct smmu_softc *, bus_size_t, uint32_t);
uint32_t smmu_cb_read_4(struct smmu_softc *, int, bus_size_t);
void smmu_cb_write_4(struct smmu_softc *, int, bus_size_t, uint32_t);
uint64_t smmu_cb_read_8(struct smmu_softc *, int, bus_size_t);
void smmu_cb_write_8(struct smmu_softc *, int, bus_size_t, uint64_t);

void smmu_tlb_sync_global(struct smmu_softc *);
void smmu_tlb_sync_context(struct smmu_domain *);

struct smmu_domain *smmu_domain_lookup(struct smmu_softc *, uint32_t);
struct smmu_domain *smmu_domain_create(struct smmu_softc *, uint32_t);

void smmu_set_l1(struct smmu_domain *, uint64_t, struct smmuvp1 *);
void smmu_set_l2(struct smmu_domain *, uint64_t, struct smmuvp1 *,
    struct smmuvp2 *);
void smmu_set_l3(struct smmu_domain *, uint64_t, struct smmuvp2 *,
    struct smmuvp3 *);

struct pte_desc *smmu_vp_lookup(struct smmu_domain *, vaddr_t, uint64_t **);
int smmu_vp_enter(struct smmu_domain *, vaddr_t, struct pte_desc *, int);

void smmu_fill_pte(struct smmu_domain *, vaddr_t, paddr_t, struct pte_desc *,
    vm_prot_t, int, int);
void smmu_pte_update(struct pte_desc *, uint64_t *);
void smmu_pte_insert(struct pte_desc *);
void smmu_pte_remove(struct pte_desc *);

int smmu_prepare(struct smmu_domain *, vaddr_t, paddr_t, vm_prot_t, int, int);
int smmu_enter(struct smmu_domain *, vaddr_t, paddr_t, vm_prot_t, int, int);
void smmu_remove(struct smmu_domain *, vaddr_t);

int smmu_load_map(struct smmu_domain *, bus_dmamap_t);
void smmu_unload_map(struct smmu_domain *, bus_dmamap_t);

int smmu_dmamap_create(bus_dma_tag_t , bus_size_t, int,
     bus_size_t, bus_size_t, int, bus_dmamap_t *);
void smmu_dmamap_destroy(bus_dma_tag_t , bus_dmamap_t);
int smmu_dmamap_load(bus_dma_tag_t , bus_dmamap_t, void *,
     bus_size_t, struct proc *, int);
int smmu_dmamap_load_mbuf(bus_dma_tag_t , bus_dmamap_t,
     struct mbuf *, int);
int smmu_dmamap_load_uio(bus_dma_tag_t , bus_dmamap_t,
     struct uio *, int);
int smmu_dmamap_load_raw(bus_dma_tag_t , bus_dmamap_t,
     bus_dma_segment_t *, int, bus_size_t, int);
void smmu_dmamap_unload(bus_dma_tag_t , bus_dmamap_t);
void smmu_dmamap_sync(bus_dma_tag_t , bus_dmamap_t,
     bus_addr_t, bus_size_t, int);

int smmu_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t,
     bus_size_t, bus_dma_segment_t *, int, int *, int);
void smmu_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int smmu_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *,
     int, size_t, caddr_t *, int);
void smmu_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);
paddr_t smmu_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *,
     int, off_t, int, int);

struct cfdriver smmu_cd = {
	NULL, "smmu", DV_DULL
};

int
smmu_attach(struct smmu_softc *sc)
{
	uint32_t reg;
	int i;

	printf("\n");

	SIMPLEQ_INIT(&sc->sc_domains);

	pool_init(&sc->sc_pted_pool, sizeof(struct pte_desc), 0, IPL_VM, 0,
	    "smmu_pted", NULL);
	pool_setlowat(&sc->sc_pted_pool, 20);
	pool_init(&sc->sc_vp_pool, sizeof(struct smmuvp0), PAGE_SIZE, IPL_VM, 0,
	    "smmu_vp", NULL);
	pool_setlowat(&sc->sc_vp_pool, 20);

	reg = smmu_gr0_read_4(sc, SMMU_IDR0);
	if (reg & SMMU_IDR0_S1TS)
		sc->sc_has_s1 = 1;
	/*
	 * Marvell's 8040 does not support 64-bit writes, hence it
	 * is not possible to invalidate stage-2, because the ASID
	 * is part of the upper 32-bits and they'd be ignored.
	 */
	if (sc->sc_is_ap806)
		sc->sc_has_s1 = 0;
	if (reg & SMMU_IDR0_S2TS)
		sc->sc_has_s2 = 1;
	if (!sc->sc_has_s1 && !sc->sc_has_s2)
		return 1;
	if (reg & SMMU_IDR0_EXIDS)
		sc->sc_has_exids = 1;

	sc->sc_num_streams = 1 << SMMU_IDR0_NUMSIDB(reg);
	if (sc->sc_has_exids)
		sc->sc_num_streams = 1 << 16;
	sc->sc_stream_mask = sc->sc_num_streams - 1;
	if (reg & SMMU_IDR0_SMS) {
		sc->sc_num_streams = SMMU_IDR0_NUMSMRG(reg);
		if (sc->sc_num_streams == 0)
			return 1;
		sc->sc_smr = mallocarray(sc->sc_num_streams,
		    sizeof(*sc->sc_smr), M_DEVBUF, M_WAITOK | M_ZERO);
	}

	reg = smmu_gr0_read_4(sc, SMMU_IDR1);
	sc->sc_pagesize = 4 * 1024;
	if (reg & SMMU_IDR1_PAGESIZE_64K)
		sc->sc_pagesize = 64 * 1024;
	sc->sc_numpage = 1 << (SMMU_IDR1_NUMPAGENDXB(reg) + 1);

	/* 0 to NUMS2CB == stage-2, NUMS2CB to NUMCB == stage-1 */
	sc->sc_num_context_banks = SMMU_IDR1_NUMCB(reg);
	sc->sc_num_s2_context_banks = SMMU_IDR1_NUMS2CB(reg);
	if (sc->sc_num_s2_context_banks > sc->sc_num_context_banks)
		return 1;
	sc->sc_cb = mallocarray(sc->sc_num_context_banks,
	    sizeof(*sc->sc_cb), M_DEVBUF, M_WAITOK | M_ZERO);

	reg = smmu_gr0_read_4(sc, SMMU_IDR2);
	if (reg & SMMU_IDR2_VMID16S)
		sc->sc_has_vmid16s = 1;

	switch (SMMU_IDR2_IAS(reg)) {
	case SMMU_IDR2_IAS_32BIT:
		sc->sc_ipa_bits = 32;
		break;
	case SMMU_IDR2_IAS_36BIT:
		sc->sc_ipa_bits = 36;
		break;
	case SMMU_IDR2_IAS_40BIT:
		sc->sc_ipa_bits = 40;
		break;
	case SMMU_IDR2_IAS_42BIT:
		sc->sc_ipa_bits = 42;
		break;
	case SMMU_IDR2_IAS_44BIT:
		sc->sc_ipa_bits = 44;
		break;
	case SMMU_IDR2_IAS_48BIT:
	default:
		sc->sc_ipa_bits = 48;
		break;
	}
	switch (SMMU_IDR2_OAS(reg)) {
	case SMMU_IDR2_OAS_32BIT:
		sc->sc_pa_bits = 32;
		break;
	case SMMU_IDR2_OAS_36BIT:
		sc->sc_pa_bits = 36;
		break;
	case SMMU_IDR2_OAS_40BIT:
		sc->sc_pa_bits = 40;
		break;
	case SMMU_IDR2_OAS_42BIT:
		sc->sc_pa_bits = 42;
		break;
	case SMMU_IDR2_OAS_44BIT:
		sc->sc_pa_bits = 44;
		break;
	case SMMU_IDR2_OAS_48BIT:
	default:
		sc->sc_pa_bits = 48;
		break;
	}
	switch (SMMU_IDR2_UBS(reg)) {
	case SMMU_IDR2_UBS_32BIT:
		sc->sc_va_bits = 32;
		break;
	case SMMU_IDR2_UBS_36BIT:
		sc->sc_va_bits = 36;
		break;
	case SMMU_IDR2_UBS_40BIT:
		sc->sc_va_bits = 40;
		break;
	case SMMU_IDR2_UBS_42BIT:
		sc->sc_va_bits = 42;
		break;
	case SMMU_IDR2_UBS_44BIT:
		sc->sc_va_bits = 44;
		break;
	case SMMU_IDR2_UBS_49BIT:
	default:
		sc->sc_va_bits = 48;
		break;
	}

#if 1
	reg = smmu_gr0_read_4(sc, SMMU_IDR0);
	printf("%s: idr0 0x%08x\n", __func__, reg);
	reg = smmu_gr0_read_4(sc, SMMU_IDR1);
	printf("%s: idr1 0x%08x\n", __func__, reg);
	reg = smmu_gr0_read_4(sc, SMMU_IDR2);
	printf("%s: idr2 0x%08x\n", __func__, reg);

	printf("%s: pagesize %zu numpage %u\n", __func__, sc->sc_pagesize,
	    sc->sc_numpage);
	printf("%s: total cb %u stage2-only cb %u\n", __func__,
	    sc->sc_num_context_banks, sc->sc_num_s2_context_banks);
#endif

	/* Clear Global Fault Status Register */
	smmu_gr0_write_4(sc, SMMU_SGFSR, smmu_gr0_read_4(sc, SMMU_SGFSR));

	for (i = 0; i < sc->sc_num_streams; i++) {
#if 1
		/* Setup all streams to fault by default */
		smmu_gr0_write_4(sc, SMMU_S2CR(i), SMMU_S2CR_TYPE_FAULT);
#else
		/* For stream indexing, USFCFG bypass isn't enough! */
		smmu_gr0_write_4(sc, SMMU_S2CR(i), SMMU_S2CR_TYPE_BYPASS);
#endif
		/*  Disable all stream map registers */
		if (sc->sc_smr)
			smmu_gr0_write_4(sc, SMMU_SMR(i), 0);
	}

	for (i = 0; i < sc->sc_num_context_banks; i++) {
		/* Disable Context Bank */
		smmu_cb_write_4(sc, i, SMMU_CB_SCTLR, 0);
		/* Clear Context Bank Fault Status Register */
		smmu_cb_write_4(sc, i, SMMU_CB_FSR, SMMU_CB_FSR_MASK);
	}

	/* Invalidate TLB */
	smmu_gr0_write_4(sc, SMMU_TLBIALLH, ~0);
	smmu_gr0_write_4(sc, SMMU_TLBIALLNSNH, ~0);

	if (sc->sc_is_mmu500) {
		reg = smmu_gr0_read_4(sc, SMMU_SACR);
		if (SMMU_IDR7_MAJOR(smmu_gr0_read_4(sc, SMMU_IDR7)) >= 2)
			reg &= ~SMMU_SACR_MMU500_CACHE_LOCK;
		reg |= SMMU_SACR_MMU500_SMTNMB_TLBEN |
		    SMMU_SACR_MMU500_S2CRB_TLBEN;
		smmu_gr0_write_4(sc, SMMU_SACR, reg);
		for (i = 0; i < sc->sc_num_context_banks; i++) {
			reg = smmu_cb_read_4(sc, i, SMMU_CB_ACTLR);
			reg &= ~SMMU_CB_ACTLR_CPRE;
			smmu_cb_write_4(sc, i, SMMU_CB_ACTLR, reg);
		}
	}

	/* Enable SMMU */
	reg = smmu_gr0_read_4(sc, SMMU_SCR0);
	reg &= ~(SMMU_SCR0_CLIENTPD |
	    SMMU_SCR0_FB | SMMU_SCR0_BSU_MASK);
#if 1
	/* Disable bypass for unknown streams */
	reg |= SMMU_SCR0_USFCFG;
#else
	/* Enable bypass for unknown streams */
	reg &= ~SMMU_SCR0_USFCFG;
#endif
	reg |= SMMU_SCR0_GFRE | SMMU_SCR0_GFIE |
	    SMMU_SCR0_GCFGFRE | SMMU_SCR0_GCFGFIE |
	    SMMU_SCR0_VMIDPNE | SMMU_SCR0_PTM;
	if (sc->sc_has_exids)
		reg |= SMMU_SCR0_EXIDENABLE;
	if (sc->sc_has_vmid16s)
		reg |= SMMU_SCR0_VMID16EN;

	smmu_tlb_sync_global(sc);
	smmu_gr0_write_4(sc, SMMU_SCR0, reg);

	return 0;
}

int
smmu_global_irq(void *cookie)
{
	struct smmu_softc *sc = cookie;
	uint32_t reg;

	reg = smmu_gr0_read_4(sc, SMMU_SGFSR);
	if (reg == 0)
		return 0;

	printf("%s:%d: SGFSR 0x%08x SGFSYNR0 0x%08x SGFSYNR1 0x%08x "
	    "SGFSYNR2 0x%08x\n", __func__, __LINE__, reg,
	    smmu_gr0_read_4(sc, SMMU_SGFSYNR0),
	    smmu_gr0_read_4(sc, SMMU_SGFSYNR1),
	    smmu_gr0_read_4(sc, SMMU_SGFSYNR2));

	smmu_gr0_write_4(sc, SMMU_SGFSR, reg);

	return 1;
}

int
smmu_context_irq(void *cookie)
{
	struct smmu_cb_irq *cbi = cookie;
	struct smmu_softc *sc = cbi->cbi_sc;
	uint32_t reg;

	reg = smmu_cb_read_4(sc, cbi->cbi_idx, SMMU_CB_FSR);
	if ((reg & SMMU_CB_FSR_MASK) == 0)
		return 0;

	printf("%s:%d: FSR 0x%08x FSYNR0 0x%08x FAR 0x%llx "
	    "CBFRSYNRA 0x%08x\n", __func__, __LINE__, reg,
	    smmu_cb_read_4(sc, cbi->cbi_idx, SMMU_CB_FSYNR0),
	    smmu_cb_read_8(sc, cbi->cbi_idx, SMMU_CB_FAR),
	    smmu_gr1_read_4(sc, SMMU_CBFRSYNRA(cbi->cbi_idx)));

	smmu_cb_write_4(sc, cbi->cbi_idx, SMMU_CB_FSR, reg);

	return 1;
}

void
smmu_tlb_sync_global(struct smmu_softc *sc)
{
	int i;

	smmu_gr0_write_4(sc, SMMU_STLBGSYNC, ~0);
	for (i = 1000; i > 0; i--) {
		if ((smmu_gr0_read_4(sc, SMMU_STLBGSTATUS) &
		    SMMU_STLBGSTATUS_GSACTIVE) == 0)
			return;
		delay(1000);
	}

	printf("%s: global TLB sync timeout\n",
	    sc->sc_dev.dv_xname);
}

void
smmu_tlb_sync_context(struct smmu_domain *dom)
{
	struct smmu_softc *sc = dom->sd_sc;
	int i;

	smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_TLBSYNC, ~0);
	for (i = 1000; i > 0; i--) {
		if ((smmu_cb_read_4(sc, dom->sd_cb_idx, SMMU_CB_TLBSTATUS) &
		    SMMU_CB_TLBSTATUS_SACTIVE) == 0)
			return;
		delay(1000);
	}

	printf("%s: context TLB sync timeout\n",
	    sc->sc_dev.dv_xname);
}

uint32_t
smmu_gr0_read_4(struct smmu_softc *sc, bus_size_t off)
{
	uint32_t base = 0 * sc->sc_pagesize;

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_gr0_write_4(struct smmu_softc *sc, bus_size_t off, uint32_t val)
{
	uint32_t base = 0 * sc->sc_pagesize;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off, val);
}

uint32_t
smmu_gr1_read_4(struct smmu_softc *sc, bus_size_t off)
{
	uint32_t base = 1 * sc->sc_pagesize;

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_gr1_write_4(struct smmu_softc *sc, bus_size_t off, uint32_t val)
{
	uint32_t base = 1 * sc->sc_pagesize;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off, val);
}

uint32_t
smmu_cb_read_4(struct smmu_softc *sc, int idx, bus_size_t off)
{
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_cb_write_4(struct smmu_softc *sc, int idx, bus_size_t off, uint32_t val)
{
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off, val);
}

uint64_t
smmu_cb_read_8(struct smmu_softc *sc, int idx, bus_size_t off)
{
	uint64_t reg;
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	if (sc->sc_is_ap806) {
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off + 4);
		reg <<= 32;
		reg |= bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off + 0);
		return reg;
	}

	return bus_space_read_8(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_cb_write_8(struct smmu_softc *sc, int idx, bus_size_t off, uint64_t val)
{
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	if (sc->sc_is_ap806) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off + 4,
		    val >> 32);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off + 0,
		    val & 0xffffffff);
		return;
	}

	bus_space_write_8(sc->sc_iot, sc->sc_ioh, base + off, val);
}

bus_dma_tag_t
smmu_device_hook(struct smmu_softc *sc, uint32_t sid, bus_dma_tag_t dmat)
{
	struct smmu_domain *dom;

	dom = smmu_domain_lookup(sc, sid);
	if (dom == NULL)
		return dmat;
	if (dom->sd_parent_dmat == NULL)
		dom->sd_parent_dmat = dmat;
	if (dom->sd_parent_dmat != dmat &&
	    memcmp(dom->sd_parent_dmat, dmat, sizeof(*dmat)) != 0) {
		printf("%s: different dma tag for same stream\n",
		    sc->sc_dev.dv_xname);
		return dmat;
	}

	if (dom->sd_device_dmat == NULL) {
		dom->sd_device_dmat = malloc(sizeof(*dom->sd_device_dmat),
		    M_DEVBUF, M_WAITOK);
		memcpy(dom->sd_device_dmat, dom->sd_parent_dmat,
		    sizeof(*dom->sd_device_dmat));
		dom->sd_device_dmat->_cookie = dom;
		dom->sd_device_dmat->_dmamap_create = smmu_dmamap_create;
		dom->sd_device_dmat->_dmamap_destroy = smmu_dmamap_destroy;
		dom->sd_device_dmat->_dmamap_load = smmu_dmamap_load;
		dom->sd_device_dmat->_dmamap_load_mbuf = smmu_dmamap_load_mbuf;
		dom->sd_device_dmat->_dmamap_load_uio = smmu_dmamap_load_uio;
		dom->sd_device_dmat->_dmamap_load_raw = smmu_dmamap_load_raw;
		dom->sd_device_dmat->_dmamap_unload = smmu_dmamap_unload;
		dom->sd_device_dmat->_dmamap_sync = smmu_dmamap_sync;
		dom->sd_device_dmat->_dmamem_map = smmu_dmamem_map;
	}

	return dom->sd_device_dmat;
}

void
smmu_pci_device_hook(void *cookie, uint32_t rid, struct pci_attach_args *pa)
{
	struct smmu_softc *sc = cookie;

	printf("%s: rid %x\n", sc->sc_dev.dv_xname, rid);
	pa->pa_dmat = smmu_device_hook(sc, rid, pa->pa_dmat);
}

struct smmu_domain *
smmu_domain_lookup(struct smmu_softc *sc, uint32_t sid)
{
	struct smmu_domain *dom;

	printf("%s: looking for %x\n", sc->sc_dev.dv_xname, sid);
	SIMPLEQ_FOREACH(dom, &sc->sc_domains, sd_list) {
		if (dom->sd_sid == sid)
			return dom;
	}

	return smmu_domain_create(sc, sid);
}

struct smmu_domain *
smmu_domain_create(struct smmu_softc *sc, uint32_t sid)
{
	struct smmu_domain *dom;
	uint32_t iovabits, reg;
	paddr_t pa;
	vaddr_t l0va;
	int i, start, end;

	printf("%s: creating for %x\n", sc->sc_dev.dv_xname, sid);
	dom = malloc(sizeof(*dom), M_DEVBUF, M_WAITOK | M_ZERO);
	mtx_init(&dom->sd_mtx, IPL_TTY);
	dom->sd_sc = sc;
	dom->sd_sid = sid;

	/* Prefer stage 1 if possible! */
	if (sc->sc_has_s1) {
		start = sc->sc_num_s2_context_banks;
		end = sc->sc_num_context_banks;
		dom->sd_stage = 1;
	} else {
		start = 0;
		end = sc->sc_num_context_banks;
		dom->sd_stage = 2;
	}

	for (i = start; i < end; i++) {
		if (sc->sc_cb[i] != NULL)
			continue;
		sc->sc_cb[i] = malloc(sizeof(struct smmu_cb),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		dom->sd_cb_idx = i;
		break;
	}
	if (i >= end) {
		free(dom, M_DEVBUF, sizeof(*dom));
		return NULL;
	}

	/* Stream indexing is easy */
	dom->sd_smr_idx = sid;

	/* Stream mapping is a bit more effort */
	if (sc->sc_smr) {
		for (i = 0; i < sc->sc_num_streams; i++) {
			if (sc->sc_smr[i] != NULL)
				continue;
			sc->sc_smr[i] = malloc(sizeof(struct smmu_smr),
			    M_DEVBUF, M_WAITOK | M_ZERO);
			dom->sd_smr_idx = i;
			break;
		}

		if (i >= sc->sc_num_streams) {
			free(sc->sc_cb[dom->sd_cb_idx], M_DEVBUF,
			    sizeof(struct smmu_cb));
			sc->sc_cb[dom->sd_cb_idx] = NULL;
			free(dom, M_DEVBUF, sizeof(*dom));
			return NULL;
		}
	}

	reg = SMMU_CBA2R_VA64;
	if (sc->sc_has_vmid16s)
		reg |= (dom->sd_cb_idx + 1) << SMMU_CBA2R_VMID16_SHIFT;
	smmu_gr1_write_4(sc, SMMU_CBA2R(dom->sd_cb_idx), reg);

	if (dom->sd_stage == 1) {
		reg = SMMU_CBAR_TYPE_S1_TRANS_S2_BYPASS |
		    SMMU_CBAR_BPSHCFG_NSH | SMMU_CBAR_MEMATTR_WB;
	} else {
		reg = SMMU_CBAR_TYPE_S2_TRANS;
		if (!sc->sc_has_vmid16s)
			reg |= (dom->sd_cb_idx + 1) << SMMU_CBAR_VMID_SHIFT;
	}
	smmu_gr1_write_4(sc, SMMU_CBAR(dom->sd_cb_idx), reg);

	if (dom->sd_stage == 1) {
		reg = SMMU_CB_TCR2_AS | SMMU_CB_TCR2_SEP_UPSTREAM;
		switch (sc->sc_ipa_bits) {
		case 32:
			reg |= SMMU_CB_TCR2_PASIZE_32BIT;
			break;
		case 36:
			reg |= SMMU_CB_TCR2_PASIZE_36BIT;
			break;
		case 40:
			reg |= SMMU_CB_TCR2_PASIZE_40BIT;
			break;
		case 42:
			reg |= SMMU_CB_TCR2_PASIZE_42BIT;
			break;
		case 44:
			reg |= SMMU_CB_TCR2_PASIZE_44BIT;
			break;
		case 48:
			reg |= SMMU_CB_TCR2_PASIZE_48BIT;
			break;
		}
		smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_TCR2, reg);
	}

	if (dom->sd_stage == 1)
		iovabits = sc->sc_va_bits;
	else
		iovabits = sc->sc_ipa_bits;
	/*
	 * Marvell's 8040 does not support 64-bit writes, hence we
	 * can only address 44-bits of VA space for TLB invalidation.
	 */
	if (sc->sc_is_ap806)
		iovabits = min(44, iovabits);
	if (iovabits >= 40)
		dom->sd_4level = 1;

	reg = SMMU_CB_TCR_TG0_4KB | SMMU_CB_TCR_T0SZ(64 - iovabits);
	if (dom->sd_stage == 1) {
		reg |= SMMU_CB_TCR_EPD1;
	} else {
		if (dom->sd_4level)
			reg |= SMMU_CB_TCR_S2_SL0_4KB_L0;
		else
			reg |= SMMU_CB_TCR_S2_SL0_4KB_L1;
		switch (sc->sc_pa_bits) {
		case 32:
			reg |= SMMU_CB_TCR_S2_PASIZE_32BIT;
			break;
		case 36:
			reg |= SMMU_CB_TCR_S2_PASIZE_36BIT;
			break;
		case 40:
			reg |= SMMU_CB_TCR_S2_PASIZE_40BIT;
			break;
		case 42:
			reg |= SMMU_CB_TCR_S2_PASIZE_42BIT;
			break;
		case 44:
			reg |= SMMU_CB_TCR_S2_PASIZE_44BIT;
			break;
		case 48:
			reg |= SMMU_CB_TCR_S2_PASIZE_48BIT;
			break;
		}
	}
	if (sc->sc_coherent)
		reg |= SMMU_CB_TCR_IRGN0_WBWA | SMMU_CB_TCR_ORGN0_WBWA |
		    SMMU_CB_TCR_SH0_ISH;
	else
		reg |= SMMU_CB_TCR_IRGN0_NC | SMMU_CB_TCR_ORGN0_NC |
		    SMMU_CB_TCR_SH0_OSH;
	smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_TCR, reg);

	if (dom->sd_4level) {
		while (dom->sd_vp.l0 == NULL) {
			dom->sd_vp.l0 = pool_get(&sc->sc_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t)dom->sd_vp.l0->l0; /* top level is l0 */
	} else {
		while (dom->sd_vp.l1 == NULL) {
			dom->sd_vp.l1 = pool_get(&sc->sc_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t)dom->sd_vp.l1->l1; /* top level is l1 */
	}
	pmap_extract(pmap_kernel(), l0va, &pa);

	if (dom->sd_stage == 1) {
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TTBR0,
		    (uint64_t)dom->sd_cb_idx << SMMU_CB_TTBR_ASID_SHIFT | pa);
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TTBR1,
		    (uint64_t)dom->sd_cb_idx << SMMU_CB_TTBR_ASID_SHIFT);
	} else
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TTBR0, pa);

	if (dom->sd_stage == 1) {
		smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_MAIR0,
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_nGnRnE, 0) |
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_nGnRE, 1) |
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_NC, 2) |
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_WB, 3));
		smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_MAIR1,
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_WT, 0));
	}

	reg = SMMU_CB_SCTLR_M | SMMU_CB_SCTLR_TRE | SMMU_CB_SCTLR_AFE |
	    SMMU_CB_SCTLR_CFRE | SMMU_CB_SCTLR_CFIE;
	if (dom->sd_stage == 1)
		reg |= SMMU_CB_SCTLR_ASIDPNE;
	smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_SCTLR, reg);

	/* Point stream to context block */
	reg = SMMU_S2CR_TYPE_TRANS | dom->sd_cb_idx;
	if (sc->sc_has_exids && sc->sc_smr)
		reg |= SMMU_S2CR_EXIDVALID;
	smmu_gr0_write_4(sc, SMMU_S2CR(dom->sd_smr_idx), reg);

	/* Map stream idx to S2CR idx */
	if (sc->sc_smr) {
		reg = sid;
		if (!sc->sc_has_exids)
			reg |= SMMU_SMR_VALID;
		smmu_gr0_write_4(sc, SMMU_SMR(dom->sd_smr_idx), reg);
	}

	snprintf(dom->sd_exname, sizeof(dom->sd_exname), "%s:%x",
	    sc->sc_dev.dv_xname, sid);
	dom->sd_iovamap = extent_create(dom->sd_exname, 0, (1LL << iovabits)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_NOCOALESCE);

#if 0
	/* FIXME MSI map on */
	{
		paddr_t msi_pa = 0x78020000; /* Ampere */
		paddr_t msi_pa = 0x6020000; /* LX2K */
		size_t msi_len = 0x20000;
		paddr_t msi_pa = 0x280000; /* 8040 */
		size_t msi_len = 0x4000;
		while (msi_len) {
			smmu_enter(dom, msi_pa, msi_pa, PROT_READ | PROT_WRITE,
			    PROT_READ | PROT_WRITE, PMAP_CACHE_WB);
			msi_pa += PAGE_SIZE;
			msi_len -= PAGE_SIZE;
		}
	}
#endif

	SIMPLEQ_INSERT_TAIL(&sc->sc_domains, dom, sd_list);
	return dom;
}

/* basically pmap follows */

/* virtual to physical helpers */
static inline int
VP_IDX0(vaddr_t va)
{
	return (va >> VP_IDX0_POS) & VP_IDX0_MASK;
}

static inline int
VP_IDX1(vaddr_t va)
{
	return (va >> VP_IDX1_POS) & VP_IDX1_MASK;
}

static inline int
VP_IDX2(vaddr_t va)
{
	return (va >> VP_IDX2_POS) & VP_IDX2_MASK;
}

static inline int
VP_IDX3(vaddr_t va)
{
	return (va >> VP_IDX3_POS) & VP_IDX3_MASK;
}

static inline uint64_t
VP_Lx(paddr_t pa)
{
	/*
	 * This function takes the pa address given and manipulates it
	 * into the form that should be inserted into the VM table.
	 */
	return pa | Lx_TYPE_PT;
}

void
smmu_set_l1(struct smmu_domain *dom, uint64_t va, struct smmuvp1 *l1_va)
{
	uint64_t pg_entry;
	paddr_t l1_pa;
	int idx0;

	if (pmap_extract(pmap_kernel(), (vaddr_t)l1_va, &l1_pa) == 0)
		panic("unable to find vp pa mapping %p\n", l1_va);

	if (l1_pa & (Lx_TABLE_ALIGN-1))
		panic("misaligned L2 table\n");

	pg_entry = VP_Lx(l1_pa);

	idx0 = VP_IDX0(va);
	dom->sd_vp.l0->vp[idx0] = l1_va;
	dom->sd_vp.l0->l0[idx0] = pg_entry;
}

void
smmu_set_l2(struct smmu_domain *dom, uint64_t va, struct smmuvp1 *vp1,
    struct smmuvp2 *l2_va)
{
	uint64_t pg_entry;
	paddr_t l2_pa;
	int idx1;

	if (pmap_extract(pmap_kernel(), (vaddr_t)l2_va, &l2_pa) == 0)
		panic("unable to find vp pa mapping %p\n", l2_va);

	if (l2_pa & (Lx_TABLE_ALIGN-1))
		panic("misaligned L2 table\n");

	pg_entry = VP_Lx(l2_pa);

	idx1 = VP_IDX1(va);
	vp1->vp[idx1] = l2_va;
	vp1->l1[idx1] = pg_entry;
}

void
smmu_set_l3(struct smmu_domain *dom, uint64_t va, struct smmuvp2 *vp2,
    struct smmuvp3 *l3_va)
{
	uint64_t pg_entry;
	paddr_t l3_pa;
	int idx2;

	if (pmap_extract(pmap_kernel(), (vaddr_t)l3_va, &l3_pa) == 0)
		panic("unable to find vp pa mapping %p\n", l3_va);

	if (l3_pa & (Lx_TABLE_ALIGN-1))
		panic("misaligned L2 table\n");

	pg_entry = VP_Lx(l3_pa);

	idx2 = VP_IDX2(va);
	vp2->vp[idx2] = l3_va;
	vp2->l2[idx2] = pg_entry;
}

struct pte_desc *
smmu_vp_lookup(struct smmu_domain *dom, vaddr_t va, uint64_t **pl3entry)
{
	struct smmuvp1 *vp1;
	struct smmuvp2 *vp2;
	struct smmuvp3 *vp3;
	struct pte_desc *pted;

	if (dom->sd_4level) {
		if (dom->sd_vp.l0 == NULL) {
			return NULL;
		}
		vp1 = dom->sd_vp.l0->vp[VP_IDX0(va)];
	} else {
		vp1 = dom->sd_vp.l1;
	}
	if (vp1 == NULL) {
		return NULL;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		return NULL;
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		return NULL;
	}

	pted = vp3->vp[VP_IDX3(va)];
	if (pl3entry != NULL)
		*pl3entry = &(vp3->l3[VP_IDX3(va)]);

	return pted;
}

int
smmu_vp_enter(struct smmu_domain *dom, vaddr_t va, struct pte_desc *pted,
    int flags)
{
	struct smmu_softc *sc = dom->sd_sc;
	struct smmuvp1 *vp1;
	struct smmuvp2 *vp2;
	struct smmuvp3 *vp3;

	if (dom->sd_4level) {
		vp1 = dom->sd_vp.l0->vp[VP_IDX0(va)];
		if (vp1 == NULL) {
			vp1 = pool_get(&sc->sc_vp_pool, PR_NOWAIT | PR_ZERO);
			if (vp1 == NULL) {
				if ((flags & PMAP_CANFAIL) == 0)
					panic("%s: unable to allocate L1",
					    __func__);
				return ENOMEM;
			}
			smmu_set_l1(dom, va, vp1);
		}
	} else {
		vp1 = dom->sd_vp.l1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		vp2 = pool_get(&sc->sc_vp_pool, PR_NOWAIT | PR_ZERO);
		if (vp2 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: unable to allocate L2", __func__);
			return ENOMEM;
		}
		smmu_set_l2(dom, va, vp1, vp2);
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		vp3 = pool_get(&sc->sc_vp_pool, PR_NOWAIT | PR_ZERO);
		if (vp3 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: unable to allocate L3", __func__);
			return ENOMEM;
		}
		smmu_set_l3(dom, va, vp2, vp3);
	}

	vp3->vp[VP_IDX3(va)] = pted;
	return 0;
}

void
smmu_fill_pte(struct smmu_domain *dom, vaddr_t va, paddr_t pa,
    struct pte_desc *pted, vm_prot_t prot, int flags, int cache)
{
	pted->pted_va = va;
	pted->pted_dom = dom;

	switch (cache) {
	case PMAP_CACHE_WB:
		break;
	case PMAP_CACHE_WT:
		break;
	case PMAP_CACHE_CI:
		break;
	case PMAP_CACHE_DEV_NGNRNE:
		break;
	case PMAP_CACHE_DEV_NGNRE:
		break;
	default:
		panic("%s: invalid cache mode", __func__);
	}
	pted->pted_va |= cache;

	pted->pted_va |= prot & (PROT_READ|PROT_WRITE|PROT_EXEC);

	pted->pted_pte = pa & PTE_RPGN;
	pted->pted_pte |= flags & (PROT_READ|PROT_WRITE|PROT_EXEC);
}

void
smmu_pte_update(struct pte_desc *pted, uint64_t *pl3)
{
	struct smmu_domain *dom = pted->pted_dom;
	uint64_t pte, access_bits;
	uint64_t attr = 0;

	/* see mair in locore.S */
	switch (pted->pted_va & PMAP_CACHE_BITS) {
	case PMAP_CACHE_WB:
		/* inner and outer writeback */
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_WB);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_WB);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_WT:
		/* inner and outer writethrough */
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_WT);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_WT);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_CI:
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_CI);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_CI);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_DEV_NGNRNE:
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_DEV_NGNRNE);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_DEV_NGNRNE);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_DEV_NGNRE:
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_DEV_NGNRE);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_DEV_NGNRE);
		attr |= ATTR_SH(SH_INNER);
		break;
	default:
		panic("%s: invalid cache mode", __func__);
	}

	access_bits = ATTR_PXN | ATTR_AF;
	if (dom->sd_stage == 1) {
		attr |= ATTR_nG;
		access_bits |= ATTR_AP(1);
		if ((pted->pted_pte & PROT_READ) &&
		    !(pted->pted_pte & PROT_WRITE))
			access_bits |= ATTR_AP(2);
	} else {
		if (pted->pted_pte & PROT_READ)
			access_bits |= ATTR_AP(1);
		if (pted->pted_pte & PROT_WRITE)
			access_bits |= ATTR_AP(2);
	}

	pte = (pted->pted_pte & PTE_RPGN) | attr | access_bits | L3_P;
	*pl3 = pte;
}

void
smmu_pte_insert(struct pte_desc *pted)
{
	struct smmu_domain *dom = pted->pted_dom;
	uint64_t *pl3;

	if (smmu_vp_lookup(dom, pted->pted_va, &pl3) == NULL) {
		panic("%s: have a pted, but missing a vp"
		    " for %lx va domain %p", __func__, pted->pted_va, dom);
	}

	smmu_pte_update(pted, pl3);
	membar_producer(); /* XXX bus dma sync? */
}

void
smmu_pte_remove(struct pte_desc *pted)
{
	/* put entry into table */
	/* need to deal with ref/change here */
	struct smmuvp1 *vp1;
	struct smmuvp2 *vp2;
	struct smmuvp3 *vp3;
	struct smmu_domain *dom = pted->pted_dom;

	if (dom->sd_4level)
		vp1 = dom->sd_vp.l0->vp[VP_IDX0(pted->pted_va)];
	else
		vp1 = dom->sd_vp.l1;
	if (vp1 == NULL) {
		panic("have a pted, but missing the l1 for %lx va domain %p",
		    pted->pted_va, dom);
	}
	vp2 = vp1->vp[VP_IDX1(pted->pted_va)];
	if (vp2 == NULL) {
		panic("have a pted, but missing the l2 for %lx va domain %p",
		    pted->pted_va, dom);
	}
	vp3 = vp2->vp[VP_IDX2(pted->pted_va)];
	if (vp3 == NULL) {
		panic("have a pted, but missing the l3 for %lx va domain %p",
		    pted->pted_va, dom);
	}
	vp3->l3[VP_IDX3(pted->pted_va)] = 0;
	vp3->vp[VP_IDX3(pted->pted_va)] = NULL;
}

int
smmu_prepare(struct smmu_domain *dom, vaddr_t va, paddr_t pa, vm_prot_t prot,
    int flags, int cache)
{
	struct smmu_softc *sc = dom->sd_sc;
	struct pte_desc *pted;
	int error;

	/* printf("%s: 0x%lx -> 0x%lx\n", __func__, va, pa); */

	pted = smmu_vp_lookup(dom, va, NULL);
	if (pted == NULL) {
		pted = pool_get(&sc->sc_pted_pool, PR_NOWAIT | PR_ZERO);
		if (pted == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: failed to allocate pted", __func__);
			error = ENOMEM;
			goto out;
		}
		if (smmu_vp_enter(dom, va, pted, flags)) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: failed to allocate L2/L3", __func__);
			error = ENOMEM;
			pool_put(&sc->sc_pted_pool, pted);
			goto out;
		}
	}

	smmu_fill_pte(dom, va, pa, pted, prot, flags, cache);

	error = 0;
out:
	return error;
}

int
smmu_enter(struct smmu_domain *dom, vaddr_t va, paddr_t pa, vm_prot_t prot,
    int flags, int cache)
{
	struct pte_desc *pted;
	int error;

	/* printf("%s: 0x%lx -> 0x%lx\n", __func__, va, pa); */

	pted = smmu_vp_lookup(dom, va, NULL);
	if (pted == NULL) {
		error = smmu_prepare(dom, va, pa, prot, PROT_NONE, cache);
		if (error)
			goto out;
		pted = smmu_vp_lookup(dom, va, NULL);
		KASSERT(pted != NULL);
	}

	pted->pted_pte |=
	    (pted->pted_va & (PROT_READ|PROT_WRITE|PROT_EXEC));
	smmu_pte_insert(pted);

	error = 0;
out:
	return error;
}

void
smmu_remove(struct smmu_domain *dom, vaddr_t va)
{
	struct smmu_softc *sc = dom->sd_sc;
	struct pte_desc *pted;

	/* printf("%s: 0x%lx\n", __func__, va); */

	pted = smmu_vp_lookup(dom, va, NULL);
	if (pted == NULL) /* XXX really? */
		return;

	smmu_pte_remove(pted);
	pted->pted_pte = 0;
	pted->pted_va = 0;
	pool_put(&sc->sc_pted_pool, pted);

	membar_producer(); /* XXX bus dma sync? */
	if (dom->sd_stage == 1)
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TLBIVAL,
		    (uint64_t)dom->sd_cb_idx << 48 | va >> PAGE_SHIFT);
	else
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TLBIIPAS2L,
		    va >> PAGE_SHIFT);
}

int
smmu_load_map(struct smmu_domain *dom, bus_dmamap_t map)
{
	bus_addr_t addr, end;
	int seg;

	mtx_enter(&dom->sd_mtx); /* XXX necessary ? */
	for (seg = 0; seg < map->dm_nsegs; seg++) {
		addr = trunc_page(map->dm_segs[seg].ds_addr);
		end = round_page(map->dm_segs[seg].ds_addr +
		    map->dm_segs[seg].ds_len);
		while (addr < end) {
			smmu_enter(dom, addr, addr, PROT_READ | PROT_WRITE,
			    PROT_READ | PROT_WRITE, PMAP_CACHE_WB);
			addr += PAGE_SIZE;
		}
	}
	mtx_leave(&dom->sd_mtx);

	return 0;
}

void
smmu_unload_map(struct smmu_domain *dom, bus_dmamap_t map)
{
	bus_addr_t addr, end;
	int curseg;

	mtx_enter(&dom->sd_mtx); /* XXX necessary ? */
	for (curseg = 0; curseg < map->dm_nsegs; curseg++) {
		addr = trunc_page(map->dm_segs[curseg].ds_addr);
		end = round_page(map->dm_segs[curseg].ds_addr +
		    map->dm_segs[curseg].ds_len);
		while (addr < end) {
			smmu_remove(dom, addr);
			addr += PAGE_SIZE;
		}
	}
	mtx_leave(&dom->sd_mtx);

	smmu_tlb_sync_context(dom);
}

int
smmu_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	struct smmu_domain *dom = t->_cookie;

	return dom->sd_parent_dmat->_dmamap_create(dom->sd_parent_dmat, size,
	    nsegments, maxsegsz, boundary, flags, dmamp);
}

void
smmu_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct smmu_domain *dom = t->_cookie;

	dom->sd_parent_dmat->_dmamap_destroy(dom->sd_parent_dmat, map);
}

int
smmu_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct smmu_domain *dom = t->_cookie;
	int error;

	error = dom->sd_parent_dmat->_dmamap_load(dom->sd_parent_dmat, map,
	    buf, buflen, p, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		dom->sd_parent_dmat->_dmamap_unload(dom->sd_parent_dmat, map);

	return error;
}

int
smmu_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m0,
    int flags)
{
	struct smmu_domain *dom = t->_cookie;
	int error;

	error = dom->sd_parent_dmat->_dmamap_load_mbuf(dom->sd_parent_dmat, map,
	    m0, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		dom->sd_parent_dmat->_dmamap_unload(dom->sd_parent_dmat, map);

	return error;
}

int
smmu_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map, struct uio *uio,
    int flags)
{
	struct smmu_domain *dom = t->_cookie;
	int error;

	error = dom->sd_parent_dmat->_dmamap_load_uio(dom->sd_parent_dmat, map,
	    uio, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		dom->sd_parent_dmat->_dmamap_unload(dom->sd_parent_dmat, map);

	return error;
}

int
smmu_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map, bus_dma_segment_t *segs,
    int nsegs, bus_size_t size, int flags)
{
	struct smmu_domain *dom = t->_cookie;
	int error;

	error = dom->sd_parent_dmat->_dmamap_load_raw(dom->sd_parent_dmat, map,
	    segs, nsegs, size, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		dom->sd_parent_dmat->_dmamap_unload(dom->sd_parent_dmat, map);

	return error;
}

void
smmu_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct smmu_domain *dom = t->_cookie;

	smmu_unload_map(dom, map);
	dom->sd_parent_dmat->_dmamap_unload(dom->sd_parent_dmat, map);
}

void
smmu_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t addr,
    bus_size_t size, int op)
{
	struct smmu_domain *dom = t->_cookie;
	dom->sd_parent_dmat->_dmamap_sync(dom->sd_parent_dmat, map,
	    addr, size, op);
}

int
smmu_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	struct smmu_domain *dom = t->_cookie;
	bus_addr_t addr, end;
	int cache, seg, error;

	error = dom->sd_parent_dmat->_dmamem_map(dom->sd_parent_dmat, segs,
	    nsegs, size, kvap, flags);
	if (error)
		return error;

	cache = PMAP_CACHE_WB;
	if (((t->_flags & BUS_DMA_COHERENT) == 0 &&
	   (flags & BUS_DMA_COHERENT)) || (flags & BUS_DMA_NOCACHE))
		cache = PMAP_CACHE_CI;
	mtx_enter(&dom->sd_mtx); /* XXX necessary ? */
	for (seg = 0; seg < nsegs; seg++) {
		addr = trunc_page(segs[seg].ds_addr);
		end = round_page(segs[seg].ds_addr + segs[seg].ds_len);
		while (addr < end) {
			smmu_prepare(dom, addr, addr, PROT_READ | PROT_WRITE,
			    PROT_NONE, cache);
			addr += PAGE_SIZE;
		}
	}
	mtx_leave(&dom->sd_mtx);

	return error;
}
