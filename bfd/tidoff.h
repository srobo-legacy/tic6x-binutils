/* Prototypes for functions we'll be using */
unsigned int coff_swap_reloc_out(bfd *abfd, void *src, void *dst);
unsigned int coff_swap_scnhdr_out(bfd *abfd, void *src, void *dst);
unsigned int coff_swap_aouthdr_out(bfd *abfd, void *src, void *dst);
unsigned int coff_swap_aux_out(bfd *abfd, void *inp, int type, int in_class,
				int indx, int numaux, void *extp);

unsigned int coff_swap_reloc_in(bfd *abfd, void *src, void *dst);
unsigned int coff_swap_aux_in(bfd *abfd, void *ext1, int type, int in_class,
				int indx, int numaux, void *in1);

bfd_boolean doff_bad_format_hook(bfd *abfd, void *filehdr);
bfd_boolean doff_set_section_contents(bfd *abfd, asection *sect,
			const void *data, file_ptr offs, bfd_size_type size);

#define coff_SWAP_sym_in	NULL
#define coff_SWAP_sym_out	NULL
#define coff_SWAP_reloc_out	NULL
#define coff_SWAP_filehdr_out	NULL
#define coff_SWAP_aouthdr_out	NULL
#define coff_SWAP_scnhdr_out	NULL

#define coff_SWAP_filehdr_in	NULL
#define coff_SWAP_aouthdr_in	NULL
#define coff_SWAP_scnhdr_in	NULL
#define coff_SWAP_reloc_in	NULL

#define bfd_pe_print_pdata	NULL

#define BADMAG(x) ((x).f_magic != DOFF_MAGIC)

#define NO_COFF_RELOCS
#define NO_COFF_FILEHDR
#define NO_COFF_SYMBOLS
#define NO_COFF_AOUTHDR
#define NO_COFF_SCNHDR
#include "coffcode.h"

static bfd_coff_backend_data tidoff_swap_table =
{
	coff_SWAP_aux_in, coff_SWAP_sym_in, coff_SWAP_lineno_in,
	coff_SWAP_aux_out, coff_SWAP_sym_out, coff_SWAP_lineno_out,
	coff_SWAP_reloc_out, coff_SWAP_filehdr_out, coff_SWAP_aouthdr_out,
	coff_SWAP_scnhdr_out,

	FILHSZ, AOUTSZ, SCNHSZ, SYMESZ, AUXESZ, RELSZ, LINESZ, FILNMLEN,
	/* FIXME - filename lengths? */

	TRUE,	/* We always have long filenames */
	TRUE,	/* And section names */
	NULL,	/* set long section names? */
	12,	/* Default alignment power */
	FALSE,	/* Force symnames in strings */
	0,	/* debug string prefix len. ??? */
	coff_SWAP_filehdr_in, coff_SWAP_aouthdr_in, coff_SWAP_scnhdr_in,
	coff_SWAP_reloc_in, doff_bad_format_hook, coff_set_arch_mach_hook,
	coff_mkobject_hook, styp_to_sec_flags, coff_set_alignment_hook,
	coff_slurp_symbol_table, symname_in_debug_hook,
	coff_pointerize_aux_hook, coff_print_aux, coff_reloc16_extra_cases,
	coff_reloc16_estimate, coff_classify_symbol,
	coff_compute_section_file_positions, coff_start_final_link,
	coff_relocate_section, coff_rtype_to_howto, coff_adjust_symndx,
	coff_link_add_one_symbol, coff_link_output_has_begun,
	coff_final_link_postscript, NULL
};
