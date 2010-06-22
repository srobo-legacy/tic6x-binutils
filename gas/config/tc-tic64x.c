/* Copyright blah */

#include <limits.h>
#include "as.h"
#include "safe-ctype.h"
#include "opcode/tic64x.h"
#include "obj-coff.h"
#include "struc-symbol.h"
#include "libbfd.h"

#define UNUSED(x) ((x) = (x))

#define TXTOPERAND_CAN_XPATH(insn, type) 				\
		((((insn)->templ->flags & TIC64X_OP_XPATH_SRC2) &&	\
					(type) == tic64x_optxt_srcreg2) ||\
		(((insn)->templ->flags & TIC64X_OP_XPATH_SRC1) &&	\
					(type) == tic64x_optxt_srcreg1))

#define abort_no_operand(insn, type)					\
		as_fatal("Instruction \"%s\" does not have expected "	\
			"operand type " type " (internal error)",	\
					insn->templ->mnemonic);

#define abort_setop_fail(insn, type) 					\
		as_fatal("Couldn't set operand " type " for "		\
			"instruction %s", insn->templ->mnemonic);

struct unitspec {
	int8_t		unit;		/* Character (ie 'L') or -1 */
	int8_t		unit_num;	/* 0 -> side 1, 1 -> side 2, or -1 */
	int8_t		mem_path;	/* 0 -> T1, 1 -> T2, not set: -1 */
	bfd_boolean	uses_xpath;
};

struct tic64x_insn {
	struct tic64x_op_template *templ;
	uint32_t opcode;		/* The opcode itself to be emitted,
					 * gets filled with operands as we
					 * work out what template to use */
	struct unitspec unitspecs;	/* Details about this instruction, what
					 * unit, side, datapath etc that it
					 * happens to use */
	int8_t parallel;		/* || prefix? */
	int8_t cond_nz;			/* Condition flag, zero or nz?*/
	int16_t cond_reg;		/* Register for comparison */

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

