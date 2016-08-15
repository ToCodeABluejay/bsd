/* $OpenBSD: sximmc.c,v 1.1 2016/08/15 21:03:27 kettenis Exp $ */
/* $NetBSD: awin_mmc.c,v 1.23 2015/11/14 10:32:40 bouyer Exp $ */

/*-
 * Copyright (c) 2014 Jared D. McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/sdmmc/sdmmcvar.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmc_ioreg.h>

#include <armv7/sunxi/sxiccmuvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define SXIMMC_GCTRL			0x0000
#define SXIMMC_CLKCR			0x0004
#define SXIMMC_TIMEOUT			0x0008
#define SXIMMC_WIDTH			0x000C
#define SXIMMC_BLKSZ			0x0010
#define SXIMMC_BYTECNT			0x0014
#define SXIMMC_CMD			0x0018
#define SXIMMC_ARG			0x001C
#define SXIMMC_RESP0			0x0020
#define SXIMMC_RESP1			0x0024
#define SXIMMC_RESP2			0x0028
#define SXIMMC_RESP3			0x002C
#define SXIMMC_IMASK			0x0030
#define SXIMMC_MINT			0x0034
#define SXIMMC_RINT			0x0038
#define SXIMMC_STATUS			0x003C
#define SXIMMC_FTRGLEVEL		0x0040
#define SXIMMC_FUNCSEL			0x0044
#define SXIMMC_CBCR			0x0048
#define SXIMMC_BBCR			0x004C
#define SXIMMC_DBGC			0x0050
#define SXIMMC_A12A			0x0058		/* A80 */
#define SXIMMC_HWRST			0x0078		/* A80 */
#define SXIMMC_DMAC			0x0080
#define SXIMMC_DLBA			0x0084
#define SXIMMC_IDST			0x0088
#define SXIMMC_IDIE			0x008C
#define SXIMMC_CHDA			0x0090
#define SXIMMC_CBDA			0x0094
#define SXIMMC_FIFO			0x0100

#define SXIMMC_GCTRL_ACCESS_BY_AHB	(1U << 31)
#define SXIMMC_GCTRL_WAIT_MEM_ACCESS_DONE (1U << 30)
#define SXIMMC_GCTRL_DDR_MODE		(1U << 10)
#define SXIMMC_GCTRL_DEBOUNCEEN		(1U << 8)
#define SXIMMC_GCTRL_DMAEN		(1U << 5)
#define SXIMMC_GCTRL_INTEN		(1U << 4)
#define SXIMMC_GCTRL_DMARESET		(1U << 2)
#define SXIMMC_GCTRL_FIFORESET		(1U << 1)
#define SXIMMC_GCTRL_SOFTRESET		(1U << 0)
#define SXIMMC_GCTRL_RESET \
	(SXIMMC_GCTRL_SOFTRESET | SXIMMC_GCTRL_FIFORESET | \
	 SXIMMC_GCTRL_DMARESET)

#define SXIMMC_CLKCR_LOWPOWERON		(1U << 17)
#define SXIMMC_CLKCR_CARDCLKON		(1U << 16)
#define SXIMMC_CLKCR_DIV		0x0000ffff

#define SXIMMC_WIDTH_1			0
#define SXIMMC_WIDTH_4			1
#define SXIMMC_WIDTH_8			2

#define SXIMMC_CMD_START		(1U << 31)
#define SXIMMC_CMD_USE_HOLD_REG		(1U << 29)
#define SXIMMC_CMD_VOL_SWITCH		(1U << 28)
#define SXIMMC_CMD_BOOT_ABORT		(1U << 27)
#define SXIMMC_CMD_BOOT_ACK_EXP		(1U << 26)
#define SXIMMC_CMD_ALT_BOOT_OPT		(1U << 25)
#define SXIMMC_CMD_ENBOOT		(1U << 24)
#define SXIMMC_CMD_CCS_EXP		(1U << 23)
#define SXIMMC_CMD_RD_CEATA_DEV		(1U << 22)
#define SXIMMC_CMD_UPCLK_ONLY		(1U << 21)
#define SXIMMC_CMD_SEND_INIT_SEQ	(1U << 15)
#define SXIMMC_CMD_STOP_ABORT_CMD	(1U << 14)
#define SXIMMC_CMD_WAIT_PRE_OVER	(1U << 13)
#define SXIMMC_CMD_SEND_AUTO_STOP	(1U << 12)
#define SXIMMC_CMD_SEQMOD		(1U << 11)
#define SXIMMC_CMD_WRITE		(1U << 10)
#define SXIMMC_CMD_DATA_EXP		(1U << 9)
#define SXIMMC_CMD_CHECK_RSP_CRC	(1U << 8)
#define SXIMMC_CMD_LONG_RSP		(1U << 7)
#define SXIMMC_CMD_RSP_EXP		(1U << 6)

