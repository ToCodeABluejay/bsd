/* $OpenBSD: i915_drv.c,v 1.37 2013/08/07 19:49:05 kettenis Exp $ */
/*
 * Copyright (c) 2008-2009 Owain G. Ainsworth <oga@openbsd.org>
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
/*-
 * Copyright © 2008 Intel Corporation
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm.h>
#include <dev/pci/drm/i915_drm.h>
#include "i915_drv.h"
#include "intel_drv.h"

#include <machine/pmap.h>

#include <sys/queue.h>
#include <sys/workq.h>
#if 0
#	define INTELDRM_WATCH_COHERENCY
#	define WATCH_INACTIVE
#endif

extern struct mutex mchdev_lock;

/*
 * Override lid status (0=autodetect, 1=autodetect disabled [default],
 * -1=force lid closed, -2=force lid open)
 */
int i915_panel_ignore_lid = 1;

/* Enable powersavings, fbc, downclocking, etc. (default: true) */
unsigned int i915_powersave = 1;

/* Use semaphores for inter-ring sync (default: -1 (use per-chip defaults)) */
int i915_semaphores = -1;

/*
 * Enable frame buffer compression for power savings
 * (default: -1 (use per-chip default))
 */
int i915_enable_fbc = -1;

/*
 * Enable power-saving render C-state 6.
 * Different stages can be selected via bitmask values
 * (0 = disable; 1 = enable rc6; 2 = enable deep rc6; 4 = enable deepest rc6).
 * For example, 3 would enable rc6 and deep rc6, and 7 would enable everything.
 * default: -1 (use per-chip default)
 */
int i915_enable_rc6 = -1;

/* Use panel (LVDS/eDP) downclocking for power savings (default: false) */
unsigned int i915_lvds_downclock = 0;

/*
 * Specify LVDS channel mode
 * (0=probe BIOS [default], 1=single-channel, 2=dual-channel)
 */
int i915_lvds_channel_mode = 0;

/*
 * Use Spread Spectrum Clock with panels [LVDS/eDP]
 * (default: auto from VBT)
 */
int i915_panel_use_ssc = -1;

/*
 * Override/Ignore selection of SDVO panel mode in the VBT
 * (-2=ignore, -1=auto [default], index in VBT BIOS table)
 */
int i915_vbt_sdvo_panel_type = -1;

/*
 * Periodically check GPU activity for detecting hangs.
 * WARNING: Disabling this can cause system wide hangs.
 * (default: true)
 */
bool i915_enable_hangcheck = true;

const struct intel_device_info *
	i915_get_device_id(int);
int	inteldrm_probe(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_activate(struct device *, int);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	inteldrm_doioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

int	inteldrm_gmch_match(struct pci_attach_args *);
void	inteldrm_timeout(void *);

/* For reset and suspend */
int	i8xx_do_reset(struct drm_device *);
int	i965_do_reset(struct drm_device *);
int	i965_reset_complete(struct drm_device *);
int	ironlake_do_reset(struct drm_device *);
int	gen6_do_reset(struct drm_device *);

void	i915_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);
void	i965_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);

int	i915_drm_freeze(struct drm_device *);
int	__i915_drm_thaw(struct drm_device *);
int	i915_drm_thaw(struct drm_device *);

