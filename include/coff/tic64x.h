/* Assembler for TMS320C64X class of processors
 * Copyright 2010 Jeremy Morse <jeremy.morse@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
 * MA 02110-1301, USA
 */

#ifndef COFF_TIC64X_H
#define COFF_TIC64X_H

/* Some ti coff documentation in spraao8 */

#define TIC64X_TARGET_ID			0x99
#define OCTETS_PER_BYTE_POWER 			0
#define TICOFF_TARGET_ARCH			bfd_arch_tic64x
#define TICOFF_DEFAULT_MAGIC			TICOFF2MAGIC
#define TI_TARGET_ID				TIC64X_TARGET_ID

/* Exactly how i386 definitions are leaking in here, I don't know */
#undef R_RELBYTE
#undef R_RELWORD
#undef R_RELLONG
#define R_RELBYTE				0x0F
#define R_RELWORD				0x10
#define R_RELLONG				0x11
#define R_C60BASE				0x50
#define R_C60DIR15				0x51
#define R_C60PCR21				0x52
#define R_C60PCR10				0x53
#define R_C60LO16				0x54
#define R_C60HI16				0x55
#define R_C60SECT				0x56
#define R_C60S16				0x57
#define R_C60PCR7				0x70
#define R_C60PCR12				0x71

#define RE_ADD					0x4000
#define RE_SUB					0x4001
#define RE_NEG					0x4002
#define RE_MPY					0x4003
#define RE_DIV					0x4004
#define RE_MOD					0x4005
#define RE_SR					0x4006
#define RE_ASR					0x4007
#define RE_SL					0x4008
#define RE_AND					0x4009
#define RE_OR					0x400A
#define RE_XOR					0x400B
#define RE_NOTB					0x400C
#define RE_ULDFLD				0x400D
#define RE_SLDFLD				0x400E
#define RE_USTFLD				0x400F
#define RE_SSTFLD				0x4010
#define RE_PUSH					0x4011
#define RE_PUSHSK				0x4012
#define RE_PUSHUK				0x4013
#define RE_PUSHPC				0x4014
#define RE_DUP					0x4015
#define RE_XSTFLD				0x4016
#define RE_PUSHSV				0xC011

#include "coff/ti.h"

/* c6000 uses 10 byte relocation struct (although its named v0): */
#undef RELOC
#undef RELSZ
#define external_reloc external_reloc_v0
#define RELOC struct external_reloc_v0
#define RELSZ RELSZ_V0

#endif /* COFF_TIC64X_H */