#define SXIMMC_INT_CARD_REMOVE		(1U << 31)
#define SXIMMC_INT_CARD_INSERT		(1U << 30)
#define SXIMMC_INT_SDIO_INT		(1U << 16)
#define SXIMMC_INT_END_BIT_ERR		(1U << 15)
#define SXIMMC_INT_AUTO_CMD_DONE	(1U << 14)
#define SXIMMC_INT_START_BIT_ERR	(1U << 13)
#define SXIMMC_INT_HW_LOCKED		(1U << 12)
#define SXIMMC_INT_FIFO_RUN_ERR		(1U << 11)
#define SXIMMC_INT_VOL_CHG_DONE		(1U << 10)
#define SXIMMC_INT_DATA_STARVE		(1U << 10)
#define SXIMMC_INT_BOOT_START		(1U << 9)
#define SXIMMC_INT_DATA_TIMEOUT		(1U << 9)
#define SXIMMC_INT_ACK_RCV		(1U << 8)
#define SXIMMC_INT_RESP_TIMEOUT		(1U << 8)
#define SXIMMC_INT_DATA_CRC_ERR		(1U << 7)
#define SXIMMC_INT_RESP_CRC_ERR		(1U << 6)
#define SXIMMC_INT_RX_DATA_REQ		(1U << 5)
#define SXIMMC_INT_TX_DATA_REQ		(1U << 4)
#define SXIMMC_INT_DATA_OVER		(1U << 3)
#define SXIMMC_INT_CMD_DONE		(1U << 2)
#define SXIMMC_INT_RESP_ERR		(1U << 1)
#define SXIMMC_INT_ERROR \
	(SXIMMC_INT_RESP_ERR | SXIMMC_INT_RESP_CRC_ERR | \
	 SXIMMC_INT_DATA_CRC_ERR | SXIMMC_INT_RESP_TIMEOUT | \
	 SXIMMC_INT_FIFO_RUN_ERR | SXIMMC_INT_HW_LOCKED | \
	 SXIMMC_INT_START_BIT_ERR  | SXIMMC_INT_END_BIT_ERR)

#define SXIMMC_STATUS_DMAREQ		(1U << 31)
#define SXIMMC_STATUS_DATA_FSM_BUSY	(1U << 10)
#define SXIMMC_STATUS_CARD_DATA_BUSY	(1U << 9)
#define SXIMMC_STATUS_CARD_PRESENT	(1U << 8)
#define SXIMMC_STATUS_FIFO_FULL		(1U << 3)
#define SXIMMC_STATUS_FIFO_EMPTY	(1U << 2)
#define SXIMMC_STATUS_TXWL_FLAG		(1U << 1)
#define SXIMMC_STATUS_RXWL_FLAG		(1U << 0)

#define SXIMMC_FUNCSEL_CEATA_DEV_INTEN (1U << 10)
#define SXIMMC_FUNCSEL_SEND_AUTO_STOP_CCSD (1U << 9)
#define SXIMMC_FUNCSEL_SEND_CCSD	(1U << 8)
#define SXIMMC_FUNCSEL_ABT_RD_DATA	(1U << 2)
#define SXIMMC_FUNCSEL_SDIO_RD_WAIT	(1U << 1)
#define SXIMMC_FUNCSEL_SEND_IRQ_RSP	(1U << 0)

#define SXIMMC_DMAC_REFETCH_DES		(1U << 31)
#define SXIMMC_DMAC_IDMA_ON		(1U << 7)
#define SXIMMC_DMAC_FIX_BURST		(1U << 1)
#define SXIMMC_DMAC_SOFTRESET		(1U << 0)

#define SXIMMC_IDST_HOST_ABT		(1U << 10)
#define SXIMMC_IDST_ABNORMAL_INT_SUM	(1U << 9)
#define SXIMMC_IDST_NORMAL_INT_SUM	(1U << 8)
#define SXIMMC_IDST_CARD_ERR_SUM	(1U << 5)
#define SXIMMC_IDST_DES_INVALID		(1U << 4)
#define SXIMMC_IDST_FATAL_BUS_ERR	(1U << 2)
#define SXIMMC_IDST_RECEIVE_INT		(1U << 1)
#define SXIMMC_IDST_TRANSMIT_INT	(1U << 0)
#define SXIMMC_IDST_ERROR \
	(SXIMMC_IDST_ABNORMAL_INT_SUM | SXIMMC_IDST_CARD_ERR_SUM | \
	 SXIMMC_IDST_DES_INVALID | SXIMMC_IDST_FATAL_BUS_ERR)
#define SXIMMC_IDST_COMPLETE \
	(SXIMMC_IDST_RECEIVE_INT | SXIMMC_IDST_TRANSMIT_INT)

struct sximmc_idma_descriptor {
	uint32_t	dma_config;
#define SXIMMC_IDMA_CONFIG_DIC	(1U << 1)
#define SXIMMC_IDMA_CONFIG_LD	(1U << 2)
#define SXIMMC_IDMA_CONFIG_FD	(1U << 3)
#define SXIMMC_IDMA_CONFIG_CH	(1U << 4)
#define SXIMMC_IDMA_CONFIG_ER	(1U << 5)
#define SXIMMC_IDMA_CONFIG_CES	(1U << 30)
#define SXIMMC_IDMA_CONFIG_OWN	(1U << 31)
	uint32_t	dma_buf_size;
	uint32_t	dma_buf_addr;
	uint32_t	dma_next;
} __packed;

#define SXIMMC_NDESC		32
 
#define SXIMMC_DMA_FTRGLEVEL_A20	0x20070008
#define SXIMMC_DMA_FTRGLEVEL_A80	0x200f0010

int	sximmc_match(struct device *, void *, void *);
void	sximmc_attach(struct device *, struct device *, void *);

int	sximmc_intr(void *);

int	sximmc_host_reset(sdmmc_chipset_handle_t);
uint32_t sximmc_host_ocr(sdmmc_chipset_handle_t);
int	sximmc_host_maxblklen(sdmmc_chipset_handle_t);
int	sximmc_card_detect(sdmmc_chipset_handle_t);
int	sximmc_write_protect(sdmmc_chipset_handle_t);
int	sximmc_bus_power(sdmmc_chipset_handle_t, uint32_t);
int	sximmc_bus_clock(sdmmc_chipset_handle_t, int, int);
int	sximmc_bus_width(sdmmc_chipset_handle_t, int);
void	sximmc_exec_command(sdmmc_chipset_handle_t, struct sdmmc_command *);

