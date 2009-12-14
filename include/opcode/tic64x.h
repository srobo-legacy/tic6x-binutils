/* Copyright blah */

#ifndef _OPCODE_TIC64X_H_
#define _OPCODE_TIC64X_H_

struct tic64x_op_template {
	char *mnemonic;
	uint32_t opcode;		/* opcode bits */
	uint32_t opcode_mask;		/* mask of which opcode bits are valid*/
	/* Insert here - actual data */
};

typedef int insn_template; /* Build hack, not intended for functional use */
struct disassemble_info;
extern const insn_template *tic64x_get_insn (struct disassemble_info *dis,
					bfd_vma, unsigned short, int *);

#endif /* _OPCODE_TIC64X_H_ */
