/* Copyright blah */

#ifndef _OPCODE_TIC64X_H_
#define _OPCODE_TIC64X_H_

/* Placeholder header for information about tic64x instructions and their
 * format, and what facilities are exported to gas / whatever. To all be
 * implemented whenever its all understood. For now, declare get_insn */

struct disassemble_info;
extern const insn_template *tic64x_get_insn (struct disassemble_info * dis,
					bfd_vma, unsigned short, int *);

#endif /* _OPCODE_TIC64X_H_ */