struct sdmmc_chip_functions sximmc_chip_functions = {
	.host_reset = sximmc_host_reset,
	.host_ocr = sximmc_host_ocr,
	.host_maxblklen = sximmc_host_maxblklen,
	.card_detect = sximmc_card_detect,
	.bus_power = sximmc_bus_power,
	.bus_clock = sximmc_bus_clock,
	.bus_width = sximmc_bus_width,
	.exec_command = sximmc_exec_command,
};

struct sximmc_softc {
	struct device sc_dev;
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	bus_space_handle_t sc_clk_bsh;
	bus_dma_tag_t sc_dmat;
	int sc_node;

	int sc_use_dma;

	void *sc_ih;

	struct device *sc_sdmmc_dev;

	uint32_t sc_fifo_reg;
	uint32_t sc_dma_ftrglevel;

	uint32_t sc_idma_xferlen;
	bus_dma_segment_t sc_idma_segs[1];
	int sc_idma_nsegs;
	bus_size_t sc_idma_size;
	bus_dmamap_t sc_idma_map;
	int sc_idma_ndesc;
	char *sc_idma_desc;

	uint32_t sc_intr_rint;
	uint32_t sc_intr_mint;
	uint32_t sc_idma_idst;

	uint32_t sc_gpio[4];
};

struct cfdriver sximmc_cd = {
	NULL, "sximmc", DV_DULL
};

struct cfattach sximmc_ca = {
	sizeof(struct sximmc_softc), sximmc_match, sximmc_attach
};

#define MMC_WRITE(sc, reg, val)	\
	bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))
#define MMC_READ(sc, reg) \
	bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))

int	sximmc_set_clock(struct sximmc_softc *sc, u_int);

int
sximmc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	if (OF_is_compatible(faa->fa_node, "allwinner,sun4i-a10-mmc"))
		return 1;
	if (OF_is_compatible(faa->fa_node, "allwinner,sun5i-a13-mmc"))
		return 1;
	if (OF_is_compatible(faa->fa_node, "allwinner,sun9i-a80-mmc"))
		return 1;

	return 0;
}

int
sximmc_idma_setup(struct sximmc_softc *sc)
{
	int error;

	if (OF_is_compatible(sc->sc_node, "allwinner,sun4i-a10-mmc")) {
		sc->sc_idma_xferlen = 0x2000;
	} else {
		sc->sc_idma_xferlen = 0x10000;
	}

	sc->sc_idma_ndesc = SXIMMC_NDESC;
	sc->sc_idma_size = sizeof(struct sximmc_idma_descriptor) *
	    sc->sc_idma_ndesc;
	error = bus_dmamem_alloc(sc->sc_dmat, sc->sc_idma_size, 0,
	    sc->sc_idma_size, sc->sc_idma_segs, 1,
	    &sc->sc_idma_nsegs, BUS_DMA_WAITOK);
	if (error)
		return error;
	error = bus_dmamem_map(sc->sc_dmat, sc->sc_idma_segs,
	    sc->sc_idma_nsegs, sc->sc_idma_size,
	    &sc->sc_idma_desc, BUS_DMA_WAITOK);
	if (error)
		goto free;
	error = bus_dmamap_create(sc->sc_dmat, sc->sc_idma_size, 1,
	    sc->sc_idma_size, 0, BUS_DMA_WAITOK, &sc->sc_idma_map);
	if (error)
		goto unmap;
	error = bus_dmamap_load(sc->sc_dmat, sc->sc_idma_map,
	    sc->sc_idma_desc, sc->sc_idma_size, NULL, BUS_DMA_WAITOK);
	if (error)
		goto destroy;
	return 0;

destroy:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_idma_map);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_idma_desc, sc->sc_idma_size);
free:
	bus_dmamem_free(sc->sc_dmat, sc->sc_idma_segs, sc->sc_idma_nsegs);
	return error;
}

