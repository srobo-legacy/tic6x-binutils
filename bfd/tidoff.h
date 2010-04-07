/* Prototypes for functions we'll be using */
#include "libdoff.h"

/* We wish to use our own swap-in functions, which lets coff read file data
 * without re-implementing too much stuff to handle the insaneness of doff.
 * However, this approaches the impossible in the case of writing the
 * contents of the file out, because so many thing have an implicit location,
 * or have some other fudge thrown in with it. So: no swapout functions are
 * unimplemented, and we #define all calls to swapouts as doff_fake_swap_out,
 * which does precisely nothing.
 * Then, we hook write_object_contents, and dump everything out then. */

#define coff_swap_sym_out	doff_fake_swap_out
#define coff_swap_reloc_out	doff_fake_swap_out
#define coff_swap_filehdr_out	doff_fake_swap_out
#define coff_swap_aouthdr_out	doff_fake_swap_out
#define coff_swap_scnhdr_out	doff_fake_swap_out

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

/* We need to hook into section content munging functions, to handle the
 * unpleasentness of how it's stored. Has to go after #include coffcode
 * as coffcode defines its own coff_set_section_contents */
#undef coff_get_section_contents
#define coff_get_section_contents	doff_get_section_contents
#define coff_set_section_contents	doff_set_section_contents

static bfd_boolean
doff_bad_format_hook(bfd *abfd ATTRIBUTE_UNUSED, void *filehdr)
{
	struct internal_filehdr *f_hdr;

	f_hdr = filehdr;

	if (f_hdr->f_magic != DOFF_MAGIC)
		return FALSE;

	return TRUE;
}

static void *
doff_mkobject_hook(bfd *abfd, void *filehdr, void *aouthdr)
{
	struct doff_filehdr f_hdr;
	struct doff_private_data *priv;
	struct coff_tdata *foo;
	char *strtable;
	off_t saved_fileptr;
	unsigned long size;

	foo = coff_mkobject_hook(abfd, filehdr, aouthdr);
	priv = bfd_alloc(abfd, sizeof(struct doff_private_data));
	foo->ti_doff_private = priv;
	if (priv == NULL)
		return NULL;

	/* Read in string table - preserve file ptr */
	saved_fileptr = bfd_tell(abfd);

	bfd_seek(abfd, 0, SEEK_SET);
	if (bfd_bread(&f_hdr, sizeof(f_hdr), abfd) != sizeof(f_hdr)) {
		free(foo);
		bfd_seek(abfd, saved_fileptr, SEEK_SET);
		return NULL;
	}

	size = H_GET_32(abfd, &f_hdr.strtab_size);
	size += 1; /* We'll stick a null byte at the end, making strlen safe */
	bfd_seek(abfd, sizeof(struct doff_filehdr) +
			sizeof(struct doff_checksum_rec), SEEK_SET);
	strtable = bfd_malloc(size);
	if (strtable == NULL) {
		free(foo);
		bfd_seek(abfd, saved_fileptr, SEEK_SET);
		return NULL;
	}

	if(bfd_bread(strtable, size, abfd) != size) {
		free(strtable);
		free(foo);
		bfd_seek(abfd, saved_fileptr, SEEK_SET);
		return NULL;
	}

	strtable[size-1] = 0; /* Ensure strlen doesn't fall off end of block */
	priv->str_sz = size;
	priv->str_table = strtable;
	obj_coff_strings(abfd) = strtable; /* Also store for coff itself */

	if (doff_index_str_table(abfd, priv)) {
		free(strtable);
		free(foo);
		bfd_seek(abfd, saved_fileptr, SEEK_SET);
		return NULL;
	}

	return foo;
}

static bfd_coff_backend_data tidoff_swap_table =
{
	coff_swap_aux_in, doff_swap_sym_in, coff_swap_lineno_in,
	coff_swap_aux_out, doff_fake_swap_out, coff_swap_lineno_out,
	doff_fake_swap_out, doff_fake_swap_out, doff_fake_swap_out,
	doff_fake_swap_out,

	FILHSZ, AOUTSZ, SCNHSZ, SYMESZ, AUXESZ, RELSZ, LINESZ, FILNMLEN,
	/* FIXME - filename lengths? */

	TRUE,	/* We always have long filenames */
	TRUE,	/* And section names */
	COFF_LONG_SECTION_NAMES_SETTER,
	12,	/* Default alignment power */
	FALSE,	/* Force symnames in strings */
	0,	/* debug string prefix len. ??? */
	doff_swap_filehdr_in, doff_swap_aouthdr_in, doff_swap_scnhdr_in,
	doff_swap_reloc_in, doff_bad_format_hook, coff_set_arch_mach_hook,
	doff_mkobject_hook, styp_to_sec_flags, coff_set_alignment_hook,
	coff_slurp_symbol_table, symname_in_debug_hook,
	coff_pointerize_aux_hook, coff_print_aux, coff_reloc16_extra_cases,
	coff_reloc16_estimate, coff_classify_symbol,
	coff_compute_section_file_positions, coff_start_final_link,
	coff_relocate_section, coff_rtype_to_howto, coff_adjust_symndx,
	coff_link_add_one_symbol, coff_link_output_has_begun,
	coff_final_link_postscript, NULL
};
