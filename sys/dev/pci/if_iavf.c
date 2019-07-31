/*	$OpenBSD: if_iavf.c,v 1.4 2019/07/31 01:27:34 jmatthew Exp $	*/

/*
 * Copyright (c) 2013-2015, Intel Corporation
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2016,2017 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2019 Jonathan Matthew <jmatthew@openbsd.org>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/timeout.h>
#include <sys/task.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define I40E_MASK(mask, shift)		((mask) << (shift))
#define I40E_AQ_LARGE_BUF		512

#define IAVF_VFR_INPROGRESS		0
#define IAVF_VFR_COMPLETED		1
#define IAVF_VFR_VFACTIVE		2

#include <dev/pci/if_ixlreg.h>

struct iavf_aq_desc {
	uint16_t	iaq_flags;
#define	IAVF_AQ_DD		(1U << 0)
#define	IAVF_AQ_CMP		(1U << 1)
#define IAVF_AQ_ERR		(1U << 2)
#define IAVF_AQ_VFE		(1U << 3)
#define IAVF_AQ_LB		(1U << 9)
#define IAVF_AQ_RD		(1U << 10)
#define IAVF_AQ_VFC		(1U << 11)
#define IAVF_AQ_BUF		(1U << 12)
#define IAVF_AQ_SI		(1U << 13)
#define IAVF_AQ_EI		(1U << 14)
#define IAVF_AQ_FE		(1U << 15)

#define IAVF_AQ_FLAGS_FMT	"\020" "\020FE" "\017EI" "\016SI" "\015BUF" \
				    "\014VFC" "\013DB" "\012LB" "\004VFE" \
				    "\003ERR" "\002CMP" "\001DD"

	uint16_t	iaq_opcode;

	uint16_t	iaq_datalen;
	uint16_t	iaq_retval;

	uint32_t	iaq_vc_opcode;
	uint32_t	iaq_vc_retval;

	uint32_t	iaq_param[4];
/*	iaq_vfid	iaq_param[0] */
/*	iaq_data_hi	iaq_param[2] */
/*	iaq_data_lo	iaq_param[3] */
} __packed __aligned(8);

/* aq commands */
#define IAVF_AQ_OP_SEND_TO_PF		0x0801
#define IAVF_AQ_OP_MSG_FROM_PF		0x0802
#define IAVF_AQ_OP_SHUTDOWN		0x0803

/* virt channel messages */
#define IAVF_VC_OP_VERSION		1
#define IAVF_VC_OP_RESET_VF		2
#define IAVF_VC_OP_GET_VF_RESOURCES	3
#define IAVF_VC_OP_CONFIG_TX_QUEUE	4
#define IAVF_VC_OP_CONFIG_RX_QUEUE	5
#define IAVF_VC_OP_CONFIG_VSI_QUEUES	6
#define IAVF_VC_OP_CONFIG_IRQ_MAP	7
#define IAVF_VC_OP_ENABLE_QUEUES	8
#define IAVF_VC_OP_DISABLE_QUEUES	9
#define IAVF_VC_OP_ADD_ETH_ADDR		10
#define IAVF_VC_OP_DEL_ETH_ADDR		11
#define IAVF_VC_OP_ADD_VLAN		12
#define IAVF_VC_OP_DEL_VLAN		13
#define IAVF_VC_OP_CONFIG_PROMISC	14
#define IAVF_VC_OP_GET_STATS		15
#define IAVF_VC_OP_EVENT		17
#define IAVF_VC_OP_GET_RSS_HENA_CAPS	25
#define IAVF_VC_OP_SET_RSS_HENA		26

/* virt channel response codes */
#define IAVF_VC_RC_SUCCESS		0
#define IAVF_VC_RC_ERR_PARAM		-5
#define IAVF_VC_RC_ERR_OPCODE		-38
#define IAVF_VC_RC_ERR_CQP_COMPL	-39
#define IAVF_VC_RC_ERR_VF_ID		-40
#define IAVF_VC_RC_ERR_NOT_SUP		-64

/* virt channel events */
#define IAVF_VC_EVENT_LINK_CHANGE	1
#define IAVF_VC_EVENT_RESET_IMPENDING	2
#define IAVF_VC_EVENT_PF_DRIVER_CLOSE	3

/* virt channel offloads */
#define IAVF_VC_OFFLOAD_L2		0x00000001
#define IAVF_VC_OFFLOAD_IWARP		0x00000002
#define IAVF_VC_OFFLOAD_RSVD		0x00000004
#define IAVF_VC_OFFLOAD_RSS_AQ		0x00000008
#define IAVF_VC_OFFLOAD_RSS_REG		0x00000010
#define IAVF_VC_OFFLOAD_WB_ON_ITR	0x00000020
#define IAVF_VC_OFFLOAD_VLAN		0x00010000
#define IAVF_VC_OFFLOAD_RX_POLLING	0x00020000
#define IAVF_VC_OFFLOAD_RSS_PCTYPE_V2	0x00040000
#define IAVF_VC_OFFLOAD_RSS_PF		0x00080000
#define IAVF_VC_OFFLOAD_ENCAP		0x00100000
#define IAVF_VC_OFFLOAD_ENCAP_CSUM	0x00200000
#define IAVF_VC_OFFLOAD_RX_ENCAP_CSUM	0x00400000

/* link speeds */
#define IAVF_VC_LINK_SPEED_100MB	0x1
#define IAVC_VC_LINK_SPEED_1000MB	0x2
#define IAVC_VC_LINK_SPEED_10GB		0x3
#define IAVC_VC_LINK_SPEED_40GB		0x4
#define IAVC_VC_LINK_SPEED_20GB		0x5
#define IAVC_VC_LINK_SPEED_25GB		0x6

struct iavf_link_speed {
	uint64_t	baudrate;
	uint64_t	media;
};

static const struct iavf_link_speed iavf_link_speeds[] = {
	{ 0, 0 },
	{ IF_Mbps(100), IFM_100_TX },
	{ IF_Mbps(1000), IFM_1000_T },
	{ IF_Gbps(10), IFM_10G_T },
	{ IF_Gbps(40), IFM_40G_CR4 },
	{ IF_Gbps(20), IFM_20G_KR2 },
	{ IF_Gbps(25), IFM_25G_CR }
};


struct iavf_vc_version_info {
	uint32_t	major;
	uint32_t	minor;
} __packed;

struct iavf_vc_txq_info {
	uint16_t	vsi_id;
	uint16_t	queue_id;
	uint16_t	ring_len;
	uint16_t	headwb_ena;		/* deprecated */
	uint64_t	dma_ring_addr;
	uint64_t	dma_headwb_addr;	/* deprecated */
} __packed;

struct iavf_vc_rxq_info {
	uint16_t	vsi_id;
	uint16_t	queue_id;
	uint32_t	ring_len;
	uint16_t	hdr_size;
	uint16_t	splithdr_ena;
	uint32_t	databuf_size;
	uint32_t	max_pkt_size;
	uint32_t	pad1;
	uint64_t	dma_ring_addr;
	uint32_t	rx_split_pos;
	uint32_t	pad2;
} __packed;

struct iavf_vc_queue_pair_info {
	struct iavf_vc_txq_info	txq;
	struct iavf_vc_rxq_info	rxq;
} __packed;

struct iavf_vc_queue_config_info {
	uint16_t	vsi_id;
	uint16_t	num_queue_pairs;
	uint32_t	pad;
	struct iavf_vc_queue_pair_info qpair[1];
} __packed;

struct iavf_vc_vector_map {
	uint16_t	vsi_id;
	uint16_t	vector_id;
	uint16_t	rxq_map;
	uint16_t	txq_map;
	uint16_t	rxitr_idx;
	uint16_t	txitr_idx;
} __packed;

struct iavf_vc_irq_map_info {
	uint16_t	num_vectors;
	struct iavf_vc_vector_map vecmap[1];
} __packed;

struct iavf_vc_queue_select {
	uint16_t	vsi_id;
	uint16_t	pad;
	uint32_t	rx_queues;
	uint32_t	tx_queues;
} __packed;

struct iavf_vc_vsi_resource {
	uint16_t	vsi_id;
	uint16_t	num_queue_pairs;
	uint32_t	vsi_type;
	uint16_t	qset_handle;
	uint8_t		default_mac[ETHER_ADDR_LEN];
} __packed;

struct iavf_vc_vf_resource {
	uint16_t	num_vsis;
	uint16_t	num_qp;
	uint16_t	max_vectors;
	uint16_t	max_mtu;
	uint32_t	offload_flags;
	uint32_t	rss_key_size;
	uint32_t	rss_lut_size;
	struct iavf_vc_vsi_resource vsi_res[1];
} __packed;

struct iavf_vc_eth_addr {
	uint8_t		addr[ETHER_ADDR_LEN];
	uint8_t		pad[2];
} __packed;

struct iavf_vc_eth_addr_list {
	uint16_t	vsi_id;
	uint16_t	num_elements;
	struct iavf_vc_eth_addr list[1];
} __packed;

struct iavf_vc_vlan_list {
	uint16_t	vsi_id;
	uint16_t	num_elements;
	uint16_t	vlan_id[1];
} __packed;

struct iavf_vc_promisc_info {
	uint16_t	vsi_id;
	uint16_t	flags;
#define IAVF_FLAG_VF_UNICAST_PROMISC	0x0001
#define IAVF_FLAG_VF_MULTICAST_PROMISC	0x0002
} __packed;

struct iavf_vc_pf_event {
	uint32_t	event;
	uint32_t	link_speed;
	uint8_t		link_status;
	uint8_t		pad[3];
	uint32_t	severity;
} __packed;

/* aq response codes */
#define IAVF_AQ_RC_OK			0  /* success */
#define IAVF_AQ_RC_EPERM		1  /* Operation not permitted */
#define IAVF_AQ_RC_ENOENT		2  /* No such element */
#define IAVF_AQ_RC_ESRCH		3  /* Bad opcode */
#define IAVF_AQ_RC_EINTR		4  /* operation interrupted */
#define IAVF_AQ_RC_EIO			5  /* I/O error */
#define IAVF_AQ_RC_ENXIO		6  /* No such resource */
#define IAVF_AQ_RC_E2BIG		7  /* Arg too long */
#define IAVF_AQ_RC_EAGAIN		8  /* Try again */
#define IAVF_AQ_RC_ENOMEM		9  /* Out of memory */
#define IAVF_AQ_RC_EACCES		10 /* Permission denied */
#define IAVF_AQ_RC_EFAULT		11 /* Bad address */
#define IAVF_AQ_RC_EBUSY		12 /* Device or resource busy */
#define IAVF_AQ_RC_EEXIST		13 /* object already exists */
#define IAVF_AQ_RC_EINVAL		14 /* invalid argument */
#define IAVF_AQ_RC_ENOTTY		15 /* not a typewriter */
#define IAVF_AQ_RC_ENOSPC		16 /* No space or alloc failure */
#define IAVF_AQ_RC_ENOSYS		17 /* function not implemented */
#define IAVF_AQ_RC_ERANGE		18 /* parameter out of range */
#define IAVF_AQ_RC_EFLUSHED		19 /* cmd flushed due to prev error */
#define IAVF_AQ_RC_BAD_ADDR		20 /* contains a bad pointer */
#define IAVF_AQ_RC_EMODE		21 /* not allowed in current mode */
#define IAVF_AQ_RC_EFBIG		22 /* file too large */

