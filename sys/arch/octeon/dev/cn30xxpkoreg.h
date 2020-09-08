/*
 * THIS FILE IS AUTOMATICALLY GENERATED
 * DONT EDIT THIS FILE
 */

/*	$OpenBSD: cn30xxpkoreg.h,v 1.2 2020/09/08 13:54:48 visa Exp $	*/

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

/*
 * Cavium Networks OCTEON CN30XX Hardware Reference Manual
 * CN30XX-HM-1.0
 * 8.9 PKO Registers
 */

#ifndef _CN30XXPKOREG_H_
#define _CN30XXPKOREG_H_

#define	PKO_REG_FLAGS				0x0001180050000000ULL
#define	PKO_REG_READ_IDX			0x0001180050000008ULL
#define	PKO_REG_CMD_BUF				0x0001180050000010ULL
#define	PKO_REG_GMX_PORT_MODE			0x0001180050000018ULL
#define	PKO_REG_QUEUE_MODE			0x0001180050000048ULL
#define	PKO_REG_BIST_RESULT			0x0001180050000050ULL
#define	PKO_REG_ERROR				0x0001180050000058ULL
#define	PKO_REG_INT_MASK			0x0001180050000090ULL
#define	PKO_REG_DEBUG0				0x0001180050000098ULL
#define	PKO_MEM_QUEUE_PTRS			0x0001180050001000ULL
#define	PKO_MEM_QUEUE_QOS			0x0001180050001008ULL
#define	PKO_MEM_COUNT0				0x0001180050001080ULL
#define	PKO_MEM_COUNT1				0x0001180050001088ULL
#define	PKO_DEBUG0				0x0001180050001100ULL
#define	PKO_DEBUG1				0x0001180050001108ULL
#define	PKO_DEBUG2				0x0001180050001110ULL
#define	PKO_DEBUG3				0x0001180050001118ULL
#define	PKO_DEBUG4				0x0001180050001120ULL
#define	PKO_DEBUG5				0x0001180050001128ULL
#define	PKO_DEBUG6				0x0001180050001130ULL
#define	PKO_DEBUG7				0x0001180050001138ULL
#define	PKO_DEBUG8				0x0001180050001140ULL
#define	PKO_DEBUG9				0x0001180050001148ULL
#define	PKO_DEBUG10				0x0001180050001150ULL
#define	PKO_DEBUG11				0x0001180050001158ULL
#define	PKO_DEBUG12				0x0001180050001160ULL
#define	PKO_DEBUG13				0x0001180050001168ULL
#define	PKO_DEBUG14				0x0001180050001170ULL

#define PKO_BASE				0x0001180050000000ULL
#define	PKO_SIZE				0x01178ULL

#define PKO_REG_FLAGS_OFFSET			0x00000ULL
#define PKO_REG_READ_IDX_OFFSET			0x00008ULL
#define	PKO_REG_CMD_BUF_OFFSET			0x00010ULL
#define	PKO_REG_GMX_PORT_MODE_OFFSET		0x00018ULL
#define	PKO_REG_QUEUE_MODE_OFFSET		0x00048ULL
#define	PKO_REG_BIST_RESULT_OFFSET		0x00080ULL
#define	PKO_REG_ERROR_OFFSET			0x00088ULL
#define	PKO_REG_INT_MASK_OFFSET			0x00090ULL
#define	PKO_REG_DEBUG0_OFFSET			0x00098ULL
#define	PKO_MEM_QUEUE_PTRS_OFFSET		0x01000ULL
#define	PKO_MEM_QUEUE_QOS_OFFSET		0x01008ULL
#define	PKO_MEM_PORT_PTRS_OFFSET		0x01010ULL
#define	PKO_MEM_COUNT0_OFFSET			0x01080ULL
#define	PKO_MEM_COUNT1_OFFSET			0x01088ULL
#define	PKO_MEM_DEBUG0_OFFSET			0x01100ULL
#define	PKO_MEM_DEBUG1_OFFSET			0x01108ULL
#define	PKO_MEM_DEBUG2_OFFSET			0x01110ULL
#define	PKO_MEM_DEBUG3_OFFSET			0x01118ULL
#define	PKO_MEM_DEBUG4_OFFSET			0x01120ULL
#define	PKO_MEM_DEBUG5_OFFSET			0x01128ULL
#define	PKO_MEM_DEBUG6_OFFSET			0x01130ULL
#define	PKO_MEM_DEBUG7_OFFSET			0x01138ULL
#define	PKO_MEM_DEBUG8_OFFSET			0x01140ULL
#define	PKO_MEM_DEBUG9_OFFSET			0x01148ULL
#define	PKO_MEM_DEBUG10_OFFSET			0x01150ULL
#define	PKO_MEM_DEBUG11_OFFSET			0x01158ULL
#define	PKO_MEM_DEBUG12_OFFSET			0x01160ULL
#define	PKO_MEM_DEBUG13_OFFSET			0x01168ULL
#define	PKO_MEM_DEBUG14_OFFSET			0x01170ULL