	/* Hack for ti's mv instruction failery - can't be resolved in initial
	 * pass, we need to inspect other parallel ops later and make a decision
	 * there */
	int mvfail;
	char *mvfail_op1;
	char *mvfail_op2;
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
int tic64x_line_had_cond;
int tic64x_line_had_nz_cond;
struct tic64x_register *tic64x_line_had_cond_reg;

static char *tic64x_parse_expr(char *s, expressionS *exp);
static void tic64x_asg(int x);
static void tic64x_noop(int x);
static void tic64x_comm(int x);
static void tic64x_sect(int x);
static void tic64x_fail(int x);
static struct tic64x_register *tic64x_sym_to_reg(char *name);
static int find_operand_index(struct tic64x_op_template *templ,
			enum tic64x_operand_type type);

static void tic64x_output_insn(struct tic64x_insn *insn, char *out, fragS *f,
								int pcoffs);

/* A few things we might want to handle - more complete table in tic54x, also
 * see spru186 for a full reference */
const pseudo_typeS md_pseudo_table[] =
{
	{"asg", 	tic64x_asg,		0},
	{"bss",		tic64x_fail,		0},
	{"byte",	cons,			1},
	{"comm",	tic64x_comm,		0},
	{"copy",	tic64x_fail,		0},
	{"def",		tic64x_fail,		0},
	{"dword",	cons,			8},
	{"hword",	cons,			2},
	{"include",	tic64x_fail,		0},
	{"mlib",	tic64x_fail,		0},
	{"ref",		tic64x_fail,		0},
	{"sect",	tic64x_sect,		0},
	{"section",	tic64x_sect,		0},
	{"set",		tic64x_fail,		0},
	{"size",	tic64x_noop,		0},
	{"string",	tic64x_fail,		0},
	{"text",	s_text,			0},
	{"type",	tic64x_noop,		0},
	{"usect",	tic64x_fail,		0},
	{"word",	cons, 			4},
	{NULL, 		NULL,			0}
};

/* Parser stuff to read a particularly kind of operand */

/* First a series of structs for storing the fundemental values of an operand */
struct opdetail_memaccess {
	const struct tic64x_register *base;
	union {
		const struct tic64x_register *reg;
		uint32_t const_offs;
	} offs;
	bfd_boolean const_offs;
};

struct opdetail_register {
	const struct tic64x_register *base;
};

struct opdetail_constant {
	uint32_t const_val;
	bfd_boolean is_signed;
};

struct read_operand {
	enum tic64x_text_operand type;
	union {
		struct opdetail_memaccess mem;
		struct opdetail_register reg;
		struct opdetail_constant constant;
	} u;
};

/* Read operand from line into read register struct */
typedef int (opreader) (char *line, bfd_boolean print_error,
				enum tic64x_text_operand optype,
				struct read_operand *out);
/* Some values for this to return: */
#define OPREADER_OK		0
#define OPREADER_BAD		1	/* Operand simply doesn't match */
#define OPREADER_PARTIAL_MATCH	2	/* _Could_ match, but not quite */

/* Take a parsed instruction and decide whether it's valid for a particular
 * instruction. If get_unitspec is set, inspect any fixed unit details in the
 * instruction and if it's possible for this situation to be valid, output any
 * further unitspec restrictions this operand would impose */
typedef int (opvalidate) (struct read_operand *in, bfd_boolean print_error,
				enum tic64x_text_operand optype,
				struct tic64x_insn *insn,
				bfd_boolean gen_unitspec,
				struct unitspec *spec);

/* Once we've actually decided to use a particular operand, we need a routine
 * to actually take it and write some bitfields in the opcode */
typedef void (opwrite) (struct read_operand *in,
				enum tic64x_text_operand optype,
				struct tic64x_insn *insn);

static opreader opread_none;
static opreader opread_memaccess;
static opreader opread_memrel15;
static opreader opread_register;
static opreader opread_double_register;
static opreader opread_constant;
static opreader opread_bfield;

static opvalidate opvalidate_none;
static opvalidate opvalidate_memaccess;
static opvalidate opvalidate_memrel15;
static opvalidate opvalidate_register;
static opvalidate opvalidate_double_register;
static opvalidate opvalidate_constant;

static opwrite opwrite_none;
static opwrite opwrite_memaccess;
static opwrite opwrite_memrel15;
static opwrite opwrite_register;
static opwrite opwrite_double_register;
static opwrite opwrite_constant;

struct {
	enum tic64x_text_operand type;
	opreader *reader;
	opvalidate *validate;
	opwrite *write;
} tic64x_operand_readers[] = {
{
	tic64x_optxt_none,
	opread_none,
	opvalidate_none,
	opwrite_none
},
{
	tic64x_optxt_memaccess,
	opread_memaccess,
	opvalidate_memaccess,
	opwrite_memaccess
},
{
	tic64x_optxt_memrel15,
	opread_memrel15,
	opvalidate_memrel15,
	opwrite_memrel15
},
{
	tic64x_optxt_dstreg,
	opread_register,
	opvalidate_register,
	opwrite_register
},
{
	tic64x_optxt_srcreg1,
	opread_register,
	opvalidate_register,
	opwrite_register
},
{
	tic64x_optxt_srcreg2,
	opread_register,
	opvalidate_register,
	opwrite_register
},
{
	tic64x_optxt_dwdst,
	opread_double_register,
	opvalidate_double_register,
	opwrite_double_register
},
{
	tic64x_optxt_dwsrc,
	opread_double_register,
	opvalidate_double_register,
	opwrite_double_register
},
{
	tic64x_optxt_dwsrc2,
	opread_double_register,
	opvalidate_double_register,
	opwrite_double_register
},
{
	tic64x_optxt_uconstant,
	opread_constant,
	opvalidate_constant,
	opwrite_constant
},
{
	tic64x_optxt_sconstant,
	opread_constant,
	opvalidate_constant,
	opwrite_constant
},
{
	tic64x_optxt_nops,
	opread_constant,
	opvalidate_constant,
	opwrite_constant
},
#if 0
/* At least for the moment, lets ignore the special case of bitfield operands,
 * they wreck havok with the nice abstractions I have worked out ;_; */
{tic64x_optxt_bfield,	opread_bfield,		opvalidate_constant},
#endif
{
	tic64x_optxt_none,
	NULL,
	NULL,
	NULL
}};


/* So, with gas at the moment, we can't detect the end of an instruction
 * packet until there's been a line without a || at the start. And we can't
 * make any reasonable decisions about p bits until we have a start and end.
 * So - read insns in with md_assemble, store them in the following table,
 * and emit them when we know we're done with a packet. Requires using
 * md_after_pass_hook probably. */
static int read_insns_index = 0;
static struct tic64x_insn *read_insns[8];
static char *read_insns_loc[8];
static fragS *read_insns_frags[8];
static segT packet_seg;
static int packet_subseg;

static void tic64x_output_insn_packet(void);
static int read_execution_unit(char **curline, struct tic64x_insn *insn);
static void generate_d_mv(struct tic64x_insn *insn);
static void generate_l_mv(struct tic64x_insn *insn, int isdw);
static void generate_s_mv(struct tic64x_insn *insn);
static int parse_operands(char **operands, struct tic64x_insn *insn);
static int apply_conditional(struct tic64x_insn *insn);

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

void
tic64x_cons_fix (fragS *frag, unsigned int offset, unsigned int len,
							expressionS *exp)
{
	bfd_reloc_code_real_type r;

	switch (len) {
	case 1:
		r = BFD_RELOC_TIC64X_BYTE;
		break;
	case 2:
		r = BFD_RELOC_TIC64X_WORD;
		break;
	case 4:
		r = BFD_RELOC_TIC64X_LONG;
		break;
	default:
		as_bad("Unsupported data relocation size %d", len);
		return;
	}

	fix_new_exp(frag, offset, len, exp, 0, r);
	return;
}

static void
tic64x_fail(int x ATTRIBUTE_UNUSED)
{

	as_fatal("Encountered supported but unimplemented pseudo-op");
	return;
}

static void
tic64x_noop(int x ATTRIBUTE_UNUSED)
{

	ignore_rest_of_line();
	return;
}

static void
tic64x_comm(int x ATTRIBUTE_UNUSED)
{

	s_lcomm(1); // Alignment required
	ignore_rest_of_line();
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

	char *name, *mod, *sect, *tmp;
	int len, sz;
	char c;

	/* Pad all sections so that they start on 0x20 alignments. Later on in
	 * linking it becomes highly painful to recalculate a bunch of PC
	 * relative addresses because the whole section has shifted by 4 bytes
	 * or something like that */
	frag_align(5, 0, 0);

	if (*input_line_pointer == '"') {
		name = demand_copy_C_string(&len);
		name = strdup(name);
	} else {
		name = input_line_pointer;
		c = get_symbol_end();
		sz = input_line_pointer - name;
		name = strdup(name);
		name[sz] = '\0';
		*input_line_pointer = c;
	}

	if (*input_line_pointer == ',') {
		input_line_pointer++;
		mod = input_line_pointer;
		ignore_rest_of_line();
		sz = input_line_pointer - mod;
		sz--; /* Zap end of line. this is feeling very hacky... */
		tmp = malloc(sz + 1);
		strncpy(tmp, mod, sz);
		tmp[sz] = 0;
		mod = tmp;
	} else {
		mod = NULL;
		demand_empty_rest_of_line();
	}

	sect = malloc(strlen(name) + ((mod) ? strlen(mod) + 5 : 5));
	sprintf(sect, "%s,%s\n", name, (mod) ? mod : "\"x\"");
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
md_pcrel_from_seg (fixS *fixP, segT segment)
{
	addressT addr;

	/* MERCY: we can use fx_pcrel_adjust to store the offset between fixup
	 * insn and start of packet. Which would be insanely difficult to
	 * calculate from here, given that we have to inspect p bits up to
	 * 8 packets backwards, possibly in a different fragment (and they only
	 * have singly linked lists... */

	/* NB: I have nothing to test against, but I assume this offset
	 * is _supposed_ to be negative */

	/* Turns out that actually, pcrel offsets are against the fetch packet
	 * address. So, truncate the lower four bits */
	addr = fixP->fx_frag->fr_address;
	addr -= segment->vma;
	addr += fixP->fx_where;
	addr -= fixP->fx_pcrel_adjust;
	addr &= ~0x1F;
	return (long)addr;
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
tc_gen_reloc(asection *section ATTRIBUTE_UNUSED, fixS *fixP)
{
	arelent *rel;
	asymbol *sym;
	int pcrel;

	pcrel = fixP->fx_pcrel;

	sym = symbol_get_bfdsym(fixP->fx_addsy);
	rel = malloc(sizeof(*rel));
	rel->sym_ptr_ptr = malloc(sizeof(asymbol *));
	rel->addend = 0;
	*rel->sym_ptr_ptr = sym;
	rel->address = fixP->fx_frag->fr_address + fixP->fx_where;

	/* XXX - I'm shakey on this, but adjust address from which offset is
	 * calculated by how far we are from pc */
	if (pcrel == 1)
		rel->address -= fixP->fx_pcrel_adjust;

	rel->howto = bfd_reloc_type_lookup(stdoutput, fixP->fx_r_type);

	if (!rel->howto)
		as_fatal("Couldn't generate a reloc with type %X",
						fixP->fx_r_type);

	return rel;
}

void
md_apply_fix(fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
	char *loc;
	uint32_t opcode;
	enum tic64x_operand_type type;
	int size, shift, ret, value;

	/* This line from tic54 :| */
	loc = fixP->fx_where + fixP->fx_frag->fr_literal;
	value = *valP;
	shift = 0;

	switch(fixP->fx_r_type) {
	case BFD_RELOC_TIC64X_BASE:
	case BFD_RELOC_TIC64X_SECT:
		as_fatal("Base/Section relocation: what are these anyway?");
	case BFD_RELOC_TIC64X_DIR15:
		type = tic64x_operand_const15;
as_fatal("FIXME: relocations of const15s need to know memory access size");
		break;
	case BFD_RELOC_TIC64X_PCR21:
		type = tic64x_operand_const21;
		shift = 2;
		break;
	case BFD_RELOC_TIC64X_PCR10:
		type = tic64x_operand_const10;
		shift = 2;
		break;
	case BFD_RELOC_TIC64X_LO16:
	case BFD_RELOC_TIC64X_S16:
		type = tic64x_operand_const16;
		break;
	case BFD_RELOC_TIC64X_HI16:
		type = tic64x_operand_const16;
		shift = 16;
		break;
	case BFD_RELOC_TIC64X_PCR7:
		type = tic64x_operand_const7;
		shift = 2;
		break;
	case BFD_RELOC_TIC64X_PCR12:
		type = tic64x_operand_const12;
		shift = 2;
		break;
	case BFD_RELOC_TIC64X_BYTE:
		type = tic64x_operand_data8;
		break;
	case BFD_RELOC_TIC64X_WORD:
		type = tic64x_operand_data16;
		break;
	case BFD_RELOC_TIC64X_LONG:
		type = tic64x_operand_data32;
		break;
	default:
		as_fatal("Bad relocation type 0x%X\n", fixP->fx_r_type);
		return;
	}

/* FACE: pcrel calculation required? */

	/* Mask value by size of operand - apparently gas/bfd will detect
	 * data loss. Possibly */
	size = tic64x_operand_positions[type].size;

	/* If operand gets shifted, actual data encoded is 2 bits larger */

	value &= ((1 << (size+shift)) - 1);

	/* And mask out the lower portion of the operand that will be dropped
	 * if any shifting occurs */
	if (shift != 0)
		value &= ~((1 << shift) - 1);

	opcode = bfd_get_32(stdoutput, loc);
	ret = tic64x_set_operand(&opcode, type, (shift) ? value >> shift
						: value, 0);
	bfd_put_32(stdoutput, opcode, loc);

	/* XXX FIXME: Ensure that too-large relocs are handled somehow. Test. */

	if (fixP->fx_addsy == NULL)
		fixP->fx_done = 1;

	*valP = value;

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

static enum bfd_reloc_code_real
type_to_rtype(struct tic64x_insn *insn, enum tic64x_operand_type type)
{

	switch(type) {
	case tic64x_operand_const21:
		if (!(insn->templ->flags & TIC64X_OP_CONST_PCREL))
			as_fatal("Opcode with 21b reloc, but isn't pcrel");

		return BFD_RELOC_TIC64X_PCR21;
	case tic64x_operand_const10:
		if (!(insn->templ->flags & TIC64X_OP_CONST_PCREL))
			as_fatal("Opcode with 10b reloc, but isn't pcrel");

		return BFD_RELOC_TIC64X_PCR10;
	case tic64x_operand_const16:
		/* One operand type, but semantics are different for mvk
		 * and mvkh/mvklh (according to docs) */

		if (!strcmp("mvk", insn->templ->mnemonic))
			return BFD_RELOC_TIC64X_S16;
		else if (!strcmp("mvkl", insn->templ->mnemonic))
			return BFD_RELOC_TIC64X_LO16;
		else if (!strcmp("mvkh", insn->templ->mnemonic) ||
			!strcmp("mvklh", insn->templ->mnemonic))
			return BFD_RELOC_TIC64X_HI16;
		else
			as_fatal("Relocation with const16 operand, but isn't "
				"mvk/mvkh/mvklh");
	case tic64x_operand_const12:
		if (!(insn->templ->flags & TIC64X_OP_CONST_PCREL))
			as_fatal("Opcode with 12b reloc, but isn't pcrel");

		return BFD_RELOC_TIC64X_PCR12;
	case tic64x_operand_const7:
		if (!(insn->templ->flags & TIC64X_OP_CONST_PCREL))
			as_fatal("Opcode with 7b reloc, but isn't pcrel");

		return BFD_RELOC_TIC64X_PCR7;
	default:
		as_fatal("Relocation on operand type that doesn't support it");
	}
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
	} else if (*line == '*' || *line == ';' || *line == '#') {
		/* This is a comment line */
		tic64x_line_had_parallel_prefix = 0;
	} else {
		/* Not a comment, no parallel insn; output instruction */
		/* packet */
		tic64x_output_insn_packet();
		memset(read_insns, 0, sizeof(read_insns));
		memset(read_insns_loc, 0, sizeof(read_insns));
		read_insns_index = 0;
		tic64x_line_had_parallel_prefix = 0;
	}

	/* Is there a conditional prefix? */
	while (ISSPACE(*line) && !is_end_of_line[(int)*line])
		line++;

	if (*line == '[') {
		tic64x_line_had_cond = 1;
		*line++ = ' ';
		tic64x_line_had_nz_cond = (*line == '!') ? 0 : 1;
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

static int
validation_and_conditions(struct tic64x_insn *insn)
{

	if (!(UNITCHAR_2_FLAG(insn->unit) & insn->templ->flags) &&
		!(insn->templ->flags & TIC64X_OP_ALL_UNITS)) {
		as_bad("Instruction \"%s\" can't go in unit %C. XXX - currently"
			" have no way of representing instructions that go "
			"in multiple units", insn->templ->mnemonic, insn->unit);
		return 1;
	}

	if (insn->templ->flags & TIC64X_OP_FIXED_UNITNO) {
		if (((insn->templ->flags & TIC64X_OP_FIXED_UNIT2) &&
					insn->unit_num != 2) ||
		    (!(insn->templ->flags & TIC64X_OP_FIXED_UNIT2) &&
					insn->unit_num != 1)) {
			as_bad("\"%s\" can't execute on unit %d ",
				insn->templ->mnemonic, insn->unit_num);
			return 1;
		}
	}

	if (insn->templ->flags & TIC64X_OP_MEMACCESS) {
		if (insn->mem_unit_num == -1) {
			as_bad("Expected memory datapath T1/T2 in unit "
				"specifier for \"%s\"", insn->templ->mnemonic);
			return 1;
		}
	} else {
		if (insn->mem_unit_num != -1) {
			as_bad("Memory datapath T1/T2 specifier found for "
				"non-memory access instruction");
			return 1;
		}
	}

	if (!(insn->templ->flags & TIC64X_OP_XPATH_SRC1) &&
				!(insn->templ->flags & TIC64X_OP_XPATH_SRC2)) {
		if (insn->uses_xpath != 0) {
			as_bad("Unexpected 'X' in unit specifier for "
				"instruction that does not use cross-path");
			return 1;
		}
	}

	return apply_conditional(insn);
}

static int
apply_conditional(struct tic64x_insn *insn)
{

	/* Did pre-read hook see a condition statement? */
	if (tic64x_line_had_cond) {
		if (insn->templ->flags & TIC64X_OP_NOCOND) {
			as_bad("Instruction does not have condition field");
			return 1;
		}

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
			return 1;
		}
	}

	return 0;
}

int
read_execution_unit(char **curline, struct tic64x_insn *insn)
{
	char *line;

	line = *curline;
	/* Expect ".xn" where x is {D,L,S,M}, and n is {1,2}. Can be followed
	 * by 'T' specifier saying which memory data path is being used */
	if (*line++ != '.') {
		/* No execution unit may be fine */
		return 0;
	}

	/* Stuff from here on should be an execution unit, so error if it's
	 * wrong */
	insn->unit = *line++;
	if (insn->unit != 'D' && insn->unit != 'L' && insn->unit != 'S' &&
							insn->unit != 'M') {
		as_bad("Unrecognised execution unit %C after \"%s\"",
					insn->unit, insn->templ->mnemonic);
		return -1;
	}

	/* I will scream if someone says "what if it isn't ascii" */
	insn->unit_num = *line++ - 0x30;
	if (insn->unit_num != 1 && insn->unit_num != 2) {
		as_bad("Bad execution unit number %d after \"%s\"",
					insn->unit_num, insn->templ->mnemonic);
		return -1;
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
			return -1;
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

	*curline = line;
	return 1;
}

int
parse_operands(char **operands, struct tic64x_insn *insn)
{
	int i, j;
	enum tic64x_text_operand optype;

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
			return 1;
		}
	}

	return 0;
}

void
md_assemble(char *line)
{
	char *operands[TIC64X_MAX_TXT_OPERANDS+1];
	struct tic64x_insn *insn;
	char *mnemonic;
	int i, bfieldfail, ret;

	bfieldfail = 0;
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
		insn->mvfail = 1; /* Horror */
	}

	/* Is this an instruction we've heard of? */
	insn->templ = hash_find(tic64x_ops, mnemonic);
	if (!insn->templ && !insn->mvfail) {
		as_bad("Unrecognised mnemonic %s", mnemonic);
		free(insn);
		return;
	}

	ret = read_execution_unit(&line, insn);
	if (ret < 0) {
		return;
	} else if (ret == 0) {
		as_warn("You haven't specified an execution unit for "
			"instruction %s: this is supposed to be supported, but "
			"expect something to explode, imminently", mnemonic);
	}

	/* Turn string of operands into array of string pointers */
	memset(operands, 0, sizeof(operands));
	operands[0] = line;
	if (strlen(operands[0]) == 0) {
		operands[0] = NULL;
		/* No operands at all */
	} else {
		i = 1;
		while (!is_end_of_line[(int)*line] &&
						i < TIC64X_MAX_TXT_OPERANDS+1) {
			if (*line == ',') {
				*line = 0;
				operands[i] = line + 1;
				i++;
			}
			line++;
		}
	}

	if (insn->mvfail) {
		/* Evil: store operands for later inspection, branch away */
		insn->mvfail_op1 = strdup(operands[0]);
		insn->mvfail_op2 = strdup(operands[1]);

		/* And because we need to handle it being 'mv' elsewhere,
		 * a goto. */
		goto wrapup;
	}

	/* Set mode nightmare: if insn has bitfield, congeal middle two
	 * operands back into one. Highly unpleasent, but saves having to
	 * patch up everything to support four operands. */
	if (insn->templ->flags & TIC64X_OP_BITFIELD) {
		/* Seeing how it was in this form in the first place.. */
		char *tmp;
		tmp = malloc(1024);
		snprintf(tmp, 1023, "%s, %s", operands[1], operands[2]);
		operands[1] = tmp;
		operands[2] = operands[3];
	}

	/* Now that we have an insn, validate a few general parts of it,
	 * also apply a conditional if the pre-read hook caught it */
	if (validation_and_conditions(insn))
		return;

	if (parse_operands(operands, insn))
		return;
wrapup:
	/* If this is the start of a new insn packet, dump the contents of
	 * the previous packet and start a new one. Include some sanity
	 * checks */

	if (read_insns_index == 0) {
		packet_seg = now_seg;
		packet_subseg = now_subseg;
	}

	if (tic64x_line_had_parallel_prefix) {
		/* Append this to the list */
		if (read_insns_index >= 8) {
			as_bad("Can't have more than 8 insns in an "
				"instruction packet");
			return;
		}
	}

	frag_align(2 /* align to 4 */, 0, 0);
	read_insns_loc[read_insns_index] = frag_more(4);
	read_insns_frags[read_insns_index] = frag_now;
	read_insns[read_insns_index++] = insn;

	if (insn->templ && insn->templ->flags & TIC64X_OP_BITFIELD) {
		free(operands[1]);
	}

	return;
}

void
md_after_pass_hook()
{

	tic64x_output_insn_packet();
	memset(read_insns, 0, sizeof(read_insns));
	memset(read_insns_loc, 0, sizeof(read_insns));
	read_insns_index = 0;

	/* Ensure that the last section in the file pads out to a size thats
	 * aligned to 0x20 - linking and recalculating a bunch of PCR addresses
	 * will not be fun if sections are stitched together and shift by some
	 * value that changes alignments */
	frag_align(5, 0, 0);

	return;
}

void
generate_l_mv(struct tic64x_insn *insn, int isdw)
{
	char buffer[4];
	char *operands[TIC64X_MAX_TXT_OPERANDS];

	if (isdw) {
		insn->templ = hash_find(tic64x_ops, "add");
		while (insn->templ->opcode != 0x58 &&
					!strcmp("add", insn->templ->mnemonic)) {
			insn->templ++;
		}

		if (insn->templ->opcode != 0x58)
			as_fatal("L unit add operand has disappeared");

		sprintf(buffer, "0");
		operands[0] = insn->mvfail_op1;
		operands[1] = buffer;
		operands[2] = insn->mvfail_op2;
		parse_operands(operands, insn);
	} else {
		insn->templ = hash_find(tic64x_ops, "or");
		while (insn->templ->opcode != 0x8F0 &&
					!strcmp("or", insn->templ->mnemonic)) {
			insn->templ++;
		}

		if (insn->templ->opcode != 0x8F0)
			as_fatal("L unit or operation has disappeared");

		sprintf(buffer, "0");
		operands[0] = buffer;
		operands[1] = insn->mvfail_op1;
		operands[2] = insn->mvfail_op2;
		parse_operands(operands, insn);
		/* munge operands */
	}

}

void
generate_d_mv(struct tic64x_insn *insn)
{
	char buffer[4];
	char *operands[TIC64X_MAX_TXT_OPERANDS];

	insn->templ = hash_find(tic64x_ops, "add");
	while (insn->templ->opcode != 0xAF0 &&
					!strcmp("add", insn->templ->mnemonic)) {
		insn->templ++;
	}

	if (insn->templ->opcode != 0xAF0)
		as_fatal("D unit or operation has disappeared");

	sprintf(buffer, "0");
	operands[0] = buffer;
	operands[1] = insn->mvfail_op1;
	operands[2] = insn->mvfail_op2;
	parse_operands(operands, insn);
}

void
generate_s_mv(struct tic64x_insn *insn)
{
	char buffer[4];
	char *operands[TIC64X_MAX_TXT_OPERANDS];

	insn->templ = hash_find(tic64x_ops, "add");
	while (insn->templ->opcode != 0x1A0 &&
					!strcmp("add", insn->templ->mnemonic)) {
		insn->templ++;
	}

	if (insn->templ->opcode != 0x1A0)
		as_fatal("S unit or operation has disappeared");

	sprintf(buffer, "0");
	operands[0] = buffer;
	operands[1] = insn->mvfail_op1;
	operands[2] = insn->mvfail_op2;
	parse_operands(operands, insn);
}

void
tic64x_output_insn_packet()
{
	struct tic64x_register *src2, *dst;
	struct tic64x_insn *insn;
	fragS *frag;
	char *out;
	int i, err, isdw, isxpath;
	char m[2], l[2], s[2], d[2];

	err = 0;
	memset(m, 0, sizeof(m));
	memset(l, 0, sizeof(l));
	memset(s, 0, sizeof(s));
	memset(d, 0, sizeof(d));

	/* No instructions -> meh */
	if (read_insns_index == 0)
		return;

	if (packet_seg != now_seg || packet_subseg != now_subseg) {
		as_bad("Segment changed within instruction packet");
		return;
	}

	/* If there were errors, don't attempt to output, state is probably
	 * not going to be valid. Leaky. */
	if (had_errors())
		return;

	/* We may need to make some decisions based on what units have been
	 * used so far - collect details on which ones those are. Also report
	 * an error if there's more than one execution unit being used in the
	 * same execution packet */
	for (i = 0; i < read_insns_index; i++) {
		insn = read_insns[i];
		if (insn->unit != 0 && insn->unit_num != 0) {
#define USEUNIT(a, i, c) do {						\
				if ((a)[(i)-1] != 0) {			\
					as_bad("Using unit %C%d more than once"\
						"in instruction packet",\
						(c), (i));		\
					err++;				\
				} else {				\
					(a)[(i)-1]++;			\
				}					\
			} while (0);

			switch (insn->unit) {
			case 'M':
				USEUNIT(m, insn->unit_num, 'M');
				break;
			case 'L':
				USEUNIT(l, insn->unit_num, 'L');
				break;
			case 'S':
				USEUNIT(s, insn->unit_num, 'S');
				break;
			case 'D':
				USEUNIT(d, insn->unit_num, 'D');
				break;
			default:
				as_fatal("Unrecognised execution unit in "
					"%s %d", __FILE__, __LINE__);
#undef USEUNIT
			}
		} else if (insn->unit_num != 0 || insn->unit != 0) {
			as_fatal("Reading insn packet units, found one "
				"specifier but not the other");
		}
	}

	/* Patch up mnemonics, actually implement 'mv's. Inject any other TI
	 * "Pseudo-op" failery here */
	for (i = 0; i < read_insns_index; i++) {
		insn = read_insns[i];
		if (insn->mvfail) {
			/* So it's a mv; we want to put it in whatever l/d/s
			 * slot is free, but we need to work out what
			 * requirements it has.... first, did the user give it
			 * a unit specifier, then, does it need an xpath and
			 * what side it has to be on */

			/* But first, a hack. */
			int side;
			if (insn->mvfail_op2[0] == 'B')
				side = 2;
			else
				side = 1;

			if (side != insn->unit_num) {
				insn->unit_num = side;
				tic64x_set_operand(&insn->opcode,
						tic64x_operand_s,
						(side == 1) ? 0 : 1, 0);
			}

			/* llvm-generated mv's are also unaware of whether
			 * the X path should be used, so work that out here */
			if (insn->mvfail_op2[0] != insn->mvfail_op1[0]){
				insn->uses_xpath = 1;
			} else {
				insn->uses_xpath = 0;
			}

			src2 = tic64x_sym_to_reg(insn->mvfail_op1);
			dst = tic64x_sym_to_reg(insn->mvfail_op2);

			if (!src2 || !dst) {
				as_bad("Bad operands \"%s\" and \"%s\" to mv",
					insn->mvfail_op1, insn->mvfail_op2);
				return;
			}

			/* Any special requirements? */
			isdw = 0;
			isxpath = 0;
			if (tic64x_optest_double_register(insn->mvfail_op1,
						insn, tic64x_optxt_dwsrc)) {
				isdw = 1;
			} else if (((src2->num & TIC64X_REG_UNIT2) &&
						!(dst->num & TIC64X_REG_UNIT2))
				|| (!(src2->num & TIC64X_REG_UNIT2) &&
						(dst->num & TIC64X_REG_UNIT2))){
				isxpath = 1;
			}

			if (insn->unit_num == 0 && insn->unit == 0) {
				/* No unit specified, guess */
				/* Side determined by destination reg */
				if (dst->num & TIC64X_REG_UNIT2) {
					insn->unit_num = 2;
				} else {
					insn->unit_num = 1;
				}

#define USEUNIT(u, n, a) do {						\
				(u)[(n)] = 1;				\
				insn->unit = a;				\
				insn->unit_num = n+1;			\
			} while (0);

#define GRABPATH(u, a) do {						\
				if ((u)[0] == 0) {			\
					USEUNIT((u), 0, (a))		\
				} else if ((u)[1] == 0) {		\
					USEUNIT((u), 1, (a))		\
				}					\
			} while (0);

				if (!isdw && !isxpath) {
					GRABPATH(d, 'D');
				}

				if (!isdw && insn->unit == 0) {
					GRABPATH(s, 'S');
				}

				if (insn->unit == 0) {
					GRABPATH(l, 'L');
				}

				if (insn->unit == 0) {
					as_bad("Can't schedule mv in remaining "
						"slots");
					return;
				}
#undef GRABPATH
#undef USEUNIT
			}

			if (insn->unit_num != 0 && insn->unit != 0) {
				/* Mkay */
				switch (insn->unit) {
				case 'L':
					generate_l_mv(insn, isdw);
					break;
				case 'S':
					generate_s_mv(insn);
					break;
				case 'D':
					generate_d_mv(insn);
					break;
				case 'M':
					as_bad("Can't generate mv instruction "
						"in multiply unit");
					return;
				default:
					as_fatal("Invalid unit encountered, "
						"%s %d\n", __FILE__, __LINE__);
				}
				free(insn->mvfail_op1);
				free(insn->mvfail_op2);

				/* If needs be, pump in conditional data */
				apply_conditional(insn);
			} else if (insn->unit_num != 0 || insn->unit != 0) {
				as_fatal("Patching up mv, have partial unit "
					"specifier, not complete");
			}
		}
	}

	/* Emit insns, with correct p-bits this time */
	for (i = 0; i < read_insns_index; i++) {
		insn = read_insns[i];
		out = read_insns_loc[i];
		frag = read_insns_frags[i];

		if (i == read_insns_index - 1)
			/* Last insn in packet - no p bit */
			insn->parallel = 0;
		else
			insn->parallel = 1;

		tic64x_output_insn(insn, out, frag, i * 4);
#if 0
/* Insn can't be freed, it might be being fixed up. Needs more thought
 * about insn lifetime */
		free(insn);
#endif
	}

	return;
}

