/* Copyright blah */

#include <limits.h>
#include "as.h"
#include "safe-ctype.h"
#include "opcode/tic64x.h"
#include "obj-coff.h"
#include "struc-symbol.h"

#define UNUSED(x) ((x) = (x))
#define UNITCHAR_2_FLAG(x) ((x) == 'D' ? TIC64X_OP_UNIT_D :		\
				(x) == 'L' ? TIC64X_OP_UNIT_L :		\
				(x) == 'S' ? TIC64X_OP_UNIT_S :		\
				TIC64X_OP_UNIT_M)

struct tic64x_insn {
	struct tic64x_op_template *templ;
	char unit;				/* Unit character ie 'L' */
	int unit_num;				/* Unit number, 1 or 2 */
	int mem_unit_num;			/* Memory data path, 1 or 2 */

	/* Template holds everything needed to build the instruction, but
	 * we need some data to actually build with. Each entry in operands
	 * array corresponds to the operand in the op template */
	struct {
		/* Once operands are parse, they should either fill out the
		 * expression for later resolvement, or set the value and
		 * set "resolved" to 1 */
		expressionS expr;
		uint32_t value;
		int resolved;
	} operand_values[TIC64X_MAX_OPERANDS];
};

const char comment_chars[] = ";";
const char line_comment_chars[] = ";*#";
const char line_separator_chars[] = "";

const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "fF";

/* No options right now */
struct option md_longopts[] =
{
	{ NULL,		no_argument,	NULL,	0}
};

size_t md_longopts_size = sizeof(md_longopts);

const char *md_shortopts = "";

static struct hash_control *tic64x_ops;
static struct hash_control *tic64x_reg_names;
static struct hash_control *tic64x_subsyms;
int tic64x_line_had_parallel_prefix;

static char *tic64x_parse_expr(char *s, expressionS *exp);
static void tic64x_asg(int x);
static void tic64x_sect(int x);
static void tic64x_fail(int x);
static struct tic64x_register *tic64x_sym_to_reg(char *name);
static char *tic64x_parse_operand(char *line, struct tic64x_insn *i,int op_num);
static void tic64x_opreader_none(char *line, struct tic64x_insn *insn);
static void tic64x_opreader_memaccess(char *line, struct tic64x_insn *insn);
static void tic64x_opreader_register(char *line, struct tic64x_insn *insn);
static void tic64x_opreader_constant(char *line, struct tic64x_insn *insn);

/* A few things we might want to handle - more complete table in tic54x, also
 * see spru186 for a full reference */
const pseudo_typeS md_pseudo_table[] =
{
	{"align",	tic64x_fail,		0},
	{"ascii",	tic64x_fail,		0},
	{"asciz",	tic64x_fail,		0},
	{"asg", 	tic64x_asg,		0},
	{"bss",		tic64x_fail,		0},
	{"byte",	tic64x_fail,		0},
	{"copy",	tic64x_fail,		0},
	{"data",	tic64x_fail,		0},
	{"def",		tic64x_fail,		0},
/*	{"global",	tic64x_fail,		0}, */
	{"include",	tic64x_fail,		0},
	{"mlib",	tic64x_fail,		0},
	{"ref",		tic64x_fail,		0},
	{"sect",	tic64x_sect,		0},
	{"set",		tic64x_fail,		0},
	{"string",	tic64x_fail,		0},
	{"text",	tic64x_fail,		0},
	{"usect",	tic64x_fail,		0},
	{"word",	tic64x_fail,		0},
	{NULL, 		NULL,			0}
};

/* Parser routines to read a particularly kind of operand */
struct {
	enum tic64x_text_operand type;
	void (*reader) (char *line, struct tic64x_insn *insn);
} tic64x_operand_readers[] = {
{	tic64x_optxt_none,	tic64x_opreader_none},
{	tic64x_optxt_memaccess,	tic64x_opreader_memaccess},
{	tic64x_optxt_register,	tic64x_opreader_register},
{	tic64x_optxt_constant,	tic64x_opreader_constant},
{	tic64x_optxt_none,	NULL}
};

int
md_parse_option(int c, char *arg)
{

	/* We don't have any command line options right now */
	UNUSED(c);
	UNUSED(arg);
	return 1;
}

void
md_show_usage(FILE *stream ATTRIBUTE_UNUSED)
{

	return;
}

