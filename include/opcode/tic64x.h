/* Copyright blah */

#ifndef _OPCODE_TIC64X_H_
#define _OPCODE_TIC64X_H_

struct tic64x_register {
	char *name;
#define TIC64X_REG_UNIT2	0x100	/* What unit are we, 1 or 2? */
	int num;
};

struct tic64x_op_template {
	char *mnemonic;
	uint32_t opcode;		/* opcode bits */
	uint32_t opcode_mask;		/* mask of which opcode bits are valid*/
	/* Insert here - actual data */
};

extern struct tic64x_op_template tic64x_opcodes[];
extern struct tic64x_register tic64x_regs[];

struct disassemble_info;
extern const struct tic64x_op_template *tic64x_get_insn (
					struct disassemble_info *dis,
					bfd_vma, unsigned short, int *);

#endif /* _OPCODE_TIC64X_H_ */
