/* Following the bfd style, implementation code goes in this header, that then
 * gets included into implementation files with customising preprocessor foo.
 * Not that any other arch will use this */

#include "sysdep.h"
#include "bfd.h"
#include "libdoff.h"
/* inclusion protection please? #include "libbfd.h" */
#include <coff/doff.h>

#define UNUSED(x) ((x) = (x))

static uint32_t
doff_checksum(const void *data, unsigned int len)
{
	const uint32_t *d;
	uint32_t sum;
	int l;

	/* XXX - some assert we can fire if non-multiple-of-4-len given? */
	sum = 0;
	d = data;
	for (l = len; l > 0; l -= sizeof(uint32_t))
		sum += *d++;

	return sum;
}

static bfd_boolean
doff_internalise_strings(bfd *abfd, struct doff_tdata *tdata,
			const char *strings, unsigned int len)
{
	int bytes_left, i, sz, tmp;

	bytes_left = len;

	/* We have a bunch of strings that are used in file - There's nothing
	 * saying how many of them there are, so take a guess and realloc if we
	 * run out of slots */

	sz = bytes_left / 10;
	tdata->max_num_strings = sz;
	tdata->string_table = bfd_alloc(abfd, sz * sizeof(char *));
	tdata->string_idx_table = bfd_alloc(abfd, sz * sizeof(int));
	for (i = 0; bytes_left > 0; i++) {
		tmp = strlen(strings) + 1;
		if (tmp > bytes_left)
			break;

		tdata->string_table[i] = bfd_alloc(abfd, tmp);
		strncpy(tdata->string_table[i], strings, tmp);
		tdata->string_idx_table[i] = len - bytes_left;
		strings += tmp;
		bytes_left -= tmp;

		if (i >= tdata->max_num_strings) {
			sz += 100;
			tdata->max_num_strings = sz;
			tdata->string_table =
				bfd_realloc(tdata->string_table,
						sz * sizeof(char *));
			tdata->string_idx_table =
				bfd_realloc(tdata->string_idx_table,
						sz * sizeof(int));
		}
	}

	tdata->num_strings = i;
	if (bytes_left < 0)
		return TRUE;

	return FALSE;
}

static bfd_boolean
doff_internalise_sections(bfd *abfd, const void *sec_data,
			struct doff_tdata *tdata)
{
	const char *name;
	const struct doff_scnhdr *scn;
	struct doff_section_data *sect;
	int i, j, stroffset;

	scn = sec_data;
	tdata->section_data = bfd_zalloc(abfd, tdata->num_sections *
					sizeof(struct doff_section_data));
	if (tdata->section_data == NULL)
		return TRUE;

	for (i = 0; i < tdata->num_sections; i++) {
		sect = bfd_zalloc(abfd, sizeof(struct doff_section_data));
		if (sect == NULL)
			return TRUE;

		tdata->section_data[i] = sect;

		/* Find index of section name in str table */
		stroffset = bfd_get_32(abfd, &scn->str_offset);
		for (j = 0; j < tdata->num_strings; j++)
			if (tdata->string_idx_table[j] == stroffset)
				break;

		if (j == tdata->num_strings)
			/* Bad section name, but tollerate */
			sect->name_str_idx = -1;
		else
			sect->name_str_idx = j;

		/* Read other fields */
		sect->prog_addr = bfd_get_32(abfd, &scn->prog_addr);
		sect->load_addr = bfd_get_32(abfd, &scn->load_addr);
		sect->size = bfd_get_32(abfd, &scn->size);
		sect->flags = bfd_get_16(abfd, &scn->flags);
		sect->pkt_start = bfd_get_32(abfd, &scn->first_pkt_offset);
		sect->num_pkts = bfd_get_32(abfd, &scn->num_pkts);

		name = (sect->name_str_idx == -1) ? "<un-named section>"
				: tdata->string_table[sect->name_str_idx];
		sect->section = bfd_make_section_anyway(abfd, name);

		scn++;
	}

	return FALSE;
}

