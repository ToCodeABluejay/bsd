/*	$OpenBSD: cn30xxgmx.c,v 1.41 2019/09/15 07:15:14 visa Exp $	*/

/*
 * Copyright (c) 2007 Internet Initiative Japan, Inc.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/octeon_model.h>
#include <machine/octeonvar.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/cn30xxasxvar.h>
#include <octeon/dev/cn30xxciureg.h>
#include <octeon/dev/cn30xxgmxreg.h>
#include <octeon/dev/cn30xxgmxvar.h>
#include <octeon/dev/cn30xxipdvar.h>
#include <octeon/dev/cn30xxpipvar.h>
#include <octeon/dev/cn30xxsmivar.h>

#define	dprintf(...)
#define	OCTEON_ETH_KASSERT	KASSERT

#define	ADDR2UINT64(u, a) \
	do { \
		u = \
		    (((uint64_t)a[0] << 40) | ((uint64_t)a[1] << 32) | \
		     ((uint64_t)a[2] << 24) | ((uint64_t)a[3] << 16) | \
		     ((uint64_t)a[4] <<  8) | ((uint64_t)a[5] <<  0)); \
	} while (0)
#define	UINT642ADDR(a, u) \
	do { \
		a[0] = (uint8_t)((u) >> 40); a[1] = (uint8_t)((u) >> 32); \
		a[2] = (uint8_t)((u) >> 24); a[3] = (uint8_t)((u) >> 16); \
		a[4] = (uint8_t)((u) >>  8); a[5] = (uint8_t)((u) >>  0); \
	} while (0)

#define	_GMX_RD8(sc, off) \
	bus_space_read_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_gmx->sc_regh, (off))
#define	_GMX_WR8(sc, off, v) \
	bus_space_write_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_gmx->sc_regh, (off), (v))
#define	_GMX_PORT_RD8(sc, off) \
	bus_space_read_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_regh, (off))
#define	_GMX_PORT_WR8(sc, off, v) \
	bus_space_write_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_regh, (off), (v))

#define PCS_READ_8(sc, reg) \
	bus_space_read_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_pcs_regh, \
	    (reg))
#define PCS_WRITE_8(sc, reg, val) \
	bus_space_write_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_pcs_regh, \
	    (reg), (val))

struct cn30xxgmx_port_ops {
	int	(*port_ops_enable)(struct cn30xxgmx_port_softc *, int);
	int	(*port_ops_speed)(struct cn30xxgmx_port_softc *);
	int	(*port_ops_timing)(struct cn30xxgmx_port_softc *);
};

int	cn30xxgmx_match(struct device *, void *, void *);
void	cn30xxgmx_attach(struct device *, struct device *, void *);
int	cn30xxgmx_print(void *, const char *);
int	cn30xxgmx_port_phy_addr(int);
int	cn30xxgmx_init(struct cn30xxgmx_softc *);
int	cn30xxgmx_rx_frm_ctl_xable(struct cn30xxgmx_port_softc *,
	    uint64_t, int);
int	cn30xxgmx_rgmii_enable(struct cn30xxgmx_port_softc *, int);
int	cn30xxgmx_rgmii_speed(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_rgmii_speed_newlink(struct cn30xxgmx_port_softc *,
	    uint64_t *);
int	cn30xxgmx_rgmii_speed_speed(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_rgmii_timing(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_sgmii_enable(struct cn30xxgmx_port_softc *, int);
int	cn30xxgmx_sgmii_speed(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_sgmii_timing(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_tx_ovr_bp_enable(struct cn30xxgmx_port_softc *, int);
int	cn30xxgmx_rx_pause_enable(struct cn30xxgmx_port_softc *, int);

#ifdef OCTEON_ETH_DEBUG
int	cn30xxgmx_rgmii_speed_newlink_log(struct cn30xxgmx_port_softc *,
	    uint64_t);
#endif

static const int	cn30xxgmx_rx_adr_cam_regs[] = {
	GMX0_RX0_ADR_CAM0, GMX0_RX0_ADR_CAM1, GMX0_RX0_ADR_CAM2,
	GMX0_RX0_ADR_CAM3, GMX0_RX0_ADR_CAM4, GMX0_RX0_ADR_CAM5
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_mii = {
	/* XXX not implemented */
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_gmii = {
	.port_ops_enable = cn30xxgmx_rgmii_enable,
	.port_ops_speed = cn30xxgmx_rgmii_speed,
	.port_ops_timing = cn30xxgmx_rgmii_timing,
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_rgmii = {
	.port_ops_enable = cn30xxgmx_rgmii_enable,
	.port_ops_speed = cn30xxgmx_rgmii_speed,
	.port_ops_timing = cn30xxgmx_rgmii_timing,
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_sgmii = {
	.port_ops_enable = cn30xxgmx_sgmii_enable,
	.port_ops_speed = cn30xxgmx_sgmii_speed,
	.port_ops_timing = cn30xxgmx_sgmii_timing,
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_spi42 = {
	/* XXX not implemented */
};

struct cn30xxgmx_port_ops *cn30xxgmx_port_ops[] = {
	[GMX_MII_PORT] = &cn30xxgmx_port_ops_mii,
	[GMX_GMII_PORT] = &cn30xxgmx_port_ops_gmii,
	[GMX_RGMII_PORT] = &cn30xxgmx_port_ops_rgmii,
	[GMX_SGMII_PORT] = &cn30xxgmx_port_ops_sgmii,
	[GMX_SPI42_PORT] = &cn30xxgmx_port_ops_spi42
};

struct cfattach cn30xxgmx_ca = {sizeof(struct cn30xxgmx_softc),
    cn30xxgmx_match, cn30xxgmx_attach, NULL, NULL};

struct cfdriver cn30xxgmx_cd = {NULL, "cn30xxgmx", DV_DULL};

int
cn30xxgmx_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = (struct cfdata *)match;
	struct iobus_attach_args *aa = aux;

	if (strcmp(cf->cf_driver->cd_name, aa->aa_name) != 0)
		return 0;
	return 1;
}

int
cn30xxgmx_get_phy_phandle(int port)
{
	char name[64];
	int node;
	int phandle = 0;

	snprintf(name, sizeof(name),
	    "/soc/pip@11800a0000000/interface@%x/ethernet@%x",
	    port / 16, port % 16);
	node = OF_finddevice(name);
	if (node != - 1)
		phandle = OF_getpropint(node, "phy-handle", 0);
	return phandle;
}

void
cn30xxgmx_attach(struct device *parent, struct device *self, void *aux)
{
	struct cn30xxgmx_attach_args gmx_aa;
	struct iobus_attach_args *aa = aux;
	struct cn30xxgmx_port_softc *port_sc;
	struct cn30xxgmx_softc *sc = (void *)self;
	struct cn30xxsmi_softc *smi;
	int i;
	int phy_addr;
	int port;
	int status;

	printf("\n");

	sc->sc_regt = aa->aa_bust; /* XXX why there are iot? */
	sc->sc_unitno = aa->aa_unitno;

	status = bus_space_map(sc->sc_regt, aa->aa_addr,
	    GMX0_BASE_IF_SIZE(sc->sc_nports), 0, &sc->sc_regh);
	if (status != 0)
		panic(": can't map register");

	cn30xxgmx_init(sc);

	sc->sc_ports = mallocarray(sc->sc_nports, sizeof(*sc->sc_ports),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_ports == NULL) {
		printf("%s: out of memory\n", sc->sc_dev.dv_xname);
		return;
	}

	for (i = 0; i < sc->sc_nports; i++) {
		port = GMX_PORT_NUM(sc->sc_unitno, i);
		if (cn30xxsmi_get_phy(cn30xxgmx_get_phy_phandle(port), port,
		    &smi, &phy_addr))
			continue;

		port_sc = &sc->sc_ports[i];
		port_sc->sc_port_gmx = sc;
		port_sc->sc_port_no = port;
		port_sc->sc_port_type = sc->sc_port_types[i];
		port_sc->sc_port_ops = cn30xxgmx_port_ops[port_sc->sc_port_type];
		status = bus_space_map(sc->sc_regt,
		    aa->aa_addr + GMX0_BASE_PORT_SIZE * i,
		    GMX0_BASE_PORT_SIZE, 0, &port_sc->sc_port_regh);
		if (status != 0)
			panic(": can't map port register");

		switch (port_sc->sc_port_type) {
		case GMX_MII_PORT:
		case GMX_GMII_PORT:
		case GMX_RGMII_PORT: {
			struct cn30xxasx_attach_args asx_aa;

			asx_aa.aa_port = i;
			asx_aa.aa_regt = aa->aa_bust;
			cn30xxasx_init(&asx_aa, &port_sc->sc_port_asx);
			break;
		}
		case GMX_SGMII_PORT:
			if (bus_space_map(sc->sc_regt,
			    PCS_BASE(sc->sc_unitno, i), PCS_SIZE, 0,
			    &port_sc->sc_port_pcs_regh))
				panic("could not map PCS registers");
			break;
		default:
			/* nothing */
			break;
		}

		(void)memset(&gmx_aa, 0, sizeof(gmx_aa));
		gmx_aa.ga_regt = aa->aa_bust;
		gmx_aa.ga_dmat = aa->aa_dmat;
		gmx_aa.ga_addr = aa->aa_addr;
		gmx_aa.ga_name = "cnmac";
		gmx_aa.ga_portno = port_sc->sc_port_no;
		gmx_aa.ga_port_type = sc->sc_port_types[i];
		gmx_aa.ga_gmx = sc;
		gmx_aa.ga_gmx_port = port_sc;
		gmx_aa.ga_phy_addr = phy_addr;
		gmx_aa.ga_smi = smi;

		config_found(self, &gmx_aa, cn30xxgmx_print);
	}
}

int
cn30xxgmx_print(void *aux, const char *pnp)
{
	struct cn30xxgmx_attach_args *ga = aux;
	static const char *types[] = {
		[GMX_MII_PORT] = "MII",
		[GMX_GMII_PORT] = "GMII",
		[GMX_RGMII_PORT] = "RGMII",
		[GMX_SGMII_PORT] = "SGMII"
	};

#if DEBUG
	if (pnp)
		printf("%s at %s", ga->ga_name, pnp);
#endif

	printf(": %s", types[ga->ga_port_type]);

	return UNCONF;
}

int
cn30xxgmx_init(struct cn30xxgmx_softc *sc)
{
	int result = 0;
	uint64_t inf_mode;
	int i, id;

	inf_mode = bus_space_read_8(sc->sc_regt, sc->sc_regh, GMX0_INF_MODE);
	if ((inf_mode & INF_MODE_EN) == 0) {
		printf("ports are disabled\n");
		sc->sc_nports = 0;
		return 1;
	}

	id = octeon_get_chipid();

	switch (octeon_model_family(id)) {
	case OCTEON_MODEL_FAMILY_CN31XX:
		/*
		 * CN31XX-HM-1.01
		 * 14.1 Packet Interface Introduction
		 * Table 14-1 Packet Interface Configuration
		 * 14.8 GMX Registers, Interface Mode Register, GMX0_INF_MODE
		 */
		if ((inf_mode & INF_MODE_TYPE) == 0) {
			/* all three ports configured as RGMII */
			sc->sc_nports = 3;
			sc->sc_port_types[0] = GMX_RGMII_PORT;
			sc->sc_port_types[1] = GMX_RGMII_PORT;
			sc->sc_port_types[2] = GMX_RGMII_PORT;
		} else {
			/* port 0: RGMII, port 1: GMII, port 2: disabled */
			/* XXX CN31XX-HM-1.01 says "Port 3: disabled"; typo? */
			sc->sc_nports = 2;
			sc->sc_port_types[0] = GMX_RGMII_PORT;
			sc->sc_port_types[1] = GMX_GMII_PORT;
		}
		break;
	case OCTEON_MODEL_FAMILY_CN30XX:
	case OCTEON_MODEL_FAMILY_CN50XX:
		/*
		 * CN30XX-HM-1.0
		 * 13.1 Packet Interface Introduction
		 * Table 13-1 Packet Interface Configuration
		 * 13.8 GMX Registers, Interface Mode Register, GMX0_INF_MODE
		 */
		if ((inf_mode & INF_MODE_P0MII) == 0)
			sc->sc_port_types[0] = GMX_RGMII_PORT;
		else
			sc->sc_port_types[0] = GMX_MII_PORT;
		if ((inf_mode & INF_MODE_TYPE) == 0) {
			/* port 1 and 2 are configred as RGMII ports */
			sc->sc_nports = 3;
			sc->sc_port_types[1] = GMX_RGMII_PORT;
			sc->sc_port_types[2] = GMX_RGMII_PORT;
		} else {
			/* port 1: GMII/MII, port 2: disabled */
			/* GMII or MII port is slected by GMX_PRT1_CFG[SPEED] */
			sc->sc_nports = 2;
			sc->sc_port_types[1] = GMX_GMII_PORT;
		}
		/* port 2 is in CN3010/CN5010 only */
		if ((octeon_model(id) != OCTEON_MODEL_CN3010) &&
		    (octeon_model(id) != OCTEON_MODEL_CN5010))
			if (sc->sc_nports == 3)
				sc->sc_nports = 2;
		break;
	case OCTEON_MODEL_FAMILY_CN61XX: {
		uint64_t qlm_cfg;

		if (sc->sc_unitno == 0)
			qlm_cfg = octeon_xkphys_read_8(MIO_QLM_CFG(2));
		else
			qlm_cfg = octeon_xkphys_read_8(MIO_QLM_CFG(0));
		if ((qlm_cfg & MIO_QLM_CFG_CFG) == 2) {
			sc->sc_nports = 4;
			for (i = 0; i < sc->sc_nports; i++)
				sc->sc_port_types[i] = GMX_SGMII_PORT;
		} else if ((qlm_cfg & MIO_QLM_CFG_CFG) == 3) {
			printf("XAUI interface is not supported\n");
			sc->sc_nports = 0;
			result = 1;
		} else {
			/* The interface is disabled. */
			sc->sc_nports = 0;
			result = 1;
		}
		break;
	}
	case OCTEON_MODEL_FAMILY_CN71XX:
		switch (inf_mode & INF_MODE_MODE) {
		case INF_MODE_MODE_SGMII:
			sc->sc_nports = 4;
			for (i = 0; i < sc->sc_nports; i++)
				sc->sc_port_types[i] = GMX_SGMII_PORT;
			break;
#ifdef notyet
		case INF_MODE_MODE_XAUI:
#endif
		default:
			sc->sc_nports = 0;
			result = 1;
		}
		break;
	case OCTEON_MODEL_FAMILY_CN38XX:
	case OCTEON_MODEL_FAMILY_CN56XX:
	case OCTEON_MODEL_FAMILY_CN58XX:
	default:
		printf("unsupported octeon model: 0x%x\n", octeon_get_chipid());
		sc->sc_nports = 0;
		result = 1;
		break;
	}

	return result;
}

/* XXX RGMII specific */
int
cn30xxgmx_link_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t prt_cfg;

	cn30xxgmx_tx_int_enable(sc, enable);
	cn30xxgmx_rx_int_enable(sc, enable);

	prt_cfg = _GMX_PORT_RD8(sc, GMX0_PRT0_CFG);
	if (enable) {
		if (cn30xxgmx_link_status(sc)) {
			SET(prt_cfg, PRTN_CFG_EN);
		}
	} else {
		CLR(prt_cfg, PRTN_CFG_EN);
	}
	_GMX_PORT_WR8(sc, GMX0_PRT0_CFG, prt_cfg);
	/*
	 * According to CN30XX-HM-1.0, 13.4.2 Link Status Changes:
	 * > software should read back to flush the write operation.
	 */
	(void)_GMX_PORT_RD8(sc, GMX0_PRT0_CFG);

	return 0;
}

/* XXX RGMII specific */
int
cn30xxgmx_stats_init(struct cn30xxgmx_port_softc *sc)
{
        _GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS_DRP, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS_BAD, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT0, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT1, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT3, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT9, 0x0ULL);
	return 0;
}

