/* Copyright blah */

#ifndef _OPCODE_TIC64X_H_
#define _OPCODE_TIC64X_H_
#include <stdint.h>

struct tic64x_register {
	char *name;
#define TIC64X_REG_UNIT2	0x100	/* What unit are we, 1 or 2? */
	int num;
};

struct tic64x_operand_pos {
	int position;
	int size;
};
extern struct tic64x_operand_pos tic64x_operand_positions[];

/* Caution - this enum is used as an index to a table in tic64x-opc.c, if
 * adding a new operand type or changing ordering, also modify operand
 * position/size table to reflect changes. */
enum tic64x_operand_type {
	tic64x_operand_invalid = 0,
	tic64x_operand_addrmode,		/* Addressing mode field */
	tic64x_operand_dstreg,			/* Destination register */
	tic64x_operand_dwdst4,			/* Doubleword reg destination,*/
	tic64x_operand_dwdst5,			/* pair or lowest address */
	tic64x_operand_basereg,			/* Base address register, l/s */
	tic64x_operand_rcoffset,		/* Register/Constant offset */
	tic64x_operand_scale,			/* Scale bit for rcoffset */
	tic64x_operand_x,			/* Crosspath bit */
	tic64x_operand_const5,			/* Generic constants */
	tic64x_operand_const5p2,		/* 5 bit, in position 2 (<<18)*/
	tic64x_operand_const21,
	tic64x_operand_const16,
	tic64x_operand_const15,
	tic64x_operand_const12,
	tic64x_operand_const10,
	tic64x_operand_const7,
	tic64x_operand_const4,
	tic64x_operand_bitfldb,			/* Bitfield specifier, see clr*/
	tic64x_operand_nops,			/* Number of nops to execute */
	tic64x_operand_srcreg1,			/* Generic source registers */
	tic64x_operand_srcreg2,			/* one and two, any use */
	tic64x_operand_dwsrc,			/* Doubleword source regs */
	tic64x_operand_p,			/* Parallel bit */
	tic64x_operand_s,			/* Side bit */
	tic64x_operand_y,			/* Memaccess srcs side bit */
	tic64x_operand_z,			/* Conditional zero test */
	tic64x_operand_creg,			/* Conditional reg num */
	tic64x_operand_data8,
	tic64x_operand_data16,
	tic64x_operand_data32
};

/* Represent operand in text - simplifies how parser/disassembler works out
 * what to expect when dealing with an instruction */
enum tic64x_text_operand {
	tic64x_optxt_none = 0,
	tic64x_optxt_memaccess,
	tic64x_optxt_memrel15,	/* see note */
	tic64x_optxt_dstreg,
	tic64x_optxt_srcreg1,
	tic64x_optxt_srcreg2,
	tic64x_optxt_dwdst,
	tic64x_optxt_dwsrc,
	tic64x_optxt_dwsrc2,	/* dwsrc, but in src1 position. '2' because
				 * it's uncommon */
	tic64x_optxt_uconstant,
	tic64x_optxt_sconstant,
	tic64x_optxt_nops,
	tic64x_optxt_bfield	/* Double operand for clr insn */
};

/* Note: So, memaccess and memrel15 operands can be near indistinguishable,
 * and if we pick memaccess and hit something that needs relocating or is large,
 * user will look like this: >:|. So, put memrel15 opcodes ahead of memaccess
 * ones in opcode table, to ensure they're selected in ambiguous circumtances.
 *
 * The correct solution is see in a memaccess opcode that this could need to
 * be bumped up to memrel15, and either switch there, or reserve some space
 * in the fragment and come back to it later. Which is more complicated. So
 * avoid that for now.
 *
 * Unfortunately this isn't going to end with the user getting enlightening
 * error messages */

#define TIC64X_ADDRMODE_NEG		0
#define TIC64X_ADDRMODE_POS		1
#define TIC64X_ADDRMODE_PRE		0
#define TIC64X_ADDRMODE_POST		2
#define TIC64X_ADDRMODE_OFFSET		0
#define TIC64X_ADDRMODE_REGISTER	4
#define TIC64X_ADDRMODE_NOMODIFY	0
#define TIC64X_ADDRMODE_MODIFY		8

