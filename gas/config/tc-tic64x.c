/* Copyright blah */

#include <limits.h>
#include "as.h"
#include "safe-ctype.h"
#include "opcode/tic64x.h"
#include "obj-coff.h"
#include "struc-symbol.h"
#include "libbfd.h"

#define UNUSED(x) ((x) = (x))

static void tic64x_asg(int x);
static void tic64x_noop(int x);
static void tic64x_comm(int x);
static void tic64x_sect(int x);
static void tic64x_fail(int x);

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

struct tic64x_insn;
struct op_handler;

struct unitspec {
	int8_t		unit;		/* Character (ie 'L') or -1 */
	int8_t		unit_num;	/* 0 -> side 1, 1 -> side 2, or -1 */
	int8_t		mem_path;	/* 0 -> T1, 1 -> T2, not set: -1 */
	int8_t		uses_xpath;	/* 0 -> No, 1-> Yes, not set: -1 */
};

/* Parser stuff to read a particularly kind of operand */

/* First a series of structs for storing the fundemental values of an operand */
struct opdetail_memaccess {
	const struct tic64x_register *base;
	union {
		const struct tic64x_register *reg;
		expressionS expr;
	} offs;
	bfd_boolean const_offs;
	int addrmode;
	bfd_boolean scale_input;	/* Input in [] brackets, to be scaled */
};

struct opdetail_register {
	const struct tic64x_register *base;
};

struct opdetail_double_register {
	const struct tic64x_register *reg1;
	const struct tic64x_register *reg2;
};

struct opdetail_constant {
	expressionS expr;
};

struct read_operand {
	struct op_handler *handler;
	union {
		struct opdetail_memaccess mem;
		struct opdetail_register reg;
		struct opdetail_double_register dreg;
		struct opdetail_constant constant;
	} u;
};

/* Read operand from line into read register struct */
typedef int (opreader) (char *line, bfd_boolean print_error,
				struct read_operand *out);
/* Some values for this to return: */
#define OPREADER_OK		0
#define OPREADER_BAD		1	/* Operand simply doesn't match */
#define OPREADER_PARTIAL_MATCH	2	/* _Could_ match, but not quite */

/* Take a parsed instruction and decide whether it's valid for a particular
 * instruction. If get_unitspec is set, inspect any fixed unit details in the
 * instruction and if it's possible for this situation to be valid, output any
 * further unitspec restrictions this operand would impose */
typedef bfd_boolean (opvalidate) (struct read_operand *in,
				bfd_boolean print_error,
				enum tic64x_text_operand optype,
				struct tic64x_op_template *templ,
				bfd_boolean gen_unitspec,
				struct unitspec *spec);

/* Once we've actually decided to use a particular operand, we need a routine
 * to actually take it and write some bitfields in the opcode */
typedef void (opwrite) (struct read_operand *in,
				enum tic64x_text_operand optype,
				struct tic64x_insn *insn);

static opreader opread_memaccess;
static opreader opread_register;
static opreader opread_double_register;
static opreader opread_constant;

static opvalidate opvalidate_memaccess;
static opvalidate opvalidate_register;
static opvalidate opvalidate_double_register;
static opvalidate opvalidate_constant;

static opwrite opwrite_memaccess;
static opwrite opwrite_register;
static opwrite opwrite_double_register;
static opwrite opwrite_constant;

struct op_handler {
	enum tic64x_text_operand type1;
	enum tic64x_text_operand type2;
	enum tic64x_text_operand type3;
	const char *name;
	opreader *reader;
	opvalidate *validate;
	opwrite *write;
} operand_handlers[] = {
{
	tic64x_optxt_memaccess,
	tic64x_optxt_none,
	tic64x_optxt_none,
	"memacc",
	opread_memaccess,
	opvalidate_memaccess,
	opwrite_memaccess
},
{
	tic64x_optxt_dstreg,
	tic64x_optxt_srcreg1,
	tic64x_optxt_srcreg2,
	"reg",
	opread_register,
	opvalidate_register,
	opwrite_register
},
{
	tic64x_optxt_dwdst,
	tic64x_optxt_dwsrc,
	tic64x_optxt_dwsrc2,
	"dwreg",
	opread_double_register,
	opvalidate_double_register,
	opwrite_double_register
},
{
	tic64x_optxt_uconstant,
	tic64x_optxt_sconstant,
	tic64x_optxt_nops,
	"const",
	opread_constant,
	opvalidate_constant,
	opwrite_constant
}
};

#define NUM_OPERAND_HANDLERS 4


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
	struct read_operand operand_values[TIC64X_MAX_TXT_OPERANDS];
	int operands;

	/* Hack for ti's mv instruction failery -  we store its operands and
	 * make a decision about what actual instruction it will be when the
	 * entire packet gets emitted */
	int mvfail;

#define MAX_NUM_INSN_TEMPLATES 16
	int num_possible_templates;
	struct tic64x_op_template *possible_templates[MAX_NUM_INSN_TEMPLATES];

	/* So, we have a bunch of possible templates, but need to know what
	 * combination of unit specifiers they are valid for: IE, which sides
	 * they'll run on and whether they use the X path in that configuration.
	 * Which unit it'll run on is specified in the template flags. Insns
	 * where the unit it runs in makes a difference get a different template
	 * altogether. */
	/* The actual data we store is the following: Two bits indicate whether
	 * the template is valid on each of side 1 or two; another two bits
	 * dictate whether the xpath is used in this configuration. A fifth bit
	 * determines whether this insn uses the memory data path on side one
	 * or two; it's only valid if the TIC64X_OP_MEMACCESS flag is set in
	 * the template flags */
	/* ALSO: some flags indicating where a template registered an error
	 * in validation (say, const too large, register on wrong side, etc).
	 * This simplifies decisions on what error to print */
	uint32_t template_validity[MAX_NUM_INSN_TEMPLATES];
#define VALID_ON_UNIT1		1
#define VALID_ON_UNIT2		2
#define VALID_MASK		(VALID_ON_UNIT1 | VALID_ON_UNIT2)
#define VALID_USES_XPATH1	4
#define VALID_USES_XPATH2	8
#define VALID_USES_DPATH_2	0x10
#define NOTVALID_OP1_S1		0x20
#define NOTVALID_OP2_S1		0x40
#define NOTVALID_OP3_S1		0x80
#define NOTVALID_OP1_S2		0x100
#define NOTVALID_OP2_S2		0x200
#define NOTVALID_OP3_S2		0x400
#define NOTVALID_OP1		0x120
#define NOTVALID_OP2		0x240
#define NOTVALID_OP3		0x480
#define NOTVALID_S1		0xE0
#define NOTVALID_S2		0x700

	/* Final emission, we need to know various bits of information to write
	 * fixups and relocations - namely what fragment we're in, and the pcrel
	 * offset of this instruction. */
	fragS *output_frag;
	int output_frag_offs;
	int output_pcoffs;
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

/* Hash tables to store various mappings of names -> info */
static struct hash_control *tic64x_ops;
static struct hash_control *tic64x_reg_names;
static struct hash_control *tic64x_subsyms;

/* Data picked up by the pre-md-assemble hook about the incoming instruction */
int tic64x_line_had_parallel_prefix;
int tic64x_line_had_cond;
int tic64x_line_had_nz_cond;
struct tic64x_register *tic64x_line_had_cond_reg;

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

/* Resource management: so for the situation where we either have move insns
 * or other insns that haven't have a unit specifier attached, we need to keep
 * information about what instructions use what resources, and how we can
 * rejuggle things to fit all instructions in a packet into the eight available
 * units. */
struct resource_rec {
	/* Flags regarding what units instructions can be coerced to run on,
	 * limit and indexes correspond to the contents of the read_insns array
	 * and read_insns_index counter */
	uint8_t units[8];

	/* Flags for each insn */
#define CAN_S1	1
#define CAN_S2	2
#define CAN_L1	4
#define CAN_L2	8
#define CAN_D1	0x10
#define CAN_D2	0x20
#define CAN_M1	0x40
#define CAN_M2	0x80
};


static bfd_boolean pick_units_for_insn_packet(struct resource_rec *res);
static bfd_boolean select_insn_unit(struct resource_rec *res, int idx);
static void tic64x_output_insn_packet(void);
static void tic64x_output_insn(struct tic64x_insn *insn, char *out);

static int read_execution_unit(char **curline, struct unitspec *spec);
static char *tic64x_parse_expr(char *s, expressionS *exp);
static struct tic64x_register *tic64x_sym_to_reg(char *name);

static int apply_conditional(struct tic64x_insn *insn);
static void fabricate_mv_insn(struct tic64x_insn *insn, char *op1, char *op2);
static void finalise_mv_insn(struct tic64x_insn *insn);
static bfd_boolean beat_instruction_around_the_bush(char **operands,
			struct tic64x_insn *insn);

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
			as_fatal("md_begin: couldn't enter %s in hash table",
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
			as_fatal("md_begin: couldn't enter %s in hash table",
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
	int size, shift, value;

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
		as_fatal("Bad relocation type 0x%X", fixP->fx_r_type);
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
	tic64x_set_operand(&opcode, type, (shift) ? value >> shift : value);
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
	} else {
		/* No conditional, signal this with -1 */
		insn->cond_nz = -1;
		insn->cond_reg = -1;
	}

	return 0;
}