void
tic64x_output_insn(struct tic64x_insn *insn, char *out, fragS *frag, int pcoffs)
{
	fixS *fix;
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
		tic64x_set_operand(&insn->opcode, tic64x_operand_p, 1, 0);

	if (!(insn->templ->flags & TIC64X_OP_NOSIDE))
		tic64x_set_operand(&insn->opcode, tic64x_operand_s, s, 0);

	if (insn->templ->flags & TIC64X_OP_UNITNO)
		tic64x_set_operand(&insn->opcode, tic64x_operand_y, y, 0);

	if (!(insn->templ->flags & TIC64X_OP_NOCOND) &&
					insn->cond_reg != 0) {
		tic64x_set_operand(&insn->opcode, tic64x_operand_z,
					(insn->cond_nz) ? 0 : 1, 0);
		tic64x_set_operand(&insn->opcode, tic64x_operand_creg,
					insn->cond_reg, 0);
	}

	for (i = 0; i < TIC64X_MAX_OPERANDS; i++) {
		if (insn->templ->operands[i] == tic64x_operand_invalid)
			continue;

		/* Create fixups for unresolved operands */
		if (!insn->operand_values[i].resolved) {
			if (insn->operand_values[i].expr.X_op == O_symbol) {

				int pcrel = (insn->templ->flags &
						TIC64X_OP_CONST_PCREL)
						? TRUE : FALSE;
				int rtype = type_to_rtype(insn,
						insn->templ->operands[i]);

				fix = fix_new_exp(frag, out - frag->fr_literal,
						4, &insn->operand_values[i].expr
						, pcrel, rtype);
				fix->fx_pcrel_adjust = pcoffs;
			} else {
				as_fatal("Unresolved operand %d for \"%s\" is "
					"not a symbol (internal error)",
					i, insn->templ->mnemonic);
			}
		}
	}

	/* Assume everything is little endian for now */
	bfd_putl32(insn->opcode, out);
	return;
}

