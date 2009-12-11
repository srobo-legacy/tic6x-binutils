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

	fprintf(stream, "TMS320C64x specific command line options: none\n");
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