int
read_execution_unit(char **curline, struct unitspec *spec)
{
	char *line;

	line = *curline;
	/* Expect ".xn" where x is {D,L,S,M}, and n is {1,2}. Can be followed
	 * by 'T' specifier saying which memory data path is being used */
	if (*line++ != '.') {
		/* No execution unit */
		spec->unit = -1;
		spec->unit_num = -1;
		spec->mem_path = -1;
		spec->uses_xpath = -1;
		return 0;
	}

	/* Stuff from here on should be an execution unit, so error if it's
	 * wrong */
	spec->unit = *line++;
	if (spec->unit != 'D' && spec->unit != 'L' && spec->unit != 'S' &&
							spec->unit != 'M') {
		as_bad("Unrecognised execution unit %C", spec->unit);
		return -1;
	}

	spec->unit_num = *line++ - 0x31;
	if (spec->unit_num != 0 && spec->unit_num != 1) {
		as_bad("Bad execution unit number %d", spec->unit_num);
		return -1;
	}

	/* We might find either T1 or T2 at end of unit specifier, indicating
	 * which data path the loaded/stored data will travel through */
	if (*line == 'T') {
		line++;
		spec->mem_path = *line++ - 0x31;
		if (spec->mem_path != 0 && spec->mem_path != 1) {
			as_bad("'%d' is not a valid unit number for memory data"					" path", spec->mem_path);
			return -1;
		}
	} else {
		spec->mem_path = -1;
	}

	/* There's also an 'X' suffix used to indicate we're using the cross
	 * path. */
	if (*line == 'X') {
		line++;
		spec->uses_xpath = 1;
	} else {
		spec->uses_xpath = 0;
	}

	/* Nom any following spaces */
	while (ISSPACE(*line))
		line++;

	*curline = line;
	return 1;
}

void
fabricate_mv_insn(struct tic64x_insn *insn, char *op1, char *op2)
{
	struct op_handler *handler;
	uint8_t src_side, dst_side;

	handler = &operand_handlers[1];

	memset(&insn->operand_values[0], 0, sizeof(insn->operand_values[0]));

	/* Assert here that each operand is a register... */
	if (handler->reader(op1, TRUE, &insn->operand_values[1]))
		return;

	src_side = (insn->operand_values[1].u.reg.base->num & TIC64X_REG_UNIT2)
									? 1 : 0;

	if (handler->reader(op2, TRUE, &insn->operand_values[2]))
		return;

	dst_side = (insn->operand_values[2].u.reg.base->num & TIC64X_REG_UNIT2)
									? 1 : 0;

	insn->operands = 0; /* To avoid any intermediate code tripping up */
	insn->num_possible_templates = 1;
	insn->possible_templates[0] = &tic64x_mv_template[0];
	insn->template_validity[0] = 0;
	insn->template_validity[0] |= (dst_side) ? VALID_ON_UNIT2
						: VALID_ON_UNIT1;
	if (src_side != dst_side)
		insn->template_validity[0] |= (dst_side) ? VALID_USES_XPATH2
							: VALID_USES_XPATH1;

	/* However, if there's a unit specifier given in the assembly line,
	 * we should honour that */
	if (insn->unitspecs.unit != -1)
		finalise_mv_insn(insn);

	return;
}

void
finalise_mv_insn(struct tic64x_insn *insn)
{
	struct op_handler *handler;
	struct read_operand op;
	unsigned int wanted_opcode;
	bfd_boolean swap_constant;

	/* Righty; at this point we should have a tied down unit specifier */
	if (insn->unitspecs.unit == -1 || insn->unitspecs.unit_num == -1 ||
					insn->unitspecs.uses_xpath == -1)
		as_fatal("Finalising mv instruction with no internal unit "
			"specifier");

	handler = &operand_handlers[3];
	if (handler->reader("0", FALSE, &op))
		as_fatal("Error finalising mv instruction when parsing constant"
			" expression");

	switch (insn->unitspecs.unit) {
	case 'S':
		wanted_opcode = 0x1A0;
		break;
	case 'L':
		wanted_opcode = 0x58;
		swap_constant = TRUE;
		break;
	case 'D':
		wanted_opcode = 0xAF0;
		break;
	case 'M':
		as_bad("mv instruction cannot execute on 'M' unit");
	default:
		as_fatal("Invalid execution unit in internal record");
	}

	insn->templ = hash_find(tic64x_ops, "add");
	while (insn->templ->opcode != wanted_opcode &&
			!strcmp("add", insn->templ->mnemonic))
		insn->templ++;

	if (insn->templ->opcode != wanted_opcode)
		as_fatal("Desired template when finalising mv instruction has "
			"disappeared");

	if (swap_constant) {
		memcpy(&insn->operand_values[0], &insn->operand_values[1],
					sizeof(insn->operand_values[1]));
		memcpy(&insn->operand_values[1], &op, sizeof(op));
	} else {
		memcpy(&insn->operand_values[0], &op, sizeof(op));
	}

	insn->possible_templates[0] = insn->templ;
	/* Uses xpath and validity flags remain the same as before - all that
	 * really changed is that we squirted a '0' constant into the right
	 * place and set the template to the correct add instruction for the
	 * unit specified */

	return;
}

void
md_assemble(char *line)
{
	char *operands[TIC64X_MAX_TXT_OPERANDS+1];
	struct tic64x_insn *insn;
	char *mnemonic;
	int i,  ret;

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
	} else if (insn->mvfail) {
		/* Special template for mv, isn't actually an instruction */
		insn->templ = &tic64x_mv_template[0];
	}

	ret = read_execution_unit(&line, &insn->unitspecs);
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

	/* If pre-assemble hook saw a conditional, place that information in
	 * the instruction record */
	if (apply_conditional(insn))
		return;

	if (!insn->mvfail) {
		if (beat_instruction_around_the_bush(operands, insn))
			return;
	} else {
		fabricate_mv_insn(insn, operands[0], operands[1]);
	}

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

/* This is the beef of assembling; here we work out what operands are, and
 * use that info to decide what instruction (out of possibly many) we're going
 * to pick. Crucially, we're discovering whether there *is* a valid
 * configuration for the instruction we're parsing, not which one. We may need
 * to modify our view or choice later on when the whole instruction packet
 * gets emitted */
