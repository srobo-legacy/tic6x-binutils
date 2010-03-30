/* doff private structures */

#ifndef _BFD_LIBDOFF_H_
#define _BFD_LIBDOFF_H_

#include "bfd-in2.h"

struct doff_ipacket {
	void *raw_data;		/* In sections lump of raw data */
	int size;		/* Size of packet */
	int num_relocs;		/* Self explanatory */
	arelent **relocs;	/* Table of num_reloc relocs */
};

struct doff_section_data {
	int name_str_idx;	/* str table idx of name; can be -1 */
	bfd_vma prog_addr;	/* "RUN" address */
	bfd_vma load_addr;	/* "LOAD" address */
	unsigned int size;	/* In bytes, or AU, or whatever */
	int flags;		/* flags; see no definitions right now */
	int pkt_start;		/* offset into file where pkt data starts */
	int num_pkts;		/* Number of said packets */

	asection *section;	/* BFD section */

	struct doff_ipacket **insn_packets;
				/* table of instruction packets */

	void *raw_data;		/* raw section data, pointed to by ipackets */
};

struct doff_symbol {
	asymbol bfd_symbol;
	/* Still don't know what we require here yet */
};

struct doff_tdata {
	int num_sections;
	struct doff_section_data **section_data;

	int num_strings;
	int max_num_strings;
	char **string_table;
	int *string_idx_table;
};

#endif /* _BFD_LIBDOFF_H_ */

const bfd_target *doff_object_p(bfd *abfd);
bfd_boolean doff_mkobject(bfd *abfd);
bfd_boolean doff_write_object_contents(bfd *abfd);
bfd_boolean doff_close_and_cleanup(bfd *abfd);
bfd_boolean doff_bfd_free_cached_info(bfd *abfd);
bfd_boolean doff_new_section_hook(bfd *abfd, sec_ptr section);
bfd_boolean doff_get_section_contents(bfd *abfd, sec_ptr section, void *data,
			file_ptr file, bfd_size_type size);
bfd_boolean doff_get_section_contents_in_window(bfd *abfd, sec_ptr section,
			bfd_window *window, file_ptr file, bfd_size_type size);
bfd_boolean doff_bfd_copy_private_bfd_data(bfd *abfd, bfd *another_bfd);
bfd_boolean doff_bfd_merge_private_bfd_data(bfd *abfd, bfd *another_bfd);
bfd_boolean doff_bfd_copy_private_section_data(bfd *abfd, sec_ptr section,
			bfd *another_bfd, sec_ptr another_section);
bfd_boolean doff_bfd_copy_private_symbol_data(bfd *abfd, asymbol *symbol,
			bfd *another_bfd, asymbol *another_symbol);
bfd_boolean doff_bfd_copy_private_header_data(bfd *abfd, bfd *another_bfd);
bfd_boolean doff_bfd_set_private_flags(bfd *abfd, flagword what);
bfd_boolean doff_bfd_print_private_bfd_data(bfd *abfd, void *what);
long doff_get_symtab_upper_bound(bfd *abfd);
long doff_canonicalize_symtab(bfd *abfd, struct bfd_symbol **symbol);
struct bfd_symbol *doff_make_empty_symbol(bfd *abfd);
void doff_print_symbol(bfd *abfd, void *what, struct bfd_symbol *symbol,
			bfd_print_symbol_type type);
void doff_get_symbol_info(bfd *abfd, struct bfd_symbol *symbol,
			symbol_info *info);
bfd_boolean doff_bfd_is_local_label_name(bfd *abfd, const char *name);
bfd_boolean doff_bfd_is_target_special_symbol(bfd *abfd, asymbol *symbol);
alent *doff_get_lineno(bfd *abfd, struct bfd_symbol *symbol);
bfd_boolean doff_find_nearest_line(bfd *abfd, struct bfd_section *section,
			struct bfd_symbol **symbol, bfd_vma addr,
			const char **what, const char **what2,
			unsigned int *what3);
bfd_boolean doff_find_inliner_info(bfd *abfd, const char **what,
			const char **what2, unsigned int *what3);
asymbol *doff_bfd_make_debug_symbol(bfd *abfd, void *what, unsigned long size);
long doff_read_minisymbols(bfd *abfd, bfd_boolean what, void **what2,
			unsigned int *what3);
asymbol *doff_minisymbol_to_symbol(bfd *abfd, bfd_boolean what,
			const void *what2, asymbol *minisymbol);
long doff_get_reloc_upper_bound(bfd *abfd, sec_ptr section);
long doff_canonicalize_reloc(bfd *abfd, sec_ptr section, arelent **relocs,
			struct bfd_symbol **symbols);
reloc_howto_type *doff_bfd_reloc_type_lookup(bfd *abfd,
			bfd_reloc_code_real_type type);
reloc_howto_type *doff_bfd_reloc_name_lookup(bfd *abfd, const char *name);
int doff_sizeof_headers(bfd *abfd, struct bfd_link_info *info);
bfd_byte * doff_bfd_get_relocated_section_contents(bfd *abfd,
			struct bfd_link_info *info,
			struct bfd_link_order *order, bfd_byte *what,
			bfd_boolean what2, struct bfd_symbol **what3);
bfd_boolean doff_bfd_relax_section(bfd *abfd, struct bfd_section *section,
			struct bfd_link_info *info, bfd_boolean *what);
struct bfd_link_hash_table *doff_bfd_link_hash_table_create(bfd *abfd);
void doff_bfd_link_hash_table_free(struct bfd_link_hash_table *table);
bfd_boolean doff_bfd_link_add_symbols(bfd *abfd, struct bfd_link_info *info);
void doff_bfd_link_just_syms(asection *section, struct bfd_link_info *info);
bfd_boolean doff_bfd_final_link(bfd *abfd, struct bfd_link_info *info);
bfd_boolean doff_bfd_link_split_section(bfd *abfd, struct bfd_section *section);
bfd_boolean doff_bfd_gc_sections(bfd *abfd, struct bfd_link_info *info);
bfd_boolean doff_bfd_merge_sections(bfd *abfd, struct bfd_link_info *info);
bfd_boolean doff_bfd_is_group_section(bfd *abfd,
			const struct bfd_section *section);
bfd_boolean doff_bfd_discard_group(bfd *abfd, struct bfd_section *section);
void doff_section_already_linked(bfd *abfd, struct bfd_section *section,
			struct bfd_link_info *info);
bfd_boolean doff_bfd_define_common_symbol(bfd *abfd, struct bfd_link_info *info,
			struct bfd_link_hash_entry *hash);
bfd_boolean doff_set_section_contents(bfd *abfd, sec_ptr section,
			const void *location, file_ptr file,
			bfd_size_type type);