void
sximmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sximmc_softc *sc = (struct sximmc_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct sdmmcbus_attach_args saa;
	int width;

	if (faa->fa_nreg < 1)
		return;

	sc->sc_node = faa->fa_node;
	sc->sc_bst = faa->fa_iot;
	sc->sc_dmat = faa->fa_dmat;

	if (bus_space_map(sc->sc_bst, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_bsh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_use_dma = 1;

	printf("\n");

	pinctrl_byname(faa->fa_node, "default");

	/* enable clock */
	sxiccmu_enablemodule(CCMU_SDMMC0);
	delay(5000);

#if 0
	if (awin_chip_id() == AWIN_CHIP_ID_A80) {
		bus_space_subregion(sc->sc_bst, aio->aio_ccm_bsh,
		    AWIN_A80_CCU_SCLK_SDMMC0_CLK_REG + (loc->loc_port * 4), 4,
		    &sc->sc_clk_bsh);
		awin_reg_set_clear(aio->aio_core_bst, aio->aio_ccm_bsh,
		    AWIN_A80_CCU_SCLK_BUS_CLK_GATING0_REG,
		    AWIN_A80_CCU_SCLK_BUS_CLK_GATING0_SD, 0);
		awin_reg_set_clear(aio->aio_core_bst, aio->aio_ccm_bsh,
		    AWIN_A80_CCU_SCLK_BUS_SOFT_RST0_REG,
		    AWIN_A80_CCU_SCLK_BUS_SOFT_RST0_SD, 0);
		awin_reg_set_clear(aio->aio_core_bst, aio->aio_core_bsh,
		    AWIN_A80_SDMMC_COMM_OFFSET + (loc->loc_port * 4),
		    AWIN_A80_SDMMC_COMM_SDC_RESET_SW |
		    AWIN_A80_SDMMC_COMM_SDC_CLOCK_SW, 0);
		delay(1000);
	} else {
		bus_space_subregion(sc->sc_bst, aio->aio_ccm_bsh,
		    AWIN_SD0_CLK_REG + (loc->loc_port * 4), 4, &sc->sc_clk_bsh);

		awin_pll6_enable();

		awin_reg_set_clear(aio->aio_core_bst, aio->aio_ccm_bsh,
		    AWIN_AHB_GATING0_REG,
		    AWIN_AHB_GATING0_SDMMC0 << loc->loc_port, 0);
		if (awin_chip_id() == AWIN_CHIP_ID_A31) {
			awin_reg_set_clear(aio->aio_core_bst, aio->aio_ccm_bsh,
			    AWIN_A31_AHB_RESET0_REG,
			    AWIN_A31_AHB_RESET0_SD0_RST << loc->loc_port, 0);
		}
	}
#endif

#if 0
	switch (awin_chip_id()) {
	case AWIN_CHIP_ID_A80:
		sc->sc_fifo_reg = AWIN_A31_MMC_FIFO;
		sc->sc_dma_ftrglevel = SXIMMC_DMA_FTRGLEVEL_A80;
		break;
	case AWIN_CHIP_ID_A31:
		sc->sc_fifo_reg = AWIN_A31_MMC_FIFO;
		sc->sc_dma_ftrglevel = SXIMMC_DMA_FTRGLEVEL_A20;
		break;
	default:
		sc->sc_fifo_reg = SXIMMC_FIFO;
		sc->sc_dma_ftrglevel = SXIMMC_DMA_FTRGLEVEL_A20;
		break;
	}
#else
	sc->sc_fifo_reg = SXIMMC_FIFO;
	sc->sc_dma_ftrglevel = SXIMMC_DMA_FTRGLEVEL_A20;
#endif

	if (sc->sc_use_dma) {
		if (sximmc_idma_setup(sc) != 0) {
			printf("%s: failed to setup DMA\n", self->dv_xname);
			return;
		}
	}

	OF_getpropintarray(sc->sc_node, "cd-gpios", sc->sc_gpio,
	    sizeof(sc->sc_gpio));
	gpio_controller_config_pin(sc->sc_gpio, GPIO_CONFIG_INPUT);

	sc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_BIO,
	    sximmc_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't to establish interrupt\n");
		return;
	}

	sximmc_host_reset(sc);
	sximmc_bus_width(sc, 1);
	sximmc_set_clock(sc, 400);

	memset(&saa, 0, sizeof(saa));
	saa.saa_busname = "sdmmc";
	saa.sct = &sximmc_chip_functions;
	saa.sch = sc;
#if 0
	saa.saa_clkmin = 400;
	saa.saa_clkmax = awin_chip_id() == AWIN_CHIP_ID_A80 ? 48000 : 50000;
#endif

	saa.caps = SMC_CAPS_SD_HIGHSPEED | SMC_CAPS_MMC_HIGHSPEED;

	width = OF_getpropint(sc->sc_node, "bus-width", 1);
	if (width >= 8)
		saa.caps |= SMC_CAPS_8BIT_MODE;
	if (width >= 4)
		saa.caps |= SMC_CAPS_4BIT_MODE;

	if (sc->sc_use_dma) {
		saa.dmat = sc->sc_dmat;
		saa.caps |= SMC_CAPS_DMA;
	}

	sc->sc_sdmmc_dev = config_found(self, &saa, NULL);
}

int
sximmc_set_clock(struct sximmc_softc *sc, u_int freq)
{
#if 0
	uint32_t odly, sdly, clksrc, n, m, clk;
	u_int osc24m_freq = AWIN_REF_FREQ / 1000;
	u_int pll_freq;

	if (awin_chip_id() == AWIN_CHIP_ID_A80) {
		pll_freq = awin_periph0_get_rate() / 1000;
	} else {
		pll_freq = awin_pll6_get_rate() / 1000;
	}

#ifdef SXIMMC_DEBUG
	printf("%s: freq = %d, pll_freq = %d\n", sc->sc_dev.dv_xname,
	    freq, pll_freq);
#endif

	if (freq <= 400) {
		odly = 0;
		sdly = 0;
		clksrc = AWIN_SD_CLK_SRC_SEL_OSC24M;
		n = 2;
		if (freq > 0)
			m = ((osc24m_freq / (1 << n)) / freq) - 1;
		else
			m = 15;
	} else if (freq <= 25000) {
		odly = 0;
		sdly = 5;
		clksrc = AWIN_SD_CLK_SRC_SEL_PLL6;
		n = awin_chip_id() == AWIN_CHIP_ID_A80 ? 2 : 0;
		m = ((pll_freq / freq) / (1 << n)) - 1;
	} else if (freq <= 50000) {
		odly = awin_chip_id() == AWIN_CHIP_ID_A80 ? 5 : 3;
		sdly = awin_chip_id() == AWIN_CHIP_ID_A80 ? 4 : 5;
		clksrc = AWIN_SD_CLK_SRC_SEL_PLL6;
		n = awin_chip_id() == AWIN_CHIP_ID_A80 ? 2 : 0;
		m = ((pll_freq / freq) / (1 << n)) - 1;
	} else {
		/* UHS speeds not implemented yet */
		return EIO;
	}

	clk = bus_space_read_4(sc->sc_bst, sc->sc_clk_bsh, 0);
	clk &= ~AWIN_SD_CLK_SRC_SEL;
	clk |= __SHIFTIN(clksrc, AWIN_SD_CLK_SRC_SEL);
	clk &= ~AWIN_SD_CLK_DIV_RATIO_N;
	clk |= __SHIFTIN(n, AWIN_SD_CLK_DIV_RATIO_N);
	clk &= ~AWIN_SD_CLK_DIV_RATIO_M;
	clk |= __SHIFTIN(m, AWIN_SD_CLK_DIV_RATIO_M);
	clk &= ~AWIN_SD_CLK_OUTPUT_PHASE_CTR;
	clk |= __SHIFTIN(odly, AWIN_SD_CLK_OUTPUT_PHASE_CTR);
	clk &= ~AWIN_SD_CLK_PHASE_CTR;
	clk |= __SHIFTIN(sdly, AWIN_SD_CLK_PHASE_CTR);
	clk |= AWIN_PLL_CFG_ENABLE;
	bus_space_write_4(sc->sc_bst, sc->sc_clk_bsh, 0, clk);
	delay(20000);
#endif

	return 0;
}


int
sximmc_intr(void *priv)
{
	struct sximmc_softc *sc = priv;
	uint32_t idst, rint, mint;

	idst = MMC_READ(sc, SXIMMC_IDST);
	rint = MMC_READ(sc, SXIMMC_RINT);
	mint = MMC_READ(sc, SXIMMC_MINT);
	if (!idst && !rint && !mint)
		return 0;

	MMC_WRITE(sc, SXIMMC_IDST, idst);
	MMC_WRITE(sc, SXIMMC_RINT, rint);
	MMC_WRITE(sc, SXIMMC_MINT, mint);

#ifdef SXIMMC_DEBUG
	printf("%s: mmc intr idst=%08X rint=%08X mint=%08X\n",
	    sc->sc_dev.dv_xname, idst, rint, mint);
#endif

	if (idst) {
		sc->sc_idma_idst |= idst;
		wakeup(&sc->sc_idma_idst);
	}

	if (rint) {
		sc->sc_intr_rint |= rint;
		wakeup(&sc->sc_intr_rint);
	}

	return 1;
}

int
sximmc_wait_rint(struct sximmc_softc *sc, uint32_t mask, int timeout)
{
	int retry;
	int error;

	splassert(IPL_BIO);

	if (sc->sc_intr_rint & mask)
		return 0;

	retry = sc->sc_use_dma ? (timeout / hz) : 10000;

	while (retry > 0) {
		if (sc->sc_use_dma) {
			error = tsleep(&sc->sc_intr_rint, PWAIT, "rint", hz);
			if (error && error != EWOULDBLOCK)
				return error;
			if (sc->sc_intr_rint & mask)
				return 0;
		} else {
			sc->sc_intr_rint |= MMC_READ(sc, SXIMMC_RINT);
			if (sc->sc_intr_rint & mask)
				return 0;
			delay(1000);
		}
		--retry;
	}

	return ETIMEDOUT;
}

void
sximmc_led(struct sximmc_softc *sc, int on)
{
}

int
sximmc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct sximmc_softc *sc = sch;
	int retry = 1000;

#ifdef SXIMMC_DEBUG
	printf("%s: host reset\n", sc->sc_dev.dv_xname);
#endif

#if 0
	if (awin_chip_id() == AWIN_CHIP_ID_A80) {
		if (sc->sc_mmc_port == 2 || sc->sc_mmc_port == 3) {
			MMC_WRITE(sc, SXIMMC_HWRST, 0);
			delay(10);
			MMC_WRITE(sc, SXIMMC_HWRST, 1);
			delay(300);
		}
	}
#endif

	MMC_WRITE(sc, SXIMMC_GCTRL,
	    MMC_READ(sc, SXIMMC_GCTRL) | SXIMMC_GCTRL_RESET);
	while (--retry > 0) {
		if (!(MMC_READ(sc, SXIMMC_GCTRL) & SXIMMC_GCTRL_RESET))
			break;
		delay(100);
	}

	MMC_WRITE(sc, SXIMMC_TIMEOUT, 0xffffffff);

	MMC_WRITE(sc, SXIMMC_IMASK,
	    SXIMMC_INT_CMD_DONE | SXIMMC_INT_ERROR |
	    SXIMMC_INT_DATA_OVER | SXIMMC_INT_AUTO_CMD_DONE);

	MMC_WRITE(sc, SXIMMC_GCTRL,
	    MMC_READ(sc, SXIMMC_GCTRL) | SXIMMC_GCTRL_INTEN);

	return 0;
}