struct iavf_tx_desc {
	uint64_t		addr;
	uint64_t		cmd;
#define IAVF_TX_DESC_DTYPE_SHIFT		0
#define IAVF_TX_DESC_DTYPE_MASK		(0xfULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_DATA		(0x0ULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_NOP		(0x1ULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_CONTEXT	(0x1ULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_FCOE_CTX	(0x2ULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_FD		(0x8ULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_DDP_CTX	(0x9ULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_FLEX_DATA	(0xbULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_FLEX_CTX_1	(0xcULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_FLEX_CTX_2	(0xdULL << IAVF_TX_DESC_DTYPE_SHIFT)
#define IAVF_TX_DESC_DTYPE_DONE		(0xfULL << IAVF_TX_DESC_DTYPE_SHIFT)

#define IAVF_TX_DESC_CMD_SHIFT		4
#define IAVF_TX_DESC_CMD_MASK		(0x3ffULL << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_EOP		(0x001 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_RS		(0x002 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_ICRC		(0x004 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_IL2TAG1	(0x008 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_DUMMY		(0x010 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_IIPT_MASK	(0x060 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_IIPT_NONIP	(0x000 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_IIPT_IPV6	(0x020 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_IIPT_IPV4	(0x040 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_IIPT_IPV4_CSUM	(0x060 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_FCOET		(0x080 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_L4T_EOFT_MASK	(0x300 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_L4T_EOFT_UNK	(0x000 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_L4T_EOFT_TCP	(0x100 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_L4T_EOFT_SCTP	(0x200 << IAVF_TX_DESC_CMD_SHIFT)
#define IAVF_TX_DESC_CMD_L4T_EOFT_UDP	(0x300 << IAVF_TX_DESC_CMD_SHIFT)

#define IAVF_TX_DESC_MACLEN_SHIFT	16
#define IAVF_TX_DESC_MACLEN_MASK	(0x7fULL << IAVF_TX_DESC_MACLEN_SHIFT)
#define IAVF_TX_DESC_IPLEN_SHIFT	23
#define IAVF_TX_DESC_IPLEN_MASK		(0x7fULL << IAVF_TX_DESC_IPLEN_SHIFT)
#define IAVF_TX_DESC_L4LEN_SHIFT	30
#define IAVF_TX_DESC_L4LEN_MASK		(0xfULL << IAVF_TX_DESC_L4LEN_SHIFT)
#define IAVF_TX_DESC_FCLEN_SHIFT	30
#define IAVF_TX_DESC_FCLEN_MASK		(0xfULL << IAVF_TX_DESC_FCLEN_SHIFT)

#define IAVF_TX_DESC_BSIZE_SHIFT	34
#define IAVF_TX_DESC_BSIZE_MAX		0x3fffULL
#define IAVF_TX_DESC_BSIZE_MASK		\
	(IAVF_TX_DESC_BSIZE_MAX << IAVF_TX_DESC_BSIZE_SHIFT)
} __packed __aligned(16);

struct iavf_rx_rd_desc_16 {
	uint64_t		paddr; /* packet addr */
	uint64_t		haddr; /* header addr */
} __packed __aligned(16);

struct iavf_rx_rd_desc_32 {
	uint64_t		paddr; /* packet addr */
	uint64_t		haddr; /* header addr */
	uint64_t		_reserved1;
	uint64_t		_reserved2;
} __packed __aligned(16);

struct iavf_rx_wb_desc_16 {
	uint64_t		qword0;
	uint64_t		qword1;
#define IAVF_RX_DESC_DD			(1 << 0)
#define IAVF_RX_DESC_EOP		(1 << 1)
#define IAVF_RX_DESC_L2TAG1P		(1 << 2)
#define IAVF_RX_DESC_L3L4P		(1 << 3)
#define IAVF_RX_DESC_CRCP		(1 << 4)
#define IAVF_RX_DESC_TSYNINDX_SHIFT	5	/* TSYNINDX */
#define IAVF_RX_DESC_TSYNINDX_MASK	(7 << IAVF_RX_DESC_TSYNINDX_SHIFT)
#define IAVF_RX_DESC_UMB_SHIFT		9
#define IAVF_RX_DESC_UMB_MASK		(0x3 << IAVF_RX_DESC_UMB_SHIFT)
#define IAVF_RX_DESC_UMB_UCAST		(0x0 << IAVF_RX_DESC_UMB_SHIFT)
#define IAVF_RX_DESC_UMB_MCAST		(0x1 << IAVF_RX_DESC_UMB_SHIFT)
#define IAVF_RX_DESC_UMB_BCAST		(0x2 << IAVF_RX_DESC_UMB_SHIFT)
#define IAVF_RX_DESC_UMB_MIRROR		(0x3 << IAVF_RX_DESC_UMB_SHIFT)
#define IAVF_RX_DESC_FLM		(1 << 11)
#define IAVF_RX_DESC_FLTSTAT_SHIFT 	12
#define IAVF_RX_DESC_FLTSTAT_MASK 	(0x3 << IAVF_RX_DESC_FLTSTAT_SHIFT)
#define IAVF_RX_DESC_FLTSTAT_NODATA 	(0x0 << IAVF_RX_DESC_FLTSTAT_SHIFT)
#define IAVF_RX_DESC_FLTSTAT_FDFILTID 	(0x1 << IAVF_RX_DESC_FLTSTAT_SHIFT)
#define IAVF_RX_DESC_FLTSTAT_RSS 	(0x3 << IAVF_RX_DESC_FLTSTAT_SHIFT)
#define IAVF_RX_DESC_LPBK		(1 << 14)
#define IAVF_RX_DESC_IPV6EXTADD		(1 << 15)
#define IAVF_RX_DESC_INT_UDP_0		(1 << 18)

#define IAVF_RX_DESC_RXE		(1 << 19)
#define IAVF_RX_DESC_HBO		(1 << 21)
#define IAVF_RX_DESC_IPE		(1 << 22)
#define IAVF_RX_DESC_L4E		(1 << 23)
#define IAVF_RX_DESC_EIPE		(1 << 24)
#define IAVF_RX_DESC_OVERSIZE		(1 << 25)

#define IAVF_RX_DESC_PTYPE_SHIFT	30
#define IAVF_RX_DESC_PTYPE_MASK		(0xffULL << IAVF_RX_DESC_PTYPE_SHIFT)

#define IAVF_RX_DESC_PLEN_SHIFT		38
#define IAVF_RX_DESC_PLEN_MASK		(0x3fffULL << IAVF_RX_DESC_PLEN_SHIFT)
#define IAVF_RX_DESC_HLEN_SHIFT		42
#define IAVF_RX_DESC_HLEN_MASK		(0x7ffULL << IAVF_RX_DESC_HLEN_SHIFT)
} __packed __aligned(16);

struct iavf_rx_wb_desc_32 {
	uint64_t		qword0;
	uint64_t		qword1;
	uint64_t		qword2;
	uint64_t		qword3;
} __packed __aligned(16);


#define IAVF_VF_MAJOR			1
#define IAVF_VF_MINOR			1

#define IAVF_TX_PKT_DESCS		8
#define IAVF_TX_QUEUE_ALIGN		128
#define IAVF_RX_QUEUE_ALIGN		128

#define IAVF_HARDMTU			9712 /* 9726 - ETHER_HDR_LEN */

#define IAVF_PCIREG			PCI_MAPREG_START

#define IAVF_ITR0			0x0
#define IAVF_ITR1			0x1
#define IAVF_ITR2			0x2
#define IAVF_NOITR			0x3

#define IAVF_AQ_NUM			256
#define IAVF_AQ_MASK			(IAVF_AQ_NUM - 1)
#define IAVF_AQ_ALIGN			64 /* lol */
#define IAVF_AQ_BUFLEN			4096

struct iavf_aq_regs {
	bus_size_t		atq_tail;
	bus_size_t		atq_head;
	bus_size_t		atq_len;
	bus_size_t		atq_bal;
	bus_size_t		atq_bah;

	bus_size_t		arq_tail;
	bus_size_t		arq_head;
	bus_size_t		arq_len;
	bus_size_t		arq_bal;
	bus_size_t		arq_bah;

	uint32_t		atq_len_enable;
	uint32_t		atq_tail_mask;
	uint32_t		atq_head_mask;

	uint32_t		arq_len_enable;
	uint32_t		arq_tail_mask;
	uint32_t		arq_head_mask;
};

struct iavf_aq_buf {
	SIMPLEQ_ENTRY(iavf_aq_buf)
				 aqb_entry;
	void			*aqb_data;
	bus_dmamap_t		 aqb_map;
};
SIMPLEQ_HEAD(iavf_aq_bufs, iavf_aq_buf);

struct iavf_dmamem {
	bus_dmamap_t		ixm_map;
	bus_dma_segment_t	ixm_seg;
	int			ixm_nsegs;
	size_t			ixm_size;
	caddr_t			ixm_kva;
};
#define IAVF_DMA_MAP(_ixm)	((_ixm)->ixm_map)
#define IAVF_DMA_DVA(_ixm)	((_ixm)->ixm_map->dm_segs[0].ds_addr)
#define IAVF_DMA_KVA(_ixm)	((void *)(_ixm)->ixm_kva)
#define IAVF_DMA_LEN(_ixm)	((_ixm)->ixm_size)

struct iavf_tx_map {
	struct mbuf		*txm_m;
	bus_dmamap_t		 txm_map;
	unsigned int		 txm_eop;
};

struct iavf_tx_ring {
	unsigned int		 txr_prod;
	unsigned int		 txr_cons;

	struct iavf_tx_map	*txr_maps;
	struct iavf_dmamem	 txr_mem;

	bus_size_t		 txr_tail;
	unsigned int		 txr_qid;
};

struct iavf_rx_map {
	struct mbuf		*rxm_m;
	bus_dmamap_t		 rxm_map;
};

struct iavf_rx_ring {
	struct iavf_softc	*rxr_sc;