void
opread_none(char *line, struct tic64x_insn *insn,
				enum tic64x_text_operand type)
{

	UNUSED(line);
	UNUSED(insn);
	UNUSED(type);
	as_bad("Excess operand");
	return;
}

void
opread_memaccess(char *line, struct tic64x_insn *insn,
			enum tic64x_text_operand type ATTRIBUTE_UNUSED)
{
	expressionS expr;
	char *regname, *offs;
	struct tic64x_register *reg, *offsetreg;
	int off_reg, pos_neg, pre_post, nomod_modify, has_offset, i, tmp, sc;
	int offs_operand, offs_size, err;
	char c, bracket;

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
			pre_post = TIC64X_ADDRMODE_PRE;
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
			pre_post = TIC64X_ADDRMODE_PRE;
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

	if (nomod_modify == -1 || pos_neg == -1 || pre_post == -1) {
		/* No pluses or minuses before or after base register - default
		 * to a positive offset/reg */
		nomod_modify = TIC64X_ADDRMODE_NOMODIFY;
		pos_neg = TIC64X_ADDRMODE_POS;
		pre_post = TIC64X_ADDRMODE_PRE;
	}

	/* Look for offset register of constant */
	offsetreg = NULL;
	bracket = '\0';
	if (*line == '[' || *line == '(') {
		has_offset = 1;
		c = (*line++ == '[') ? ']' : ')';
		bracket = c;
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

/* XXX - use gas' built in register section / register symbols support,
 * so that expression parsing can handle our detection of registers */

		/* Need to know early whether this is a register or not - if it
		 * is, should just be a single symbol that we can translate. */
		offsetreg = tic64x_sym_to_reg(offs);
		if (offsetreg) {
			/* joy */
			off_reg = TIC64X_ADDRMODE_REGISTER;
			/* Memory addr registers _have_ to come from the
			 * side of the processor we're executing on */
			if (((reg->num & TIC64X_REG_UNIT2) &&
					insn->unit_num != 2) ||
			    (!(reg->num & TIC64X_REG_UNIT2) &&
					insn->unit_num != 1)) {
				as_bad("Base address register must be on same "
					"side of processor as instruction");
				return;
			}
		} else {
			tic64x_parse_expr(offs, &expr);
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
		/* Can't have + and - /after/ the base register */
		if (nomod_modify == TIC64X_ADDRMODE_NOMODIFY &&
				pre_post == TIC64X_ADDRMODE_POST) {
			as_bad("Cannot have + and - modes after base register");
			return;
		}
	}
	/* Someone with more sleep needs to think up more checks */

	/* So - we have a base register, addressing mode, offset and maybe scale
	 * bit, which we need to fill out in insn. Simple ones first */
	err = tic64x_set_operand(&insn->opcode, tic64x_operand_basereg,
						reg->num & 0x1F, 0);
	if (err)
		abort_setop_fail(insn, "tic64x_operand_basereg");

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

	err = tic64x_set_operand(&insn->opcode, tic64x_operand_addrmode, tmp,
									0);
	if (err)
		abort_setop_fail(insn, "tic64x_operand_addrmode");

	offs_operand = find_operand_index(insn->templ, tic64x_operand_rcoffset);
	if (offs_operand < 0)
		as_fatal("memaccess with no r/c offset operand");

	if (!has_offset) {
		/* _really_ simple, no offset, no scale */
		/* XXX Don't scale register offsets for now, not aware of
		 * assembly syntax to express which way it should go */
		tmp = 0;
		sc = 0;
	} else if (off_reg == TIC64X_ADDRMODE_REGISTER) {
		/* All's fine and well */
		tmp = offsetreg->num & 0x1F;
		sc = 0;
	} else if (has_offset && expr.X_op == O_constant) {
		offs_size = tic64x_operand_positions[tic64x_operand_rcoffset].size;
		tmp = expr.X_add_number;

		if (bracket == ']') {
			/* Programmer provides pre-scaled value within square
			 * brackets - unscale it to meet our expectations */
			i = insn->templ->flags & TIC64X_OP_MEMSZ_MASK;
			i >>= TIC64X_OP_MEMSZ_SHIFT;
			tmp <<= i;
		}

		if (tmp < 0) {
			as_bad("tic64x-pedantic-mode: can't add/subtract a "
				"negative constant from base register, use "
				"correct addressing mode instead");
			return;
		}

		if (tmp > ((1 << offs_size) - 1) ||
			insn->templ->flags & TIC64X_OP_CONST_SCALE) {

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
			if (tmp > ((1 << offs_size) - 1 )) {
				as_bad("Constant offset too large");
				return;
			}

			/* Success; enable scaling */
			sc = 1;
		} else {
			/* When < size and we have a choice, don't scale */
			sc = 0;
		}
	} else {
		as_bad("Offset field not a constant");
		return;
	}

	/* So, we may have decided to scale above... but can we? */
	if (sc == 1 && !(insn->templ->flags & TIC64X_OP_MEMACC_SBIT) &&
			!(insn->templ->flags & TIC64X_OP_CONST_SCALE)) {
		/* We can't scale. Can't fit offset in field then */
		as_bad("Constant offset too large, or insn cannot scale it");
		return;
	} else if(sc == 1 && (insn->templ->flags & TIC64X_OP_MEMACC_SBIT)) {
		/* If we have that bit and must scale, do it here */
		/* Unless the instruction always scales, that is */

		if (!(insn->templ->flags & TIC64X_OP_CONST_SCALE)) {
			err = tic64x_set_operand(&insn->opcode,
						tic64x_operand_scale,
						c, 0);
			if (err)
				abort_setop_fail(insn, "tic64x_operand_scale");
		}
	}

	err = tic64x_set_operand(&insn->opcode, tic64x_operand_rcoffset, tmp,
									0);
	if (err)
		abort_setop_fail(insn, "tic64x_operand_r/c");

	insn->operand_values[offs_operand].resolved = 1;

	return;
}

void
opread_memrel15(char *line, struct tic64x_insn *insn,
				enum tic64x_text_operand type ATTRIBUTE_UNUSED)
{
	expressionS expr;
	int y, shift, val, index, err;
	char bracket;