void
md_begin()
{
	struct tic64x_op_template *op;
	struct tic64x_register *reg;

	tic64x_ops = hash_new();
	tic64x_reg_names = hash_new();
	tic64x_subsyms = hash_new();

	for (op = tic64x_opcodes; op->mnemonic; op++)
		if (hash_insert(tic64x_ops, op->mnemonic, (void *)op))
			as_fatal("md_begin: couldn't enter %s in hash table\n",
				op->mnemonic);

	for (reg = tic64x_regs; reg->name; reg++)
		if (hash_insert(tic64x_reg_names, reg->name, (void *)reg))
			as_fatal("md_begin: couldn't enter %s in hash table\n",
				reg->name);

	return;
}

char *
tic64x_parse_expr(char *s, expressionS *exp)
{
	char *tmp, *end;

	/* This is horrific. */
	tmp = input_line_pointer;
	input_line_pointer = s;
	expression(exp);
	end = input_line_pointer;
	input_line_pointer = tmp;
	return end;
}

static void
tic64x_fail(int x ATTRIBUTE_UNUSED)
{

	as_fatal("Encountered supported but unimplemented pseudo-op");
	return;
}

static void
tic64x_asg(int x ATTRIBUTE_UNUSED)
{
	const char *err;
	char *str, *sym;
	int dummy;
	char c;

	if (*input_line_pointer == '"') {
		str = demand_copy_C_string(&dummy);
		c = *input_line_pointer++;
	} else {
		str = input_line_pointer;
		while (1) {
			/* XXX - following tic54x, but can this just be
			 * replaced with a call to get_symbol_end? */
			c = *input_line_pointer;
			if (c == ',' || is_end_of_line[(int)c])
				break;
			input_line_pointer++;
		}
		*input_line_pointer++ = 0;
	}

	if (c != ',') {
		as_bad("No comma while parsing .asg");
		ignore_rest_of_line();
		return;
	}

	sym = input_line_pointer;
	c = get_symbol_end();
	if (!ISALPHA(*sym)) {
		as_bad("Bad starting character in asg assignment");
		ignore_rest_of_line();
		return;
	}

	str = strdup(str);
	sym = strdup(sym);
	if (!str || !sym)
		as_fatal("OOM @ %s %d", __FILE__, __LINE__);

	err = hash_jam(tic64x_subsyms, sym, str);

	return;
}

static void
tic64x_sect(int x ATTRIBUTE_UNUSED)
{

	char *name, *sect;
	int len;
	char c;

	if (*input_line_pointer == '"') {
		name = demand_copy_C_string(&len);
		demand_empty_rest_of_line();
		name = strdup(name);
	} else {
		name = input_line_pointer;
		c = get_symbol_end();
		name = strdup(name);
		*input_line_pointer = c;
		demand_empty_rest_of_line();
	}

	sect = malloc(strlen(name) + 5);
	sprintf(sect, "%s,\"w\"\n", name);
	free(name);

	/* Knobble subsections. obj_coff doesn't support them, its pretty
	 * pointless for us to too. */
	for (name = sect; *name; name++)
		if (*name == ':')
			*name = '_';

	input_scrub_insert_line(sect);
	obj_coff_section(0);
	/* XXX - when can we free that buffer? */
}

char *
md_atof(int type, char *literal, int *size)
{

	return ieee_md_atof(type, literal, size, TRUE);
}

symbolS *
md_undefined_symbol(char *name)
{

	/*
	 * Documentation says that this'll be called when gas can't find a
	 * symbol, to allow for dynamically changing names or predefined
	 * symbols. I'm not certain how to go about this; we don't know enough
	 * about TIs assembler to know what varying symbols it uses, tic54x
	 * says there are "subtle differences" in the way symbols are managed.
	 */
	/* So for now do nothing */
	UNUSED(name);
	return NULL;
}

int
md_estimate_size_before_relax(fragS *frag, segT seg)
{

	/* tic54x doesn't implement this either; we'll see if its needed */
	UNUSED(frag);
	UNUSED(seg);
	return 0;
}

long
md_pcrel_from (fixS *fixP)
{

	/* Another thing not implemented by tic54x, we probably don't need to
	 * either */
	UNUSED(fixP);
	return 0;
}

valueT
md_section_align(segT segment, valueT section_size)
{

	UNUSED(segment);
	/* No alignment rules for sections, AFAIK */
	return section_size;
}

