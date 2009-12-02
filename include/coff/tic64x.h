/* Copyright gpl blah blah */

#ifndef COFF_TIC64X_H
#define COFF_TIC64X_H

/* Some ti coff documentation in spraao8 */

#define TIC64X_TARGET_ID			0x99
#define OCTETS_PER_BYTE_POWER 			1
#define TICOFF_TARGET_ARCH			bfd_arch_tic64x
#define TICOFF_DEFAULT_MAGIC			TICOFF2MAGIC
#define TI_TARGET_ID				TIC64X_TARGET_ID

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

#include "coff/ti.h"

/* c6000 uses 10 byte relocation struct (although its named v0): */
#undef RELOC
#undef RELSZ
#define RELOC struct external_reloc_v0
#define RELSZ RELSZ_V0

#endif /* COFF_TIC64X_H */
