/* doff private structures */

#ifndef _BFD_LIBDOFF_H_
#define _BFD_LIBDOFF_H_

#include "bfd-in2.h"

struct doff_section_data {
	int stroffset;		/* Offset into str table... of name? */
	bfd_vma prog_addr;	/* "RUN" address */
	bfd_vma load_addr;	/* "LOAD" address */
	int size;		/* In bytes, or AU, or whatever */
	int flags;		/* flags; see no definitions right now */
	int pkt_start;		/* offset into file where pkt data starts */
	int num_pkts;		/* Number of said packets */

	asection *section;	/* BFD section */
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