uint32_t
sximmc_host_ocr(sdmmc_chipset_handle_t sch)
{
#if 0
	return MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V | MMC_OCR_HCS;
#else
	return MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V;
#endif
}

int
sximmc_host_maxblklen(sdmmc_chipset_handle_t sch)
{
	return 8192;
}

int
sximmc_card_detect(sdmmc_chipset_handle_t sch)
{
	struct sximmc_softc *sc = sch;
	int inverted, val;

	if (OF_getproplen(sc->sc_node, "non-removable") == 0)
		return 1;

	val = gpio_controller_get_pin(sc->sc_gpio);

	inverted = (OF_getproplen(sc->sc_node, "cd-inverted") == 0);
	return inverted ? !val : val;;
}

int
sximmc_bus_power(sdmmc_chipset_handle_t sch, uint32_t ocr)
{
	return 0;
}

int
sximmc_update_clock(struct sximmc_softc *sc)
{
	uint32_t cmd;
	int retry;

#ifdef SXIMMC_DEBUG
	printf("%s: update clock\n", sc->sc_dev.dv_xname);
#endif

	cmd = SXIMMC_CMD_START |
	      SXIMMC_CMD_UPCLK_ONLY |
	      SXIMMC_CMD_WAIT_PRE_OVER;
	MMC_WRITE(sc, SXIMMC_CMD, cmd);
	retry = 0xfffff;
	while (--retry > 0) {
		if (!(MMC_READ(sc, SXIMMC_CMD) & SXIMMC_CMD_START))
			break;
		delay(10);
	}

	if (retry == 0) {
		printf("%s: timeout updating clock\n", sc->sc_dev.dv_xname);
#ifdef SXIMMC_DEBUG
		printf("GCTRL: 0x%08x\n", MMC_READ(sc, SXIMMC_GCTRL));
		printf("CLKCR: 0x%08x\n", MMC_READ(sc, SXIMMC_CLKCR));
		printf("TIMEOUT: 0x%08x\n", MMC_READ(sc, SXIMMC_TIMEOUT));
		printf("WIDTH: 0x%08x\n", MMC_READ(sc, SXIMMC_WIDTH));
		printf("CMD: 0x%08x\n", MMC_READ(sc, SXIMMC_CMD));
		printf("MINT: 0x%08x\n", MMC_READ(sc, SXIMMC_MINT));
		printf("RINT: 0x%08x\n", MMC_READ(sc, SXIMMC_RINT));
		printf("STATUS: 0x%08x\n", MMC_READ(sc, SXIMMC_STATUS));
#endif
		return ETIMEDOUT;
	}

	return 0;
}