	struct if_rxring	 rxr_acct;
	struct timeout		 rxr_refill;

	unsigned int		 rxr_prod;
	unsigned int		 rxr_cons;

	struct iavf_rx_map	*rxr_maps;
	struct iavf_dmamem	 rxr_mem;

	struct mbuf		*rxr_m_head;
	struct mbuf		**rxr_m_tail;

	bus_size_t		 rxr_tail;
	unsigned int		 rxr_qid;
};

struct iavf_softc {
	struct device		 sc_dev;
	struct arpcom		 sc_ac;
	struct ifmedia		 sc_media;
	uint64_t		 sc_media_status;
	uint64_t		 sc_media_active;

	pci_chipset_tag_t	 sc_pc;
	pci_intr_handle_t	 sc_ih;
	void			*sc_ihc;
	pcitag_t		 sc_tag;

	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_size_t		 sc_mems;

	uint32_t		 sc_major_ver;
	uint32_t		 sc_minor_ver;

	int			 sc_got_vf_resources;
	int			 sc_got_irq_map;
	uint32_t		 sc_vf_id;
	uint16_t		 sc_vsi_id;
	uint16_t		 sc_qset_handle;
	unsigned int		 sc_base_queue;

	struct cond		 sc_admin_cond;
	int			 sc_admin_result;
	struct timeout		 sc_admin_timeout;

	struct iavf_dmamem	 sc_scratch;

	const struct iavf_aq_regs *
				 sc_aq_regs;

	struct mutex		 sc_atq_mtx;
	struct iavf_dmamem	 sc_atq;
	unsigned int		 sc_atq_prod;
	unsigned int		 sc_atq_cons;

	struct iavf_dmamem	 sc_arq;
	struct task		 sc_arq_task;
	struct iavf_aq_bufs	 sc_arq_idle;
	struct iavf_aq_bufs	 sc_arq_live;
	struct if_rxring	 sc_arq_ring;
	unsigned int		 sc_arq_prod;
	unsigned int		 sc_arq_cons;

	struct task		 sc_link_state_task;

	unsigned int		 sc_tx_ring_ndescs;
	unsigned int		 sc_rx_ring_ndescs;
	unsigned int		 sc_nqueues;	/* 1 << sc_nqueues */

	struct rwlock		 sc_cfg_lock;
	unsigned int		 sc_dead;

	uint8_t			 sc_enaddr[ETHER_ADDR_LEN];
};
#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)

#define delaymsec(_ms)	delay(1000 * (_ms))

static int	iavf_dmamem_alloc(struct iavf_softc *, struct iavf_dmamem *,
		    bus_size_t, u_int);
static void	iavf_dmamem_free(struct iavf_softc *, struct iavf_dmamem *);

static int	iavf_arq_fill(struct iavf_softc *);
static void	iavf_arq_unfill(struct iavf_softc *);
static void	iavf_arq_timeout(void *);
static int	iavf_arq_wait(struct iavf_softc *, int);

static int	iavf_atq_post(struct iavf_softc *, struct iavf_aq_desc *);
static void	iavf_atq_done(struct iavf_softc *);

static int	iavf_get_version(struct iavf_softc *);
static int	iavf_get_vf_resources(struct iavf_softc *);
static int	iavf_config_irq_map(struct iavf_softc *);

static int	iavf_add_del_addr(struct iavf_softc *, uint8_t *, int);
static int	iavf_process_arq(struct iavf_softc *);
static void	iavf_arq_task(void *);

static int	iavf_match(struct device *, void *, void *);
static void	iavf_attach(struct device *, struct device *, void *);

static int	iavf_media_change(struct ifnet *);
static void	iavf_media_status(struct ifnet *, struct ifmediareq *);
static void	iavf_watchdog(struct ifnet *);
static int	iavf_ioctl(struct ifnet *, u_long, caddr_t);
static void	iavf_start(struct ifqueue *);
static int	iavf_intr(void *);
static int	iavf_up(struct iavf_softc *);
static int	iavf_down(struct iavf_softc *);
static int	iavf_iff(struct iavf_softc *);

static struct iavf_tx_ring *
		iavf_txr_alloc(struct iavf_softc *, unsigned int);
static void	iavf_txr_clean(struct iavf_softc *, struct iavf_tx_ring *);
static void	iavf_txr_free(struct iavf_softc *, struct iavf_tx_ring *);
static int	iavf_txeof(struct iavf_softc *, struct ifqueue *);

static struct iavf_rx_ring *
		iavf_rxr_alloc(struct iavf_softc *, unsigned int);
static void	iavf_rxr_clean(struct iavf_softc *, struct iavf_rx_ring *);
static void	iavf_rxr_free(struct iavf_softc *, struct iavf_rx_ring *);
static int	iavf_rxeof(struct iavf_softc *, struct ifiqueue *);
static void	iavf_rxfill(struct iavf_softc *, struct iavf_rx_ring *);
static void	iavf_rxrefill(void *);
static int	iavf_rxrinfo(struct iavf_softc *, struct if_rxrinfo *);

struct cfdriver iavf_cd = {
	NULL,
	"iavf",
	DV_IFNET,
};

struct cfattach iavf_ca = {
	sizeof(struct iavf_softc),
	iavf_match,
	iavf_attach,
};

static const struct iavf_aq_regs iavf_aq_regs = {
	.atq_tail	= I40E_VF_ATQT1,
	.atq_tail_mask	= I40E_VF_ATQT1_ATQT_MASK,
	.atq_head	= I40E_VF_ATQH1,
	.atq_head_mask	= I40E_VF_ARQH1_ARQH_MASK,
	.atq_len	= I40E_VF_ATQLEN1,
	.atq_bal	= I40E_VF_ATQBAL1,
	.atq_bah	= I40E_VF_ATQBAH1,
	.atq_len_enable	= I40E_VF_ATQLEN1_ATQENABLE_MASK,

	.arq_tail	= I40E_VF_ARQT1,
	.arq_tail_mask	= I40E_VF_ARQT1_ARQT_MASK,
	.arq_head	= I40E_VF_ARQH1,
	.arq_head_mask	= I40E_VF_ARQH1_ARQH_MASK,
	.arq_len	= I40E_VF_ARQLEN1,
	.arq_bal	= I40E_VF_ARQBAL1,
	.arq_bah	= I40E_VF_ARQBAH1,
	.arq_len_enable	= I40E_VF_ARQLEN1_ARQENABLE_MASK,
};

#define iavf_rd(_s, _r) \
	bus_space_read_4((_s)->sc_memt, (_s)->sc_memh, (_r))
#define iavf_wr(_s, _r, _v) \
	bus_space_write_4((_s)->sc_memt, (_s)->sc_memh, (_r), (_v))
#define iavf_barrier(_s, _r, _l, _o) \
	bus_space_barrier((_s)->sc_memt, (_s)->sc_memh, (_r), (_l), (_o))
#define iavf_intr_enable(_s) \
	iavf_wr((_s), I40E_VFINT_DYN_CTL01, I40E_VFINT_DYN_CTL0_INTENA_MASK | \
	    I40E_VFINT_DYN_CTL0_CLEARPBA_MASK | \
	    (IAVF_NOITR << I40E_VFINT_DYN_CTL0_ITR_INDX_SHIFT)); \
	iavf_wr((_s), I40E_VFINT_ICR0_ENA1, I40E_VFINT_ICR0_ENA1_ADMINQ_MASK)

#define iavf_nqueues(_sc)	(1 << (_sc)->sc_nqueues)
#define iavf_allqueues(_sc)	((1 << ((_sc)->sc_nqueues+1)) - 1)

#ifdef __LP64__
#define iavf_dmamem_hi(_ixm)	(uint32_t)(IAVF_DMA_DVA(_ixm) >> 32)
#else
#define iavf_dmamem_hi(_ixm)	0
#endif

#define iavf_dmamem_lo(_ixm) 	(uint32_t)IAVF_DMA_DVA(_ixm)

static inline void
iavf_aq_dva(struct iavf_aq_desc *iaq, bus_addr_t addr)
{
#ifdef __LP64__
	htolem32(&iaq->iaq_param[2], addr >> 32);
#else
	iaq->iaq_param[2] = htole32(0);
#endif
	htolem32(&iaq->iaq_param[3], addr);
}

#if _BYTE_ORDER == _BIG_ENDIAN
#define HTOLE16(_x)	(uint16_t)(((_x) & 0xff) << 8 | ((_x) & 0xff00) >> 8)
#else
#define HTOLE16(_x)	(_x)
#endif

static const struct pci_matchid iavf_devices[] = {
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_VF },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_VF_HV },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_X722_VF },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_ADAPTIVE_VF },
};

static int
iavf_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, iavf_devices, nitems(iavf_devices)));
}

