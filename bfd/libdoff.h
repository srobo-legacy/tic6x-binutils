#ifndef _BFD_LIBDOFF_H_
#define _BFD_LIBDOFF_H_

#include <unistd.h>

struct doff_private_data {
	int str_sz;		/* size of string table */
	char *str_table;	/* string table */

	struct doff_checksum_rec checksum;
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

#endif /* _BFD_LIBDOFF_H_ */