static const bfd_target *
doff_object_p(bfd *abfd)
{
	struct bfd_preserve preserve;
	struct doff_filehdr d_hdr;
	struct doff_checksum_rec checksums;
	struct doff_tdata *tdata;
	const bfd_target *target;
	void *data;
	unsigned int size;

	preserve.marker = NULL;
	target = abfd->xvec;

	if (bfd_bread(&d_hdr, sizeof(d_hdr), abfd) != sizeof(d_hdr)) {
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	/* At this point we're supposed to check what kind of file this is.
	 * However, TI didn't deign it necessary to include a magic number in
	 * their file format, so we have to make guesses and conjecture: using
	 * the "byte reshuffle" field, we see it:
	 *"identifies byte ordering of file; always set to BYTE_RESHUFFLE_VALUE"
	 */

	if (bfd_get_32(abfd, &d_hdr.byte_reshuffle) != DOFF_BYTE_RESHUFFLE)
		goto wrong_format;

	if (~doff_checksum(&d_hdr, sizeof(d_hdr))) {
		/* XXX - no erorr code for "bad checksum" or the like? */
		fprintf(stderr, "doff backend: file matches, bad checksum\n");
		goto wrong_format;
	}

	/* That gives us some light assurance. It also ensures that the byte
	 * ordering matches the target, whos get_32 we used. Now try parsing
	 * rest of file */
	if (!bfd_preserve_save(abfd, &preserve))
		goto unwind;

	/* Create something to store our info in */
	if (!(target->_bfd_set_format[bfd_object](abfd)))
		goto unwind;

	preserve.marker = abfd->tdata.doff_obj_data;
	tdata = abfd->tdata.doff_obj_data;

	if (bfd_bread(&checksums, sizeof(checksums), abfd)
					 != sizeof(checksums)) {
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	/* So, there's a checksum of checksum field */
	if (~doff_checksum(&checksums, sizeof(checksums))) {
		fprintf(stderr, "doff backend: bad checksum table\n");
		goto wrong_format;
	}

	/* The string table follows the file header */
	size = bfd_get_32(abfd, &d_hdr.strtab_size);
	if (size > 0x00200000) {
		/* Stupidly sized string table */
		fprintf(stderr, "doff backend: oversized string table\n");
		goto wrong_format;
	}

	data = bfd_malloc(size);
	if (!data) {
		bfd_set_error(bfd_error_no_memory);
		goto unwind;
	}

	if (bfd_bread(data, size, abfd) != size) {
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	if (~(doff_checksum(data, size) + checksums.strtable_checksum)) {
		fprintf(stderr, "doff backend: bad string table checksum\n");
		goto wrong_format;
	}

	/* We have a big table of strings. The first is the originating file
	 * name, followed by section names, followed by normal strings */
	if (doff_internalise_strings(abfd, tdata, data, size)) {
		free(data);
		goto wrong_format;
	}
	free(data);

	/* Now read section table - it's immediately after string table */
	tdata->num_sections = bfd_get_16(abfd, &d_hdr.num_scns);
	if (tdata->num_sections > 0x1000) {
		fprintf(stderr, "doff backend: oversized section num\n");
		goto wrong_format;
	}

	size = tdata->num_sections * sizeof(struct doff_scnhdr);
	data = bfd_malloc(size);
	if (!data) {
		bfd_set_error(bfd_error_no_memory);
		goto unwind;
	}

	if (bfd_bread(data, size, abfd) != size) {
		free(data);
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	if (doff_internalise_sections(abfd, data, tdata)) {
		free(data);
		bfd_set_error(bfd_error_no_memory);
		goto unwind;
	}
	free(data);

	bfd_set_start_address(abfd, bfd_get_32(abfd, &d_hdr.entry_point));
	bfd_preserve_finish(abfd, &preserve);
	return target;

	wrong_format:
	bfd_set_error(bfd_error_wrong_format);

	unwind:
	if (preserve.marker != NULL) {
		/* As it happens, bfd_releasing the marker, aka the tdata,
		 * will also release anything else allocated after that; so all
		 * the string data, all the section data, is automagically
		 * released by this call.
		 * XXX - do we need to kill/release asection records? */
		bfd_preserve_restore(abfd, &preserve);
	}
	return NULL;
}

static bfd_boolean
doff_mkobject(bfd *abfd)
{

	abfd->tdata.doff_obj_data = bfd_zalloc(abfd, sizeof(struct doff_tdata));
	if (abfd->tdata.doff_obj_data == NULL)
		return FALSE;

	return TRUE;
}

static bfd_boolean
doff_write_object_contents(bfd *abfd)
{

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_write_contents");
	abort();
}

static bfd_boolean
doff_close_and_cleanup(bfd *abfd)
{

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_close_and_cleanup");
	abort();
}

static bfd_boolean
doff_bfd_free_cached_info(bfd *abfd)
{

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_bfd_free_cached_info");
	abort();
}

static bfd_boolean
doff_new_section_hook(bfd *abfd, sec_ptr section)
{

	/* I can't currently imagine what to do with this, come back later */
	return _bfd_generic_new_section_hook(abfd, section);
}

static bfd_boolean
doff_get_section_contents(bfd *abfd, sec_ptr section, void *data,
				file_ptr file, bfd_size_type size)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(data);
	UNUSED(file);
	UNUSED(size);
	fprintf(stderr, "Implement doff_bfd_get_section_contents");
	abort();
}

static bfd_boolean
doff_get_section_contents_in_window(bfd *abfd, sec_ptr section,
			bfd_window *window, file_ptr file, bfd_size_type size)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(window);
	UNUSED(file);
	UNUSED(size);
	fprintf(stderr, "Implement doff_bfd_get_section_contents_in_window");
	abort();
}

static bfd_boolean
doff_bfd_copy_private_bfd_data(bfd *abfd, bfd *another_bfd)
{

	UNUSED(abfd);
	UNUSED(another_bfd);
	fprintf(stderr, "Implement doff_bfd_copy_private_bfd_data");
	abort();
}

static bfd_boolean
doff_bfd_merge_private_bfd_data(bfd *abfd, bfd *another_bfd)
{

	UNUSED(abfd);
	UNUSED(another_bfd);
	fprintf(stderr, "Implement doff_bfd_merge_private_bfd_data");
	abort();
}

#if 0
BFD_JUMP_TABLE_COPY initializer declines to use this
static bfd_boolean
doff_bfd_init_private_section_data(bfd *abfd, sec_ptr section, bfd *another_bfd,
			sec_ptr another_section, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(another_bfd);
	UNUSED(another_section);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_init_section_private_data");
	abort();
}
#endif

static bfd_boolean
doff_bfd_copy_private_section_data(bfd *abfd, sec_ptr section, bfd *another_bfd,
			sec_ptr another_section)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(another_bfd);
	UNUSED(another_section);
	fprintf(stderr, "Implement doff_bfd_copy_private_section_data");
	abort();
}

static bfd_boolean
doff_bfd_copy_private_symbol_data(bfd *abfd, asymbol *symbol, bfd *another_bfd,
			asymbol *another_symbol)
{

	UNUSED(abfd);
	UNUSED(symbol);
	UNUSED(another_bfd);
	UNUSED(another_symbol);
	fprintf(stderr, "Implement doff_bfd_copy_private_symbol_data");
	abort();
}

static bfd_boolean
doff_bfd_copy_private_header_data(bfd *abfd, bfd *another_bfd)
{

	UNUSED(abfd);
	UNUSED(another_bfd);
	fprintf(stderr, "Implement doff_bfd_copy_private_header_data");
	abort();
}

static bfd_boolean
doff_bfd_set_private_flags(bfd *abfd, flagword what)
{

	UNUSED(abfd);
	UNUSED(what);
	fprintf(stderr, "Implement doff_bfd_set_private_flags");
	abort();
}

static bfd_boolean
doff_bfd_print_private_bfd_data(bfd *abfd, void *what)
{

	UNUSED(abfd);
	UNUSED(what);
	fprintf(stderr, "Implement doff_bfd_set_private_flags");
	abort();
}

static long
doff_get_symtab_upper_bound(bfd *abfd)
{

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_get_symtab_upper_bound");
	abort();
}

static long
doff_canonicalize_symtab(bfd *abfd, struct bfd_symbol **symbol)
{

	UNUSED(abfd);
	UNUSED(symbol);
	fprintf(stderr, "Implement doff_canonicalize_symtab");
	abort();
}

static struct bfd_symbol *
doff_make_empty_symbol(bfd *abfd)
{
	struct doff_symbol *symbol;

	symbol = bfd_zalloc(abfd, sizeof(struct doff_symbol));
	return &symbol->bfd_symbol;
}

static void
doff_print_symbol(bfd *abfd, void *what, struct bfd_symbol *symbol,
			bfd_print_symbol_type type)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(symbol);
	UNUSED(type);
	fprintf(stderr, "Implement doff_print_symbol");
	abort();
}