	index = find_operand_index(insn->templ, tic64x_operand_const15);
	if (index < 0)
		as_fatal("memrel15 operand with no corresponding const15 "
								"field");

	/* Need to be able to distinguish between this and memaccess */
	if (*line++ != '*' || *line++ != '+' || *line++ != 'B' ||
							*line++ != '1') {
		as_bad("Malformed memrel15 operand");
		return;
	}

	if (*line == '4') {
		y = 0;
	} else if (*line == '5') {
		y = 1;
	} else {
		as_bad("memrel15 operand must use B14 or B15 base register");
		return;
	}
	err = tic64x_set_operand(&insn->opcode, tic64x_operand_y, y, 0);
	if (err)
		as_fatal("Error setting y bit in memrel15 operand");

	line++;

	/* Hopefully no-one puts whitespace between the base reg and offset,
	 * but just in case */
	while (ISSPACE(*line))
		line++;

	if (*line != '[' && *line != '(') {
		as_bad("Unexpected character when looking for '['");
		return;
	}

	bracket = *line++;
	while (ISSPACE(*line))
		line++;

	tic64x_parse_expr(line, &expr);
	if (expr.X_op == O_constant) {
		/* We're good, can set it now */

		if (expr.X_add_number < 0) {
			as_bad("Negative offset to base register not supported "
				"for this instruction form");
			return;
		}

		val = expr.X_add_number;
		if (bracket == '[') {
			/* User has pre-scaled this operand for us */
			shift = insn->templ->flags & TIC64X_OP_MEMSZ_MASK;
			shift >>= TIC64X_OP_MEMSZ_SHIFT;
			val <<= shift;
		}

		if (insn->templ->flags & TIC64X_OP_CONST_SCALE) {
			shift = insn->templ->flags & TIC64X_OP_MEMSZ_MASK;
			shift >>= TIC64X_OP_MEMSZ_SHIFT;
			if ((val & ((1 << shift) - 1))) {
				as_bad("Offset not aligned to memory access "
					"granuality");
				return;
			}

			val >>= shift;
		}

		err = tic64x_set_operand(&insn->opcode, tic64x_operand_const15,
									val, 0);
		if (err) {
			as_bad("Memory offset exceeds size of 15bit field");
			return;
		}

		insn->operand_values[index].resolved = 1;
	} else if (expr.X_op == O_symbol) {
		/* Going to have to emit a relocation */
		memcpy(&insn->operand_values[index].expr, &expr, sizeof(expr));
	} else {
		as_bad("Unsupported expression type");
	}

