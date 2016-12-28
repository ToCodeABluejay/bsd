/* Public Domain */

/*
 * i.MX6Q clocks.
 */

#define IMX6_CLK_IPG		0x3e
#define IMX6_CLK_IPG_PER	0x3f
#define IMX6_CLK_I2C1		0x7d
#define IMX6_CLK_I2C2		0x7e
#define IMX6_CLK_I2C3		0x7f
#define IMX6_CLK_UART_IPG	0xa0
#define IMX6_CLK_UART_SERIAL	0xa1
#define IMX6_CLK_USBOH3		0xa2
#define IMX6_CLK_USDHC1		0xa3
#define IMX6_CLK_USDHC2		0xa4
#define IMX6_CLK_USDHC3		0xa5
#define IMX6_CLK_USDHC4		0xa6

struct imxccm_gate imx6_gates[] = {
	[IMX6_CLK_I2C1] = { CCM_CCGR2, 3, IMX6_CLK_IPG_PER },
	[IMX6_CLK_I2C2] = { CCM_CCGR2, 4, IMX6_CLK_IPG_PER },
	[IMX6_CLK_I2C3] = { CCM_CCGR2, 5, IMX6_CLK_IPG_PER },
	[IMX6_CLK_UART_IPG] = { CCM_CCGR5, 12, IMX6_CLK_IPG },
	[IMX6_CLK_UART_SERIAL] = { CCM_CCGR5, 13 },
	[IMX6_CLK_USBOH3] = { CCM_CCGR6, 0 },
	[IMX6_CLK_USDHC1] = { CCM_CCGR6, 1 },
	[IMX6_CLK_USDHC2] = { CCM_CCGR6, 2 },
	[IMX6_CLK_USDHC3] = { CCM_CCGR6, 3 },
	[IMX6_CLK_USDHC4] = { CCM_CCGR6, 4 },
};