void
md_convert_frag(bfd *b, segT seg, fragS *frag)
{

	UNUSED(b);
	UNUSED(seg);
	UNUSED(frag);
	fprintf(stderr, "Unimplemented md_convert_frag in tic64x called\n");
	exit(1);
}

arelent *
tc_gen_reloc(asection *section, fixS *fixP)
{

	UNUSED(section);
	UNUSED(fixP);
	fprintf(stderr, "Unimplemented tc_gen_reloc in tic64x called\n");
	exit(1);
}

void
md_apply_fix(fixS *fixP, valueT *valP, segT seg)
{

	UNUSED(valP);
	UNUSED(seg);
	switch(fixP->fx_r_type) {
	default:
		as_fatal("Bad relocation type %X\n", fixP->fx_r_type);
		return;
	}
	return;
}

struct tic64x_register *
tic64x_sym_to_reg(char *regname)
{
	char *subsym;
	struct tic64x_register *reg;

	reg = hash_find(tic64x_reg_names, regname);
	if (!reg) {
		subsym = hash_find(tic64x_subsyms, regname);
		if (!subsym) {
			return NULL;
		}

		reg = hash_find(tic64x_reg_names, subsym);
		if (!reg) {
			return NULL;
		}
	}

	return reg;
}

/* Some kind of convention as to what can be md_blah and what needs to be
 * #defined to md_blah would be nice... */
void
tic64x_start_line_hook(void)
{
	char *line;

	/* Only thing we want to know/do is stop gas tripping over "||"
	 * bars indicating parallel execution */
	line = input_line_pointer;
	while (ISSPACE(*line) && !is_end_of_line[(int)*line])
		line++;

	if (*line == '|' && *(line+1) == '|') {
		*line++ = ' ';
		*line++ = ' ';
		tic64x_line_had_parallel_prefix = 1;
	} else {
		tic64x_line_had_parallel_prefix = 0;
	}

	return;
}

void
tic64x_opreader_none(char *line, struct tic64x_insn *insn)
{

	UNUSED(line);
	UNUSED(insn);
	as_bad("Unsupported operand type");
	return;
}