	return;
}

void
opread_register(char *line, struct tic64x_insn *insn,
				enum tic64x_text_operand type)
{
	struct tic64x_register *reg;
	enum tic64x_operand_type t2;
	int tmp, err;

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
		as_bad("Unexpected operand type in opread_register");
		return;
	}

	/* Verify register is in same unit num as this instruction, or that
	 * we have a good excuse for using another one */
	if (!tic64x_optest_register(line, insn, type)) {
		as_bad("Register \"%s\" not suitable for this instruction "
			"format", insn->templ->mnemonic);
		return;
	}

	err = tic64x_set_operand(&insn->opcode, t2, reg->num & 0x1F, 0);
	if (err)
		abort_setop_fail(insn, "{register}");

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

		err = tic64x_set_operand(&insn->opcode, tic64x_operand_x, tmp,
									0);
		if (err)
			abort_setop_fail(insn, "tic64x_operand_x");
	}

	return;
}

void opread_double_register(char *line, struct tic64x_insn *insn,
			enum tic64x_text_operand optype)
{
	struct tic64x_register *reg1, *reg2;
	char *rtext;
	enum tic64x_operand_type type;
	int tmp, i, side, err;
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
				"opread_double_register: low "
				"bit set in register address");
		}
		insn->operand_values[i].resolved = 1;
	} else if (optype == tic64x_optxt_dwsrc) {
		type = tic64x_operand_dwsrc;
		tmp = (reg2->num & 0x1F);
	} else if (optype == tic64x_optxt_dwsrc2) {
		type = tic64x_operand_srcreg1;
		tmp = (reg2->num & 0x1F);
	} else {
		as_bad("opread_double_register: unknown operand type");
		return;
	}

	err = tic64x_set_operand(&insn->opcode, type, tmp, 0);
	if (err)
		abort_setop_fail(insn, "dwreg");

	if (insn->templ->flags & TIC64X_OP_NOSIDE)
		abort_no_operand(insn, "tic64x_operand_s");

	/* Validate that this pair comes from the right side. It has to be
	 * the side of execution (can't put dw over xpath), unless it's memory
	 * access */

	side = (insn->templ->flags & TIC64X_OP_MEMACCESS)
		? insn->mem_unit_num : insn->unit_num;

	if ((side == 2 && !(reg2->num & TIC64X_REG_UNIT2)) ||
	    (side == 1 && (reg2->num & TIC64X_REG_UNIT2)))
		as_bad("Register pair differ in side from "
			"execution unit specifier");

	return;
}