int
cn30xxgmx_tx_stats_rd_clr(struct cn30xxgmx_port_softc *sc, int enable)
{
	_GMX_PORT_WR8(sc, GMX0_TX0_STATS_CTL, enable ? 0x1ULL : 0x0ULL);
	return 0;
}

int
cn30xxgmx_rx_stats_rd_clr(struct cn30xxgmx_port_softc *sc, int enable)
{
	_GMX_PORT_WR8(sc, GMX0_RX0_STATS_CTL, enable ? 0x1ULL : 0x0ULL);
	return 0;
}

void
cn30xxgmx_rx_stats_dec_bad(struct cn30xxgmx_port_softc *sc)
{
	uint64_t tmp;

        tmp = _GMX_PORT_RD8(sc, GMX0_RX0_STATS_PKTS_BAD);
	_GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS_BAD, tmp - 1);
}

int
cn30xxgmx_tx_ovr_bp_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t ovr_bp;
	int index = GMX_PORT_INDEX(sc->sc_port_no);

	ovr_bp = _GMX_RD8(sc, GMX0_TX_OVR_BP);
	if (enable) {
		CLR(ovr_bp, (1 << index) << TX_OVR_BP_EN_SHIFT);
		SET(ovr_bp, (1 << index) << TX_OVR_BP_BP_SHIFT);
		/* XXX really??? */
		SET(ovr_bp, (1 << index) << TX_OVR_BP_IGN_FULL_SHIFT);
	} else {
		SET(ovr_bp, (1 << index) << TX_OVR_BP_EN_SHIFT);
		CLR(ovr_bp, (1 << index) << TX_OVR_BP_BP_SHIFT);
		/* XXX really??? */
		SET(ovr_bp, (1 << index) << TX_OVR_BP_IGN_FULL_SHIFT);
	}
	_GMX_WR8(sc, GMX0_TX_OVR_BP, ovr_bp);
	return 0;
}

