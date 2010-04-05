#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "bfd-in2.h"
#include <coff/doff.h>
#include <coff/internal.h>
#include "libcoff.h"
#include "libdoff.h"

#include <stdint.h>

#define UNUSED(x) ((x) = (x))

static uint32_t
doff_checksum(const void *data, unsigned int len)
{
	const uint32_t *d;
	uint32_t sum;
	int l;

	BFD_ASSERT((len & 3) == 0);
	sum = 0;
	d = data;
	for (l = len; l > 0; l -= sizeof(uint32_t))
		sum += *d++;

	return sum;
}


unsigned int
doff_swap_reloc_out(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_reloc_out\n");
	abort();
}
	
unsigned int
doff_swap_sym_out(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_sym_out\n");
	abort();
}

unsigned int
doff_swap_scnhdr_out(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_scnhdr_out\n");
	abort();
}

unsigned int
doff_swap_aouthdr_out(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_aouthdr_out\n");
	abort();
}

unsigned int
doff_swap_filehdr_out(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_reloc_out\n");
	abort();
}

void
doff_swap_reloc_in(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_reloc_out\n");
	abort();
}

void
doff_swap_sym_in(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_reloc_out\n");
	abort();
}

void
doff_swap_scnhdr_in(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_reloc_out\n");
	abort();
}

void
doff_swap_aouthdr_in(bfd *abfd, void *src, void *dst)
{

	UNUSED(abfd);
	UNUSED(src);
	UNUSED(dst);
	fprintf(stderr, "Implement doff_swap_reloc_out\n");
	abort();
}

void
doff_swap_filehdr_in(bfd *abfd, void *src, void *dst)
{
	struct doff_filehdr *f_hdr;
	struct internal_filehdr *out;
	struct doff_checksum_rec *checksums;
	void *data;
	uint32_t checksum;
	unsigned int sz;

	f_hdr = src;
	out = dst;

	/* Check checksum and byte_reshuffle meets expectations - it's the
	 * closest thing to a magic number tidoff has. Also ensure the target
	 * id is not zero, that's used to indicate bad format to bad_format_hook
	 */
	if (doff_checksum(f_hdr, sizeof(*f_hdr)) != 0xFFFFFFFF ||
		H_GET_32(abfd, &f_hdr->byte_reshuffle) != DOFF_BYTE_RESHUFFLE
		|| H_GET_16(abfd, &f_hdr->target_id) == 0) {
		memset(out, 0, sizeof(*out));
		/* XXX - should we set an error? */
		return;
	}

	out->f_magic = H_GET_16(abfd, &f_hdr->target_id);
	out->f_nscns = H_GET_16(abfd, &f_hdr->num_scns);
	out->f_symptr = H_GET_32(abfd, &f_hdr->strtab_size) +
				(out->f_nscns * sizeof(struct doff_scnhdr));
	out->f_nsyms = H_GET_16(abfd, &f_hdr->num_syms);
	out->f_flags = F_AR32WR;	/* Little endian; meh */
	/* Let's set all possibly things this may have; then return zero of them
	 * if it later turns out that they don't exist (although that's more
	 * coff's problem than ours) */
	out->f_flags |= F_RELFLG | F_EXEC | F_LNNO | F_LSYMS;

	/* There is no opt header */
	out->f_opthdr = 0;

	/* Due to other hacks, file header also includes checksum header */
	checksums = (void *)(f_hdr + 1);
	if (doff_checksum(checksums, sizeof(*checksums)) != 0xFFFFFFFF) {
		memset(out, 0, sizeof(*out));
		return;
	}

	out->f_timdat = H_GET_32(abfd, &checksums->timestamp);

	sz = H_GET_32(abfd, &f_hdr->strtab_size);
	data = bfd_malloc(sz);
	if (data == NULL) {
		/* alloc will have set memory error for us */
		memset(out, 0, sizeof(*out));
		return;
	}

	if (bfd_bread(data, sz, abfd) != sz) {
		memset(out, 0, sizeof(*out));
		return;
	}

	checksum = doff_checksum(data, sz);
	checksum += checksums->strtable_checksum;
	if (checksum != 0xFFFFFFFF) {
		memset(out, 0, sizeof(*out));
		return;
	}

	/* While we're here, read in all the section table and validate it */
	sz = sizeof(struct doff_scnhdr) * out->f_nscns;
	data = bfd_malloc(sz);
	if (data == NULL)
		return;

	if (bfd_bread(data, sz, abfd) == sz) {
		free(data);
		return;
	}

	checksum = doff_checksum(data, sz);
	free(data);
	checksum += checksums->section_checksum;
	if (checksum != 0xFFFFFFFF) {
		memset(out, 0, sizeof(*out));
		return;
	}

	/* Validate symbol table too... */
	sz = sizeof(struct doff_symbol) * out->f_nsyms;
	data  = bfd_malloc(sz);
	if (data == NULL)
		return;

	if(bfd_bread(data, sz, abfd) != sz) {
		free(data);
		return;
	}

	checksum = doff_checksum(data, sz);
	free(data);
	checksum += checksums->symbol_checksum;
	if (checksum != 0xFFFFFFFF) {
		memset(out, 0, sizeof(*out));
		return;
	}

	return;
}

bfd_boolean
doff_set_section_contents(bfd *abfd, asection *sect, const void *data,
					file_ptr offs, bfd_size_type size)
{

	UNUSED(abfd);
	UNUSED(sect);
	UNUSED(data);
	UNUSED(offs);
	UNUSED(size);
	fprintf(stderr, "Implement doff_set_section_contents\n");
	abort();
}
