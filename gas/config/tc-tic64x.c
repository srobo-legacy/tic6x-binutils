/* Copyright blah */

#include <limits.h>
#include "as.h"
#include "safe-ctype.h"
#include "opcode/tic64x.h"
#include "obj-coff.h"
#include "struc-symbol.h"

#define UNUSED(x) ((x) = (x))

#define TXTOPERAND_CAN_XPATH(insn, type) 				\
		((((insn)->templ->flags & TIC64X_OP_XPATH_SRC2) &&	\
					(type) == tic64x_optxt_srcreg2) ||\
		(!((insn)->templ->flags & TIC64X_OP_XPATH_SRC2) &&	\
					(type) == tic64x_optxt_srcreg1))

#define abort_no_operand(insn, type)					\
		as_fatal("Instruction \"%s\" does not have expected "	\
			"operand type " type " (internal error)",	\
					insn->templ->mnemonic);

#define abort_setop_fail(insn, type, err) 				\
		as_fatal("Couldn't set operand " type " for "		\
			"instruction %s: %s", insn->templ->mnemonic,	\
			err);

struct tic64x_insn {
	struct tic64x_op_template *templ;
	uint32_t opcode;			/* Code, filled with operands
						 * as we parse them */
	char unit;				/* Unit character ie 'L' */
	int8_t unit_num;			/* Unit number, 1 or 2 */
	int8_t mem_unit_num;			/* Memory data path, 1 or 2 */
	int8_t uses_xpath;			/* Unit specifier had X suffix*/
	int8_t parallel;			/* || prefix? */
	int8_t cond_nz;				/* Condition flag, zero or nz?*/
	int16_t cond_reg;			/* Register for comparison */

	/* Template holds everything needed to build the instruction, but
	 * we need some data to actually build with. Each entry in operands
	 * array corresponds to the operand in the op template */
	struct {
		/* Once operands are parse, they should either fill out the
		 * expression for later resolvement, or set the value and
		 * set "resolved" to 1 */
		expressionS expr;
		int resolved;
	} operand_values[TIC64X_MAX_OPERANDS];
};

typedef int (optester) (char *line, struct tic64x_insn *insn,
                                        enum tic64x_text_operand optype);
typedef void (opreader) (char *line, struct tic64x_insn *insn,
                                        enum tic64x_text_operand optype);


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
int tic64x_line_had_cond;
int tic64x_line_had_nz_cond;
struct tic64x_register *tic64x_line_had_cond_reg;

static char *tic64x_parse_expr(char *s, expressionS *exp);
static void tic64x_asg(int x);
static void tic64x_sect(int x);
static void tic64x_fail(int x);
static struct tic64x_register *tic64x_sym_to_reg(char *name);
static int find_operand_index(struct tic64x_op_template *templ,
			enum tic64x_operand_type type);
static opreader tic64x_opreader_none;
static optester tic64x_optest_none;
static opreader tic64x_opreader_memaccess;
static optester tic64x_optest_memaccess;
static opreader tic64x_opreader_register;
static optester tic64x_optest_register;
static opreader tic64x_opreader_double_register;
static optester tic64x_optest_double_register;
static opreader tic64x_opreader_constant;
static optester tic64x_optest_constant;

static int tic64x_compare_operands(struct tic64x_insn *insn,
					struct tic64x_op_template *templ,
							char **operands);
static void tic64x_output_insn(struct tic64x_insn *insn);

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
	opreader *reader;
	optester *test;
} tic64x_operand_readers[] = {
{tic64x_optxt_none,	tic64x_opreader_none,	tic64x_optest_none},
{tic64x_optxt_memaccess,tic64x_opreader_memaccess,tic64x_optest_memaccess},
{tic64x_optxt_dstreg,	tic64x_opreader_register,tic64x_optest_register},
{tic64x_optxt_srcreg1,	tic64x_opreader_register,tic64x_optest_register},
{tic64x_optxt_srcreg2,	tic64x_opreader_register,tic64x_optest_register},
{tic64x_optxt_dwdst,	tic64x_opreader_double_register,tic64x_optest_double_register},
{tic64x_optxt_dwsrc,	tic64x_opreader_double_register,tic64x_optest_double_register},
{tic64x_optxt_uconstant,tic64x_opreader_constant,tic64x_optest_constant},
{tic64x_optxt_sconstant,tic64x_opreader_constant,tic64x_optest_constant},
{tic64x_optxt_nops,	tic64x_opreader_constant,tic64x_optest_constant},
{tic64x_optxt_none,	NULL, NULL}
};

/* So, with gas at the moment, we can't detect the end of an instruction
 * packet until there's been a line without a || at the start. And we can't
 * make any reasonable decisions about p bits until we have a start and end.
 * So - read insns in with md_assemble, store them in the following table,
 * and emit them when we know we're done with a packet. Requires using
 * md_after_pass_hook probably. */
