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
doff_parse_strings(bfd *abfd, struct doff_tdata *tdata, const char *strings,
			unsigned int len)
{
	int bytes_left, i, tmp;

	bytes_left = len;

	/* First string is the source file name */
	tmp = strlen(strings) + 1;
	if (tmp > bytes_left)
		return TRUE;

	tdata->source_filename = bfd_alloc(abfd, tmp);
	strncpy(tdata->source_filename, strings, tmp);
	strings += tmp;
	bytes_left -= tmp;

	/* Following this is a set of strings naming each section */
	tdata->scn_names = bfd_alloc(abfd, sizeof(char*) * tdata->num_sections);
	tdata->num_scn_names = tdata->num_sections;
	tdata->max_num_scn_names = tdata->num_sections;
	memset(tdata->scn_names, 0, sizeof(char *) * tdata->num_sections);

	for (i = 0; i < tdata->num_sections; i++) {
		tmp = strlen(strings) + 1;
		if (tmp > bytes_left)
			break;

		tdata->scn_names[i] = bfd_alloc(abfd, tmp);
		strncpy(tdata->scn_names[i], strings, tmp);
		strings += tmp;
		bytes_left -= tmp;
	}

	if (bytes_left < 0)
		return TRUE;

	/* Finally, we have a bunch of strings that are actually used in file.
	 * There's nothing saying how many of them there are, so take a guess
	 * and realloc if we run out of slots */

	tdata->max_num_strings = bytes_left / 10;
	tdata->string_table = bfd_alloc(abfd, bytes_left / 10);
	for (i = 0; bytes_left > 0; i++) {
		tmp = strlen(strings) + 1;
		if (tmp > bytes_left)
			break;

		tdata->string_table[i] = bfd_alloc(abfd, tmp);
		strncpy(tdata->string_table[i], strings, tmp);
		strings += tmp;
		bytes_left -= tmp;

		if (i >= tdata->max_num_strings) {
			tdata->max_num_strings += 100;
			tdata->string_table = bfd_realloc(tdata->string_table,
				tdata->max_num_strings * sizeof(char *));
		}
	}

	tdata->num_strings = i;
	if (bytes_left < 0)
		return TRUE;

	return FALSE;
}

static void
doff_free_strings(bfd *abfd, struct doff_tdata *tdata)
{
	int i;

	if (tdata->source_filename != NULL)
		bfd_release(abfd, tdata->source_filename);

	if (tdata->scn_names)
		for (i = 0; i < tdata->num_sections; i++)
			if (tdata->scn_names[i])
				bfd_release(abfd, tdata->scn_names[i]);

	if (tdata->num_strings && tdata->string_table)
		for (i = 0; i < tdata->num_strings; i++)
			if (tdata->string_table[i])
				bfd_release(abfd, tdata->string_table[i]);

	tdata->source_filename = NULL;
	tdata->num_scn_names = 0;
	tdata->max_num_scn_names = 0;
	tdata->scn_names = NULL;
	tdata->num_strings = 0;
	tdata->max_num_strings = 0;
	tdata->string_table = NULL;
	return;
}

static const bfd_target *
doff_object_p(bfd *abfd)
{
	struct bfd_preserve preserve;
	struct doff_filehdr d_hdr;
	struct doff_tdata *tdata;
	const bfd_target *target;
	char *strings;
	unsigned int string_table_sz;

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

	tdata->num_sections = bfd_get_32(abfd, &d_hdr.num_scns);
	if (tdata->num_sections > 0x1000) {
		fprintf(stderr, "doff backend: oversized section num\n");
		goto wrong_format;
	}

	/* The string table follows the file header */
	string_table_sz = bfd_get_32(abfd, &d_hdr.strtab_size);
	if (string_table_sz > 0x00200000) {
		/* Stupidly sized string table */
		fprintf(stderr, "doff backend: oversized string table\n");
		goto wrong_format;
	}

	strings = bfd_alloc(abfd, string_table_sz);
	if (!strings) {
		bfd_set_error(bfd_error_no_memory);
		goto unwind;
	}

	if (bfd_bread(strings, string_table_sz, abfd) != string_table_sz) {
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	/* We have a big table of strings. The first is the originating file
	 * name, followed by section names, followed by normal strings */
	if (doff_parse_strings(abfd, tdata, strings, string_table_sz)) {
		doff_free_strings(abfd, tdata);
		bfd_release(abfd, strings);
		goto wrong_format;
	}
	bfd_release(abfd, strings);

	wrong_format:
	bfd_set_error(bfd_error_wrong_format);

	unwind:
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

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_new_section_hook");
	abort();
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

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_make_empty_symbol");
	abort();
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