bfd_boolean
beat_instruction_around_the_bush(char **operands, struct tic64x_insn *insn)
{
	struct unitspec spec;
	struct tic64x_op_template *templ, *cur;
	struct op_handler *handler;
	int i, idx, ret;
	uint8_t valid, tmp_byte;
	bfd_boolean matched_unit, matched_side, matched_xpath;

	memset(insn->operand_values, 0, sizeof(insn->operand_values));

	/* So: for every operand in the list, we try parsing it until we find
	 * a reader that accepts it. If we don't find one, iterate back through
	 * looking for a partial match that can print a useful error. If no
	 * partial matches, scream. */
	i = 0;
	while (operands[i] != NULL) {

		for (idx = 0; idx < NUM_OPERAND_HANDLERS; idx++) {
			ret = operand_handlers[idx].reader(operands[i],
					FALSE, &insn->operand_values[i]);

			if (ret == OPREADER_OK)
				break;
		}

		/* So; did we find a match to this operand? */
		if (ret != OPREADER_OK) {
			/* No; Look for a partial match */
			for (idx = 0; idx < NUM_OPERAND_HANDLERS; idx++) {
				ret = operand_handlers[idx].reader(operands[i],
						FALSE,&insn->operand_values[i]);

				if (ret == OPREADER_PARTIAL_MATCH) {
					operand_handlers[idx].reader(operands[i]
						,TRUE,&insn->operand_values[i]);
					return TRUE;
				}
			}

			/* No partial match at all */
			as_bad("Unrecognized operand \"%s\"", operands[i]);
			return TRUE;
		}

		/* Move along to next operand */
		insn->operand_values[i].handler = &operand_handlers[idx];
		i++;
	}

	insn->operands = i;

	/* Yay, we now have some operands. Now for the delicious task of working
	 * out which one to pick from the bunch. First, lets actually make a
	 * list of all the different templates that _might_ actually match,
	 * then compare their operands against the possible operands we just
	 * read, and store the resulting set. insn->templ points at the first
	 * instruction in the list of them that we're looking at */
	insn->num_possible_templates = 0;

	/* Store the first instruction in the list so we can compare names */
	templ = insn->templ;
	cur = templ;
	while (1) {
		/* Check we haven't gone past the block of instructions that
		 * we're interested in */
		if (cur->mnemonic == NULL ||
					strcmp(templ->mnemonic, cur->mnemonic))
			break;

		for (i = 0; i < insn->operands; i++) {
			handler = insn->operand_values[i].handler;
			if (cur->textops[i] != handler->type1 &&
				cur->textops[i] != handler->type2 &&
				cur->textops[i] != handler->type3)
				break;
		}

		if (i == insn->operands) { /* It matched */
			if (insn->num_possible_templates + 1 ==
							MAX_NUM_INSN_TEMPLATES)
				as_fatal("More than %d potential instructions "
					"when parsing %s, increase max template"
					" cap", MAX_NUM_INSN_TEMPLATES,
					cur->mnemonic);

			insn->possible_templates[insn->num_possible_templates]
				= cur;
			insn->num_possible_templates++;
		}

		/* Point at next instruction in the opcode template table */
		cur++;
	}

	/* If nothing matched at all, bail and inform the user. Picking the
	 * error here is nontrivial, because although we've correctly parsed
	 * all the operands, it doesn't match any of the shapes we expect.
	 * Dumping all the shapes we might have accepted is unacceptably verbose
	 * (unless perhaps the user askes for it; future feature?), so let's
	 * print out what operand form we were looking for and let the user
	 * work out what's wrong */
	if (insn->num_possible_templates == 0) {
		char message[128];

		sprintf(message, "No instruction matched format \"%s",
						insn->templ->mnemonic);

		/* Chance of buffer overflow is nil while we're limited to
		 * three operands and their names are less than ~30 chars */
		for (i = 0; i < insn->operands; i++) {
			strcat(message, " ");
			strcat(message, insn->operand_values->handler->name);
			if (i != insn->operands - 1)
				strcat(message, ",");
		}

		strcat(message, "\"");
		as_bad(message);
		return FALSE;
	}

	/* We now have a gigantic list of all the instruction templates that
	 * might match the _form_ of the current instruction. We now iterate
	 * through each, trying a variety of specifier combinations to see how
	 * this instruction could be formed, and what options it would use in
	 * that case.
	 * Crucially, at this stage all we care about is that there is _one_
	 * valid way to output this instruction. If there are more than one,
	 * thats fine too. We'll decide on what particular form is chosen when
	 * the instruction packet gets emitted */

	for (idx = 0; idx < insn->num_possible_templates; idx++) {

		insn->template_validity[idx] = 0;
		cur = insn->possible_templates[idx];

		/* The important part: work out what side this will
		 * work on, and what extra constraints there are */
		spec.unit = spec.mem_path = spec.uses_xpath = -1;

		for (spec.unit_num = 0; spec.unit_num < 2; spec.unit_num++) {
			for (i = 0; i < insn->operands; i++)
				if (insn->operand_values[i].handler->validate(
						&insn->operand_values[i],
						FALSE, cur->textops[i], cur,
						TRUE, &spec))
					break;

			if (i == insn->operands) { /* Reached end, is valid */

				/* Final check - is this template fixed on one
				 * side of the processor, and is that the
				 * opposite side to the one we're checking right
				 * now? */
				if (cur->flags & TIC64X_OP_FIXED_UNITNO) {
					if (cur->flags & TIC64X_OP_FIXED_UNIT2){
						if (spec.unit_num == 0)
							continue;
					} else {
						if (spec.unit_num == 1)
							continue;
					}
				}

				if (spec.unit_num == 0) {
					tmp_byte = VALID_ON_UNIT1;
					if (spec.uses_xpath == 1)
						tmp_byte |= VALID_USES_XPATH1;

				} else {
					tmp_byte = VALID_ON_UNIT2;
					if (spec.uses_xpath == 1)
						tmp_byte |= VALID_USES_XPATH2;
				}

				insn->template_validity[idx] |= tmp_byte;
				if (spec.mem_path == 1) {
					insn->template_validity[idx] |=
							VALID_USES_DPATH_2;
				}

				/* Sanity check - to ensure the use dpath 2...
				 * indicator flag is valid for insns with
				 * TIC64X_OP_MEMACCESS set, ensure validator
				 * handler actually set the mem_path field */
				if (cur->flags & TIC64X_OP_MEMACCESS)
					if (spec.mem_path == -1)
						as_fatal("Memory access insn "
							"encountered, but "
							"register handler did "
							"not specify which side"
							" data path is used");
			} else { /* If we didn't match an operand... */
				switch (i) {
				case 0:
					i = (spec.unit_num == 0)
							? NOTVALID_OP1_S1
							: NOTVALID_OP1_S2;
					break;
				case 1:
					i = (spec.unit_num == 0)
							? NOTVALID_OP2_S1
							: NOTVALID_OP2_S2;
					break;
				case 2:
					i = (spec.unit_num == 0)
							? NOTVALID_OP3_S1
							: NOTVALID_OP3_S2;
					break;
				default:
					as_fatal("Can't have more than 3 "
						"internal operands");
				}

				insn->template_validity[idx] |= i;
			}
		} /* End of side 1-or-2 loop */
	} /* End of templates loop */

	/* This is the point where a bunch of user errors are going to turn
	 * up: registers on the wrong side, constants that are too big, memory
	 * offsets that are too large or won't scale. Yet again it's nontrival
	 * to work out what error to print, because we don't know what form the
	 * user ultimately wanted. So, go back through operands, finding the
	 * instruction that best matches before an error occurs: IE, how far can
	 * we get into the operand list before finding an error. When that's
	 * found, first remaning template is used to validate (and print the
	 * error message */
	/* First, did we find anything valid at all? */
	for (idx = 0; idx < insn->num_possible_templates; idx++)
		if ((insn->template_validity[idx] & VALID_MASK) != 0)
			break;

	if (idx == insn->num_possible_templates) {
		/* So an error occured; find an operand to print the error for*/

		/* Try final operand... */
		for (idx = 0; idx < insn->num_possible_templates; idx++)
			if (insn->template_validity[idx] & NOTVALID_OP3)
				break;

		/* Otherwise second operand */
		if (idx == insn->num_possible_templates)
			for (idx = 0; idx < insn->num_possible_templates; idx++)
				if (insn->template_validity[idx] & NOTVALID_OP2)
					break;

		/* Otherwise the first */
		if (idx == insn->num_possible_templates)
			for (idx = 0; idx < insn->num_possible_templates; idx++)
				if (insn->template_validity[idx] & NOTVALID_OP1)
					break;

		if (idx == insn->num_possible_templates)
			as_fatal("No valid instruction templates found, but no "
				"invalid operands found either - internal error"
				);

		cur = insn->possible_templates[idx];
		/* OK, we have a template to play with. Which instruction? */
		if (insn->template_validity[idx] & NOTVALID_OP3) {
			i = 2;
			if (insn->template_validity[idx] & NOTVALID_OP3_S1)
				spec.unit_num = 0;
			else
				spec.unit_num = 1;
		 } else if (insn->template_validity[idx] & NOTVALID_OP2) {
			i = 1;
			if (insn->template_validity[idx] & NOTVALID_OP2_S1)
				spec.unit_num = 0;
			else
				spec.unit_num = 1;
		 } else {
			i = 0;
			if (insn->template_validity[idx] & NOTVALID_OP1_S1)
				spec.unit_num = 0;
			else
				spec.unit_num = 1;
		}

		/* Call validator, permit it to print error */
		spec.unit = spec.mem_path = spec.uses_xpath =-1;
		if (!insn->operand_values[i].handler->validate(
				&insn->operand_values[i], TRUE, cur->textops[i],
				cur, FALSE, &spec))
			as_fatal("Validator for %s marked operand invalid, but"
				"then changed its mind; internal error",
				insn->operand_values[i].handler->name);

		/* kdone */
		return TRUE;
	}

	/* Bonus points! If the user added a unit specifier, check whether
	 * any of the matching templates can be used with those constraints */
	/* If they didn't, we're good */
	if (insn->unitspecs.unit == -1)
		return FALSE;

	matched_unit = matched_side = matched_xpath = FALSE;
	for (idx = 0; idx < insn->num_possible_templates; idx++) {
		cur = insn->possible_templates[idx];

		switch (insn->unitspecs.unit) {
		case 'S':
			i = TIC64X_OP_UNIT_S;
			break;
		case 'L':
			i = TIC64X_OP_UNIT_L;
			break;
		case 'D':
			i = TIC64X_OP_UNIT_D;
			break;
		case 'M':
			i = TIC64X_OP_UNIT_M;
			break;
		default:
			as_fatal("Invalid unit specifier in internal unitspec "
								"record");
		}

		/* Does this template run on the specified unit? */
		if (!(cur->flags & i))
			continue;

		matched_unit = TRUE;

		/* How about the specified side? Also store the xpath flag
		 * for this side for checking a little later */
		if (insn->unitspecs.unit_num == 0) {
			if (!(insn->template_validity[idx] & VALID_ON_UNIT1))
				continue;
			valid = insn->template_validity[idx] &
					VALID_USES_XPATH1;
		} else if (insn->unitspecs.unit_num == 1) {
			if (!(insn->template_validity[idx] & VALID_ON_UNIT2))
				continue;
			valid = insn->template_validity[idx] &
					VALID_USES_XPATH2;
		} else {
			as_fatal("Invalid side specifier in internal unitspec "
								"record");
		}

		matched_side = TRUE;

		/* Finally; if the xpath was specified check it concurs with the
		 * operand parser; and the same if it wasn't specified */
		if (insn->unitspecs.uses_xpath == 0) {
			if (valid != 0)
				continue;
		} else if (insn->unitspecs.uses_xpath == 1) {
			if (valid == 0)
				continue;
		} else {
			as_fatal("Invalid xpath specifier in internal unitspec "
								"record");
		}

		matched_xpath = TRUE;

		/* Looks like its good. See if the memory path is needed for
		 * this instruction */
		if (cur->flags & TIC64X_OP_MEMACCESS) {
			if (insn->unitspecs.uses_xpath == -1)
				continue;

			if (insn->unitspecs.mem_path == 0) {
				if (insn->template_validity[idx] &
							VALID_USES_DPATH_2)
					continue;
			} else if (insn->unitspecs.mem_path == 1) {
				if (!(insn->template_validity[idx] &
							VALID_USES_DPATH_2))
					continue;
			} else {
				as_fatal("Invalid memory path specifier in "
						"internal unitspec record");
			}
		} else {
			/* did user specify it on a non-data-path insn? */
			if (insn->unitspecs.mem_path != -1)
				continue;
		}

		/* Success; now eliminate all other possible instructions */
		insn->template_validity[0] = insn->template_validity[idx];
		insn->possible_templates[0] = cur;
		insn->num_possible_templates = 1;
		return FALSE;
	} /* end of iterating through instruction templates */

	/* We didn't find any instruction templates that matched the specifiers
	 * the user put in. There can be many of them, and they can fail for
	 * many reasons; this makes selecting a particular error nontrivial.
	 * So we'll report an error based on which template got the closest to
	 * being accepted */

	if (matched_unit == FALSE) {
		as_bad("Instruction cannot execute on %C unit",
				insn->unitspecs.unit);
		return TRUE;
	}

	if (matched_side == FALSE) {
		as_bad("Instruction must execute on other side of processor");
		return TRUE;
	}

	if (matched_xpath == FALSE) {
		as_bad("'X'path unit specifier incorrectly used or omitted");
		return TRUE;
	}

	/* Otherwise, the only other reason we could have rejected this is
	 * because the memory data path specifier was wrong */
	as_bad("Memory data path specified incorrectly used or omitted");
	return TRUE;
}