int
cn30xxgmx_rx_pause_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	if (enable) {
		cn30xxgmx_rx_frm_ctl_enable(sc, RXN_FRM_CTL_CTL_BCK);
	} else {
		cn30xxgmx_rx_frm_ctl_disable(sc, RXN_FRM_CTL_CTL_BCK);
	}

	return 0;
}

void
cn30xxgmx_tx_int_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t tx_int_xxx = 0;

	SET(tx_int_xxx,
	    TX_INT_REG_LATE_COL |
	    TX_INT_REG_XSDEF |
	    TX_INT_REG_XSCOL |
	    TX_INT_REG_UNDFLW |
	    TX_INT_REG_PKO_NXA);
	_GMX_WR8(sc, GMX0_TX_INT_REG, tx_int_xxx);
	_GMX_WR8(sc, GMX0_TX_INT_EN, enable ? tx_int_xxx : 0);
}

void
cn30xxgmx_rx_int_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t rx_int_xxx = 0;

	SET(rx_int_xxx, 0 |
	    RXN_INT_REG_PHY_DUPX |
	    RXN_INT_REG_PHY_SPD |
	    RXN_INT_REG_PHY_LINK |
	    RXN_INT_REG_IFGERR |
	    RXN_INT_REG_COLDET |
	    RXN_INT_REG_FALERR |
	    RXN_INT_REG_RSVERR |
	    RXN_INT_REG_PCTERR |
	    RXN_INT_REG_OVRERR |
	    RXN_INT_REG_NIBERR |
	    RXN_INT_REG_SKPERR |
	    RXN_INT_REG_RCVERR |
	    RXN_INT_REG_LENERR |
	    RXN_INT_REG_ALNERR |
	    RXN_INT_REG_FCSERR |
	    RXN_INT_REG_JABBER |
	    RXN_INT_REG_MAXERR |
	    RXN_INT_REG_CAREXT |
	    RXN_INT_REG_MINERR);
	_GMX_PORT_WR8(sc, GMX0_RX0_INT_REG, rx_int_xxx);
	_GMX_PORT_WR8(sc, GMX0_RX0_INT_EN, enable ? rx_int_xxx : 0);
}

