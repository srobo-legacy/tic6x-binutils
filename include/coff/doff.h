/* Header for doff, TIs "rework" of coff for loading code via their dspbridge
 * driver on OMAP processors. Seeing how different this is from coff, in theory
 * this should go in its own dir, but that'd be overkill. */

#include <stdint.h>

/* XXX - because someone at TI was on crack when this was designed, different
 * fields are in different places depending on whether the file is big endian
 * or little endian. Which is ridiculous, but the standard. Ugh */

struct doff_filehdr {
	uint32_t strtab_size;		/* Entire strtable size */
	uint32_t entry_point;		/* Self explanatory */
	uint32_t byte_reshuffle;
#define DOFF_BYTE_RESHUFFLE 0x00010203
	uint32_t scn_name_sz;		/* str table size up to last scn name */
	uint16_t num_syms;		/* Self explanatory */
	uint16_t max_str_len;		/* Longest str length with null */
					/* Doesn't include preceeding filename*/
	uint16_t num_scns;		/* Self explanatory */
	uint16_t target_scns;		/* Sections that get loaded to target */
	uint16_t doff_version;		/* Self explanatory */
	uint16_t target_id;		/* See following defines */
#define DOFF_PROC_TMS470		0x97
#define DOFF_PROC_LEAD			0x98 /* ??? */
#define DOFF_PROC_TMS320C6000		0x99
#define DOFF_PROC_LEAD3			0x9c
	uint16_t flags;			/* See following flags */
#define DOFF_LITTLE_ENDIAN		0x100
#define DOFF_BIG_ENDIAN			0x200
#define DOFF_BYTE_ORDER			0x300
	int16_t entry_scn;		/* Idx for entry_point scn, otherwise:*/
#define DOFF_ENTRY_SCN_UNDEF		0
#define DOFF_ENTRY_SCN_ABS		-1
	uint32_t checksum;		/* Sum of previous values as uint32s */
};

struct doff_scnhdr {
	int32_t str_offset;		/* Offset in string table of name */
	int32_t prog_addr;		/* Program start or "RUN" address */
	int32_t load_addr;		/* Address for section to load to */
	int16_t size;			/* Section size */
					/* Those three "in target AU"...? */
/* Probably find it's byte size differences between 54x and rest of world */
	int16_t flags;			/* Self explanatory */
	uint32_t first_pkt_offset;	/* Absolute offset into file of data */
	int32_t num_pkts;		/* Self explanatory */
};