void
iavf_attach(struct device *parent, struct device *self, void *aux)
{
	struct iavf_softc *sc = (struct iavf_softc *)self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct pci_attach_args *pa = aux;
	pcireg_t memtype;
	int tries;

	rw_init(&sc->sc_cfg_lock, "iavfcfg");

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_aq_regs = &iavf_aq_regs;

	sc->sc_nqueues = 0; /* 1 << 0 is 1 queue */
	sc->sc_tx_ring_ndescs = 1024;
	sc->sc_rx_ring_ndescs = 1024;

	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, IAVF_PCIREG);
	if (pci_mapreg_map(pa, IAVF_PCIREG, memtype, 0,
	    &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf(": unable to map registers\n");
		return;
	}

	for (tries = 0; tries < 100; tries++) {
		uint32_t reg;
		reg = iavf_rd(sc, I40E_VFGEN_RSTAT) &
		    I40E_VFGEN_RSTAT_VFR_STATE_MASK;
		if (reg == IAVF_VFR_VFACTIVE ||
		    reg == IAVF_VFR_COMPLETED)
			break;

		delay(10000);
	}
	if (tries == 100) {
		printf(": VF reset timed out\n");
		return;
	}

	mtx_init(&sc->sc_atq_mtx, IPL_NET);

	if (iavf_dmamem_alloc(sc, &sc->sc_atq,
	    sizeof(struct iavf_aq_desc) * IAVF_AQ_NUM, IAVF_AQ_ALIGN) != 0) {
		printf("\n" "%s: unable to allocate atq\n", DEVNAME(sc));
		goto unmap;
	}

	SIMPLEQ_INIT(&sc->sc_arq_idle);
	SIMPLEQ_INIT(&sc->sc_arq_live);
	if_rxr_init(&sc->sc_arq_ring, 2, IAVF_AQ_NUM - 1);
	task_set(&sc->sc_arq_task, iavf_arq_task, sc);
	sc->sc_arq_cons = 0;
	sc->sc_arq_prod = 0;

	if (iavf_dmamem_alloc(sc, &sc->sc_arq,
	    sizeof(struct iavf_aq_desc) * IAVF_AQ_NUM, IAVF_AQ_ALIGN) != 0) {
		printf("\n" "%s: unable to allocate arq\n", DEVNAME(sc));
		goto free_atq;
	}

	if (!iavf_arq_fill(sc)) {
		printf("\n" "%s: unable to fill arq descriptors\n",
		    DEVNAME(sc));
		goto free_arq;
	}
	timeout_set(&sc->sc_admin_timeout, iavf_arq_timeout, sc);

	if (iavf_dmamem_alloc(sc, &sc->sc_scratch, PAGE_SIZE, IAVF_AQ_ALIGN) != 0) {
		printf("\n" "%s: unable to allocate scratch\n", DEVNAME(sc));
		goto shutdown;
	}

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_atq),
	    0, IAVF_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_arq),
	    0, IAVF_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	iavf_wr(sc, sc->sc_aq_regs->atq_head, 0);
	iavf_wr(sc, sc->sc_aq_regs->arq_head, 0);
	iavf_wr(sc, sc->sc_aq_regs->atq_tail, 0);

	iavf_barrier(sc, 0, sc->sc_mems, BUS_SPACE_BARRIER_WRITE);

	iavf_wr(sc, sc->sc_aq_regs->atq_bal,
	    iavf_dmamem_lo(&sc->sc_atq));
	iavf_wr(sc, sc->sc_aq_regs->atq_bah,
	    iavf_dmamem_hi(&sc->sc_atq));
	iavf_wr(sc, sc->sc_aq_regs->atq_len,
	    sc->sc_aq_regs->atq_len_enable | IAVF_AQ_NUM);

	iavf_wr(sc, sc->sc_aq_regs->arq_bal,
	    iavf_dmamem_lo(&sc->sc_arq));
	iavf_wr(sc, sc->sc_aq_regs->arq_bah,
	    iavf_dmamem_hi(&sc->sc_arq));
	iavf_wr(sc, sc->sc_aq_regs->arq_len,
	    sc->sc_aq_regs->arq_len_enable | IAVF_AQ_NUM);

	iavf_wr(sc, sc->sc_aq_regs->arq_tail, sc->sc_arq_prod);

	if (iavf_get_version(sc) != 0) {
		printf(", unable to get VF interface version\n");
		goto free_scratch;
	}

	if (iavf_get_vf_resources(sc) != 0) {
		/* error printed by iavf_get_vf_resources */
		goto free_scratch;
	}

	if (iavf_config_irq_map(sc) != 0) {
		/* error printed by iavf_config_irq_map */
		goto free_scratch;
	}

	/* msix only? */
	if (pci_intr_map_msix(pa, 0, &sc->sc_ih) != 0) {
		printf(", unable to map interrupt\n");
		goto free_scratch;
	}

	/* generate an address if the pf didn't give us one */
	memcpy(sc->sc_enaddr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);
	if (memcmp(sc->sc_ac.ac_enaddr, etheranyaddr, ETHER_ADDR_LEN) == 0)
		ether_fakeaddr(ifp);

	printf(", %s, address %s\n", pci_intr_string(sc->sc_pc, sc->sc_ih),
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	sc->sc_ihc = pci_intr_establish(sc->sc_pc, sc->sc_ih,
	    IPL_NET | IPL_MPSAFE, iavf_intr, sc, DEVNAME(sc));
	if (sc->sc_ihc == NULL) {
		printf("%s: unable to establish interrupt handler\n",
		    DEVNAME(sc));
		goto free_scratch;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = iavf_ioctl;
	ifp->if_qstart = iavf_start;
	ifp->if_watchdog = iavf_watchdog;
	if (ifp->if_hardmtu == 0)
		ifp->if_hardmtu = IAVF_HARDMTU;
	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, 1);

	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
	    IFCAP_CSUM_UDPv4;
#endif

	ifmedia_init(&sc->sc_media, 0, iavf_media_change, iavf_media_status);

	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	if_attach_queues(ifp, iavf_nqueues(sc));
	if_attach_iqueues(ifp, iavf_nqueues(sc));

	iavf_intr_enable(sc);

	return;
free_scratch:
	iavf_dmamem_free(sc, &sc->sc_scratch);
shutdown:
	iavf_wr(sc, sc->sc_aq_regs->atq_head, 0);
	iavf_wr(sc, sc->sc_aq_regs->arq_head, 0);
	iavf_wr(sc, sc->sc_aq_regs->atq_tail, 0);
	iavf_wr(sc, sc->sc_aq_regs->arq_tail, 0);

	iavf_wr(sc, sc->sc_aq_regs->atq_bal, 0);
	iavf_wr(sc, sc->sc_aq_regs->atq_bah, 0);
	iavf_wr(sc, sc->sc_aq_regs->atq_len, 0);

	iavf_wr(sc, sc->sc_aq_regs->arq_bal, 0);
	iavf_wr(sc, sc->sc_aq_regs->arq_bah, 0);
	iavf_wr(sc, sc->sc_aq_regs->arq_len, 0);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_arq),
	    0, IAVF_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_atq),
	    0, IAVF_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	iavf_arq_unfill(sc);
free_arq:
	iavf_dmamem_free(sc, &sc->sc_arq);
free_atq:
	iavf_dmamem_free(sc, &sc->sc_atq);
unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

static int
iavf_media_change(struct ifnet *ifp)
{
	return (EOPNOTSUPP);
}

static void
iavf_media_status(struct ifnet *ifp, struct ifmediareq *ifm)
{
	struct iavf_softc *sc = ifp->if_softc;

	NET_ASSERT_LOCKED();

	ifm->ifm_status = sc->sc_media_status;
	ifm->ifm_active = sc->sc_media_active;
}

static void
iavf_watchdog(struct ifnet *ifp)
{

}

int
iavf_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iavf_softc *sc = (struct iavf_softc *)ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	uint8_t addrhi[ETHER_ADDR_LEN], addrlo[ETHER_ADDR_LEN];
	int /*aqerror,*/ error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				error = iavf_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = iavf_down(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCGIFRXR:
		error = iavf_rxrinfo(sc, (struct if_rxrinfo *)ifr->ifr_data);
		break;

	case SIOCADDMULTI:
		if (ether_addmulti(ifr, &sc->sc_ac) == ENETRESET) {
			error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
			if (error != 0)
				return (error);

			iavf_add_del_addr(sc, addrlo, 1);
			/* check result i guess? */

			if (sc->sc_ac.ac_multirangecnt > 0) {
				SET(ifp->if_flags, IFF_ALLMULTI);
				error = ENETRESET;
			}
		}
		break;

	case SIOCDELMULTI:
		if (ether_delmulti(ifr, &sc->sc_ac) == ENETRESET) {
			error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
			if (error != 0)
				return (error);

			iavf_add_del_addr(sc, addrlo, 0);

			if (ISSET(ifp->if_flags, IFF_ALLMULTI) &&
			    sc->sc_ac.ac_multirangecnt == 0) {
				CLR(ifp->if_flags, IFF_ALLMULTI);
				error = ENETRESET;
			}
		}
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	if (error == ENETRESET)
		error = iavf_iff(sc);

	return (error);
}

static int
iavf_config_vsi_queues(struct iavf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct iavf_aq_desc iaq;
	struct iavf_vc_queue_config_info *config;
	struct iavf_vc_txq_info *txq;
	struct iavf_vc_rxq_info *rxq;
	struct iavf_rx_ring *rxr;
	struct iavf_tx_ring *txr;
	int rv, i;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_CONFIG_VSI_QUEUES);
	iaq.iaq_datalen = htole16(sizeof(*config) +
	    iavf_nqueues(sc) * sizeof(struct iavf_vc_queue_pair_info));
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	config = IAVF_DMA_KVA(&sc->sc_scratch);
	config->vsi_id = htole16(sc->sc_vsi_id);
	config->num_queue_pairs = htole16(iavf_nqueues(sc));

	for (i = 0; i < iavf_nqueues(sc); i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		txq = &config->qpair[i].txq;
		txq->vsi_id = htole16(sc->sc_vsi_id);
		txq->queue_id = htole16(i);
		txq->ring_len = sc->sc_tx_ring_ndescs;
		txq->headwb_ena = 0;
		htolem64(&txq->dma_ring_addr, IAVF_DMA_DVA(&txr->txr_mem));
		txq->dma_headwb_addr = 0;

		rxq = &config->qpair[i].rxq;
		rxq->vsi_id = htole16(sc->sc_vsi_id);
		rxq->queue_id = htole16(i);
		rxq->ring_len = sc->sc_rx_ring_ndescs;
		rxq->splithdr_ena = 0;
		rxq->databuf_size = htole32(MCLBYTES);
		rxq->max_pkt_size = htole32(IAVF_HARDMTU);
		htolem64(&rxq->dma_ring_addr, IAVF_DMA_DVA(&rxr->rxr_mem));
		rxq->rx_split_pos = 0;
	}

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_scratch), 0,
	    IAVF_DMA_LEN(&sc->sc_scratch),
	    BUS_DMASYNC_PREREAD);

	iavf_atq_post(sc, &iaq);
	rv = iavf_arq_wait(sc, 250);
	if (rv != IAVF_VC_RC_SUCCESS) {
		printf("%s: CONFIG_VSI_QUEUES failed: %d\n", DEVNAME(sc), rv);
		return (1);
	}

	return (0);
}

static int
iavf_config_hena(struct iavf_softc *sc)
{
	struct iavf_aq_desc iaq;
	uint64_t *caps;
	int rv;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_SET_RSS_HENA);
	iaq.iaq_datalen = htole32(sizeof(*caps));
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	caps = IAVF_DMA_KVA(&sc->sc_scratch);
	*caps = 0;

	iavf_atq_post(sc, &iaq);
	rv = iavf_arq_wait(sc, 250);
	if (rv != IAVF_VC_RC_SUCCESS) {
		printf("%s: SET_RSS_HENA failed: %d\n", DEVNAME(sc), rv);
		return (1);
	}

	caps = IAVF_DMA_KVA(&sc->sc_scratch);

	return (0);
}

