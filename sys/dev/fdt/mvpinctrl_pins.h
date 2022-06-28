/* Public Domain */


const struct mvpinctrl_pin armada_38x_pins[] = {
	MPP(0, "gpio", 0),
	MPP(0, "ua0", 1),
	MPP(1, "gpio", 0),
	MPP(1, "ua0", 1),
	MPP(2, "gpio", 0),
	MPP(2, "i2c0", 1),
	MPP(3, "gpio", 0),
	MPP(3, "i2c0", 1),
	MPP(4, "gpio", 0),
	MPP(4, "ge", 1),
	MPP(4, "ua1", 2),
	MPP(4, "ua0", 3),
	MPP(5, "gpio", 0),
	MPP(5, "ge", 1),
	MPP(5, "ua1", 2),
	MPP(5, "ua0", 3),
	MPP(6, "gpio", 0),
	MPP(6, "ge0", 1),
	MPP(6, "ge0", 2),
	MPP(6, "dev", 5),
	MPP(7, "gpio", 0),
	MPP(7, "ge0", 1),
	MPP(7, "dev", 5),
	MPP(8, "gpio", 0),
	MPP(8, "ge0", 1),
	MPP(8, "dev", 5),
	MPP(9, "gpio", 0),
	MPP(9, "ge0", 1),
	MPP(9, "dev", 5),
	MPP(10, "gpio", 0),
	MPP(10, "ge0", 1),
	MPP(10, "dev", 5),
	MPP(11, "gpio", 0),
	MPP(11, "ge0", 1),
	MPP(11, "dev", 5),
	MPP(12, "gpio", 0),
	MPP(12, "ge0", 1),
	MPP(12, "pcie0", 2),
	MPP(12, "spi0", 4),
	MPP(12, "dev", 5),
	MPP(12, "pcie3", 6),
	MPP(13, "gpio", 0),
	MPP(13, "ge0", 1),
	MPP(13, "pcie0", 2),
	MPP(13, "pcie1", 3),
	MPP(13, "spi0", 4),
	MPP(13, "dev", 5),
	MPP(13, "pcie2", 6),
	MPP(14, "gpio", 0),
	MPP(14, "ge0", 1),
	MPP(14, "ptp", 2),
	MPP(14, "dram", 3),
	MPP(14, "spi0", 4),
	MPP(14, "dev", 5),
	MPP(14, "pcie3", 6),
	MPP(15, "gpio", 0),
	MPP(15, "ge0", 1),
	MPP(15, "ge", 2),
	MPP(15, "pcie0", 3),
	MPP(15, "spi0", 4),
	MPP(16, "gpio", 0),
	MPP(16, "ge0", 1),
	MPP(16, "ge", 2),
	MPP(16, "dram", 3),
	MPP(16, "spi0", 4),
	MPP(16, "pcie0", 5),
	MPP(16, "pcie1", 6),
	MPP(17, "gpio", 0),
	MPP(17, "ge0", 1),
	MPP(17, "ptp", 2),
	MPP(17, "ua1", 3),
	MPP(17, "spi0", 4),
	MPP(17, "sata1", 5),
	MPP(17, "sata0", 6),
	MPP(18, "gpio", 0),
	MPP(18, "ge0", 1),
	MPP(18, "ptp", 2),
	MPP(18, "ua1", 3),
	MPP(18, "spi0", 4),
	MPP(19, "gpio", 0),
	MPP(19, "ge0", 1),
	MPP(19, "ptp", 2),
	MPP(19, "ge0", 3),
	MPP(19, "sata1", 4),
	MPP(19, "ua0", 5),
	MPP(19, "ua1", 6),
	MPP(20, "gpio", 0),
	MPP(20, "ge0", 1),
	MPP(20, "ptp", 2),
	MPP(20, "sata0", 4),
	MPP(20, "ua0", 5),
	MPP(20, "ua1", 6),
	MPP(21, "gpio", 0),
	MPP(21, "spi0", 1),
	MPP(21, "ge1", 2),
	MPP(21, "sata0", 3),
	MPP(21, "sd0", 4),
	MPP(21, "dev", 5),
	MPP(21, "sata1", 6),
	MPP(22, "gpio", 0),
	MPP(22, "spi0", 1),
	MPP(22, "dev", 5),
	MPP(23, "gpio", 0),
	MPP(23, "spi0", 1),
	MPP(23, "dev", 5),
	MPP(24, "gpio", 0),
	MPP(24, "spi0", 1),
	MPP(24, "ua0", 2),
	MPP(24, "ua1", 3),
	MPP(24, "sd0", 4),
	MPP(24, "dev", 5),
	MPP(25, "gpio", 0),
	MPP(25, "spi0", 1),
	MPP(25, "ua0", 2),
	MPP(25, "ua1", 3),
	MPP(25, "sd0", 4),
	MPP(25, "dev", 5),
	MPP(26, "gpio", 0),
	MPP(26, "spi0", 1),
	MPP(26, "i2c1", 3),
	MPP(26, "sd0", 4),
	MPP(26, "dev", 5),
	MPP(27, "gpio", 0),
	MPP(27, "spi0", 1),
	MPP(27, "ge1", 2),
	MPP(27, "i2c1", 3),
	MPP(27, "sd0", 4),
	MPP(27, "dev", 5),
	MPP(28, "gpio", 0),
	MPP(28, "ge1", 2),
	MPP(28, "sd0", 4),
	MPP(28, "dev", 5),
	MPP(29, "gpio", 0),
	MPP(29, "ge1", 2),
	MPP(29, "dev", 5),
	MPP(30, "gpio", 0),
	MPP(30, "ge1", 2),
	MPP(30, "dev", 5),
	MPP(31, "gpio", 0),
	MPP(31, "ge1", 2),
	MPP(31, "dev", 5),
	MPP(32, "gpio", 0),
	MPP(32, "ge1", 2),
	MPP(32, "dev", 5),
	MPP(33, "gpio", 0),
	MPP(33, "dram", 1),
	MPP(33, "dev", 5),
	MPP(34, "gpio", 0),
	MPP(34, "dev", 5),
	MPP(35, "gpio", 0),
	MPP(35, "ref", 1),
	MPP(35, "dev", 5),
	MPP(36, "gpio", 0),
	MPP(36, "ptp", 1),
	MPP(36, "dev", 5),
	MPP(37, "gpio", 0),
	MPP(37, "ptp", 1),
	MPP(37, "ge1", 2),
	MPP(37, "sd0", 4),
	MPP(37, "dev", 5),
	MPP(38, "gpio", 0),
	MPP(38, "ptp", 1),
	MPP(38, "ge1", 2),
	MPP(38, "ref", 3),
	MPP(38, "sd0", 4),
	MPP(38, "dev", 5),
	MPP(39, "gpio", 0),
	MPP(39, "i2c1", 1),
	MPP(39, "ge1", 2),
	MPP(39, "ua0", 3),
	MPP(39, "sd0", 4),
	MPP(39, "dev", 5),
	MPP(40, "gpio", 0),
	MPP(40, "i2c1", 1),
	MPP(40, "ge1", 2),
	MPP(40, "ua0", 3),
	MPP(40, "sd0", 4),
	MPP(40, "dev", 5),
	MPP(41, "gpio", 0),
	MPP(41, "ua1", 1),
	MPP(41, "ge1", 2),
	MPP(41, "ua0", 3),
	MPP(41, "spi1", 4),
	MPP(41, "dev", 5),
	MPP(41, "nand", 6),
	MPP(42, "gpio", 0),
	MPP(42, "ua1", 1),
	MPP(42, "ua0", 3),
	MPP(42, "dev", 5),
	MPP(43, "gpio", 0),
	MPP(43, "pcie0", 1),
	MPP(43, "dram", 2),
	MPP(43, "dram", 3),
	MPP(43, "spi1", 4),
	MPP(43, "dev", 5),
	MPP(43, "nand", 6),
	MPP(44, "gpio", 0),
	MPP(44, "sata0", 1),
	MPP(44, "sata1", 2),
	MPP(44, "sata2", 3),
	MPP(44, "sata3", 4),
	MPP(45, "gpio", 0),
	MPP(45, "ref", 1),
	MPP(45, "pcie0", 2),
	MPP(45, "ua1", 6),
	MPP(46, "gpio", 0),
	MPP(46, "ref", 1),
	MPP(46, "pcie0", 2),
	MPP(46, "ua1", 6),
	MPP(47, "gpio", 0),
	MPP(47, "sata0", 1),
	MPP(47, "sata1", 2),
	MPP(47, "sata2", 3),
	MPP(47, "sata3", 5),
	MPP(48, "gpio", 0),
	MPP(48, "sata0", 1),
	MPP(48, "dram", 2),
	MPP(48, "tdm", 3),
	MPP(48, "audio", 4),
	MPP(48, "sd0", 5),
	MPP(48, "pcie0", 6),
	MPP(49, "gpio", 0),
	MPP(49, "sata2", 1),
	MPP(49, "sata3", 2),
	MPP(49, "tdm", 3),
	MPP(49, "audio", 4),
	MPP(49, "sd0", 5),
	MPP(49, "pcie1", 6),
	MPP(50, "gpio", 0),
	MPP(50, "pcie0", 1),
	MPP(50, "tdm", 3),
	MPP(50, "audio", 4),
	MPP(50, "sd0", 5),
	MPP(51, "gpio", 0),
	MPP(51, "tdm", 3),
	MPP(51, "audio", 4),
	MPP(51, "dram", 5),
	MPP(51, "ptp", 6),
	MPP(52, "gpio", 0),
	MPP(52, "pcie0", 1),
	MPP(52, "tdm", 3),
	MPP(52, "audio", 4),
	MPP(52, "sd0", 5),
	MPP(52, "ptp", 6),
	MPP(53, "gpio", 0),
	MPP(53, "sata1", 1),
	MPP(53, "sata0", 2),
	MPP(53, "tdm", 3),
	MPP(53, "audio", 4),
	MPP(53, "sd0", 5),
	MPP(53, "ptp", 6),
	MPP(54, "gpio", 0),
	MPP(54, "sata0", 1),
	MPP(54, "sata1", 2),
	MPP(54, "pcie0", 3),
	MPP(54, "ge0", 4),
	MPP(54, "sd0", 5),
	MPP(55, "gpio", 0),
	MPP(55, "ua1", 1),
	MPP(55, "ge", 2),
	MPP(55, "pcie1", 3),
	MPP(55, "spi1", 4),
	MPP(55, "sd0", 5),
	MPP(55, "ua1", 6),
	MPP(56, "gpio", 0),
	MPP(56, "ua1", 1),
	MPP(56, "ge", 2),
	MPP(56, "dram", 3),
	MPP(56, "spi1", 4),
	MPP(56, "ua1", 6),
	MPP(57, "gpio", 0),
	MPP(57, "spi1", 4),
	MPP(57, "sd0", 5),
	MPP(57, "ua1", 6),
	MPP(58, "gpio", 0),
	MPP(58, "pcie1", 1),
	MPP(58, "i2c1", 2),
	MPP(58, "pcie2", 3),
	MPP(58, "spi1", 4),
	MPP(58, "sd0", 5),
	MPP(58, "ua1", 6),
	MPP(59, "gpio", 0),
	MPP(59, "pcie0", 1),
	MPP(59, "i2c1", 2),
	MPP(59, "spi1", 4),
	MPP(59, "sd0", 5),
};