void
tic64x_opreader_memaccess(char *line, struct tic64x_insn *insn)
{
	expressionS expr;
	char *regname, *offs;
	struct tic64x_register *reg, *offsetreg;
	int off_reg, pos_neg, pre_post, nomod_modify, has_offset, i, tmp, sc;
	char c;

	off_reg = -1;
	pos_neg = -1;
	pre_post = -1;
	nomod_modify = -1;
	has_offset = -1;

	/* We expect firstly to start wih a '*' */
	if (*line++ != '*') {
		as_bad("expected '*' before memory operand");
		return;
	}

	/* See page 79 of spru732h for table of address modes */
	if (*line == '+') {
		if (*(line+1) == '+') {
			/* Preincrement */
			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			pos_neg = TIC64X_ADDRMODE_POS;
			pre_post = TIC64X_ADDRMODE_PRE;
			line += 2;
		} else {
			nomod_modify = TIC64X_ADDRMODE_NOMODIFY;
			pos_neg = TIC64X_ADDRMODE_POS;
			line++;
		}
	} else if (*line == '-') {
		if (*(line+1) == '-') {
			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			pos_neg = TIC64X_ADDRMODE_NEG;
			pre_post = TIC64X_ADDRMODE_PRE;
			line += 2;
		} else {
			nomod_modify = TIC64X_ADDRMODE_NOMODIFY;
			pos_neg = TIC64X_ADDRMODE_NEG;
			line++;
		}
	}

	/* We should now have an alpha-num register name, possibly .asg'd */
	regname = line;
	while (ISALPHA(*line) || ISDIGIT(*line) || *line == '_')
		line++;

	if (regname == line) { /* Invalid register name */
		as_bad("Expected register name in memory operand of \"%s\"",
						insn->templ->mnemonic);
		return;
	}

	c = *line;
	*line = 0;
	reg = tic64x_sym_to_reg(regname);
	*line = c;
	if (!reg) {
		as_bad("\"%s\" is not a register", regname);
		return;
	}

	/* Memory addr registers _have_ to come from the side of the processor
	 * we're executing on */
	if (((reg->num & TIC64X_REG_UNIT2) && insn->unit_num != 2) ||
	    (!(reg->num & TIC64X_REG_UNIT2) && insn->unit_num != 1)) {
		as_bad("Base address register must be on same side of processor"
			" as instruction");
		return;
	}

	/* We should now have a register to work with - it can be suffixed
	 * with a postdecrement/increment, offset constant or register */
	if (*line == '-') {
		if (*(line+1) == '-') {
			if (pos_neg != -1 || nomod_modify != -1) {
				as_bad("Can't specify both pre and post "
					"operators on address register");
				return;
			}

			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			pos_neg = TIC64X_ADDRMODE_NEG;
			pre_post = TIC64X_ADDRMODE_POST;
		} else {
			as_bad("Bad operator following address register");
			return;
		}
	} else if (*line == '+') {
		if (*(line+1) == '+') {
			if (pos_neg != -1 || nomod_modify != -1) {
				as_bad("Can't specify both pre and post "
					"operators on address register");
				return;
			}

			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			pos_neg = TIC64X_ADDRMODE_POS;
			pre_post = TIC64X_ADDRMODE_POST;
		} else {
			as_bad("Bad operator following address register");
			return;
		}
	}

	/* Look for offset register of constant */
	offsetreg = NULL;
	if (*line == '[' || *line == '(') {
		has_offset = 1;
		c = (*line++ == '[') ? ']' : ')';
		offs = line;

		while (*line != c && !is_end_of_line[(int)*line])
			line++;

		if (is_end_of_line[(int)*line]) {
			as_bad("Unexpected end of file while reading address "
				"register offset");
			return;
		}

		/* We have an offset - read it into an expr, then
		 * leave it for the moment */
		c = *line;
		*line = 0;
		tic64x_parse_expr(offs, &expr);
		*line++ = c;

		/* Need to know early whether this is a register or not - if it
		 * is, should just be a single symbol that we can translate. */
		if (expr.X_op == O_symbol) {
			offsetreg = tic64x_sym_to_reg(offs);
			if (offsetreg) {
				/* woot, it's a register. Just check: */
				if (expr.X_add_number != 0) {
					as_bad("Cannot add/subtract from "
						"register in offset field");
					return;
				}

				off_reg = TIC64X_ADDRMODE_REGISTER;
				/* Memory addr registers _have_ to come from the
				 * side of the processor we're executing on */
				if (((reg->num & TIC64X_REG_UNIT2) &&
						insn->unit_num != 2) ||
				    (!(reg->num & TIC64X_REG_UNIT2) &&
						insn->unit_num != 1)) {
					as_bad("Base address register must be "
						"on same side of processor "
						"as instruction");
					return;
				}
			}
		}
	} else {
		/* No offset, so set offs to constant zero */
		has_offset = 0;
		off_reg = TIC64X_ADDRMODE_OFFSET;
	}

	/* Offset / reg should be the last thing we (might) read - ensure that
	 * we're at the end of the string we were passed */
	if (*line != 0) {
		as_bad("Trailing rubbish at end of address operand");
		return;
	}

	/* There are a million and one ways this operand could have been
	 * constructed - try and make sense of all this */
	if (has_offset) {
		if (nomod_modify == -1 || pos_neg == -1) {
			as_bad("Offset present, but no + or - address mode "
				"specified when reading address register");
			return;
		}

		/* Can't have + and - /after/ the base register */
		if (nomod_modify == TIC64X_ADDRMODE_NOMODIFY &&
				pre_post == TIC64X_ADDRMODE_POST) {
			as_bad("Cannot have + and - modes after base register");
			return;
		}

		/* Otherwise, just make sure the mode we have is consistent */
		/* Which I can't quite work my mind around right now */
	} else if (nomod_modify == TIC64X_ADDRMODE_NOMODIFY) {
		/* Had just + or -, but no offset */
		as_bad("Can't specify + or - with no offset for base address "
								"register");
		return;
	}
	/* Someone with more sleep needs to think up more checks */

	/* So - we have a base register, addressing mode, offset and scale bit,
	 * which we need to fill out in insn. Simple ones first */
	for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
		if (insn->templ->operands[i].type == tic64x_operand_basereg) {
			insn->operand_values[i].value = reg->num & 0x1F;
			insn->operand_values[i].resolved = 1;
			break;
		}
	}

	if (i == TIC64X_MAX_OPERANDS)
		as_fatal("tic64x_opreader_memaccess: instruction \"%s\" has "
			"tic64x_optxt_memaccess operand, but no corresponding "
			"tic64x_operand_basereg operand field",
					insn->templ->mnemonic);

	/* Addressing mode - ditch any fields that haven't been set or are zero.
	 * We rely on earlier checks (XXX not written yet...) to ensure it's
	 * a valid mode that the user expects */
	tmp = 0;
	if (off_reg > 0)
		tmp |= off_reg;
	if (pos_neg > 0)
		tmp |= pos_neg;
	if (pre_post > 0)
		tmp |= pre_post;
	if (nomod_modify > 0)
		tmp |= nomod_modify;

	for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
		if (insn->templ->operands[i].type == tic64x_operand_addrmode) {
			insn->operand_values[i].value = tmp;
			insn->operand_values[i].resolved = 1;
			break;
		}
	}

	if (i == TIC64X_MAX_OPERANDS)
		as_fatal("tic64x_opreader_memaccess: instruction \"%s\" has "
			"tic64x_optxt_memaccess operand, but no corresponding "
			"tic64x_operand_addrmode operand field",
					insn->templ->mnemonic);

	/* Tricky part - offset might not be resolved (with 5 bits, not certain
	 * _why_ that would be, but never mind) so we'll check if it's a
	 * register or constant. If the former, write it. If the latter, check
	 * if it's a numeric constant or whether it involves symbols. If symbols
	 * it'll have to be left unresolved.
	 * Also for constants, see whether we need to scale them or not. Turn
	 * scaling off for register offsets - I don't think there's assembly
	 * syntax for scaled registers */

	if (!has_offset) {
		/* _really_ simple, no offset, no scale */
		tmp = 0;
		sc = 0;
	} else if (off_reg == TIC64X_ADDRMODE_REGISTER) {
		/* All's fine and well */
		tmp = offsetreg->num & 0x1F;
		sc = 0;
	} else if (has_offset && expr.X_op == O_constant) {
		tmp = expr.X_add_number;
		if (tmp < 0) {
			as_bad("tic64x-pedantic-mode: can't add/subtract a "
				"negative constant from base register, use "
				"correct addressing mode instead");
			return;
		}

		if (tmp > 31) {
			/* All is not lost - see if we can't scale it. The
			 * amount by which it's scaled is the size of memory
			 * access this instruction performs - so 8b for dword
			 * memory access, 2b for half byte access. The power
			 * of this shift is stored in insn template flags */
			i = insn->templ->flags & TIC64X_OP_MEMSZ_MASK;
			i >>= TIC64X_OP_MEMSZ_SHIFT;

			/* Shift 1 by power to make multiplier */
			i = 1 << i;

			/* Is constant offset aligned to this boundry? */
			if (tmp & (i-1)) {
				as_bad("Constant offset to base address "
					"register too large, cannot be scaled "
					"as %d is not on a %d byte boundry",
									tmp, i);
				return;
			}

			/* Ok, we can scale it, but even then does it fit? */
			tmp /= i;
			if (tmp > 31) {
				as_bad("Constant offset too large");
				return;
			}

			/* Success; enable scaling */
			sc = 1;
		} else {
			/* When < 31 in the first place, don't scale */
			sc = 0;
		}
	} else {
		/* offset, not reg, not constant, so it has a symbol.
		 * resolve that later */
		goto skip_offset; /* Mwuuhahahaaaha */
	}

	/* Write offset/scale values */
	for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
		if (insn->templ->operands[i].type == tic64x_operand_rcoffset) {
			insn->operand_values[i].value = tmp;
			insn->operand_values[i].resolved = 1;
			break;
		}
	}

	if (i == TIC64X_MAX_OPERANDS)
		as_fatal("tic64x_opreader_memaccess: instruction \"%s\" has "
			"tic64x_optxt_memaccess operand, but no corresponding "
			"tic64x_operand_rcoffset operand field",
					insn->templ->mnemonic);


	for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
		if (insn->templ->operands[i].type == tic64x_operand_scale) {
			insn->operand_values[i].value = sc;
			insn->operand_values[i].resolved = 1;
			break;
		}
	}

	if (i == TIC64X_MAX_OPERANDS)
		as_fatal("tic64x_opreader_memaccess: instruction \"%s\" has "
			"tic64x_optxt_memaccess operand, but no corresponding "
			"tic64x_operand_scale operand field",
					insn->templ->mnemonic);

	skip_offset:
	return;
}