static int
iavf_queue_select(struct iavf_softc *sc, int opcode)
{
	struct iavf_aq_desc iaq;
	struct iavf_vc_queue_select *qsel;
	int rv;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	iaq.iaq_vc_opcode = htole32(opcode);
	iaq.iaq_datalen = htole16(sizeof(*qsel));
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	qsel = IAVF_DMA_KVA(&sc->sc_scratch);
	qsel->vsi_id = htole16(sc->sc_vsi_id);
	qsel->rx_queues = htole32(iavf_allqueues(sc));
	qsel->tx_queues = htole32(iavf_allqueues(sc));

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_scratch), 0,
	    IAVF_DMA_LEN(&sc->sc_scratch),
	    BUS_DMASYNC_PREREAD);

	iavf_atq_post(sc, &iaq);
	rv = iavf_arq_wait(sc, 250);
	if (rv != IAVF_VC_RC_SUCCESS) {
		printf("%s: queue op %d failed: %d\n", DEVNAME(sc), opcode, rv);
		return (1);
	}

	return (0);
}

static int
iavf_up(struct iavf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct iavf_rx_ring *rxr;
	struct iavf_tx_ring *txr;
	unsigned int nqueues, i;
	int rv = ENOMEM;

	nqueues = iavf_nqueues(sc);
	KASSERT(nqueues == 1); /* XXX */

	rw_enter_write(&sc->sc_cfg_lock);
	if (sc->sc_dead) {
		rw_exit_write(&sc->sc_cfg_lock);
		return (ENXIO);
	}

	for (i = 0; i < nqueues; i++) {
		rxr = iavf_rxr_alloc(sc, i);
		if (rxr == NULL)
			goto free;

		txr = iavf_txr_alloc(sc, i);
		if (txr == NULL) {
			iavf_rxr_free(sc, rxr);
			goto free;
		}

		ifp->if_iqs[i]->ifiq_softc = rxr;
		ifp->if_ifqs[i]->ifq_softc = txr;

		iavf_rxfill(sc, rxr);
	}

	if (iavf_config_vsi_queues(sc) != 0)
		goto down;

	if (iavf_config_hena(sc) != 0)
		goto down;

	if (iavf_queue_select(sc, IAVF_VC_OP_ENABLE_QUEUES) != 0)
		goto down;

	SET(ifp->if_flags, IFF_RUNNING);

	iavf_wr(sc, I40E_VFINT_ITR01(0), 0x7a);
	iavf_wr(sc, I40E_VFINT_ITR01(1), 0x7a);
	iavf_wr(sc, I40E_VFINT_ITR01(2), 0);

	rw_exit_write(&sc->sc_cfg_lock);

	return (ENETRESET);

free:
	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		if (rxr == NULL) {
			/*
			 * tx and rx get set at the same time, so if one
			 * is NULL, the other is too.
			 */
			continue;
		}

		iavf_txr_free(sc, txr);
		iavf_rxr_free(sc, rxr);
	}
	rw_exit_write(&sc->sc_cfg_lock);
	return (rv);
down:
	rw_exit_write(&sc->sc_cfg_lock);
	iavf_down(sc);
	return (ETIMEDOUT);
}

static int
iavf_config_promisc_mode(struct iavf_softc *sc, int unicast, int multicast)
{
	struct iavf_aq_desc iaq;
	struct iavf_vc_promisc_info *promisc;
	int rv, flags;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_CONFIG_PROMISC);
	iaq.iaq_datalen = htole16(sizeof(*promisc));
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	flags = 0;
	if (unicast)
		flags |= IAVF_FLAG_VF_UNICAST_PROMISC;
	if (multicast)
		flags |= IAVF_FLAG_VF_MULTICAST_PROMISC;

	promisc = IAVF_DMA_KVA(&sc->sc_scratch);
	promisc->vsi_id = htole16(sc->sc_vsi_id);
	promisc->flags = htole16(flags);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_scratch), 0,
	    IAVF_DMA_LEN(&sc->sc_scratch),
	    BUS_DMASYNC_PREREAD);

	iavf_atq_post(sc, &iaq);
	rv = iavf_arq_wait(sc, 250);
	if (rv != IAVF_VC_RC_SUCCESS) {
		printf("%s: CONFIG_PROMISC_MODE failed: %d\n", DEVNAME(sc), rv);
		return (1);
	}

	return (0);
}

static int
iavf_add_del_addr(struct iavf_softc *sc, uint8_t *addr, int add)
{
	struct iavf_aq_desc iaq;
	struct iavf_vc_eth_addr_list *addrs;
	struct iavf_vc_eth_addr *vcaddr;
	int rv;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	if (add)
		iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_ADD_ETH_ADDR);
	else
		iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_DEL_ETH_ADDR);
	iaq.iaq_datalen = htole16(sizeof(*addrs) + sizeof(*vcaddr));
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	addrs = IAVF_DMA_KVA(&sc->sc_scratch);
	addrs->vsi_id = htole16(sc->sc_vsi_id);
	addrs->num_elements = htole16(1);

	vcaddr = addrs->list;
	memcpy(vcaddr->addr, addr, ETHER_ADDR_LEN);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_scratch), 0,
	    IAVF_DMA_LEN(&sc->sc_scratch),
	    BUS_DMASYNC_PREREAD);

	iavf_atq_post(sc, &iaq);
	rv = iavf_arq_wait(sc, 250);
	if (rv != IAVF_VC_RC_SUCCESS) {
		printf("%s: ADD/DEL_ETH_ADDR failed: %d\n", DEVNAME(sc), rv);
		return (1);
	}

	return (0);
}

static int
iavf_iff(struct iavf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int unicast, multicast;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return (0);

	rw_enter_write(&sc->sc_cfg_lock);

	unicast = 0;
	multicast = 0;
	if (ISSET(ifp->if_flags, IFF_PROMISC)) {
		unicast = 1;
		multicast = 1;
	} else if (ISSET(ifp->if_flags, IFF_ALLMULTI)) {
		multicast = 1;
	}
	iavf_config_promisc_mode(sc, unicast, multicast);

	if (memcmp(sc->sc_enaddr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN) != 0) {
		if (memcmp(sc->sc_enaddr, etheranyaddr, ETHER_ADDR_LEN) != 0)
			iavf_add_del_addr(sc, sc->sc_enaddr, 0);
		memcpy(sc->sc_enaddr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);
		iavf_add_del_addr(sc, sc->sc_enaddr, 1);
	}

	rw_exit_write(&sc->sc_cfg_lock);
	return (0);
}

static int
iavf_down(struct iavf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct iavf_rx_ring *rxr;
	struct iavf_tx_ring *txr;
	unsigned int nqueues, i;
	uint32_t reg;
	int error = 0;

	nqueues = iavf_nqueues(sc);

	rw_enter_write(&sc->sc_cfg_lock);

	CLR(ifp->if_flags, IFF_RUNNING);

	NET_UNLOCK();

	/* disable queues */
	if (iavf_queue_select(sc, IAVF_VC_OP_DISABLE_QUEUES) != 0)
		goto die;

	/* mask interrupts */
	reg = iavf_rd(sc, I40E_VFINT_DYN_CTL01);
	reg |= I40E_VFINT_DYN_CTL0_INTENA_MSK_MASK |
	    (IAVF_NOITR << I40E_VFINT_DYN_CTL0_ITR_INDX_SHIFT);
	iavf_wr(sc, I40E_VFINT_DYN_CTL01, reg);

	/* make sure no hw generated work is still in flight */
	intr_barrier(sc->sc_ihc);
	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		ifq_barrier(ifp->if_ifqs[i]);

		timeout_del_barrier(&rxr->rxr_refill);
	}

	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		iavf_txr_clean(sc, txr);
		iavf_rxr_clean(sc, rxr);

		iavf_txr_free(sc, txr);
		iavf_rxr_free(sc, rxr);

		ifp->if_iqs[i]->ifiq_softc = NULL;
		ifp->if_ifqs[i]->ifq_softc =  NULL;
	}

	/* unmask */
	reg = iavf_rd(sc, I40E_VFINT_DYN_CTL01);
	reg |= (IAVF_NOITR << I40E_VFINT_DYN_CTL0_ITR_INDX_SHIFT);
	iavf_wr(sc, I40E_VFINT_DYN_CTL01, reg);

out:
	rw_exit_write(&sc->sc_cfg_lock);
	NET_LOCK();
	return (error);
die:
	sc->sc_dead = 1;
	log(LOG_CRIT, "%s: failed to shut down rings", DEVNAME(sc));
	error = ETIMEDOUT;
	goto out;
}

static struct iavf_tx_ring *
iavf_txr_alloc(struct iavf_softc *sc, unsigned int qid)
{
	struct iavf_tx_ring *txr;
	struct iavf_tx_map *maps, *txm;
	unsigned int i;

	txr = malloc(sizeof(*txr), M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (txr == NULL)
		return (NULL);

	maps = mallocarray(sizeof(*maps),
	    sc->sc_tx_ring_ndescs, M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (maps == NULL)
		goto free;

	if (iavf_dmamem_alloc(sc, &txr->txr_mem,
	    sizeof(struct iavf_tx_desc) * sc->sc_tx_ring_ndescs,
	    IAVF_TX_QUEUE_ALIGN) != 0)
		goto freemap;

	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		if (bus_dmamap_create(sc->sc_dmat,
		    IAVF_HARDMTU, IAVF_TX_PKT_DESCS, IAVF_HARDMTU, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
		    &txm->txm_map) != 0)
			goto uncreate;

		txm->txm_eop = -1;
		txm->txm_m = NULL;
	}

	txr->txr_cons = txr->txr_prod = 0;
	txr->txr_maps = maps;

	txr->txr_tail = I40E_QTX_TAIL1(qid);
	txr->txr_qid = qid;

	return (txr);

uncreate:
	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		if (txm->txm_map == NULL)
			continue;

		bus_dmamap_destroy(sc->sc_dmat, txm->txm_map);
	}

	iavf_dmamem_free(sc, &txr->txr_mem);
freemap:
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_tx_ring_ndescs);
free:
	free(txr, M_DEVBUF, sizeof(*txr));
	return (NULL);
}

static void
iavf_txr_clean(struct iavf_softc *sc, struct iavf_tx_ring *txr)
{
	struct iavf_tx_map *maps, *txm;
	bus_dmamap_t map;
	unsigned int i;

	maps = txr->txr_maps;
	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		if (txm->txm_m == NULL)
			continue;

		map = txm->txm_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);

		m_freem(txm->txm_m);
		txm->txm_m = NULL;
	}
}

static void
iavf_txr_free(struct iavf_softc *sc, struct iavf_tx_ring *txr)
{
	struct iavf_tx_map *maps, *txm;
	unsigned int i;

	maps = txr->txr_maps;
	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		bus_dmamap_destroy(sc->sc_dmat, txm->txm_map);
	}

	iavf_dmamem_free(sc, &txr->txr_mem);
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_tx_ring_ndescs);
	free(txr, M_DEVBUF, sizeof(*txr));
}