/*
 * PKO_REG_FLAGS
 */
#define PKO_REG_FLAGS_63_7		0xfffffffffffffff0ULL
#define PKO_REG_FLAGS_RESET		0x0000000000000008ULL
#define PKO_REG_FLAGS_STORE_BE		0x0000000000000004ULL
#define PKO_REG_FLAGS_ENA_DWB		0x0000000000000002ULL
#define PKO_REG_FLAGS_ENA_PKO		0x0000000000000001ULL

/*
 * PKO_REG_READ_IDX
 */
#define PKO_REG_READ_IDX_63_16		0xffffffffffff0000ULL
#define PKO_REG_READ_IDX_INC		0x000000000000ff00ULL
#define PKO_REG_READ_IDX_IDX		0x00000000000000ffULL

/*
 * PKO_REG_CMD_BUF
 */
#define PKO_REG_CMD_BUF_63_23		0xffffffffff800000ULL
#define PKO_REG_CMD_BUF_POOL		0x0000000000700000ULL
#define PKO_REG_CMD_BUF_19_13		0x00000000000fe000ULL
#define PKO_REG_CMD_BUF_SIZE		0x0000000000001fffULL

/*
 * PKO_REG_GMX_PORT_MODE
 */
#define PKO_REG_GMX_PORT_MODE_63_6	0xffffffffffffffc0ULL
#define PKO_REG_GMX_PORT_MODE_MODE1	0x0000000000000038ULL
#define PKO_REG_GMX_PORT_MODE_MODE0	0x0000000000000007ULL

/*
 * PKO_REG_QUEUE_MODE
 */
#define PKO_REG_QUEUE_MODE_63_6		0xfffffffffffffffcULL
#define PKO_REG_QUEUE_MODE_MODE		0x000000000000000eULL

/*
 * PKO_REG_BIST_RESULT
 */
#define PKO_REG_BIST_RESULT_63_27	0xfffffffff8000000ULL
#define PKO_REG_BIST_RESULT_PSB2	0x0000000007c00000ULL
#define PKO_REG_BIST_RESULT_COUNT	0x0000000000200000ULL
#define PKO_REG_BIST_RESULT_RIF		0x0000000000100000ULL
#define PKO_REG_BIST_RESULT_WIF		0x0000000000080000ULL
#define PKO_REG_BIST_RESULT_NCB		0x0000000000040000ULL
#define PKO_REG_BIST_RESULT_OUT		0x0000000000020000ULL
#define PKO_REG_BIST_RESULT_CRC		0x0000000000010000ULL
#define PKO_REG_BIST_RESULT_CHK		0x0000000000008000ULL
#define PKO_REG_BIST_RESULT_QSB		0x0000000000006000ULL
#define PKO_REG_BIST_RESULT_QCB		0x0000000000001800ULL
#define PKO_REG_BIST_RESULT_PDB		0x0000000000000780ULL
#define PKO_REG_BIST_RESULT_PSB		0x000000000000007fULL

/*
 * PKO_REG_ERROR
 */
#define PKO_REG_ERROR_63_2		0xfffffffffffffffcULL
#define PKO_REG_ERROR_DOORBELL		0x0000000000000002ULL
#define PKO_REG_ERROR_PARITY		0x0000000000000001ULL

/*
 * PKO_REG_INT_MASK
 */
#define PKO_REG_INT_MASK_63_2		0xfffffffffffffffcULL
#define PKO_REG_INT_MASK_DOORBELL	0x0000000000000002ULL
#define PKO_REG_INT_MASK_PARITY		0x0000000000000001ULL

/*
 * PKO_REG_DEBUG0
 */
#define PKO_REG_DEBUG0_63_17		0xfffffffffffe0000ULL
#define PKO_REG_DEBUG0_ASSERTS		0x000000000001ffffULL