static void
doff_get_symbol_info(bfd *abfd, struct bfd_symbol *symbol, symbol_info *info)
{

	UNUSED(abfd);
	UNUSED(symbol);
	UNUSED(info);
	fprintf(stderr, "Implement doff_get_symbol_info");
	abort();
}

static bfd_boolean
doff_bfd_is_local_label_name(bfd *abfd, const char *name)
{

	UNUSED(abfd);
	UNUSED(name);
	fprintf(stderr, "Implement doff_is_local_label_name");
	abort();
}

static bfd_boolean
doff_bfd_is_target_special_symbol(bfd *abfd, asymbol *symbol)
{

	UNUSED(abfd);
	UNUSED(symbol);
	fprintf(stderr, "Implement doff_is_target_special_name");
	abort();
}

static alent *
doff_get_lineno(bfd *abfd, struct bfd_symbol *symbol)
{

	UNUSED(abfd);
	UNUSED(symbol);
	fprintf(stderr, "Implement doff_get_lineno");
	abort();
}

static bfd_boolean
doff_find_nearest_line(bfd *abfd, struct bfd_section *section,
			struct bfd_symbol **symbol, bfd_vma addr,
			const char **what, const char **what2,
			unsigned int *what3)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(symbol);
	UNUSED(addr);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_find_nearest_line");
	abort();
}

