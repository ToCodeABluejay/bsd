/* Public Domain */

/*
 * RK3288 clocks.
 */

#define RK3288_PLL_APLL			1
#define RK3288_PLL_CPLL			3
#define RK3288_PLL_GPLL			4
#define RK3288_PLL_NPLL			5
#define RK3288_ARMCLK			6

#define RK3288_CLK_SDMMC		68
#define RK3288_CLK_TSADC		72
#define RK3288_CLK_UART0		77
#define RK3288_CLK_UART1		78
#define RK3288_CLK_UART2		79
#define RK3288_CLK_UART3		80
#define RK3288_CLK_UART4		81
#define RK3288_CLK_MAC_RX		102
#define RK3288_CLK_MAC_TX		103
#define RK3288_CLK_SDMMC_DRV		114
#define RK3288_CLK_SDMMC_SAMPLE		118
#define RK3288_CLK_MAC			151

#define RK3288_ACLK_GMAC		196

#define RK3288_PCLK_I2C0		332
#define RK3288_PCLK_I2C1		333
#define RK3288_PCLK_I2C2		334
#define RK3288_PCLK_I2C3		335
#define RK3288_PCLK_I2C4		336
#define RK3288_PCLK_I2C5		337
#define RK3288_PCLK_TSADC		346
#define RK3288_PCLK_GMAC		349

#define RK3288_HCLK_HOST0		450
#define RK3288_HCLK_SDMMC		456

#define RK3288_XIN24M			1023

/*
 * RK3328 clocks.
 */

#define RK3328_PLL_APLL			1
#define RK3328_PLL_DPLL			2
#define RK3328_PLL_CPLL			3
#define RK3328_PLL_GPLL			4
#define RK3328_PLL_NPLL			5
#define RK3328_ARMCLK			6

#define RK3328_CLK_RTC32K		30
#define RK3328_CLK_SDMMC		33
#define RK3328_CLK_SDIO			34
#define RK3328_CLK_EMMC			35
#define RK3328_CLK_TSADC		36
#define RK3328_CLK_UART0		38
#define RK3328_CLK_UART1		39
#define RK3328_CLK_UART2		40
#define RK3328_CLK_WIFI			53
#define RK3328_CLK_I2C0			55
#define RK3328_CLK_I2C1			56
#define RK3328_CLK_I2C2			57
#define RK3328_CLK_I2C3			58
#define RK3328_CLK_CRYPTO		59
#define RK3328_CLK_PDM			61
#define RK3328_CLK_VDEC_CABAC		65
#define RK3328_CLK_VDEC_CORE		66
#define RK3328_CLK_VENC_DSP		67
#define RK3328_CLK_VENC_CORE		68
#define RK3328_CLK_TSP			92
#define RK3328_CLK_MAC2IO_SRC		99
#define RK3328_CLK_MAC2IO		100
#define RK3328_CLK_MAC2IO_EXT		102

#define RK3328_DCLK_LCDC		120
#define RK3328_HDMIPHY			122
#define RK3328_USB480M			123
#define RK3328_DCLK_LCDC_SRC		124

#define RK3328_ACLK_VOP_PRE		131
#define RK3328_ACLK_RGA_PRE		133
#define RK3328_ACLK_BUS_PRE		136
#define RK3328_ACLK_PERI_PRE		137
#define RK3328_ACLK_RKVDEC_PRE		138
#define RK3328_ACLK_RKVENC		140
#define RK3328_ACLK_VPU_PRE		141
#define RK3328_ACLK_VIO_PRE		142

#define RK3328_PCLK_BUS_PRE		216
#define RK3328_PCLK_PERI		230

#define RK3328_HCLK_PERI		308
#define RK3328_HCLK_BUS_PRE		328
#define RK3328_HCLK_CRYPTO_SLV		337

#define RK3328_XIN24M			1023
#define RK3328_CLK_24M			1022
#define RK3328_GMAC_CLKIN		1021

/*
 * RK3399 clocks.
 */

#define RK3399_PLL_ALPLL		1
#define RK3399_PLL_ABPLL		2
#define RK3399_PLL_DPLL			3
#define RK3399_PLL_CPLL			4
#define RK3399_PLL_GPLL			5
#define RK3399_PLL_NPLL			6
#define RK3399_PLL_VPLL			7
#define RK3399_ARMCLKL			8
#define RK3399_ARMCLKB			9