/*
 * PKO_MEM_QUEUE_PTRS
 */
#define PKO_MEM_QUEUE_PTRS_S_TAIL	0x8000000000000000ULL
#define PKO_MEM_QUEUE_PTRS_STATIC_P	0x4000000000000000ULL
#define PKO_MEM_QUEUE_PTRS_STATIC_Q	0x2000000000000000ULL
#define PKO_MEM_QUEUE_PTRS_QOS_MASK	0x1fe0000000000000ULL
#define PKO_MEM_QUEUE_PTRS_BUF_PTR	0x001ffffffffe0000ULL
#define PKO_MEM_QUEUE_PTRS_TAIL		0x0000000000010000ULL
#define PKO_MEM_QUEUE_PTRS_IDX		0x000000000000e000ULL
#define PKO_MEM_QUEUE_PTRS_PID		0x0000000000001f80ULL
#define PKO_MEM_QUEUE_PTRS_QID		0x000000000000007fULL

/*
 * PKO_MEM_QUEUE_QOS
 */
#define PKO_MEM_QUEUE_QOS_63_61		0xe000000000000000ULL
#define PKO_MEM_QUEUE_QOS_QOS_MASK	0x1fe0000000000000ULL
#define PKO_MEM_QUEUE_QOS_52_13		0x001fffffffffe000ULL
#define PKO_MEM_QUEUE_QOS_PID		0x0000000000001f80ULL
#define PKO_MEM_QUEUE_QOS_QID		0x000000000000007fULL

/*
 * PKO_MEM_PORT_PTRS
 */
#define PKO_MEM_PORT_PTRS_BP_PORT_M	0x000000000000fc00ULL
#define PKO_MEM_PORT_PTRS_BP_PORT_S	10
#define PKO_MEM_PORT_PTRS_EID_M		0x00000000000003c0ULL
#define PKO_MEM_PORT_PTRS_EID_S		6
#define PKO_MEM_PORT_PTRS_PID_M		0x000000000000003fULL

/*
 * PKO_MEM_COUNT0
 */
#define PKO_MEM_COUNT0_63_32		0xffffffff00000000ULL
#define PKO_MEM_COUNT0_COUNT		0x00000000ffffffffULL

/*
 * PKO_MEM_COUNT1
 */
#define PKO_MEM_COUNT1_63_48		0xffff000000000000ULL
#define PKO_MEM_COUNT1_COUNT		0x0000ffffffffffffULL

/*
 * PKO_MEM_DEBUG0
 */
#define PKO_MEM_DEBUG0_FAU		0xfffffff000000000ULL
#define PKO_MEM_DEBUG0_CMD		0x0000000fffc00000ULL
#define PKO_MEM_DEBUG0_SEGS		0x00000000003f0000ULL
#define PKO_MEM_DEBUG0_SIZE		0x000000000000ffffULL

/*
 * PKO_MEM_DEBUG1
 */
#define PKO_MEM_DEBUG1_I		0x8000000000000000ULL
#define PKO_MEM_DEBUG1_BACK		0x7800000000000000ULL
#define PKO_MEM_DEBUG1_POOL		0x0700000000000000ULL
#define PKO_MEM_DEBUG1_SIZE		0x00ffff0000000000ULL
#define PKO_MEM_DEBUG1_PTR		0x000000ffffffffffULL

/*
 * PKO_MEM_DEBUG2
 */
#define PKO_MEM_DEBUG2_I		0x8000000000000000ULL
#define PKO_MEM_DEBUG2_BACK		0x7800000000000000ULL
#define PKO_MEM_DEBUG2_POOL		0x0700000000000000ULL
#define PKO_MEM_DEBUG2_SIZE		0x00ffff0000000000ULL
#define PKO_MEM_DEBUG2_PTR		0x000000ffffffffffULL

/*
 * PKO_MEM_DEBUG3
 */
#define PKO_MEM_DEBUG3_I		0x8000000000000000ULL
#define PKO_MEM_DEBUG3_BACK		0x7800000000000000ULL
#define PKO_MEM_DEBUG3_POOL		0x0700000000000000ULL
#define PKO_MEM_DEBUG3_SIZE		0x00ffff0000000000ULL
#define PKO_MEM_DEBUG3_PTR		0x000000ffffffffffULL

/*
 * PKO_MEM_DEBUG4
 */
