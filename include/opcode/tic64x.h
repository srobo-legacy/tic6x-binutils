/* Copyright blah */

#ifndef _OPCODE_TIC64X_H_
#define _OPCODE_TIC64X_H_

struct tic64x_register {
	char *name;
#define TIC64X_REG_UNIT2	0x100	/* What unit are we, 1 or 2? */
	int num;
};

enum tic64x_operand_type {
	tic64x_operand_invalid = 0,
	tic64x_operand_addrmode,		/* Addressing mode field */
	tic64x_operand_dstreg,			/* Destination register */
	tic64x_operand_dwdst,			/* Doubleword reg destination */
	tic64x_operand_basereg,			/* Base address register, l/s */
	tic64x_operand_rcoffset,		/* Register/Constant offset */
	tic64x_operand_scale,			/* Scale bit for rcoffset */
	tic64x_operand_vconstant,		/* Generic constant, any size */
	tic64x_operand_srcreg1,			/* Generic source registers */
	tic64x_operand_srcreg2,			/* one and two, any use */
	tic64x_operand_dwsrc			/* Doubleword source regs */
};

/* Represent operand in opcode */
struct tic64x_operand {
	enum tic64x_operand_type type;
	int position;		/* Location in opcode, bits from zero */
	int size;		/* Size of operand in bits */
};

/* Represent operand in text - simplifies how parser/disassembler works out
 * what to expect when dealing with an instruction */
enum tic64x_text_operand {
	tic64x_optxt_none = 0,
	tic64x_optxt_memaccess,
	tic64x_optxt_destreg,
	tic64x_optxt_srcreg1,
	tic64x_optxt_srcreg2,
	tic64x_optxt_dwdst,
	tic64x_optxt_dwsrc,
	tic64x_optxt_uconstant,
	tic64x_optxt_sconstant
};

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
#define TIC64X_OP_COND		0x20	/* Has conditional execution field,
					 * top three bits of insn. Implies
					 * that there's also a 'z' field */
#define TIC64X_OP_SIDE		0x40	/* 'Side' A/B for destination register*/
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
#define TIC64X_OP_MEMSZ_BYTE	0x0
#define TIC64X_OP_MEMSZ_HWORD	0x400
#define TIC64X_OP_MEMSZ_WORD	0x800
#define TIC64X_OP_MEMSZ_DWORD	0xC00

#define TIC64X_OP_MULTI_MNEMONIC 0x1000	/* This instruction is the first in a
					 * set that all have the same mnemonic
					 * but are actually different templates:
					 * md_assemble expects this to come
					 * first */

#define TIC64X_OP_USE_XPATH	0x2000	/* An operand uses cross path, must
					 * have x bit 12 in opcode */
#define TIC64X_OP_XPATH_SRC1	0x0000	/* Not set -> src1 */
#define TIC64X_OP_XPATH_SRC2	0x4000	/* Set -> src2 is xpath */


#define TIC64X_MAX_TXT_OPERANDS	3
	enum tic64x_text_operand textops[TIC64X_MAX_TXT_OPERANDS];
#define TIC64X_MAX_OPERANDS	5
	struct tic64x_operand operands[TIC64X_MAX_OPERANDS];
};

extern struct tic64x_op_template tic64x_opcodes[];
extern struct tic64x_register tic64x_regs[];

struct disassemble_info;
extern const struct tic64x_op_template *tic64x_get_insn (
					struct disassemble_info *dis,
					bfd_vma, unsigned short, int *);

#endif /* _OPCODE_TIC64X_H_ */
