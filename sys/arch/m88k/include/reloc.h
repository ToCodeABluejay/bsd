/*	$OpenBSD: reloc.h,v 1.1 2013/01/05 11:20:56 miod Exp $	*/

/* public domain */

#ifndef	_M88K_RELOC_H_
#define	_M88K_RELOC_H_

#define	RELOC_NONE		0
#define	RELOC_COPY		1
#define	RELOC_GOTP_ENT		2
#define	RELOC_8			4
#define	RELOC_8S		5
#define	RELOC_16S		7
#define	RELOC_DISP16		8
#define	RELOC_DISP26		10
#define	RELOC_PLT_DISP26	14
#define	RELOC_BBASED_32		16
#define	RELOC_BBASED_32UA	17
#define	RELOC_BBASED_16H	18
#define	RELOC_BBASED_16L	19
#define	RELOC_ABDIFF_32		24
#define	RELOC_ABDIFF_32UA	25
#define	RELOC_ABDIFF_16H	26
#define	RELOC_ABDIFF_16L	27
#define	RELOC_ABDIFF_16		28
#define	RELOC_32		32
#define	RELOC_32UA		33
#define	RELOC_16H		34
#define	RELOC_16L		35
#define	RELOC_16		36
#define	RELOC_GOT_32		40
#define	RELOC_GOT_32UA		41
#define	RELOC_GOT_16H		42
#define	RELOC_GOT_16L		43
#define	RELOC_GOT_16		44
#define	RELOC_GOTP_32		48
#define	RELOC_GOTP_32UA		49
#define	RELOC_GOTP_16H		50
#define	RELOC_GOTP_16L		51
#define	RELOC_GOTP_16		52
#define	RELOC_PLT_32		56
#define	RELOC_PLT_32UA		57
#define	RELOC_PLT_16H		58
#define	RELOC_PLT_16L		59
#define	RELOC_PLT_16		60
#define	RELOC_ABREL_32		64
#define	RELOC_ABREL_32UA	65
#define	RELOC_ABREL_16H		66
#define	RELOC_ABREL_16L		67
#define	RELOC_ABREL_16		68
#define	RELOC_GOT_ABREL_32	72
#define	RELOC_GOT_ABREL_32UA	73
#define	RELOC_GOT_ABREL_16H	74
#define	RELOC_GOT_ABREL_16L	75
#define	RELOC_GOT_ABREL_16	76
#define	RELOC_GOTP_ABREL_32	80
#define	RELOC_GOTP_ABREL_32UA	81
#define	RELOC_GOTP_ABREL_16H	82
#define	RELOC_GOTP_ABREL_16L	83
#define	RELOC_GOTP_ABREL_16	84
#define	RELOC_PLT_ABREL_32	88
#define	RELOC_PLT_ABREL_32UA	89
#define	RELOC_PLT_ABREL_16H	90
#define	RELOC_PLT_ABREL_16L	91
#define	RELOC_PLT_ABREL_16	92
#define	RELOC_SREL_32		96
#define	RELOC_SREL_32UA		97
#define	RELOC_SREL_16H		98
#define	RELOC_SREL_16L		99

#endif	/* _M88K_RELOC_H_ */