/* Instruction unit selector - look at what units the specified instruction
 * (by index) can execute on, and decide an order of choices to try the rest of
 * selection with. The choice that eliminates the least number of possibilities 
 * in the rest of selection comes first; IE, the choice that leaves the most
 * options open later */
bfd_boolean
select_insn_unit(struct resource_rec *input, int idx)
{
	struct resource_rec res;
	int i, j, k, flag;
	uint8_t weights[8];

	/* If we've reached the bottom of the list of insns */
	if (idx == read_insns_index)
		return FALSE;

	/* Calculate weights of different choices. '8' means we can't make this
	 * choice anyway due to it being impossible, or prevented by a previous
	 * selection. Other numbers represent the number of other instructions
	 * that could use the specified unit, but which won't if we make this
	 * choice */
	for (i = 0, flag = 1; i < 8; i++, flag <<= 1) {
		if (!(input->units[idx] & flag)) {
			/* This instruction can't go on that unit anyway */
			weights[i] = 8;
			continue;
		}

		/* What else runs on this? */
		weights[i] = 0;
		for (j = 0; j < 8; j++) {
			if (input->units[j] & flag) {
				weights[i]++;
			}
		}

		/* So now thats a weight, with a minimum of '1' (this insn),
		 * we'll remove that bias for convenience */
		weights[i]--;
	}

	/* So here we are with our weights; lets try them out */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			if (weights[j] == i) {
				/* Copy resource state so we can revert it */
				memcpy(&res, input, sizeof(res));

				/* Eliminate all uses of this unit */
				flag = 1 << j;
				for (k = 0; k < 8; k++)
					res.units[k] &= ~flag;

				/* Except for _this_ insn */
				res.units[idx] |= flag;

				/* Does that work? */
				if (!select_insn_unit(&res, idx+1)) {
					memcpy(input, &res, sizeof(res));
					return FALSE;
				}

				/* Can't use this unit then */
				weights[j] = 8;
			}
		}
	}

	/* If we reach this point, we've been through each unit and discovered
	 * either that we can't run on it or that the other insns can't be
	 * selected if we make this choice. So, make this the callers problem,
	 * who either makes a different choice of unit for a previous insn,
	 * or acknowledges that this packet is unselectable */

	/* But first a sanity check */
	for (i = 0; i < 8; i++)
		if (weights[i] != 8)
			as_fatal("Reached end of unit selection pass, but not "
				"all units are marked as impossible to "
				"select");

	return TRUE;
}

bfd_boolean
pick_units_for_insn_packet(struct resource_rec *res)
{
	struct tic64x_insn *insn;
	struct tic64x_op_template *templ;
	int i, j, idx, flag;
	uint8_t use1, use2;

	for (i = 0; i < read_insns_index; i++) {
		insn = read_insns[i];
		res->units[i] = 0;

		for (idx = 0; idx < insn->num_possible_templates; idx++) {
			templ = insn->possible_templates[idx];

			for (j = 0; j < 4; j++) {
				switch (j) {
				case 0:
					flag = TIC64X_OP_UNIT_S;
					use1 = CAN_S1;
					use2 = CAN_S2;
					break;
				case 1:
					flag = TIC64X_OP_UNIT_L;
					use1 = CAN_L1;
					use2 = CAN_L2;
					break;
				case 2:
					flag = TIC64X_OP_UNIT_D;
					use1 = CAN_D1;
					use2 = CAN_D2;
					break;
				case 3:
					flag = TIC64X_OP_UNIT_M;
					use1 = CAN_M1;
					use2 = CAN_M2;
					break;
				}

				if (templ->flags & flag) {
					if (insn->template_validity[idx] &
							VALID_ON_UNIT1)
						res->units[i] |= use1;
					if (insn->template_validity[idx] &
							VALID_ON_UNIT2)
						res->units[i] |= use2;
				}
			} /* Units in template loop */
		} /* Template loop */
	} /* Read instructions loop */

	return select_insn_unit(res, 0);
}