static int read_insns_index;
static struct tic64x_insn *read_insns[8];
static void tic64x_output_insn_packet(void);

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

	read_insns_index = 0;
	memset(read_insns, 0, sizeof(read_insns));

	tic64x_ops = hash_new();
	tic64x_reg_names = hash_new();
	tic64x_subsyms = hash_new();

	for (op = tic64x_opcodes; op->mnemonic; op++) {
		if (hash_insert(tic64x_ops, op->mnemonic, (void *)op))
			as_fatal("md_begin: couldn't enter %s in hash table\n",
				op->mnemonic);

		/* In the unpleasent circumstance when there are more than
		 * one instruction with the same mnemonic, skip through until
		 * we find a different one */
		if (op->flags & TIC64X_OP_MULTI_MNEMONIC)
			while ((op+1)->mnemonic &&
					!strcmp(op->mnemonic, (op+1)->mnemonic))
				op++;
	}

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
	if (err)
		as_bad("hash_jam failed handling .asg: \"%s\"", err);

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
	sprintf(sect, "%s,\"x\"\n", name);
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
		} else {
			reg = hash_find(tic64x_reg_names, subsym);
		}
	}

	return reg;
}

int
find_operand_index(struct tic64x_op_template *templ,
			enum tic64x_operand_type type)
{
	int i;

	for (i = 0; i < TIC64X_MAX_OPERANDS; i++)
		if (templ->operands[i] == type)
			return i;

	return -1;
}

/* Some kind of convention as to what can be md_blah and what needs to be
 * #defined to md_blah would be nice... */
void
tic64x_start_line_hook(void)
{
	char *line;
	char *reg;

	/* Do we have a "||" prefix? */
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

	/* Is there a conditional prefix? */
	while (ISSPACE(*line) && !is_end_of_line[(int)*line])
		line++;

	if (*line == '[') {
		tic64x_line_had_cond = 1;
		*line++ = ' ';
		tic64x_line_had_nz_cond = (*line == '!') ? 1 : 0;
		*line++ = ' ';

		reg = line;
		while (*line != ']' && !is_end_of_line[(int)*line])
			line++;

		if (is_end_of_line[(int)*line]) {
			as_bad("Unexpected end of line reading conditional "
				"register name");
			return;
		}

		*line = 0;
		tic64x_line_had_cond_reg = tic64x_sym_to_reg(reg);
		if (tic64x_line_had_cond_reg == NULL) {
			as_bad("Expected register in conditional prefix");
			return;
		}

		/* And to make sure nothing else stumbles over this, overwrite
		 * that register name with spaces */
		for ( ; reg != line; reg++)
			*reg = ' ';

		*line = ' ';
	} else {
		tic64x_line_had_cond = 0;
		tic64x_line_had_cond_reg = NULL;
	}

	return;
}

int
tic64x_optest_none(char *line ATTRIBUTE_UNUSED,
		struct tic64x_insn *insn ATTRIBUTE_UNUSED,
		enum tic64x_text_operand op ATTRIBUTE_UNUSED)

{

	return 0; /* Never matches */
}

int
tic64x_optest_register(char *line, struct tic64x_insn *insn,
				enum tic64x_text_operand type)
{
	struct tic64x_register *reg;

	reg = tic64x_sym_to_reg(line);
	if (!reg)
		return 0;

	if (((reg->num & TIC64X_REG_UNIT2) && insn->unit_num == 1) ||
	    (!(reg->num & TIC64X_REG_UNIT2) && insn->unit_num == 2)) {
		/* Register doesn't match the side of processor we're using.
		 * There are a few circumstances where this permittable though*/

		/* Is xpath on, and does it match our register */
		if ((insn->templ->flags & TIC64X_OP_USE_XPATH) &&
					TXTOPERAND_CAN_XPATH(insn, type)) {
			/* Xpath on, matches us, we're valid. */
			return 1;
		/* We're also excused if we're dest/src of a load/store */
		} else if (insn->mem_unit_num != -1 &&
		(((reg->num & TIC64X_REG_UNIT2) && insn->mem_unit_num == 2) ||
		(!(reg->num & TIC64X_REG_UNIT2) && insn->mem_unit_num == 1))) {
			return 1;
		} else {
			/* Wrong side, no excuse, doesn't match */
			return 0;
		}
	}

	/* On right side, it's good */
	return 1;
}

int
tic64x_optest_double_register(char *line,
				struct tic64x_insn *insn ATTRIBUTE_UNUSED,
				enum tic64x_text_operand op ATTRIBUTE_UNUSED)
{
	char *reg1, *reg2;
	int tmp;

	if (!strchr(line, ':'))
		return 0;

