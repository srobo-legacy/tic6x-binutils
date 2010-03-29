/* Header for doff, TIs "rework" of coff for loading code via their dspbridge
 * driver on OMAP processors. Seeing how different this is from coff, in theory
 * this should go in its own dir, but that'd be overkill. */

#include <stdint.h>

struct doff_filehdr {
	uint32_t strtab_size;
	uint32_t entry_point;
	uint32_t byte_reshuffle;
	uint32_t scn_name_sz;
	uint16_t num_syms;
	uint16_t max_str_len;
	uint16_t num_scns;
	uint16_t target_scns;
	uint16_t doff_version;
	uint16_t target_id;
#define DOFF_PROC_TMS470		0x97
#define DOFF_PROC_LEAD			0x98 /* ??? */
#define DOFF_PROC_TMS320C6000		0x99
#define DOFF_PROC_LEAD3			0x9c
	uint16_t flags;
#define DOFF_LITTLE_ENDIAN		0x100
#define DOFF_BIG_ENDIAN			0x200
#define DOFF_BYTE_ORDER			0x300
	int16_t entry_scn;
#define DOFF_ENTRY_SCN_UNDEF		0
#define DOFF_ENTRY_SCN_ABS		-1
	uint32_t checksum;
};