static inline int
iavf_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m)
{
	int error;

	error = bus_dmamap_load_mbuf(dmat, map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT);
	if (error != EFBIG)
		return (error);

	error = m_defrag(m, M_DONTWAIT);
	if (error != 0)
		return (error);

	return (bus_dmamap_load_mbuf(dmat, map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT));
}

static void
iavf_start(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	struct iavf_softc *sc = ifp->if_softc;
	struct iavf_tx_ring *txr = ifq->ifq_softc;
	struct iavf_tx_desc *ring, *txd;
	struct iavf_tx_map *txm;
	bus_dmamap_t map;
	struct mbuf *m;
	uint64_t cmd;
	unsigned int prod, free, last, i;
	unsigned int mask;
	int post = 0;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	if (!LINK_STATE_IS_UP(ifp->if_link_state)) {
		ifq_purge(ifq);
		return;
	}

	prod = txr->txr_prod;
	free = txr->txr_cons;
	if (free <= prod)
		free += sc->sc_tx_ring_ndescs;
	free -= prod;

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&txr->txr_mem),
	    0, IAVF_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_POSTWRITE);

	ring = IAVF_DMA_KVA(&txr->txr_mem);
	mask = sc->sc_tx_ring_ndescs - 1;

	for (;;) {
		if (free <= IAVF_TX_PKT_DESCS) {
			ifq_set_oactive(ifq);
			break;
		}

		m = ifq_dequeue(ifq);
		if (m == NULL)
			break;

		txm = &txr->txr_maps[prod];
		map = txm->txm_map;

		if (iavf_load_mbuf(sc->sc_dmat, map, m) != 0) {
			ifq->ifq_errors++;
			m_freem(m);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    map->dm_mapsize, BUS_DMASYNC_PREWRITE);

		for (i = 0; i < map->dm_nsegs; i++) {
			txd = &ring[prod];

			cmd = (uint64_t)map->dm_segs[i].ds_len <<
			    IAVF_TX_DESC_BSIZE_SHIFT;
			cmd |= IAVF_TX_DESC_DTYPE_DATA | IAVF_TX_DESC_CMD_ICRC;

			htolem64(&txd->addr, map->dm_segs[i].ds_addr);
			htolem64(&txd->cmd, cmd);

			last = prod;

			prod++;
			prod &= mask;
		}
		cmd |= IAVF_TX_DESC_CMD_EOP | IAVF_TX_DESC_CMD_RS;
		htolem64(&txd->cmd, cmd);

		txm->txm_m = m;
		txm->txm_eop = last;

#if NBPFILTER > 0
		if_bpf = ifp->if_bpf;
		if (if_bpf)
			bpf_mtap_ether(if_bpf, m, BPF_DIRECTION_OUT);
#endif

		free -= i;
		post = 1;
	}

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&txr->txr_mem),
	    0, IAVF_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_PREWRITE);

	if (post) {
		txr->txr_prod = prod;
		iavf_wr(sc, txr->txr_tail, prod);
	}
}

static int
iavf_txeof(struct iavf_softc *sc, struct ifqueue *ifq)
{
	struct iavf_tx_ring *txr = ifq->ifq_softc;
	struct iavf_tx_desc *ring, *txd;
	struct iavf_tx_map *txm;
	bus_dmamap_t map;
	unsigned int cons, prod, last;
	unsigned int mask;
	uint64_t dtype;
	int done = 0;

	prod = txr->txr_prod;
	cons = txr->txr_cons;

	if (cons == prod)
		return (0);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&txr->txr_mem),
	    0, IAVF_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_POSTREAD);

	ring = IAVF_DMA_KVA(&txr->txr_mem);
	mask = sc->sc_tx_ring_ndescs - 1;

	do {
		txm = &txr->txr_maps[cons];
		last = txm->txm_eop;
		txd = &ring[last];

		dtype = txd->cmd & htole64(IAVF_TX_DESC_DTYPE_MASK);
		if (dtype != htole64(IAVF_TX_DESC_DTYPE_DONE))
			break;

		map = txm->txm_map;

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);
		m_freem(txm->txm_m);

		txm->txm_m = NULL;
		txm->txm_eop = -1;

		cons = last + 1;
		cons &= mask;

		done = 1;
	} while (cons != prod);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&txr->txr_mem),
	    0, IAVF_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_PREREAD);

	txr->txr_cons = cons;

	//ixl_enable(sc, txr->txr_msix);

	if (ifq_is_oactive(ifq))
		ifq_restart(ifq);

	return (done);
}

static struct iavf_rx_ring *
iavf_rxr_alloc(struct iavf_softc *sc, unsigned int qid)
{
	struct iavf_rx_ring *rxr;
	struct iavf_rx_map *maps, *rxm;
	unsigned int i;

	rxr = malloc(sizeof(*rxr), M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (rxr == NULL)
		return (NULL);

	maps = mallocarray(sizeof(*maps),
	    sc->sc_rx_ring_ndescs, M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (maps == NULL)
		goto free;

	if (iavf_dmamem_alloc(sc, &rxr->rxr_mem,
	    sizeof(struct iavf_rx_rd_desc_32) * sc->sc_rx_ring_ndescs,
	    IAVF_RX_QUEUE_ALIGN) != 0)
		goto freemap;

	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		if (bus_dmamap_create(sc->sc_dmat,
		    IAVF_HARDMTU, 1, IAVF_HARDMTU, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
		    &rxm->rxm_map) != 0)
			goto uncreate;

		rxm->rxm_m = NULL;
	}

	rxr->rxr_sc = sc;
	if_rxr_init(&rxr->rxr_acct, 17, sc->sc_rx_ring_ndescs - 1);
	timeout_set(&rxr->rxr_refill, iavf_rxrefill, rxr);
	rxr->rxr_cons = rxr->rxr_prod = 0;
	rxr->rxr_m_head = NULL;
	rxr->rxr_m_tail = &rxr->rxr_m_head;
	rxr->rxr_maps = maps;

	rxr->rxr_tail = I40E_QRX_TAIL1(qid);
	rxr->rxr_qid = qid;

	return (rxr);

uncreate:
	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		if (rxm->rxm_map == NULL)
			continue;

		bus_dmamap_destroy(sc->sc_dmat, rxm->rxm_map);
	}

	iavf_dmamem_free(sc, &rxr->rxr_mem);
freemap:
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_rx_ring_ndescs);
free:
	free(rxr, M_DEVBUF, sizeof(*rxr));
	return (NULL);
}

static void
iavf_rxr_clean(struct iavf_softc *sc, struct iavf_rx_ring *rxr)
{
	struct iavf_rx_map *maps, *rxm;
	bus_dmamap_t map;
	unsigned int i;

	timeout_del_barrier(&rxr->rxr_refill);

	maps = rxr->rxr_maps;
	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		if (rxm->rxm_m == NULL)
			continue;

		map = rxm->rxm_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);

		m_freem(rxm->rxm_m);
		rxm->rxm_m = NULL;
	}

	m_freem(rxr->rxr_m_head);
	rxr->rxr_m_head = NULL;
	rxr->rxr_m_tail = &rxr->rxr_m_head;

	rxr->rxr_prod = rxr->rxr_cons = 0;
}

static void
iavf_rxr_free(struct iavf_softc *sc, struct iavf_rx_ring *rxr)
{
	struct iavf_rx_map *maps, *rxm;
	unsigned int i;

	maps = rxr->rxr_maps;
	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		bus_dmamap_destroy(sc->sc_dmat, rxm->rxm_map);
	}

	iavf_dmamem_free(sc, &rxr->rxr_mem);
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_rx_ring_ndescs);
	free(rxr, M_DEVBUF, sizeof(*rxr));
}

static int
iavf_rxeof(struct iavf_softc *sc, struct ifiqueue *ifiq)
{
	struct iavf_rx_ring *rxr = ifiq->ifiq_softc;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct iavf_rx_wb_desc_32 *ring, *rxd;
	struct iavf_rx_map *rxm;
	bus_dmamap_t map;
	unsigned int cons, prod;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	uint64_t word;
	unsigned int len;
	unsigned int mask;
	int done = 0;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return (0);

	prod = rxr->rxr_prod;
	cons = rxr->rxr_cons;

	if (cons == prod)
		return (0);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&rxr->rxr_mem),
	    0, IAVF_DMA_LEN(&rxr->rxr_mem),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	ring = IAVF_DMA_KVA(&rxr->rxr_mem);
	mask = sc->sc_rx_ring_ndescs - 1;

	do {
		rxd = &ring[cons];

		word = lemtoh64(&rxd->qword1);
		if (!ISSET(word, IAVF_RX_DESC_DD))
			break;

		if_rxr_put(&rxr->rxr_acct, 1);

		rxm = &rxr->rxr_maps[cons];

		map = rxm->rxm_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, map);
		
		m = rxm->rxm_m;
		rxm->rxm_m = NULL;

		len = (word & IAVF_RX_DESC_PLEN_MASK) >> IAVF_RX_DESC_PLEN_SHIFT;
		m->m_len = len;
		m->m_pkthdr.len = 0;

		m->m_next = NULL;
		*rxr->rxr_m_tail = m;
		rxr->rxr_m_tail = &m->m_next;

		m = rxr->rxr_m_head;
		m->m_pkthdr.len += len;

		if (ISSET(word, IAVF_RX_DESC_EOP)) {
			if (!ISSET(word,
			    IAVF_RX_DESC_RXE | IAVF_RX_DESC_OVERSIZE)) {
				ml_enqueue(&ml, m);
			} else {
				ifp->if_ierrors++; /* XXX */
				m_freem(m);
			}

			rxr->rxr_m_head = NULL;
			rxr->rxr_m_tail = &rxr->rxr_m_head;
		}

		cons++;
		cons &= mask;

		done = 1;
	} while (cons != prod);

	if (done) {
		rxr->rxr_cons = cons;
		if (ifiq_input(ifiq, &ml))
			if_rxr_livelocked(&rxr->rxr_acct);
		iavf_rxfill(sc, rxr);
	}

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&rxr->rxr_mem),
	    0, IAVF_DMA_LEN(&rxr->rxr_mem),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return (done);
}

