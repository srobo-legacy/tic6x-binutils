/* Copyright blah */

#include <limits.h>
#include "as.h"
#include "safe-ctype.h"
#include "opcode/tic64x.h"
#include "obj-coff.h"
#include "struc-symbol.h"

#define UNUSED(x) ((x) = (x))

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

static void tic64x_asg(int x);
static void tic64x_sect(int x);
static void tic64x_fail(int x);

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
md_assemble(char *line)
{
	struct tic64x_op_template *tmpl;
	char *mnemonic, *ex_unit;
	int unit_num, mem_unit_num;
	char unit;

	/* Insert here - double bar detection */

	mem_unit_num = -1;
	unit_num = -1;
	unit = 0;

	mnemonic = line;
	while (!ISSPACE(*line) && !is_end_of_line[(int)*line])
		line++;
	*line++ = 0;

	/* Is this an instruction we've heard of? */
	tmpl = hash_find(tic64x_ops, mnemonic);
	if (!tmpl) {
		as_bad("Unrecognised mnemonic %s", mnemonic);
		return;
	}

	mnemonic = strdup(mnemonic);

	/* Next read execution unit */
	while (ISSPACE(*line) &&  !is_end_of_line[(int)*line])
		line++;
	ex_unit = line;

	while (!ISSPACE(*line) &&  !is_end_of_line[(int)*line])
		line++;
	*line++ = 0;

	/* Expect ".xn" where x is {D,L,S,M}, and n is {1,2}. Can be followed
	 * by 'T' specifier saying which memory data path is being used */
	if (*ex_unit++ != '.') {
		as_bad("Expected execution unit specifier after \"%s\"",
							mnemonic);
		free(mnemonic);
		return;
	}

	unit = *ex_unit++;
	if (unit != 'D' && unit != 'L' && unit != 'S' && unit != 'M') {
		as_bad("Unrecognised execution unit %C after \"%s\"",
							unit, mnemonic);
		free(mnemonic);
		return;
	}

	/* I will scream if someone says "what if it isn't ascii" */
	unit_num = *ex_unit++ - 0x30;
	if (unit_num != 1 && unit_num != 2) {
		as_bad("Bad execution unit number %d after \"%s\"",
							unit_num, mnemonic);
		free(mnemonic);
		return;
	}

	if (tmpl->flags & TIC64X_OP_MEMACCESS) {
		/* We should find either T1 or T2 at end of unit specifier,
		 * indicating which data path the loaded/stored data will
		 * travel through (only address needs to be in same unit) */
		if (*ex_unit++ != 'T') {
			as_bad("Expected memory datapath T1/T2 in unit "
				"specifier for \"%s\"", mnemonic);
			free(mnemonic);
			return;
		}

		mem_unit_num = *ex_unit++ - 0x30;
		if (mem_unit_num != 1 && mem_unit_num != 2) {
			as_bad("%d not a valid unit number for memory data path"
				" in \"%s\"", mem_unit_num, mnemonic);
			free(mnemonic);
			return;
		}
	}

	printf("Got mnemonic %s unit %C num %d memunit %d\n", mnemonic, unit,
							unit_num, mem_unit_num);
	free(mnemonic);
	return;
}
