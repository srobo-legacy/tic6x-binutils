/* Insert copyright header here */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/internal.h"
#include "coff/tic64x.h"
#include "libcoff.h"

#undef F_LSYMS
#define F_LSYMS		F_LSYMS_TICOFF
/* XXX - dictated by coff/ti.h, but ti's docs say F_LSYMS remains 0x8 */

static void rtype2howto(arelent *internal, struct internal_reloc *dst);
static bfd_boolean tic64x_set_arch_mach(bfd *b, enum bfd_architecture a,
		unsigned long);
static bfd_boolean tic64x_set_section_contents(bfd *b, sec_ptr section,
		const PTR location, file_ptr offset, bfd_size_type bytes);

/* Customize + include coffcode.h */
#define BADMAG(x) COFF2_BADMAG(x)
#ifndef bfd_pe_print_pdata
#define bfd_pe_print_pdata	NULL
#endif

#define RTYPE2HOWTO(internal, reloc) rtype2howto(internal, reloc);

reloc_howto_type *tic64x_coff_reloc_type_lookup(bfd*, bfd_reloc_code_real_type);
reloc_howto_type *tic64x_coff_reloc_name_lookup (bfd *abfd, const char *name);
#define coff_bfd_reloc_type_lookup tic64x_coff_reloc_type_lookup
#define coff_bfd_reloc_name_lookup tic64x_coff_reloc_name_lookup

#include "coffcode.h"

/* FIXME: coffcode defines ticoff{0,1}_swap_table, however we don't use
 * this for any target vectors. So gcc -Wextra complains that we're not using
 * a static function. To solve this, make some dummy pointers to those values.
 * Real fix should be switches to specify which tables we want to pull in,
 * but I don't understand the binutils situation for ticoff */

bfd_coff_backend_data *dummy_backend_0 = &ticoff0_swap_table;
bfd_coff_backend_data *dummy_backend_1 = &ticoff1_swap_table;

static bfd_boolean
tic64x_set_arch_mach(bfd *b, enum bfd_architecture arch, unsigned long machine)
{

	if (arch == bfd_arch_unknown)
		arch = bfd_arch_tic64x;
	else if (arch != bfd_arch_tic64x)
		return FALSE;

	return bfd_default_set_arch_mach(b, arch, machine);
}

static bfd_boolean
tic64x_set_section_contents(bfd *b, sec_ptr section, const PTR location,
		file_ptr offset, bfd_size_type bytes)
{

	return coff_set_section_contents(b, section, location, offset, bytes);
}

reloc_howto_type tic64x_howto_table[] = {
	HOWTO(R_C60BASE, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
		NULL, "RBASE", FALSE, 0xFFFFFFFF, 0xFFFFFFFF, FALSE),

	HOWTO(R_C60DIR15, 0, 2, 15, FALSE, 8, complain_overflow_bitfield,
		NULL, "RDIR15", FALSE, 0x7FFF, 0x7FFF, FALSE),

	/* 21 bits pcrels: must be branch, shr by 2, pcrel_offset stored in
	 * offset slot... */
	HOWTO(R_C60PCR21, 2, 2, 21, TRUE, 7, complain_overflow_bitfield,
		NULL, "RPCR21", FALSE, 0x1FFFFF, 0x1FFFFF, TRUE),

	/* similar */
	HOWTO(R_C60PCR10, 2, 2, 10, TRUE, 13, complain_overflow_bitfield,
		NULL, "RPCR10", FALSE, 0x3FF, 0x3FF, TRUE),

	HOWTO(R_C60LO16, 0, 2, 16, FALSE, 7, complain_overflow_bitfield,
		NULL, "RLO16", FALSE, 0xFFFF, 0xFFFF, FALSE),

	HOWTO(R_C60HI16, 16, 2, 16, FALSE, 7, complain_overflow_bitfield,
		NULL, "RHI16", FALSE, 0xFFFF0000, 0xFFFF, FALSE),

/* I don't know what this section offset is supposed to be... */

	HOWTO(R_C60S16, 0, 2, 16, FALSE, 7, complain_overflow_bitfield,
		NULL, "RS16", FALSE, 0xFFFF, 0xFFFF, FALSE),

	HOWTO(R_C60PCR7, 2, 2, 7, TRUE, 16, complain_overflow_bitfield,
		NULL, "RPCR7", FALSE, 0x7F, 0x7F, TRUE),

	HOWTO(R_C60PCR12, 2, 2, 12, TRUE, 16, complain_overflow_bitfield,
		NULL, "RPCR12", FALSE, 0xFFF, 0xFFF, TRUE),

	HOWTO(R_RELBYTE, 0, 2, 8, FALSE, 0, complain_overflow_bitfield,
		NULL, "RELBYTE", FALSE, 0xFF, 0xFF, FALSE),

	HOWTO(R_RELWORD, 0, 2, 16, FALSE, 0, complain_overflow_bitfield,
		NULL, "RELWORD", FALSE, 0xFFFF, 0xFFFF, FALSE),

	HOWTO(R_RELLONG, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
		NULL, "RELLONG", FALSE, 0xFFFFFFFF, 0xFFFFFFFF, FALSE)
};