static const enum tic64x_operand_type constant_types[] = {
	tic64x_operand_const5, tic64x_operand_const5p2,
	tic64x_operand_const21, tic64x_operand_const16,
	tic64x_operand_const15, tic64x_operand_const12,
	tic64x_operand_const10, tic64x_operand_const7,
	tic64x_operand_const4, tic64x_operand_invalid };

void
opread_constant(char *line, struct tic64x_insn *insn,
			enum tic64x_text_operand type)
{
	expressionS expr;
	enum tic64x_operand_type realtype;
	int i, j, shift, err;

	/* Pre-lookup the operand index we expect... */
	if (type == tic64x_optxt_nops) {
		realtype = tic64x_operand_nops;
		i = find_operand_index(insn->templ, realtype);
	} else {
		for (j = 0; constant_types[j] != tic64x_operand_invalid; j++) {
			if ((i = (find_operand_index(insn->templ,
						constant_types[j]))) >= 0) {
				realtype = constant_types[j];
				break;
			}
		}

		if (constant_types[j] == tic64x_operand_invalid) {
			abort_no_operand(insn, "tic64x_operand_const*");
		}
	}

	/* Do we need to shift at all? */
	if (insn->templ->flags & TIC64X_OP_CONST_SCALE) {
		shift = insn->templ->flags & TIC64X_OP_MEMSZ_MASK;
		shift >>= TIC64X_OP_MEMSZ_SHIFT;
	} else if (insn->templ->flags & TIC64X_OP_USE_TOP_HWORD) {
		shift = 16;
	} else {
		shift = 0;
	}