static void
iavf_rxfill(struct iavf_softc *sc, struct iavf_rx_ring *rxr)
{
	struct iavf_rx_rd_desc_32 *ring, *rxd;
	struct iavf_rx_map *rxm;
	bus_dmamap_t map;
	struct mbuf *m;
	unsigned int prod;
	unsigned int slots;
	unsigned int mask;
	int post = 0;

	slots = if_rxr_get(&rxr->rxr_acct, sc->sc_rx_ring_ndescs);
	if (slots == 0)
		return;

	prod = rxr->rxr_prod;

	ring = IAVF_DMA_KVA(&rxr->rxr_mem);
	mask = sc->sc_rx_ring_ndescs - 1;

	do {
		rxm = &rxr->rxr_maps[prod];

		m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES + ETHER_ALIGN);
		if (m == NULL)
			break;
		m->m_data += (m->m_ext.ext_size - (MCLBYTES + ETHER_ALIGN));
		m->m_len = m->m_pkthdr.len = MCLBYTES + ETHER_ALIGN;

		map = rxm->rxm_map;

		if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
		    BUS_DMA_NOWAIT) != 0) {
			m_freem(m);
			break;
		}

		rxm->rxm_m = m;

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);

		rxd = &ring[prod];

		htolem64(&rxd->paddr, map->dm_segs[0].ds_addr);
		rxd->haddr = htole64(0);

		prod++;
		prod &= mask;

		post = 1;
	} while (--slots);

	if_rxr_put(&rxr->rxr_acct, slots);

	if (if_rxr_inuse(&rxr->rxr_acct) == 0)
		timeout_add(&rxr->rxr_refill, 1);
	else if (post) {
		rxr->rxr_prod = prod;
		iavf_wr(sc, rxr->rxr_tail, prod);
	}
}

void
iavf_rxrefill(void *arg)
{
	struct iavf_rx_ring *rxr = arg;
	struct iavf_softc *sc = rxr->rxr_sc;

	iavf_rxfill(sc, rxr);
}

static int
iavf_rxrinfo(struct iavf_softc *sc, struct if_rxrinfo *ifri)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct if_rxring_info *ifr;
	struct iavf_rx_ring *ring;
	int i, rv;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return (ENOTTY);

	ifr = mallocarray(sizeof(*ifr), iavf_nqueues(sc), M_TEMP,
	    M_WAITOK|M_CANFAIL|M_ZERO);
	if (ifr == NULL)
		return (ENOMEM);

	for (i = 0; i < iavf_nqueues(sc); i++) {
		ring = ifp->if_iqs[i]->ifiq_softc;
		ifr[i].ifr_size = MCLBYTES;
		ifr[i].ifr_info = ring->rxr_acct;
	}

	rv = if_rxr_info_ioctl(ifri, iavf_nqueues(sc), ifr);
	free(ifr, M_TEMP, iavf_nqueues(sc) * sizeof(*ifr));

	return (rv);
}

static int
iavf_intr(void *xsc)
{
	struct iavf_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t icr, ena;
	int i, rv = 0;

	ena = iavf_rd(sc, I40E_VFINT_ICR0_ENA1);
	iavf_intr_enable(sc);
	icr = iavf_rd(sc, I40E_VFINT_ICR01);

	if (ISSET(icr, I40E_VFINT_ICR01_ADMINQ_MASK)) {
		iavf_atq_done(sc);
		task_add(systq, &sc->sc_arq_task);
		rv = 1;
	}

	if (ISSET(icr, I40E_VFINT_ICR01_QUEUE_0_MASK)) {
		for (i = 0; i < iavf_nqueues(sc); i++) {
			rv |= iavf_rxeof(sc, ifp->if_iqs[i]);
			rv |= iavf_txeof(sc, ifp->if_ifqs[i]);
		}
	}

	return (rv);
}

static void
iavf_process_vf_resources(struct iavf_softc *sc, struct iavf_aq_desc *desc,
    struct iavf_aq_buf *buf)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct iavf_vc_vf_resource *vf_res;
	struct iavf_vc_vsi_resource *vsi_res;
	int mtu;

	sc->sc_got_vf_resources = 1;

	vf_res = buf->aqb_data;
	if (letoh16(vf_res->num_vsis) == 0) {
		printf(", no VSI available\n");
		/* set vsi number to something */
		return;
	}

	mtu = letoh16(vf_res->max_mtu);
	if (mtu != 0)
		ifp->if_hardmtu = MIN(IAVF_HARDMTU, mtu);

	/* limit vectors to what we got here? */

	/* just take the first vsi */
	vsi_res = &vf_res->vsi_res[0];
	sc->sc_vsi_id = letoh16(vsi_res->vsi_id);
	sc->sc_qset_handle = letoh16(vsi_res->qset_handle);
	/* limit number of queues to what we got here */
	/* is vsi type interesting? */

	sc->sc_vf_id = letoh32(desc->iaq_param[0]);

	memcpy(sc->sc_ac.ac_enaddr, vsi_res->default_mac, ETHER_ADDR_LEN);
	printf(", VF %d VSI %d", sc->sc_vf_id, sc->sc_vsi_id);
}

static const struct iavf_link_speed *
iavf_find_link_speed(struct iavf_softc *sc, uint32_t link_speed)
{
	int i;
	for (i = 0; i < nitems(iavf_link_speeds); i++) {
		if (link_speed & (1 << i))
			return (&iavf_link_speeds[i]);
	}

	return (NULL);
}

static void
iavf_process_vc_event(struct iavf_softc *sc, struct iavf_aq_desc *desc,
    struct iavf_aq_buf *buf)
{
	struct iavf_vc_pf_event *event;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	const struct iavf_link_speed *speed;
	int link;

	event = buf->aqb_data;
	switch (event->event) {
	case IAVF_VC_EVENT_LINK_CHANGE:
		sc->sc_media_status = IFM_AVALID;
		sc->sc_media_active = IFM_ETHER;
		link = LINK_STATE_DOWN;
		if (event->link_status) {
			link = LINK_STATE_UP;
			sc->sc_media_status |= IFM_ACTIVE;

			ifp->if_baudrate = 0;
			speed = iavf_find_link_speed(sc, event->link_speed);
			if (speed != NULL) {
				sc->sc_media_active |= speed->media;
				ifp->if_baudrate = speed->baudrate;
			}
		}

		if (ifp->if_link_state != link) {
			ifp->if_link_state = link;
			if_link_state_change(ifp);
		}
		break;

	default:
		break;
	}
}

static void
iavf_process_irq_map(struct iavf_softc *sc, struct iavf_aq_desc *desc)
{
	if (letoh32(desc->iaq_vc_retval) != IAVF_VC_RC_SUCCESS) {
		printf("config irq map failed: %d\n", letoh32(desc->iaq_vc_retval));
	}
	sc->sc_got_irq_map = 1;
}

static int
iavf_process_arq(struct iavf_softc *sc)
{
	struct iavf_aq_desc *arq, *iaq;
	struct iavf_aq_buf *aqb;
	struct iavf_vc_version_info *ver;
	unsigned int cons = sc->sc_arq_cons;
	unsigned int prod;
	int done = 0;

	prod = iavf_rd(sc, sc->sc_aq_regs->arq_head) &
	    sc->sc_aq_regs->arq_head_mask;

	if (cons == prod)
		return (0);

	arq = IAVF_DMA_KVA(&sc->sc_arq);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_arq),
	    0, IAVF_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	do {
		iaq = &arq[cons];

		aqb = SIMPLEQ_FIRST(&sc->sc_arq_live);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_arq_live, aqb_entry);
		bus_dmamap_sync(sc->sc_dmat, aqb->aqb_map, 0, IAVF_AQ_BUFLEN,
		    BUS_DMASYNC_POSTREAD);

		switch (letoh32(iaq->iaq_vc_opcode)) {
		case IAVF_VC_OP_VERSION:
			ver = aqb->aqb_data;
			sc->sc_major_ver = letoh32(ver->major);
			sc->sc_minor_ver = letoh32(ver->minor);
			break;

		case IAVF_VC_OP_GET_VF_RESOURCES:
			iavf_process_vf_resources(sc, iaq, aqb);
			break;

		case IAVF_VC_OP_EVENT:
			iavf_process_vc_event(sc, iaq, aqb);
			break;

		case IAVF_VC_OP_CONFIG_IRQ_MAP:
			iavf_process_irq_map(sc, iaq);
			break;

		case IAVF_VC_OP_CONFIG_TX_QUEUE:
		case IAVF_VC_OP_CONFIG_RX_QUEUE:
		case IAVF_VC_OP_CONFIG_VSI_QUEUES:
		case IAVF_VC_OP_ENABLE_QUEUES:
		case IAVF_VC_OP_DISABLE_QUEUES:
		case IAVF_VC_OP_GET_RSS_HENA_CAPS:
		case IAVF_VC_OP_SET_RSS_HENA:
		case IAVF_VC_OP_ADD_ETH_ADDR:
		case IAVF_VC_OP_DEL_ETH_ADDR:
		case IAVF_VC_OP_CONFIG_PROMISC:
			sc->sc_admin_result = letoh32(iaq->iaq_vc_retval);
			cond_signal(&sc->sc_admin_cond);
			break;
		}

		memset(iaq, 0, sizeof(*iaq));
		SIMPLEQ_INSERT_TAIL(&sc->sc_arq_idle, aqb, aqb_entry);
		if_rxr_put(&sc->sc_arq_ring, 1);

		cons++;
		cons &= IAVF_AQ_MASK;

		done = 1;
	} while (cons != prod);

	if (done && iavf_arq_fill(sc))
		iavf_wr(sc, sc->sc_aq_regs->arq_tail, sc->sc_arq_prod);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_arq),
	    0, IAVF_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->sc_arq_cons = cons;
	return (done);
}

static void
iavf_arq_task(void *xsc)
{
	struct iavf_softc *sc = xsc;

	iavf_process_arq(sc);
	iavf_intr_enable(sc);
}

static void
iavf_atq_done(struct iavf_softc *sc)
{
	struct iavf_aq_desc *atq, *slot;
	unsigned int cons;
	unsigned int prod;

	prod = sc->sc_atq_prod;
	cons = sc->sc_atq_cons;

	if (prod == cons)
		return;

	atq = IAVF_DMA_KVA(&sc->sc_atq);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_atq),
	    0, IAVF_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	do {
		slot = &atq[cons];
		if (!ISSET(slot->iaq_flags, htole16(IAVF_AQ_DD)))
			break;

		memset(slot, 0, sizeof(*slot));

		cons++;
		cons &= IAVF_AQ_MASK;
	} while (cons != prod);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_atq),
	    0, IAVF_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->sc_atq_cons = cons;
}

static int
iavf_atq_post(struct iavf_softc *sc, struct iavf_aq_desc *iaq)
{
	struct iavf_aq_desc *atq, *slot;
	unsigned int prod;

	atq = IAVF_DMA_KVA(&sc->sc_atq);
	prod = sc->sc_atq_prod;
	slot = atq + prod;

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_atq),
	    0, IAVF_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_POSTWRITE);

	*slot = *iaq;
	slot->iaq_flags |= htole16(IAVF_AQ_SI);

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_atq),
	    0, IAVF_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_PREWRITE);

	prod++;
	prod &= IAVF_AQ_MASK;
	sc->sc_atq_prod = prod;
	iavf_wr(sc, sc->sc_aq_regs->atq_tail, prod);
	return (prod);
}