#define PKO_MEM_DEBUG4_DATA		0xffffffffffffffffULL

/*
 * PKO_MEM_DEBUG5
 */
#define PKO_MEM_DEBUG5_DWRI_MOD			0x8000000000000000ULL
#define PKO_MEM_DEBUG5_DWRI_SOP			0x4000000000000000ULL
#define PKO_MEM_DEBUG5_DWRI_LEN			0x2000000000000000ULL
#define PKO_MEM_DEBUG5_DWRI_CNT			0x1fff000000000000ULL
#define PKO_MEM_DEBUG5_CMND_SIZ			0x0000ffff00000000ULL
#define PKO_MEM_DEBUG5_UID			0x0000000080000000ULL
#define PKO_MEM_DEBUG5_XFER_WOR			0x0000000040000000ULL
#define PKO_MEM_DEBUG5_XFER_DWR			0x0000000020000000ULL
#define PKO_MEM_DEBUG5_CBUF_FRE			0x0000000010000000ULL
#define PKO_MEM_DEBUG5_27			0x0000000008000000ULL
#define PKO_MEM_DEBUG5_CHK_MODE			0x0000000004000000ULL
#define PKO_MEM_DEBUG5_ACTIVE			0x0000000002000000ULL
#define PKO_MEM_DEBUG5_QOS			0x0000000001c00000ULL
#define PKO_MEM_DEBUG5_QCB_RIDX			0x00000000003e0000ULL
#define PKO_MEM_DEBUG5_QID_OFF			0x000000000001c000ULL
#define PKO_MEM_DEBUG5_QID_BASE			0x0000000000003f80ULL
#define PKO_MEM_DEBUG5_WAIT			0x0000000000000040ULL
#define PKO_MEM_DEBUG5_MINOR			0x0000000000000030ULL
#define PKO_MEM_DEBUG5_MAJOR			0x000000000000000fULL

/*
 * PKO_MEM_DEBUG6
 */
#define PKO_MEM_DEBUG6_63_11		0xfffffffffffff800ULL
#define PKO_MEM_DEBUG6_QID_OFFM		0x0000000000000700ULL
#define PKO_MEM_DEBUG6_STATIC_P		0x0000000000000080ULL
#define PKO_MEM_DEBUG6_WORK_MIN		0x0000000000000070ULL
#define PKO_MEM_DEBUG6_DWRI_CHK		0x0000000000000008ULL
#define PKO_MEM_DEBUG6_DWRI_UID		0x0000000000000004ULL
#define PKO_MEM_DEBUG6_DWRI_MOD		0x0000000000000003ULL

/*
 * PKO_MEM_DEBUG7
 */
#define PKO_MEM_DEBUG7_63_58		0xfc00000000000000ULL
#define PKO_MEM_DEBUG7_DWB		0x03fe000000000000ULL
#define PKO_MEM_DEBUG7_START		0x0001ffffffff0000ULL
#define PKO_MEM_DEBUG7_SIZE		0x000000000000ffffULL

/*
 * PKO_MEM_DEBUG8
 */
#define PKO_MEM_DEBUG8_QOS		0xf800000000000000ULL
#define PKO_MEM_DEBUG8_TAIL		0x0400000000000000ULL
#define PKO_MEM_DEBUG8_BUF_SIZ		0x03ffe00000000000ULL
#define PKO_MEM_DEBUG8_BUF_PTR		0x00001ffffffff000ULL
#define PKO_MEM_DEBUG8_QCB_WIDX		0x0000000000000fc0ULL
#define PKO_MEM_DEBUG8_QCB_RIDX		0x000000000000003fULL

/*
 * PKO_MEM_DEBUG9
 */
#define PKO_MEM_DEBUG9_63_28		0xfffffffff0000000ULL
#define PKO_MEM_DEBUG9_DOORBELL		0x000000000fffff00ULL
#define PKO_MEM_DEBUG9_7_5		0x00000000000000e0ULL
#define PKO_MEM_DEBUG9_S_TAIL		0x0000000000000010ULL
#define PKO_MEM_DEBUG9_STATIC_Q		0x0000000000000008ULL
#define PKO_MEM_DEBUG9_QOOS		0x0000000000000007ULL

/*
 * PKO_MEM_DEBUG10
 */
