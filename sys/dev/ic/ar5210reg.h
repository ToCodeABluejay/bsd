/*	$OpenBSD: ar5210reg.h,v 1.1 2004/11/02 03:01:16 reyk Exp $	*/

/*
 * Copyright (c) 2004 Reyk Floeter <reyk@vantronix.net>. 
 *
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY
 * SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Known registers of the Atheros AR5000 Wireless LAN chipset
 * (AR5210 + AR5110).
 */

#ifndef _AR5K_AR5210_REG_H
#define _AR5K_AR5210_REG_H

/*
 * First tansmit queue descriptor pointer register ("data queue")
 */
#define AR5K_AR5210_TXDP0	0x0000

/*
 * Second transmit queue descriptor pointer register ("beacon queue")
 */
#define AR5K_AR5210_TXDP1 	0x0004

/*
 * Command register
 */
#define AR5K_AR5210_CR 		0x0008
#define AR5K_AR5210_CR_TXE0 	0x00000001
#define AR5K_AR5210_CR_TXE1 	0x00000002
#define AR5K_AR5210_CR_RXE 	0x00000004
#define AR5K_AR5210_CR_TXD0 	0x00000008
#define AR5K_AR5210_CR_TXD1 	0x00000010
#define AR5K_AR5210_CR_RXD 	0x00000020
#define AR5K_AR5210_CR_SWI 	0x00000040

/*
 * Receive queue descriptor pointer register
 */
#define AR5K_AR5210_RXDP 	0x000c

/*
 * Configuration and status register
 */
#define AR5K_AR5210_CFG 	0x0014
#define AR5K_AR5210_CFG_SWTD 	0x00000001	
#define AR5K_AR5210_CFG_SWTB 	0x00000002	
#define AR5K_AR5210_CFG_SWRD 	0x00000004	
#define AR5K_AR5210_CFG_SWRB 	0x00000008	
#define AR5K_AR5210_CFG_SWRG 	0x00000010	
#define AR5K_AR5210_CFG_EEBS 	0x00000200	
#define AR5K_AR5210_CFG_TXCNT 	0x00007800
#define AR5K_AR5210_CFG_TXCNT_S	11
#define AR5K_AR5210_CFG_TXFSTAT 0x00008000
#define AR5K_AR5210_CFG_TXFSTRT 0x00010000

/*
 * Interrupt service register
 */
#define AR5K_AR5210_ISR		0x001c		
#define AR5K_AR5210_ISR_RXOK	0x00000001
#define AR5K_AR5210_ISR_RXDESC	0x00000002
#define AR5K_AR5210_ISR_RXERR	0x00000004			
#define AR5K_AR5210_ISR_RXNOFRM	0x00000008			
#define AR5K_AR5210_ISR_RXEOL 	0x00000120
#define AR5K_AR5210_ISR_RXORN 	0x00000020			
#define AR5K_AR5210_ISR_TXOK 	0x00000040			
#define AR5K_AR5210_ISR_TXDESC 	0x00000080			
#define AR5K_AR5210_ISR_TXERR 	0x00000100			
#define AR5K_AR5210_ISR_TXNOFRM 0x00000200			
#define AR5K_AR5210_ISR_TXEOL 	0x00000400			
#define AR5K_AR5210_ISR_TXURN 	0x00000800			
#define AR5K_AR5210_ISR_MIB 	0x00001000			
#define AR5K_AR5210_ISR_SWI	0x00002000			
#define AR5K_AR5210_ISR_RXPHY	0x00004000			
#define AR5K_AR5210_ISR_RXKCM	0x00008000			
#define AR5K_AR5210_ISR_SWBA	0x00010000			
#define AR5K_AR5210_ISR_BRSSI	0x00020000			
#define AR5K_AR5210_ISR_BMISS	0x00040000			
#define AR5K_AR5210_ISR_MCABT	0x00100000			
#define AR5K_AR5210_ISR_SSERR	0x00200000			
#define AR5K_AR5210_ISR_DPERR	0x00400000			
#define AR5K_AR5210_ISR_GPIO	0x01000000			
#define	AR5K_AR5210_ISR_FATAL 	(					\
        AR5K_AR5210_ISR_MCABT | AR5K_AR5210_ISR_SSERR |			\
        AR5K_AR5210_ISR_DPERR | AR5K_AR5210_ISR_RXORN			\
)