void
tic64x_output_insn_packet()
{
	struct resource_rec res;
	struct tic64x_insn *insn;
	struct tic64x_op_template *cur;
	fragS *frag;
	char *out;
	int i, idx, err, wanted_unit;
	uint8_t unit_flag;
	bfd_boolean xpath1_used, xpath2_used, dpath1_used, dpath2_used;

	err = 0;

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

	/* Attempt to schedule existing instructions into available units */
	if (pick_units_for_insn_packet(&res)) {
		as_bad("Unable to allocate all instructions to units in "
			"execute packet: a resource conflict occured");
		as_bad("Coming soon to an assembler near you: descriptive "
			"error messages!");
		return;
	}

	/* Loop through all instructions in this packet, moving the decided
	 * unit specs into the instruction record, and eliminating the remaining
	 * instruction templates */
	for (i = 0; i < read_insns_index; i++) {
		insn = read_insns[i];
		unit_flag = res.units[i];

		switch (unit_flag) {
		case CAN_S1:
		case CAN_S2:
			insn->unitspecs.unit = 'S';
			wanted_unit = TIC64X_OP_UNIT_S;
			break;
		case CAN_L1:
		case CAN_L2:
			insn->unitspecs.unit = 'L';
			wanted_unit = TIC64X_OP_UNIT_L;
			break;
		case CAN_D1:
		case CAN_D2:
			insn->unitspecs.unit = 'D';
			wanted_unit = TIC64X_OP_UNIT_D;
			break;
		case CAN_M1:
		case CAN_M2:
			insn->unitspecs.unit = 'M';
			wanted_unit = TIC64X_OP_UNIT_M;
			break;
		}

		switch (unit_flag) {
		case CAN_S1:
		case CAN_L1:
		case CAN_D1:
		case CAN_M1:
			insn->unitspecs.unit_num = 0;
			break;
		case CAN_S2:
		case CAN_L2:
		case CAN_D2:
		case CAN_M2:
			insn->unitspecs.unit_num = 1;
		}

		/* We now have a unit and a side - go and find the first thing
		 * that matches this in the big list of templates */
		for (idx = 0; idx < insn->num_possible_templates; idx++) {
			cur = insn->possible_templates[idx];
			if (cur->flags & wanted_unit) {
				insn->templ = cur;
				insn->num_possible_templates = 0;

				if (insn->template_validity[idx] &
					(VALID_USES_XPATH1 | VALID_USES_XPATH2))
					insn->unitspecs.uses_xpath = 1;
				else
					insn->unitspecs.uses_xpath = 0;

				if (cur->flags & TIC64X_OP_MEMACCESS) {
					if (insn->template_validity[idx] &
							VALID_USES_DPATH_2)
						insn->unitspecs.mem_path = 1;
					else
						insn->unitspecs.mem_path = 0;
				}

				break;
			}
		}

		/* Ok, we've fetched the template; anything else? */
		if (insn->mvfail)
			finalise_mv_insn(insn);
	}

	/* Righty. Everything should now be in some kind of standard form.
	 * Apply some extra sanity checks */

	xpath1_used = xpath2_used = dpath1_used = dpath2_used = FALSE;
	for (i = 0; i < read_insns_index; i++) {
		insn = read_insns[i];

		if (insn->unitspecs.uses_xpath == 1) {
			if (insn->unitspecs.unit_num == 0) {
				if (xpath1_used == TRUE) {
					as_bad("Mutiple uses of x-path 1 in "
						"execute packet");
				} else {
					xpath1_used = TRUE;
				}
			} else {
				if (xpath2_used == TRUE) {
					as_bad("Multiple uses of x-path 2 in "
						"execute packet");
				} else {
					xpath2_used = TRUE;
				}
			}
		}

		if (insn->unitspecs.mem_path == 0) {
			if (dpath1_used) {
				as_bad("Multiple uses of data path 1 in "
					"execute packet");
			} else {
				dpath1_used = TRUE;
			}
		} else if (insn->unitspecs.mem_path == 1) {
			if (dpath2_used) {
				as_bad("Multiple uses of data path 2 in "
					"execute packet");
			}
		}

		/* That checks for resource violations. Finally, are the
		 * conditional attributes attached to this sane? */

		if (insn->cond_nz != -1) {
			/* This is the band for side-B registers */
			if (insn->cond_reg >= 1 && insn->cond_reg <= 3) {
				if (insn->unitspecs.unit_num == 0) {
					as_bad("Condition register read from "
						"wrong side of processor in "
						"execute packet");
					/* XXX - how about printing which
					 * particular instruction it is that
					 * went wrong, eh? */
				}
			} else {
				if (insn->unitspecs.unit_num == 1) {
					as_bad("Condition register read from "
						"wrong side of processor in "
						"execute packet");
				}
			}
		}
	} /* End of loop checking instruction resources */

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

		insn->output_frag = frag;
		insn->output_frag_offs = out - frag->fr_literal;
		insn->output_pcoffs = i *4;
		tic64x_output_insn(insn, out);
#if 0
/* Insn can't be freed, it might be being fixed up. Needs more thought
 * about insn lifetime */
		free(insn);
#endif
	}

	return;
}

void
tic64x_output_insn(struct tic64x_insn *insn, char *out)
{
	int i, s, y;

	/* Generate the wadge of binary that represents the assembled
	 * instruction. This consists of setting the operand independant
	 * portions of the opcode, then calling the operand writer helper
	 * to actually pump out operand information */

	insn->opcode |= insn->templ->opcode;

	/* The side bit is a special case - most of the time it does mean which
	 * side the instruction executes on, but in the case of memory access
	 * the 'y' bit does that job, and the 's' bit specifies which data path
	 * the data takes. Except in the case of memrel15, where s still
	 * specifies the datapath, but y is something else.
	 * ANYWAY: so we have to swap the meaning of s and maybe write y if the
	 * instruction is memory access */
	if (insn->templ->flags & TIC64X_OP_MEMACCESS) {
		s = (insn->unitspecs.mem_path == 2) ? 1 : 0;
		y = (insn->unitspecs.unit_num == 2) ? 1 : 0;
	} else {
		s = (insn->unitspecs.unit_num == 2) ? 1 : 0;
		y = 0;
	}

	if (!(insn->templ->flags & TIC64X_OP_NOSIDE))
		tic64x_set_operand(&insn->opcode, tic64x_operand_s, s);

	if (insn->templ->flags & TIC64X_OP_UNITNO) {
		if (!(insn->templ->flags & TIC64X_OP_MEMACCESS))
			as_fatal("Insn marked UNITNO but not MEMACCESS");

		tic64x_set_operand(&insn->opcode, tic64x_operand_y, y);
	}

	if (insn->parallel)
		tic64x_set_operand(&insn->opcode, tic64x_operand_p, 1);

	if (!(insn->templ->flags & TIC64X_OP_NOCOND) &&
					insn->cond_reg != 0) {
		tic64x_set_operand(&insn->opcode, tic64x_operand_z,
					(insn->cond_nz) ? 0 : 1);
		tic64x_set_operand(&insn->opcode, tic64x_operand_creg,
					insn->cond_reg);
	}

	/* Now, pump out some operands */
	for (i = 0; i < insn->operands; i++) {

		insn->operand_values[i].handler->write(&insn->operand_values[i],
						insn->templ->textops[i], insn);

		/* Happily we don't need to beat fixups here, that can be
		 * delagated to the writer routine */
	}

	/* Assume everything is little endian for now */
	bfd_putl32(insn->opcode, out);
	return;
}

#define READ_ERROR(x) if (print_error) as_bad x

int
opread_memaccess(char *line, bfd_boolean print_error, struct read_operand *out)
{
	expressionS expr;
	char *regname, *offs;
	struct tic64x_register *reg, *offsetreg;
	int off_reg, pos_neg, pre_post, nomod_modify, has_offset;
	char c, bracket;

	off_reg = -1;
	pos_neg = -1;
	pre_post = -1;
	nomod_modify = -1;
	has_offset = -1;

	/* We expect firstly to start wih a '*' */
	if (*line++ != '*') {
		READ_ERROR(("expected '*' before memory operand"));

		return OPREADER_BAD;
	}

	/* See page 79 of spru732h for table of address modes */
	if (*line == '+') {
		pos_neg = TIC64X_ADDRMODE_POS;
		pre_post = TIC64X_ADDRMODE_PRE;
		if (*(line+1) == '+') {
			/* Preincrement */
			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			line += 2;
		} else {
			nomod_modify = TIC64X_ADDRMODE_NOMODIFY;
			line++;
		}
	} else if (*line == '-') {
		pos_neg = TIC64X_ADDRMODE_NEG;
		pre_post = TIC64X_ADDRMODE_PRE;
		if (*(line+1) == '-') {
			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			line += 2;
		} else {
			nomod_modify = TIC64X_ADDRMODE_NOMODIFY;
			line++;
		}
	}

	/* We should now have an alpha-num register name, possibly .asg'd */
	regname = line;
	while (ISALPHA(*line) || ISDIGIT(*line) || *line == '_')
		line++;

	/* If there's something that can't be a register name here,
	 * complain about it */
	if (regname == line) {
		READ_ERROR(("Unexpected '%C' reading memory base register",
								*line));
		return OPREADER_PARTIAL_MATCH;
	}

	/* Now truncate the input line so that we only have the text of the
	 * register name, try to look that up */
	c = *line;
	*line = 0;
	reg = tic64x_sym_to_reg(regname);

	if (!reg) {
		READ_ERROR(("Expected base address register, found \"%s\"",
								regname));
		return OPREADER_PARTIAL_MATCH;
	}

	/* un-truncate input line */
	*line = c;

	/* We should now have a register to work with - it can be suffixed
	 * with a postdecrement/increment, offset constant or register */
	if (*line == '-') {
		if (*(line+1) == '-') {
			if (pos_neg != -1 || nomod_modify != -1) {
				READ_ERROR(("Can't specify both pre and post"
					" operators on address register"));
				return OPREADER_PARTIAL_MATCH;
			}

			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			pos_neg = TIC64X_ADDRMODE_NEG;
			pre_post = TIC64X_ADDRMODE_POST;
			line += 2;
		} else {
			READ_ERROR(("Bad operator after address register"));
			return OPREADER_PARTIAL_MATCH;
		}
	} else if (*line == '+') {
		if (*(line+1) == '+') {
			if (pos_neg != -1 || nomod_modify != -1) {
				READ_ERROR(("Can't specify both pre and post"
					" operators on address register"));
				return OPREADER_PARTIAL_MATCH;
			}

			nomod_modify = TIC64X_ADDRMODE_MODIFY;
			pos_neg = TIC64X_ADDRMODE_POS;
			pre_post = TIC64X_ADDRMODE_POST;
			line += 2;
		} else {
			READ_ERROR(("Bad operator after address register"));
			return OPREADER_PARTIAL_MATCH;
		}
	}

	if (nomod_modify == -1 || pos_neg == -1 || pre_post == -1) {
		/* No pluses or minuses before or after base register - default
		 * to a positive offset/reg */
		nomod_modify = TIC64X_ADDRMODE_NOMODIFY;
		pos_neg = TIC64X_ADDRMODE_POS;
		pre_post = TIC64X_ADDRMODE_PRE;
	}

	/* Look for offset register or constant */
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
			READ_ERROR(("Unexpected end of file while reading "
						"address register offset"));
			return OPREADER_PARTIAL_MATCH;
		}

		/* We have an offset - read it into an expr, then
		 * leave it for the moment */
		c = *line;
		*line = 0;

		/* We have some text - is it a register? */
		offsetreg = tic64x_sym_to_reg(offs);
		if (offsetreg) {
			off_reg = TIC64X_ADDRMODE_REGISTER;
		} else {
			/* No, must be a constant, parse that instead */
			tic64x_parse_expr(offs, &expr);
			off_reg = TIC64X_ADDRMODE_OFFSET;

			/* There are some kinds of expression that are either
			 * invalid or likely to cause damage to your eyes: */
			if (expr.X_op == O_illegal) {
				READ_ERROR(("Memory offset is neither a "
					"register nor valid expression"));
				return OPREADER_PARTIAL_MATCH;
			} else if (expr.X_op == O_absent) {
				READ_ERROR(("Expected an expression for memory "
					"offset, found nothing"));
				return OPREADER_PARTIAL_MATCH;
			} else if (expr.X_op == O_symbol) {
				as_warn("Caution - you have used a symbol based"
					" expression for a memory _offset_ - "
					"this is supported by TIs relocations "
					"and loader, but the codepath for "
					"writing these in binutils is untested."
					);
			}
		}
	} else {
		/* No offset, implement as zero constant offs */
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
		READ_ERROR(("Trailing rubbish at end of address operand"));
		return OPREADER_PARTIAL_MATCH;
	}

	/* Now that we've read some stuff, now fill out the read_operand struct
	 * with the details of whats been parsed */
	out->u.mem.base = reg;
	out->u.mem.addrmode = off_reg | pos_neg | pre_post | nomod_modify;
	out->u.mem.scale_input = (bracket == ']') ? TRUE : FALSE;
	if (off_reg == TIC64X_ADDRMODE_OFFSET) {
		out->u.mem.const_offs = TRUE;
		if (has_offset) {
			memcpy(&out->u.mem.offs.expr, &expr, sizeof(expr));
		} else {
			tic64x_parse_expr("0", &out->u.mem.offs.expr);
		}
	} else {
		out->u.mem.const_offs = FALSE;
		out->u.mem.offs.reg = offsetreg;
	}

	return OPREADER_OK;
}

