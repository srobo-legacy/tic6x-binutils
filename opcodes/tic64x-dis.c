/* Copyright blah */

#include <errno.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"
#include "coff/tic64x.h"

#define UNUSED(x) ((x) = (x))

#define TXTOPERAND_CAN_XPATH(templ, type)				\
		((((templ)->flags & TIC64X_OP_XPATH_SRC2) &&		\
					(type) == tic64x_optxt_srcreg2) ||\
		(((templ)->flags & TIC64X_OP_XPATH_SRC1) &&		\
					(type) == tic64x_optxt_srcreg1))


static void print_insn(struct tic64x_op_template *templ, uint32_t opcode,
					struct disassemble_info *info,
					int double_bar);

typedef void (op_printer) (struct tic64x_op_template *t,
				uint32_t opcode, enum tic64x_text_operand type,
				char *buffer, int len);

op_printer print_op_none;
op_printer print_op_memaccess;
op_printer print_op_memrel15;
op_printer print_op_register;
op_printer print_op_dwreg;
op_printer print_op_constant;

struct {
	enum tic64x_text_operand type;
	op_printer *print;
} operand_printers[] = {
{ tic64x_optxt_none,		print_op_none		},
{ tic64x_optxt_memaccess,	print_op_memaccess	},
{ tic64x_optxt_memrel15,	print_op_memrel15	},
{ tic64x_optxt_dstreg,		print_op_register	},
{ tic64x_optxt_srcreg1,		print_op_register	},
{ tic64x_optxt_srcreg2,		print_op_register	},
{ tic64x_optxt_dwdst,		print_op_dwreg		},
{ tic64x_optxt_dwsrc,		print_op_dwreg		},
{ tic64x_optxt_dwsrc2,		print_op_dwreg		},
{ tic64x_optxt_uconstant,	print_op_constant	},
{ tic64x_optxt_sconstant,	print_op_constant	},
{ tic64x_optxt_nops,		print_op_constant	}
};

struct tic64x_disasm_priv {
	bfd_vma packet_start;
	bfd_vma next_packet;
	uint32_t compact_header; /* Zero on not-compact, nz otherwise */
	int no_pbar_next;	/* Don't print parallel bar in next insn */
};

/* Hack: nonzero when we're in a compact insn. Because right now the method
 * of scaling from a 16 bit insn to 32 bit looses some information (see
 * print_op_constant */
static int in_compact_insn;

int
print_insn_tic64x(bfd_vma addr, struct disassemble_info *info)
{
	struct tic64x_op_template *templ;
	struct tic64x_compact_table *ctable;
	struct tic64x_disasm_priv *priv;
	bfd_byte opbuf[4];
	uint32_t opcode;
	int ret, i, sz, p;

	opcode = 0;
	sz = 4;

	if (!info->private_data) {
		priv = malloc(sizeof(struct tic64x_disasm_priv));
		info->private_data = priv;
		if (!info->private_data) {
			info->memory_error_func(ENOMEM, addr, info);
			return -1;
		}

		priv->packet_start = -1;
		priv->next_packet = 0;
		priv->compact_header = 0;
		priv->no_pbar_next = 1;
	} else {
		priv = info->private_data;
	}

	if (addr >= priv->next_packet) {
		priv->no_pbar_next = 1;
		priv->packet_start = addr;
		priv->compact_header = 0;

		/* Firstly, are we compact? */
		for (i = 0; i < 8; i++) {
			ret = info->read_memory_func(addr + (i * 4),
							opbuf,
							4,
							info);
			if (!ret) {
				opcode = bfd_getl32(opbuf);

				if ((opcode & 0xF0000000) == 0xE0000000) {
					priv->next_packet = addr + (i * 4) + 4;
					priv->compact_header = opcode;
				}
			}
		}

		/* If not compact search for end of p bits */
		if (priv->compact_header == 0) {
			/* We need to search for end of p bits */
			for (i = 0; i < 8; i++) {
				ret = info->read_memory_func(addr + (i * 4),
								opbuf, 4, info);

				if (ret && i == 0) {
					info->memory_error_func(ret, addr,info);
					return -1;
				} else if (ret) {
					i--;	/* Last insn in packet is
						 * wherever the last valid
						 * opcode we read was */
					break;
				}

				opcode = bfd_getl32(opbuf);
				if (!tic64x_get_operand(opcode,
							tic64x_operand_p, 0)) {
					priv->next_packet = addr + (i * 4) + 4;
					break;
				}
			}

			priv->next_packet = addr + (i * 4) + 4;
		}
	}

	/* Actually disassemble something */
	ret = info->read_memory_func(addr, opbuf, 4, info);
	if (ret) {
		info->memory_error_func(ret, addr,info);
		return -1;
	}
	opcode = bfd_getl32(opbuf);
	in_compact_insn = 0;

	if (priv->compact_header) {
		/* Compact packet; are we a compact instruction?
		 * Calculate offset from next packet: compact header will
		 * be that - 4. */

		i = priv->next_packet - addr;
		if (i == 4)
			return 4;	/* Don't disassemble packet header */

		i += 3;
		i &= ~3;		/* Round up to dword boundry */

		i /= 4;			/* dword offset */
		i -= 2;			/* We've handled packet header */
		i = 0x8000000 >> i;	/* Bit in packet header */

		if (priv->compact_header & i) {
			in_compact_insn = 1;
			i = (addr - priv->packet_start) / 2;
			i = 1 << i;
			p = (priv->compact_header & i) ? 1 : 0;

			/* Compact; try and scale up */
			opcode = bfd_getl16(opbuf);
			for (ctable = tic64x_compact_formats; ctable->scale_up;
								ctable++) {
				if ((opcode & ctable->opcode_mask)
						== ctable->opcode) {
					/* We win; joy  */
					ret = ctable->scale_up(opcode,
							priv->compact_header,
							&opcode);
					if (ret) {
						fprintf(stderr, "Error scaling "
							"16 bit instruction: %s"
							, strerror(ret));
						return -1;
					}

					break;
				}
			}

			if (!ctable->scale_up) {
				/* We went through compact opcode table, but
				 * didn't find the opcode to match this one */
				info->fprintf_func(info->stream, "????");
				return 2;
			}

			sz = 2;
			if (p)
				tic64x_set_operand(&opcode, tic64x_operand_p,1);
		}
	}

	/* Find a template that matches and print */
	for (templ = tic64x_opcodes; templ->mnemonic; templ++) {
		if ((opcode & templ->opcode_mask) == templ->opcode) {
			break;
		}
	}

	if (priv->no_pbar_next)
		p = 0;
	else
		p = 1;

	priv->no_pbar_next = 0;

	if (templ->mnemonic == NULL) {
		info->fprintf_func(info->stream, "????");
	} else {
		print_insn(templ, opcode, info, p);
	}

	if (tic64x_get_operand(opcode, tic64x_operand_p, 0) == 0) {
		/* No p bit -> next insn printed has no bar */
		priv->no_pbar_next = 1;
	}

	return sz;
}

