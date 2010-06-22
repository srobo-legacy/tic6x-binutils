/* Assembler for TMS320C64X class of processors
 * Copyright 2010 Jeremy Morse <jeremy.morse@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
 * MA 02110-1301, USA
 */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/internal.h"
#include "coff/tic64x.h"
#include "libcoff.h"
#include "libdoff.h"

#undef F_LSYMS
#define F_LSYMS		F_LSYMS_TICOFF
/* XXX - dictated by coff/ti.h, but ti's docs say F_LSYMS remains 0x8 */

void tic64x_rtype2howto(arelent *internal, struct internal_reloc *dst);
bfd_boolean tic64x_set_arch_mach(bfd *b, enum bfd_architecture a,
		unsigned long);
bfd_boolean tic64x_set_section_contents(bfd *b, sec_ptr section,
		const PTR location, file_ptr offset, bfd_size_type bytes);

/* Customize + include coffcode.h */
#define BADMAG(x) COFF2_BADMAG(x)
#ifndef bfd_pe_print_pdata
#define bfd_pe_print_pdata	NULL
#endif

#define RTYPE2HOWTO(internal, reloc) tic64x_rtype2howto(internal, reloc);

reloc_howto_type *tic64x_coff_reloc_type_lookup(bfd*, bfd_reloc_code_real_type);
reloc_howto_type *tic64x_coff_reloc_name_lookup (bfd *abfd, const char *name);
#define coff_bfd_reloc_type_lookup tic64x_coff_reloc_type_lookup
#define coff_bfd_reloc_name_lookup tic64x_coff_reloc_name_lookup

#define COFF_LONG_SECTION_NAMES 1

#include "coffcode.h"

/* FIXME: coffcode defines ticoff{0,1}_swap_table, however we don't use
 * this for any target vectors. So gcc -Wextra complains that we're not using
 * a static function. To solve this, make some dummy pointers to those values.
 * Real fix should be switches to specify which tables we want to pull in,
 * but I don't understand the binutils situation for ticoff */

bfd_coff_backend_data *dummy_backend_0 = &ticoff0_swap_table;
bfd_coff_backend_data *dummy_backend_1 = &ticoff1_swap_table;

bfd_boolean
tic64x_set_arch_mach(bfd *b, enum bfd_architecture arch, unsigned long machine)
{

	if (arch == bfd_arch_unknown)
		arch = bfd_arch_tic64x;
	else if (arch != bfd_arch_tic64x)
		return FALSE;

	return bfd_default_set_arch_mach(b, arch, machine);
}

bfd_boolean
tic64x_set_section_contents(bfd *b, sec_ptr section, const PTR location,
		file_ptr offset, bfd_size_type bytes)
{

	return coff_set_section_contents(b, section, location, offset, bytes);
}

static bfd_reloc_status_type
tic64x_pcr_reloc_special_func(bfd *abfd, arelent *rel, struct bfd_symbol *sym,
				void *data, asection *input_section,
				bfd *output_bfd ATTRIBUTE_UNUSED,
				char **error_msg ATTRIBUTE_UNUSED)
{
	long val, in_insn_val;
	bfd_reloc_status_type ret;

	/* So - doffs keep addends in the instruction field. We can convince
	 * bfd to take this into account, however it won't round the offset to
	 * be relative to the fetch packet address. So, we twiddle with that
	 * explicitly here instead */

	if (bfd_is_und_section(sym->section)) {
		/* Store here the offset between the relocations fetch packet
		 * address and the start of the section - that way if this
		 * ends up being fed to the dspbridge loader it'll correctly
		 * adjust the offset to something being linked in */
		val = input_section->output_offset + rel->address;
		val &= ~0x1F;
		val = -val;

		ret = bfd_reloc_ok;
		goto calculated; /* Har */
	}

	/* Address of instruction */
	val = input_section->output_offset + rel->address + input_section->vma;
	/* Round that down to the fetch packet address */
	val &= ~0x1F;
	/* Offset from there to symbol */
	val = (sym->value + sym->section->output_offset + sym->section->vma)
			- val;

	/* Now we need to write this value into the instruction. This portion
	 * largely copied from bfd_perform_relocation */
	if (rel->howto->complain_on_overflow != complain_overflow_dont)
	ret = bfd_check_overflow (rel->howto->complain_on_overflow,
				rel->howto->bitsize, rel->howto->rightshift,
				bfd_arch_bits_per_address (abfd), val);
	if (ret != bfd_reloc_ok)
		return ret;

calculated:

	val >>= rel->howto->rightshift;
	val <<= rel->howto->bitpos;

	/* Seeing how we have all the information we need available to us,
	 * instead of adding to what already exists in the instruction addend
	 * and thus causing pain, just update with our new value */
	in_insn_val = bfd_get_32(abfd, data + rel->address);
	long x = (in_insn_val & ~rel->howto->dst_mask);
	in_insn_val = rel->howto->src_mask & val;
	in_insn_val |= x;
	bfd_put_32(abfd, in_insn_val, data + rel->address);

	/* Adjust address of where this relocation is - because we're
	 * overriding what perform_relocation does, we have to do this
	 * manually here */
	rel->address += input_section->output_offset;

	return ret;
}