const struct mvpinctrl_pin armada_ap806_pins[] = {
	MPP(0, "gpio", 0),
	MPP(0, "sdio", 1),
	MPP(0, "spi0", 3),
	MPP(1, "gpio", 0),
	MPP(1, "sdio", 1),
	MPP(1, "spi0", 3),
	MPP(2, "gpio", 0),
	MPP(2, "sdio", 1),
	MPP(2, "spi0", 3),
	MPP(3, "gpio", 0),
	MPP(3, "sdio", 1),
	MPP(3, "spi0", 3),
	MPP(4, "gpio", 0),
	MPP(4, "sdio", 1),
	MPP(4, "i2c0", 3),
	MPP(5, "gpio", 0),
	MPP(5, "sdio", 1),
	MPP(5, "i2c0", 3),
	MPP(6, "gpio", 0),
	MPP(6, "sdio", 1),
	MPP(7, "gpio", 0),
	MPP(7, "sdio", 1),
	MPP(7, "uart1", 3),
	MPP(8, "gpio", 0),
	MPP(8, "sdio", 1),
	MPP(8, "uart1", 3),
	MPP(9, "gpio", 0),
	MPP(9, "sdio", 1),
	MPP(9, "spi0", 3),
	MPP(10, "gpio", 0),
	MPP(10, "sdio", 1),
	MPP(11, "gpio", 0),
	MPP(11, "uart0", 3),
	MPP(12, "gpio", 0),
	MPP(12, "sdio", 1),
	MPP(12, "sdio", 2),
	MPP(13, "gpio", 0),
	MPP(14, "gpio", 0),
	MPP(15, "gpio", 0),
	MPP(16, "gpio", 0),
	MPP(17, "gpio", 0),
	MPP(18, "gpio", 0),
	MPP(19, "gpio", 0),
	MPP(19, "uart0", 3),
	MPP(19, "sdio", 4),
};

