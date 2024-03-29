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

#include <errno.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"

const struct tic64x_register tic64x_regs[] = {
/* General purpose registers */
	{"A0",	0},	{"A1",	1},	{"A2",	2},	{"A3",	3},
	{"A4",	4},	{"A5",	5},	{"A6",	6},	{"A7",	7},
	{"A8",	8},	{"A9",	9},	{"A10",	10},	{"A11",	11},
	{"A12",	12},	{"A13",	13},	{"A14",	14},	{"A15",	15},
	{"A16",	16},	{"A17",	17},	{"A18",	18},	{"A19",	19},
	{"A20",	20},	{"A21",	21},	{"A22",	22},	{"A23",	23},
	{"A24",	24},	{"A25",	25},	{"A26",	26},	{"A27",	27},
	{"A28",	28},	{"A29",	29},	{"A30",	30},	{"A31",	31},
	{"B0",	TIC64X_REG_UNIT2 | 0},	{"B1",	TIC64X_REG_UNIT2 | 1},
	{"B2",	TIC64X_REG_UNIT2 | 2},	{"B3",	TIC64X_REG_UNIT2 | 3},
	{"B4",	TIC64X_REG_UNIT2 | 4},	{"B5",	TIC64X_REG_UNIT2 | 5},
	{"B6",	TIC64X_REG_UNIT2 | 6},	{"B7",	TIC64X_REG_UNIT2 | 7},
	{"B8",	TIC64X_REG_UNIT2 | 8},	{"B9",	TIC64X_REG_UNIT2 | 9},
	{"B10",	TIC64X_REG_UNIT2 | 10},	{"B11",	TIC64X_REG_UNIT2 | 11},
	{"B12",	TIC64X_REG_UNIT2 | 12},	{"B13",	TIC64X_REG_UNIT2 | 13},
	{"B14",	TIC64X_REG_UNIT2 | 14},	{"B15",	TIC64X_REG_UNIT2 | 15},
	{"B16",	TIC64X_REG_UNIT2 | 16},	{"B17",	TIC64X_REG_UNIT2 | 17},
	{"B18",	TIC64X_REG_UNIT2 | 18},	{"B19",	TIC64X_REG_UNIT2 | 19},
	{"B20",	TIC64X_REG_UNIT2 | 20},	{"B21",	TIC64X_REG_UNIT2 | 21},
	{"B22",	TIC64X_REG_UNIT2 | 22},	{"B23",	TIC64X_REG_UNIT2 | 23},
	{"B24",	TIC64X_REG_UNIT2 | 24},	{"B25",	TIC64X_REG_UNIT2 | 25},
	{"B26",	TIC64X_REG_UNIT2 | 26},	{"B27",	TIC64X_REG_UNIT2 | 27},
	{"B28",	TIC64X_REG_UNIT2 | 28},	{"B29",	TIC64X_REG_UNIT2 | 29},
	{"B30",	TIC64X_REG_UNIT2 | 30},	{"B31",	TIC64X_REG_UNIT2 | 31},
/* Control registers, accessed via 'mvc' instruction */
	{"AMR",	TIC64X_CTRL_REG | 0},	{"CSR", TIC64X_CTRL_REG | 1},
	{"DIER",TIC64X_CTRL_REG | 25},	{"DNUM",TIC64X_CTRL_REG | 17},
	{"ECR", TIC64X_CTRL_REG | 29},	{"EFR", TIC64X_CTRL_REG | 29},/*same?*/
	{"GFPGFR", TIC64X_CTRL_REG |24},{"GPLYA",TIC64X_CTRL_REG | 22},
	{"GPLYB", TIC64X_CTRL_REG |23},	{"ICR", TIC64X_CTRL_REG | 3},
	{"IER", TIC64X_CTRL_REG | 4},	{"IERR",TIC64X_CTRL_REG | 31},
	{"IFR", TIC64X_CTRL_REG | 2},	{"ILC", TIC64X_CTRL_REG | 13},
	{"IRP", TIC64X_CTRL_REG | 6},	{"ISR", TIC64X_CTRL_REG | 2},/* IFR? */
	{"ISTP",TIC64X_CTRL_REG | 5},	{"ISTR",TIC64X_CTRL_REG | 27},
	{"NRP", TIC64X_CTRL_REG | 7},	{"NTSR",TIC64X_CTRL_REG | 28},
	{"PCE1",TIC64X_CTRL_REG | 16},	{"REP", TIC64X_CTRL_REG | 15},
	{"RILC",TIC64X_CTRL_REG | 14},	{"SSR", TIC64X_CTRL_REG | 21},
	{"TSCH",TIC64X_CTRL_REG | 11},	{"TSCL",TIC64X_CTRL_REG | 10},
	{"TSR", TIC64X_CTRL_REG | 26},	{NULL,	0}
};

const struct tic64x_operand_pos tic64x_operand_positions [] = {
/* Note - this array indexed by tic64x_operand_type, if ordering is changed
 * here, you need to change that enum definition */
{	0,	0	},	/* invalid */
{	9,	4	},	/* addrmode */
{	23,	5	},	/* dstreg */
{	24,	4	},	/* dwdst4 */
{	23,	5	},	/* dwdst5 */
{	18,	5	},	/* basereg */
{	13,	5	},	/* rcoffset */
{	23,	1	},	/* scale */
{	12,	1	},	/* x */
{	13,	5	},	/* const5 */
{	18,	5	},	/* const5p2 */
{	7,	21	},	/* const21 */
{	7,	16	},	/* const16 */
{	8,	15	},	/* const15 */
{	16,	12	},	/* const12 */
{	13,	10	},	/* const10 */
{	16,	7	},	/* const7 */
{	13,	4	},	/* const4 */
{	8,	5	},	/* bitfldb */
{	13,	3	},	/* nops */
{	13,	5	},	/* srcreg1 */
{	18,	5	},	/* srcreg2 */
{	18,	5	},	/* dwsrc */
{	0,	1	},	/* p */
{	1,	1	},	/* s */
{	7,	1	},	/* y */
{	28,	1	},	/* z */
{	29,	3	},	/* creg */
{	0,	8	},	/* tic64x_operand_data8 */
{	0,	16	},	/* tic64x_operand_data16 */
{	0,	32	}	/* tic64x_operand_data32 */
};