/*
 * Interrupt mask register
 */
#define AR5K_AR5210_IMR 	0x0020			
#define AR5K_AR5210_IMR_RXOK 	0x00000001			
#define AR5K_AR5210_IMR_RXDESC 	0x00000002			
#define AR5K_AR5210_IMR_RXERR 	0x00000004			
#define AR5K_AR5210_IMR_RXNOFRM	0x00000008			
#define AR5K_AR5210_IMR_RXEOL	0x00000010			
#define AR5K_AR5210_IMR_RXORN 	0x00000020			
#define AR5K_AR5210_IMR_TXOK 	0x00000040			
#define AR5K_AR5210_IMR_TXDESC 	0x00000080			
#define AR5K_AR5210_IMR_TXERR 	0x00000100			
#define AR5K_AR5210_IMR_TXNOFRM 0x00000200			
#define AR5K_AR5210_IMR_TXEOL 	0x00000400			
#define AR5K_AR5210_IMR_TXURN 	0x00000800			
#define AR5K_AR5210_IMR_MIB 	0x00001000			
#define AR5K_AR5210_IMR_SWI 	0x00002000			
#define AR5K_AR5210_IMR_RXPHY 	0x00004000			
#define AR5K_AR5210_IMR_RXKCM 	0x00008000			
#define AR5K_AR5210_IMR_SWBA 	0x00010000			
#define AR5K_AR5210_IMR_BRSSI 	0x00020000			
#define AR5K_AR5210_IMR_BMISS 	0x00040000			
#define AR5K_AR5210_IMR_MCABT 	0x00100000			
#define AR5K_AR5210_IMR_SSERR 	0x00200000			
#define AR5K_AR5210_IMR_DPERR 	0x00400000			
#define AR5K_AR5210_IMR_GPIO 	0x01000000			

/*
 * Interrupt enable register
 */
#define AR5K_AR5210_IER 	0x0024
#define AR5K_AR5210_IER_DISABLE	0x00000000			
#define AR5K_AR5210_IER_ENABLE 	0x00000001			

/*
 * Beacon control register
 */
#define AR5K_AR5210_BCR		0x0028
#define AR5K_AR5210_BCR_AP 	0x00000000
#define AR5K_AR5210_BCR_ADHOC 	0x00000001
#define AR5K_AR5210_BCR_BDMAE 	0x00000002			
#define AR5K_AR5210_BCR_TQ1FV 	0x00000004			
#define AR5K_AR5210_BCR_TQ1V 	0x00000008			
#define AR5K_AR5210_BCR_BCGET 	0x00000010			

/*
 * Beacon status register
 */
#define AR5K_AR5210_BSR 		0x002c			
#define AR5K_AR5210_BSR_BDLYSW 		0x00000001			
#define AR5K_AR5210_BSR_BDLYDMA 	0x00000002			
#define AR5K_AR5210_BSR_TXQ1F 		0x00000004			
#define AR5K_AR5210_BSR_ATIMDLY 	0x00000008			
#define AR5K_AR5210_BSR_SNPBCMD 	0x00000100			
#define AR5K_AR5210_BSR_SNPBDMAE 	0x00000200			
#define AR5K_AR5210_BSR_SNPTQ1FV 	0x00000400			
#define AR5K_AR5210_BSR_SNPTQ1V 	0x00000800			
#define AR5K_AR5210_BSR_SNAPPEDBCRVALID	0x00001000			
#define AR5K_AR5210_BSR_SWBA_CNT	0x00ff0000			

/*
 * DMA size definitions
 */
typedef enum {
	AR5K_AR5210_DMASIZE_4B = 0,
	AR5K_AR5210_DMASIZE_8B,
	AR5K_AR5210_DMASIZE_16B,
	AR5K_AR5210_DMASIZE_32B,
	AR5K_AR5210_DMASIZE_64B,
	AR5K_AR5210_DMASIZE_128B,
	AR5K_AR5210_DMASIZE_256B,
	AR5K_AR5210_DMASIZE_512B
} ar5k_ar5210_dmasize_t;

/*
 * Transmit configuration register
 */