int
cn30xxgmx_rx_frm_ctl_enable(struct cn30xxgmx_port_softc *sc,
    uint64_t rx_frm_ctl)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	unsigned int maxlen;

	maxlen = roundup(ifp->if_hardmtu + ETHER_HDR_LEN + ETHER_CRC_LEN +
	    ETHER_VLAN_ENCAP_LEN, 8);
	_GMX_PORT_WR8(sc, GMX0_RX0_JABBER, maxlen);

	return cn30xxgmx_rx_frm_ctl_xable(sc, rx_frm_ctl, 1);
}

int
cn30xxgmx_rx_frm_ctl_disable(struct cn30xxgmx_port_softc *sc,
    uint64_t rx_frm_ctl)
{
	return cn30xxgmx_rx_frm_ctl_xable(sc, rx_frm_ctl, 0);
}

int
cn30xxgmx_rx_frm_ctl_xable(struct cn30xxgmx_port_softc *sc,
    uint64_t rx_frm_ctl, int enable)
{
	uint64_t tmp;

	tmp = _GMX_PORT_RD8(sc, GMX0_RX0_FRM_CTL);
	if (enable)
		SET(tmp, rx_frm_ctl);
	else
		CLR(tmp, rx_frm_ctl);
	_GMX_PORT_WR8(sc, GMX0_RX0_FRM_CTL, tmp);

	return 0;
}

int
cn30xxgmx_tx_thresh(struct cn30xxgmx_port_softc *sc, int cnt)
{
	_GMX_PORT_WR8(sc, GMX0_TX0_THRESH, cnt);
	return 0;
}

int
cn30xxgmx_set_mac_addr(struct cn30xxgmx_port_softc *sc, uint8_t *addr)
{
	uint64_t mac;
	int i;

	ADDR2UINT64(mac, addr);

	cn30xxgmx_link_enable(sc, 0);

	sc->sc_mac = mac;
	_GMX_PORT_WR8(sc, GMX0_SMAC0, mac);
	for (i = 0; i < 6; i++)
		_GMX_PORT_WR8(sc, cn30xxgmx_rx_adr_cam_regs[i], addr[i]);

	cn30xxgmx_link_enable(sc, 1);

	return 0;
}