void
tic64x_opreader_register(char *line, struct tic64x_insn *insn)
{

	UNUSED(line);
	UNUSED(insn);
	as_bad("Unsupported operand type");
	return;
}

void
tic64x_opreader_constant(char *line, struct tic64x_insn *insn)
{

	UNUSED(line);
	UNUSED(insn);
	as_bad("Unsupported operand type");
	return;
}

char *
tic64x_parse_operand(char *line, struct tic64x_insn *insn, int op_num)
{
	enum tic64x_text_operand optype;
	char *operand;
	int i;
	char end;

	/* Read up til end of operand */
	operand = line;
	while (*line != ',' && !is_end_of_line[(int)*line])
		line++;

	/* Mark end of operand */
	end = *line;
	*line = 0;

	/* We have some text, lookup what kind of operand we expect, and call
	 * its parser / handler / whatever */
	optype = insn->templ->textops[op_num];
	for (i = 0; tic64x_operand_readers[i].reader != NULL; i++) {
		if (tic64x_operand_readers[i].type == optype)
			break;
	}

	if (tic64x_operand_readers[i].reader) {
		tic64x_operand_readers[i].reader(operand, insn);
	} else {
		as_bad("\"%s\" has unrecognised operand %d",
				insn->templ->mnemonic, op_num);
	}

	return line;
}

