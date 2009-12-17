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
		{tic64x_operand_invalid,		0,		0},
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
		{tic64x_operand_invalid,		0,		0},
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
	TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_none, tic64x_optxt_none},
	{
		{tic64x_operand_srcreg1,		18,		5},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
		{tic64x_operand_invalid,		0,		0},
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
	TIC64X_OP_UNIT_D | TIC64X_OP_UNIT_S | TIC64X_OP_UNIT_L |
	TIC64X_OP_UNITNO | TIC64X_OP_COND,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{
		{tic64x_operand_vconstant,		7,		16},
		{tic64x_operand_dstreg,			23,		5},
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