int
cn30xxgmx_set_filter(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	struct arpcom *ac = sc->sc_port_ac;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint64_t cam_en = 0x01ULL;
	uint64_t ctl = 0;
	int multi = 0;

	cn30xxgmx_link_enable(sc, 0);

	SET(ctl, RXN_ADR_CTL_CAM_MODE);
	CLR(ctl, RXN_ADR_CTL_MCST_ACCEPT | RXN_ADR_CTL_MCST_AFCAM |
	    RXN_ADR_CTL_MCST_REJECT);
	CLR(ifp->if_flags, IFF_ALLMULTI);

	/*
	 * Always accept broadcast frames.
	 */
	SET(ctl, RXN_ADR_CTL_BCST);

	if (ISSET(ifp->if_flags, IFF_PROMISC)) {
		SET(ifp->if_flags, IFF_ALLMULTI);
		CLR(ctl, RXN_ADR_CTL_CAM_MODE);
		SET(ctl, RXN_ADR_CTL_MCST_ACCEPT);
		cam_en = 0x00ULL;
	} else if (ac->ac_multirangecnt > 0 || ac->ac_multicnt > 7) {
		SET(ifp->if_flags, IFF_ALLMULTI);
		SET(ctl, RXN_ADR_CTL_MCST_ACCEPT);
	} else {
		/*
		 * Note first entry is self MAC address; other 7 entires are
		 * available for multicast addresses.
		 */
		ETHER_FIRST_MULTI(step, sc->sc_port_ac, enm);
		while (enm != NULL) {
			int i;

			dprintf("%d: %02x:%02x:%02x:%02x:%02x:%02x\n"
			    multi + 1,
			    enm->enm_addrlo[0], enm->enm_addrlo[1],
			    enm->enm_addrlo[2], enm->enm_addrlo[3],
			    enm->enm_addrlo[4], enm->enm_addrlo[5]);
			multi++;

			SET(cam_en, 1ULL << multi); /* XXX */

			for (i = 0; i < 6; i++) {
				uint64_t tmp;

				/* XXX */
				tmp = _GMX_PORT_RD8(sc,
				    cn30xxgmx_rx_adr_cam_regs[i]);
				CLR(tmp, 0xffULL << (8 * multi));
				SET(tmp, (uint64_t)enm->enm_addrlo[i] <<
				    (8 * multi));
				_GMX_PORT_WR8(sc, cn30xxgmx_rx_adr_cam_regs[i],
				    tmp);
			}

			for (i = 0; i < 6; i++)
				dprintf("cam%d = %016llx\n", i,
				    _GMX_PORT_RD8(sc,
				    cn30xxgmx_rx_adr_cam_regs[i]));

			ETHER_NEXT_MULTI(step, enm);
		}

		if (multi)
			SET(ctl, RXN_ADR_CTL_MCST_AFCAM);
		else
			SET(ctl, RXN_ADR_CTL_MCST_REJECT);

		OCTEON_ETH_KASSERT(enm == NULL);
	}

	dprintf("ctl = %llx, cam_en = %llx\n", ctl, cam_en);
	_GMX_PORT_WR8(sc, GMX0_RX0_ADR_CTL, ctl);
	_GMX_PORT_WR8(sc, GMX0_RX0_ADR_CAM_EN, cam_en);

	cn30xxgmx_link_enable(sc, 1);

	return 0;
}

int
cn30xxgmx_port_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	(*sc->sc_port_ops->port_ops_enable)(sc, enable);
	return 0;
}

int
cn30xxgmx_reset_speed(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	if (ISSET(sc->sc_port_mii->mii_flags, MIIF_DOINGAUTO)) {
		log(LOG_WARNING,
		    "%s: autonegotiation has not been completed yet\n",
		    ifp->if_xname);
		return 1;
	}
	(*sc->sc_port_ops->port_ops_speed)(sc);
	return 0;
}

int
cn30xxgmx_reset_timing(struct cn30xxgmx_port_softc *sc)
{
	(*sc->sc_port_ops->port_ops_timing)(sc);
	return 0;
}

int
cn30xxgmx_reset_board(struct cn30xxgmx_port_softc *sc)
{

	return 0;
}

int
cn30xxgmx_reset_flowctl(struct cn30xxgmx_port_softc *sc)
{
	struct ifmedia_entry *ife = sc->sc_port_mii->mii_media.ifm_cur;

	/*
	 * Get flow control negotiation result.
	 */
#ifdef GMX_802_3X_DISABLE_AUTONEG
	/* Tentative support for SEIL-compat.. */
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		sc->sc_port_flowflags &= ~IFM_ETH_FMASK;
	}
#else
	/* Default configuration of NetBSD */
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO &&
	    (sc->sc_port_mii->mii_media_active & IFM_ETH_FMASK) !=
			sc->sc_port_flowflags) {
		sc->sc_port_flowflags =
			sc->sc_port_mii->mii_media_active & IFM_ETH_FMASK;
		sc->sc_port_mii->mii_media_active &= ~IFM_ETH_FMASK;
	}
#endif /* GMX_802_3X_DISABLE_AUTONEG */

	/*
	 * 802.3x Flow Control Capabilities
	 */
	if (sc->sc_port_flowflags & IFM_ETH_TXPAUSE) {
		cn30xxgmx_tx_ovr_bp_enable(sc, 1);
	} else {
		cn30xxgmx_tx_ovr_bp_enable(sc, 0);
	}
	if (sc->sc_port_flowflags & IFM_ETH_RXPAUSE) {
		cn30xxgmx_rx_pause_enable(sc, 1);
	} else {
		cn30xxgmx_rx_pause_enable(sc, 0);
	}

	return 0;
}

int
cn30xxgmx_rgmii_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t mode;

	/* XXX */
	mode = _GMX_RD8(sc, GMX0_INF_MODE);
	if (ISSET(mode, INF_MODE_EN)) {
		cn30xxasx_enable(sc->sc_port_asx, 1);
	}

	return 0;
}

int
cn30xxgmx_rgmii_speed(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	uint64_t newlink;
	int baudrate;

	/* XXX */
	cn30xxgmx_link_enable(sc, 1);

	cn30xxgmx_rgmii_speed_newlink(sc, &newlink);
	if (sc->sc_link == newlink) {
		return 0;
	}
#ifdef OCTEON_ETH_DEBUG
	cn30xxgmx_rgmii_speed_newlink_log(sc, newlink);
#endif
	sc->sc_link = newlink;

	switch (sc->sc_link & RXN_RX_INBND_SPEED) {
	case RXN_RX_INBND_SPEED_2_5:
		baudrate = IF_Mbps(10);
		break;
	case RXN_RX_INBND_SPEED_25:
		baudrate = IF_Mbps(100);
		break;
	case RXN_RX_INBND_SPEED_125:
		baudrate = IF_Gbps(1);
		break;
	default:
		baudrate = 0/* XXX */;
		break;
	}
	ifp->if_baudrate = baudrate;

	cn30xxgmx_link_enable(sc, 0);

	/*
	 * According to CN30XX-HM-1.0, 13.4.2 Link Status Changes:
	 * wait a max_packet_time
	 * max_packet_time(us) = (max_packet_size(bytes) * 8) / link_speed(Mbps)
	 */
	delay((GMX_FRM_MAX_SIZ * 8) / (baudrate / 1000000));

	cn30xxgmx_rgmii_speed_speed(sc);

	cn30xxgmx_link_enable(sc, 1);
	cn30xxasx_enable(sc->sc_port_asx, 1);

	return 0;
}