	reg1 = strdup(line);
	reg2 = strchr(reg1, ':');
	*reg2++ = 0;

	tmp = (tic64x_sym_to_reg(reg1) && tic64x_sym_to_reg(reg2));
	free(reg1);
	return tmp;
}

int
tic64x_optest_memaccess(char *line, struct tic64x_insn *insn ATTRIBUTE_UNUSED,
				enum tic64x_text_operand op ATTRIBUTE_UNUSED)
{

	/* Difficult to validate - instead check if the first char is '*',
	 * nothing else has any business doing so */
	return (*line == '*');
}

int
tic64x_optest_constant(char *line, struct tic64x_insn *insn ATTRIBUTE_UNUSED,
				enum tic64x_text_operand op ATTRIBUTE_UNUSED)
{
	expressionS expr;

	/* Constants can either be an actual constant, or a symbol that'll
	 * eventually become an offset / whatever. However because we aren't
	 * telling gas about registers, first check we're not one of those */
	if (tic64x_sym_to_reg(line))
		return 0;

	tic64x_parse_expr(line, &expr);
	return (expr.X_op == O_constant || expr.X_op == O_symbol);
}

void
tic64x_opreader_none(char *line, struct tic64x_insn *insn,
				enum tic64x_text_operand type)
{

	UNUSED(line);
	UNUSED(insn);
	UNUSED(type);
	as_bad("Excess operand");
	return;
}

void
tic64x_opreader_memaccess(char *line, struct tic64x_insn *insn,
			enum tic64x_text_operand type ATTRIBUTE_UNUSED)
{
	expressionS expr;
	const char *err;
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
			line += 2;
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
			line += 2;
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

	/* For completeness, we could/should replace the (possibly) trailing
	 * bracket here */
	if (c)
		*line++ = c;

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

	/* So - we have a base register, addressing mode, offset and maybe scale
	 * bit, which we need to fill out in insn. Simple ones first */
	i = find_operand_index(insn->templ, tic64x_operand_basereg);
	if (i < 0)
		abort_no_operand(insn, "tic64x_operand_basereg");

	err = tic64x_set_operand(&insn->opcode, tic64x_operand_basereg,
							reg->num & 0x1F);
	if (err)
		abort_setop_fail(insn, "tic64x_operand_basereg", err);

	insn->operand_values[i].resolved = 1;

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

	i = find_operand_index(insn->templ, tic64x_operand_addrmode);
	if (i < 0)
		abort_no_operand(insn, "tic64x_operand_addrmode");

	err = tic64x_set_operand(&insn->opcode, tic64x_operand_addrmode, tmp);
	if (err)
		abort_setop_fail(insn, "tic64x_operand_addrmode", err);
	insn->operand_values[i].resolved = 1;

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

		if (tmp > 31 || insn->templ->flags & TIC64X_OP_MEMACC_SCALE) {
			/* If the instruction always scales the offset, or if
			 * the offset is too large to fit, scale it. The
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
		i = find_operand_index(insn->templ, tic64x_operand_rcoffset);
		if (i < 0)
			abort_no_operand(insn, "tic64x_operand_rcoffset");

		memcpy(&insn->operand_values[i].expr, &expr, sizeof(expr));

		/* Set scale bit to resolved - I don't forsee a situation where
		 * someone's going to both have an unresolved offset, _and_
		 * have it extremely large. XXX is appropriate I guess */
		/* Don't set if instruction _always_ scales */
		if (!(insn->templ->flags & TIC64X_OP_MEMACC_SCALE)) {
			i = find_operand_index(insn->templ,
					tic64x_operand_scale);
			if (i < 0)
				abort_no_operand(insn, "tic64x_operand_scale");

			err = tic64x_set_operand(&insn->opcode,
						tic64x_operand_scale,0);
			if (err)
				abort_setop_fail(insn, "tic64x_operand_scale",
									err);
			insn->operand_values[i].resolved = 1;
		}

		goto skip_offset; /* Mwuuhahahaaaha */
	}

	/* Write offset/scale values - scale only if we have a scale bit */
	i = find_operand_index(insn->templ, tic64x_operand_rcoffset);
	if (i < 0)
		abort_no_operand(insn, "tic64x_operand_rcoffset");

	err = tic64x_set_operand(&insn->opcode, tic64x_operand_rcoffset, tmp);
	if (err)
		abort_setop_fail(insn, "tic64x_operand_rcoffset", err);
	insn->operand_values[i].resolved = 1;

	/* Don't set scale if insn always scales */
	if (!(insn->templ->flags & TIC64X_OP_MEMACC_SCALE)) {
		i = find_operand_index(insn->templ, tic64x_operand_scale);
		if (i < 0)
			abort_no_operand(insn, "tic64x_operand_scale");

		err = tic64x_set_operand(&insn->opcode,tic64x_operand_scale,sc);
		if (err)
			abort_setop_fail(insn, "tic64x_operand_scale", err);
		insn->operand_values[i].resolved = 1;
	}

