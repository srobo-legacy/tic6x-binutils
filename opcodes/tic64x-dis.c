/* Copyright blah */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"
#include "coff/tic64x.h"

#define UNUSED(x) ((x) = (x))

#define OPERAND_LENGTH_FORMAT "-15"

#define TXTOPERAND_CAN_XPATH(templ, type)				\
		((((templ)->flags & TIC64X_OP_XPATH_SRC2) &&		\
					(type) == tic64x_optxt_srcreg2) ||\
		(!((templ)->flags & TIC64X_OP_XPATH_SRC2) &&	\
					(type) == tic64x_optxt_srcreg1))


static void print_insn(struct tic64x_op_template *templ, uint32_t opcode,
					struct disassemble_info *info);

typedef void (op_printer) (struct tic64x_op_template *t,
				uint32_t opcode, struct disassemble_info *i,
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
	if (tic64x_get_operand(opcode, tic64x_operand_p, 0))
		info->fprintf_func(info->stream, "||");
	else
		info->fprintf_func(info->stream, "  ");

	/* Conditional? */
	if (templ->flags & TIC64X_OP_COND) {
		info->fprintf_func(info->stream, "[");

		if (tic64x_get_operand(opcode, tic64x_operand_z, 0))
			info->fprintf_func(info->stream, "!");
		else
			info->fprintf_func(info->stream, " ");

		switch (tic64x_get_operand(opcode, tic64x_operand_creg, 0)) {
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

			if (tic64x_get_operand(opcode, tic64x_operand_y, 0)) {
				unit_no = '2';
			} else {
				unit_no = '1';
			}
		} else if (templ->flags & TIC64X_OP_SIDE) {
			if (tic64x_get_operand(opcode, tic64x_operand_s, 0)) {
				unit_no = '1';
			} else {
				unit_no = '0';
			}
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
			if (tic64x_get_operand(opcode, tic64x_operand_s, 0)) {
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
	if (templ->flags & TIC64X_OP_USE_XPATH &&
				tic64x_get_operand(opcode, tic64x_operand_x, 0))
		xpath = 'X';
	else
		xpath = ' ';

	info->fprintf_func(info->stream, ".%C%C%C%C%C  ", unit, unit_no,
						tchar, memnum, xpath);

	/* Now put some operands out - needs some abstraction though */
	for (i = 0; i < TIC64X_MAX_TXT_OPERANDS; i++) {
		for (j = 0; operand_printers[j].print != NULL; j++) {
			if (operand_printers[j].type == templ->textops[i]) {
				operand_printers[j].print(templ, opcode, info,
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

void
print_op_none(struct tic64x_op_template *t ATTRIBUTE_UNUSED,
		uint32_t opcode ATTRIBUTE_UNUSED,
		struct disassemble_info *info ATTRIBUTE_UNUSED,
		enum tic64x_text_operand type ATTRIBUTE_UNUSED)
{

	return;
}

void
print_op_memaccess(struct tic64x_op_template *t, uint32_t opcode,
		struct disassemble_info *info,
		enum tic64x_text_operand type ATTRIBUTE_UNUSED)
{
	const char *pre, *regchar, *post, *offs;
	int addrmode, basereg, offset, scale, scalenum, y;
	char finalstr[16];
	char offsetstr[8];
	char regno[4];

	pre = NULL;
	regchar = NULL;
	post = NULL;
	offs = NULL;
	/* Hopefully this won't become the pile of pain the assembler
	 * version is */

	addrmode = tic64x_get_operand(opcode, tic64x_operand_addrmode, 0);
	basereg = tic64x_get_operand(opcode, tic64x_operand_basereg, 0);

	/* Offset is always ucst5 or register, no sign extension */
	offset = tic64x_get_operand(opcode, tic64x_operand_rcoffset, 0);

	/* scale bit? Might not have one, read it anyway */
	scale = tic64x_get_operand(opcode, tic64x_operand_rcoffset, 0);

	/* Memory accesses set destination side with s bit, source regs with
	 * y bit, which we'll read here. Confirm later TIX64X_OP_UNITNO is set*/
	y = tic64x_get_operand(opcode, tic64x_operand_rcoffset, 0);

	/* Pre inc/decrementer, or offset +/- */
	if (!(addrmode & TIC64X_ADDRMODE_MODIFY)) {
		if (addrmode & TIC64X_ADDRMODE_POS) {
			pre =  "+";
		} else {
			pre = "-";
		}
	} else if (!(addrmode & TIC64X_ADDRMODE_POST)) {
		if (addrmode & TIC64X_ADDRMODE_POS) {
			pre = "++";
		} else  {
			pre = "--";
		}
	} else {
		pre = "";
	}

	/* Base register */
	if (!(t->flags & TIC64X_OP_UNITNO)) {
		fprintf(stderr, "tic64x_print_memaccess: \"%s\" has no y bit?",
								t->mnemonic);
		regchar = "?";
	} else if (y) {
		regchar = "B";
	} else {
		regchar = "A";
	}

	/* Register number itself */
	snprintf(regno, 3, "%d", basereg);

	/* Post modifiers */
	if (addrmode & TIC64X_ADDRMODE_POST) {
		if (addrmode & TIC64X_ADDRMODE_MODIFY) {
			if (addrmode & TIC64X_ADDRMODE_POS) {
				post = "++";
			} else {
				post = "--";
			}
		} else {
			fprintf(stderr, "tic64x_print_memaccess: \"%s\" with "
					"post-nomodifying address mode?",
								t->mnemonic);
			post = "";
		}
	}

	/* Check whether or not scaling is forced */
	if (t->flags & TIC64X_OP_MEMACC_SCALE)
		scale = 1;

	/* If we scale, calculate by how much */
	if (scale) {
		scalenum = (addrmode & TIC64X_OP_MEMSZ_MASK) >>
							TIC64X_OP_MEMSZ_SHIFT;
		scalenum = 1 << scalenum;
	} else {
		scalenum = 1;
	}

	/* What kind of offset? */
	if (addrmode & TIC64X_ADDRMODE_REGISTER) {
		if (scale) {
			snprintf(offsetstr, 7, "%s%d*%d", regchar, offset,
								scalenum);
		} else {
			snprintf(offsetstr, 7, "%s%d", regchar, offset);
		}
	} else { /* Constant */
		if (offset == 0) {
			snprintf(offsetstr, 7, "%c", 0);
		} else if (scale) {
			snprintf(offsetstr, 7, "%d", offset * scalenum);
		} else {
			snprintf(offsetstr, 7, "%d", offset);
		}
	}

	/* And I think that's it */
	if (*offsetstr == 0) {
		snprintf(finalstr, 15, "*%s%s%s%s,", pre, regchar, regno, post);
	} else {
		snprintf(finalstr, 15, "*%s%s%s%s[%s],", pre, regchar, regno,
							post, offsetstr);
	}

	info->fprintf_func(info->stream, "%"OPERAND_LENGTH_FORMAT"s", finalstr);
	return;
}

void
print_op_register(struct tic64x_op_template *t, uint32_t opcode,
		struct disassemble_info *info, enum tic64x_text_operand type)
{
	enum tic64x_operand_type t2;
	int regnum, side, x, y;
	char finalstr[16], unitchar;

	if (type == tic64x_optxt_dstreg) {
		t2 = tic64x_operand_dstreg;
	} else if (type == tic64x_optxt_srcreg1) {
		t2 = tic64x_operand_srcreg1;
	} else if (type == tic64x_optxt_srcreg2) {
		t2 = tic64x_operand_srcreg2;
	} else {
		fprintf(stderr, "tic64x print_op_register called with invalid "
				"operand type\n");
		info->fprintf_func(info->stream,"%"OPERAND_LENGTH_FORMAT"s","");
		return;
	}

	if (!(t->flags & TIC64X_OP_SIDE)) {
		fprintf(stderr, "tic64x print_op_register: \"%s\" has no "
					"side bit?", t->mnemonic);
		info->fprintf_func(info->stream,"%"OPERAND_LENGTH_FORMAT"s","");
		return;
	}

	regnum = tic64x_get_operand(opcode, t2, 0);
	side = tic64x_get_operand(opcode, tic64x_operand_s, 0);
	x = tic64x_get_operand(opcode, tic64x_operand_x, 0);
	y = tic64x_get_operand(opcode, tic64x_operand_y, 0);

	/* Delicious curly braces... */
	unitchar = '?';
	if (!(t->flags & TIC64X_OP_UNITNO)) {
		/* insn doesn't have y bit, so ignore that */
		if (t->flags & TIC64X_OP_FIXED_UNITNO) {
			/* insn fixes what side it executes on... */
			if (t->flags & TIC64X_OP_FIXED_UNIT2) {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'A';
				} else {
					unitchar = 'B';
				}
			} else {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'B';
				} else {
					unitchar = 'A';
				}
			}
		} else if (t->flags & TIC64X_OP_SIDE) {
			if (side) {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'A';
				} else {
					unitchar = 'B';
				}
			} else {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'B';
				} else {
					unitchar = 'A';
				}
			}
		}
	} else {
		/* We use a y bit to specify which side we execute on (s says
		 * where result is stored); so if we're a src, use y, if dst,
		 * use s */
		if (type == tic64x_optxt_dwsrc) {
			if (y) {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'A';
				} else {
					unitchar = 'B';
				}
			} else {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'B';
				} else {
					unitchar = 'A';
				}
			}
		} else {
			if (side) {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'A';
				} else {
					unitchar = 'B';
				}
			} else {
				if (TXTOPERAND_CAN_XPATH(t, type) && x) {
					unitchar = 'B';
				} else {
					unitchar = 'A';
				}
			}
		}
	}
	/* If there's a better way, lack of coffee prevents me seeing it */

	snprintf(finalstr, 15, "%c%d", unitchar, regnum);
	info->fprintf_func(info->stream, "%"OPERAND_LENGTH_FORMAT"s", finalstr);
	return;
}

void
print_op_dwreg(struct tic64x_op_template *t, uint32_t opcode,
		struct disassemble_info *info, enum tic64x_text_operand type)
{
	enum tic64x_operand_type t2;
	int regnum, side, y, i;
	char unitchar, finalstr[16];

	t2 = tic64x_operand_invalid;
	unitchar = '?';

	for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
		if ((t->operands[i] == tic64x_operand_dwdst4 &&
						type == tic64x_optxt_dwdst) ||
		    (t->operands[i] == tic64x_operand_dwdst5 &&
						type == tic64x_optxt_dwdst) ||
		    (t->operands[i] == tic64x_operand_dwsrc &&
						type == tic64x_optxt_dwsrc)) {
			t2 = t->operands[i];
			break;
		}
	}

	if (i == TIC64X_MAX_OPERANDS) {
		fprintf(stderr, "tic64x print_op_dwreg: \"%s\" has no matching "
				"dword reg operand\n", t->mnemonic);
		info->fprintf_func(info->stream,"%"OPERAND_LENGTH_FORMAT"s","");
		return;
	}

	if (!(t->flags & TIC64X_OP_SIDE)) {
		fprintf(stderr, "tic64x print_op_register: \"%s\" has no "
					"side bit?", t->mnemonic);
		info->fprintf_func(info->stream,"%"OPERAND_LENGTH_FORMAT"s","");
		return;
	}

	regnum = tic64x_get_operand(opcode, t2, 0);
	side = tic64x_get_operand(opcode, tic64x_operand_s, 0);
	y = tic64x_get_operand(opcode, tic64x_operand_y, 0);

	/* Delicious curly braces... */
	if (!(t->flags & TIC64X_OP_UNITNO)) {
		/* insn doesn't have y bit, so ignore that */
		if (t->flags & TIC64X_OP_FIXED_UNITNO) {
			/* insn fixes what side it executes on... */
			if (t->flags & TIC64X_OP_FIXED_UNIT2) {
				unitchar = 'B';
			} else {
				unitchar = 'A';
			}
		} else if (t->flags & TIC64X_OP_SIDE) {
			/* We can use side bit to determine where this is read
			 * from. AFAIK you can't read dw reg's over xpath,
			 * so we can ignore that situation */
			if (side) {
				unitchar = 'B';
			} else {
				unitchar = 'A';
			}
		}
	} else {
		/* We use a y bit to specify which side we execute on (s says
		 * where result is stored); so if we're a src, use y, if dst,
		 * use s */
		if (type == tic64x_optxt_dwsrc) {
			if (y) {
				unitchar = 'B';
			} else {
				unitchar = 'A';
			}
		} else {
			if (side) {
				unitchar = 'B';
			} else {
				unitchar = 'A';
			}
		}
	}

	snprintf(finalstr, 15, "%c%d:%c%d", unitchar, regnum, unitchar,
								regnum+1);
	info->fprintf_func(info->stream, "%"OPERAND_LENGTH_FORMAT"s", finalstr);
	return;
}

void
print_op_constant(struct tic64x_op_template *t, uint32_t opcode,
		struct disassemble_info *info, enum tic64x_text_operand type)
{
}