int
sximmc_bus_clock(sdmmc_chipset_handle_t sch, int freq, int timing)
{
	struct sximmc_softc *sc = sch;
	uint32_t clkcr;

	clkcr = MMC_READ(sc, SXIMMC_CLKCR);
	if (clkcr & SXIMMC_CLKCR_CARDCLKON) {
		clkcr &= ~SXIMMC_CLKCR_CARDCLKON;
		MMC_WRITE(sc, SXIMMC_CLKCR, clkcr);
		if (sximmc_update_clock(sc) != 0)
			return 1;
	}

	if (freq) {
		clkcr &= ~SXIMMC_CLKCR_DIV;
		MMC_WRITE(sc, SXIMMC_CLKCR, clkcr);
		if (sximmc_update_clock(sc) != 0)
			return 1;

		if (sximmc_set_clock(sc, freq) != 0)
			return 1;

		clkcr |= SXIMMC_CLKCR_CARDCLKON;
		MMC_WRITE(sc, SXIMMC_CLKCR, clkcr);
		if (sximmc_update_clock(sc) != 0)
			return 1;
	}

	return 0;
}

int
sximmc_bus_width(sdmmc_chipset_handle_t sch, int width)
{
	struct sximmc_softc *sc = sch;

#ifdef SXIMMC_DEBUG
	printf("%s: width = %d\n", sc->sc_dev.dv_xname, width);
#endif

	switch (width) {
	case 1:
		MMC_WRITE(sc, SXIMMC_WIDTH, SXIMMC_WIDTH_1);
		break;
	case 4:
		MMC_WRITE(sc, SXIMMC_WIDTH, SXIMMC_WIDTH_4);
		break;
	case 8:
		MMC_WRITE(sc, SXIMMC_WIDTH, SXIMMC_WIDTH_8);
		break;
	default:
		return 1;
	}

	return 0;
}

int
sximmc_pio_wait(struct sximmc_softc *sc, struct sdmmc_command *cmd)
{
	int retry = 0xfffff;
	uint32_t bit = (cmd->c_flags & SCF_CMD_READ) ?
	    SXIMMC_STATUS_FIFO_EMPTY : SXIMMC_STATUS_FIFO_FULL;

	while (--retry > 0) {
		uint32_t status = MMC_READ(sc, SXIMMC_STATUS);
		if (!(status & bit))
			return 0;
		delay(10);
	}

	return ETIMEDOUT;
}

int
sximmc_pio_transfer(struct sximmc_softc *sc, struct sdmmc_command *cmd)
{
	uint32_t *datap = (uint32_t *)cmd->c_data;
	int i;

	for (i = 0; i < (cmd->c_resid >> 2); i++) {
		if (sximmc_pio_wait(sc, cmd))
			return ETIMEDOUT;
		if (cmd->c_flags & SCF_CMD_READ) {
			datap[i] = MMC_READ(sc, sc->sc_fifo_reg);
		} else {
			MMC_WRITE(sc, sc->sc_fifo_reg, datap[i]);
		}
	}

	return 0;
}