int
opread_register(char *line, bfd_boolean print_error, struct read_operand *out)
{
	struct tic64x_register *reg;

	/* Expect only a single piece of text, should be register. Simple. */
	reg = tic64x_sym_to_reg(line);
	if (!reg) {
		READ_ERROR(("Expected \"%s\" to be register", line));
		return OPREADER_BAD;
	}

	out->u.reg.base = reg;

	return OPREADER_OK;
}

int
opread_double_register(char *line, bfd_boolean print_error,
				struct read_operand *out)
{
	struct tic64x_register *reg1, *reg2;
	char *rtext;
	char c;

	/* Double register is of the form "A0:A1", or whatever symbolic names
	 * user has assigned. Register numbers must be consecutive. */

	/* Read up to ':' seperator */
	rtext = line;
	while (*line != ':' && !is_end_of_line[(int)*line])
		line++;

	/* Bail out if there wasn't one */
	if (is_end_of_line[(int)*line]) {
		READ_ERROR(("Unexpected end-of-line reading double register"));
		return OPREADER_BAD;
	}

	/* Actually try and read register */
	*line = 0;
	reg1 = tic64x_sym_to_reg(rtext);
	if (!reg1) {
		READ_ERROR(("\"%s\" is not a register", rtext));
		*line++ = ':';
		return OPREADER_PARTIAL_MATCH;
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
		READ_ERROR(("\"%s\" is not a register", rtext));
		*line = c;
		return OPREADER_PARTIAL_MATCH;
	}
	*line = c;

	/* To store this register pair, we _could_ just use the existing
	 * opdetail struct, however then we'd lose checking for whether this
	 * operand has consecutive/same-side/whatever registers. And I don't
	 * want to put them here, because I want the reading phase to _just_
	 * be parsing */
	out->u.dreg.reg1 = reg1;
	out->u.dreg.reg2 = reg2;

	return OPREADER_OK;
}

int
opread_constant(char *line, bfd_boolean print_error, struct read_operand *out)
{
	expressionS expr;

	tic64x_parse_expr(line, &expr);
	if (expr.X_op == O_illegal) {
		READ_ERROR(("Illegal constant expression"));
		return OPREADER_BAD;
	} else if (expr.X_op == O_absent) {
		READ_ERROR(("Expected constant expression"));
		return OPREADER_BAD;
	}

	/* Everything else is the job of the validator */
	memcpy(&out->u.constant.expr, &expr, sizeof(expr));

	return OPREADER_OK;
}

#undef READ_ERROR
#define NOT_VALID(x) if (print_error) as_bad x

bfd_boolean
opvalidate_memaccess(struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_op_template *templ,
			bfd_boolean gen_unitspec,
			struct unitspec *spec)
{
	struct opdetail_memaccess *detail;
	int field_sz, field_shift, max, mask;
	int32_t val;
	enum tic64x_operand_type offs_type;
	int8_t base_side, offs_side;
	bfd_boolean do_scale, opt_scale;

	detail = &in->u.mem;

	if (optype != tic64x_optxt_memaccess)
		as_fatal("Invalid operand type has made its way to memaccess "
			"validator");

	/* Gather some information on this template... */
	if (templ->operands[0] == tic64x_operand_rcoffset ||
		templ->operands[1] == tic64x_operand_rcoffset) {
		offs_type = tic64x_operand_rcoffset;
		field_sz = 5;
	} else if (templ->operands[0] == tic64x_operand_const15 ||
		templ->operands[1] == tic64x_operand_const15) {
		offs_type = tic64x_operand_const15;
		field_sz = 15;
	} else {
		as_fatal("memaccess operand with no corresponding offset field "
								"size");
	}

	field_shift = templ->flags & TIC64X_OP_MEMSZ_MASK;
	field_shift <<= TIC64X_OP_MEMSZ_SHIFT;

	if (templ->flags & TIC64X_OP_MEMACC_SBIT)
		opt_scale = TRUE;
	else
		opt_scale = FALSE;

	if (templ->flags & TIC64X_OP_CONST_SCALE)
		do_scale = TRUE;
	else
		do_scale = FALSE;

	base_side = (detail->base->num & TIC64X_REG_UNIT2) ? 1 : 0;

	if (detail->const_offs == FALSE)
		offs_side = (detail->offs.reg->num & TIC64X_REG_UNIT2) ? 1 : 0;
	else
		offs_side = -1;

	/* First of all, make some checks inre memrel15 instructions, which
	 * have extra restrictions in addition to the usual... */
	if (offs_type == tic64x_operand_const15) {
		if (detail->const_offs == FALSE) {
			NOT_VALID(("memrel15 instructions may only have "
							"constant offsets"));
			return TRUE;
		}

		if ((detail->base->num & 0x1F) != 15 &&
					(detail->base->num & 0x1F) != 14) {
			NOT_VALID(("memrel15 instructions may only take regs "
					"14 and 15 as the base address"));
			return TRUE;
		}

		if (spec->unit_num == 0) {
			NOT_VALID(("memrel15 instructions may only execute "
					"on side 2 of processor"));
			return TRUE;
		}

		if (detail->addrmode != (TIC64X_ADDRMODE_POS |
				TIC64X_ADDRMODE_PRE | TIC64X_ADDRMODE_OFFSET |
				TIC64X_ADDRMODE_NOMODIFY)) {
			NOT_VALID(("Invalid addressing mode for memrel15 "
							"instruction"));
			return TRUE;
		}
	}

	/* Now do some actual operand-specific checks */
	/* Does the base register sit on the correct side of the processor */
	if (spec->unit_num != base_side && spec->unit_num != -1) {
		NOT_VALID(("Base register must be on same side as "
						"execution unit"));
		return TRUE;
	}