	skip_offset:
	return;
}

void
tic64x_opreader_register(char *line, struct tic64x_insn *insn,
				enum tic64x_text_operand type)
{
	const char *err;
	struct tic64x_register *reg;
	enum tic64x_operand_type t2;
	int i, tmp;

	/* Expect only a single piece of text, should be register */
	reg = tic64x_sym_to_reg(line);
	if (!reg) {
		as_bad("Expected \"%s\" to be register", line);
		return;
	}

	switch (type) {
	case tic64x_optxt_srcreg1:
		t2 = tic64x_operand_srcreg1;
		break;
	case tic64x_optxt_srcreg2:
		t2 = tic64x_operand_srcreg2;
		break;
	case tic64x_optxt_dstreg:
		t2 = tic64x_operand_dstreg;
		break;
	default:
		as_bad("Unexpected operand type in tic64x_opreader_register");
		return;
	}

	/* Verify register is in same unit num as this instruction, or that
	 * we have a good excuse for using another one */
	if (!tic64x_optest_register(line, insn, type)) {
		as_bad("Register \"%s\" not suitable for this instruction "
			"format", insn->templ->mnemonic);
		return;
	}

	i = find_operand_index(insn->templ, t2);
	if (i < 0)
		/* XXX - I guess those abort routines should just print the
		 * enumeration number, as we're unlikely to have the text name*/
		abort_no_operand(insn, "{register}");

	err = tic64x_set_operand(&insn->opcode, t2, reg->num & 0x1F);
	if (err)
		abort_setop_fail(insn, "{register}", err);
	insn->operand_values[i].resolved = 1;

	/* Now set some more interesting fields - if working on destination reg,
	 * set the side of processor we're working on. Also set xpath field */
	if (t2 == tic64x_operand_dstreg) {

		/* Destination must be on same side of processor as we're
		 * executing, except when it's memory access. Bleaugh */

		if (insn->templ->flags & TIC64X_OP_MEMACCESS) {
			if ((insn->mem_unit_num == 2 &&
					!(reg->num & TIC64X_REG_UNIT2))
				|| (insn->mem_unit_num == 1 &&
					(reg->num & TIC64X_REG_UNIT2))) {
				as_bad("Destination register must match data "
					"path specifier");
				return;
			}
		} else {
			/* Ensure destination is just on the correct side */
			if ((insn->unit_num == 2 &&
					!(reg->num & TIC64X_REG_UNIT2))
					|| (insn->unit_num == 1 &&
					reg->num & TIC64X_REG_UNIT2)) {
				as_bad("Destination register must be on side "
					"or processor where insn executes");
				return;
			}
		}
	} else if (TXTOPERAND_CAN_XPATH(insn, type)) {
		/* This operand can use the xpath, do we? */
		if (((reg->num & TIC64X_REG_UNIT2) && insn->unit_num == 1) ||
		    (!(reg->num & TIC64X_REG_UNIT2) && insn->unit_num == 2)) {
			/* Yes */
			tmp = 1;
		} else {
			/* No */
			tmp = 0;
		}

		/* Cross-check with what user said */
                if (insn->uses_xpath == 0 && tmp == 1) {
			as_bad("Specify 'X' in execution unit when "
				"addressing registers in other side of "
				"processor");
			return;
		} else if (insn->uses_xpath == 1 && tmp == 0) {
			as_bad("'X' in execution unit, but cross-path register "
				"is on same side of processor as instruction");
			return;
		}

		i = find_operand_index(insn->templ, tic64x_operand_x);
		if (i < 0)
			abort_no_operand(insn, "tic64x_operand_x");

		err = tic64x_set_operand(&insn->opcode, tic64x_operand_x, tmp);
		if (err)
			abort_setop_fail(insn, "tic64x_operand_x", err);
		insn->operand_values[i].resolved = 1;
	}

	return;
}

void tic64x_opreader_double_register(char *line, struct tic64x_insn *insn,
			enum tic64x_text_operand optype)
{
	struct tic64x_register *reg1, *reg2;
	const char *err;
	char *rtext;
	enum tic64x_operand_type type;
	int tmp, i;
	char c;

	/* Double register is of the form "A0:A1", or whatever symbolic names
	 * user has assigned. Register numbers must be consecutive, and stored
	 * as the top four bits */

	/* Read up to ':' seperator */
	rtext = line;
	while (*line != ':' && !is_end_of_line[(int)*line])
		line++;

	/* Bail out if there wasn't one */
	if (is_end_of_line[(int)*line]) {
		as_bad("Unexpected end-of-line, expected ':' double register "
			"seperator");
		return;
	}