static int
iavf_get_version(struct iavf_softc *sc)
{
	struct iavf_aq_desc iaq;
	struct iavf_vc_version_info *ver;
	int tries;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_VERSION);
	iaq.iaq_datalen = htole16(sizeof(struct iavf_vc_version_info));
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	ver = IAVF_DMA_KVA(&sc->sc_scratch);
	ver->major = htole32(IAVF_VF_MAJOR);
	ver->minor = htole32(IAVF_VF_MINOR);
	sc->sc_major_ver = UINT_MAX;
	sc->sc_minor_ver = UINT_MAX;

	membar_sync();
	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_scratch), 0, IAVF_DMA_LEN(&sc->sc_scratch),
	    BUS_DMASYNC_PREREAD);

	iavf_atq_post(sc, &iaq);

	for (tries = 0; tries < 100; tries++) {
		iavf_process_arq(sc);
		if (sc->sc_major_ver != -1)
			break;

		delaymsec(1);
	}
	if (tries == 100) {
		printf(", timeout waiting for VF version");
		return (1);
	}

	if (sc->sc_major_ver != IAVF_VF_MAJOR) {
		printf(", unsupported VF version %d", sc->sc_major_ver);
		return (1);
	}

	printf(", VF version %d.%d%s", sc->sc_major_ver, sc->sc_minor_ver,
	    (sc->sc_minor_ver > IAVF_VF_MINOR) ? " (minor mismatch)" : "");

	return (0);
}

static int
iavf_get_vf_resources(struct iavf_softc *sc)
{
	struct iavf_aq_desc iaq;
	uint32_t *cap;
	int tries;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_GET_VF_RESOURCES);
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	if (sc->sc_minor_ver > 0) {
		iaq.iaq_datalen = htole16(sizeof(uint32_t));
		cap = IAVF_DMA_KVA(&sc->sc_scratch);
		*cap = htole32(IAVF_VC_OFFLOAD_L2 | IAVF_VC_OFFLOAD_VLAN |
		    IAVF_VC_OFFLOAD_RSS_PF);
	}

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_scratch), 0, IAVF_DMA_LEN(&sc->sc_scratch),
	    BUS_DMASYNC_PREREAD);

	sc->sc_got_vf_resources = 0;
	iavf_atq_post(sc, &iaq);

	for (tries = 0; tries < 100; tries++) {
		iavf_process_arq(sc);
		if (sc->sc_got_vf_resources != 0)
			break;

		delaymsec(1);
	}
	if (tries == 100) {
		printf(", timeout waiting for VF resources");
		return (1);
	}

	return (0);
}

static int
iavf_config_irq_map(struct iavf_softc *sc)
{
	struct iavf_aq_desc iaq;
	struct iavf_vc_vector_map *vec;
	struct iavf_vc_irq_map_info *map;
	int tries;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IAVF_AQ_BUF | IAVF_AQ_RD);
	iaq.iaq_opcode = htole16(IAVF_AQ_OP_SEND_TO_PF);
	iaq.iaq_vc_opcode = htole32(IAVF_VC_OP_CONFIG_IRQ_MAP);
	iaq.iaq_datalen = htole16(sizeof(*map) + sizeof(*vec));
	iavf_aq_dva(&iaq, IAVF_DMA_DVA(&sc->sc_scratch));

	map = IAVF_DMA_KVA(&sc->sc_scratch);
	map->num_vectors = letoh16(1);

	vec = map->vecmap;
	vec[0].vsi_id = letoh16(sc->sc_vsi_id);
	vec[0].vector_id = 0;
	vec[0].rxq_map = letoh16(iavf_allqueues(sc));
	vec[0].txq_map = letoh16(iavf_allqueues(sc));
	vec[0].rxitr_idx = IAVF_NOITR;
	vec[0].txitr_idx = IAVF_NOITR;

	bus_dmamap_sync(sc->sc_dmat, IAVF_DMA_MAP(&sc->sc_scratch), 0, IAVF_DMA_LEN(&sc->sc_scratch),
	    BUS_DMASYNC_PREREAD);

	sc->sc_got_irq_map = 0;
	iavf_atq_post(sc, &iaq);

	for (tries = 0; tries < 100; tries++) {
		iavf_process_arq(sc);
		if (sc->sc_got_irq_map != 0)
			break;

		delaymsec(1);
	}
	if (tries == 100) {
		printf(", timeout waiting for IRQ map response");
		return (1);
	}

	return (0);
}

static struct iavf_aq_buf *
iavf_aqb_alloc(struct iavf_softc *sc)
{
	struct iavf_aq_buf *aqb;

	aqb = malloc(sizeof(*aqb), M_DEVBUF, M_WAITOK);
	if (aqb == NULL)
		return (NULL);

	aqb->aqb_data = dma_alloc(IAVF_AQ_BUFLEN, PR_WAITOK);
	if (aqb->aqb_data == NULL)
		goto free;

	if (bus_dmamap_create(sc->sc_dmat, IAVF_AQ_BUFLEN, 1,
	    IAVF_AQ_BUFLEN, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
	    &aqb->aqb_map) != 0)
		goto dma_free;

	if (bus_dmamap_load(sc->sc_dmat, aqb->aqb_map, aqb->aqb_data,
	    IAVF_AQ_BUFLEN, NULL, BUS_DMA_WAITOK) != 0)
		goto destroy;

	return (aqb);

destroy:
	bus_dmamap_destroy(sc->sc_dmat, aqb->aqb_map);
dma_free:
	dma_free(aqb->aqb_data, IAVF_AQ_BUFLEN);
free:
	free(aqb, M_DEVBUF, sizeof(*aqb));

	return (NULL);
}

static void
iavf_aqb_free(struct iavf_softc *sc, struct iavf_aq_buf *aqb)
{
	bus_dmamap_unload(sc->sc_dmat, aqb->aqb_map);
	bus_dmamap_destroy(sc->sc_dmat, aqb->aqb_map);
	dma_free(aqb->aqb_data, IAVF_AQ_BUFLEN);
	free(aqb, M_DEVBUF, sizeof(*aqb));
}

static int
iavf_arq_fill(struct iavf_softc *sc)
{
	struct iavf_aq_buf *aqb;
	struct iavf_aq_desc *arq, *iaq;
	unsigned int prod = sc->sc_arq_prod;
	unsigned int n;
	int post = 0;

	n = if_rxr_get(&sc->sc_arq_ring, IAVF_AQ_NUM);
	arq = IAVF_DMA_KVA(&sc->sc_arq);

	while (n > 0) {
		aqb = SIMPLEQ_FIRST(&sc->sc_arq_idle);
		if (aqb != NULL)
			SIMPLEQ_REMOVE_HEAD(&sc->sc_arq_idle, aqb_entry);
		else if ((aqb = iavf_aqb_alloc(sc)) == NULL)
			break;

		memset(aqb->aqb_data, 0, IAVF_AQ_BUFLEN);

		bus_dmamap_sync(sc->sc_dmat, aqb->aqb_map, 0, IAVF_AQ_BUFLEN,
		    BUS_DMASYNC_PREREAD);

		iaq = &arq[prod];
		iaq->iaq_flags = htole16(IAVF_AQ_BUF |
		    (IAVF_AQ_BUFLEN > I40E_AQ_LARGE_BUF ? IAVF_AQ_LB : 0));
		iaq->iaq_opcode = 0;
		iaq->iaq_datalen = htole16(IAVF_AQ_BUFLEN);
		iaq->iaq_retval = 0;
		iaq->iaq_vc_opcode = 0;
		iaq->iaq_vc_retval = 0;
		iaq->iaq_param[0] = 0;
		iaq->iaq_param[1] = 0;
		iavf_aq_dva(iaq, aqb->aqb_map->dm_segs[0].ds_addr);

		SIMPLEQ_INSERT_TAIL(&sc->sc_arq_live, aqb, aqb_entry);

		prod++;
		prod &= IAVF_AQ_MASK;

		post = 1;

		n--;
	}

	if_rxr_put(&sc->sc_arq_ring, n);
	sc->sc_arq_prod = prod;

	return (post);
}

static void
iavf_arq_unfill(struct iavf_softc *sc)
{
	struct iavf_aq_buf *aqb;

	while ((aqb = SIMPLEQ_FIRST(&sc->sc_arq_live)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_arq_live, aqb_entry);

		bus_dmamap_sync(sc->sc_dmat, aqb->aqb_map, 0, IAVF_AQ_BUFLEN,
		    BUS_DMASYNC_POSTREAD);
		iavf_aqb_free(sc, aqb);
	}
}

static void
iavf_arq_timeout(void *xsc)
{
	struct iavf_softc *sc = xsc;

	sc->sc_admin_result = -1;
	cond_signal(&sc->sc_admin_cond);
}

static int
iavf_arq_wait(struct iavf_softc *sc, int msec)
{
	cond_init(&sc->sc_admin_cond);

	timeout_add_msec(&sc->sc_admin_timeout, msec);

	cond_wait(&sc->sc_admin_cond, "iavfarq");
	timeout_del(&sc->sc_admin_timeout);

	return sc->sc_admin_result;
}

static int
iavf_dmamem_alloc(struct iavf_softc *sc, struct iavf_dmamem *ixm,
    bus_size_t size, u_int align)
{
	ixm->ixm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, ixm->ixm_size, 1,
	    ixm->ixm_size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
	    &ixm->ixm_map) != 0)
		return (1);
	if (bus_dmamem_alloc(sc->sc_dmat, ixm->ixm_size,
	    align, 0, &ixm->ixm_seg, 1, &ixm->ixm_nsegs,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;
	if (bus_dmamem_map(sc->sc_dmat, &ixm->ixm_seg, ixm->ixm_nsegs,
	    ixm->ixm_size, &ixm->ixm_kva, BUS_DMA_WAITOK) != 0)
		goto free;
	if (bus_dmamap_load(sc->sc_dmat, ixm->ixm_map, ixm->ixm_kva,
	    ixm->ixm_size, NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	return (0);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, ixm->ixm_kva, ixm->ixm_size);
free:
	bus_dmamem_free(sc->sc_dmat, &ixm->ixm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, ixm->ixm_map);
	return (1);
}

static void
iavf_dmamem_free(struct iavf_softc *sc, struct iavf_dmamem *ixm)
{
	bus_dmamap_unload(sc->sc_dmat, ixm->ixm_map);
	bus_dmamem_unmap(sc->sc_dmat, ixm->ixm_kva, ixm->ixm_size);
	bus_dmamem_free(sc->sc_dmat, &ixm->ixm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, ixm->ixm_map);
}