const struct mvpinctrl_pin armada_cp110_pins[] = {
	MPP(0, "gpio", 0),
	MPP(0, "dev", 1),
	MPP(0, "au", 2),
	MPP(0, "ge0", 3),
	MPP(0, "tdm", 4),
	MPP(0, "ptp", 6),
	MPP(0, "mss_i2c", 7),
	MPP(0, "uart0", 8),
	MPP(0, "sata0", 9),
	MPP(0, "ge", 10),
	MPP(1, "gpio", 0),
	MPP(1, "dev", 1),
	MPP(1, "au", 2),
	MPP(1, "ge0", 3),
	MPP(1, "tdm", 4),
	MPP(1, "ptp", 6),
	MPP(1, "mss_i2c", 7),
	MPP(1, "uart0", 8),
	MPP(1, "sata1", 9),
	MPP(1, "ge", 10),
	MPP(2, "gpio", 0),
	MPP(2, "dev", 1),
	MPP(2, "au", 2),
	MPP(2, "ge0", 3),
	MPP(2, "tdm", 4),
	MPP(2, "mss_uart", 5),
	MPP(2, "ptp", 6),
	MPP(2, "i2c1", 7),
	MPP(2, "uart1", 8),
	MPP(2, "sata0", 9),
	MPP(2, "xg", 10),
	MPP(3, "gpio", 0),
	MPP(3, "dev", 1),
	MPP(3, "au", 2),
	MPP(3, "ge0", 3),
	MPP(3, "tdm", 4),
	MPP(3, "mss_uart", 5),
	MPP(3, "pcie", 6),
	MPP(3, "i2c1", 7),
	MPP(3, "uart1", 8),
	MPP(3, "sata1", 9),
	MPP(3, "xg", 10),
	MPP(4, "gpio", 0),
	MPP(4, "dev", 1),
	MPP(4, "au", 2),
	MPP(4, "ge0", 3),
	MPP(4, "tdm", 4),
	MPP(4, "mss_uart", 5),
	MPP(4, "uart1", 6),
	MPP(4, "pcie0", 7),
	MPP(4, "uart3", 8),
	MPP(4, "ge", 10),
	MPP(5, "gpio", 0),
	MPP(5, "dev", 1),
	MPP(5, "au", 2),
	MPP(5, "ge0", 3),
	MPP(5, "tdm", 4),
	MPP(5, "mss_uart", 5),
	MPP(5, "uart1", 6),
	MPP(5, "pcie1", 7),
	MPP(5, "uart3", 8),
	MPP(5, "ge", 10),
	MPP(6, "gpio", 0),
	MPP(6, "dev", 1),
	MPP(6, "ge0", 3),
	MPP(6, "spi0", 4),
	MPP(6, "au", 5),
	MPP(6, "sata1", 6),
	MPP(6, "pcie2", 7),
	MPP(6, "uart0", 8),
	MPP(6, "ptp", 9),
	MPP(7, "gpio", 0),
	MPP(7, "dev", 1),
	MPP(7, "ge0", 3),
	MPP(7, "spi0", 4),
	MPP(7, "spi1", 5),
	MPP(7, "sata0", 6),
	MPP(7, "led", 7),
	MPP(7, "uart0", 8),
	MPP(7, "ptp", 9),
	MPP(8, "gpio", 0),
	MPP(8, "dev", 1),
	MPP(8, "ge0", 3),
	MPP(8, "spi0", 4),
	MPP(8, "spi1", 5),
	MPP(8, "uart0", 6),
	MPP(8, "led", 7),
	MPP(8, "uart2", 8),
	MPP(8, "ptp", 9),
	MPP(8, "synce1", 10),
	MPP(9, "gpio", 0),
	MPP(9, "dev", 1),
	MPP(9, "ge0", 3),
	MPP(9, "spi0", 4),
	MPP(9, "spi1", 5),
	MPP(9, "pcie", 7),
	MPP(9, "synce2", 10),
	MPP(10, "gpio", 0),
	MPP(10, "dev", 1),
	MPP(10, "ge0", 3),
	MPP(10, "spi0", 4),
	MPP(10, "spi1", 5),
	MPP(10, "uart0", 6),
	MPP(10, "sata1", 7),
	MPP(11, "gpio", 0),
	MPP(11, "dev", 1),
	MPP(11, "ge0", 3),
	MPP(11, "spi0", 4),
	MPP(11, "spi1", 5),
	MPP(11, "uart0", 6),
	MPP(11, "led", 7),
	MPP(11, "uart2", 8),
	MPP(11, "sata0", 9),
	MPP(12, "gpio", 0),
	MPP(12, "dev", 1),
	MPP(12, "nf", 2),
	MPP(12, "spi1", 3),
	MPP(12, "ge0", 4),
	MPP(13, "gpio", 0),
	MPP(13, "dev", 1),
	MPP(13, "nf", 2),
	MPP(13, "spi1", 3),
	MPP(13, "ge0", 4),
	MPP(13, "mss_spi", 8),
	MPP(14, "gpio", 0),
	MPP(14, "dev", 1),
	MPP(14, "dev", 2),
	MPP(14, "spi1", 3),
	MPP(14, "spi0", 4),
	MPP(14, "au", 5),
	MPP(14, "spi0", 6),
	MPP(14, "sata0", 7),
	MPP(14, "mss_spi", 8),
	MPP(15, "gpio", 0),
	MPP(15, "dev", 1),
	MPP(15, "spi1", 3),
	MPP(15, "spi0", 6),
	MPP(15, "mss_spi", 8),
	MPP(15, "ptp", 11),
	MPP(16, "gpio", 0),
	MPP(16, "dev", 1),
	MPP(16, "spi1", 3),
	MPP(16, "mss_spi", 8),
	MPP(17, "gpio", 0),
	MPP(17, "dev", 1),
	MPP(17, "ge0", 4),
	MPP(18, "gpio", 0),
	MPP(18, "dev", 1),
	MPP(18, "ge0", 4),
	MPP(18, "ptp", 11),
	MPP(19, "gpio", 0),
	MPP(19, "dev", 1),
	MPP(19, "ge0", 4),
	MPP(19, "wakeup", 11),
	MPP(20, "gpio", 0),
	MPP(20, "dev", 1),
	MPP(20, "ge0", 4),
	MPP(21, "gpio", 0),
	MPP(21, "dev", 1),
	MPP(21, "ge0", 4),
	MPP(21, "sei", 11),
	MPP(22, "gpio", 0),
	MPP(22, "dev", 1),
	MPP(22, "ge0", 4),
	MPP(22, "wakeup", 11),
	MPP(23, "gpio", 0),
	MPP(23, "dev", 1),
	MPP(23, "au", 5),
	MPP(23, "link", 11),
	MPP(24, "gpio", 0),
	MPP(24, "dev", 1),
	MPP(24, "au", 5),
	MPP(25, "gpio", 0),
	MPP(25, "dev", 1),
	MPP(25, "au", 5),
	MPP(26, "gpio", 0),
	MPP(26, "dev", 1),
	MPP(26, "au", 5),
	MPP(27, "gpio", 0),
	MPP(27, "dev", 1),
	MPP(27, "spi1", 2),
	MPP(27, "mss_gpio4", 3),
	MPP(27, "ge0", 4),
	MPP(27, "spi0", 5),
	MPP(27, "ge", 8),
	MPP(27, "sata0", 9),
	MPP(27, "uart0", 10),
	MPP(27, "rei", 11),
	MPP(28, "gpio", 0),
	MPP(28, "dev", 1),
	MPP(28, "spi1", 2),
	MPP(28, "mss_gpio5", 3),
	MPP(28, "ge0", 4),
	MPP(28, "spi0", 5),
	MPP(28, "pcie2", 6),
	MPP(28, "ptp", 7),
	MPP(28, "ge", 8),
	MPP(28, "sata1", 9),
	MPP(28, "uart0", 10),
	MPP(28, "led", 11),
	MPP(29, "gpio", 0),
	MPP(29, "dev", 1),
	MPP(29, "spi1", 2),
	MPP(29, "mss_gpio6", 3),
	MPP(29, "ge0", 4),
	MPP(29, "spi0", 5),
	MPP(29, "pcie1", 6),
	MPP(29, "ptp", 7),
	MPP(29, "mss_i2c", 8),
	MPP(29, "sata0", 9),
	MPP(29, "uart0", 10),
	MPP(29, "led", 11),
	MPP(30, "gpio", 0),
	MPP(30, "dev", 1),
	MPP(30, "spi1", 2),
	MPP(30, "mss_gpio7", 3),
	MPP(30, "ge0", 4),
	MPP(30, "spi0", 5),
	MPP(30, "pcie0", 6),
	MPP(30, "ptp", 7),
	MPP(30, "mss_i2c", 8),
	MPP(30, "sata1", 9),
	MPP(30, "uart0", 10),
	MPP(30, "led", 11),
	MPP(31, "gpio", 0),
	MPP(31, "dev", 1),
	MPP(31, "mss_gpio4", 3),
	MPP(31, "pcie", 6),
	MPP(31, "ge", 8),
	MPP(32, "gpio", 0),
	MPP(32, "mii", 1),
	MPP(32, "mii", 2),
	MPP(32, "mss_spi", 3),
	MPP(32, "tdm", 4),
	MPP(32, "au", 5),
	MPP(32, "au", 6),
	MPP(32, "ge", 7),
	MPP(32, "sdio", 8),
	MPP(32, "pcie1", 9),
	MPP(32, "mss_gpio0", 10),
	MPP(33, "gpio", 0),
	MPP(33, "mii", 1),
	MPP(33, "sdio", 2),
	MPP(33, "mss_spi", 3),
	MPP(33, "tdm", 4),
	MPP(33, "au", 5),
	MPP(33, "sdio", 6),
	MPP(33, "xg", 8),
	MPP(33, "pcie2", 9),
	MPP(33, "mss_gpio1", 10),
	MPP(34, "gpio", 0),
	MPP(34, "mii", 1),
	MPP(34, "sdio", 2),
	MPP(34, "mss_spi", 3),
	MPP(34, "tdm", 4),
	MPP(34, "au", 5),
	MPP(34, "sdio", 6),
	MPP(34, "ge", 7),
	MPP(34, "pcie0", 9),
	MPP(34, "mss_gpio2", 10),
	MPP(35, "gpio", 0),
	MPP(35, "sata1", 1),
	MPP(35, "i2c1", 2),
	MPP(35, "mss_spi", 3),
	MPP(35, "tdm", 4),
	MPP(35, "au", 5),
	MPP(35, "sdio", 6),
	MPP(35, "xg", 7),
	MPP(35, "ge", 8),
	MPP(35, "pcie", 9),
	MPP(35, "mss_gpio3", 10),
	MPP(36, "gpio", 0),
	MPP(36, "synce2", 1),
	MPP(36, "i2c1", 2),
	MPP(36, "ptp", 3),
	MPP(36, "synce1", 4),
	MPP(36, "au", 5),
	MPP(36, "sata0", 6),
	MPP(36, "xg", 7),
	MPP(36, "ge", 8),
	MPP(36, "pcie2", 9),
	MPP(36, "mss_gpio5", 10),
	MPP(37, "gpio", 0),
	MPP(37, "uart2", 1),
	MPP(37, "i2c0", 2),
	MPP(37, "ptp", 3),
	MPP(37, "tdm", 4),
	MPP(37, "mss_i2c", 5),
	MPP(37, "sata1", 6),
	MPP(37, "ge", 7),
	MPP(37, "xg", 8),
	MPP(37, "pcie1", 9),
	MPP(37, "mss_gpio6", 10),
	MPP(37, "link", 11),
	MPP(38, "gpio", 0),
	MPP(38, "uart2", 1),
	MPP(38, "i2c0", 2),
	MPP(38, "ptp", 3),
	MPP(38, "tdm", 4),
	MPP(38, "mss_i2c", 5),
	MPP(38, "sata0", 6),
	MPP(38, "ge", 7),
	MPP(38, "xg", 8),
	MPP(38, "au", 9),
	MPP(38, "mss_gpio7", 10),
	MPP(38, "ptp", 11),
	MPP(39, "gpio", 0),
	MPP(39, "sdio", 1),
	MPP(39, "au", 4),
	MPP(39, "ptp", 5),
	MPP(39, "spi0", 6),
	MPP(39, "sata1", 9),
	MPP(39, "mss_gpio0", 10),
	MPP(40, "gpio", 0),
	MPP(40, "sdio", 1),
	MPP(40, "synce1", 2),
	MPP(40, "mss_i2c", 3),
	MPP(40, "au", 4),
	MPP(40, "ptp", 5),
	MPP(40, "spi0", 6),
	MPP(40, "uart1", 7),
	MPP(40, "ge", 8),
	MPP(40, "sata0", 9),
	MPP(40, "mss_gpio1", 10),
	MPP(41, "gpio", 0),
	MPP(41, "sdio", 1),
	MPP(41, "sdio", 2),
	MPP(41, "mss_i2c", 3),
	MPP(41, "au", 4),
	MPP(41, "ptp", 5),
	MPP(41, "spi0", 6),
	MPP(41, "uart1", 7),
	MPP(41, "ge", 8),
	MPP(41, "sata1", 9),
	MPP(41, "mss_gpio2", 10),
	MPP(41, "rei", 11),
	MPP(42, "gpio", 0),
	MPP(42, "sdio", 1),
	MPP(42, "sdio", 2),
	MPP(42, "synce2", 3),
	MPP(42, "au", 4),
	MPP(42, "mss_uart", 5),
	MPP(42, "spi0", 6),
	MPP(42, "uart1", 7),
	MPP(42, "xg", 8),
	MPP(42, "sata0", 9),
	MPP(42, "mss_gpio4", 10),
	MPP(43, "gpio", 0),
	MPP(43, "sdio", 1),
	MPP(43, "synce1", 3),
	MPP(43, "au", 4),
	MPP(43, "mss_uart", 5),
	MPP(43, "spi0", 6),
	MPP(43, "uart1", 7),
	MPP(43, "xg", 8),
	MPP(43, "sata1", 9),
	MPP(43, "mss_gpio5", 10),
	MPP(43, "wakeup", 11),
	MPP(44, "gpio", 0),
	MPP(44, "ge1", 1),
	MPP(44, "uart0", 7),
	MPP(44, "ptp", 11),
	MPP(45, "gpio", 0),
	MPP(45, "ge1", 1),
	MPP(45, "uart0", 7),
	MPP(45, "pcie", 9),
	MPP(46, "gpio", 0),
	MPP(46, "ge1", 1),
	MPP(46, "uart1", 7),
	MPP(47, "gpio", 0),
	MPP(47, "ge1", 1),
	MPP(47, "spi1", 5),
	MPP(47, "uart1", 7),
	MPP(47, "ge", 8),
	MPP(48, "gpio", 0),
	MPP(48, "ge1", 1),
	MPP(48, "spi1", 5),
	MPP(48, "xg", 8),
	MPP(48, "wakeup", 11),
	MPP(49, "gpio", 0),
	MPP(49, "ge1", 1),
	MPP(49, "mii", 2),
	MPP(49, "spi1", 5),
	MPP(49, "uart1", 7),
	MPP(49, "ge", 8),
	MPP(49, "pcie0", 9),
	MPP(49, "sdio", 10),
	MPP(49, "sei", 11),
	MPP(50, "gpio", 0),
	MPP(50, "ge1", 1),
	MPP(50, "mss_i2c", 2),
	MPP(50, "spi1", 5),
	MPP(50, "uart2", 6),
	MPP(50, "uart0", 7),
	MPP(50, "xg", 8),
	MPP(50, "sdio", 10),
	MPP(51, "gpio", 0),
	MPP(51, "ge1", 1),
	MPP(51, "mss_i2c", 2),
	MPP(51, "spi1", 5),
	MPP(51, "uart2", 6),
	MPP(51, "uart0", 7),
	MPP(51, "sdio", 10),
	MPP(52, "gpio", 0),
	MPP(52, "ge1", 1),
	MPP(52, "synce1", 2),
	MPP(52, "synce2", 4),
	MPP(52, "spi1", 5),
	MPP(52, "uart1", 7),
	MPP(52, "led", 8),
	MPP(52, "pcie", 9),
	MPP(52, "pcie0", 10),
	MPP(53, "gpio", 0),
	MPP(53, "ge1", 1),
	MPP(53, "ptp", 3),
	MPP(53, "spi1", 5),
	MPP(53, "uart1", 7),
	MPP(53, "led", 8),
	MPP(53, "sdio", 11),
	MPP(54, "gpio", 0),
	MPP(54, "ge1", 1),
	MPP(54, "synce2", 2),
	MPP(54, "ptp", 3),
	MPP(54, "synce1", 4),
	MPP(54, "led", 8),
	MPP(54, "sdio", 10),
	MPP(54, "sdio", 11),
	MPP(55, "gpio", 0),
	MPP(55, "ge1", 1),
	MPP(55, "ptp", 3),
	MPP(55, "sdio", 10),
	MPP(55, "sdio", 11),
	MPP(56, "gpio", 0),
	MPP(56, "tdm", 4),
	MPP(56, "au", 5),
	MPP(56, "spi0", 6),
	MPP(56, "uart1", 7),
	MPP(56, "sata1", 9),
	MPP(56, "sdio", 14),
	MPP(57, "gpio", 0),
	MPP(57, "mss_i2c", 2),
	MPP(57, "ptp", 3),
	MPP(57, "tdm", 4),
	MPP(57, "au", 5),
	MPP(57, "spi0", 6),
	MPP(57, "uart1", 7),
	MPP(57, "sata0", 9),
	MPP(57, "sdio", 14),
	MPP(58, "gpio", 0),
	MPP(58, "mss_i2c", 2),
	MPP(58, "ptp", 3),
	MPP(58, "tdm", 4),
	MPP(58, "au", 5),
	MPP(58, "spi0", 6),
	MPP(58, "uart1", 7),
	MPP(58, "led", 8),
	MPP(58, "sdio", 14),
	MPP(59, "gpio", 0),
	MPP(59, "mss_gpio7", 1),
	MPP(59, "synce2", 2),
	MPP(59, "tdm", 4),
	MPP(59, "au", 5),
	MPP(59, "spi0", 6),
	MPP(59, "uart0", 7),
	MPP(59, "led", 8),
	MPP(59, "uart1", 9),
	MPP(59, "sdio", 14),
	MPP(60, "gpio", 0),
	MPP(60, "mss_gpio6", 1),
	MPP(60, "ptp", 3),
	MPP(60, "tdm", 4),
	MPP(60, "au", 5),
	MPP(60, "spi0", 6),
	MPP(60, "uart0", 7),
	MPP(60, "led", 8),
	MPP(60, "uart1", 9),
	MPP(60, "sdio", 14),
	MPP(61, "gpio", 0),
	MPP(61, "mss_gpio5", 1),
	MPP(61, "ptp", 3),
	MPP(61, "tdm", 4),
	MPP(61, "au", 5),
	MPP(61, "spi0", 6),
	MPP(61, "uart0", 7),
	MPP(61, "uart2", 8),
	MPP(61, "sata1", 9),
	MPP(61, "ge", 10),
	MPP(61, "sdio", 14),
	MPP(62, "gpio", 0),
	MPP(62, "mss_gpio4", 1),
	MPP(62, "synce1", 2),
	MPP(62, "ptp", 3),
	MPP(62, "sata1", 5),
	MPP(62, "spi0", 6),
	MPP(62, "uart0", 7),
	MPP(62, "uart2", 8),
	MPP(62, "sata0", 9),
	MPP(62, "ge", 10),
};