int
sximmc_dma_prepare(struct sximmc_softc *sc, struct sdmmc_command *cmd)
{
	struct sximmc_idma_descriptor *dma = (void *)sc->sc_idma_desc;
	bus_addr_t desc_paddr = sc->sc_idma_map->dm_segs[0].ds_addr;
	bus_size_t off;
	int desc, resid, seg;
	uint32_t val;

	desc = 0;
	for (seg = 0; seg < cmd->c_dmamap->dm_nsegs; seg++) {
		bus_addr_t paddr = cmd->c_dmamap->dm_segs[seg].ds_addr;
		bus_size_t len = cmd->c_dmamap->dm_segs[seg].ds_len;
		resid = min(len, cmd->c_resid);
		off = 0;
		while (resid > 0) {
			if (desc == sc->sc_idma_ndesc)
				break;
			len = min(sc->sc_idma_xferlen, resid);
			dma[desc].dma_buf_size = htole32(len);
			dma[desc].dma_buf_addr = htole32(paddr + off);
			dma[desc].dma_config = htole32(SXIMMC_IDMA_CONFIG_CH |
					       SXIMMC_IDMA_CONFIG_OWN);
			cmd->c_resid -= len;
			resid -= len;
			off += len;
			if (desc == 0) {
				dma[desc].dma_config |=
				    htole32(SXIMMC_IDMA_CONFIG_FD);
			}
			if (cmd->c_resid == 0) {
				dma[desc].dma_config |=
				    htole32(SXIMMC_IDMA_CONFIG_LD);
				dma[desc].dma_config |=
				    htole32(SXIMMC_IDMA_CONFIG_ER);
				dma[desc].dma_next = 0;
			} else {
				dma[desc].dma_config |=
				    htole32(SXIMMC_IDMA_CONFIG_DIC);
				dma[desc].dma_next = htole32(
				    desc_paddr + ((desc+1) *
				    sizeof(struct sximmc_idma_descriptor)));
			}
			++desc;
		}
	}
	if (desc == sc->sc_idma_ndesc) {
		printf("%s: not enough descriptors for %d byte transfer!\n",
		    sc->sc_dev.dv_xname, cmd->c_datalen);
		return EIO;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_idma_map, 0,
	    sc->sc_idma_size, BUS_DMASYNC_PREWRITE);

	sc->sc_idma_idst = 0;

	val = MMC_READ(sc, SXIMMC_GCTRL);
	val |= SXIMMC_GCTRL_DMAEN;
	val |= SXIMMC_GCTRL_INTEN;
	MMC_WRITE(sc, SXIMMC_GCTRL, val);
	val |= SXIMMC_GCTRL_DMARESET;
	MMC_WRITE(sc, SXIMMC_GCTRL, val);
	MMC_WRITE(sc, SXIMMC_DMAC, SXIMMC_DMAC_SOFTRESET);
	MMC_WRITE(sc, SXIMMC_DMAC,
	    SXIMMC_DMAC_IDMA_ON|SXIMMC_DMAC_FIX_BURST);
	val = MMC_READ(sc, SXIMMC_IDIE);
	val &= ~(SXIMMC_IDST_RECEIVE_INT|SXIMMC_IDST_TRANSMIT_INT);
	if (cmd->c_flags & SCF_CMD_READ)
		val |= SXIMMC_IDST_RECEIVE_INT;
	else
		val |= SXIMMC_IDST_TRANSMIT_INT;
	MMC_WRITE(sc, SXIMMC_IDIE, val);
	MMC_WRITE(sc, SXIMMC_DLBA, desc_paddr);
	MMC_WRITE(sc, SXIMMC_FTRGLEVEL, sc->sc_dma_ftrglevel);

	return 0;
}

void
sximmc_dma_complete(struct sximmc_softc *sc)
{
	bus_dmamap_sync(sc->sc_dmat, sc->sc_idma_map, 0,
	    sc->sc_idma_size, BUS_DMASYNC_POSTWRITE);
}

void
sximmc_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct sximmc_softc *sc = sch;
	uint32_t cmdval = SXIMMC_CMD_START;
	int retry;
	int s;

#ifdef SXIMMC_DEBUG
	printf("%s: opcode %d flags 0x%x data %p datalen %d blklen %d\n",
	    sc->sc_dev.dv_xname, cmd->c_opcode, cmd->c_flags,
	    cmd->c_data, cmd->c_datalen, cmd->c_blklen);
#endif

	s = splbio();

	if (cmd->c_opcode == 0)
		cmdval |= SXIMMC_CMD_SEND_INIT_SEQ;
	if (cmd->c_flags & SCF_RSP_PRESENT)
		cmdval |= SXIMMC_CMD_RSP_EXP;
	if (cmd->c_flags & SCF_RSP_136)
		cmdval |= SXIMMC_CMD_LONG_RSP;
	if (cmd->c_flags & SCF_RSP_CRC)
		cmdval |= SXIMMC_CMD_CHECK_RSP_CRC;

	if (cmd->c_datalen > 0) {
		unsigned int nblks;

		cmdval |= SXIMMC_CMD_DATA_EXP | SXIMMC_CMD_WAIT_PRE_OVER;
		if (!ISSET(cmd->c_flags, SCF_CMD_READ)) {
			cmdval |= SXIMMC_CMD_WRITE;
		}

		nblks = cmd->c_datalen / cmd->c_blklen;
		if (nblks == 0 || (cmd->c_datalen % cmd->c_blklen) != 0)
			++nblks;

		if (nblks > 1) {
			cmdval |= SXIMMC_CMD_SEND_AUTO_STOP;
		}

		MMC_WRITE(sc, SXIMMC_BLKSZ, cmd->c_blklen);
		MMC_WRITE(sc, SXIMMC_BYTECNT, nblks * cmd->c_blklen);
	}

	sc->sc_intr_rint = 0;

#if 0
	if (awin_chip_id() == AWIN_CHIP_ID_A80) {
		MMC_WRITE(sc, SXIMMC_A12A,
		    (cmdval & SXIMMC_CMD_SEND_AUTO_STOP) ? 0 : 0xffff);
	}
