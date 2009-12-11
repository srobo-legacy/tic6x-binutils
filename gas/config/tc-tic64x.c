/* Copyright blah */

#include <limits.h>
#include "as.h"
#include "opcode/tic64x.h"
#include "obj-coff.h"

#define UNUSED(x) ((x) = (x))

const char comment_chars[] = ";";
const char line_comment_chars[] = ";*#";
const char line_separator_chars[] = "";

/* No options right now */
struct option md_longopts[] =
{
	{ NULL,		no_argument,	NULL,	0}
};

size_t md_longopts_size = sizeof(md_longopts);

const char *md_shortopts = "";

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
