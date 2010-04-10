#ifndef _BFD_LIBDOFF_H_
#define _BFD_LIBDOFF_H_

#include <unistd.h>

struct doff_private_data {
	unsigned int str_sz;	/* size of string table */
	char *str_table;	/* string table */
	unsigned int num_strs;	/* How many *actual* strings there are*/
	char **str_idx_table;	/* Pointers into string table, corresponding
				 * to numbered entries */
};

struct doff_internal_reloc {
	bfd_vma vaddr;
	unsigned int symidx;
	short type;
};

/* Our own internal representation of a section */
struct doff_internal_sectdata {
	unsigned int size;		/* of raw data */
	void *raw_data;			/* self explanatory */
	bfd_boolean download;		/* Downloaded to target? dictates
					 * whether we put packet headers in
					 * front when writing out / the like */
	unsigned int num_relocs;	/* self explanatory */
	unsigned int max_num_relocs;	/* self explanatory */
	struct doff_internal_reloc *relocs; /* self explanatory */
};

unsigned int doff_fake_swap_out(bfd *abfd, void *src, void *dst);

void doff_swap_sym_in(bfd *abfd, void *src, void *dst);
void doff_swap_scnhdr_in(bfd *abfd, void *src, void *dst);
void doff_swap_aouthdr_in(bfd *abfd, void *src, void *dst);
void doff_swap_filehdr_in(bfd *abfd, void *src, void *dst);
void doff_regurgitate_reloc(bfd *abfd, asection *sect, unsigned int idx,
				struct internal_reloc *dst);

bfd_boolean doff_get_section_contents(bfd *abfd, asection *sect,
			void *data, file_ptr offs, bfd_size_type size);
bfd_boolean doff_set_section_contents(bfd *abfd, asection *sect,
			const void *data, file_ptr offs, bfd_size_type size);
bfd_boolean doff_index_str_table(bfd *abfd, struct doff_private_data *priv);
struct doff_internal_sectdata *doff_internalise_sectiondata(bfd *abfd,
				bfd_boolean download, bfd_size_type sect_size,
				file_ptr sect_offset);
struct doff_internal_sectdata *doff_blank_sectiondata(bfd *abfd,
				bfd_boolean download, bfd_size_type size);
void doff_free_internal_sectiondata(struct doff_internal_sectdata *data);

bfd_reloc_status_type ti_reloc_fail(bfd *abfd, arelent *reloc,
				struct bfd_symbol *sym, void *what,
				asection *sect, bfd *bfd2, char **what2);

#endif /* _BFD_LIBDOFF_H_ */