	/* Actually try and read register */
	*line = 0;
	reg1 = tic64x_sym_to_reg(rtext);
	if (!reg1) {
		as_bad("\"%s\" is not a register", rtext);
		return;
	}
	*line++ = ':';

	/* Now read up to next non-name */
	rtext = line;
	while (ISALPHA(*line) || ISDIGIT(*line) || *line == '_')
		line++;

	/* And find register */
	c = *line;
	*line = 0;
	reg2 = tic64x_sym_to_reg(rtext);
	if (!reg2) {
		as_bad("\"%s\" is not a register", rtext);
		*line = c;
		return;
	}
	*line = c;

	/* Now for some validation - same side? */
	if ((reg1->num & TIC64X_REG_UNIT2) != (reg2->num & TIC64X_REG_UNIT2)) {
		as_bad("Double register operands must be on same side of "
								"processor");
		return;
	}

	/* Consecutive? */
	if ((reg1->num & 0x1F) != (reg2->num & 0x1F) + 1) {
		as_bad("Double register operands must be consecutive");
		return;
	}

	/* These are fine and can be written into opcode operand */
	if (optype == tic64x_optxt_dwdst) {

		/* dword destination operands can have two forms - 5 bit
		 * register address of the even one of the pair, or four
		 * bit address, top bits of destination regs. Identify
		 * which way around we are, adjust value accordingly */

		i = find_operand_index(insn->templ, tic64x_operand_dwdst5);
		if (i < 0) {
			i = find_operand_index(insn->templ,
					tic64x_operand_dwdst4);
			if (i < 0)
				abort_no_operand(insn, "dwdst");

			type = tic64x_operand_dwdst4;
			tmp = (reg2->num & 0x1F) >> 1;
		} else {
			type = tic64x_operand_dwdst5;
			tmp = (reg2->num & 0x1F);
			if (tmp & 1) as_fatal(
				"tic64x_opreader_double_register: low "
				"bit set in register address");
		}
	} else if (optype == tic64x_optxt_dwsrc) {
		i = find_operand_index(insn->templ, tic64x_operand_dwsrc);
		if (i < 0)
			abort_no_operand(insn, "dwsrc");

		type = tic64x_operand_dwsrc;
		tmp = (reg2->num & 0x1F);
	} else {
		as_bad("tic64x_opreader_double_register: unknown operand type");
		return;
	}

	/* XXX XXX XXX - some of the add instructions can have a single reg
	 * identifier in one mode, then in another the same field is used for
	 * addressing a pair: do we know for certain that pair addresses are
	 * always shifted, even if there's a spare bit? */
	/* Update - spru732h p 241 has a comment indicating that for 5 bit wide
	 * address fields, we don't shift, and ensure the lowest bit is 0 */

	err = tic64x_set_operand(&insn->opcode, type, tmp);
	if (err)
		abort_setop_fail(insn, "dwreg", err);
	insn->operand_values[i].resolved = 1;

	/* Also in this series - if this dw pair happen to be the destintation,
	 * set the side field for this insn */
	if (optype == tic64x_optxt_dwdst) {
		if (!(insn->templ->flags & TIC64X_OP_SIDE))
			abort_no_operand(insn, "tic64x_operand_s");

		err = tic64x_set_operand(&insn->opcode, tic64x_operand_s,
				(reg1->num & TIC64X_REG_UNIT2) ? 1 : 0);
		if (err)
			abort_setop_fail(insn, "tic64x_operand_s", err);
	}

	return;
}
void
tic64x_opreader_constant(char *line, struct tic64x_insn *insn,
			enum tic64x_text_operand type)
{
	expressionS expr;
	const char *err;
	enum tic64x_operand_type realtype;
	int i;

	/* Pre-lookup the operand index we expect... */
	if (type == tic64x_optxt_nops) {
		realtype = tic64x_operand_nops;
		i = find_operand_index(insn->templ, realtype);
		if (i < 0)
			abort_no_operand(insn, "tic64x_operand_nops");

	} else if ((i = find_operand_index(insn->templ, tic64x_operand_const5))
									>= 0) {
		realtype = tic64x_operand_const5;
	} else if ((i = find_operand_index(insn->templ,
					tic64x_operand_const5p2)) >= 0) {
		realtype = tic64x_operand_const5p2;
	} else if ((i = find_operand_index(insn->templ,
					tic64x_operand_const21)) >= 0) {
		realtype = tic64x_operand_const21;
	} else if ((i = find_operand_index(insn->templ,
					tic64x_operand_const16)) >= 0) {
		realtype = tic64x_operand_const16;
	} else if ((i = find_operand_index(insn->templ,
					tic64x_operand_const15)) >= 0) {
		realtype = tic64x_operand_const15;
	} else if ((i = find_operand_index(insn->templ,
					tic64x_operand_const12)) >= 0) {
		realtype = tic64x_operand_const12;
	} else {
		abort_no_operand(insn, "tic64x_operand_const*");
	}