#define AR5K_AR5210_TXCFG		0x0030			
#define AR5K_AR5210_TXCFG_SDMAMR 	0x00000007			
#define AR5K_AR5210_TXCFG_TXFSTP 	0x00000008			
#define AR5K_AR5210_TXCFG_TXFULL 	0x00000070			
#define AR5K_AR5210_TXCFG_TXCONT_EN	0x00000080			

/*
 * Receive configuration register
 */
#define AR5K_AR5210_RXCFG		0x0034			
#define AR5K_AR5210_RXCFG_SDMAMW	0x00000007			
#define AR5K_AR5210_RXCFG_ZLFDMA	0x00000010			

/*
 * MIB control register
 */
#define AR5K_AR5210_MIBC 		0x0040
#define AR5K_AR5210_MIBC_COW 		0x00000001
#define AR5K_AR5210_MIBC_FMC 		0x00000002			
#define AR5K_AR5210_MIBC_CMC		0x00000004			
#define AR5K_AR5210_MIBC_MCS 		0x00000008			

/*
 * Timeout prescale register
 */
#define AR5K_AR5210_TOPS 		0x0044

/*
 * Receive timeout register (no frame received)
 */
#define AR5K_AR5210_RXNOFRM 		0x0048

/*
 * Transmit timeout register (no frame sent)
 */
#define AR5K_AR5210_TXNOFRM 		0x004c

/*
 * Receive frame gap timeout register
 */
#define AR5K_AR5210_RPGTO 		0x0050

/* 
 * Receive frame count limit register 
 */
#define AR5K_AR5210_RFCNT 		0x0054
#define AR5K_AR5210_RFCNT_RFCL 		0x0000000f			

/*
 * Misc settings/status register 
 */
#define AR5K_AR5210_MISC 		0x0058
#define AR5K_AR5210_MISC_LED_DECAY	0x001c0000			
#define AR5K_AR5210_MISC_LED_BLINK	0x00e00000			

/*
 * Reset control register
 */
#define AR5K_AR5210_RC			0x4000
#define AR5K_AR5210_RC_PCU 		0x00000001			
#define AR5K_AR5210_RC_DMA 		0x00000002			
#define AR5K_AR5210_RC_MAC 		0x00000004			
#define AR5K_AR5210_RC_PHY 		0x00000008			
#define AR5K_AR5210_RC_PCI 		0x00000010			
#define AR5K_AR5210_RC_CHIP    		(				\
	AR5K_AR5210_RC_PCU | AR5K_AR5210_RC_DMA | 			\
        AR5K_AR5210_RC_MAC | AR5K_AR5210_RC_PHY				\
)

/*
 * Sleep control register
 */
#define AR5K_AR5210_SCR			0x4004
#define AR5K_AR5210_SCR_SLDUR 		0x0000ffff			
#define AR5K_AR5210_SCR_SLE 		0x00030000			
#define AR5K_AR5210_SCR_SLE_WAKE 	0x00000000			
#define AR5K_AR5210_SCR_SLE_SLP 	0x00010000			
#define AR5K_AR5210_SCR_SLE_ALLOW 	0x00020000			

/*
 * Interrupt pending register
 */
#define AR5K_AR5210_INTPEND 		0x4008
#define AR5K_AR5210_INTPEND_IP 		0x00000001

/*
 * Sleep force register
 */
#define AR5K_AR5210_SFR 		0x400c
#define AR5K_AR5210_SFR_SF 		0x00000001			

/*
 * PCI configuration register
 */
#define AR5K_AR5210_PCICFG 		0x4010
#define AR5K_AR5210_PCICFG_EEAE 	0x00000001			
#define AR5K_AR5210_PCICFG_CLKRUNEN 	0x00000004			
#define AR5K_AR5210_PCICFG_LED_PEND 	0x00000020			
#define AR5K_AR5210_PCICFG_LED_ACT 	0x00000040			
#define AR5K_AR5210_PCICFG_SL_INTEN 	0x00000800			
#define AR5K_AR5210_PCICFG_LED_BCTL 	0x00001000			
#define AR5K_AR5210_PCICFG_SL_INPEN 	0x00002800			
#define AR5K_AR5210_PCICFG_SPWR_DN 	0x00010000			

