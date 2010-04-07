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
	unsigned int num_relocs;	/* self explanatory */
	unsigned int max_num_relocs;	/* self explanatory */
	struct doff_internal_reloc *relocs; /* self explanatory */
};

unsigned int doff_swap_reloc_out(bfd *abfd, void *src, void *dst);
unsigned int doff_swap_sym_out(bfd *abfd, void *src, void *dst);
unsigned int doff_swap_scnhdr_out(bfd *abfd, void *src, void *dst);
unsigned int doff_swap_aouthdr_out(bfd *abfd, void *src, void *dst);
unsigned int doff_swap_filehdr_out(bfd *abfd, void *src, void *dst);

void doff_swap_reloc_in(bfd *abfd, void *src, void *dst);
void doff_swap_sym_in(bfd *abfd, void *src, void *dst);
void doff_swap_scnhdr_in(bfd *abfd, void *src, void *dst);
void doff_swap_aouthdr_in(bfd *abfd, void *src, void *dst);
void doff_swap_filehdr_in(bfd *abfd, void *src, void *dst);

bfd_boolean doff_set_section_contents(bfd *abfd, asection *sect,
			const void *data, file_ptr offs, bfd_size_type size);
bfd_boolean doff_index_str_table(bfd *abfd, struct doff_private_data *priv);
struct doff_internal_sectdata *doff_internalise_sectiondata(bfd *abfd,
					struct internal_scnhdr *sectinfo);
void doff_free_internal_sectiondata(struct doff_internal_sectdata *data);

#endif /* _BFD_LIBDOFF_H_ */