struct tic64x_op_template {
	char *mnemonic;
	uint32_t opcode;		/* opcode bits */
	uint32_t opcode_mask;		/* mask of which opcode bits are valid*/
	uint32_t flags;			/* Some flags - operands that are always
					 * in exactly the same place but not
					 * necessarily present in all insns */
#define TIC64X_OP_UNIT_D	1
#define TIC64X_OP_UNIT_L	2
#define TIC64X_OP_UNIT_S	4
#define TIC64X_OP_UNIT_M	8	/* Pretty self explanitory */

#define TIC64X_OP_UNITNO	0x10	/* Insn has 'y' bit at bit 7, specifying
					 * unit 1 or 2 */
#define TIC64X_OP_NOCOND	0x20	/* Doesn't have conditional execution
					 * field, almost all insns have them. */
#define TIC64X_OP_NOSIDE	0x40	/* Has no side bit */
#define TIC64X_OP_MEMACCESS	0x80	/* Accesses memory, ie is a load or
					 * store instruction. Used to work out
					 * when we should have a T1 or T2 suffix
					 * to the execution unit specifier */
#define TIC64X_OP_FIXED_UNITNO	0x100	/* Instruction can only execute on one
					 * side of processor */
#define TIC64X_OP_FIXED_UNIT2	0x200	/* Insn only executes on unit 2; not
					 * set means unit 1 */
#define TIC64X_OP_MEMSZ_MASK	0xC00	/* Mask for finding memory access size
					 * power */
#define TIC64X_OP_MEMSZ_SHIFT	10	/* Values need to be shr'd 8 bits... */
#define TIC64X_OP_MEMSZ_MASK	0xC00	/* Mask to select memsz */
#define TIC64X_OP_MEMSZ_BYTE	0x0
#define TIC64X_OP_MEMSZ_HWORD	0x400
#define TIC64X_OP_MEMSZ_WORD	0x800
#define TIC64X_OP_MEMSZ_DWORD	0xC00

#define TIC64X_OP_MULTI_MNEMONIC 0x1000	/* This instruction is the first in a
					 * set that all have the same mnemonic
					 * but are actually different templates:
					 * md_assemble expects this to come
					 * first */

#define TIC64X_OP_XPATH_SRC1	0x2000	/* src1 is xpath */
#define TIC64X_OP_XPATH_SRC2	0x4000	/* src2 is xpath */
/* 0x8000 rescinded and free */
#define TIC64X_OP_MEMACC_SBIT	0x10000	/* Scale bit exists in this insn.
					 * Needs a rename */
#define TIC64X_OP_CONST_SCALE 	0x20000 /* branch instructions and the like,
					 * particularly memory access insns
					 * always scale their offsets by a
					 * certain amount - use memsz flags
					 * to specify by how much, and this
					 * flag to indicate const is scaled */
#define TIC64X_OP_CONST_PCREL	0x40000 /* Constant, whatever it may be,
					 * is PC relative */
#define TIC64X_OP_BITFIELD	0x80000	/* Instruction contains a bitfield
					 * operand, making this four text
					 * operands */
#define TIC64X_OP_USE_TOP_HWORD	0x100000/* Some insns only use the top 16 bits
					 * of the constant operand - such as
					 * mvkh */
#define TIC64X_OP_NO_RANGE_CHK	0x200000/* Don't range check constant when
					 * inserting into opcode - for some
					 * things like mvkh and mvkl, this is
					 * entirely valid */

/* Instruction can execute on any unit - no point having it as a special flag*/
#define TIC64X_OP_ALL_UNITS (TIC64X_OP_UNIT_S | TIC64X_OP_UNIT_L |\
				TIC64X_OP_UNIT_D | TIC64X_OP_UNIT_M)


#define TIC64X_MAX_TXT_OPERANDS	3
	enum tic64x_text_operand textops[TIC64X_MAX_TXT_OPERANDS];
#define TIC64X_MAX_OPERANDS	2
	enum tic64x_operand_type operands[TIC64X_MAX_OPERANDS];
};

