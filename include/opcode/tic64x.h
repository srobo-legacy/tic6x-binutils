/* Copyright blah */

#ifndef _OPCODE_TIC64X_H_
#define _OPCODE_TIC64X_H_

struct tic64x_register {
	char *name;
#define TIC64X_REG_UNIT2	0x100	/* What unit are we, 1 or 2? */
	int num;
};

struct tic64x_operand {
#define TIC64X_OPERAND_INVALID		0	/* Not valid */
	int type;
	int position;		/* Location in opcode, bits from zero */
	int size;		/* Size of operand in bits */
};

struct tic64x_op_template {
	char *mnemonic;
	uint32_t flags;			/* Some flags - operands that are always
					 * in exactly the same place but not
					 * necessarily present in all insns */
#define TIC64X_OP_UNIT_MASK	3	/* Bit mask for finding instructions
					 * execution unit */
#define TIC64X_OP_UNIT_D	0
#define TIC64X_OP_UNIT_L	1
#define TIC64X_OP_UNIT_S	2
#define TIC64X_OP_UNIT_M	3	/* Pretty self explanitory */

#define TIC64X_OP_UNITNO	4	/* Insn has 'y' bit at bit 7, specifying
					 * unit 1 or 2 */
#define TIC64X_OP_COND		8	/* Has conditional execution field,
					 * top three bits of insn. Implies
					 * that there's also a 'z' field */
#define TIC64X_OP_SIDE		0x10	/* 'Side' A/B for destination register*/

	uint32_t opcode;		/* opcode bits */
	uint32_t opcode_mask;		/* mask of which opcode bits are valid*/

	struct tic64x_operand operands[4];
};

extern struct tic64x_op_template tic64x_opcodes[];
extern struct tic64x_register tic64x_regs[];

struct disassemble_info;
extern const struct tic64x_op_template *tic64x_get_insn (
					struct disassemble_info *dis,
					bfd_vma, unsigned short, int *);

#endif /* _OPCODE_TIC64X_H_ */