/*
 * "General Purpose Input/Output" (GPIO) control register
 */
#define AR5K_AR5210_GPIOCR 		0x4014
#define AR5K_AR5210_GPIOCR_INT_ENA 	0x00008000			
#define AR5K_AR5210_GPIOCR_INT_SELL 	0x00000000			
#define AR5K_AR5210_GPIOCR_INT_SELH 	0x00010000			
#define AR5K_AR5210_GPIOCR_IN(n) 	(0 << ((n) * 2))			
#define AR5K_AR5210_GPIOCR_OUT0(n) 	(1 << ((n) * 2))			
#define AR5K_AR5210_GPIOCR_OUT1(n) 	(2 << ((n) * 2))			
#define AR5K_AR5210_GPIOCR_OUT(n) 	(3 << ((n) * 2))			
#define AR5K_AR5210_GPIOCR_ALL(n) 	(3<< ((n) * 2))			
#define AR5K_AR5210_GPIOCR_INT_SEL(n) 	((n) << 12)			

#define AR5K_AR5210_NUM_GPIO 	6

/*
 * "General Purpose Input/Output" (GPIO) data output register
 */
#define AR5K_AR5210_GPIODO 	0x4018

/*
 * "General Purpose Input/Output" (GPIO) data input register
 */
#define AR5K_AR5210_GPIODI 	0x401c
#define AR5K_AR5210_GPIOD_MASK 	0x0000002f

/*
 * Silicon revision register
 */
#define AR5K_AR5210_SREV 		0x4020
#define AR5K_AR5210_SREV_ID_M 		0x000000ff
#define AR5K_AR5210_SREV_FPGA 		1
#define AR5K_AR5210_SREV_PHYPLUS 	2
#define AR5K_AR5210_SREV_PHYPLUS_MS 	3 
#define AR5K_AR5210_SREV_CRETE 		4
#define AR5K_AR5210_SREV_CRETE_MS 	5 
#define AR5K_AR5210_SREV_CRETE_MS23 	7 
#define AR5K_AR5210_SREV_CRETE_23 	8 

/*
 * EEPROM access registers
 */
#define AR5K_AR5210_EEPROM_BASE 	0x6000 
#define AR5K_AR5210_EEPROM_RDATA 	0x6800 
#define AR5K_AR5210_EEPROM_STATUS 	0x6c00 
#define AR5K_AR5210_EEPROM_STAT_RDERR 	0x0001 
#define AR5K_AR5210_EEPROM_STAT_RDDONE 	0x0002 
#define AR5K_AR5210_EEPROM_STAT_WRERR 	0x0004 
#define AR5K_AR5210_EEPROM_STAT_WRDONE 	0x0008 

/*
 * AR5210 EEPROM data registers
 */
#define	AR5K_AR5210_EEPROM_MAGIC		0x3d
#define	AR5K_AR5210_EEPROM_MAGIC_VALUE 		0x5aa5
#define AR5K_AR5210_EEPROM_PROTECT		0x3f
#define	AR5K_AR5210_EEPROM_PROTECT_128_191 	0x80
#define AR5K_AR5210_EEPROM_REG_DOMAIN 		0xbf
#define AR5K_AR5210_EEPROM_INFO_BASE 		0xc0
#define AR5K_AR5210_EEPROM_INFO_VERSION 				\
        (AR5K_AR5210_EEPROM_INFO_BASE + 1)
#define	AR5K_AR5210_EEPROM_INFO_MAX 					\
        (0x400 - AR5K_AR5210_EEPROM_INFO_BASE)

/*
 * PCU registers
 */

#define AR5K_AR5210_PCU_MIN 	0x8000
#define AR5K_AR5210_PCU_MAX 	0x8fff

/*
 * First station id register (MAC address in lower 32 bits)
 */
#define AR5K_AR5210_STA_ID0 	0x8000

/*
 * Second station id register (MAC address in upper 16 bits)
 */
