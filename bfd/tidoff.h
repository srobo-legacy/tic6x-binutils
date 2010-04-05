/* Prototypes for functions we'll be using */
#include "libdoff.h"

#define coff_swap_sym_out	doff_swap_sym_out
#define coff_swap_reloc_out	doff_swap_reloc_out
#define coff_swap_filehdr_out	doff_swap_filehdr_out
#define coff_swap_aouthdr_out	doff_swap_aouthdr_out
#define coff_swap_scnhdr_out	doff_swap_scnhdr_out

#define coff_swap_sym_in	doff_swap_sym_in
#define coff_swap_filehdr_in	doff_swap_filehdr_in
#define coff_swap_aouthdr_in	doff_swap_aouthdr_in
#define coff_swap_scnhdr_in	doff_swap_scnhdr_in
#define coff_swap_reloc_in	doff_swap_reloc_in

#define bfd_pe_print_pdata	NULL

#define BADMAG(x) ((x).f_magic != DOFF_MAGIC)
#define TIDOFF

#define NO_COFF_RELOCS
#define NO_COFF_FILEHDR
#define NO_COFF_SYMBOLS
#define NO_COFF_AOUTHDR
#define NO_COFF_SCNHDR
#include "coffcode.h"

static bfd_coff_backend_data tidoff_swap_table =
{
	coff_swap_aux_in, doff_swap_sym_in, coff_swap_lineno_in,
	coff_swap_aux_out, doff_swap_sym_out, coff_swap_lineno_out,
	doff_swap_reloc_out, doff_swap_filehdr_out, doff_swap_aouthdr_out,
	doff_swap_scnhdr_out,

	FILHSZ, AOUTSZ, SCNHSZ, SYMESZ, AUXESZ, RELSZ, LINESZ, FILNMLEN,
	/* FIXME - filename lengths? */

	TRUE,	/* We always have long filenames */
	TRUE,	/* And section names */
	NULL,	/* set long section names? */
	12,	/* Default alignment power */
	FALSE,	/* Force symnames in strings */
	0,	/* debug string prefix len. ??? */
	doff_swap_filehdr_in, doff_swap_aouthdr_in, doff_swap_scnhdr_in,
	doff_swap_reloc_in, doff_bad_format_hook, coff_set_arch_mach_hook,
	coff_mkobject_hook, styp_to_sec_flags, coff_set_alignment_hook,
	coff_slurp_symbol_table, symname_in_debug_hook,
	coff_pointerize_aux_hook, coff_print_aux, coff_reloc16_extra_cases,
	coff_reloc16_estimate, coff_classify_symbol,
	coff_compute_section_file_positions, coff_start_final_link,
	coff_relocate_section, coff_rtype_to_howto, coff_adjust_symndx,
	coff_link_add_one_symbol, coff_link_output_has_begun,
	coff_final_link_postscript, NULL
};
