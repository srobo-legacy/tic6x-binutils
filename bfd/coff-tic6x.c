/* Insert copyright header here */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/tic6x.h"
#include "coff/internal.h"
#include "libcoff.h"

#undef F_LSYMS
#define F_LSYMS		F_LSYMS_TICOFF
/* XXX - dictated by coff/ti.h, but ti's docs say F_LSYMS remains 0x8 */

/* Customize + include coffcode.h */
#define BADMAG(x) COFF2_BADMAG(x)
#ifndef bfd_pe_print_pdata
#define bfd_pe_print_pdata	NULL
#endif
#include "coffcode.h"

const bfd_target tic6x_coff2_vec =
{
	"coff2-c6x",				/* Name */
	bfd_target_coff_flavour,
	BFD_ENDIAN_LITTLE,			/* data */
	BFD_ENDIAN_LITTLE,			/* header */
	(HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS
	 | HAS_LOCALS | WP_TEXT),
	(SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC),
	'_',					/* "symbol_leading_char" */
	'/',					/* archive "pad_char" */
	15,					/* max chars in archive header*/

	/* Functions to byte swap data. */
	bfd_getl64, bfd_getl_signed_64, bfd_putl64, /* data gets/sets */
	tic6x_getl32, tic6x_getl_signed_32, tic6x_putl32,
	bfd_getl16, bfd_getl_signed_16, bfd_putl16,
	bfd_getl64, bfd_getl_signed_64, bfd_putl64, /* header gets/sets */
	bfd_getl32, bfd_getl_signed_32, bfd_putl32,
	bfd_getl16, bfd_getl_signed_16, bfd_putl16,

	/* Routines for checking file format; looks like we pass through
	 * to generic coff stuff. Coppy tic54x */
	{
		_bfd_dummy_target,		/* bfd_unknown */
		coff_object_p,			/* bfd_object */
		bfd_generic_archive_p,		/* bfd_archive */
		_bfd_dummy_target		/* bfd_core (dump) */
	},
	/* Routines for setting file format. */
	{
		bfd_false,
		coff_mkobject,
		_bfd_generic_mkarchive,
		bfd_false
	},
	/* Flush/write contents out */
	{
		bfd_false,
		coff_write_object_contents,
		_bfd_write_archive_contents,
		bfd_false
	},

	BFD_JUMP_TABLE_GENERIC(coff),
	BFD_JUMP_TABLE_COPY(coff),
	BFD_JUMP_TABLE_CORE(_bfd_nocore),
	BFD_JUMP_TABLE_ARCHIVE(_bfd_archive_coff),
	BFD_JUMP_TABLE_SYMBOLS(coff),
	BFD_JUMP_TABLE_RELOCS(coff),
	BFD_JUMP_TABLE_WRITE(tic6x),
	BFD_JUMP_TABLE_LINK(coff),
	BFD_JUMP_TABLE_DYNAMIC(_bfd_nodynamic),
	NULL,
	&ticoff2_swap_table
};