#define AR5K_AR5210_STA_ID1 			0x8004
#define AR5K_AR5210_STA_ID1_AP 			0x00010000			
#define AR5K_AR5210_STA_ID1_ADHOC 		0x00020000			
#define AR5K_AR5210_STA_ID1_PWR_SV 		0x00040000			
#define AR5K_AR5210_STA_ID1_NO_KEYSRCH 		0x00080000			
#define AR5K_AR5210_STA_ID1_NO_PSPOLL 		0x00100000			
#define AR5K_AR5210_STA_ID1_PCF 		0x00200000			
#define AR5K_AR5210_STA_ID1_DESC_ANTENNA 	0x00400000			
#define AR5K_AR5210_STA_ID1_DEFAULT_ANTENNA	0x00800000			
#define AR5K_AR5210_STA_ID1_ACKCTS_6MB 		0x01000000			

/*
 * First BSSID register (MAC address, lower 32bits)
 */
#define AR5K_AR5210_BSS_ID0	0x8008

/*
 * Second BSSID register (MAC address in upper 16 bits)
 *
 * AID: Association ID
 */
#define AR5K_AR5210_BSS_ID1 		0x800c
#define AR5K_AR5210_BSS_ID1_AID 	0xffff0000			
#define AR5K_AR5210_BSS_ID1_AID_S 	16

/*
 * Backoff slot time register
 */
#define AR5K_AR5210_SLOT_TIME 	0x8010

/*
 * ACK/CTS timeout register
 */
#define AR5K_AR5210_TIME_OUT 		0x8014
#define AR5K_AR5210_TIME_OUT_ACK 	0x00001fff			
#define AR5K_AR5210_TIME_OUT_ACK_S	0
#define AR5K_AR5210_TIME_OUT_CTS 	0x1fff0000			
#define AR5K_AR5210_TIME_OUT_CTS_S 	16

/*
 * RSSI threshold register
 */
#define AR5K_AR5210_RSSI_THR 		0x8018
#define AR5K_AR5210_RSSI_THR_BM_THR 	0x00000700			
#define AR5K_AR5210_RSSI_THR_BM_THR_S	8

/*
 * Retry limit register
 */
#define AR5K_AR5210_RETRY_LMT 			0x801c
#define AR5K_AR5210_RETRY_LMT_SH_RETRY 		0x0000000f			
#define AR5K_AR5210_RETRY_LMT_SH_RETRY_S 	0
#define AR5K_AR5210_RETRY_LMT_LG_RETRY 		0x000000f0			
#define AR5K_AR5210_RETRY_LMT_LG_RETRY_S 	4
#define AR5K_AR5210_RETRY_LMT_SSH_RETRY 	0x00003f00			
#define AR5K_AR5210_RETRY_LMT_SSH_RETRY_S	8
#define AR5K_AR5210_RETRY_LMT_SLG_RETRY 	0x000fc000			
#define AR5K_AR5210_RETRY_LMT_SLG_RETRY_S	14
#define AR5K_AR5210_RETRY_LMT_CW_MIN 		0x3ff00000
#define AR5K_AR5210_RETRY_LMT_CW_MIN_S 		20

/*
 * Transmit latency register
 */
#define AR5K_AR5210_USEC 		0x8020
#define AR5K_AR5210_USEC_1 		0x0000007f			
#define AR5K_AR5210_USEC_1_S 		0
#define AR5K_AR5210_USEC_32 		0x00003f80			
#define AR5K_AR5210_USEC_32_S 		7
#define AR5K_AR5210_USEC_TX_LATENCY 	0x000fc000			
#define AR5K_AR5210_USEC_TX_LATENCY_S 	14
#define AR5K_AR5210_USEC_RX_LATENCY 	0x03f00000			
#define AR5K_AR5210_USEC_RX_LATENCY_S 	20

/*
 * PCU beacon control register
 */
#define AR5K_AR5210_BEACON 		0x8024
#define AR5K_AR5210_BEACON_PERIOD 	0x0000ffff			
#define AR5K_AR5210_BEACON_PERIOD_S 	0
#define AR5K_AR5210_BEACON_TIM		0x007f0000			
#define AR5K_AR5210_BEACON_TIM_S	16
#define AR5K_AR5210_BEACON_EN		0x00800000			
#define AR5K_AR5210_BEACON_RESET_TSF	0x01000000			

/*
 * CFP period register
 */
#define AR5K_AR5210_CFP_PERIOD		0x8028			

/*
 * Next beacon time register
 */
#define AR5K_AR5210_TIMER0 		0x802c			