reloc_howto_type tic64x_howto_table[] = {
	HOWTO(R_C60BASE, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
		NULL, "RBASE", FALSE, 0xFFFFFFFF, 0xFFFFFFFF, FALSE),

	HOWTO(R_C60DIR15, 0, 2, 15, FALSE, 8, complain_overflow_bitfield,
		NULL, "RDIR15", FALSE, 0x7FFF00, 0x7FFF00, FALSE),

	/* 21 bits pcrels: must be branch, shr by 2, pcrel_offset stored in
	 * offset slot... */
	HOWTO(R_C60PCR21, 2, 2, 21, TRUE, 7, complain_overflow_bitfield,
		tic64x_pcr_reloc_special_func, "RPCR21", TRUE, 0xFFFFF80,
		0xFFFFF80, TRUE),

	/* similar */
	HOWTO(R_C60PCR10, 2, 2, 10, TRUE, 13, complain_overflow_bitfield,
		tic64x_pcr_reloc_special_func, "RPCR10", TRUE, 0x7FE000,
		0x7FE000, TRUE),

	HOWTO(R_C60LO16, 0, 2, 16, FALSE, 7, complain_overflow_bitfield,
		NULL, "RLO16", TRUE, 0x7FFF80, 0x7FFF80, FALSE),

	HOWTO(R_C60HI16, 16, 2, 16, FALSE, 7, complain_overflow_bitfield,
		NULL, "RHI16", TRUE, 0x7FFF80, 0x7FFF80, FALSE),

/* I don't know what this section offset is supposed to be... */

	HOWTO(R_C60S16, 0, 2, 16, FALSE, 7, complain_overflow_bitfield,
		NULL, "RS16", TRUE, 0x7FFF80, 0x7FFF80, FALSE),

	HOWTO(R_C60PCR7, 2, 2, 7, TRUE, 16, complain_overflow_bitfield,
		tic64x_pcr_reloc_special_func, "RPCR7", TRUE, 0x7F0000,
		0x7F0000, TRUE),

	HOWTO(R_C60PCR12, 2, 2, 12, TRUE, 16, complain_overflow_bitfield,
		tic64x_pcr_reloc_special_func, "RPCR12", TRUE, 0xFFF0000,
		0xFFF0000, TRUE),

	HOWTO(R_RELBYTE, 0, 2, 8, FALSE, 0, complain_overflow_bitfield,
		NULL, "RELBYTE", FALSE, 0xFF, 0xFF, FALSE),

	HOWTO(R_RELWORD, 0, 2, 16, FALSE, 0, complain_overflow_bitfield,
		NULL, "RELWORD", FALSE, 0xFFFF, 0xFFFF, FALSE),

	HOWTO(R_RELLONG, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
		NULL, "RELLONG", FALSE, 0xFFFFFFFF, 0xFFFFFFFF, FALSE),

	HOWTO(RE_ADD, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_ADD", FALSE, 0, 0, FALSE),

	HOWTO(RE_SUB, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_USB", FALSE, 0, 0, FALSE),

	HOWTO(RE_NEG, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_NEG", FALSE, 0, 0, FALSE),

	HOWTO(RE_MPY, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_MPY", FALSE, 0, 0, FALSE),

	HOWTO(RE_DIV, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_DIV", FALSE, 0, 0, FALSE),

	HOWTO(RE_MOD, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_MOD", FALSE, 0, 0, FALSE),

	HOWTO(RE_SR, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_SR", FALSE, 0, 0, FALSE),

	HOWTO(RE_ASR, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_ASR", FALSE, 0, 0, FALSE),

	HOWTO(RE_SL, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_SL", FALSE, 0, 0, FALSE),

	HOWTO(RE_AND, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_AND", FALSE, 0, 0, FALSE),

	HOWTO(RE_OR, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_OR", FALSE, 0, 0, FALSE),

	HOWTO(RE_XOR, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_XOR", FALSE, 0, 0, FALSE),

	HOWTO(RE_NOTB, 0, 0, 0, FALSE, 0, complain_overflow_dont, ti_reloc_fail,
		"RE_NOTB", FALSE, 0, 0, FALSE),

	HOWTO(RE_ULDFLD, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_ULDFLD", FALSE, 0, 0, FALSE),

	HOWTO(RE_SLDFLD, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_SLDFLD", FALSE, 0, 0, FALSE),

	HOWTO(RE_USTFLD, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_USTFLD", FALSE, 0, 0, FALSE),

	HOWTO(RE_SSTFLD, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_SSTFLD", FALSE, 0, 0, FALSE),

	HOWTO(RE_PUSH, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_PUSH", FALSE, 0, 0, FALSE),

	HOWTO(RE_PUSHSK, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_PUSHSK", FALSE, 0, 0, FALSE),

	HOWTO(RE_PUSHUK, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_PUSHUK", FALSE, 0, 0, FALSE),

	HOWTO(RE_PUSHPC, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_PUSHPC", FALSE, 0, 0, FALSE),

	HOWTO(RE_DUP, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_DUP", FALSE, 0, 0, FALSE),

	HOWTO(RE_XSTFLD, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_XSTFLD", FALSE, 0, 0, FALSE),

	HOWTO(RE_PUSHSV, 0, 0, 0, FALSE, 0, complain_overflow_dont,
		ti_reloc_fail, "RE_PUSHSV", FALSE, 0, 0, FALSE)
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

void
tic64x_rtype2howto(arelent *internal, struct internal_reloc *dst)
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