int
cn30xxgmx_rgmii_speed_newlink(struct cn30xxgmx_port_softc *sc,
    uint64_t *rnewlink)
{
	uint64_t newlink;

	/* Inband status does not seem to work */
	newlink = _GMX_PORT_RD8(sc, GMX0_RX0_RX_INBND);

	*rnewlink = newlink;
	return 0;
}

#ifdef OCTEON_ETH_DEBUG
int
cn30xxgmx_rgmii_speed_newlink_log(struct cn30xxgmx_port_softc *sc,
    uint64_t newlink)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	const char *status_str;
	const char *speed_str;
	const char *duplex_str;
	int is_status_changed;
	int is_speed_changed;
	int is_linked;
	char status_buf[80/* XXX */];
	char speed_buf[80/* XXX */];

	is_status_changed = (newlink & RXN_RX_INBND_STATUS) !=
	    (sc->sc_link & RXN_RX_INBND_STATUS);
	is_speed_changed = (newlink & RXN_RX_INBND_SPEED) !=
	    (sc->sc_link & RXN_RX_INBND_SPEED);
	is_linked = ISSET(newlink, RXN_RX_INBND_STATUS);
	if (is_status_changed) {
		if (is_linked)
			status_str = "link up";
		else
			status_str = "link down";
	} else {
		if (is_linked) {
			/* any other conditions? */
			if (is_speed_changed)
				status_str = "link change";
			else
				status_str = NULL;
		} else {
			status_str = NULL;
		}
	}

	if (status_str != NULL) {
		if ((is_speed_changed && is_linked) || is_linked) {
			switch (newlink & RXN_RX_INBND_SPEED) {
			case RXN_RX_INBND_SPEED_2_5:
				speed_str = "10baseT";
				break;
			case RXN_RX_INBND_SPEED_25:
				speed_str = "100baseTX";
				break;
			case RXN_RX_INBND_SPEED_125:
				speed_str = "1000baseT";
				break;
			default:
				panic("Unknown link speed");
				break;
			}

			if (ISSET(newlink, RXN_RX_INBND_DUPLEX))
				duplex_str = "-FDX";
			else
				duplex_str = "";

			(void)snprintf(speed_buf, sizeof(speed_buf), "(%s%s)",
			    speed_str, duplex_str);
		} else {
			speed_buf[0] = '\0';
		}
		(void)snprintf(status_buf, sizeof(status_buf), "%s: %s%s%s\n",
		    ifp->if_xname, status_str, (is_speed_changed | is_linked) ? " " : "",
		    speed_buf);
		log(LOG_CRIT, status_buf);
	}

	return 0;
}
#endif

int
cn30xxgmx_rgmii_speed_speed(struct cn30xxgmx_port_softc *sc)
{
	uint64_t prt_cfg;
	uint64_t tx_clk, tx_slot, tx_burst;

	prt_cfg = _GMX_PORT_RD8(sc, GMX0_PRT0_CFG);

	switch (sc->sc_link & RXN_RX_INBND_SPEED) {
	case RXN_RX_INBND_SPEED_2_5:
		/* 10Mbps */
		/*
		 * "GMX Tx Clock Generation Registers", CN30XX-HM-1.0;
		 * > 8ns x 50 = 400ns (2.5MHz TXC clock)
		 */
		tx_clk = 50;
		/*
		 * "TX Slottime Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set SLOT to 0x40
		 */
		tx_slot = 0x40;
		/*
		 * "TX Burst-Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set BURST to 0x0
		 */
		tx_burst = 0;
		/*
		 * "GMX Tx Port Configuration Registers", CN30XX-HM-1.0;
		 * > Slot time for half-duplex operation
		 * >   0 = 512 bittimes (10/100Mbps operation)
		 */
		CLR(prt_cfg, PRTN_CFG_SLOTTIME);
		/*
		 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
		 * > Link speed
		 * >   0 = 10/100Mbps operation
		 * >     in RGMII mode: GMX0_TX(0..2)_CLK[CLK_CNT] > 1
		 */
		CLR(prt_cfg, PRTN_CFG_SPEED);
		break;
	case RXN_RX_INBND_SPEED_25:
		/* 100Mbps */
		/*
		 * "GMX Tx Clock Generation Registers", CN30XX-HM-1.0;
		 * > 8ns x 5 = 40ns (25.0MHz TXC clock)
		 */
		tx_clk = 5;
		/*
		 * "TX Slottime Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set SLOT to 0x40
		 */
		tx_slot = 0x40;
		/*
		 * "TX Burst-Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set BURST to 0x0
		 */
		tx_burst = 0;
		/*
		 * "GMX Tx Port Configuration Registers", CN30XX-HM-1.0;
		 * > Slot time for half-duplex operation
		 * >   0 = 512 bittimes (10/100Mbps operation)
		 */
		CLR(prt_cfg, PRTN_CFG_SLOTTIME);
		/*
		 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
		 * > Link speed
		 * >   0 = 10/100Mbps operation
		 * >     in RGMII mode: GMX0_TX(0..2)_CLK[CLK_CNT] > 1
		 */
		CLR(prt_cfg, PRTN_CFG_SPEED);
		break;
	case RXN_RX_INBND_SPEED_125:
		/* 1000Mbps */
		/*
		 * "GMX Tx Clock Generation Registers", CN30XX-HM-1.0;
		 * > 8ns x 1 = 8ns (125.0MHz TXC clock)
		 */
		tx_clk = 1;
		/*
		 * "TX Slottime Counter Registers", CN30XX-HM-1.0;
		 * > 1000Mbps: set SLOT to 0x200
		 */
		tx_slot = 0x200;
		/*
		 * "TX Burst-Counter Registers", CN30XX-HM-1.0;
		 * > 1000Mbps: set BURST to 0x2000
		 */
		tx_burst = 0x2000;
		/*
		 * "GMX Tx Port Configuration Registers", CN30XX-HM-1.0;
		 * > Slot time for half-duplex operation
		 * >   1 = 4096 bittimes (1000Mbps operation)
		 */
		SET(prt_cfg, PRTN_CFG_SLOTTIME);
		/*
		 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
		 * > Link speed
		 * >   1 = 1000Mbps operation
		 */
		SET(prt_cfg, PRTN_CFG_SPEED);
		break;
	default:
		/* NOT REACHED! */
		/* Following configuration is default value of system.
		*/
		tx_clk = 1;
		tx_slot = 0x200;
		tx_burst = 0x2000;
		SET(prt_cfg, PRTN_CFG_SLOTTIME);
		SET(prt_cfg, PRTN_CFG_SPEED);
		break;
	}

	/* Setup Duplex mode(negotiated) */
	/*
	 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
	 * > Duplex mode: 0 = half-duplex mode, 1=full-duplex
	 */
	if (ISSET(sc->sc_link, RXN_RX_INBND_DUPLEX)) {
		/* Full-Duplex */
		SET(prt_cfg, PRTN_CFG_DUPLEX);
	} else {
		/* Half-Duplex */
		CLR(prt_cfg, PRTN_CFG_DUPLEX);
	}

	_GMX_PORT_WR8(sc, GMX0_TX0_CLK, tx_clk);
	_GMX_PORT_WR8(sc, GMX0_TX0_SLOT, tx_slot);
	_GMX_PORT_WR8(sc, GMX0_TX0_BURST, tx_burst);
	_GMX_PORT_WR8(sc, GMX0_PRT0_CFG, prt_cfg);

	return 0;
}

