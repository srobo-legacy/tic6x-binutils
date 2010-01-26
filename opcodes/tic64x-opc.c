/* Copyright blah */

#include <errno.h>
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

struct tic64x_operand_pos tic64x_operand_positions [] = {
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
{	13,	3	},	/* nops */
{	13,	5	},	/* srcreg1 */
{	18,	5	},	/* srcreg2 */
{	18,	5	},	/* dwsrc */
{	0,	1	},	/* p */
{	1,	1	},	/* s */
{	7,	1	},	/* y */
{	28,	1	},	/* z */
{	29,	3	}	/* creg */
};

const char *
tic64x_set_operand(uint32_t *op, enum tic64x_operand_type type, int value)
{

	if (value < 0) {
		if (-value >= (1 << tic64x_operand_positions[type].size)) {
			return "Operand too large for position";
		}
	} else {
		if (value >= (1 << tic64x_operand_positions[type].size)) {
			return "Operand too large for position";
		}
	}

	if (tic64x_operand_positions[type].position +
				tic64x_operand_positions[type].size >32)
		return "Operand type falls off end of opcode";

	value &= ((1 << tic64x_operand_positions[type].size) - 1);
	value <<= tic64x_operand_positions[type].position;
	*op |= value;
	return NULL;
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
struct tic64x_op_template tic64x_opcodes[] = {
{"abs",		0x358,		0x3EFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg2, tic64x_optxt_dstreg, tic64x_optxt_none },
	{
		tic64x_operand_srcreg2,
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"abs",		0x718,		0x3EFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dwsrc, tic64x_optxt_dwdst, tic64x_optxt_none },
	{
		tic64x_operand_dwsrc,
		tic64x_operand_dwdst5,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"add",		0x78,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_MULTI_MNEMONIC | TIC64X_OP_USE_XPATH |
	TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"add",		0x478,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst},
	{
		tic64x_operand_dwdst5,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"add",		0x438,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{
		tic64x_operand_dwdst5,
		tic64x_operand_srcreg1,
		tic64x_operand_dwsrc,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"add",		0x58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_sconstant, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"add",		0x418,		0xFFC,
	TIC64X_OP_UNIT_L, /* no xpath today */
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{
		tic64x_operand_dwdst5,
		tic64x_operand_const5,
		tic64x_operand_dwsrc,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"add",		0x1E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"add",		0x1A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
/* Caution - opcode map field reverses in ti spec for some reason
 * for these two ops */
{"add",		0x840,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"add",		0x940,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_uconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"add",		0xAB0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"add",		0xAF0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"b",		0x10,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_PCREL | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_none, tic64x_optxt_none},
	{
		tic64x_operand_const21,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"b",		0x360,		0x0F83EFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2 |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_none, tic64x_optxt_none},
	{
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"bnop",	0x120,		0x1FFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_CONST_SCALE | TIC64X_OP_MEMSZ_WORD |
	TIC64X_OP_CONST_PCREL | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_nops, tic64x_optxt_none},
	{
		tic64x_operand_const12,
		tic64x_operand_nops,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"bnop",	0x800362,	0x0F800FFE,
	TIC64X_OP_UNIT_S | TIC64X_OP_NOSIDE |
	 TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2,
	{ tic64x_optxt_srcreg2, tic64x_optxt_nops, tic64x_optxt_none},
	{
		tic64x_operand_srcreg2,
		tic64x_operand_nops,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"dotpu4",	0x1B0,		0xFFC,
	TIC64X_OP_UNIT_M | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"lddw",	0x164,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS |
	TIC64X_OP_MEMSZ_DWORD | TIC64X_OP_MEMACC_SCALE,
	{ tic64x_optxt_memaccess, tic64x_optxt_dwdst, tic64x_optxt_none},
	{
		tic64x_operand_dwdst5,
		tic64x_operand_basereg,
		tic64x_operand_addrmode,
		tic64x_operand_rcoffset,
		tic64x_operand_invalid
	}
},
{"ldndw",	 0x124,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO |
	TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_DWORD,
	{ tic64x_optxt_memaccess, tic64x_optxt_dwdst, tic64x_optxt_none },
	{
		tic64x_operand_dwdst4,
		tic64x_operand_basereg,
		tic64x_operand_addrmode,
		tic64x_operand_rcoffset,
		tic64x_operand_scale
	}
},
{"mvk",		0x28,		0x7C,
	TIC64X_OP_UNIT_S | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{
		tic64x_operand_const16,
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
/* XXX XXX XXX - spru732h p327, says xpath bit present, but not in opcode map,
 * and we never read anything from a register in this insn, so it's not
 * part of the opcode field but also not in flags */
{"mvk",		0xA358,		0x3EFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{
		tic64x_operand_const5p2,
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
/* XXX XXX XXX - spru732h p327 again, "src2" field present in opcode, but
 * not used in actual instruction syntax or even operation. Field entirely
 * ignored for this implementation */
{"mvk",		0x40,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{tic64x_optxt_sconstant, tic64x_optxt_dstreg, tic64x_optxt_none},
	{
		tic64x_operand_const5,
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"or",		0x8B0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"or",		0x8F0,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"or",		0xFF8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"or",		0xFD8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"or",		0x6E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"or",		0x6A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"pack2",	0x18,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"pack2",	0xFF0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"sadd",	0x278,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"sadd",	0x638,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_dwsrc, tic64x_optxt_dwdst },
	{
		tic64x_operand_srcreg1,
		tic64x_operand_dwsrc,
		tic64x_operand_dwdst5,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"sadd",	0x258,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg },
	{
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"sadd",	0x618,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst },
	{
		tic64x_operand_const5,
		tic64x_operand_dwsrc,
		tic64x_operand_dwdst5,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"stw",		0x74,		0x17C,
	TIC64X_OP_UNIT_D | TIC64X_OP_UNITNO |
	TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_WORD | TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_dstreg, tic64x_optxt_memaccess, tic64x_optxt_none },
	{
		tic64x_operand_dstreg,
		tic64x_operand_addrmode,
		tic64x_operand_basereg,
		tic64x_operand_rcoffset,
		tic64x_operand_invalid
	}
},
/* XXX - slightly confusing is that this opcode can only be executed on unit
 * no 2, but the side bit being present means that it can use src/dst registers
 * on side 1 for everything? That might violate something: needs checking */
{"stw",		0x7C,		0x7C,
	TIC64X_OP_UNIT_D | TIC64X_OP_FIXED_UNITNO | TIC64X_OP_FIXED_UNIT2 |
	TIC64X_OP_UNITNO | TIC64X_OP_MEMACCESS | TIC64X_OP_MEMSZ_WORD,
	{ tic64x_optxt_dstreg, tic64x_optxt_uconstant, tic64x_optxt_none },
	{
		tic64x_operand_dstreg,
		tic64x_operand_const15,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"sub",		0xF8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2 |
	TIC64X_OP_MULTI_MNEMONIC,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"sub",		0x2F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"sub",		0x4F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst},
	{
		tic64x_operand_dwdst5,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"sub",		0x6F8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dwdst},
	{
		tic64x_operand_dwdst5,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid,
	}
},
{"sub",		0x0D8,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"sub",		0x498,		0xFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_sconstant, tic64x_optxt_dwsrc, tic64x_optxt_dwdst},
	{
		tic64x_operand_dwdst5,
		tic64x_operand_const5,
		tic64x_operand_dwsrc,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"sub",		0x5E0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"sub",		0x5A0,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_sconstant, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
/* TI comment p 459 says operands are swapped here */
/* Also, first (only) instance of instruction with no cond */
{"sub",		0xD70,		0xFFC,
	TIC64X_OP_UNIT_S | TIC64X_OP_NOCOND |
	TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC1,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"sub",		0x8C0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_srcreg1, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"sub",		0x9C0,		0x1FFC,
	TIC64X_OP_UNIT_D,
	{ tic64x_optxt_srcreg2, tic64x_optxt_uconstant, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_const5,
		tic64x_operand_srcreg2,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"sub",		0xB30,		0xFFC,
	TIC64X_OP_UNIT_D | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
},
{"subabs4",	0xB58,		0xFFC,
	TIC64X_OP_UNIT_L | TIC64X_OP_USE_XPATH | TIC64X_OP_XPATH_SRC2,
	{ tic64x_optxt_srcreg1, tic64x_optxt_srcreg2, tic64x_optxt_dstreg},
	{
		tic64x_operand_dstreg,
		tic64x_operand_srcreg1,
		tic64x_operand_srcreg2,
		tic64x_operand_x,
		tic64x_operand_invalid
	}
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
	{
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"zero",	0xBC0,		0x7FFFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{"zero",	0x1BC0,		0x7FFFFC,
	TIC64X_OP_UNIT_L,
	{ tic64x_optxt_dwdst, tic64x_optxt_none, tic64x_optxt_none},
	{
/* XXX XXX XXX - I've no idea whether this could be dwdst4 for zero */
		tic64x_operand_dwdst5,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
/* See preceeding XXX's, this opcode is identical to one of the L opcodes,
 * only difference is that it goes down the S units. Uuughh, docs please.*/
{"zero",	0xBC0,		0x7FFFC,
	TIC64X_OP_UNIT_S,
	{ tic64x_optxt_dstreg, tic64x_optxt_none, tic64x_optxt_none},
	{
		tic64x_operand_dstreg,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
},
{NULL, 		0,		0,		0,
	{ tic64x_optxt_none, tic64x_optxt_none, tic64x_optxt_none },
	{
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid,
		tic64x_operand_invalid
	}
}
}; /* End of main opcode table */

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
static int scaleup_sbs7(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_sx1b(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
static int scaleup_s3(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);

struct tic64x_compact_table tic64x_compact_formats[] = {
{0,		0xFFFF,	bad_scaledown, bad_scaleup},	/* invalid */
{0x4,		0x406,	bad_scaledown, scaleup_doff4},	/* doff4 */
{0x404,		0xC06,	bad_scaledown, scaleup_dind},	/* dind */
{0xC04,		0xCC06,	bad_scaledown, bad_scaleup},	/* dinc */
{0x4C04,	0x4C06,	bad_scaledown, bad_scaleup},	/* ddec */
{0x8C04,	0x8C06,	bad_scaledown, bad_scaleup},	/* dstk */
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
Face: This opcode form is indistinguishable from S3. We're not likely to
see it any time soon, so ignore for now. Seeing how presumably the silicon
itself can't tell the difference, I assume a datasheet bug
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
{0xC6E,		0x1FFF,	bad_scaledown, bad_scaleup},	/* unop */
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

int
scaleup_doff4(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode)
{
	int dsz, i, reg, base, offs, s, t, sz, ls;

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

	return 0;
}

/* XXX - this can probably be merged into scaleup_doff4 */
int
scaleup_dind(uint16_t opcode, uint32_t hdr, uint32_t *out_opcode)
{
	int dsz, i, reg, base, idx, s, t, sz, ls;

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
