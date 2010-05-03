/* Header for doff, TIs "rework" of coff for loading code via their dspbridge
 * driver on OMAP processors. Seeing how different this is from coff, in theory
 * this should go in its own dir, but that'd be overkill. */

#ifndef _INCLUDE_COFF_DOFF_H_
#define _INCLUDE_COFF_DOFF_H_

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

struct doff_checksum_rec {
	uint32_t timestamp;
	uint32_t section_checksum;
	uint32_t strtable_checksum;
	uint32_t symbol_checksum;
	uint32_t self_checksum;
};

#define FILHDR	struct doff_filehdr
#define FILHSZ sizeof(struct doff_filehdr) + sizeof(struct doff_checksum_rec)
/* Doff doesn't have the conventional file header / opt header; so for the
 * purposes of the coff backend, have the checksum record be part of the file
 * header, where we can pull the timestamp info in from. */

#define AOUTSZ 0
/* There's no opt header; more importantly, it's pretty unpleasent to get
 * the data that it would hold anyway. So don't bother with it */

struct doff_scnhdr {
	int32_t str_offset;		/* Offset in string table of name */
	int32_t prog_addr;		/* Program start or "RUN" address */
	int32_t load_addr;		/* Address for section to load to */
	int32_t size;			/* Section size */
					/* Those three "in target AU"...? */
/* Probably find it's byte size differences between 54x and rest of world */
	int16_t page;			/* "memory page id" - what? */
	int16_t flags;			/* Self explanatory */
#define DOFF_SCN_FLAG_TYPE_MASK	0xF	/* Different types of sections */
#define DOFF_SCN_TYPE_TEXT	0
#define DOFF_SCN_TYPE_DATA	1
#define DOFF_SCN_TYPE_BSS	2
#define DOFF_SCN_TYPE_CINIT	3
#define DOFF_SCN_FLAG_ALLOC	0x10	/* Allocate space on target for scn */
#define DOFF_SCN_FLAG_DOWNLOAD	0x20	/* Section to be loaded onto target */
#define DOFF_SCN_FLAG_ALIGN	0xF00	/* scn alignment: 2^this_field */
#define DOFF_SCN_FLAGS_ALIGN_SHIFT 8	/* See above */
	uint32_t first_pkt_offset;	/* Absolute offset into file of data */
	int32_t num_pkts;		/* Self explanatory */
};

#define SCNHDR	struct doff_scnhdr
#define SCNHSZ	sizeof(struct doff_scnhdr)

struct doff_symbol {
	int32_t str_offset;		/* Bytes into string table */
	int32_t value;			/* "Value" */
	int16_t scn_num;		/* Self explanatory */
	int16_t storage_class;		/* ??? */
};

#define SYMENT	struct doff_symbol
#define SYMESZ	sizeof(struct doff_symbol)

/* These usually come from external.h, but we don't want it's external struct */
#define N_BTMASK	0xf
#define N_TMASK		0x30
#define N_BTSHFT	4
#define N_TSHIFT	2

struct doff_image_packet {
	int32_t num_relocs;		/* Num relocs */
	int32_t packet_sz;		/* Size in this packet */
	int32_t checksum;		/* Image packet checksum */
					/* Including relocs apparently */
	/* Followed by the actual packet data. Then perhaps relocs? */
};

struct doff_reloc {
	int32_t vaddr;			/* Vaddr of reloc */

	/* Actual relocation data; lacking any real documentation, largely
	 * copied from TIs implementation */
	union {
		struct {
			uint8_t offset;        /* bit offset of rel fld      */
			uint8_t fieldsz;       /* size of rel fld            */
			uint8_t wordsz;        /* # bytes containing rel fld */
			uint8_t dum1;
			uint16_t dum2;
			uint16_t type;
		} r_field;

		struct {
			uint32_t spc;		/* Image packet relative PC */
			uint16_t dum;
			uint16_t type;		/* relocation type */
		} r_spc;

		struct {
			uint32_t uval;		/* constant value */
			uint16_t dum;
			uint16_t type;		/* relocation type */
		} r_uval;

		struct {
			int32_t sym_idx;	/* symbol table index */
			uint16_t disp;		/* "extra addr encode data" */
			uint16_t type;		/* reloc type */
		} r_sym;
	} reloc;
};

#define RELOC struct doff_reloc
#define RELSZ sizeof(struct doff_reloc)
#define external_reloc doff_reloc

/* Some guesses, until we care more */
#define L_LNNO_SIZE	4
#define E_DIMNUM	4
#define E_FILNMLEN	14

/* Pick up some extra definitions */
#define DO_NOT_DEFINE_FILHDR
#define DO_NOT_DEFINE_AOUTHDR
#define DO_NOT_DEFINE_SCNHDR
#define DO_NOT_DEFINE_SYMENT
#include <coff/external.h>

#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"
#define _CINIT	".cinit"
#define _SCONST	".const"
#define _SWITCH	".switch"
#define _STACK	".stack"
#define _SYSMEM	".sysmem"

#endif /* _INCLUDE_COFF_DOFF_H_ */
