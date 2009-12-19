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
	info->fprintf_func(info->stream, "%-8s", templ->mnemonic);

	/* Unit, no, xpath, memunit */
	info->fprintf_func(info->stream, ".");

	/* XXX XXX XXX - we can't determine which execution unit we're using
	 * if an opcode format supports more than one, as there are no fields
	 * saying where to execute. Presumably the c6x cores themselves decide
	 * on-the-fly where it's going to execute. Anyhoo; only way to do this
	 * is to read an instruction packet and decode it all in one go, which
	 * right now is more trouble that it's worth. So instead just print
	 * whichever unit matches first; this can still be assembled in
	 * exactly the same way. */
	info->fprintf_func(info->stream, "%C", UNITFLAGS_2_CHAR(templ->flags));

	/*  Bleaugh - a million and one ifs. Is there a better way? */
	if (!(templ->flags & TIC64X_OP_FIXED_UNITNO) {
		/* Re-name required! */
		if (templ->flags & TIC64X_OP_UNITNO) {
			/* Instruction is probably memory access, specifies
			 * the side of processor to use regs / execute on
			 * through 'y' bit */

			if (opcode & TIC64X_BIT_UNITNO) {
				info->fprintf_func(info->stream, "2");
			} else {
				info->fprintf_func(info->stream, "1");
			}
		} else if (templ->flags & TIC64X_OP_SIDE) {
			info->fprintf_func(info->stream, "%d",
				(opcode & TIC64X_BIT_SIDE) ? 2 : 1);
		} else {
			fprintf(stderr, "tic64x print_insn: instruction with "
				"no fixed unit, but not side bit?\n");
			info->fprintf_func(info->stream, "?");
			return;
		}
	} else {
		if (templ->flags & TIC64X_OP_FIXED_UNIT_2) {
			info->fprintf_func(info->stream, "2");
		} else {
			info->fprintf_func(info->stream, "1");
		}
	}

	/* T1/T2 specifier? */
	if (templ->flags & TIC64X_OP_MEMACCESS) {
		if (!(templ->flags & TIC64X_OP_SIDE)) {
			fprintf(stderr, "tic64x print_insn: instruction with "
				"memory access but not dest/side bit?");
			info->fprintf_func(info->stream, "T?");
		} else {
			if (opcode & TIC64X_BIT_SIDE) {
				info->fprintf_func(info->stream, "T2");
			} else {
				info->fprintf_func(info->stream, "T1");
			}
		}
	}

	/* Cross-path? */
	if (templ->flags & TIC64X_OP_USE_XPATH && opcode & TIC64X_BIT_XPATH)
		info->fprintf_func(info->stream, "X");

}