int
cn30xxgmx_rgmii_timing(struct cn30xxgmx_port_softc *sc)
{
	int clk_tx_setting;
	int clk_rx_setting;
	uint64_t rx_frm_ctl;

	/* RGMII TX Threshold Registers, CN30XX-HM-1.0;
	 * > Number of 16-byte ticks to accumulate in the TX FIFO before
	 * > sending on the RGMII interface. This field should be large
	 * > enough to prevent underflow on the RGMII interface and must
	 * > never be set to less than 0x4. This register cannot exceed
	 * > the TX FIFO depth of 0x40 words.
	 */
	/* Default parameter of CN30XX */
	cn30xxgmx_tx_thresh(sc, 32);

	rx_frm_ctl = 0 |
	    /* RXN_FRM_CTL_NULL_DIS |	(cn5xxx only) */
	    /* RXN_FRM_CTL_PRE_ALIGN |	(cn5xxx only) */
	    /* RXN_FRM_CTL_PAD_LEN |	(cn3xxx only) */
	    /* RXN_FRM_CTL_VLAN_LEN |	(cn3xxx only) */
	    RXN_FRM_CTL_PRE_FREE |
	    RXN_FRM_CTL_CTL_SMAC |
	    RXN_FRM_CTL_CTL_MCST |
	    RXN_FRM_CTL_CTL_DRP |
	    RXN_FRM_CTL_PRE_STRP |
	    RXN_FRM_CTL_PRE_CHK;
	cn30xxgmx_rx_frm_ctl_enable(sc, rx_frm_ctl);

	/* XXX PHY-dependent parameter */
	/* RGMII RX Clock-Delay Registers, CN30XX-HM-1.0;
	 * > Delay setting to place n RXC (RGMII receive clock) delay line.
	 * > The intrinsic delay can range from 50ps to 80ps per tap,
	 * > which corresponds to skews of 1.25ns to 2.00ns at 25 taps(CSR+1).
	 * > This is the best match for the RGMII specification which wants
	 * > 1ns - 2.6ns of skew.
	 */
	/* RGMII TX Clock-Delay Registers, CN30XX-HM-1.0;
	 * > Delay setting to place n TXC (RGMII transmit clock) delay line.
	 * > ...
	 */

	switch (octeon_boot_info->board_type) {
	default:
		/* Default parameter of CN30XX */
		clk_tx_setting = 24;
		clk_rx_setting = 24;
		break;
	case BOARD_TYPE_UBIQUITI_E100:
	case BOARD_TYPE_UBIQUITI_E120:
		clk_tx_setting = 16;
		clk_rx_setting = 0;
		break;
	}

	cn30xxasx_clk_set(sc->sc_port_asx, clk_tx_setting, clk_rx_setting);

	return 0;
}