	tic64x_parse_expr(line, &expr);
	if (expr.X_op == O_constant) {
		if (type == tic64x_optxt_sconstant && expr.X_add_number < 0) {
			as_bad("Negative operand, expected unsigned");
			return;
		}

		err = tic64x_set_operand(&insn->opcode, realtype,
						expr.X_add_number);
		if (err) {
			/* Don't abort this time - user is allowed to enter
			 * a constant that's too large, it's not an internal
			 * error if that occurs */
			as_bad("Error setting constant operand: %s", err);
			return;
		}

		insn->operand_values[i].resolved = 1;
	} else {
		/* Not something useful right now, leave unresovled */
		memcpy(&insn->operand_values[i].expr, &expr, sizeof(expr));
	}

	return;
}

int
tic64x_compare_operands(struct tic64x_insn *insn,
			struct tic64x_op_template *templ,
						char **ops)
{
	enum tic64x_text_operand type;
	int i, j, ret;

	/* Loop through all operands, looking up expected type and calling
	 * test function for that type. Return true if they all match. Expects
	 * that there's at least one operand */

	for (i = 0; i < TIC64X_MAX_TXT_OPERANDS; i++) {
		if (ops[i] == NULL)
			/* Wherever in loop we are, we haven't errored out, so
			 * it looks like this matches */
			return 1;

		type = templ->textops[i];
		for (j = 0; tic64x_operand_readers[j].test; j++) {
			if (tic64x_operand_readers[j].type == type) {
				ret = tic64x_operand_readers[j].test(ops[i],
								insn, type);
				break;
			}
		}

		if (tic64x_operand_readers[j].test == NULL) {
			/* We hit a bad opcode record */
			as_fatal("tic64x_compare_operands: \"%s\" opcode has "
				"textop operand not in readers",
						templ->mnemonic);
		}

		if (ret == 0)
			return 0;

		/* Otherwise, move along to next operand */
	}

	/* If we ran out of operands, they must have all matched */
	return 1;
}