/*
 * Next DMA beacon alert register
 */
#define AR5K_AR5210_TIMER1 		0x8030			

/*
 * Next software beacon alert register
 */
#define AR5K_AR5210_TIMER2 		0x8034			

/*
 * Next ATIM window time register
 */
#define AR5K_AR5210_TIMER3 		0x8038			

/*
 * First inter frame spacing register (IFS)
 */
#define AR5K_AR5210_IFS0 		0x8040
#define AR5K_AR5210_IFS0_SIFS 		0x000007ff			
#define AR5K_AR5210_IFS0_SIFS_S 	0
#define AR5K_AR5210_IFS0_DIFS 		0x007ff800			
#define AR5K_AR5210_IFS0_DIFS_S 	11

/*
 * Second inter frame spacing register (IFS)
 */
#define AR5K_AR5210_IFS1 		0x8044
#define AR5K_AR5210_IFS1_PIFS 		0x00000fff			
#define AR5K_AR5210_IFS1_PIFS_S 	0
#define AR5K_AR5210_IFS1_EIFS 		0x03fff000			
#define AR5K_AR5210_IFS1_EIFS_S 	12
#define AR5K_AR5210_IFS1_CS_EN 		0x04000000			

/*
 * CFP duration register
 */
#define AR5K_AR5210_CFP_DUR 		0x8048			

/*
 * Receive filter register
 */
#define AR5K_AR5210_RX_FILTER 		0x804c			
#define AR5K_AR5210_RX_FILTER_UNICAST 	0x00000001			
#define AR5K_AR5210_RX_FILTER_MULTICAST	0x00000002			
#define AR5K_AR5210_RX_FILTER_BROADCAST	0x00000004			
#define AR5K_AR5210_RX_FILTER_CONTROL 	0x00000008			
#define AR5K_AR5210_RX_FILTER_BEACON 	0x00000010			
#define AR5K_AR5210_RX_FILTER_PROMISC	0x00000020			

/*
 * Multicast filter register (lower 32 bits)
 */
#define AR5K_AR5210_MCAST_FIL0	0x8050

/*
 * Multicast filter register (higher 16 bits)
 */
#define AR5K_AR5210_MCAST_FIL1	0x8054

/*
 * Transmit mask register (lower 32 bits)
 */
#define AR5K_AR5210_TX_MASK0	0x8058			

/*
 * Transmit mask register (higher 16 bits)
 */
#define AR5K_AR5210_TX_MASK1	0x805c

/*
 * Clear transmit mask
 */
#define AR5K_AR5210_CLR_TMASK	0x8060

/*
 * Trigger level register (before transmission)
 */
#define AR5K_AR5210_TRIG_LVL	0x8064

/*
 * PCU control register
 */
#define AR5K_AR5210_DIAG_SW 			0x8068
#define AR5K_AR5210_DIAG_SW_DIS_WEP_ACK 	0x00000001			
#define AR5K_AR5210_DIAG_SW_DIS_ACK 		0x00000002			
#define AR5K_AR5210_DIAG_SW_DIS_CTS 		0x00000004			
#define AR5K_AR5210_DIAG_SW_DIS_ENC 		0x00000008			
#define AR5K_AR5210_DIAG_SW_DIS_DEC 		0x00000010			
#define AR5K_AR5210_DIAG_SW_DIS_TX 		0x00000020			
#define AR5K_AR5210_DIAG_SW_DIS_RX 		0x00000040			
#define AR5K_AR5210_DIAG_SW_LOOP_BACK 		0x00000080			
#define AR5K_AR5210_DIAG_SW_CORR_FCS 		0x00000100			
#define AR5K_AR5210_DIAG_SW_CHAN_INFO 		0x00000200			
#define AR5K_AR5210_DIAG_SW_EN_SCRAM_SEED 	0x00000400			
#define AR5K_AR5210_DIAG_SW_SCVRAM_SEED 	0x0003f800			
#define AR5K_AR5210_DIAG_SW_DIS_SEQ_INC 	0x00040000			
#define AR5K_AR5210_DIAG_SW_FRAME_NV0 		0x00080000			

/*
 * TSF (clock) register (lower 32 bits)
 */
#define AR5K_AR5210_TSF_L32	0x806c