/* Returns zero on success, nonzero if too large for field. Always actually
 * sets operand, as some code doesn't care about data loss here */
void
tic64x_set_operand(uint32_t *op, enum tic64x_operand_type type, int value)
{

	value &= ((1 << tic64x_operand_positions[type].size) - 1);
	value <<= tic64x_operand_positions[type].position;
	*op |= value;
	return;
}

static int
get_operand(uint32_t opcode, int position, int size, int sx)
{
	int sign_extended;

	opcode >>= position;
	opcode &= ((1 << size) - 1);

	if (!sx)
		return opcode;

	/* Sign extend */
	if (opcode & (1 << (size - 1))) {
		sign_extended = ~opcode;
		sign_extended &= ((1 << size) - 1);
		sign_extended += 1;
		sign_extended = -sign_extended;
	} else {
		sign_extended = opcode;
	}

	return sign_extended;
}

int
tic64x_get_operand(uint32_t opcode,  enum tic64x_operand_type t, int signx)
{

	return get_operand(opcode, tic64x_operand_positions[t].position,
				tic64x_operand_positions[t].size, signx);
}

/* NB: When transcribing add instructions, I may have missed some of the ti
 * comments about which way around operands are (ie, src1 and src2). Doesn't
 * matter functionally because it's add, but this may need to be fixed in the
 * future */