void
md_assemble(char *line)
{
	struct tic64x_insn *insn;
	char *mnemonic;
	int i;

	insn = malloc(sizeof(*insn));
	memset(insn, 0, sizeof(*insn));

	mnemonic = line;
	while (!ISSPACE(*line) && !is_end_of_line[(int)*line])
		line++;
	*line++ = 0;

	/* Is this an instruction we've heard of? */
	insn->templ = hash_find(tic64x_ops, mnemonic);
	if (!insn->templ) {
		as_bad("Unrecognised mnemonic %s", mnemonic);
		free(insn);
		return;
	}

	/* Expect ".xn" where x is {D,L,S,M}, and n is {1,2}. Can be followed
	 * by 'T' specifier saying which memory data path is being used */
	if (*line++ != '.') {
		as_bad("Expected execution unit specifier after \"%s\"",
							insn->templ->mnemonic);
		free(insn);
		return;
	}

	insn->unit = *line++;
	if (insn->unit != 'D' && insn->unit != 'L' && insn->unit != 'S' &&
							insn->unit != 'M') {
		as_bad("Unrecognised execution unit %C after \"%s\"",
					insn->unit, insn->templ->mnemonic);
		free(insn);
		return;
	}

	if (UNITCHAR_2_FLAG(insn->unit) !=
			(insn->templ->flags & TIC64X_OP_UNIT_MASK)) {
		as_bad("Instruction \"%s\" can't go in unit %C. XXX - currently"
			" have no way of representing instructions that go "
			"in multiple units", insn->templ->mnemonic, insn->unit);
		return;
	}

	/* I will scream if someone says "what if it isn't ascii" */
	insn->unit_num = *line++ - 0x30;
	if (insn->unit_num != 1 && insn->unit_num != 2) {
		as_bad("Bad execution unit number %d after \"%s\"",
					insn->unit_num, insn->templ->mnemonic);
		free(insn);
		return;
	}

	if (insn->templ->flags & TIC64X_OP_MEMACCESS) {
		/* We should find either T1 or T2 at end of unit specifier,
		 * indicating which data path the loaded/stored data will
		 * travel through (only address needs to be in same unit) */
		if (*line++ != 'T') {
			as_bad("Expected memory datapath T1/T2 in unit "
				"specifier for \"%s\"", insn->templ->mnemonic);
			free(insn);
			return;
		}

		insn->mem_unit_num = *line++ - 0x30;
		if (insn->mem_unit_num != 1 && insn->mem_unit_num != 2) {
			as_bad("%d not a valid unit number for memory data path"
					" in \"%s\"", insn->mem_unit_num,
							insn->templ->mnemonic);
			free(insn);
			return;
		}
	}

	i = 0;
	while (!is_end_of_line[(int)*line])
		line = tic64x_parse_operand(line, insn, i++);

	printf("Got mnemonic %s unit %C num %d memunit %d\n",
		insn->templ->mnemonic, insn->unit, insn->unit_num,
					insn->mem_unit_num);
	free(insn);
	return;
}