#define RK3399_CLK_I2C1			65
#define RK3399_CLK_I2C2			66
#define RK3399_CLK_I2C3			67
#define RK3399_CLK_I2C5			68
#define RK3399_CLK_I2C6			69
#define RK3399_CLK_I2C7			70
#define RK3399_CLK_SDMMC		76
#define RK3399_CLK_SDIO			77
#define RK3399_CLK_EMMC			78
#define RK3399_CLK_TSADC		79
#define RK3399_CLK_UART0		81
#define RK3399_CLK_UART1		82
#define RK3399_CLK_UART2		83
#define RK3399_CLK_UART3		84
#define RK3399_CLK_SPDIF_8CH		85
#define RK3399_CLK_I2S0_8CH		86
#define RK3399_CLK_I2S1_8CH		87
#define RK3399_CLK_I2S2_8CH		88
#define RK3399_CLK_I2S_8CH_OUT		89
#define RK3399_CLK_MAC_RX		103
#define RK3399_CLK_MAC_TX		104
#define RK3399_CLK_MAC			105
#define RK3399_CLK_USB3OTG0_REF		129
#define RK3399_CLK_USB3OTG1_REF		130
#define RK3399_CLK_USB3OTG0_SUSPEND	131
#define RK3399_CLK_USB3OTG1_SUSPEND	132
#define RK3399_CLK_SDMMC_DRV		154
#define RK3399_CLK_SDMMC_SAMPLE		155

#define RK3399_DCLK_VOP0		180
#define RK3399_DCLK_VOP1		181
#define RK3399_DCLK_VOP0_DIV		182
#define RK3399_DCLK_VOP1_DIV		183
#define RK3399_DCLK_VOP0_FRAC		185
#define RK3399_DCLK_VOP1_FRAC		186

#define RK3399_ACLK_PERIPH		192
#define RK3399_ACLK_PERILP0		194
#define RK3399_ACLK_CCI			201
#define RK3399_ACLK_GMAC		213
#define RK3399_ACLK_VOP0_NOC		216
#define RK3399_ACLK_VOP0		217
#define RK3399_ACLK_VOP1_NOC		218
#define RK3399_ACLK_VOP1		219
#define RK3399_ACLK_HDCP		222
#define RK3399_ACLK_VIO			227
#define RK3399_ACLK_EMMC		240
#define RK3399_ACLK_USB3OTG0		246
#define RK3399_ACLK_USB3OTG1		247
#define RK3399_ACLK_USB3_GRF		249
#define RK3399_ACLK_GIC_PRE		262

#define RK3399_PCLK_PERIPH		320
#define RK3399_PCLK_PERILP0		322
#define RK3399_PCLK_PERILP1		323
#define RK3399_PCLK_I2C1		341
#define RK3399_PCLK_I2C2		342
#define RK3399_PCLK_I2C3		343
#define RK3399_PCLK_I2C5		344
#define RK3399_PCLK_I2C6		345
#define RK3399_PCLK_I2C7		346
#define RK3399_PCLK_TSADC		356
#define RK3399_PCLK_GMAC		358
#define RK3399_PCLK_DDR			376

#define RK3399_HCLK_PERIPH		448
#define RK3399_HCLK_PERILP0		449
#define RK3399_HCLK_PERILP1		450
#define RK3399_HCLK_HOST0		456
#define RK3399_HCLK_HOST0_ARB		457
#define RK3399_HCLK_HOST1		458
#define RK3399_HCLK_HOST1_ARB		459
#define RK3399_HCLK_SDMMC		462
#define RK3399_HCLK_VOP0_NOC		472
#define RK3399_HCLK_VOP0		473
#define RK3399_HCLK_VOP1_NOC		474
#define RK3399_HCLK_VOP1		475

/* PMUCRU */

#define RK3399_PLL_PPLL			1

#define RK3399_CLK_I2C0			9
#define RK3399_CLK_I2C4			10
#define RK3399_CLK_I2C8			11

#define RK3399_PCLK_I2C0		27
#define RK3399_PCLK_I2C4		28
#define RK3399_PCLK_I2C8		29
#define RK3399_PCLK_RKPWM		30

#define RK3399_XIN24M			1023
#define RK3399_CLK_32K			1022
#define RK3399_XIN12M			1021
#define RK3399_CLK_I2S0_DIV		1020
#define RK3399_CLK_I2S1_DIV		1019
#define RK3399_CLK_I2S2_DIV		1018
#define RK3399_CLK_I2SOUT_SRC		1017