static const struct intel_device_info intel_i830_info = {
	.gen = 2, .is_mobile = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_845g_info = {
	.gen = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i85x_info = {
	.gen = 2, .is_i85x = 1, .is_mobile = 1,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i865g_info = {
	.gen = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i915g_info = {
	.gen = 3, .is_i915g = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};
static const struct intel_device_info intel_i915gm_info = {
	.gen = 3, .is_mobile = 1,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
};
static const struct intel_device_info intel_i945g_info = {
	.gen = 3, .has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};
static const struct intel_device_info intel_i945gm_info = {
	.gen = 3, .is_i945gm = 1, .is_mobile = 1,
	.has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
};

static const struct intel_device_info intel_i965g_info = {
	.gen = 4, .is_broadwater = 1,
	.has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_i965gm_info = {
	.gen = 4, .is_crestline = 1,
	.is_mobile = 1, .has_fbc = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.supports_tv = 1,
};

static const struct intel_device_info intel_g33_info = {
	.gen = 3, .is_g33 = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_g45_info = {
	.gen = 4, .is_g4x = 1, .need_gfx_hws = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_gm45_info = {
	.gen = 4, .is_g4x = 1,
	.is_mobile = 1, .need_gfx_hws = 1, .has_fbc = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.supports_tv = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_pineview_info = {
	.gen = 3, .is_g33 = 1, .is_pineview = 1, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_ironlake_d_info = {
	.gen = 5,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_ironlake_m_info = {
	.gen = 5, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_sandybridge_m_info = {
	.gen = 6, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_ivybridge_d_info = {
	.is_ivybridge = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_ivybridge_m_info = {
	.is_ivybridge = 1, .gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,	/* FBC is not enabled on Ivybridge mobile yet */
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_valleyview_m_info = {
	.gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.is_valleyview = 1,
};

static const struct intel_device_info intel_valleyview_d_info = {
	.gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.is_valleyview = 1,
};

static const struct intel_device_info intel_haswell_d_info = {
	.is_haswell = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_haswell_m_info = {
	.is_haswell = 1, .gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

const static struct drm_pcidev inteldrm_pciidlist[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82854_IGD,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_4SERIES_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G41_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82B43_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82B43_IGD_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_IGC_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_M_IGC_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CLARKDALE_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ARRANDALE_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_S_GT,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2_PLUS,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2_PLUS,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_GT2_2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT1_1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT2_1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT3_1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT1_2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT2_2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT3_2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_SDV_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_SDV_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_SDV_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_SDV_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_SDV_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_SDV_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_SDV_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_SDV_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_SDV_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT1_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT2_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT3_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT1_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT2_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT3_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT1_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT2_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT3_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT1_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT2_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT3_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_CRW_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_CRW_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_CRW_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_CRW_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_CRW_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_CRW_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_CRW_GT1,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_CRW_GT2,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_CRW_GT3,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT1_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT2_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT3_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT1_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT2_2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT3_2,	0 },
	{0, 0, 0}
};

static const struct intel_gfx_device_id {
	int vendor;
        int device;
        const struct intel_device_info *info;
} inteldrm_pciidlist_info[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD,
	    &intel_i830_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD,
	    &intel_845g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82854_IGD,
	    &intel_i85x_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD,
	    &intel_i85x_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD,
	    &intel_i865g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1,
	    &intel_i915g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD,
	    &intel_i915g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1,
	    &intel_i915gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1,
	    &intel_i945g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1,
	    &intel_i945gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1,
	    &intel_i945gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1,
	    &intel_i965gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1,
	    &intel_i965gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1,
	    &intel_g33_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1,
	    &intel_g33_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1,
	    &intel_g33_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1,
	    &intel_gm45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_4SERIES_IGD,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G41_IGD_1,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82B43_IGD_1,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82B43_IGD_2,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_IGC_1,
	    &intel_pineview_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_M_IGC_1,
	    &intel_pineview_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CLARKDALE_IGD,
	    &intel_ironlake_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ARRANDALE_IGD,
	    &intel_ironlake_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT1,
	    &intel_sandybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT1,
	    &intel_sandybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_S_GT,
	    &intel_sandybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2,
	    &intel_sandybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2,
	    &intel_sandybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2_PLUS,
	    &intel_sandybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2_PLUS,
	    &intel_sandybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT1,
	    &intel_ivybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT1,
	    &intel_ivybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT1,
	    &intel_ivybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT2,
	    &intel_ivybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT2,
	    &intel_ivybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT2,
	    &intel_ivybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_GT1, /* GT1 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_GT2, /* GT2 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_GT3, /* GT3 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_GT1, /* GT1 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_GT2, /* GT2 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_GT3, /* GT3 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_GT1, /* GT1 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_GT2, /* GT2 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_GT2_2, /* GT2 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT1_1, /* GT1 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT2_1, /* GT2 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT3_1, /* GT3 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT1_2, /* GT1 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT2_2, /* GT2 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_GT3_2, /* GT3 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_SDV_GT1, /* SDV GT1 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_SDV_GT2, /* SDV GT2 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_SDV_GT3, /* SDV GT3 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_SDV_GT1, /* SDV GT1 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_SDV_GT2, /* SDV GT2 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_SDV_GT3, /* SDV GT3 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_SDV_GT1, /* SDV GT1 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_SDV_GT2, /* SDV GT2 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_SDV_GT3, /* SDV GT3 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT1_1, /* SDV GT1 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT2_1, /* SDV GT2 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT3_1, /* SDV GT3 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT1_2, /* SDV GT1 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT2_2, /* SDV GT2 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_SDV_GT3_2, /* SDV GT3 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT1, /* ULT GT1 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT2, /* ULT GT2 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT3, /* ULT GT3 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT1, /* ULT GT1 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT2, /* ULT GT2 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT3, /* ULT GT3 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT1, /* ULT GT1 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT2, /* ULT GT2 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT3, /* ULT GT3 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT1_1, /* ULT GT1 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT2_1, /* ULT GT2 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT3_1, /* ULT GT3 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT1_2, /* ULT GT1 reserved */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT2_2, /* ULT GT2 reserved */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT3_2, /* ULT GT3 reserved */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_CRW_GT1, /* CRW GT1 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_CRW_GT2, /* CRW GT2 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_D_CRW_GT3, /* CRW GT3 desktop */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_CRW_GT1, /* CRW GT1 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_CRW_GT2, /* CRW GT2 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_S_CRW_GT3, /* CRW GT3 server */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_CRW_GT1, /* CRW GT1 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_CRW_GT2, /* CRW GT2 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_CRW_GT3, /* CRW GT3 mobile */
	    &intel_haswell_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT1_1, /* CRW GT1 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT2_1, /* CRW GT2 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT3_1, /* CRW GT3 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT1_2, /* CRW GT1 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT2_2, /* CRW GT2 reserved */
	    &intel_haswell_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_R_CRW_GT3_2, /* CRW GT3 reserved */
	    &intel_haswell_d_info },
	{0, 0, NULL}
};

static struct drm_driver_info inteldrm_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.file_priv_size		= sizeof(struct inteldrm_file),
	.ioctl			= inteldrm_ioctl,
	.open 			= i915_driver_open,
	.close			= i915_driver_close,
	.lastclose		= i915_driver_lastclose,

	.gem_init_object	= i915_gem_init_object,
	.gem_free_object	= i915_gem_free_object,
	.gem_fault		= i915_gem_fault,
	.gem_size		= sizeof(struct drm_i915_gem_object),

	.dumb_create		= i915_gem_dumb_create,
	.dumb_map_offset	= i915_gem_mmap_gtt,
	.dumb_destroy		= i915_gem_dumb_destroy,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_AGP_REQUIRE |
				    DRIVER_MTRR | DRIVER_IRQ | DRIVER_GEM |
				    DRIVER_MODESET,
};

const struct intel_device_info *
i915_get_device_id(int device)
{
	const struct intel_gfx_device_id *did;

	for (did = &inteldrm_pciidlist_info[0]; did->device != 0; did++) {
		if (did->device != device)
			continue;
		return (did->info);
	}
	return (NULL);
}

int
inteldrm_probe(struct device *parent, void *match, void *aux)
{
	return (drm_pciprobe((struct pci_attach_args *)aux,
	    inteldrm_pciidlist));
}

bool
i915_semaphore_is_enabled(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		return 0;

	if (i915_semaphores >= 0)
		return i915_semaphores;

#ifdef CONFIG_INTEL_IOMMU
	/* Enable semaphores on SNB when IO remapping is off */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_gfx_mapped)
		return false;
#endif

	return 1;
}

int
i915_drm_freeze(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	drm_kms_helper_poll_disable(dev);

#if 0
	pci_save_state(dev->pdev);
#endif

	/* If KMS is active, we do the leavevt stuff here */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		int error = i915_gem_idle(dev);
		if (error) {
			printf("GEM idle failed, resume might fail\n");
			return error;
		}

		timeout_del(&dev_priv->rps.delayed_resume_to);

		intel_modeset_disable(dev);

		drm_irq_uninstall(dev);
	}

	i915_save_state(dev);

	intel_opregion_fini(dev);

	/* Modeset on resume, not lid events */
	dev_priv->modeset_on_lid = 0;

	return 0;
}

int
__i915_drm_thaw(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int error = 0;

	i915_restore_state(dev);
	intel_opregion_setup(dev);

	/* KMS EnterVT equivalent */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		intel_init_pch_refclk(dev);

		DRM_LOCK();
		dev_priv->mm.suspended = 0;

		error = i915_gem_init_hw(dev);
		DRM_UNLOCK();

		intel_modeset_init_hw(dev);
		intel_modeset_setup_hw_state(dev, false);
		drm_irq_install(dev);
	}

	intel_opregion_init(dev);

	dev_priv->modeset_on_lid = 0;

	return error;
}

int
i915_drm_thaw(struct drm_device *dev)
{
	int error = 0;

	intel_gt_reset(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		DRM_LOCK();
		i915_gem_restore_gtt_mappings(dev);
		DRM_UNLOCK();
	}

	__i915_drm_thaw(dev);

	return error;
}

/*
 * We're intel IGD, bus 0 function 0 dev 0 should be the GMCH, so it should
 * be Intel
 */
int
inteldrm_gmch_match(struct pci_attach_args *pa)
{
	if (pa->pa_bus == 0 && pa->pa_device == 0 && pa->pa_function == 0 &&
	    PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_HOST)
		return (1);
	return (0);
}


int inteldrm_wsioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t inteldrm_wsmmap(void *, off_t, int);
int inteldrm_alloc_screen(void *, const struct wsscreen_descr *,
    void **, int *, int *, long *);
void inteldrm_free_screen(void *, void *);
int inteldrm_show_screen(void *, void *, int,
    void (*)(void *, int, int), void *);
void inteldrm_doswitch(void *, void *);
int inteldrm_getchar(void *, int, int, struct wsdisplay_charcell *);
void inteldrm_burner(void *, u_int, u_int);

struct wsscreen_descr inteldrm_stdscreen = {
	"std",
	0, 0,
	0,
	0, 0,
	WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *inteldrm_scrlist[] = {
	&inteldrm_stdscreen,
};

struct wsscreen_list inteldrm_screenlist = {
	nitems(inteldrm_scrlist), inteldrm_scrlist
};

struct wsdisplay_accessops inteldrm_accessops = {
	inteldrm_wsioctl,
	inteldrm_wsmmap,
	inteldrm_alloc_screen,
	inteldrm_free_screen,
	inteldrm_show_screen,
	NULL,
	NULL,
	inteldrm_getchar,
	inteldrm_burner
};

extern int (*ws_get_param)(struct wsdisplay_param *);
extern int (*ws_set_param)(struct wsdisplay_param *);

int
inteldrm_wsioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct inteldrm_softc *dev_priv = v;
	struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;
	struct wsdisplay_param *dp = (struct wsdisplay_param *)data;
	extern u32 _intel_panel_get_max_backlight(struct drm_device *);

	switch (cmd) {
	case WSDISPLAYIO_GETPARAM:
		if (ws_get_param && ws_get_param(dp) == 0)
			return 0;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			dp->min = 0;
			dp->max = _intel_panel_get_max_backlight(dev);
			dp->curval = dev_priv->backlight_level;
			return (dp->max > dp->min) ? 0 : -1;
		}
		break;
	case WSDISPLAYIO_SETPARAM:
		if (ws_set_param && ws_set_param(dp) == 0)
			return 0;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			intel_panel_set_backlight(dev, dp->curval);
			return 0;
		}
		break;
	}

	return (-1);
}

paddr_t
inteldrm_wsmmap(void *v, off_t off, int prot)
{
	return (-1);
}

int
inteldrm_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_alloc_screen(ri, cookiep, curxp, curyp, attrp);
}

void
inteldrm_free_screen(void *v, void *cookie)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_free_screen(ri, cookie);
}

int
inteldrm_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	if (cookie == ri->ri_active)
		return (0);

	dev_priv->switchcb = cb;
	dev_priv->switchcbarg = cbarg;
	if (cb) {
		workq_queue_task(NULL, &dev_priv->switchwqt, 0,
		    inteldrm_doswitch, v, cookie);
		return (EAGAIN);
	}

	inteldrm_doswitch(v, cookie);

	return (0);
}

void
inteldrm_doswitch(void *v, void *cookie)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;
	struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;

	rasops_show_screen(ri, cookie, 0, NULL, NULL);
	intel_fb_restore_mode(dev);

	if (dev_priv->switchcb)
		(*dev_priv->switchcb)(dev_priv->switchcbarg, 0, 0);
}

int
inteldrm_getchar(void *v, int row, int col, struct wsdisplay_charcell *cell)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_getchar(ri, row, col, cell);
}

void
inteldrm_burner(void *v, u_int on, u_int flags)
{
	struct inteldrm_softc *dev_priv = v;
	struct drm_fb_helper *helper = &dev_priv->fbdev->helper;
	int dpms_mode;

	if (on)
		dpms_mode = DRM_MODE_DPMS_ON;
	else {
		if (flags & WSDISPLAY_BURN_VBLANK)
			dpms_mode = DRM_MODE_DPMS_OFF;
		else
			dpms_mode = DRM_MODE_DPMS_STANDBY;
	}

	drm_fb_helper_dpms(helper, dpms_mode);
}

/*
 * Accelerated routines.
 */

int inteldrm_copyrows(void *, int, int, int);

int
inteldrm_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct inteldrm_softc *sc = ri->ri_hw;

	if ((dst == 0 && (src + num) == ri->ri_rows) ||
	    (src == 0 && (dst + num) == ri->ri_rows)) {
		struct inteldrm_softc *dev_priv = sc;
		struct drm_fb_helper *helper = &dev_priv->fbdev->helper;
		size_t size = dev_priv->fbdev->ifb.obj->base.size / 2;
		int stride = ri->ri_font->fontheight * ri->ri_stride;
		int i;

		if (dst == 0) {
			int delta = src * stride;
			bzero(ri->ri_bits, delta);

			sc->sc_offset += delta;
			ri->ri_bits += delta;
			ri->ri_origbits += delta;
			if (sc->sc_offset > size) {
				sc->sc_offset -= size;
				ri->ri_bits -= size;
				ri->ri_origbits -= size;
			}
		} else {
			int delta = dst * stride;
			bzero(ri->ri_bits + num * stride, delta);

			sc->sc_offset -= delta;
			ri->ri_bits -= delta;
			ri->ri_origbits -= delta;
			if (sc->sc_offset < 0) {
				sc->sc_offset += size;
				ri->ri_bits += size;
				ri->ri_origbits += size;
			}
		}

		for (i = 0; i < helper->crtc_count; i++) {
			struct drm_mode_set *mode_set =
			    &helper->crtc_info[i].mode_set;
			struct drm_crtc *crtc = mode_set->crtc;
			struct drm_framebuffer *fb = helper->fb;

			if (!crtc->enabled)
				continue;

			mode_set->x = (sc->sc_offset % ri->ri_stride) /
			    (ri->ri_depth / 8);
			mode_set->y = sc->sc_offset / ri->ri_stride;
			if (fb == crtc->fb)
				dev_priv->display.update_plane(crtc, fb,
				    mode_set->x, mode_set->y);
		}

		return 0;
	}

	return sc->sc_copyrows(cookie, src, dst, num);
}

void
inteldrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)self;
	struct vga_pci_softc	*vga_sc = (struct vga_pci_softc *)parent;
	struct pci_attach_args	*pa = aux, bpa;
	struct vga_pci_bar	*bar;
	struct drm_device	*dev;
	const struct drm_pcidev	*id_entry;
	int			 i;
	uint16_t		 pci_device;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), inteldrm_pciidlist);
	pci_device = PCI_PRODUCT(pa->pa_id);
	dev_priv->info = i915_get_device_id(pci_device);
	KASSERT(dev_priv->info->gen != 0);

	dev_priv->pc = pa->pa_pc;
	dev_priv->tag = pa->pa_tag;
	dev_priv->dmat = pa->pa_dmat;
	dev_priv->bst = pa->pa_memt;

	printf("\n");

	/* All intel chipsets need to be treated as agp, so just pass one */
	dev_priv->drmdev = drm_attach_pci(&inteldrm_driver, pa, 1, self);

	dev = (struct drm_device *)dev_priv->drmdev;

	/* we need to use this api for now due to sharing with intagp */
	bar = vga_pci_bar_info(vga_sc, (IS_I9XX(dev) ? 0 : 1));
	if (bar == NULL) {
		printf(": can't get BAR info\n");
		return;
	}

	dev_priv->regs = vga_pci_bar_map(vga_sc, bar->addr, 0, 0);
	if (dev_priv->regs == NULL) {
		printf(": can't map mmio space\n");
		return;
	}

	intel_detect_pch(dev_priv);

	/*
	 * i945G/GM report MSI capability despite not actually supporting it.
	 * so explicitly disable it.
	 */
	if (IS_I945G(dev) || IS_I945GM(dev))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	if (pci_intr_map(pa, &dev_priv->ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intel_irq_init(dev);

	/*
	 * set up interrupt handler, note that we don't switch the interrupt
	 * on until the X server talks to us, kms will change this.
	 */
	dev_priv->irqh = pci_intr_establish(dev_priv->pc, dev_priv->ih, IPL_TTY,
	    inteldrm_driver.irq_handler,
	    dev_priv, dev_priv->dev.dv_xname);
	if (dev_priv->irqh == NULL) {
		printf(": couldn't  establish interrupt\n");
		return;
	}

	dev_priv->workq = workq_create("intelrel", 1, IPL_TTY);
	if (dev_priv->workq == NULL) {
		printf("couldn't create workq\n");
		return;
	}

	/* GEM init */
	timeout_set(&dev_priv->hangcheck_timer, i915_hangcheck_elapsed, dev_priv);
	dev_priv->next_seqno = 1;
	dev_priv->mm.suspended = 1;
	dev_priv->error_completion = 0;

	if (pci_find_device(&bpa, inteldrm_gmch_match) == 0) {
		printf(": can't find GMCH\n");
		return;
	}

	/* Set up the IFP for chipset flushing */
	if (IS_I915G(dev) || IS_I915GM(dev) || IS_I945G(dev) ||
	    IS_I945GM(dev)) {
		i915_alloc_ifp(dev_priv, &bpa);
	} else if (INTEL_INFO(dev)->gen >= 4 || IS_G33(dev)) {
		i965_alloc_ifp(dev_priv, &bpa);
	} else {
		int nsegs;
		/*
		 * I8XX has no flush page mechanism, we fake it by writing until
		 * the cache is empty. allocate a page to scribble on
		 */
		dev_priv->ifp.i8xx.kva = NULL;
		if (bus_dmamem_alloc(pa->pa_dmat, PAGE_SIZE, 0, 0,
		    &dev_priv->ifp.i8xx.seg, 1, &nsegs, BUS_DMA_WAITOK) == 0) {
			if (bus_dmamem_map(pa->pa_dmat, &dev_priv->ifp.i8xx.seg,
			    1, PAGE_SIZE, &dev_priv->ifp.i8xx.kva, 0) != 0) {
				bus_dmamem_free(pa->pa_dmat,
				    &dev_priv->ifp.i8xx.seg, nsegs);
				dev_priv->ifp.i8xx.kva = NULL;
			}
		}
	}

        /* Try to make sure MCHBAR is enabled before poking at it */
        intel_setup_mchbar(dev_priv, &bpa);

	i915_gem_load(dev);

	mtx_init(&dev_priv->irq_lock, IPL_TTY);
	mtx_init(&dev_priv->rps.lock, IPL_TTY);
	mtx_init(&dev_priv->dpio_lock, IPL_TTY);
	mtx_init(&mchdev_lock, IPL_TTY);
	mtx_init(&dev_priv->error_completion_lock, IPL_NONE);

	rw_init(&dev_priv->rps.hw_lock, "rpshw");

	if (IS_IVYBRIDGE(dev) || IS_HASWELL(dev))
		dev_priv->num_pipe = 3;
	else if (IS_MOBILE(dev) || !IS_GEN2(dev))
		dev_priv->num_pipe = 2;
	else
		dev_priv->num_pipe = 1;

	if (drm_vblank_init(dev, dev_priv->num_pipe)) {
		printf(": vblank init failed\n");
		return;
	}

	intel_gt_init(dev);

	intel_opregion_setup(dev);
	intel_setup_bios(dev);
	intel_setup_gmbus(dev_priv);

	/* XXX would be a lot nicer to get agp info before now */
	uvm_page_physload(atop(dev->agp->base), atop(dev->agp->base +
	    dev->agp->info.ai_aperture_size), atop(dev->agp->base),
	    atop(dev->agp->base + dev->agp->info.ai_aperture_size),
	    PHYSLOAD_DEVICE);
	/* array of vm pages that physload introduced. */
	dev_priv->pgs = PHYS_TO_VM_PAGE(dev->agp->base);
	KASSERT(dev_priv->pgs != NULL);
	/*
	 * XXX mark all pages write combining so user mmaps get the right
	 * bits. We really need a proper MI api for doing this, but for now
	 * this allows us to use PAT where available.
	 */
	for (i = 0; i < atop(dev->agp->info.ai_aperture_size); i++)
		atomic_setbits_int(&(dev_priv->pgs[i].pg_flags), PG_PMAP_WC);
	if (agp_init_map(dev_priv->bst, dev->agp->base,
	    dev->agp->info.ai_aperture_size, BUS_SPACE_MAP_LINEAR |
	    BUS_SPACE_MAP_PREFETCHABLE, &dev_priv->agph))
		panic("can't map aperture");

	/* XXX */
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		i915_load_modeset_init(dev);
	intel_opregion_init(dev);

#if 1
{
	extern int wsdisplay_console_initted;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &dev_priv->ro;

	if (ri->ri_bits == NULL)
		return;

	intel_fb_restore_mode(dev);

	ri->ri_flg = RI_CENTER | RI_VCONS;
	rasops_init(ri, 160, 160);

	ri->ri_hw = dev_priv;
	dev_priv->sc_copyrows = ri->ri_copyrows;
	ri->ri_copyrows = inteldrm_copyrows;

	/*
	 * On older hardware the fast scrolling code causes page table
	 * errors.  As a workaround, we set the "avoid framebuffer
	 * reads" flag, which has the side-effect of disabling the
	 * fast scrolling code, but still gives us a half-decent
	 * scrolling speed.
	 */
	if (INTEL_INFO(dev)->gen < 3 || IS_I915G(dev) || IS_I915GM(dev))
		ri->ri_flg |= RI_WRONLY;

	inteldrm_stdscreen.capabilities = ri->ri_caps;
	inteldrm_stdscreen.nrows = ri->ri_rows;
	inteldrm_stdscreen.ncols = ri->ri_cols;
	inteldrm_stdscreen.textops = &ri->ri_ops;
	inteldrm_stdscreen.fontwidth = ri->ri_font->fontwidth;
	inteldrm_stdscreen.fontheight = ri->ri_font->fontheight;

	aa.console = 0;
	aa.scrdata = &inteldrm_screenlist;
	aa.accessops = &inteldrm_accessops;
	aa.accesscookie = dev_priv;
	aa.defaultscreens = 0;

	if (wsdisplay_console_initted) {
		long defattr;

		ri->ri_ops.alloc_attr(ri->ri_active, 0, 0, 0, &defattr);
		wsdisplay_cnattach(&inteldrm_stdscreen, ri->ri_active,
		    0, 0, defattr);
		aa.console = 1;
	}

	printf("%s: %dx%d\n", dev_priv->dev.dv_xname, ri->ri_width, ri->ri_height);

	vga_sc->sc_type = -1;
	config_found(parent, &aa, wsemuldisplaydevprint);
}
#endif
}

int
inteldrm_detach(struct device *self, int flags)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)self;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	/* this will quiesce any dma that's going on and kill the timeouts. */
	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	if (IS_I9XX(dev) && dev_priv->ifp.i9xx.bsh != 0) {
		bus_space_unmap(dev_priv->ifp.i9xx.bst, dev_priv->ifp.i9xx.bsh,
		    PAGE_SIZE);
	} else if ((IS_I830(dev) || IS_845G(dev) || IS_I85X(dev) ||
	    IS_I865G(dev)) && dev_priv->ifp.i8xx.kva != NULL) {
		bus_dmamem_unmap(dev_priv->dmat, dev_priv->ifp.i8xx.kva,
		     PAGE_SIZE);
		bus_dmamem_free(dev_priv->dmat, &dev_priv->ifp.i8xx.seg, 1);
	}

	pci_intr_disestablish(dev_priv->pc, dev_priv->irqh);

	if (dev_priv->regs != NULL)
		vga_pci_bar_unmap(dev_priv->regs);

	return (0);
}

