/* Copyright blah */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"
#include "coff/tic64x.h"

#define UNUSED(x) ((x) = (x))

static void print_insn(struct tic64x_op_template *templ, uint32_t opcode);

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
		if ((opcode & templ->operand_mask) == templ->opcode)
			break;
	}

	if (templ->mnemonic == NULL) {
		info->fprintf_func(info->stream, "????");
		return 0;
	}

	print_insn(templ, opcode);
	return 0;
}