extern struct tic64x_op_template tic64x_opcodes[];
extern struct tic64x_op_template tic64x_mv_template[];
extern struct tic64x_register tic64x_regs[];

/* get/set calls for actual operands. Returns nonzero if value is too large */
int tic64x_set_operand(uint32_t *opcode, enum tic64x_operand_type type,
						 int value, int is_signed);
int tic64x_get_operand(uint32_t opcode,  enum tic64x_operand_type t, int signx);

/* Utility macros(s) */

#define UNITCHAR_2_FLAG(x) ((x) == 'D' ? TIC64X_OP_UNIT_D :		\
				(x) == 'L' ? TIC64X_OP_UNIT_L :		\
				(x) == 'S' ? TIC64X_OP_UNIT_S :		\
				TIC64X_OP_UNIT_M)

#define UNITFLAGS_2_CHAR(o) ((o) & TIC64X_OP_UNIT_D ? 'D' :		\
				(o) & TIC64X_OP_UNIT_L ? 'L' :		\
				(o) & TIC64X_OP_UNIT_S ? 'S' :		\
				'M')



/* 16 bit opcode handlers - Opcode and mask for identifying when disassembling,
 * and scale-up / scale-down routines for munging between 16/32 bit opcode
 * formats. There's no good way to match a 32 bit instruction against a 16 bit
 * format, XXX so opcode table will need to store an index corresponding to
 * the compact form */

/* Names taken from ti spec. Caution - this enum is used to index a table
 * in tic64x-opc.c, if making a change here in ordering or adding a new fmt,
 * make sure these changes reflect in compact opcode table. */
enum tic64x_compact_fmt {
	tic64x_cfmt_invalid = 0,
	tic64x_cfmt_doff4,
	tic64x_cfmt_dind,
	tic64x_cfmt_dinc,
	tic64x_cfmt_ddec,
	tic64x_cfmt_dstk,
	tic64x_cfmt_dx2op,
	tic64x_cfmt_dx5,
	tic64x_cfmt_dx5p,
	tic64x_cfmt_dx1,
	tic64x_cfmt_dpp,
	tic64x_cfmt_l3,
	tic64x_cfmt_l3i,
	tic64x_cfmt_ltbd, /* Looks like that's "To be decided..." */
	tic64x_cfmt_l2c,
	tic64x_cfmt_lx5,
	tic64x_cfmt_lx3c,
	tic64x_cfmt_lx1c,
	tic64x_cfmt_lx1,
	tic64x_cfmt_m3,
	tic64x_cfmt_sbs7,
	tic64x_cfmt_sbu8,
#if 0
see comment in tic64x-opc.c for why this is commented out
	tic64x_cfmt_scs10,
#endif
	tic64x_cfmt_sbs7c,
	tic64x_cfmt_sbu8c,
	tic64x_cfmt_s3,
	tic64x_cfmt_s3i,
	tic64x_cfmt_smvk8,
	tic64x_cfmt_ssh5,
	tic64x_cfmt_s2sh,
	tic64x_cfmt_sc5,
	tic64x_cfmt_s2ext,
	tic64x_cfmt_sx2op,
	tic64x_cfmt_sx5,
	tic64x_cfmt_sx1,
	tic64x_cfmt_sx1b,
	tic64x_cfmt_lsd_mvto,
	tic64x_cfmt_lsd_mvfr,
	tic64x_cfmt_lsd_x1c,
	tic64x_cfmt_lsd_x1,
	tic64x_cfmt_uspl,
	tic64x_cfmt_uspldr,
	tic64x_cfmt_uspk,
	tic64x_cfmt_uspm,
	tic64x_cfmt_unop
};

struct tic64x_compact_table {
	uint16_t opcode;
	uint16_t opcode_mask;
	int (*scale_down) (uint32_t opcode, uint16_t *out);
	int (*scale_up) (uint16_t opcode, uint32_t hdr, uint32_t *out_opcode);
};

extern struct tic64x_compact_table tic64x_compact_formats[];

#endif /* _OPCODE_TIC64X_H_ */
