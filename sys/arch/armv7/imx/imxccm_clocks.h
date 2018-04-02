/* Public Domain */

/*
 * i.MX6Q clocks.
 */

#define IMX6_CLK_IPG		0x3e
#define IMX6_CLK_IPG_PER	0x3f
#define IMX6_CLK_ARM		0x68
#define IMX6_CLK_AHB		0x69
#define IMX6_CLK_ENET		0x75
#define IMX6_CLK_I2C1		0x7d
#define IMX6_CLK_I2C2		0x7e
#define IMX6_CLK_I2C3		0x7f
#define IMX6_CLK_SATA		0x9a
#define IMX6_CLK_UART_IPG	0xa0
#define IMX6_CLK_UART_SERIAL	0xa1
#define IMX6_CLK_USBOH3		0xa2
#define IMX6_CLK_USDHC1		0xa3
#define IMX6_CLK_USDHC2		0xa4
#define IMX6_CLK_USDHC3		0xa5
#define IMX6_CLK_USDHC4		0xa6
#define IMX6_CLK_USBPHY1	0xb6
#define IMX6_CLK_USBPHY2	0xb7
#define IMX6_CLK_SATA_REF	0xba
#define IMX6_CLK_SATA_REF_100	0xbb

struct imxccm_gate imx6_gates[] = {
	[IMX6_CLK_ENET] = { CCM_CCGR1, 5, IMX6_CLK_IPG },
	[IMX6_CLK_I2C1] = { CCM_CCGR2, 3, IMX6_CLK_IPG_PER },
	[IMX6_CLK_I2C2] = { CCM_CCGR2, 4, IMX6_CLK_IPG_PER },
	[IMX6_CLK_I2C3] = { CCM_CCGR2, 5, IMX6_CLK_IPG_PER },
	[IMX6_CLK_SATA] = { CCM_CCGR5, 2 },
	[IMX6_CLK_UART_IPG] = { CCM_CCGR5, 12, IMX6_CLK_IPG },
	[IMX6_CLK_UART_SERIAL] = { CCM_CCGR5, 13 },
	[IMX6_CLK_USBOH3] = { CCM_CCGR6, 0 },
	[IMX6_CLK_USDHC1] = { CCM_CCGR6, 1 },
	[IMX6_CLK_USDHC2] = { CCM_CCGR6, 2 },
	[IMX6_CLK_USDHC3] = { CCM_CCGR6, 3 },
	[IMX6_CLK_USDHC4] = { CCM_CCGR6, 4 },
};

/*
 * i.MX6UL clocks.
 */

#define IMX6UL_CLK_ARM		0x5d
#define IMX6UL_CLK_PERCLK	0x63
#define IMX6UL_CLK_IPG		0x64
#define IMX6UL_CLK_GPT1_BUS	0x98
#define IMX6UL_CLK_GPT1_SERIAL	0x99
#define IMX6UL_CLK_I2C1		0x9c
#define IMX6UL_CLK_I2C2		0x9d
#define IMX6UL_CLK_I2C3		0x9e
#define IMX6UL_CLK_I2C4		0x9f
#define IMX6UL_CLK_UART1_IPG	0xbd
#define IMX6UL_CLK_UART1_SERIAL	0xbe
#define IMX6UL_CLK_USBOH3	0xcd
#define IMX6UL_CLK_USDHC1	0xce
#define IMX6UL_CLK_USDHC2	0xcf

struct imxccm_gate imx6ul_gates[] =
{
	[IMX6UL_CLK_GPT1_BUS] = { CCM_CCGR1, 10, IMX6UL_CLK_PERCLK },
	[IMX6UL_CLK_GPT1_SERIAL] = { CCM_CCGR1, 11, IMX6UL_CLK_PERCLK },
	[IMX6UL_CLK_I2C1] = { CCM_CCGR2, 3, IMX6UL_CLK_PERCLK },
	[IMX6UL_CLK_I2C2] = { CCM_CCGR2, 4, IMX6UL_CLK_PERCLK },
	[IMX6UL_CLK_I2C3] = { CCM_CCGR2, 5, IMX6UL_CLK_PERCLK },
	[IMX6UL_CLK_I2C4] = { CCM_CCGR6, 12, IMX6UL_CLK_PERCLK },
	[IMX6UL_CLK_UART1_IPG] = { CCM_CCGR5, 12, IMX6UL_CLK_IPG },
	[IMX6UL_CLK_UART1_SERIAL] = { CCM_CCGR5, 12 },
	[IMX6UL_CLK_USBOH3] = { CCM_CCGR6, 0 },
	[IMX6UL_CLK_USDHC1] = { CCM_CCGR6, 1 },
	[IMX6UL_CLK_USDHC2] = { CCM_CCGR6, 2 },
};