int
inteldrm_activate(struct device *arg, int act)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)arg;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	switch (act) {
	case DVACT_QUIESCE:
		i915_drm_freeze(dev);
		break;
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		i915_drm_thaw(dev);
		intel_fb_restore_mode(dev);
		break;
	}

	return (0);
}

struct cfattach inteldrm_ca = {
	sizeof(struct inteldrm_softc), inteldrm_probe, inteldrm_attach,
	inteldrm_detach, inteldrm_activate
};

struct cfdriver inteldrm_cd = {
	0, "inteldrm", DV_DULL
};

int
inteldrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int			 error = 0;

	dev_priv->entries++;

	error = -inteldrm_doioctl(dev, cmd, data, file_priv);

	dev_priv->entries--;
	return (error);
}

int
inteldrm_doioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_GETPARAM:
			return (i915_getparam(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_EXECBUFFER2:
			return (i915_gem_execbuffer2(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_BUSY:
			return (i915_gem_busy_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_THROTTLE:
			return (i915_gem_ring_throttle(dev, file_priv));
		case DRM_IOCTL_I915_GEM_MMAP:
			return (i915_gem_mmap_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_MMAP_GTT:
			return (i915_gem_mmap_gtt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_CREATE:
			return (i915_gem_create_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PREAD:
			return (i915_gem_pread_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PWRITE:
			return (i915_gem_pwrite_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_SET_DOMAIN:
			return (i915_gem_set_domain_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GEM_SET_TILING:
			return (i915_gem_set_tiling(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_GET_TILING:
			return (i915_gem_get_tiling(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_GET_APERTURE:
			return (i915_gem_get_aperture_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID:
			return (intel_get_pipe_from_crtc_id(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_MADVISE:
			return (i915_gem_madvise_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_SW_FINISH:
			return (i915_gem_sw_finish_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_SET_CACHING:
			return (i915_gem_set_caching_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GEM_GET_CACHING:
			return (i915_gem_get_caching_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
			return (i915_gem_context_create_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
			return (i915_gem_context_destroy_ioctl(dev, data,
			    file_priv));
		default:
			break;
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_SETPARAM:
			return (i915_setparam(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_INIT:
			return (i915_gem_init_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_ENTERVT:
			return (i915_gem_entervt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_LEAVEVT:
			return (i915_gem_leavevt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PIN:
			return (i915_gem_pin_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_UNPIN:
			return (i915_gem_unpin_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_OVERLAY_PUT_IMAGE:
			return (intel_overlay_put_image(dev, data, file_priv));
		case DRM_IOCTL_I915_OVERLAY_ATTRS:
			return (intel_overlay_attrs(dev, data, file_priv));
		case DRM_IOCTL_I915_GET_SPRITE_COLORKEY:
			return (intel_sprite_get_colorkey(dev, data, file_priv));
		case DRM_IOCTL_I915_SET_SPRITE_COLORKEY:
			return (intel_sprite_set_colorkey(dev, data, file_priv));
		}
	}
	return -EINVAL;
}

void
i915_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	reg;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	reg = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR);
	if (reg & 0x1) {
		addr = (bus_addr_t)reg;
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL ||
	    extent_alloc_subregion(bpa->pa_memex, 0x100000, 0xffffffff,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) ||
	    bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
	    &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR, addr | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf(": no ifp ");
}

void
i965_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	lo, hi;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	hi = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4);
	lo = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR);
	if (lo & 0x1) {
		addr = (((u_int64_t)hi << 32) | lo);
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL ||
	    extent_alloc_subregion(bpa->pa_memex, 0x100000, 0xffffffff,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) ||
	    bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
	    &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4,
	    upper_32_bits(addr));
	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR,
	    (addr & 0xffffffff) | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf(": no ifp ");
}

void
i915_gem_chipset_flush(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	/*
	 * Write to this flush page flushes the chipset write cache.
	 * The write will return when it is done.
	 */
	if (IS_I9XX(dev)) {
	    if (dev_priv->ifp.i9xx.bsh != 0)
		bus_space_write_4(dev_priv->ifp.i9xx.bst,
		    dev_priv->ifp.i9xx.bsh, 0, 1);
	} else {
		int i;

		wbinvd();

#define I830_HIC        0x70

		I915_WRITE(I830_HIC, (I915_READ(I830_HIC) | (1<<31)));
		for (i = 1000; i; i--) {
			if (!(I915_READ(I830_HIC) & (1<<31)))
				break;
			delay(100);
		}

	}
}

/**
 * Pin an object to the GTT and evaluate the relocations landing in it.
 */
int
i915_gem_object_pin_and_relocate(struct drm_obj *obj,
    struct drm_file *file_priv, struct drm_i915_gem_exec_object2 *entry,
    struct drm_i915_gem_relocation_entry *relocs)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_obj		*target_obj;
	struct drm_i915_gem_object *obj_priv = to_intel_bo(obj);
	bus_space_handle_t	 bsh;
	int			 i, ret, needs_fence;

	DRM_ASSERT_HELD(obj);
	needs_fence = ((entry->flags & EXEC_OBJECT_NEEDS_FENCE) &&
	    obj_priv->tiling_mode != I915_TILING_NONE);
	if (needs_fence)
		atomic_setbits_int(&obj->do_flags, I915_EXEC_NEEDS_FENCE);

	/* Choose the GTT offset for our buffer and put it there. */
	ret = i915_gem_object_pin(obj_priv, (u_int32_t)entry->alignment,
	    needs_fence, false);
	if (ret)
		return ret;

	if (needs_fence) {
		ret = i915_gem_object_get_fence(obj_priv);
		if (ret)
			return ret;

		if (i915_gem_object_pin_fence(obj_priv))
			obj->do_flags |= __EXEC_OBJECT_HAS_FENCE;

		obj_priv->pending_fenced_gpu_access = true;
	}

	entry->offset = obj_priv->gtt_offset;

	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_i915_gem_relocation_entry *reloc = &relocs[i];
		struct drm_i915_gem_object *target_obj_priv;
		uint32_t reloc_val, reloc_offset;

		target_obj = drm_gem_object_lookup(obj->dev, file_priv,
		    reloc->target_handle);
		/* object must have come before us in the list */
		if (target_obj == NULL) {
			i915_gem_object_unpin(obj_priv);
			return -ENOENT;
		}
		if ((target_obj->do_flags & I915_IN_EXEC) == 0) {
			printf("%s: object not already in execbuffer\n",
			__func__);
			ret = -EBADF;
			goto err;
		}

		target_obj_priv = to_intel_bo(target_obj);

		/* The target buffer should have appeared before us in the
		 * exec_object list, so it should have a GTT space bound by now.
		 */
		if (target_obj_priv->dmamap == 0) {
			DRM_ERROR("No GTT space found for object %d\n",
				  reloc->target_handle);
			ret = -EINVAL;
			goto err;
		}

		/* must be in one write domain and one only */
		if (reloc->write_domain & (reloc->write_domain - 1)) {
			ret = -EINVAL;
			goto err;
		}
		if (reloc->read_domains & I915_GEM_DOMAIN_CPU ||
		    reloc->write_domain & I915_GEM_DOMAIN_CPU) {
			DRM_ERROR("relocation with read/write CPU domains: "
			    "obj %p target %d offset %d "
			    "read %08x write %08x", obj,
			    reloc->target_handle, (int)reloc->offset,
			    reloc->read_domains, reloc->write_domain);
			ret = -EINVAL;
			goto err;
		}

		if (reloc->write_domain && target_obj->pending_write_domain &&
		    reloc->write_domain != target_obj->pending_write_domain) {
			DRM_ERROR("Write domain conflict: "
				  "obj %p target %d offset %d "
				  "new %08x old %08x\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset,
				  reloc->write_domain,
				  target_obj->pending_write_domain);
			ret = -EINVAL;
			goto err;
		}

		target_obj->pending_read_domains |= reloc->read_domains;
		target_obj->pending_write_domain |= reloc->write_domain;


		if (reloc->offset > obj->size - 4) {
			DRM_ERROR("Relocation beyond object bounds: "
				  "obj %p target %d offset %d size %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset, (int) obj->size);
			ret = -EINVAL;
			goto err;
		}
		if (reloc->offset & 3) {
			DRM_ERROR("Relocation not 4-byte aligned: "
				  "obj %p target %d offset %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset);
			ret = -EINVAL;
			goto err;
		}

		/* Map the page containing the relocation we're going to
		 * perform.
		 */
		reloc_offset = obj_priv->gtt_offset + reloc->offset;
		reloc_val = target_obj_priv->gtt_offset + reloc->delta;

		if (target_obj_priv->gtt_offset == reloc->presumed_offset) {
			drm_gem_object_unreference(target_obj);
			 continue;
		}

		ret = i915_gem_object_set_to_gtt_domain(obj_priv, true);
		if (ret)
			goto err;

		if ((ret = agp_map_subregion(dev_priv->agph,
		    trunc_page(reloc_offset), PAGE_SIZE, &bsh)) != 0) {
			DRM_ERROR("map failed...\n");
			goto err;
		}

		bus_space_write_4(dev_priv->bst, bsh, reloc_offset & PAGE_MASK,
		     reloc_val);

		reloc->presumed_offset = target_obj_priv->gtt_offset;

		agp_unmap_subregion(dev_priv->agph, bsh, PAGE_SIZE);
		drm_gem_object_unreference(target_obj);
	}

	return 0;

err:
	/* we always jump to here mid-loop */
	drm_gem_object_unreference(target_obj);
	i915_gem_object_unpin(obj_priv);
	return ret;
}

int
i915_gem_get_relocs_from_user(struct drm_i915_gem_exec_object2 *exec_list,
    u_int32_t buffer_count, struct drm_i915_gem_relocation_entry **relocs)
{
	u_int32_t	reloc_count = 0, reloc_index = 0, i;
	int		ret, relocs_max;

	relocs_max = INT_MAX / sizeof(struct drm_i915_gem_relocation_entry);

	*relocs = NULL;
	for (i = 0; i < buffer_count; i++) {
		/* First check for malicious input causing overflow in
		 * the worst case where we need to allocate the entire
		 * relocation tree as a single array.
		 */
		if (exec_list[i].relocation_count > relocs_max - reloc_count)
			return (EINVAL);
		reloc_count += exec_list[i].relocation_count;
	}

	if (reloc_count == 0)
		return (0);

	if (SIZE_MAX / reloc_count < sizeof(**relocs))
		return (EINVAL);
	*relocs = drm_alloc(reloc_count * sizeof(**relocs));
	for (i = 0; i < buffer_count; i++) {
		if ((ret = copyin((void *)(uintptr_t)exec_list[i].relocs_ptr,
		    &(*relocs)[reloc_index], exec_list[i].relocation_count *
		    sizeof(**relocs))) != 0) {
			drm_free(*relocs);
			*relocs = NULL;
			return (ret);
		}
		reloc_index += exec_list[i].relocation_count;
	}

	return (0);
}

int
i915_gem_put_relocs_to_user(struct drm_i915_gem_exec_object2 *exec_list,
    u_int32_t buffer_count, struct drm_i915_gem_relocation_entry *relocs)
{
	u_int32_t	reloc_count = 0, i;
	int		ret = 0;

	if (relocs == NULL)
		return (0);

	for (i = 0; i < buffer_count; i++) {
		if ((ret = copyout(&relocs[reloc_count],
		    (void *)(uintptr_t)exec_list[i].relocs_ptr,
		    exec_list[i].relocation_count * sizeof(*relocs))) != 0)
			break;
		reloc_count += exec_list[i].relocation_count;
	}

	drm_free(relocs);

	return (ret);
}

void
inteldrm_timeout(void *arg)
{
	struct inteldrm_softc *dev_priv = arg;
	int err;

	err = workq_add_task(dev_priv->workq, 0, i915_gem_retire_work_handler,
	    dev_priv, NULL);
	if (err)
		DRM_ERROR("failed to run retire handler\n");
}

int
i8xx_do_reset(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (IS_I85X(dev))
		return -ENODEV;

	I915_WRITE(D_STATE, I915_READ(D_STATE) | DSTATE_GFX_RESET_I830);
	POSTING_READ(D_STATE);

	if (IS_I830(dev) || IS_845G(dev)) {
		I915_WRITE(DEBUG_RESET_I830,
			   DEBUG_RESET_DISPLAY |
			   DEBUG_RESET_RENDER |
			   DEBUG_RESET_FULL);
		POSTING_READ(DEBUG_RESET_I830);
		drm_msleep(1, "8res1");

		I915_WRITE(DEBUG_RESET_I830, 0);
		POSTING_READ(DEBUG_RESET_I830);
	}

	drm_msleep(1, "8res2");

	I915_WRITE(D_STATE, I915_READ(D_STATE) & ~DSTATE_GFX_RESET_I830);
	POSTING_READ(D_STATE);

	return 0;
}

int
i965_reset_complete(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u8 gdrst;
	gdrst = (pci_conf_read(dev_priv->pc, dev_priv->tag, I965_GDRST) >> 24);
	return (gdrst & GRDOM_RESET_ENABLE) == 0;
}

int
i965_do_reset(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	pcireg_t reg;
	int retries;

	/*
	 * Set the domains we want to reset (GRDOM/bits 2 and 3) as
	 * well as the reset bit (GR/bit 0).  Setting the GR bit
	 * triggers the reset; when done, the hardware will clear it.
	 */
	reg = pci_conf_read(dev_priv->pc, dev_priv->tag, I965_GDRST);
	reg |= ((GRDOM_RENDER | GRDOM_RESET_ENABLE) << 24);
	pci_conf_write(dev_priv->pc, dev_priv->tag, I965_GDRST, reg);

	for (retries = 500; retries > 0 ; retries--) {
		if (i965_reset_complete(dev))
			break;
		DELAY(1000);
	}
	if (retries == 0) {
		DRM_ERROR("965 reset timed out\n");
		return -ETIMEDOUT;
	}

	/* We can't reset render&media without also resetting display ... */
	reg = pci_conf_read(dev_priv->pc, dev_priv->tag, I965_GDRST);
	reg |= ((GRDOM_MEDIA | GRDOM_RESET_ENABLE) << 24);
	pci_conf_write(dev_priv->pc, dev_priv->tag, I965_GDRST, reg);

	for (retries = 500; retries > 0 ; retries--) {
		if (i965_reset_complete(dev))
			break;
		DELAY(1000);
	}
	if (retries == 0) {
		DRM_ERROR("965 reset 2 timed out\n");
		return -ETIMEDOUT;
	}

	return (0);
}

int
ironlake_do_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 gdrst;
	int retries;

	gdrst = I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR);
	I915_WRITE(MCHBAR_MIRROR_BASE + ILK_GDSR,
		   gdrst | GRDOM_RENDER | GRDOM_RESET_ENABLE);
	for (retries = 500; retries > 0 ; retries--) {
		if (I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR) & 0x1)
			break;
		DELAY(1000);
	}
	if (retries == 0) {
		DRM_ERROR("ironlake reset timed out\n");
		return -ETIMEDOUT;		
	}

	/* We can't reset render&media without also resetting display ... */
	gdrst = I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR);
	I915_WRITE(MCHBAR_MIRROR_BASE + ILK_GDSR,
		   gdrst | GRDOM_MEDIA | GRDOM_RESET_ENABLE);
	for (retries = 500; retries > 0 ; retries--) {
		if (I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR) & 0x1)
			break;
		DELAY(1000);
	}
	if (retries == 0) {
		DRM_ERROR("ironlake reset timed out\n");
		return -ETIMEDOUT;		
	}
	
	return (0);
}

int
gen6_do_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;
	int retries;

	/* Hold gt_lock across reset to prevent any register access
	 * with forcewake not set correctly
	 */
	mtx_enter(&dev_priv->gt_lock);

	/* Reset the chip */

	/* GEN6_GDRST is not in the gt power well, no need to check
	 * for fifo space for the write or forcewake the chip for
	 * the read
	 */
	I915_WRITE_NOTRACE(GEN6_GDRST, GEN6_GRDOM_FULL);

	/* Spin waiting for the device to ack the reset request */
	for (retries = 500; retries > 0 ; retries--) {
		if ((I915_READ_NOTRACE(GEN6_GDRST) & GEN6_GRDOM_FULL) == 0)
			break;
		DELAY(1000);
	}
	if (retries == 0) {
		DRM_ERROR("gen6 reset timed out\n");
		ret = -ETIMEDOUT;
	}

	/* If reset with a user forcewake, try to restore, otherwise turn it off */
	if (dev_priv->forcewake_count)
		dev_priv->gt.force_wake_get(dev_priv);
	else
		dev_priv->gt.force_wake_put(dev_priv);

	/* Restore fifo count */
	dev_priv->gt_fifo_count = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);

	mtx_leave(&dev_priv->gt_lock);
	return ret;
}

int
intel_gpu_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = -ENODEV;

	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6:
		ret = gen6_do_reset(dev);
		break;
	case 5:
		ret = ironlake_do_reset(dev);
		break;
	case 4:
		ret = i965_do_reset(dev);
		break;
	case 2:
		ret = i8xx_do_reset(dev);
		break;
	}

	/* Also reset the gpu hangman. */
	if (dev_priv->stop_rings) {
		DRM_DEBUG("Simulated gpu hang, resetting stop_rings\n");
		dev_priv->stop_rings = 0;
		if (ret == -ENODEV) {
			DRM_ERROR("Reset not implemented, but ignoring "
				  "error for simulated gpu hangs\n");
			ret = 0;
		}
	}

	return ret;
}

/**
 * i915_reset - reset chip after a hang
 * @dev: drm device to reset
 *
 * Reset the chip.  Useful if a hang is detected. Returns zero on successful
 * reset or otherwise an error code.
 *
 * Procedure is fairly simple:
 *   - reset the chip using the reset reg
 *   - re-init context state
 *   - re-init hardware status page
 *   - re-init ring buffer
 *   - re-init interrupt state
 *   - re-init display
 */
int
i915_reset(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

#ifdef notyet
	if (!i915_try_reset)
		return 0;
#endif

	DRM_LOCK();

	i915_gem_reset(dev);

	ret = -ENODEV;
	if (time_second - dev_priv->last_gpu_reset < 5)
		DRM_ERROR("GPU hanging too fast, declaring wedged!\n");
	else
		ret = intel_gpu_reset(dev);

	dev_priv->last_gpu_reset = time_second;
	if (ret) {
		DRM_ERROR("Failed to reset chip.\n");
		DRM_UNLOCK();
		return ret;
	}

	/* Ok, now get things going again... */

	/*
	 * Everything depends on having the GTT running, so we need to start
	 * there.  Fortunately we don't need to do this unless we reset the
	 * chip at a PCI level.
	 *
	 * Next we need to restore the context, but we don't use those
	 * yet either...
	 *
	 * Ring buffer needs to be re-initialized in the KMS case, or if X
	 * was running at the time of the reset (i.e. we weren't VT
	 * switched away).
	 */
	if (drm_core_check_feature(dev, DRIVER_MODESET) ||
			!dev_priv->mm.suspended) {
		struct intel_ring_buffer *ring;
		int i;

		dev_priv->mm.suspended = 0;

		i915_gem_init_swizzling(dev);

		for_each_ring(ring, dev_priv, i)
			ring->init(ring);

		i915_gem_context_init(dev);
#ifdef notyet
		i915_gem_init_ppgtt(dev);
#endif

		/*
		 * It would make sense to re-init all the other hw state, at
		 * least the rps/rc6/emon init done within modeset_init_hw. For
		 * some unknown reason, this blows up my ilk, so don't.
		 */

		DRM_UNLOCK();

		drm_irq_uninstall(dev);
		drm_irq_install(dev);
	} else {
		DRM_UNLOCK();
	}

	return 0;
}

/*
 * Debug code from here.
 */
#ifdef WATCH_INACTIVE
void
inteldrm_verify_inactive(struct inteldrm_softc *dev_priv, char *file,
    int line)
{
	struct drm_obj		*obj;
	struct drm_i915_gem_object *obj_priv;

	TAILQ_FOREACH(obj_priv, &dev_priv->mm.inactive_list, list) {
		obj = &obj_priv->base;
		if (obj_priv->pin_count || obj_priv->active ||
		    obj->write_domain & I915_GEM_GPU_DOMAINS)
			DRM_ERROR("inactive %p (p $d a $d w $x) %s:%d\n",
			    obj, obj_priv->pin_count, obj_priv->active,
			    obj->write_domain, file, line);
	}
}
#endif /* WATCH_INACTIVE */

#if (INTELDRM_DEBUG > 1)

int i915_gem_object_list_info(int, uint);

static const char *get_pin_flag(struct drm_i915_gem_object *obj)
{
	if (obj->user_pin_count > 0)
		return "P";
	if (obj->pin_count > 0)
		return "p";
	else
		return " ";
}

static const char *get_tiling_flag(struct drm_i915_gem_object *obj)
{
    switch (obj->tiling_mode) {
    default:
    case I915_TILING_NONE: return " ";
    case I915_TILING_X: return "X";
    case I915_TILING_Y: return "Y";
    }
}

static const char *
cache_level_str(int type)
{
	switch (type) {
	case I915_CACHE_NONE: return " uncached";
	case I915_CACHE_LLC: return " snooped (LLC)";
	case I915_CACHE_LLC_MLC: return " snooped (LLC+MLC)";
	default: return "";
	}
}

static void
describe_obj(struct drm_i915_gem_object *obj)
{
	printf("%p: %s%s %8zdKiB %04x %04x %d %d %d%s%s%s",
		   &obj->base,
		   get_pin_flag(obj),
		   get_tiling_flag(obj),
		   obj->base.size / 1024,
		   obj->base.read_domains,
		   obj->base.write_domain,
		   obj->last_read_seqno,
		   obj->last_write_seqno,
		   obj->last_fenced_seqno,
		   cache_level_str(obj->cache_level),
		   obj->dirty ? " dirty" : "",
		   obj->madv == I915_MADV_DONTNEED ? " purgeable" : "");
	if (obj->base.name)
		printf(" (name: %d)", obj->base.name);
	if (obj->pin_count)
		printf(" (pinned x %d)", obj->pin_count);
	if (obj->fence_reg != I915_FENCE_REG_NONE)
		printf(" (fence: %d)", obj->fence_reg);
#if 0
	if (obj->gtt_space != NULL)
		printf(" (gtt offset: %08x, size: %08x)",
			   obj->gtt_offset, (unsigned int)obj->gtt_space->size);
	if (obj->pin_mappable || obj->fault_mappable) {
		char s[3], *t = s;
		if (obj->pin_mappable)
			*t++ = 'p';
		if (obj->fault_mappable)
			*t++ = 'f';
		*t = '\0';
		printf(" (%s mappable)", s);
	}
#endif
	if (obj->ring != NULL)
		printf(" (%s)", obj->ring->name);
}

#define ACTIVE_LIST 0
#define INACTIVE_LIST 1

int
i915_gem_object_list_info(int kdev, uint list)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct list_head *head;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	size_t total_obj_size, total_gtt_size;
	int count;
#if 0
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;
#endif

	switch (list) {
	case ACTIVE_LIST:
		printf("Active:\n");
		head = &dev_priv->mm.active_list;
		break;
	case INACTIVE_LIST:
		printf("Inactive:\n");
		head = &dev_priv->mm.inactive_list;
		break;
	default:
//		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	total_obj_size = total_gtt_size = count = 0;
	list_for_each_entry(obj, head, mm_list) {
		printf("   ");
		describe_obj(obj);
		printf("\n");
		total_obj_size += obj->base.size;
		count++;
	}
//	mutex_unlock(&dev->struct_mutex);

	printf("Total %d objects, %zu bytes, %zu GTT size\n",
		   count, total_obj_size);
	return 0;
}


static void i915_ring_seqno_info(struct intel_ring_buffer *ring)
{
	if (ring->get_seqno) {
		printf("Current sequence (%s): %d\n",
		       ring->name, ring->get_seqno(ring, false));
	}
}

void
i915_gem_seqno_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int			 i;

	for_each_ring(ring, dev_priv, i)
		i915_ring_seqno_info(ring);
}

void
i915_interrupt_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int			 i, pipe;

	if (IS_VALLEYVIEW(dev)) {
		printf("Display IER:\t%08x\n",
			   I915_READ(VLV_IER));
		printf("Display IIR:\t%08x\n",
			   I915_READ(VLV_IIR));
		printf("Display IIR_RW:\t%08x\n",
			   I915_READ(VLV_IIR_RW));
		printf("Display IMR:\t%08x\n",
			   I915_READ(VLV_IMR));
		for_each_pipe(pipe)
			printf("Pipe %c stat:\t%08x\n",
				   pipe_name(pipe),
				   I915_READ(PIPESTAT(pipe)));

		printf("Master IER:\t%08x\n",
			   I915_READ(VLV_MASTER_IER));

		printf("Render IER:\t%08x\n",
			   I915_READ(GTIER));
		printf("Render IIR:\t%08x\n",
			   I915_READ(GTIIR));
		printf("Render IMR:\t%08x\n",
			   I915_READ(GTIMR));

		printf("PM IER:\t\t%08x\n",
			   I915_READ(GEN6_PMIER));
		printf("PM IIR:\t\t%08x\n",
			   I915_READ(GEN6_PMIIR));
		printf("PM IMR:\t\t%08x\n",
			   I915_READ(GEN6_PMIMR));

		printf("Port hotplug:\t%08x\n",
			   I915_READ(PORT_HOTPLUG_EN));
		printf("DPFLIPSTAT:\t%08x\n",
			   I915_READ(VLV_DPFLIPSTAT));
		printf("DPINVGTT:\t%08x\n",
			   I915_READ(DPINVGTT));

	} else if (!HAS_PCH_SPLIT(dev)) {
		printf("Interrupt enable:    %08x\n",
			   I915_READ(IER));
		printf("Interrupt identity:  %08x\n",
			   I915_READ(IIR));
		printf("Interrupt mask:      %08x\n",
			   I915_READ(IMR));
		for_each_pipe(pipe)
			printf("Pipe %c stat:         %08x\n",
				   pipe_name(pipe),
				   I915_READ(PIPESTAT(pipe)));
	} else {
		printf("North Display Interrupt enable:		%08x\n",
			   I915_READ(DEIER));
		printf("North Display Interrupt identity:	%08x\n",
			   I915_READ(DEIIR));
		printf("North Display Interrupt mask:		%08x\n",
			   I915_READ(DEIMR));
		printf("South Display Interrupt enable:		%08x\n",
			   I915_READ(SDEIER));
		printf("South Display Interrupt identity:	%08x\n",
			   I915_READ(SDEIIR));
		printf("South Display Interrupt mask:		%08x\n",
			   I915_READ(SDEIMR));
		printf("Graphics Interrupt enable:		%08x\n",
			   I915_READ(GTIER));
		printf("Graphics Interrupt identity:		%08x\n",
			   I915_READ(GTIIR));
		printf("Graphics Interrupt mask:		%08x\n",
			   I915_READ(GTIMR));
	}
#if 0
	printf("Interrupts received: %d\n",
		   atomic_read(&dev_priv->irq_received));
#endif
	for_each_ring(ring, dev_priv, i) {
		if (IS_GEN6(dev) || IS_GEN7(dev)) {
			printf(
				   "Graphics Interrupt mask (%s):	%08x\n",
				   ring->name, I915_READ_IMR(ring));
		}
		i915_ring_seqno_info(ring);
	}
}

void
i915_gem_fence_regs_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int i;

	printf("Reserved fences = %d\n", dev_priv->fence_reg_start);
	printf("Total fences = %d\n", dev_priv->num_fence_regs);
	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_i915_gem_object *obj = dev_priv->fence_regs[i].obj;

		printf("Fence %d, pin count = %d, object = ",
			   i, dev_priv->fence_regs[i].pin_count);
		if (obj == NULL)
			printf("unused");
		else
			describe_obj(obj);
		printf("\n");
	}
}

void
i915_hws_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->rings[RCS];
	int i;
	volatile u32 *hws;

	hws = (volatile u32 *)ring->status_page.page_addr;
	if (hws == NULL)
		return;

	for (i = 0; i < 4096 / sizeof(u32) / 4; i += 4) {
		printf("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   i * 4,
			   hws[i], hws[i + 1], hws[i + 2], hws[i + 3]);
	}
}

static void
i915_dump_pages(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t size)
{
	bus_addr_t	offset = 0;
	int		i = 0;

	/*
	 * this is a bit odd so i don't have to play with the intel
	 * tools too much.
	 */
	for (offset = 0; offset < size; offset += 4, i += 4) {
		if (i == PAGE_SIZE)
			i = 0;
		printf("%08x :  %08x\n", i, bus_space_read_4(bst, bsh,
		    offset));
	}
}

void
i915_batchbuffer_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_obj		*obj;
	struct drm_i915_gem_object *obj_priv;
	bus_space_handle_t	 bsh;
	int			 ret;

	list_for_each_entry(obj_priv, &dev_priv->mm.active_list, mm_list) {
		obj = &obj_priv->base;
		if (obj->read_domains & I915_GEM_DOMAIN_COMMAND) {
			if ((ret = agp_map_subregion(dev_priv->agph,
			    obj_priv->gtt_offset, obj->size, &bsh)) != 0) {
				DRM_ERROR("Failed to map pages: %d\n", ret);
				return;
			}
			printf("--- gtt_offset = 0x%08x\n",
			    obj_priv->gtt_offset);
			i915_dump_pages(dev_priv->bst, bsh, obj->size);
			agp_unmap_subregion(dev_priv->agph,
			    dev_priv->rings[RCS].bsh, obj->size);
		}
	}
}

void
i915_ringbuffer_data(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	bus_size_t		 off;

	if (!dev_priv->rings[RCS].obj) {
		printf("No ringbuffer setup\n");
		return;
	}

	for (off = 0; off < dev_priv->rings[RCS].size; off += 4)
		printf("%08x :  %08x\n", off, bus_space_read_4(dev_priv->bst,
		    dev_priv->rings[RCS].bsh, off));
}

void
i915_ringbuffer_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	u_int32_t		 head, tail;

	head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	tail = I915_READ(PRB0_TAIL) & TAIL_ADDR;

	printf("RingHead :  %08x\n", head);
	printf("RingTail :  %08x\n", tail);
	printf("RingMask :  %08x\n", dev_priv->rings[RCS].size - 1);
	printf("RingSize :  %08lx\n", dev_priv->rings[RCS].size);
	printf("Acthd :  %08x\n", I915_READ(INTEL_INFO(dev)->gen >= 4 ?
	    ACTHD_I965 : ACTHD));
}

#endif

static int
intel_pch_match(struct pci_attach_args *pa)
{
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_ISA)
		return (1);
	return (0);
}

void
intel_detect_pch(struct inteldrm_softc *dev_priv)
{
	struct pci_attach_args	pa;
	unsigned short id;
	if (pci_find_device(&pa, intel_pch_match) == 0) {
		DRM_DEBUG_KMS("No Intel PCI-ISA bridge found\n");
	}
	id = PCI_PRODUCT(pa.pa_id) & INTEL_PCH_DEVICE_ID_MASK;
	dev_priv->pch_id = id;

	switch (id) {
	case INTEL_PCH_IBX_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_IBX;
		dev_priv->num_pch_pll = 2;
		DRM_DEBUG_KMS("Found Ibex Peak PCH\n");
		break;
	case INTEL_PCH_CPT_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_CPT;
		dev_priv->num_pch_pll = 2;
		DRM_DEBUG_KMS("Found CougarPoint PCH\n");
		break;
	case INTEL_PCH_PPT_DEVICE_ID_TYPE:
		/* PantherPoint is CPT compatible */
		dev_priv->pch_type = PCH_CPT;
		dev_priv->num_pch_pll = 2;
		DRM_DEBUG_KMS("Found PatherPoint PCH\n");
		break;
	case INTEL_PCH_LPT_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_LPT;
		dev_priv->num_pch_pll = 0;
		DRM_DEBUG_KMS("Found LynxPoint PCH\n");
		break;
	case INTEL_PCH_LPT_LP_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_LPT;
		dev_priv->num_pch_pll = 0;
		DRM_DEBUG_KMS("Found LynxPoint LP PCH\n");
		break;
	default:
		DRM_DEBUG_KMS("No PCH detected\n");
	}
}