#endif

	MMC_WRITE(sc, SXIMMC_ARG, cmd->c_arg);

#ifdef SXIMMC_DEBUG
	printf("%s: cmdval = %08x\n", sc->sc_dev.dv_xname, cmdval);
#endif

	if (cmd->c_datalen == 0) {
		MMC_WRITE(sc, SXIMMC_CMD, cmdval | cmd->c_opcode);
	} else {
		cmd->c_resid = cmd->c_datalen;
		sximmc_led(sc, 0);
		if (cmd->c_dmamap && sc->sc_use_dma) {
			cmd->c_error = sximmc_dma_prepare(sc, cmd);
			MMC_WRITE(sc, SXIMMC_CMD, cmdval | cmd->c_opcode);
			if (cmd->c_error == 0) {
				cmd->c_error = tsleep(&sc->sc_idma_idst,
				    PWAIT, "idma", hz*10);
			}
			sximmc_dma_complete(sc);
			if (sc->sc_idma_idst & SXIMMC_IDST_ERROR) {
				cmd->c_error = EIO;
			} else if (!(sc->sc_idma_idst & SXIMMC_IDST_COMPLETE)) {
				cmd->c_error = ETIMEDOUT;
			}
		} else {
			splx(s);
			MMC_WRITE(sc, SXIMMC_CMD, cmdval | cmd->c_opcode);
			cmd->c_error = sximmc_pio_transfer(sc, cmd);
			s = splbio();
		}
		sximmc_led(sc, 1);
		if (cmd->c_error) {
#ifdef SXIMMC_DEBUG
			printf("%s: xfer failed, error %d\n",
			    sc->sc_dev.dv_xname, cmd->c_error);
#endif
			goto done;
		}
	}

	cmd->c_error = sximmc_wait_rint(sc,
	    SXIMMC_INT_ERROR|SXIMMC_INT_CMD_DONE, hz * 10);
	if (cmd->c_error == 0 && (sc->sc_intr_rint & SXIMMC_INT_ERROR)) {
		if (sc->sc_intr_rint & SXIMMC_INT_RESP_TIMEOUT) {
			cmd->c_error = ETIMEDOUT;
		} else {
			cmd->c_error = EIO;
		}
	}
	if (cmd->c_error) {
#ifdef SXIMMC_DEBUG
		printf("%s: cmd failed, error %d\n",
		    sc->sc_dev.dv_xname,  cmd->c_error);
#endif
		goto done;
	}
		
	if (cmd->c_datalen > 0) {
		cmd->c_error = sximmc_wait_rint(sc,
		    SXIMMC_INT_ERROR|
		    SXIMMC_INT_AUTO_CMD_DONE|
		    SXIMMC_INT_DATA_OVER,
		    hz*10);
		if (cmd->c_error == 0 &&
		    (sc->sc_intr_rint & SXIMMC_INT_ERROR)) {
			cmd->c_error = ETIMEDOUT;
		}
		if (cmd->c_error) {
#ifdef SXIMMC_DEBUG
			printf("%s: data timeout, rint = %08x\n",
			    sc->sc_dev.dv_xname, sc->sc_intr_rint);
#endif
			cmd->c_error = ETIMEDOUT;
			goto done;
		}
	}

	if (cmd->c_flags & SCF_RSP_PRESENT) {
		if (cmd->c_flags & SCF_RSP_136) {
			cmd->c_resp[0] = MMC_READ(sc, SXIMMC_RESP0);
			cmd->c_resp[1] = MMC_READ(sc, SXIMMC_RESP1);
			cmd->c_resp[2] = MMC_READ(sc, SXIMMC_RESP2);
			cmd->c_resp[3] = MMC_READ(sc, SXIMMC_RESP3);
			if (cmd->c_flags & SCF_RSP_CRC) {
				cmd->c_resp[0] = (cmd->c_resp[0] >> 8) |
				    (cmd->c_resp[1] << 24);
				cmd->c_resp[1] = (cmd->c_resp[1] >> 8) |
				    (cmd->c_resp[2] << 24);
				cmd->c_resp[2] = (cmd->c_resp[2] >> 8) |
				    (cmd->c_resp[3] << 24);
				cmd->c_resp[3] = (cmd->c_resp[3] >> 8);
			}
		} else {
			cmd->c_resp[0] = MMC_READ(sc, SXIMMC_RESP0);
		}
	}

done:
	cmd->c_flags |= SCF_ITSDONE;
	splx(s);

	if (cmd->c_error) {
#ifdef SXIMMC_DEBUG
		printf("%s: i/o error %d\n", sc->sc_dev.dv_xname,
		    cmd->c_error);
#endif
		MMC_WRITE(sc, SXIMMC_GCTRL,
		    MMC_READ(sc, SXIMMC_GCTRL) |
		      SXIMMC_GCTRL_DMARESET | SXIMMC_GCTRL_FIFORESET);
		for (retry = 0; retry < 1000; retry++) {
			if (!(MMC_READ(sc, SXIMMC_GCTRL) & SXIMMC_GCTRL_RESET))
				break;
			delay(10);
		}
		sximmc_update_clock(sc);
	}

	if (!cmd->c_dmamap || !sc->sc_use_dma) {
		MMC_WRITE(sc, SXIMMC_GCTRL,
		    MMC_READ(sc, SXIMMC_GCTRL) | SXIMMC_GCTRL_FIFORESET);
	}
}