#define PKO_MEM_DEBUG10_FAU		0xfffffff000000000ULL
#define PKO_MEM_DEBUG10_CMD		0x0000000fffc00000ULL
#define PKO_MEM_DEBUG10_SEGS		0x00000000003f0000ULL
#define PKO_MEM_DEBUG10_SIZE		0x000000000000ffffULL

/*
 * PKO_MEM_DEBUG11
 */
#define PKO_MEM_DEBUG11_I		0x8000000000000000ULL
#define PKO_MEM_DEBUG11_BACK		0x7800000000000000ULL
#define PKO_MEM_DEBUG11_POOL		0x0700000000000000ULL
#define PKO_MEM_DEBUG11_SIZE		0x00ffff0000000000ULL
#define PKO_MEM_DEBUG11_PTR		0x000000ffffffffffULL

/*
 * PKO_MEM_DEBUG12
 */
#define PKO_MEM_DEBUG12_DATA		0xffffffffffffffffULL

/*
 * PKO_MEM_DEBUG13
 */
#define PKO_MEM_DEBUG13_63_51		0xfff8000000000000ULL
#define PKO_MEM_DEBUG13_WIDX		0x0007fffc00000000ULL
#define PKO_MEM_DEBUG13_RIDX2		0x00000003fffe0000ULL
#define PKO_MEM_DEBUG13_WIDX2		0x000000000001ffffULL

/*
 * PKO_MEM_DEBUG14
 */
#define PKO_MEM_DEBUG13_63_17		0xfffffffffffe0000ULL
#define PKO_MEM_DEBUG13_RIDX		0x000000000001ffffULL

/*
 * PKO_CMD_WORD0
 */
#define PKO_CMD_WORD0_SZ1		0xc000000000000000ULL
#define PKO_CMD_WORD0_SZ0		0x3000000000000000ULL
#define PKO_CMD_WORD0_S1		0x0800000000000000ULL
#define PKO_CMD_WORD0_REG1		0x07ff000000000000ULL
#define PKO_CMD_WORD0_S0		0x0000800000000000ULL
#define PKO_CMD_WORD0_REG0		0x00007ff000000000ULL
#define PKO_CMD_WORD0_LE		0x0000000800000000ULL
#define PKO_CMD_WORD0_N2		0x0000000400000000ULL
#define PKO_CMD_WORD0_Q			0x0000000200000000ULL
#define PKO_CMD_WORD0_R			0x0000000100000000ULL
#define PKO_CMD_WORD0_G			0x0000000080000000ULL
#define PKO_CMD_WORD0_IPOFFP1		0x000000007f000000ULL
#define PKO_CMD_WORD0_II		0x0000000000800000ULL
#define PKO_CMD_WORD0_DF		0x0000000000400000ULL
#define PKO_CMD_WORD0_SEGS		0x00000000003f0000ULL
#define PKO_CMD_WORD0_TOTALBYTES	0x000000000000ffffULL

/*
 * PKO_CMD_WORD1
 */
#define PKO_CMD_WORD1_I			0x8000000000000000ULL
#define PKO_CMD_WORD1_BACK		0x7800000000000000ULL
#define PKO_CMD_WORD1_POOL		0x0700000000000000ULL
#define PKO_CMD_WORD1_SIZE		0x00ffff0000000000ULL
#define PKO_CMD_WORD1_ADDR		0x000000ffffffffffULL

/*
 * PKO_CMD_WORD2
 */
#define PKO_CMD_WORD2_63_36		0xfffffff000000000ULL
#define PKO_CMD_WORD2_PTR		0x0000000fffffffffULL

/*
 *  DOORBELL_WRITE
 */
#define PKO_DOORBELL_WRITE_IO_BIT	0x0001000000000000ULL
#define PKO_DOORBELL_WRITE_MAJOR_DID	0x0000f80000000000ULL
#define PKO_DOORBELL_WRITE_SUB_DID	0x0000070000000000ULL
#define PKO_DOORBELL_WRITE_39_16	0x000000ffffff0000ULL
#define PKO_DOORBELL_WRITE_PID		0x000000000003f000ULL
#define PKO_DOORBELL_WRITE_QID		0x0000000000000ff8ULL
#define PKO_DOORBELL_WRITE_2_0		0x0000000000000007ULL
 
#define PKO_DOORBELL_WRITE_WDC		0x00000000000fffffULL

/* ---- operations */

#define	CN30XXPKO_MAJORDID	0x0a
#define	CN30XXPKO_SUBDID	0x02

#endif /* _CN30XXPKOREG_H_ */