	tic64x_parse_expr(line, &expr);
	if (expr.X_op == O_constant) {
		if ((type == tic64x_optxt_uconstant || type ==tic64x_optxt_nops)
			&& expr.X_add_number < 0 &&
			!(insn->templ->flags & TIC64X_OP_NO_RANGE_CHK)) {
			as_bad("Negative operand, expected unsigned");
			return;
		}

		err = tic64x_set_operand(&insn->opcode, realtype,
					expr.X_add_number >> shift,
					(type == tic64x_optxt_sconstant)
					? 1 : 0);

		/* Trying to set operand that's too big: that's an error, unless
		 * it's an instruction that expects this and that has set the
		 * no range check flag */
		if (err && !(insn->templ->flags & TIC64X_OP_NO_RANGE_CHK)) {
			as_bad("Constant operand exceeds permitted size");
			return;
		}

		insn->operand_values[i].resolved = 1;
	} else {
		/* Not something useful right now, leave unresovled */
		/*  Shifting will be handled by fixup/reloc code */
		memcpy(&insn->operand_values[i].expr, &expr, sizeof(expr));
	}

	return;
}

void
opread_bfield(char *line, struct tic64x_insn *insn,
			enum tic64x_text_operand type ATTRIBUTE_UNUSED)
{
	expressionS expr1, expr2;
	char *part2;
	int val1, val2;

	/* This code remains untested until we actually run into a bfield op */

	part2 = strchr(line, ',');
	*part2++ = 0;

	tic64x_parse_expr(line, &expr1);
	tic64x_parse_expr(part2, &expr2);

	if (expr1.X_op != O_constant || expr2.X_op != O_constant) {
		as_bad("bitfield range operands not constants");
		return;
	}

	val1 = expr1.X_add_number;
	val2 = expr2.X_add_number;

	if (val1 >= 32 || val2 >= 32 || val1 < 0 || val2 < 0) {
		as_bad("bitfield operand out of range: must be 0-31");
		return;
	}

	tic64x_set_operand(&insn->opcode, tic64x_operand_const5, val1, 0);
	tic64x_set_operand(&insn->opcode, tic64x_operand_bitfldb, val2, 0);

	return;
}

int
opvalidate_none(struct read_operand *in ATTRIBUTE_UNUSED,
			bfd_boolean  print_error ATTRIBUTE_UNUSED,
			enum tic64x_text_operand optype ATTRIBUTE_UNUSED,
			struct tic64x_insn *insn ATTRIBUTE_UNUSED,
			bfd_boolean gen_unitspec ATTRIBUTE_UNUSED,
			struct unitspec *spec ATTRIBUTE_UNUSED)
{

	as_bad("Attempted to validate excess operand");
	return 1;
}

int
opvalidate_memaccess(struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_insn *insn, bfd_boolean gen_unitspec,
			struct unitspec *spec)
{

	UNUSED(in);
	UNUSED(print_error);
	UNUSED(optype);
	UNUSED(insn);
	UNUSED(gen_unitspec);
	UNUSED(spec);
	as_fatal("Unimplemented opvalidate_memaccess\n");
	return 1;
}

int
opvalidate_memrel15(struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_insn *insn, bfd_boolean gen_unitspec,
			struct unitspec *spec)
{

	UNUSED(in);
	UNUSED(print_error);
	UNUSED(optype);
	UNUSED(insn);
	UNUSED(gen_unitspec);
	UNUSED(spec);
	as_fatal("Unimplemented opvalidate_memrel15\n");
	return 1;
}

int
opvalidate_register(struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_insn *insn, bfd_boolean gen_unitspec,
			struct unitspec *spec)
{

	UNUSED(in);
	UNUSED(print_error);
	UNUSED(optype);
	UNUSED(insn);
	UNUSED(gen_unitspec);
	UNUSED(spec);
	as_fatal("Unimplemented opvalidate_register\n");
	return 1;
}

int
opvalidate_double_register(struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_insn *insn, bfd_boolean gen_unitspec,
			struct unitspec *spec)
{

	UNUSED(in);
	UNUSED(print_error);
	UNUSED(optype);
	UNUSED(insn);
	UNUSED(gen_unitspec);
	UNUSED(spec);
	as_fatal("Unimplemented opvalidate_double_register\n");
	return 1;
}

int
opvalidate_constant (struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_insn *insn, bfd_boolean gen_unitspec,
			struct unitspec *spec)
{

	UNUSED(in);
	UNUSED(print_error);
	UNUSED(optype);
	UNUSED(insn);
	UNUSED(gen_unitspec);
	UNUSED(spec);
	as_fatal("Unimplemented opvalidate_constant\n");
	return 1;
}

void
opwrite_none(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{

	UNUSED(in);
	UNUSED(optype);
	UNUSED(insn);
	as_bad("Attempted to write out excess operand\n");
	return;
}

void
opwrite_memaccess(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{

	UNUSED(in);
	UNUSED(optype);
	UNUSED(insn);
	as_fatal("Unimplemented opwrite_memaccess\n");
	return;
}

void
opwrite_memrel15(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{

	UNUSED(in);
	UNUSED(optype);
	UNUSED(insn);
	as_fatal("Unimplemented opwrite_memrel15\n");
	return;
}

void
opwrite_register(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{

	UNUSED(in);
	UNUSED(optype);
	UNUSED(insn);
	as_fatal("Unimplemented opwrite_register\n");
	return;
}

void
opwrite_double_register(struct read_operand *in,
			enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{

	UNUSED(in);
	UNUSED(optype);
	UNUSED(insn);
	as_fatal("Unimplemented opwrite_double_register\n");
	return;
}

void
opwrite_constant(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{

	UNUSED(in);
	UNUSED(optype);
	UNUSED(insn);
	as_fatal("Unimplemented opwrite_constant\n");
	return;
}
