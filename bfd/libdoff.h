/* doff private structures */

#ifndef _BFD_LIBDOFF_H_
#define _BFD_LIBDOFF_H_

struct doff_tdata {
	char *source_filename;

	int num_sections;

	int num_scn_names;
	int max_num_scn_names;
	char **scn_names;

	int num_strings;
	int max_num_strings;
	char **string_table;
};

#endif /* _BFD_LIBDOFF_H_ */