const struct tic64x_op_template tic64x_opcodes[] = {
{"abs",		0x358,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"abs",		0x718,		0x3EFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dwsrc, tic64x_optxt_dwdst, tic64x_optxt_none },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"abs2",	0x83580,	0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add",		0x78,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add",		0x478,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst},
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"add",		0x438,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"add",		0x58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_sconstant, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"add",		0x418,		0xFFC,
	TIC64X_OP_UNIT_L, /* no xpath today */
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{ tic64x_operand_dwdst5, tic64x_operand_const5 }
},
{"add",		0x1E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add",		0x1A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* Caution - opcode map field reverses in ti spec for some reason
 * for these two ops */
{"add",		0x840,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add",		0x940,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_uconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"add",		0xAB0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add",		0xAF0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"addab",	0x1840,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"addab",	0x1940,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* XXX addab form on p 107 is too hacky for us right now */
{"addad",	0x1E40,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"addad", 	0x1EC0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"addah",	0x1A40,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"addah",	0x1B40,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* XXX addah form on p 107 is too hacky for us right now */
{"addaw",	0x1C40,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"addaw",	0x1D40,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* XXX addaw form on p 107 is too hacky for us right now */
{"addk",	0x50,		0x7C,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const16, tic64x_operand_invalid }
},
{"addkpc",	0x160,		0x1FFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2 |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD | TIC64X_OP_CONST_PCREL,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_uconstant },
	{ tic64x_operand_const7, tic64x_operand_invalid }
},
/* XXX - addsub instruction counts as being hacky, as does addsub2 */
{"addu",	0x578,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"addu",	0x538,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"add2",	0x60,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add2",	0xB8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add2",	0x930,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"add4",	0xCB8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"and",		0xF78,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"and",		0xF58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"and",		0x7E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"and",		0x7A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"and",		0x9B0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"and",		0x9F0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"andn",	0xF98,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"andn",	0xDB0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"andn",	0x830,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"avg2",	0x4F0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"avgu4",	0x4B0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* This file is alphabetically ordered except for callp; unfortunately it's
 * not possible to distinguish between it and "b" using just the lower bits,
 * so we need to test the more specific pattern first. Bees */
{"callp",	0x10000010,	0xF000007C,
	TIC64X_OP_UNIT_S | TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_PCREL | TIC64X_OP_NOCOND,
	{ tic64x_optxt_sconstant, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_const21, tic64x_operand_invalid }
},
{"b",		0x10,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_PCREL | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_none, tic64x_optxt_none},
	{ tic64x_operand_const21, tic64x_operand_invalid }
},
{"b",		0x360,		0x0F83EFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2 |
	TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_none, tic64x_optxt_none},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* B IRP - we have no way of naming a _specific_ register that the instruction
 * must match against (why would we?), so instead the enforcement that this is
 * IRP will occur in the register validator as a special case. Unpleasent, but
 * better than hacking something together to allow only matching against named
 * registers, which will happen no-where else. */
{"b",		0x1800E0,	0xFFFFFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_ctrlreg, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* B NRP - same story as B IRP */
{"b",		0x1C00E0,	0xFFFFFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_ctrlreg, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"bdec",	0x1020,		0x1FFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_PCREL,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const10, tic64x_operand_invalid }
},
{"bitc4",	0x3C0F0,	0x3EFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"bitr",	0x3E0F0,	0x3EFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"bnop",	0x120,		0x1FFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_PCREL | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_nops, tic64x_optxt_none },
	{ tic64x_operand_const12, tic64x_operand_nops }
},
{"bnop",	0x800362,	0x0F800FFE,
	TIC64X_OP_UNIT_S | TIC64X_OP_NOSIDE | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_nops, tic64x_optxt_none },
	{ tic64x_operand_nops, tic64x_operand_invalid }
},
{"bpos",	0x20,		0x1FFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_PCREL,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const10, tic64x_operand_invalid }
},
{"clr",		0xC8,		0xFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_BITFIELD,
	{ tic64x_optxt_srcreg2, tic64x_optxt_bfield, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"clr",		0xFE0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpeq",	0xA78,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpeq",	0xA58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"cmpeq",	0xA38,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpeq",	0xA18,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"cmpeq2",	0x760,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpeq4",	0x720,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpgt",	0x8F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpgt",	0x8D8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"cmpgt",	0x8B8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpgt",	0x898,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"cmpgt2",	0x520,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpgtu",	0x9F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpgtu",	0x9D8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_uconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const4, tic64x_operand_invalid }
},
{"cmpgtu",	0x9B8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpgtu",	0x998,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_uconstant, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_const4, tic64x_operand_invalid }
},
{"cmpgtu4",	0x560,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmplt",	0xAF8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmplt",	0xAD8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"cmplt",	0xAB8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmplt",	0xA98,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* Danger Will Robinson: this is identical to cmpgt (by TI design), just with
 * the optxt operand order swapped */
{"cmplt2",	0x520,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpltu",	0xBF8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpltu",	0xBD8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_uconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const4, tic64x_operand_invalid }
},
{"cmpltu",	0xBB8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpltu",	0xB98,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_uconstant, tic64x_optxt_dwsrc, tic64x_optxt_dstreg },
	{ tic64x_operand_const4, tic64x_operand_invalid }
},
/* Danger Will Robinson: cmpltu4 is actually cmpgtu4 with swapped text ops */
{"cmpltu4",	0x560,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpy",	0x100002B0,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"cmpyr",	0x100002F0,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"cmpyr1",	0x10000330,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX - not using cmtl for now, has slightly odd absolute pointer situation,
 * also is c64x+, also is atomic and so unlikely to be used */
{"ddotp4",	0x10000630,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"ddotph2",	0x100005F0,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_dwsrc2, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"ddotph2r",	0x10000570,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_dwsrc2, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"ddotpl2",	0x100005B0,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_dwsrc2, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"ddotpl2r",	0x10000530,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_dwsrc2, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"deal",	0x3A0F0,	0x3EFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* This isn't really an instruction that's useful right now, but it's simple
 * to type in and doesn't require hacks, so what the hell. */
{"dint",	0x10004000,	0xFFFFFFFC,
	TIC64X_OP_NOCOND | TIC64X_OP_NOSIDE | TIC64X_OP_ALL_UNITS,
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"dmv",		0xEF0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"dotp2",	0x330,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"dotp2",	0x2F0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"dotpn2",	0x270,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"dotpnrsu2",	0x1F0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* Danger Will Robinson: dotpnrus2 is dotpnrsu2 with the operands swapped. */
{"dotpnrus2",	0x1F0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"dotprsu2",	0x370,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* Again; the below and above insns are the same */
{"dotprus2", 	0x370,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"dotpsu4",	0xB0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* Again; above and below are the same, swapped operands */
{"dotpus4",	0xB0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"dotpu4",	0x1B0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"dpack2",	0x10000698,	0xF0800FFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst4, tic64x_operand_invalid }
},
{"dpackx2",	0x10000678,	0xF0800FFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst4, tic64x_operand_invalid }
},
{"ext",		0x48,		0xFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_BITFIELD | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_bfield, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"ext",		0xBE0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"extu",	0x8,		0xFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_BITFIELD | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_bfield, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"extu",	0xAE0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* Do we want or need Galois field instructions? Not right now */
{"idle",	0x1E000,	0x3FFFC,
	TIC64X_OP_NOCOND,
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"ldb",		0x24,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_BYTE |
	TIC64X_OP_UNITNO | TIC64X_OP_CONST_SCALE | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"ldb",		0x2C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_BYTE |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
{"ldbu",	0x14,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_BYTE |
	TIC64X_OP_UNITNO | TIC64X_OP_CONST_SCALE | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"ldbu",	0x1C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_BYTE |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
{"lddw",	0x164,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_DWORD | TIC64X_OP_CONST_SCALE,
	{ tic64x_optxt_memaccess, tic64x_optxt_dwdst, tic64x_optxt_none},
	{ tic64x_operand_dwdst5, tic64x_operand_rcoffset }
},
{"ldh",		0x44,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_HWORD |
	TIC64X_OP_UNITNO | TIC64X_OP_CONST_SCALE | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"ldh",		0x4C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_HWORD |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
{"ldhu",	0x4,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_HWORD |
	TIC64X_OP_UNITNO | TIC64X_OP_CONST_SCALE | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"ldhu",	0xC,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_HWORD |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
{"ldndw",	0x124,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACC_SBIT |
	TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_DWORD,
	{ tic64x_optxt_memaccess, tic64x_optxt_dwdst, tic64x_optxt_none },
	{ tic64x_operand_dwdst4, tic64x_operand_rcoffset }
},
{"ldnw",	0x134,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_UNITNO | TIC64X_OP_CONST_SCALE,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"ldw",		0x64,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_UNITNO | TIC64X_OP_CONST_SCALE | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"ldw",		0x6C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_memaccess, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
/* LL counts as a "weird" instruction, and needn't be supported for now */
{"lmbd",	0xD78,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"lmbd",	0xD58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_uconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
{"max2",	0x858,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"max2",	0xF70,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"maxu4",	0x878,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"min2",	0x838,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"min2",	0xF30,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"minu4",	0x918,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpy",		0xC80,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpy",		0xC00,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"mpyh",	0x080,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhi",	0x530,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpyhir",	0x430,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhl",	0x480,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhlu",	0x780,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhslu",	0x580,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhsu",	0x180,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhu",	0x380,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhuls",	0x680,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyhus",	0x280,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX - spec does not explicitly have dst operand as being a dword dest in
 * the textual description */
/* this next insn is just mpyhi with src2 and src1 swapped */
{"mpyih",	0x530,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
/* This uses mpyhir, just with src2/src1 swapped */
{"mpyihr",	0x430,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* This uses mpyli with operands swapped; XXX spec doesn't have text dst as
 * being a dwdst */
{"mpyil",	0x570,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
/* Uses mpylir, like the above */
{"mpyilr",	0x3B0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpylh",	0x880,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpylhu",	0xB80,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX again, dst is dwreg, not explicit in spec */
{"mpyli",	0x570,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpylir",	0x3B0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpylshu",	0x980,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpyluhs",	0xA80,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpysu",	0xD80,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpysu",	0xF00,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* XXX destination not explicitly dword in spec */
{"mpysu4",	0x170,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpyu",	0xF80,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX destination not explicitly dword in spec */
{"mpyu4",	0x130,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpyus",	0xE80,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* As ever, this just uses mpysu4, with swapped operands */
{"mpyus4",	0x170,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpy2",	0x030,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpy2ir",	0x100003F0,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpy32",	0x800,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mpy32",	0x500,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpy32su",	0xB00,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpy32u",	0x630,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"mpy32us",	0x670,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
/* XXX - mvc instruction, has two different forms: those with two 5 bits consts
 * used to address the target control register, and those with only one five bit
 * field. I'm just going to implement the single-field one, seeing how c64x has
 * is (and the other seems c64x+ only). It solves a lot of pain that would occur
 * otherwise with constant writing to two fields.
 * Incidently, the TI docs shed no light on whether or not particular control
 * registers can only be accessed through either form of mvc, or why there
 * happen to be two kinds in the first place */
/* XXX: mvc ctrl->dst instruction claims to have xpath, is patently lying */
/* Don't have labeled side bits, they're hardcoded to 2: this works just as
 * well with the current configuration, as we'll only permit it to execute on
 * side 2, so ignore that fact */
{"mvc",		0x3E0,		0x3FFFC,
	TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_UNIT_S | TIC64X_OP_FIXED_UNITNO |
	TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_ctrlreg, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_basereg, tic64x_operand_invalid }
},
{"mvc",		0x3A0,		0x3EFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2 |
	TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_ctrlreg, tic64x_optxt_none },
	{ tic64x_operand_dstreg, tic64x_operand_invalid }
},
{"mvd",		0x340F0,	0x3EFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"mvk",		0x28,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{ tic64x_operand_const16, tic64x_operand_invalid }
},
/* XXX XXX XXX - spru732h p327, says xpath bit present, but not in opcode map,
 * and we never read anything from a register in this insn, so it's not
 * part of the opcode field but also not in flags */
{"mvk",		0xA358,		0x3EFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{ tic64x_operand_const5p2, tic64x_operand_invalid }
},
/* XXX XXX XXX - spru732h p327 again, "src2" field present in opcode, but
 * not used in actual instruction syntax or even operation. Field entirely
 * ignored for this implementation */
{"mvk",		0x40,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* XXX - see page 329 of spec, I don't know which way round the "h" bit of
 * the opcode goes for these two. Needs testing against a reference. */
{"mvkh",	0x68,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_TOP_HWORD | TIC64X_OP_NO_RANGE_CHK,
	{ tic64x_optxt_uconstant, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const16, tic64x_operand_invalid }
},
{"mvklh",	0x28,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_NO_RANGE_CHK,
	{ tic64x_optxt_uconstant, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const16, tic64x_operand_invalid }
},
{"mvkl",	0x28,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_NO_RANGE_CHK,
	{ tic64x_optxt_uconstant, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_const16, tic64x_operand_invalid }
},
{"neg",		0x5A0,		0x3EFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"neg",		0xD8,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"neg",		0x490,		0x3EFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dwsrc, tic64x_optxt_dwdst, tic64x_optxt_none },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"norm",	0xC78,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"norm",	0xC18,		0x3EFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dwsrc, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"not",		0x3EDD8,	0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"not",		0x3E2A0,	0x3EFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"or",		0x8B0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"or",		0x8F0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"or",		0xFF8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"or",		0xFD8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"or",		0x6E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"or",		0x6A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"pack2",	0x18,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"pack2",	0xFF0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packh2",	0x3D8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packh2",	0x260,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packh4",	0xD38,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packhl2",	0x398,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packhl2",	0x220,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packlh2",	0x378,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packlh2",	0x420,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"packl4",	0xD18,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"rint",	0x10006000,	0xFFFFFFFC,
	TIC64X_OP_NOCOND | TIC64X_OP_NOSIDE | TIC64X_OP_ALL_UNITS,
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"rotl",	0x770,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"rotl",	0x7B0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"rpack2",	0x10000EF0,	0xF0000FFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sadd",	0x278,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sadd",	0x638,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"sadd",	0x258,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"sadd",	0x618,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst },
	{ tic64x_operand_const5, tic64x_operand_dwdst5 }
},
{"sadd",	0x820,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sadd2",	0xC30,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"saddsub",	0x100001D8,	0xF0800FFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst4, tic64x_operand_invalid }
},
{"saddsub2",	0x100001F8,	0xF0800FFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst4, tic64x_operand_invalid }
},
/* Uses saddus2, swaps operands */
{"saddsu2",	0xC70,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"saddus2",	0xC70,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"saddu4",	0xCF0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX XXX XXX - src operand can't be xpathed, but spec says x bit present
 * in opcode. Bug? */
{"sat",		0x818,		0x3EFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dwsrc, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"set",		0x88,		0xFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_bfield, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"set",		0xEE0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shfl",	0x380F0,	0x3EFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shfl3",	0x100006D8,	0xF0000FFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"shl",		0xCE0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shl",		0xC60,		0xFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dwsrc, tic64x_optxt_srcreg1,tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"shl",		0x4E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"shl",		0xCA0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"shl",		0xC20,		0xFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dwsrc, tic64x_optxt_uconstant, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"shl",		0x4A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"shlmb",	0xC38,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shlmb",	0xE70,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shr",		0xDE0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shr",		0xD60,		0xFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dwsrc, tic64x_optxt_srcreg1, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"shr",		0xDA0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"shr",		0xD20,		0xFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dwsrc, tic64x_optxt_uconstant, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_const5 }
},
{"shr2",	0xDF0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shr2",	0x620,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"shrmb",	0xC58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shrmb",	0xEB0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shru",	0x9E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shru",	0x960,		0xFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dwsrc, tic64x_optxt_srcreg1, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"shru",	0x9A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"shru",	0x920,		0xFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dwsrc, tic64x_optxt_uconstant, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_const5 }
},
{"shru2",	0xE30,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"shru2",	0x660,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* XXX not implementing SL as it's atomic, we don't care that much */
{"smpy",	0xD00,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"smpyh",	0x100,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"smpyhl",	0x500,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"smpylh",	0x900,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"smpy2",	0x70,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"smpy32",	0x10000670,	0xF0000FFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_NOCOND,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"spack2",	0xCB0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"spacku4",	0xD30,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX No spkernel, spkernelr, sploop, splood, sploopw, spmask, spmaskr
 * instructions implemented. Thisi project isn't aimed at getting all the c6x's
 * firepower working immediately. */
{"sshl",	0x8E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sshl",	0x8A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"sshvl",	0x730,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sshvr",	0x770,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"ssub",	0x1F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"ssub",	0x3F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"ssub",	0x1D8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"ssub",	0x598,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst },
	{ tic64x_operand_const5, tic64x_operand_dwdst5 }
},
{"ssub2",	0xC98,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"stb",		0x34,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_BYTE | TIC64X_OP_CONST_SCALE | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"stb",		0x3C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_BYTE |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess,  tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
/* XXX - spec doesn't have src explicitly as a dword */
/* It also states two opcode maps, sans opcodes, to describe the offsetR/const
 * offset situation, which differs from other opcodes documentation, while
 * doing apparently exactly the same thing */
{"stdw",	0x144,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_DWORD | TIC64X_OP_CONST_SCALE,
	{ tic64x_optxt_dwdst, tic64x_optxt_memaccess, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_dwdst5 }
},
{"sth",		0x54,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_HWORD | TIC64X_OP_CONST_SCALE |TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"sth",		0x5C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_HWORD |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess,  tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
{"stndw",	0x174,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_DWORD | TIC64X_OP_MEMACC_SBIT,
	{ tic64x_optxt_dwdst, tic64x_optxt_memaccess, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_dwdst4 }
},
{"stnw",	0x154,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_WORD | TIC64X_OP_CONST_SCALE,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"stw",		0x74,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_WORD | TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_CONST_SCALE
	| TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess, tic64x_optxt_none },
	{ tic64x_operand_rcoffset, tic64x_operand_invalid }
},
{"stw",		0x7C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_SCALE | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess,  tic64x_optxt_none },
	{ tic64x_operand_const15, tic64x_operand_invalid }
},
{"sub",		0xF8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub",		0x2F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub",		0x4F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst},
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"sub",		0x6F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst},
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"sub",		0x0D8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"sub",		0x498,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{ tic64x_operand_dwdst5, tic64x_operand_const5 }
},
{"sub",		0x5E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub",		0x5A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* TI comment p 459 says operands are swapped here */
/* ... and somehow this has a z bit, but no cond register. Datasheet bug? */
{"sub",		0xD70,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_NOCOND | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub",		0x8C0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub",		0x9C0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"sub",		0xB30,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"subab",	0x18C0,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"subab",	0x19C0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg},
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"subabs4",	0xB58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"subah",	0x1AC0,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"subah",	0x1BC0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"subaw",	0x1CC0,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"subaw",	0x1DC0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"subc",	0x978,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX XXX XXX - spec p471 says at the start, this can take the form where the
 * destination is one register, or a pair. However the opcode map only features
 * dword destinations. I'm going to trust the opcode map, but this needs tests*/
{"subu",	0x5F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"subu",	0x7F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst },
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
{"sub2",	0x98,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub2",	0x460,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub2",	0x970,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"sub4",	0xCD8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* Swap2 opcodes are pseudoops, and are actually packlh2 / the like, with
 * some swapped operand orders. */
/* XXX XXX XXX - src2 and src1 need to be replicated in both slots. Need to
 * introduce a flag to do this.. */
{"swap2",	0x378,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"swap2",	0x420,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* Datasheet should say src2 where it says src, otehrwise fine */
{"swap4",	0x358,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"swe",		0x10000000,	0xFFFFFFFC,
	TIC64X_OP_ALL_UNITS | TIC64X_OP_NOSIDE, /* There is, but don't care */
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"swnr",	0x10002000,	0xFFFFFFFC,
	TIC64X_OP_ALL_UNITS | TIC64X_OP_NOSIDE, /* There is, but don't care */
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"unpkhu4",	0x6358,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"unpkhu4",	0x6F20,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"unpklu4",	0x4358,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"unpklu4",	0x4F20,		0x3EFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"xor",		0xDF8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2 | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"xor",		0xDD8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"xor",		0x2E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"xor",		0x2A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
{"xor",		0xBB0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"xor",		0xBF0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{ tic64x_operand_const5, tic64x_operand_invalid }
},
/* XXX xormpy is a galois field insn, not interesting for now */
{"xpnd2",	0x320F0,	0x3EFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"xpnd4",	0x300F0,	0x3EFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* XXX XXX XXX XXX XXX XXX XXX XXX XXX */
/* The zero opcode is just entirely absent from spru732h; there's a page (495)
 * for it, but they just don't appear to have bothered to put it in. spru186q
 * p74 has a binary dump of some assembly, I've assumed the destination reg
 * is in exactly the same as ever and pieced it back together from that. There
 * _is_ an opcode map field in the usual reference though */
{"zero",	0x8C0,		0x7FFFFC,		/* damned if I know */
	TIC64X_OP_UNIT_D | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"zero",	0xBC0,		0x7FFFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{"zero",	0x1BC0,		0x7FFFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dwdst, tic64x_optxt_none, tic64x_optxt_none},
/* XXX XXX XXX - I've no idea whether this could be dwdst4 for zero */
	{ tic64x_operand_dwdst5, tic64x_operand_invalid }
},
/* See preceeding XXX's, this opcode is identical to one of the L opcodes,
 * only difference is that it goes down the S units. Uuughh, docs please.*/
{"zero",	0xBC0,		0x7FFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
/* Nop needs to go at the end of the file, as the zero opcode matches all
 * kinds of foo */
{"nop",		0x0,		0x21FFE,
	TIC64X_OP_NOCOND | TIC64X_OP_NOSIDE | TIC64X_OP_ALL_UNITS,
	{ tic64x_optxt_nops, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_nops, tic64x_operand_invalid }
},
{NULL, 		0,		0,		0,
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
}
}; /* End of main opcode table */

/* MV instruction representation - this is for the assembler to have a template
 * for mv, but without it actually being part of the opcode table, where it
 * might actually require an opcode / the like */
const struct tic64x_op_template tic64x_mv_template[] = {
{"mv",		0,		0,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNIT_S | TIC64X_OP_UNIT_L |
	TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
},
{NULL,		0,		0,		0,
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{ tic64x_operand_invalid, tic64x_operand_invalid }
}
};

/* Compact instructions are made up of twisty turny formats, all different.
 * Rather than beating faces against keyboards and trying to have some generic
 * way of converting a compact back to a large instruction, which would no doubt
 * lead to a million and one flags, instead grow a table of compact opcodes and
 * their masks, and a set of conversion routines for each different format.
 * Unpleasent, but then again, so are these instruction formats. */

static int bad_scaledown(uint32_t opcode, uint16_t *out);
static int bad_scaleup(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);

static int scaleup_doff4(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_dind(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_ddec(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_dstk(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_sbs7(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_sx1b(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_s3(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_unop(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);

struct tic64x_compact_table tic64x_compact_formats[] = {
{0,		0xFFFF,	bad_scaledown, bad_scaleup},	/* invalid */
{0x4,		0x406,	bad_scaledown, scaleup_doff4},	/* doff4 */
{0x404,		0xC06,	bad_scaledown, scaleup_dind},	/* dind */
{0xC04,		0xCC06,	bad_scaledown, bad_scaleup},	/* dinc */
{0x4C04,	0x4C06,	bad_scaledown, scaleup_ddec},	/* ddec */
{0x8C04,	0x8C06,	bad_scaledown, scaleup_dstk},	/* dstk */
{0x36,		0x47E,	bad_scaledown, bad_scaleup},	/* dx2op */
{0x436,		0x47E,	bad_scaledown, bad_scaleup},	/* dx5 */
{0xC77,		0x1C7F,	bad_scaledown, bad_scaleup},	/* dx5p */
{0x1876,	0x1C7E,	bad_scaledown, bad_scaleup},	/* dx1 */
{0x77,		0x87F,	bad_scaledown, bad_scaleup},	/* dpp */
{0,		0x40E,	bad_scaledown, bad_scaleup},	/* l3 */
{0x400,		0x40E,	bad_scaledown, bad_scaleup},	/* l3i */
{8,		0x40E,	bad_scaledown, bad_scaleup},	/* ltbd */
{0x408,		0x40E,	bad_scaledown, bad_scaleup},	/* l2c */
{0x426,		0x47E,	bad_scaledown, bad_scaleup},	/* lx5 */
{0x26,		0x147E,	bad_scaledown, bad_scaleup},	/* lx3c */
{0x1026,	0x147E,	bad_scaledown, bad_scaleup},	/* lx1c */
{0x1866,	0x1C7E,	bad_scaledown, bad_scaleup},	/* lx1 */
{0x1E,		0x1E,	bad_scaledown, bad_scaleup},	/* m3 */
{0xA,		0x3E,	bad_scaledown, scaleup_sbs7},	/* sbs7 */
{0xC00A,	0xC03E,	bad_scaledown, bad_scaleup},	/* sbu8 */
#if 0
/* Face: This opcode form is indistinguishable from S3. We're not likely to
see it any time soon, so ignore for now. Seeing how presumably the silicon
itself can't tell the difference, I assume a datasheet bug */
{0x1A,		0x3E,	bad_scaledown, bad_scaleup},	/* scs10 */
#endif
{0x2A,		0x2E,	bad_scaledown, bad_scaleup},	/* sbs7c */
{0xC02A,	0xC02E,	bad_scaledown, bad_scaleup},	/* sbu8c */
{0xA,		0x40E,	bad_scaledown, scaleup_s3},	/* s3 */
{0x40A,		0x40E,	bad_scaledown, bad_scaleup},	/* s3i */
{0x12,		0x1E,	bad_scaledown, bad_scaleup},	/* smvk8 */
{0x402,		0x41E,	bad_scaledown, bad_scaleup},	/* ssh5 */
{0x462,		0x47E,	bad_scaledown, bad_scaleup},	/* s2sh */
{2,		0x41E,	bad_scaledown, bad_scaleup},	/* sc5 */
{0x62,		0x47E,	bad_scaledown, bad_scaleup},	/* s2ext */
{0x2E,		0x47E,	bad_scaledown, bad_scaleup},	/* sx2op */
{0x42E,		0x47E,	bad_scaledown, bad_scaleup},	/* sx5 */
{0x186E,	0x1C7E,	bad_scaledown, bad_scaleup},	/* sx1 */
{0x6E,		0x187E,	bad_scaledown, scaleup_sx1b},	/* sx1b */
{6,		0x66,	bad_scaledown, bad_scaleup},	/* lsd_mvto */
{0x46,		0x66,	bad_scaledown, bad_scaleup},	/* lsd_mvfr */
{0x866,		0x1C66,	bad_scaledown, bad_scaleup},	/* lsd_x1c */
{0x1866,	0x1C66,	bad_scaledown, bad_scaleup},	/* lsd_x1 */
{0xC66,		0xBC7E,	bad_scaledown, bad_scaleup},	/* uspl */
{0x8C66,	0xBC7E,	bad_scaledown, bad_scaleup},	/* uspldr */
{0x1C66,	0x3C7E,	bad_scaledown, bad_scaleup},	/* uspk */
{0x2C66,	0x2C7E,	bad_scaledown, bad_scaleup},	/* uspm */
/* This should cover spmask and spmaskr, see p670 */
{0xC6E,		0x1FFF,	bad_scaledown, scaleup_unop},	/* unop */
{0,		0,	NULL,		NULL	}
};

/*Opcodes corresponding to dsz and l/s field for 'd' unit memory access scales*/
static uint32_t compact_d_scaleup_codes[] = {
	0x34,		/* stb */
	0x14,		/* ldbu */
	0x34,		/* stb */
	0x24,		/* ldb */
	0x54,		/* sth */
	4,		/* ldhu */
	0x54,		/* sth */
	0x44,		/* ldh */
	0x74,		/* stw */
	0x64,		/* lwd */
	0x34,		/* stb again */
	0x24,		/* ldb again */
	0x154,		/* stnw */
	0x134,		/* ldnw */
	0x54,		/* sth again */
	0x44,		/* ldh again */
};

int
bad_scaledown(uint32_t opcode ATTRIBUTE_UNUSED, uint16_t *out ATTRIBUTE_UNUSED)
{

	return ENODEV;	/*Operation not supported by device; suprisingly right*/
}

int
bad_scaleup(uint16_t opcode ATTRIBUTE_UNUSED, uint32_t hdr ATTRIBUTE_UNUSED,
		uint32_t *out_opcode ATTRIBUTE_UNUSED)
{

	return ENODEV;
}

static void
scaleup_mem_access_opcopde(uint32_t *out_opcode, uint16_t opcode, int sz,
				int dsz, int ls, int reg)
{
	int i;

	if (sz) {
		i = dsz << 1;
		i |= ls;
		*out_opcode |= compact_d_scaleup_codes[i];
	} else if (!(dsz & 4)) {
		/* Either stw or ldw */
		i = 8;
		i |= ls;
		*out_opcode |= compact_d_scaleup_codes[i];
	} else {
		i = ls;
		i |= (opcode & 0x10) ? 2 : 0; /* non-aligned bit */
		if (opcode & 0x10) {

			/* ??NDW insns only have 4 bit field for dest reg */
			tic64x_set_operand(out_opcode, tic64x_operand_dwdst4,
								reg >> 1);
			tic64x_set_operand(out_opcode, tic64x_operand_scale, 0);
		}

		switch (i) {
		case 0:
			*out_opcode |= 0x144;	/* stdw */
			break;
		case 1:
			*out_opcode |= 0x164;	/* lddw */
			break;
		case 2:
			*out_opcode |= 0x174;	/* stndw */
			break;
		case 3:
			*out_opcode |= 0x124;	/* ldndw */
			break;
		}
	}

	return;
}

int
scaleup_doff4(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode)
{
	int dsz, reg, base, offs, s, t, sz, ls;

	*out_opcode = 0;

	/* Decompose opcode */
	dsz = hdr >> 16;
	dsz &= 7;
	s = opcode & 1;
	ls = (opcode & 8) ? 1 : 0;
	t = (opcode & 0x1000) ? 1 : 0;
	sz = (opcode & 0x200) ? 1 : 0;
	reg = (opcode >> 4) & 7;
	base = (opcode >> 7) & 3;
	offs = (opcode >> 13) & 7;
	offs |= (opcode & 0x800) ? 8 : 0;

	/* Put these operands into 32 bit opcode, which thankfully appears to
	 * all be the same form, aside from dw access */

	/* If RS bit is set, use high registers */
	if (hdr & 0x80000) { /* XXX - #define please */
		reg += 16;
		base += 16;
	}

	/* XXX XXX XXX: TI spec says nothing about how to munge the ptr field
	 * into a base register - however, disassembling an example of theirs
	 * shows that the ptr field is zero for an instruction that uses reg B4,
	 * which suggests to me that we just need to offset base by 4. This
	 * may explode in the future, but the best we have for the moment */
	base += 4;

	tic64x_set_operand(out_opcode, tic64x_operand_addrmode,
				TIC64X_ADDRMODE_NOMODIFY |
				TIC64X_ADDRMODE_OFFSET |
				TIC64X_ADDRMODE_PRE |
				TIC64X_ADDRMODE_POS);
	tic64x_set_operand(out_opcode, tic64x_operand_rcoffset, offs);
	tic64x_set_operand(out_opcode, tic64x_operand_dstreg, reg);
	tic64x_set_operand(out_opcode, tic64x_operand_basereg, base);
	tic64x_set_operand(out_opcode, tic64x_operand_z, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_creg, 0);

	/* Need to set y and s bit - in compact insns, s is side of base ptr,
	 * and t is the side of the "src/dst", for us the destination ptr.
	 * normal l/s's use y to select the basereg, s to select the dest, so
	 * we set 16 -> 32 as s -> y, t -> s */
	tic64x_set_operand(out_opcode, tic64x_operand_y, s);
	tic64x_set_operand(out_opcode, tic64x_operand_s, t);

	scaleup_mem_access_opcopde(out_opcode, opcode, sz, dsz, ls, reg);

	return 0;
}

/* XXX - this can probably be merged into scaleup_doff4 */
int
scaleup_dind(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode)
{
	int dsz, reg, base, idx, s, t, sz, ls;

	*out_opcode = 0;

	/* Decompose opcode */
	dsz = hdr >> 16;
	dsz &= 7;
	s = opcode & 1;
	ls = (opcode & 8) ? 1 : 0;
	t = (opcode & 0x1000) ? 1 : 0;
	sz = (opcode & 0x200) ? 1 : 0;
	reg = (opcode >> 4) & 7;
	base = (opcode >> 7) & 3;
	idx = get_operand(opcode, 13, 3, 1); /* Sign extend... */

	/* Put these operands into 32 bit opcode, which thankfully appears to
	 * all be the same form, aside from dw access */

	/* If RS bit is set, use high registers */
	if (hdr & 0x80000) { /* XXX - #define please */
		reg += 16;
		base += 16;
	}

	/* XXX XXX XXX: TI spec says nothing about how to munge the ptr field
	 * into a base register - however, disassembling an example of theirs
	 * shows that the ptr field is zero for an instruction that uses reg B4,
	 * which suggests to me that we just need to offset base by 4. This
	 * may explode in the future, but the best we have for the moment */
	base += 4;

	tic64x_set_operand(out_opcode, tic64x_operand_addrmode,
				TIC64X_ADDRMODE_NOMODIFY |
				TIC64X_ADDRMODE_OFFSET |
				TIC64X_ADDRMODE_PRE |
				TIC64X_ADDRMODE_POS);
	tic64x_set_operand(out_opcode, tic64x_operand_rcoffset, idx);
	tic64x_set_operand(out_opcode, tic64x_operand_dstreg, reg);
	tic64x_set_operand(out_opcode, tic64x_operand_basereg, base);
	tic64x_set_operand(out_opcode, tic64x_operand_z, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_creg, 0);

	/* Need to set y and s bit - in compact insns, s is side of base ptr,
	 * and t is the side of the "src/dst", for us the destination ptr.
	 * normal l/s's use y to select the basereg, s to select the dest, so
	 * we set 16 -> 32 as s -> y, t -> s */
	tic64x_set_operand(out_opcode, tic64x_operand_y, s);
	tic64x_set_operand(out_opcode, tic64x_operand_s, t);

	scaleup_mem_access_opcopde(out_opcode, opcode, sz, dsz, ls, reg);

	return 0;
}

int
scaleup_ddec(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode)
{
	int dsz, s, ls, reg, ptr, sz, t, cst;

	dsz = hdr >> 16;
	dsz &= 7;
	s = opcode & 1;
	ls = (opcode & 8) ? 1 : 0;
	reg = (opcode >> 4) & 7;
	ptr = (opcode >> 7) & 3;
	sz = (opcode & 0x200) ? 1 : 0;
	t = (opcode & 0x1000) ? 1 : 0;
	cst = (opcode & 0x2000) ? 1 : 0;
	cst += 1; /* See footnote in datasheet */

	if (hdr & 0x80000) {
		reg += 16;
		ptr += 16;
	}

	ptr += 4; /* See death-notice in dind */

	*out_opcode = 0;
	tic64x_set_operand(out_opcode, tic64x_operand_addrmode,
				TIC64X_ADDRMODE_MODIFY|
				TIC64X_ADDRMODE_OFFSET |
				TIC64X_ADDRMODE_PRE |
				TIC64X_ADDRMODE_NEG);
	tic64x_set_operand(out_opcode, tic64x_operand_rcoffset, cst);
	tic64x_set_operand(out_opcode, tic64x_operand_dstreg, reg);
	tic64x_set_operand(out_opcode, tic64x_operand_basereg, ptr);
	tic64x_set_operand(out_opcode, tic64x_operand_z, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_creg, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_y, s);
	tic64x_set_operand(out_opcode, tic64x_operand_s, t);

	scaleup_mem_access_opcopde(out_opcode, opcode, sz, dsz, ls, reg);

	return 0;
}

int
scaleup_dstk(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode)
{
	int s, ls, reg, t, ucst;

	s = opcode & 1;
	ls = (opcode & 8) ? 1 : 0;
	reg = (opcode >> 4) & 7;
	t = (opcode & 0x1000) ? 1 : 0;
	ucst = (opcode >> 13) & 3;
	ucst |= (opcode >> 5) & 0x1C;

	if (hdr & 0x80000)
		reg += 16;

	*out_opcode = 0;
	tic64x_set_operand(out_opcode, tic64x_operand_dstreg, reg);
	/* Base register is always B15 according to datasheet */
	tic64x_set_operand(out_opcode, tic64x_operand_basereg, 0xF);
	tic64x_set_operand(out_opcode, tic64x_operand_z, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_creg, 0);
	/* XXX - while datasheet says the base is always on the B side, theres
	 * still an s bit present in the instruction. */
	if (s != 1) {
		fprintf(stderr, "dstk insn with wrong s bit: I'm covered in bees\n");
	}
	tic64x_set_operand(out_opcode, tic64x_operand_y, t);
	tic64x_set_operand(out_opcode, tic64x_operand_s, t);
	tic64x_set_operand(out_opcode, tic64x_operand_const15, ucst);

	if (ls)
		*out_opcode |= 0x6C; /* load */
	else
		*out_opcode |= 0x7C; /* Store */

	return 0;
}

int
scaleup_sbs7(uint16_t opcode, uint32_t hdr ATTRIBUTE_UNUSED,
					uint32_t *out_opcode)
{
	int offs, nops, s;

	/* Thankfully only single instruction... */

	*out_opcode = 0;
	*out_opcode |= 0x120;

	offs = get_operand(opcode, 6, 7, 1); /* Read const field, sign extend */
	tic64x_set_operand(out_opcode, tic64x_operand_const12, offs);
	nops = get_operand(opcode, 13, 3, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_nops, nops);
	tic64x_set_operand(out_opcode, tic64x_operand_z, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_creg, 0);
	s = get_operand(opcode, 0, 1, 0);
	tic64x_set_operand(out_opcode, tic64x_operand_s, s);

	return 0;
}

int
scaleup_sx1b(uint16_t opcode, uint32_t hdr ATTRIBUTE_UNUSED,
					uint32_t *out_opcode)
{

	/* This only applies to BNOP */
	/* It's also unconditional */
	*out_opcode = 0x800362;
	tic64x_set_operand(out_opcode, tic64x_operand_nops, (opcode >> 13) & 7);
	/* Comment on p659 says src2 is from B0 -> B15 */
	tic64x_set_operand(out_opcode, tic64x_operand_srcreg2,
					(opcode >> 7) & 0xF);

	/* XXX XXX XXX - compact opcode apparently has side bit, but only
	 * mapped opcode is fixed on unit S2 */

	return 0;
}

int
scaleup_s3(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode)
{
	uint32_t src1, src2, dst, op, s, x;

	src1 = (opcode >> 13) & 0x7;
	src2 = (opcode >> 7) & 0x7;
	dst = (opcode >> 4) & 0x7;
	op = (opcode & 0x800) ? 1 : 0;
	s = (opcode & 1) ? 1 : 0;
	x = (opcode & 0x1000) ? 1 : 0;

	/* XXX - #defines please */
	if (hdr & 0x8000) {
		return ENODEV; /* 'BR' not valid for this */
	}

	/* Use RS field to determine if we're using the high or low
	 * register set */
	if (hdr & 0x80000) {
		src1 += 16;
		src2 += 16;
		dst += 16;
	}

	if (hdr & 0x4000) {
		if (op == 1)
			return ENODEV; /* Undefined behavior, etc */

		*out_opcode = 0x278;
	} else {
		/* These are all in this form share all fields, just need to set
		 * opcode depending on which one we're using */
		if (op == 0)
			*out_opcode = 0x1E0;
		else
			*out_opcode = 0x5E0;
	}

	/* Unconditional, etc */
	tic64x_set_operand(out_opcode, tic64x_operand_s, s);
	tic64x_set_operand(out_opcode, tic64x_operand_x, x);
	tic64x_set_operand(out_opcode, tic64x_operand_srcreg1, src1);
	tic64x_set_operand(out_opcode, tic64x_operand_srcreg2, src2);
	tic64x_set_operand(out_opcode, tic64x_operand_dstreg, dst);

	return 0;
}

static int
scaleup_unop(uint16_t opcode, uint32_t hdr ATTRIBUTE_UNUSED,
					uint32_t *out_opcode)
{

	/* Enjoyably simple: top 3 bits are a number of nops, the end. */
	*out_opcode = 0;
	tic64x_set_operand(out_opcode, tic64x_operand_nops, opcode >> 13);
	return 0;
}
