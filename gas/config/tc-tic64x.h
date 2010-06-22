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

#ifndef _TC_TIC64X_H_
#define _TC_TIC64X_H_

#include "write.h"

/* Set cpu for obj-coff.h */
#define TC_TIC64X

#define TARGET_BYTES_BIG_ENDIAN 0
#define OCTETS_PER_BYTE_POWER 0

#define TARGET_ARCH		bfd_arch_tic64x

/* Compiler won't (shouldn't) emit 16 bit displacements */
#define WORKING_DOT_WORD	0
/* Trailing h or b on constants */
#define NUMBERS_WITH_SUFFIX 1
/* "section program counter" according to tic54x */
#define DOLLAR_DOT 1

/* Enable the "||" specifier on a line to indicate parallel execution */
#define DOUBLEBAR_PARALLEL 1

/* AFAIK no need for special/dynamic operand names */
#define md_operand(x)

/* AFAIK everything we want to deal with will be treated little endianly */
#define md_number_to_chars(b,v,n) number_to_chars_littleendian(b,v,n)

void tic64x_start_line_hook(void);
#define md_start_line_hook() tic64x_start_line_hook()

/* We detect the end of an instruction packet in the start_line_hook right now,
 * but that can't detect the end of file and flush the current packet, so do
 * that with the after_pass hook. */
void md_after_pass_hook(void);
#define md_after_pass_hook md_after_pass_hook

/* For accurate pcrel calculations... */
long md_pcrel_from_seg(fixS *fixP, segT segment);
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_seg((FIX), (SEC))

#define TC_CONS_FIX_NEW(FRAG,OFF,LEN,EXP) tic64x_cons_fix(FRAG, OFF, LEN, EXP)
extern void tic64x_cons_fix (fragS *frag, unsigned int offset, unsigned int len,
							expressionS *exp);

#endif /* _TC_TIC64X_H_ */