static bfd_boolean
doff_find_inliner_info(bfd *abfd, const char **what, const char **what2,
			unsigned int *what3)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_find_inliner_info");
	abort();
}

static asymbol *
doff_bfd_make_debug_symbol(bfd *abfd, void *what, unsigned long size)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(size);
	fprintf(stderr, "Implement doff_bfd_make_debug_symbol");
	abort();
}

static long
doff_read_minisymbols(bfd *abfd, bfd_boolean what, void **what2,
			unsigned int *what3)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_read_minisymbols");
	abort();
}

static asymbol *
doff_minisymbol_to_symbol(bfd *abfd, bfd_boolean what, const void *what2,
			asymbol *minisymbol)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(minisymbol);
	fprintf(stderr, "Implement doff_minisymbol_to_symbol");
	abort();
}

static long
doff_get_reloc_upper_bound(bfd *abfd, sec_ptr section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_get_reloc_upper_bound");
	abort();
}

static long
doff_canonicalize_reloc(bfd *abfd, sec_ptr section, arelent **relocs,
			struct bfd_symbol **symbols)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(relocs);
	UNUSED(symbols);
	fprintf(stderr, "Implement doff_canonicalize_reloc");
	abort();
}

static reloc_howto_type *
doff_bfd_reloc_type_lookup(bfd *abfd, bfd_reloc_code_real_type type)
{

	UNUSED(abfd);
	UNUSED(type);
	fprintf(stderr, "Implement doff_reloc_type_lookup");
	abort();
}

static reloc_howto_type *
doff_bfd_reloc_name_lookup(bfd *abfd, const char *name)
{

	UNUSED(abfd);
	UNUSED(name);
	fprintf(stderr, "Impement doff_reloc_name_lookup");
	abort();
}

static int
doff_sizeof_headers(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_sizeof_headers");
	abort();
}

static bfd_byte *
doff_bfd_get_relocated_section_contents(bfd *abfd, struct bfd_link_info *info,
			struct bfd_link_order *order, bfd_byte *what,
			bfd_boolean what2, struct bfd_symbol **what3)
{

	UNUSED(abfd);
	UNUSED(info);
	UNUSED(order);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_bfd_get_relocated_section_contents");
	abort();
}

static bfd_boolean
doff_bfd_relax_section(bfd *abfd, struct bfd_section *section,
			struct bfd_link_info *info, bfd_boolean *what)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(info);
	UNUSED(what);
	fprintf(stderr, "Implement doff_bfd_relax_section");
	abort();
}

static struct bfd_link_hash_table *
doff_bfd_link_hash_table_create(bfd *abfd)
{

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_bfd_link_hash_table_create");
	abort();
}

static void
doff_bfd_link_hash_table_free(struct bfd_link_hash_table *table)
{

	UNUSED(table);
	fprintf(stderr, "Implement doff_bfd_link_hash_table_free");
	abort();
}

static bfd_boolean
doff_bfd_link_add_symbols(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_link_add_symbols");
	abort();
}

static void
doff_bfd_link_just_syms(asection *section, struct bfd_link_info *info)
{

	UNUSED(section);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_link_just_syms");
	abort();
}

static bfd_boolean
doff_bfd_final_link(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_final_link");
	abort();
}

static bfd_boolean
doff_bfd_link_split_section(bfd *abfd, struct bfd_section *section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_bfd_link_split_section");
	abort();
}

static bfd_boolean
doff_bfd_gc_sections(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_gc_sections");
	abort();
}

static bfd_boolean
doff_bfd_merge_sections(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_merge_sections");
	abort();
}

static bfd_boolean
doff_bfd_is_group_section(bfd *abfd, const struct bfd_section *section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_bfd_is_group_section");
	abort();
}

static bfd_boolean
doff_bfd_discard_group(bfd *abfd, struct bfd_section *section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_bfd_discard_group");
	abort();
}

static void
doff_section_already_linked(bfd *abfd, struct bfd_section *section,
			struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(info);
	fprintf(stderr, "Implement doff_section_already_linked");
	abort();
}

static bfd_boolean
doff_bfd_define_common_symbol(bfd *abfd, struct bfd_link_info *info,
			struct bfd_link_hash_entry *hash)
{

	UNUSED(abfd);
	UNUSED(info);
	UNUSED(hash);
	fprintf(stderr, "Implement doff_bfd_define_common_symbol");
	abort();
}
static bfd_boolean
doff_set_section_contents(bfd *abfd, sec_ptr section, const void *location,
			file_ptr file, bfd_size_type type)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(location);
	UNUSED(file);
	UNUSED(type);
	fprintf(stderr, "Implement doff_set_section_contents");
	abort();
}