	if (detail->const_offs == FALSE) {
		/* Check that offset register is on same side as base */
		if (base_side != offs_side) {
			NOT_VALID(("Offset and base registers must be on "
							"the same side"));
			return TRUE;
		}
	} else {
		/* Check constant fits in field. Rather intense. */
		if (detail->offs.expr.X_op == O_constant) {
			val = detail->offs.expr.X_add_number;
			if (detail->offs.expr.X_unsigned == 0 && val < 0) {
				NOT_VALID(("Negative constant as offset: use "
					"negative addressing mode instead"));
				return TRUE;
			}

			/* If the user encased the offset in [] brackets, then
			 * the constant they gave us was prescaled to fit in
			 * the field. Un-do that for testing. */
			if (detail->scale_input)
				val <<= field_shift;

			max = 1 << field_sz;
			if (do_scale)
				max <<= field_shift;

			if (val >= max && opt_scale) {
				/* It's too large - invoke optional scale */
				max <<= field_shift;
				do_scale = TRUE;
			}

			if (val >= max) {
				NOT_VALID(("Constant operand exceeds field "
								"size"));
				return TRUE;
			}

			/* Mkay, it fits in the field; but is it aligned? */
			mask = (1 << field_shift) - 1;
			if (do_scale && (val & mask)) {
				NOT_VALID(("Constant operand not sufficiently "
								"aligned"));
				return TRUE;
			}

			/* Hurrah, it's a valid constant */
		} else if (detail->offs.expr.X_op == O_symbol) {
			/* We're a symbol based expression... we can't be
			 * certain of what value we're going to get. So, ignore
			 * this for the moment, and apply it as a fixup when
			 * writing the operand */
		} else {
			NOT_VALID(("Offset expression neither a constant nor "
				"based on a symbol"));
			return TRUE;
		}
	}

	/* Can't think of other things to check */

	if (gen_unitspec == TRUE) {
		/* Only constraint we impose is what side we execute on, given
		 * where the base register is. Unless we're memrel15 */
		if (offs_type == tic64x_operand_const15) {
			spec->unit_num = 1;
		 } else {
			spec->unit_num = base_side;
		}
	}

	return FALSE;
}

bfd_boolean
opvalidate_register(struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_op_template *templ,
			bfd_boolean gen_unitspec,
			struct unitspec *spec)
{
	int flag;
	int8_t reg_side;
	bfd_boolean uses_xpath, uses_dpath;

	uses_xpath = FALSE;
	uses_dpath = FALSE;
	reg_side = (in->u.reg.base->num & TIC64X_REG_UNIT2) ? 1 : 0;

	/* Registers: this is nice and simple to cope with. Destination regs
	 * have to be on the execution side, the others too unless they can
	 * xpath. Unfortunately a hack is required though: this is how we
	 * determine what datapath a load/store instruction takes, by where
	 * the destination register lies */

	/* So what can we do? Check the side of each register */
	switch (optype) {
	case tic64x_optxt_srcreg1:
		flag = TIC64X_OP_XPATH_SRC1;
		break;
	case tic64x_optxt_srcreg2:
		flag = TIC64X_OP_XPATH_SRC2;
		break;
	case tic64x_optxt_dstreg:
		flag = 0; /* No condition where dst reg can be on other side */
		break;
	default:
		as_fatal("Invalid operand type has made its way into register "
			"validator");
	}

	if (reg_side != spec->unit_num && spec->unit_num != -1) {
		/* Can we xpath? */
		if (!(templ->flags & flag)) {
			NOT_VALID(("Register %C%d on wrong side of processor",
						(reg_side) ? 'A' : 'B',
						in->u.reg.base->num & 0x1F));
			return TRUE;
		}
	}

	if (templ->flags & flag)
		uses_xpath = TRUE;

	/* OK, what else? If it's a dst reg and we're a memaccess, that means
	 * we're going to end up using the data path on this side */
	if (templ->flags & TIC64X_OP_MEMACCESS && optype ==tic64x_optxt_dstreg){
		uses_dpath = TRUE;

		if (spec->mem_path != -1 && spec->mem_path != reg_side) {
			NOT_VALID(("Memory datapath on wrong side of processor"
									));
			return TRUE;
		}
	}

	/* We got this far, if we don't need to update constraints, return now*/
	if (gen_unitspec == FALSE)
		return FALSE;

	/* Right. So, what constraints does this place? If the register can't
	 * use the xpath then we're locked to that side (this is the case for
	 * dstreg and srcregs that don't have the xpath flag). */
	if (uses_xpath == FALSE && spec->unit_num == -1)
		spec->unit_num = reg_side;

	/* Alternately if we *could* use the xpath, but don't, say so */
	if (uses_xpath == TRUE && spec->unit_num == reg_side)
		spec->uses_xpath = 0;

	/* If we're tied to a side that differs from the reg side and we got
	 * this far, the xpath _has_ to be used */
	if (uses_xpath == TRUE && spec->unit_num != -1 &&
				 spec->unit_num != reg_side)
		spec->uses_xpath = 1;

	/* Finally, did we end up finding that we also decide the datapath? */
	if (uses_dpath == TRUE)
		spec->mem_path = reg_side;

	/* I can't think of anything else. Erk. */

	return FALSE;
}

bfd_boolean
opvalidate_double_register(struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_op_template *templ,
			bfd_boolean gen_unitspec,
			struct unitspec *spec)
{
	const struct tic64x_register *reg1, *reg2;
	int8_t reg1_side, reg2_side, reg1_num, reg2_num;
	bfd_boolean is_memacc;

	if (optype != tic64x_optxt_dwdst && optype != tic64x_optxt_dwsrc &&
					optype != tic64x_optxt_dwsrc2)
		as_fatal("Non-dword operand has made its way to dword "
							"validator");

	reg1 = in->u.dreg.reg1;
	reg2 = in->u.dreg.reg2;
	reg1_num = reg1->num & 0x1F;
	reg2_num = reg2->num & 0x1F;
	reg1_side = (reg1->num & TIC64X_REG_UNIT2) ? 1 : 0;
	reg2_side = (reg2->num & TIC64X_REG_UNIT2) ? 1 : 0;
	is_memacc = (templ->flags & TIC64X_OP_MEMACCESS) ? TRUE : FALSE;

	/* Double register restrictions: we can't use the xpath, so they're
	 * always on the same side. They *can* also be the target of a load
	 * or store, so check for data path constraints too. First though, some
	 * more obvious issues */
	if (reg1_num != reg2_num + 1) {
		NOT_VALID(("Double register pairs must be sequential, "
							"decreasing"));
		return TRUE;
	}

	if (reg1_side != reg2_side) {
		NOT_VALID(("Double register pairs must both be on same side"));
		return TRUE;
	}

	if (!(reg1_num & 1)) {
		NOT_VALID(("Double register pair must start on odd register"));
		return TRUE;
	}

	/* So the registers specified are ok, how about side issues? Can't
	 * do dregs over xpath, so they must only ever be on the same side
	 * as the execution unit. Except in the case where it's a memory
	 * load/store, in which case it can be on either side, but we need
	 * to check the data path specifier */
	if (!is_memacc && spec->unit_num != -1 && spec->unit_num != reg1_side) {
		NOT_VALID(("Double register pair on wrong side of processor"));
		return TRUE;
	}

	/* We can also load and store double registers */
	if (is_memacc && spec->mem_path != -1 && spec->mem_path != reg1_side) {
		NOT_VALID(("Double-reg data path on wrong side of processor"));
		return TRUE;
	}

	/* So its good; now extra specifiers? */
	if (gen_unitspec == FALSE)
		return 0;

	if (!is_memacc && spec->unit_num == -1)
		spec->unit_num = reg1_side;

	if (is_memacc && spec->mem_path == -1)
		spec->mem_path = reg1_side;

	return FALSE;
}

static const enum tic64x_operand_type constant_types[] = {
	tic64x_operand_const5, tic64x_operand_const5p2,
	tic64x_operand_const21, tic64x_operand_const16,
	tic64x_operand_const15, tic64x_operand_const12,
	tic64x_operand_const10, tic64x_operand_const7,
	tic64x_operand_const4, tic64x_operand_invalid
};

bfd_boolean
opvalidate_constant (struct read_operand *in, bfd_boolean print_error,
			enum tic64x_text_operand optype,
			struct tic64x_op_template *templ,
			bfd_boolean gen_unitspec ATTRIBUTE_UNUSED,
			struct unitspec *spec ATTRIBUTE_UNUSED)
{
	expressionS *e;
	uint32_t unum;
	int32_t snum;
	int field_sz, i, max, mask;
	enum tic64x_operand_type type;

	if (optype != tic64x_optxt_uconstant && optype !=tic64x_optxt_sconstant)
		as_fatal("Non-constant reached constant validator routine");

	e = &in->u.constant.expr;
	snum = e->X_add_number;
	unum = e->X_add_number;

	for (i = 0; constant_types[i] != tic64x_operand_invalid; i++) {
		type = constant_types[i];
		if (type == templ->operands[0] || type == templ->operands[1])
			break;
	}

	if (type == tic64x_operand_invalid)
		as_fatal("Insn with constant operand, but no corresponding "
								"field");

	field_sz = tic64x_operand_positions[type].size;
	max = 1 << field_sz;

	/* There _are_ some instructions that shift their constant operands */
	if (templ->flags & TIC64X_OP_CONST_SCALE) {
		int tmp;

		tmp = templ->flags & TIC64X_OP_MEMSZ_MASK;
		tmp >>= TIC64X_OP_MEMSZ_SHIFT;

		max <<= tmp;
		mask = (1 << tmp) - 1;
	} else {
		mask = 0;
	}

