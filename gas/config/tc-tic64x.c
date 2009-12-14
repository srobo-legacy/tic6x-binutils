/* Copyright blah */

#include <limits.h>
#include "as.h"
#include "opcode/tic64x.h"
#include "obj-coff.h"

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

const pseudo_typeS md_pseudo_table[] =
{
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
md_show_usage(FILE *stream)
{

	return;
}

void
md_begin()
{

	fprintf(stderr, "md begin called");
	return;
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

void
md_assemble(char *line)
{

	fprintf(stderr, "Congratulations! You've got as far as assembling "
			"lines, now you just need to implement the assembling "
			"part\n");
	UNUSED(line);
	exit(1);
}