void
md_assemble(char *line)
{
	char *operands[TIC64X_MAX_TXT_OPERANDS];
	struct tic64x_insn *insn;
	struct tic64x_op_template *multi;
	char *mnemonic;
	enum tic64x_text_operand optype;
	int i, j, mvfail;

	mvfail = 0;
	insn = malloc(sizeof(*insn));
	memset(insn, 0, sizeof(*insn));

	/* pre-read hook will tell us if this had parallel double bar */
	if (tic64x_line_had_parallel_prefix)
		insn->parallel = 1;

	mnemonic = line;
	while (!ISSPACE(*line) && !is_end_of_line[(int)*line])
		line++;
	*line++ = 0;

	/* XXX quirk - TI assembly uses "RET {reg}" instead of "B {reg}". If
	 * there are more cases of renames like this it should be handled
	 * generically, but for now patch it up here (ugh) */
	if (!strcmp(mnemonic, "ret")) {
		strcpy(mnemonic, "b");
	} else if (!strcmp(mnemonic, "mv")) {
		/* XXX kludge: TI define a "mv" pseudo-op to describe writing
		 * the contents of one register to another. Their assembler
		 * apparently will read this, look at what's currently scheduled
		 * and select the most appropriate instruction that doesn't lead
		 * to a stall. This is entirely beyond what gas is supposed to
		 * do so hack it instead */
		mnemonic = "add";
		as_warn("Replacing \"mv\" instruction with add 0; schedule your"
			" own instructions");
		mvfail = 1; /* Horror */
	}
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

	/* I will scream if someone says "what if it isn't ascii" */
	insn->unit_num = *line++ - 0x30;
	if (insn->unit_num != 1 && insn->unit_num != 2) {
		as_bad("Bad execution unit number %d after \"%s\"",
					insn->unit_num, insn->templ->mnemonic);
		free(insn);
		return;
	}

	/* We might find either T1 or T2 at end of unit specifier, indicating
	 * which data path the loaded/stored data will travel through */
	if (*line == 'T') {
		line++;
		insn->mem_unit_num = *line++ - 0x30;
		if (insn->mem_unit_num != 1 && insn->mem_unit_num != 2) {
			as_bad("%d not a valid unit number for memory data path"
					" in \"%s\"", insn->mem_unit_num,
							insn->templ->mnemonic);
			free(insn);
			return;
		}
	} else {
		insn->mem_unit_num = -1;
	}

	/* There's also an 'X' suffix used to indicate we're using the cross
	 * path. */
	if (*line == 'X') {
		line++;
		insn->uses_xpath = 1;
	} else {
		insn->uses_xpath = 0;
	}

	/* Nom any leading spaces */
	while (ISSPACE(*line))
		line++;

	/* Turn string of operands into array of string pointers */
	memset(operands, 0, sizeof(operands));
	operands[0] = line;
	i = 1;
	while (!is_end_of_line[(int)*line] && i < TIC64X_MAX_TXT_OPERANDS) {
		if (*line == ',') {
			*line = 0;
			operands[i] = line + 1;
			i++;
		}
		line++;
	}

	if (mvfail) {
		/* We replaced mv with add, fiddle with operands */
		operands[2] = operands[1];
		operands[1] = "0";
	}

	/* Horror: if we have multiple operations for this mnemonic,
	 * start guessing which one we are. Grrr. */
	if (insn->templ->flags & TIC64X_OP_MULTI_MNEMONIC) {
		/* For each insn, test length and probe each operand */
		/* Count number of text operands... */
		for (i = 0; i < TIC64X_MAX_TXT_OPERANDS && operands[i]; i++)
			;

		if (i == 0) {
			as_bad("Cowardly refusing to try and match instruction "
				"with no operands against multiple opcodes for "
				"that opcode");
			return;
		}

		/* Loop through each insn template - warning, pointer abuse.
		 * assumes that templ pointed into tic64x_opcodes, and that
		 * the first one is marked with the MULTI_MNEMONIC flag */
		for (multi = insn->templ; !strcmp(multi->mnemonic,
					insn->templ->mnemonic); multi++) {
			for (j = 0; j < TIC64X_MAX_TXT_OPERANDS; j++)
				if (multi->textops[j] == tic64x_optxt_none)
					break;

			if (j != i)
				continue;

			if (j == 0) {
				as_fatal("tic64x md_assemble: instruction "
					"\"%s\" with zero operands and "
					"sharing mnemonics matches everything",
					multi->mnemonic);
			}

			/* Reject template if it doesn't support the execution
			 * unit specified by the user */
			if (!(UNITCHAR_2_FLAG(insn->unit) & multi->flags))
				continue;

			/* No such luck - probe each operand to see if it's
			 * what we expect it to be. So ugly it has to go in
			 * a different function */
			if (tic64x_compare_operands(insn, multi, operands))
				break;
		}

		if (strcmp(multi->mnemonic, insn->templ->mnemonic)) {
			as_bad("Unrecognised instruction format for \"%s\"",
						insn->templ->mnemonic);
			return;
		}

		insn->templ = multi;	/* Swap type of instruction */
	}

	/* Now that our instruction is definate, some checks */
	if (!(UNITCHAR_2_FLAG(insn->unit) & insn->templ->flags)) {
		as_bad("Instruction \"%s\" can't go in unit %C. XXX - currently"
			" have no way of representing instructions that go "
			"in multiple units", insn->templ->mnemonic, insn->unit);
		return;
	}

	if (insn->templ->flags & TIC64X_OP_FIXED_UNITNO) {
		if (((insn->templ->flags & TIC64X_OP_FIXED_UNIT2) &&
					insn->unit_num != 2) ||
		    (!(insn->templ->flags & TIC64X_OP_FIXED_UNIT2) &&
					insn->unit_num != 1)) {
			as_bad("\"%s\" can't execute on unit %d ",
				insn->templ->mnemonic, insn->unit_num);
			return;
		}
	}

	if (insn->templ->flags & TIC64X_OP_MEMACCESS) {
		if (insn->mem_unit_num == -1) {
			as_bad("Expected memory datapath T1/T2 in unit "
				"specifier for \"%s\"", insn->templ->mnemonic);
			return;
		}
	} else {
		if (insn->mem_unit_num != -1) {
			as_bad("Memory datapath T1/T2 specifier found for "
				"non-memory access instruction");
			return;
		}
	}

	if (!(insn->templ->flags & TIC64X_OP_USE_XPATH)) {
		if (insn->uses_xpath != 0) {
			as_bad("Unexpected 'X' in unit specifier for "
				"instruction that does not use cross-path");
			return;
		}
	}

	/* Did pre-read hook see a condition statement? Done here to allow
	 * more checking against unit/unit-no */
	if (tic64x_line_had_cond) {
		insn->cond_nz = tic64x_line_had_nz_cond;
		insn->cond_reg = tic64x_line_had_cond_reg->num;

		if ((insn->unit_num == 1 && (insn->cond_reg & TIC64X_REG_UNIT2))
		||(insn->unit_num == 2 && !(insn->cond_reg & TIC64X_REG_UNIT2)))
			if (insn->uses_xpath)
				as_warn("Caution: condition register on "
					"opposite side of processor, and xpath "
					"is used in instruction; author is "
					"uncertain whether this is permitted");

		/* See creg format */
		switch (insn->cond_reg) {
		case TIC64X_REG_UNIT2 | 0:
			insn->cond_reg = 1;
			break;
		case TIC64X_REG_UNIT2 | 1:
			insn->cond_reg = 2;
			break;
		case TIC64X_REG_UNIT2 | 2:
			insn->cond_reg = 3;
			break;
		case 1:
			insn->cond_reg = 4;
			break;
		case 2:
			insn->cond_reg = 5;
			break;
		case 0:
			insn->cond_reg = 6;
			break;
		default:
			as_bad("Invalid register for conditional, must be 0-2");
			return;
		}
	}
	/* Leave untouched if no condition - all zeros means unconditional */

	for (i = 0; i < TIC64X_MAX_TXT_OPERANDS && operands[i]; i++) {
		/* We have some text, lookup what kind of operand we expect,
		 * and call its parser / handler / whatever */
		optype = insn->templ->textops[i];
		for (j = 0; tic64x_operand_readers[j].reader != NULL; j++) {
			if (tic64x_operand_readers[j].type == optype)
				break;
		}

		if (tic64x_operand_readers[j].reader) {
			tic64x_operand_readers[j].reader(operands[i], insn,
					tic64x_operand_readers[j].type);
		} else {
			as_bad("\"%s\" has unrecognised operand %d",
				insn->templ->mnemonic, i);
		}
	}

	/* If this is the start of a new insn packet, dump the contents of
	 * the previous packet and start a new one. Include some sanity
	 * checks */

	if (tic64x_line_had_parallel_prefix) {
		/* Append this to the list */
		if (read_insns_index >= 8) {
			as_bad("Can't have more than 8 insns in an "
				"instruction packet");
			return;
		}

		read_insns[read_insns_index++] = insn;
	} else {
		tic64x_output_insn_packet();
		memset(read_insns, 0, sizeof(read_insns));
		read_insns_index = 0;

		read_insns[read_insns_index++] = insn;
	}

	return;
}

