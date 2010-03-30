/* doff private structures */

#ifndef _BFD_LIBDOFF_H_
#define _BFD_LIBDOFF_H_

struct doff_section_data {
	int stroffset;		/* Offset into str table... of name? */
	bfd_vma prog_addr;	/* "RUN" address */
	bfd_vma load_addr;	/* "LOAD" address */
	int size;		/* In bytes, or AU, or whatever */
};

struct doff_tdata {
	char *source_filename;

	int num_sections;
	struct doff_section_data *section_data;

	int num_scn_names;
	int max_num_scn_names;
	char **scn_names;

	int num_strings;
	int max_num_strings;
	char **string_table;
};

#endif /* _BFD_LIBDOFF_H_ */