/*
 * TSF (clock) register (higher 32 bits)
 */
#define AR5K_AR5210_TSF_U32 	0x8070

/*
 * Last beacon timestamp register
 */
#define AR5K_AR5210_LAST_TSTP 	0x8080

/*
 * Retry count register
 */
#define AR5K_AR5210_RETRY_CNT 		0x8084
#define AR5K_AR5210_RETRY_CNT_SSH 	0x0000003f			
#define AR5K_AR5210_RETRY_CNT_SLG 	0x00000fc0			

/*
 * Back-off status register
 */
#define AR5K_AR5210_BACKOFF 		0x8088
#define AR5K_AR5210_BACKOFF_CW 		0x000003ff			
#define AR5K_AR5210_BACKOFF_CNT 	0x03ff0000			

/*
 * NAV register (current)
 */
#define AR5K_AR5210_NAV 	0x808c

/*
 * RTS success register
 */
#define AR5K_AR5210_RTS_OK	0x8090

/*
 * RTS failure register
 */
#define AR5K_AR5210_RTS_FAIL	0x8094

/*
 * ACK failure register
 */
#define AR5K_AR5210_ACK_FAIL	0x8098

/*
 * FCS failure register
 */
#define AR5K_AR5210_FCS_FAIL	0x809c

/*
 * Beacon count register
 */
#define AR5K_AR5210_BEACON_CNT	0x80a0

/*
 * Key table (WEP) register
 */
#define AR5K_AR5210_KEYTABLE_0		0x9000
#define AR5K_AR5210_KEYTABLE(n) 	(AR5K_AR5210_KEYTABLE_0 + ((n) * 32))
#define AR5K_AR5210_KEYTABLE_TYPE_40 	0x00000000
#define AR5K_AR5210_KEYTABLE_TYPE_104 	0x00000001
#define AR5K_AR5210_KEYTABLE_TYPE_128 	0x00000003
#define AR5K_AR5210_KEYTABLE_VALID 	0x00008000

#define AR5K_AR5210_KEYTABLE_SIZE 	64
#define AR5K_AR5210_KEYCACHE_SIZE 	8

/* 
 * PHY register
 */
#define	AR5K_AR5210_PHY(_n)	(0x9800 + ((_n) << 2))

/*
 * PHY frame control register
 */
#define	AR5K_AR5210_PHY_FC 		0x9804
#define	AR5K_AR5210_PHY_FC_TURBO_MODE 	0x00000001
#define	AR5K_AR5210_PHY_FC_TURBO_SHORT 	0x00000002
#define	AR5K_AR5210_PHY_FC_TIMING_ERR 	0x01000000
#define	AR5K_AR5210_PHY_FC_PARITY_ERR 	0x02000000
#define	AR5K_AR5210_PHY_FC_ILLRATE_ERR 	0x04000000
#define	AR5K_AR5210_PHY_FC_ILLLEN_ERR 	0x08000000
#define	AR5K_AR5210_PHY_FC_SERVICE_ERR 	0x20000000
#define	AR5K_AR5210_PHY_FC_TXURN_ERR 	0x40000000

/*
 * PHY agility command register
 */
#define	AR5K_AR5210_PHY_AGC 		0x9808
#define	AR5K_AR5210_PHY_AGC_DISABLE 	0x08000000

/*
 * PHY chip revision register
 */
#define	AR5K_AR5210_PHY_CHIP_ID 	0x9818

/*
 * PHY activation register
 */
#define	AR5K_AR5210_PHY_ACTIVE		0x981c
#define	AR5K_AR5210_PHY_ENABLE 		0x00000001
#define	AR5K_AR5210_PHY_DISABLE 	0x00000002

/*
 * PHY agility control register
 */
#define	AR5K_AR5210_PHY_AGCCTL		0x9860
#define	AR5K_AR5210_PHY_AGC_CAL		0x00000001
#define	AR5K_AR5210_PHY_AGC_NF		0x00000002

/*
 * Misc PHY/radio registers
 */
#define AR5K_AR5210_BB_GAIN(_n)		(0x9b00 + ((_n) << 2))
#define AR5K_AR5210_RF_GAIN(_n)		(0x9a00 + ((_n) << 2))

#endif
