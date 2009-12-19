/* Copyright blah */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"
#include "coff/tic64x.h"

#define UNUSED(x) ((x) = (x))

static void print_insn(struct tic64x_op_template *templ, uint32_t opcode,
					struct disassemble_info *info);

int
print_insn_tic64x(bfd_vma addr, struct disassemble_info *info)
{
	struct tic64x_op_template *templ;
	bfd_byte opbuf[4];
	uint32_t opcode;
	int ret;

	ret = info->read_memory_func(addr, opbuf, 4, info);
	if (ret) {
		info->memory_error_func(ret, addr, info);
		return -1;
	}

	opcode = bfd_getl32(opbuf);

	for (templ = tic64x_opcodes; templ->mnemonic; templ++) {
		if ((opcode & templ->opcode_mask) == templ->opcode)
			break;
	}

	if (templ->mnemonic == NULL) {
		info->fprintf_func(info->stream, "????");
		return 0;
	}

	print_insn(templ, opcode, info);
	return 0;
}

void
print_insn(struct tic64x_op_template *templ, uint32_t opcode,
				struct disassemble_info *info)
{

	/* All instructions have 'p' bit AFAIK */
	if (opcode & TIC64X_BIT_PARALLEL)
		info->fprintf_func(info->stream, "||");
	else
		info->fprintf_func(info->stream, "  ");

	/* Conditional? */
	if (templ->flags & TIC64X_OP_COND) {
		info->fprintf_func(info->stream, "[");

		if (opcode & TIC64X_BIT_Z)
			info->fprintf_func(info->stream, "!");
		else
			info->fprintf_func(info->stream, " ");

		switch (opcode >> TIC64X_SHIFT_CREG) {
		case 1:
			info->fprintf_func(info->stream, "B0");
			break;
		case 2:
			info->fprintf_func(info->stream, "B1");
			break;
		case 3:
			info->fprintf_func(info->stream, "B2");
			break;
		case 4:
			info->fprintf_func(info->stream, "A1");
			break;
		case 5:
			info->fprintf_func(info->stream, "A2");
			break;
		case 6:
			info->fprintf_func(info->stream, "A0");
			break;
		default:
			info->fprintf_func(info->stream, "??");
			break;
		}

		info->fprintf_func(info->stream, "]");
	} else {
		info->fprintf_func(info->stream, "     ");
	}

	/* Mnemonic */
	info->fprintf_func(info->stream, "%-7s", templ->mnemonic);
}