int
cn30xxgmx_sgmii_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t ctl_reg, status, timer_count;
	uint64_t cpu_freq = octeon_boot_info->eclock / 1000000;
	int done;
	int i;

	if (!enable)
		return 0;

	/* Set link timer interval to 1.6ms. */
	timer_count = PCS_READ_8(sc, PCS_LINK_TIMER_COUNT);
	CLR(timer_count, PCS_LINK_TIMER_COUNT_MASK);
	SET(timer_count, ((1600 * cpu_freq) >> 10) & PCS_LINK_TIMER_COUNT_MASK);
	PCS_WRITE_8(sc, PCS_LINK_TIMER_COUNT, timer_count);

	/* Reset the PCS. */
	ctl_reg = PCS_READ_8(sc, PCS_MR_CONTROL);
	SET(ctl_reg, PCS_MR_CONTROL_RESET);
	PCS_WRITE_8(sc, PCS_MR_CONTROL, ctl_reg);

	/* Wait for the reset to complete. */
	done = 0;
	for (i = 0; i < 1000000; i++) {
		ctl_reg = PCS_READ_8(sc, PCS_MR_CONTROL);
		if (!ISSET(ctl_reg, PCS_MR_CONTROL_RESET)) {
			done = 1;
			break;
		}
	}
	if (!done) {
		printf("SGMII reset timeout on port %d\n", sc->sc_port_no);
		return 1;
	}

	/* Start a new SGMII autonegotiation. */
	SET(ctl_reg, PCS_MR_CONTROL_AN_EN);
	SET(ctl_reg, PCS_MR_CONTROL_RST_AN);
	CLR(ctl_reg, PCS_MR_CONTROL_PWR_DN);
	PCS_WRITE_8(sc, PCS_MR_CONTROL, ctl_reg);

	/* Wait for the SGMII autonegotiation to complete. */
	done = 0;
	for (i = 0; i < 1000000; i++) {
		status = PCS_READ_8(sc, PCS_MR_STATUS);
		if (ISSET(status, PCS_MR_STATUS_AN_CPT)) {
			done = 1;
			break;
		}
	}
	if (!done) {
		printf("SGMII autonegotiation timeout on port %d\n",
		    sc->sc_port_no);
		return 1;
	}

	return 0;
}

int
cn30xxgmx_sgmii_speed(struct cn30xxgmx_port_softc *sc)
{
	uint64_t misc_ctl, prt_cfg;
	int tx_burst, tx_slot;

	cn30xxgmx_link_enable(sc, 0);

	prt_cfg = _GMX_PORT_RD8(sc, GMX0_PRT0_CFG);

	if (ISSET(sc->sc_port_mii->mii_media_active, IFM_FDX))
		SET(prt_cfg, PRTN_CFG_DUPLEX);
	else
		CLR(prt_cfg, PRTN_CFG_DUPLEX);

	misc_ctl = PCS_READ_8(sc, PCS_MISC_CTL);
	CLR(misc_ctl, PCS_MISC_CTL_SAMP_PT);

	/* Disable the GMX port if the link is down. */
	if (cn30xxgmx_link_status(sc))
		CLR(misc_ctl, PCS_MISC_CTL_GMXENO);
	else
		SET(misc_ctl, PCS_MISC_CTL_GMXENO);

	switch (sc->sc_port_ac->ac_if.if_baudrate) {
	case IF_Mbps(10):
		tx_slot = 0x40;
		tx_burst = 0;
		CLR(prt_cfg, PRTN_CFG_SPEED);
		SET(prt_cfg, PRTN_CFG_SPEED_MSB);
		CLR(prt_cfg, PRTN_CFG_SLOTTIME);
		misc_ctl |= 25 & PCS_MISC_CTL_SAMP_PT;
		break;
	case IF_Mbps(100):
		tx_slot = 0x40;
		tx_burst = 0;
		CLR(prt_cfg, PRTN_CFG_SPEED);
		CLR(prt_cfg, PRTN_CFG_SPEED_MSB);
		CLR(prt_cfg, PRTN_CFG_SLOTTIME);
		misc_ctl |= 5 & PCS_MISC_CTL_SAMP_PT;
		break;
	case IF_Gbps(1):
	default:
		tx_slot = 0x200;
		tx_burst = 0x2000;
		SET(prt_cfg, PRTN_CFG_SPEED);
		CLR(prt_cfg, PRTN_CFG_SPEED_MSB);
		SET(prt_cfg, PRTN_CFG_SLOTTIME);
		misc_ctl |= 1 & PCS_MISC_CTL_SAMP_PT;
		break;
	}

	PCS_WRITE_8(sc, PCS_MISC_CTL, misc_ctl);

	_GMX_PORT_WR8(sc, GMX0_TX0_SLOT, tx_slot);
	_GMX_PORT_WR8(sc, GMX0_TX0_BURST, tx_burst);
	_GMX_PORT_WR8(sc, GMX0_PRT0_CFG, prt_cfg);

	cn30xxgmx_link_enable(sc, 1);

	return 0;
}

int
cn30xxgmx_sgmii_timing(struct cn30xxgmx_port_softc *sc)
{
	uint64_t rx_frm_ctl;

	cn30xxgmx_tx_thresh(sc, 32);

	rx_frm_ctl =
	    RXN_FRM_CTL_PRE_FREE |
	    RXN_FRM_CTL_CTL_SMAC |
	    RXN_FRM_CTL_CTL_MCST |
	    RXN_FRM_CTL_CTL_DRP |
	    RXN_FRM_CTL_PRE_STRP |
	    RXN_FRM_CTL_PRE_CHK;
	cn30xxgmx_rx_frm_ctl_enable(sc, rx_frm_ctl);

	return 0;
}

void
cn30xxgmx_stats(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	uint64_t tmp;

	ifp->if_ierrors +=
	    (uint32_t)_GMX_PORT_RD8(sc, GMX0_RX0_STATS_PKTS_BAD);
	ifp->if_iqdrops +=
	    (uint32_t)_GMX_PORT_RD8(sc, GMX0_RX0_STATS_PKTS_DRP);
	tmp = _GMX_PORT_RD8(sc, GMX0_TX0_STAT0);
	ifp->if_oerrors +=
	    (uint32_t)tmp + ((uint32_t)(tmp >> 32) * 16);
	ifp->if_collisions += ((uint32_t)tmp) * 16;
	tmp = _GMX_PORT_RD8(sc, GMX0_TX0_STAT1);
	ifp->if_collisions +=
	    ((uint32_t)tmp * 2) + (uint32_t)(tmp >> 32);
	tmp = _GMX_PORT_RD8(sc, GMX0_TX0_STAT9);
	ifp->if_oerrors += (uint32_t)(tmp >> 32);
}