reloc_howto_type *
tic64x_coff_reloc_type_lookup(bfd *abfd ATTRIBUTE_UNUSED,
				bfd_reloc_code_real_type code)
{

	switch (code) {
	case BFD_RELOC_TIC64X_BASE:
		return &tic64x_howto_table[0];
	case BFD_RELOC_TIC64X_DIR15:
		return &tic64x_howto_table[1];
	case BFD_RELOC_TIC64X_PCR21:
		return &tic64x_howto_table[2];
	case BFD_RELOC_TIC64X_PCR10:
		return &tic64x_howto_table[3];
	case BFD_RELOC_TIC64X_LO16:
		return &tic64x_howto_table[4];
	case BFD_RELOC_TIC64X_HI16:
		return &tic64x_howto_table[5];
	case BFD_RELOC_TIC64X_S16:
		return &tic64x_howto_table[6];
	case BFD_RELOC_TIC64X_PCR7:
		return &tic64x_howto_table[7];
	case BFD_RELOC_TIC64X_PCR12:
		return &tic64x_howto_table[8];
	case BFD_RELOC_TIC64X_BYTE:
		return &tic64x_howto_table[9];
	case BFD_RELOC_TIC64X_WORD:
		return &tic64x_howto_table[10];
	case BFD_RELOC_TIC64X_LONG:
		return &tic64x_howto_table[11];
	case BFD_RELOC_TIC64X_SECT:
	default:
		return NULL;
	}
}

static void
rtype2howto(arelent *internal, struct internal_reloc *dst)
{
	unsigned int i;

	for (i = 0; i < sizeof (tic64x_howto_table) /
			sizeof (tic64x_howto_table[0]);
						i++) {
		if (tic64x_howto_table[i].type == dst->r_type) {
			internal->howto = &tic64x_howto_table[i];
			return;
		}
	}

	(*_bfd_error_handler) (_("Unrecognized reloc type 0x%x"),
						(unsigned int) dst->r_type);
	internal->howto = NULL;
	return;
}

reloc_howto_type *
tic64x_coff_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED, const char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(tic64x_howto_table) /
				sizeof(tic64x_howto_table[0]); i++)
		if (!strcmp(tic64x_howto_table[i].name, name))
			return &tic64x_howto_table[i];

	return NULL;
}


const bfd_target tic64x_coff2_vec =
{
	"coff2-c64x",				/* Name */
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
	bfd_getl32, bfd_getl_signed_32, bfd_putl32,
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
	BFD_JUMP_TABLE_WRITE(tic64x),
	BFD_JUMP_TABLE_LINK(coff),
	BFD_JUMP_TABLE_DYNAMIC(_bfd_nodynamic),
	NULL,
	&ticoff2_swap_table
};

const bfd_target tic64x_doff_vec =
{
	"doff-c64x",
	bfd_target_doff_flavour,
	BFD_ENDIAN_LITTLE,
	BFD_ENDIAN_LITTLE,	/* Supports big too; no time right now */
	(HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS
		| DYNAMIC | WP_TEXT),
	(SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY | SEC_CODE | SEC_DATA
		| SEC_HAS_CONTENTS | SEC_DEBUGGING | SEC_STRINGS | SEC_MERGE),
	'_',
	'/',
	15,
	bfd_getl64, bfd_getl_signed_64, bfd_putl64,
	bfd_getl32, bfd_getl_signed_32, bfd_putl32,
	bfd_getl16, bfd_getl_signed_16, bfd_putl16,
	bfd_getl64, bfd_getl_signed_64, bfd_putl64,
	bfd_getl32, bfd_getl_signed_32, bfd_putl32,
	bfd_getl16, bfd_getl_signed_16, bfd_putl16,
	
	{
		_bfd_dummy_target,
		doff_object_p,
		bfd_generic_archive_p,
		_bfd_dummy_target
	},
	{
		bfd_false,
		doff_mkobject,
		_bfd_generic_mkarchive,
		bfd_false
	},
	{
		bfd_false,
		doff_write_object_contents,
		_bfd_write_archive_contents,
		bfd_false
	},
	BFD_JUMP_TABLE_GENERIC(doff),
	BFD_JUMP_TABLE_COPY(doff),
	BFD_JUMP_TABLE_CORE(_bfd_nocore),
	BFD_JUMP_TABLE_ARCHIVE(_bfd_archive_doff),
	BFD_JUMP_TABLE_SYMBOLS(doff),
	BFD_JUMP_TABLE_RELOCS(doff),
	BFD_JUMP_TABLE_WRITE(tic64x_doff),
	BFD_JUMP_TABLE_LINK(doff),
	BFD_JUMP_TABLE_DYNAMIC(_bfd_nodynamic),
	NULL,
	doff_backend_data
};