	/* Some different tests depending on whether we expected a signed or
	 * unsigned constant */
	if (optype == tic64x_optxt_sconstant) {
		if (e->X_unsigned == 1 && snum < 0) {
			NOT_VALID(("Negative constant, expected unsigned"));
			return TRUE;
		}

		max >>= 1;
		if (snum < -max || snum >= max) {
			NOT_VALID(("Signed constant exceeds field size"));
			return TRUE;
		}
	} else {
		if (snum < -max || snum >= max) {
			NOT_VALID(("Unsigned constant exceeds field size"));
			return TRUE;
		}
	}

	/* Happily there's no munging of scale bits - however if we had a
	 * constant shift factored into the maximum number comparisons above,
	 * ensure that the alignment of the number is sufficient to fit into
	 * the field without dropping lower bits */
	if (unum & mask) {
		NOT_VALID(("Constant is not sufficiently aligned to fit in "
						"shifted const field"));
		return TRUE;
	}

	/* Mkay, I can't think of any other constraints. Happily, there are no
	 * constraints on units, sides, xpaths and suchlike imposed by
	 * constants */

	return FALSE;
}

void
opwrite_memaccess(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{
	struct opdetail_memaccess *mem;
	enum tic64x_operand_type type;
	uint32_t offs;

	if (optype != tic64x_optxt_memaccess)
		as_fatal("Non-memaccess operand made its way to opwriter");

	mem = &in->u.mem;

	/* Two kinds of memaccess... memrel15 and normal */
	if (insn->templ->operands[0] == tic64x_operand_rcoffset ||
		insn->templ->operands[1] == tic64x_operand_rcoffset)
		type = tic64x_operand_rcoffset;
	else if (insn->templ->operands[0] == tic64x_operand_const15 ||
		insn->templ->operands[1] == tic64x_operand_const15)
		type = tic64x_operand_const15;
	else
		as_fatal("Memaccess operand without corresponding field being"
			" written");

	/* So, the main emit opcode routine will have set the generic fields,
	 * including 'y', so lets start with anything common to all memaccess.
	 * Oh, there aren't. Except the destination, but that's handled like
	 * all other registers */
	if (type == tic64x_operand_rcoffset) {
		tic64x_set_operand(&insn->opcode, tic64x_operand_addrmode,
							mem->addrmode);
		tic64x_set_operand(&insn->opcode, tic64x_operand_basereg,
						mem->base->num & 0x1F);
		if (mem->const_offs) {
			/* Uuurgh, not this again */
			unsigned int tmp;

			tmp = insn->templ->flags & TIC64X_OP_MEMSZ_MASK;
			tmp >>= TIC64X_OP_MEMSZ_SHIFT;
			offs = mem->offs.expr.X_add_number;

			if (insn->templ->flags & TIC64X_OP_CONST_SCALE)
				offs >>= tmp;

			/* Do we have to twiddle with the optional scale bit? */
			if (insn->templ->flags & TIC64X_OP_MEMACC_SBIT) {
				if (offs >= (unsigned int)(32 << tmp)) {
					offs >>= tmp;
					tic64x_set_operand(&insn->opcode,
						tic64x_operand_scale, 1);
				} else {
					tic64x_set_operand(&insn->opcode,
						tic64x_operand_scale, 0);
				}
			}

			tic64x_set_operand(&insn->opcode, type, offs);
		} else {
			/* We use a register instead */
			tic64x_set_operand(&insn->opcode, type,
					mem->offs.reg->num & 0x1F);
		}

		/* Ho-kay, I think that's it */
	} else {
		/* For memrel15 we have some different situations: the base
		 * register is restricted and the y bit specifies whether it's
		 * B14 or B15. Validator must have checked that it's one or
		 * the other */

		if (mem->base->num == TIC64X_REG_UNIT2 + 14)
			tic64x_set_operand(&insn->opcode, tic64x_operand_y, 0);
		else
			tic64x_set_operand(&insn->opcode, tic64x_operand_y, 1);

		/* Validator will reject non-const offsets */
		if (mem->offs.expr.X_op == O_constant) {
			offs = mem->offs.expr.X_add_number;
			tic64x_set_operand(&insn->opcode, type, offs);
		} else if (mem->offs.expr.X_op == O_symbol) {
			fixS *fix;
			int rtype;

			/* Create a fixup for this field - code largely stolen
			 * from the constant writer handler */
			if (insn->templ->flags & TIC64X_OP_CONST_PCREL)
				as_fatal("Instruction with PCREL flag, in "
					"operand writer for memrel15");

			rtype = type_to_rtype(insn, type);
			fix = fix_new_exp(insn->output_frag,
						insn->output_frag_offs,
						4, &mem->offs.expr, FALSE,
						rtype);
		} else {
			as_fatal("Non constant/symbol offset for memrel15 "
								"operand");
		}

		/* And that seems to be it */
	}

	return;
}

void
opwrite_register(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{
	enum tic64x_operand_type type;
	int reg;

	/* There is precisely nothing interesting about writing these back */
	switch (optype) {
	case tic64x_optxt_srcreg1:
		type = tic64x_operand_srcreg1;
		break;
	case tic64x_optxt_srcreg2:
		type = tic64x_operand_srcreg2;
		break;
	case tic64x_optxt_dstreg:
		type = tic64x_operand_dstreg;
		break;
	default:
		as_fatal("Non-register operand has reached register writer");
	}

	reg = in->u.reg.base->num & 0x1F;
	tic64x_set_operand(&insn->opcode, type, reg);
	return;
}

void
opwrite_double_register(struct read_operand *in,
			enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{
	enum tic64x_operand_type type;
	int reg;

	/* Double register writer is slightly nontrivial - there are two forms
	 * they can be written as, 4 bits identifying the higher 4 bits of the
	 * register address, or 5 bits specifying the lower register address */

	/* Second reg should be the even numbered address */
	reg = in->u.dreg.reg2->num & 0x1F;

	switch (optype) {
	case tic64x_optxt_dwdst:
		if (insn->templ->operands[0] == tic64x_operand_dwdst5 ||
			insn->templ->operands[0] == tic64x_operand_dwdst5) {
			type = tic64x_operand_dwdst5;
		} else if (insn->templ->operands[0] == tic64x_operand_dwdst4 ||
			insn->templ->operands[0] == tic64x_operand_dwdst4) {
			type = tic64x_operand_dwdst4;
			/* Shift right 1 bit: we address with top four bits of
			 * the register pair */
			reg >>= 1;
		}
		break;

	case tic64x_optxt_dwsrc:
		type = tic64x_operand_dwsrc;
		break;

	case tic64x_optxt_dwsrc2:
		type = tic64x_operand_srcreg1;
		break;

	default:
		as_fatal("Non-dword operand reached dword writer");
	}

	tic64x_set_operand(&insn->opcode, type, reg);
	return;
}

void
opwrite_constant(struct read_operand *in, enum tic64x_text_operand optype,
			struct tic64x_insn *insn)
{
	expressionS *e = &in->u.constant.expr;
	enum tic64x_operand_type type;
	int i, tmp;
	int32_t val;

	if (optype != tic64x_optxt_sconstant && optype !=tic64x_optxt_uconstant)
		as_fatal("Non-constant operand reached constant writer");

	/* So the variety of oddities we can experience with constants - the
	 * usual set of different field sizes that can be extracted from the
	 * operand type list and scaling. There is no optional scaling. Also,
	 * normally we only have one constant in an instruction, the exception
	 * is nops. Those shouldn't be scaled, even if the instruction does
	 * scaling (see: bnop) */

	/* So first, find operand type */
	for (i = 0; type != tic64x_operand_invalid; i++) {
		type = constant_types[i];
		if (type == insn->templ->operands[0] ||
					type == insn->templ->operands[1])
			break;
	}

	if (type == tic64x_operand_invalid)
		as_fatal("Instruction with constant text operand, but no "
				"corresponding constant field operand");

	if (e->X_op == O_constant) {

		/* Do we scale? */
		val = e->X_add_number;
		if (insn->templ->flags & TIC64X_OP_CONST_SCALE) {
			tmp = insn->templ->flags & TIC64X_OP_MEMSZ_MASK;
			tmp >>= TIC64X_OP_MEMSZ_SHIFT;
			val >>= tmp;
		}

		tic64x_set_operand(&insn->opcode, type, val);
	} else if (e->X_op == O_symbol) {
		fixS *fix;
		bfd_boolean pcrel;
		int rtype;

		/* Emit a fixup - this symbol may jump around and will need
		 * relaxing later */

		pcrel = (insn->templ->flags & TIC64X_OP_CONST_PCREL)
				? TRUE : FALSE;
		rtype = type_to_rtype(insn, type);
		fix = fix_new_exp(insn->output_frag, insn->output_frag_offs, 4,
							e, pcrel, rtype);
		fix->fx_pcrel_adjust = insn->output_pcoffs;
	} else {
		as_fatal("Invalid expression type in constant writer");
	}

	/* And I believe that's it */
	return;
}
