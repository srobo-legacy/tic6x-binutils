/* Copyright blah */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"

struct tic64x_register tic64x_regs[] = {
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
	{NULL,	0}
};

/* Dummy table for dummy opcodes */
struct tic64x_op_template tic64x_opcodes[] = {
{"add",		0x78,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_USE_XPATH |
	TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0x478,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst},
	{
		{tic64x_operand_dwdst,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0x438,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{
		{tic64x_operand_dwdst,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_dwsrc,			18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0x58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_sconstant, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_vconstant,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0x418,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE, /* no xpath today */
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{
		{tic64x_operand_dwdst,			23,		5},
		{tic64x_operand_vconstant,		13,		5},
		{tic64x_operand_dwsrc,			18,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0x1E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0x1A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_vconstant,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
/* Caution - opcode map field reverses in ti spec for some reason
 * for these two ops */
{"add",		0x840,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_COND | TIC64X_OP_SIDE,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0x940,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_COND | TIC64X_OP_SIDE,
	{ tic64x_optxt_uconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_vconstant,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0xAB0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"add",		0xAF0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_vconstant,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"b",		0x10,		0x7C,
	TIC64X_OP_COND | TIC64X_OP_UNIT_S | TIC64X_OP_SIDE |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_none, tic64x_optxt_none},
	{
		{tic64x_operand_vconstant,		7,		21},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"b",		0x360,		0x0F83EFFC,
	TIC64X_OP_COND | TIC64X_OP_UNIT_S | TIC64X_OP_SIDE |
	TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2 |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_none, tic64x_optxt_none},
	{
		{tic64x_operand_srcreg1,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"dotpu4",	0x1B0,		0xFFC,
	TIC64X_OP_COND | TIC64X_OP_UNIT_M | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"lddw",	0x164,		0x17C,
	TIC64X_OP_COND | TIC64X_OP_UNIT_D | TIC64X_OP_SIDE |
	TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_DWORD |
	TIC64X_OP_MEMACC_SCALE,
	{ tic64x_optxt_memaccess, tic64x_optxt_dwdst, tic64x_optxt_none},
	{
		{tic64x_operand_dwdst,			23,		5},
		{tic64x_operand_basereg,		18,		5},
		{tic64x_operand_addrmode,		9,		4},
		{tic64x_operand_rcoffset,		13,		5},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"ldndw",	 0x124,		0x17C,
	TIC64X_OP_COND | TIC64X_OP_UNIT_D | TIC64X_OP_SIDE |
	TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_DWORD,
	{ tic64x_optxt_memaccess, tic64x_optxt_dwdst, tic64x_optxt_none },
	{
		{tic64x_operand_dwdst,			24,		4},
		{tic64x_operand_basereg,		18,		5},
		{tic64x_operand_addrmode,		9,		4},
		{tic64x_operand_rcoffset,		13,		5},
		{tic64x_operand_scale,			23,		1}
	}
},
{"mvk",		0x28,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{
		{tic64x_operand_vconstant,		7,		16},
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
/* XXX XXX XXX - spru732h p327, says xpath bit present, but not in opcode map,
 * and we never read anything from a register in this insn, so it's not
 * part of the opcode field but also not in flags */
{"mvk",		0xA358,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{
		{tic64x_operand_vconstant,		18,		5},
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
/* XXX XXX XXX - spru732h p327 again, "src2" field present in opcode, but
 * not used in actual instruction syntax or even operation. Field entirely
 * ignored for this implementation */
{"mvk",		0x40,		0x1FFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_COND | TIC64X_OP_SIDE,
	{tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{
		{tic64x_operand_vconstant,		13,		5},
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"subabs4",	0xB58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_srcreg1,		13,		5},
		{tic64x_operand_srcreg2,		18,		5},
		{tic64x_operand_x,			12,		1},
		{tic64x_operand_invalid,		0,		0}
	}
},
/* XXX XXX XXX XXX XXX XXX XXX XXX XXX */
/* The zero opcode is just entirely absent from spru732h; there's a page (495)
 * for it, but they just don't appear to have bothered to put it in. spru186q
 * p74 has a binary dump of some assembly, I've assumed the destination reg
 * is in exactly the same as ever and pieced it back together from that. There
 * _is_ an opcode map field in the usual reference though */
{"zero",	0x8C0,		0x7FFFFC,		/* damned if I know */
	TIC64X_OP_UNIT_D  | TIC64X_OP_COND | TIC64X_OP_SIDE |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"zero",	0xBC0,		0x7FFFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{"zero",	0x1BC0,		0x7FFFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_COND | TIC64X_OP_SIDE,
	{ tic64x_optxt_dwdst, tic64x_optxt_none, tic64x_optxt_none},
	{
		{tic64x_operand_dwdst,			23,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
/* See preceeding XXX's, this opcode is identical to one of the L opcodes,
 * only difference is that it goes down the S units. Uuughh, docs please.*/
{"zero",	0xBC0,		0x7FFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_COND | TIC64X_OP_SIDE,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{
		{tic64x_operand_dstreg,			23,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
},
{NULL, 		0,		0,		0,
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0}
	}
}
};