void
md_after_pass_hook()
{

	tic64x_output_insn_packet();
	return;
}

void
tic64x_output_insn_packet()
{
	struct tic64x_insn *insn;
	int i;

	/* Emit insns, with correct p-bits this time */
	for (i = 0; i < read_insns_index; i++) {
		insn = read_insns[i];

		if (i == read_insns_index - 1)
			/* Last insn in packet - no p bit */
			insn->parallel = 0;
		else
			insn->parallel = 1;

		tic64x_output_insn(insn);
		free(insn); /* XXX - sanity check */
	}

	return;
}

void
tic64x_output_insn(struct tic64x_insn *insn)
{
	char *out;
	int i, s, y;

	insn->opcode |= insn->templ->opcode;

	/* Side bit specifies execution unit except for memory access, which
	 * uses 's' for destination side / data path, and y for src/unit no */
	if (insn->templ->flags & TIC64X_OP_MEMACCESS) {
		s = (insn->mem_unit_num == 2) ? 1 : 0;
		y = (insn->unit_num == 2) ? 1 : 0;
	} else {
		s = (insn->unit_num == 2) ? 1 : 0;
		y = (insn->unit_num == 2) ? 1 : 0;
	}

	/* From bottom to top, fixed fields, the other operands */
	if (insn->parallel)
		tic64x_set_operand(&insn->opcode, tic64x_operand_p, 1);

	if (insn->templ->flags & TIC64X_OP_SIDE)
		tic64x_set_operand(&insn->opcode, tic64x_operand_s, s);

	if (insn->templ->flags & TIC64X_OP_UNITNO)
		tic64x_set_operand(&insn->opcode, tic64x_operand_y, y);

	if (insn->templ->flags & TIC64X_OP_COND) {
		tic64x_set_operand(&insn->opcode, tic64x_operand_z,
					(insn->cond_nz) ? 0 : 1);
		tic64x_set_operand(&insn->opcode, tic64x_operand_creg,
					insn->cond_reg);
	}

	for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
		if (insn->templ->operands[i] == tic64x_operand_invalid)
			continue;

		/* Create fixups for unresolved operands */
		if (!insn->operand_values[i].resolved) {
			if (insn->operand_values[i].expr.X_op == O_symbol) {
				as_warn("Implement fixups please");
			} else {
				as_fatal("Unresolved operand %d for \"%s\" is "
					"not a symbol (internal error)",
					i, insn->templ->mnemonic);
			}
		}
	}

	/* That should have generated our instruction with all the available
	 * data, now align and write that out */

	frag_align(2 /* align to 4 */, 0, 0);
	out = frag_more(4);

	/* Assume everything is little endian for now */
	bfd_putl32(insn->opcode, out);

	/* Now go back through and look for relocs */
	/* XXX - do this */
	return;
}