void
print_insn(struct tic64x_op_template *templ, uint32_t opcode,
		struct disassemble_info *info, int double_bar)
{
	const char *tchar, *memnum, *xpath;
	int i, j, z, creg;
	char unit, unit_no;
	char finalstr[16], finalstr2[17];

	if (double_bar) {
		info->fprintf_func(info->stream, "||");
	} else {
		info->fprintf_func(info->stream, "  ");
	}

	/* Conditional? */
	z = tic64x_get_operand(opcode, tic64x_operand_z, 0);
	creg = tic64x_get_operand(opcode, tic64x_operand_creg, 0);
	/* creg is zero for unconditional */
	if (!(templ->flags & TIC64X_OP_NOCOND) && creg) {
		info->fprintf_func(info->stream, "[");

		if (z)
			info->fprintf_func(info->stream, "!");
		else
			info->fprintf_func(info->stream, " ");

		switch (creg) {
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
		} else if (!(templ->flags & TIC64X_OP_NOSIDE)) {
			if (tic64x_get_operand(opcode, tic64x_operand_s, 0)) {
				unit_no = '2';
			} else {
				unit_no = '1';
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
		tchar = "T";
		if (templ->flags & TIC64X_OP_NOSIDE) {
			fprintf(stderr, "tic64x print_insn: instruction with "
				"memory access but not dest/side bit?");
			memnum = "?";
		} else {
			if (tic64x_get_operand(opcode, tic64x_operand_s, 0)) {
				memnum = "2";
			} else {
				memnum = "1";
			}
		}
	} else {
		tchar = "";
		memnum = "";
	}

	/* Cross-path? */
	if (((templ->flags & TIC64X_OP_XPATH_SRC1) ||
				(templ->flags & TIC64X_OP_XPATH_SRC2)) &&
				tic64x_get_operand(opcode, tic64x_operand_x, 0))
		xpath = "X";
	else
		xpath = "";

	snprintf(finalstr, 15, ".%C%C%s%s%s", unit, unit_no, tchar,
							memnum, xpath);
	info->fprintf_func(info->stream, "%-7s", finalstr);

	/* Now put some operands out - needs some abstraction though */
	for (i = 0; i < TIC64X_MAX_TXT_OPERANDS; i++) {
		for (j = 0; operand_printers[j].print != NULL; j++) {
			if (operand_printers[j].type == templ->textops[i]) {
				operand_printers[j].print(templ, opcode,
							templ->textops[i],
							finalstr, 15);
				if (i == TIC64X_MAX_TXT_OPERANDS-1) {
					info->fprintf_func(info->stream, "%s",
								finalstr);
				} else if (strlen(finalstr) != 0) {
					snprintf(finalstr2, 16,
								"%s,",
								finalstr);
					info->fprintf_func(info->stream,
								"%-15s",
								finalstr2);
				}
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
		enum tic64x_text_operand type ATTRIBUTE_UNUSED,
		char *buffer, int len)
{

	snprintf(buffer, len, "%C", '\0');
	return;
}

void
print_op_memaccess(struct tic64x_op_template *t, uint32_t opcode,
		enum tic64x_text_operand type ATTRIBUTE_UNUSED,
		char *buffer, int len)
{
	const char *pre, *regchar, *post, *offs;
	int addrmode, basereg, offset, scale, scalenum, y, s;
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
	 * y bit. Confirm later TIX64X_OP_UNITNO is set*/
	s = tic64x_get_operand(opcode, tic64x_operand_s, 0);
	y = tic64x_get_operand(opcode, tic64x_operand_y, 0);

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
	} else {
		post = "";
	}

	/* Check whether or not scaling is forced */
	if (t->flags & TIC64X_OP_MEMACC_SCALE)
		scale = 1;

	/* If we scale, calculate by how much */
	if (scale) {
		scalenum = (t->flags & TIC64X_OP_MEMSZ_MASK) >>
							TIC64X_OP_MEMSZ_SHIFT;
		scalenum = 1 << scalenum;
	} else {
		scalenum = 1;
	}

	/* What kind of offset? */
	if (addrmode & TIC64X_ADDRMODE_REGISTER) {
		if (scale && scalenum != 1) {
			snprintf(offsetstr, 7, "%s%d*%d", regchar, offset,
								scalenum);
		} else {
			snprintf(offsetstr, 7, "%s%d", regchar, offset);
		}
	} else { /* Constant */
		if (offset == 0) {
			snprintf(offsetstr, 7, "%c", 0);
			/* Also, don't print '+' at the front */
			pre = "";
		} else if (scale) {
			snprintf(offsetstr, 7, "%d", offset * scalenum);
		} else {
			snprintf(offsetstr, 7, "%d", offset);
		}
	}

	/* And I think that's it */
	if (*offsetstr == 0) {
		snprintf(buffer, len, "*%s%s%s%s", pre, regchar, regno, post);
	} else {
		snprintf(buffer, len, "*%s%s%s%s(%s)", pre, regchar, regno,
							post, offsetstr);
	}
	return;
}

void
print_op_memrel15(struct tic64x_op_template *t, uint32_t opcode,
		enum tic64x_text_operand type ATTRIBUTE_UNUSED,
		char *buffer, int len)
{
	int regno, offset, scale;

	if (tic64x_get_operand(opcode, tic64x_operand_y, 0))
		regno = 15;
	else
		regno = 14;

	offset = tic64x_get_operand(opcode, tic64x_operand_const15, 0);
	if (t->flags & TIC64X_OP_CONST_SCALE) {
		scale = t->flags & TIC64X_OP_MEMSZ_MASK;
		scale >>= TIC64X_OP_MEMSZ_SHIFT;
		offset <<= scale;
	}

	snprintf(buffer, len, "*+B%d(%X)", regno, offset);
	return;
}

void
print_op_register(struct tic64x_op_template *t, uint32_t opcode,
		enum tic64x_text_operand type, char *buffer, int len)
{
	enum tic64x_operand_type t2;
	int regnum, side, x, y;
	char unitchar;

	if (type == tic64x_optxt_dstreg) {
		t2 = tic64x_operand_dstreg;
	} else if (type == tic64x_optxt_srcreg1) {
		t2 = tic64x_operand_srcreg1;
	} else if (type == tic64x_optxt_srcreg2) {
		t2 = tic64x_operand_srcreg2;
	} else {
		fprintf(stderr, "tic64x print_op_register called with invalid "
				"operand type\n");
		snprintf(buffer, len, "%C", '\0');
		return;
	}

	if ((t->flags & TIC64X_OP_NOSIDE) &&
					!(t->flags & TIC64X_OP_FIXED_UNITNO)) {
		fprintf(stderr, "tic64x print_op_register: \"%s\" has no "
					"side bit?", t->mnemonic);
		snprintf(buffer, len, "%C", '\0');
		return;
	}

	regnum = tic64x_get_operand(opcode, t2, 0);
	side = tic64x_get_operand(opcode, tic64x_operand_s, 0);
	x = tic64x_get_operand(opcode, tic64x_operand_x, 0);
	y = tic64x_get_operand(opcode, tic64x_operand_y, 0);

	if (t->flags & TIC64X_OP_FIXED_UNITNO) {
		if (t->flags & TIC64X_OP_FIXED_UNIT2)
			side = 1;
		else
			side = 0;
	}

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
		} else if (!(t->flags & TIC64X_OP_NOSIDE)) {
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
		if (type == tic64x_optxt_dwsrc || type == tic64x_optxt_dwsrc2) {
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

	snprintf(buffer, len, "%c%d", unitchar, regnum);
	return;
}

void
print_op_dwreg(struct tic64x_op_template *t, uint32_t opcode,
		enum tic64x_text_operand type, char *buffer, int len)
{
	enum tic64x_operand_type t2;
	int regnum, side, y, i;
	char unitchar;

	t2 = tic64x_operand_invalid;
	unitchar = '?';

	if (type == tic64x_optxt_dwsrc) {
		t2 = tic64x_operand_dwsrc;
	} else if (type == tic64x_optxt_dwsrc2) {
		t2 = tic64x_operand_srcreg1;
	} else if (type == tic64x_optxt_dwdst) {
		for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
			if ((t->operands[i] == tic64x_operand_dwdst4 &&
						type == tic64x_optxt_dwdst) ||
			    (t->operands[i] == tic64x_operand_dwdst5 &&
						type == tic64x_optxt_dwdst)) {
				t2 = t->operands[i];
				break;
			}
		}
		if (i == TIC64X_MAX_OPERANDS) {
			fprintf(stderr, "tic64x print_op_dwreg: \"%s\" has no "
				"matching dword reg operand\n", t->mnemonic);
			snprintf(buffer, len, "%C", '\0');
			return;
		}
	}


	if (t->flags & TIC64X_OP_NOSIDE) {
		fprintf(stderr, "tic64x print_op_register: \"%s\" has no "
					"side bit?", t->mnemonic);
		snprintf(buffer, len, "%C", '\0');
		return;
	}

	regnum = tic64x_get_operand(opcode, t2, 0);
	side = tic64x_get_operand(opcode, tic64x_operand_s, 0);
	y = tic64x_get_operand(opcode, tic64x_operand_y, 0);

	if (t2 == tic64x_operand_dwdst4)
		regnum <<= 1;

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
		} else if (!(t->flags & TIC64X_OP_NOSIDE)) {
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

	snprintf(buffer, len, "%c%d:%c%d", unitchar, regnum+1, unitchar,regnum);
	return;
}

void
print_op_constant(struct tic64x_op_template *t, uint32_t opcode,
		enum tic64x_text_operand type, char *buffer, int len)
{
	enum tic64x_operand_type t2;
	int i, val, memsz;

	/* There are multiple constant operand forms... */
	t2 = tic64x_operand_invalid;
	if (type == tic64x_optxt_nops) {
		t2 = tic64x_operand_nops;
		i = 0;
	} else {
		for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
			if (t->operands[i] == tic64x_operand_const5 ||
				t->operands[i] == tic64x_operand_const5p2 ||
				t->operands[i] == tic64x_operand_const21 ||
				t->operands[i] == tic64x_operand_const16 ||
				t->operands[i] == tic64x_operand_const15 ||
				t->operands[i] == tic64x_operand_const12 ||
				t->operands[i] == tic64x_operand_const10 ||
				t->operands[i] == tic64x_operand_const7 ||
				t->operands[i] == tic64x_operand_const4 ||
				t->operands[i] == tic64x_operand_nops) {
					t2 = t->operands[i];
					break;
			}
		}
	}

	if (i == TIC64X_MAX_OPERANDS) {
		fprintf(stderr, "tic64x print_op_constant: \"%s\" has no "
				"matching dword reg operand\n", t->mnemonic);
		snprintf(buffer, len, "%C", '\0');
		return;
	}

	val = tic64x_get_operand(opcode, t2,
				(type == tic64x_optxt_sconstant) ? 1 : 0);

	if (t->flags & TIC64X_OP_CONST_SCALE) {
		/* Opcode wants its constants being scaled by a certain amount*/
		memsz = (t->flags & TIC64X_OP_MEMSZ_MASK)
						>> TIC64X_OP_MEMSZ_SHIFT;

		/* Fail: if we scaled up from a constant insn, we tend to
		 * scale the constant slightly less (by 1). Currently there's
		 * no way of communicating this piece of information. ugh. */
		if (in_compact_insn)
			memsz -= 1;

		val <<= memsz;
	} else if (t->flags & TIC64X_OP_USE_TOP_HWORD) {
		val <<= 16;
	}

	/* Print all operands as hex, limit to 32 bits of FFFF... */
	snprintf(buffer, len, "0x%X", val & 0xFFFFFFFF);
	return;
}
