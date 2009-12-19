/* Copyright blah */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"
#include "coff/tic64x.h"

#define UNUSED(x) ((x) = (x))

static void print_insn(struct tic64x_op_template *templ, uint32_t opcode,
					struct disassemble_info *info);

typedef void (op_printer) (struct tic64x_op_template *t,
				struct disassemble_info *i,
				enum tic64x_text_operand type);

op_printer print_op_none;
op_printer print_op_memaccess;
op_printer print_op_register;
op_printer print_op_dwreg;
op_printer print_op_constant;

struct {
	enum tic64x_text_operand type;
	op_printer *print;
} operand_printers[] = {
{ tic64x_optxt_none,		print_op_none		},
{ tic64x_optxt_memaccess,	print_op_memaccess	},
{ tic64x_optxt_dstreg,		print_op_register	},
{ tic64x_optxt_srcreg1,		print_op_register	},
{ tic64x_optxt_srcreg2,		print_op_register	},
{ tic64x_optxt_dwdst,		print_op_dwreg		},
{ tic64x_optxt_dwsrc,		print_op_dwreg		},
{ tic64x_optxt_uconstant,	print_op_constant	},
{ tic64x_optxt_sconstant,	print_op_constant	}
};

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
	int i, j;
	char unit, unit_no, tchar, memnum, xpath;

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

	/* XXX XXX XXX - we can't determine which execution unit we're using
	 * if an opcode format supports more than one, as there are no fields
	 * saying where to execute. Presumably the c6x cores themselves decide
	 * on-the-fly where it's going to execute. Anyhoo; only way to do this
	 * is to read an instruction packet and decode it all in one go, which
	 * right now is more trouble that it's worth. So instead just print
	 * whichever unit matches first; this can still be assembled in
	 * exactly the same way. */
	unit = UNITFLAGS_2_CHAR(templ->flags);

	/*  Bleaugh - a million and one ifs. Is there a better way? */
	if (!(templ->flags & TIC64X_OP_FIXED_UNITNO)) {
		/* Re-name required! */
		if (templ->flags & TIC64X_OP_UNITNO) {
			/* Instruction is probably memory access, specifies
			 * the side of processor to use regs / execute on
			 * through 'y' bit */

			if (opcode & TIC64X_BIT_UNITNO) {
				unit_no = '2';
			} else {
				unit_no = '1';
			}
		} else if (templ->flags & TIC64X_OP_SIDE) {
			unit_no = (opcode & TIC64X_BIT_SIDE) ? '2' : '1';
		} else {
			fprintf(stderr, "tic64x print_insn: instruction with "
				"no fixed unit, but not side bit?\n");
			unit_no = '?';
			return;
		}
	} else {
		if (templ->flags & TIC64X_OP_FIXED_UNIT2) {
			unit_no = '2';
		} else {
			unit_no = '1';
		}
	}

	/* T1/T2 specifier? */
	if (templ->flags & TIC64X_OP_MEMACCESS) {
		tchar = 'T';
		if (!(templ->flags & TIC64X_OP_SIDE)) {
			fprintf(stderr, "tic64x print_insn: instruction with "
				"memory access but not dest/side bit?");
			memnum = '?';
		} else {
			if (opcode & TIC64X_BIT_SIDE) {
				memnum = '2';
			} else {
				memnum = '1';
			}
		}
	} else {
		tchar = ' ';
		memnum = ' ';
	}

	/* Cross-path? */
	if (templ->flags & TIC64X_OP_USE_XPATH && opcode & TIC64X_BIT_XPATH)
		xpath = 'X';
	else
		xpath = ' ';

	info->fprintf_func(info->stream, ".%C%C%C%C%C  ", unit, unit_no,
						tchar, memnum, xpath);

	/* Now put some operands out - needs some abstraction though */
	for (i = 0; i < TIC64X_MAX_TXT_OPERANDS; i++) {
		for (j = 0; operand_printers[j].print != NULL; j++) {
			if (operand_printers[j].type == templ->textops[i]) {
				operand_printers[j].print(templ, info,
							templ->textops[i]);
				break;
			}
		}

		if (operand_printers[j].print == NULL)
			fprintf(stderr, "tic64x print_insn, couldn't find "
					"printer for \"%s\" operand %d\n",
							templ->mnemonic, i);
	}
}
