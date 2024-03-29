/* Assembler for TMS320C64X class of processors
 * Copyright 2010 Jeremy Morse <jeremy.morse@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
 * MA 02110-1301, USA
 */

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

struct scn_swapout {
	struct doff_scnhdr hdr;
	void *raw_scn_data;
	unsigned int num_ipkts;
	unsigned int raw_data_sz;
};

bfd_boolean doff_externalise_section_data(asection *curscn,
				struct scn_swapout *output);

bfd_reloc_status_type
ti_reloc_fail(bfd *abfd ATTRIBUTE_UNUSED, arelent *reloc ATTRIBUTE_UNUSED,
		struct bfd_symbol *sym ATTRIBUTE_UNUSED,
		void *what ATTRIBUTE_UNUSED,
		asection *sect ATTRIBUTE_UNUSED,
		bfd *bfd2 ATTRIBUTE_UNUSED,
		char **what2 ATTRIBUTE_UNUSED)
{

	fprintf(stderr, "Dear valued user:\n"
			"You are attempting to link a TI DOFF file section "
			"with relocations that become resolved and require "
			"evaluating. TI's \"generic\" relocs are a language "
			"by themselves, and a cricket bat with nails in would "
			"not convince me to go anywhere near it.\n"
			"I recommend that you either link in one step (ie, not "
			"incrementally or any other way that involves "
			"evaluating relocs) or if using a precompiled doff "
			"file your should just allow the dspbridge dynamic "
			"loader to handle it (saving both your and my sanity)."
			"\nYours faithfully,\nThe Author\n");
	return bfd_reloc_outofrange;
}


static uint32_t
doff_checksum(const void *data, unsigned int len)
{
	const uint32_t *d;
	const uint8_t *u;
	uint32_t sum;
	int l, i;

	sum = 0;
	d = data;
	for (l = len; l > 3; l -= sizeof(uint32_t))
		sum += *d++;

	if (l != 0) {
		/* Tack in the remaining data, little endian. Dunno if this
		 * is right or wrong, the actual information about this checksum
		 * isn't documented anywhere */
		u = (uint8_t *)d;
		for (i = 0; l > 0; l--, i++) {
			sum += u[i] << (i * 8);
		}
	}

	return sum;
}

static struct doff_internal_sectdata *
doff_get_internal_sectdata(bfd *abfd, asection *sect, int direction)
{
	struct coff_section_tdata *coff_tdata;
	struct doff_internal_sectdata *doff_tdata;

	/* It appears coff tdata is only created on demand, ie when things
	 * actually need to use it. So, we need to allocate it ourselves in
	 * certain circumstances */
	coff_tdata = sect->used_by_bfd;
	if (coff_tdata == NULL) {
		coff_tdata = bfd_zalloc(abfd,sizeof(struct coff_section_tdata));
		if (coff_tdata == NULL)
			return FALSE;

		sect->used_by_bfd = coff_tdata;
	}

	doff_tdata = coff_tdata->tdata;
	if (doff_tdata == NULL) {
		if (direction == read_direction && (
					abfd->direction == read_direction
					|| abfd->direction == both_direction)) {
			doff_tdata = doff_internalise_sectiondata(abfd,
						sect->size, sect->filepos);
		} else if (direction == write_direction && (
					abfd->direction == write_direction
					|| abfd->direction == both_direction)) {
			doff_tdata = doff_blank_sectiondata(abfd,
						sect->size);
		} else {
			bfd_set_error(bfd_error_invalid_operation);
			return FALSE;
		}

		if (doff_tdata == NULL)
			return FALSE;

		coff_tdata->tdata = doff_tdata;
	}

	return doff_tdata;
}

/* See tidoff.h commentry on what we're doing about swaping out */
unsigned int
doff_fake_swap_out(bfd *abfd ATTRIBUTE_UNUSED, void *src ATTRIBUTE_UNUSED,				void *dst ATTRIBUTE_UNUSED)
{

	return 1;
}
	
void
doff_swap_sym_in(bfd *abfd, void *src, void *dst)
{
	struct doff_symbol *sym;
	struct internal_syment *out;

	sym = src;
	out = dst;

	out->_n._n_n._n_zeroes = 0;
	out->_n._n_n._n_offset = H_GET_32(abfd, &sym->str_offset);
	out->n_value = H_GET_32(abfd, &sym->value);
	out->n_scnum = H_GET_16(abfd, &sym->scn_num);
	out->n_flags = 0;
	out->n_type = 0;
	out->n_sclass = H_GET_16(abfd, &sym->storage_class);

	/* Many symbols have no storage class, so give them the (ti specific?)
	 * undefined external class */
	if (out->n_sclass == 0)
		out->n_sclass = C_UEXT;

	out->n_numaux = 0;
	return;
}

void
doff_swap_scnhdr_in(bfd *abfd, void *src, void *dst)
{
	struct doff_scnhdr *scnhdr;
	struct internal_scnhdr *out;
	struct doff_private_data *priv;
	struct doff_internal_sectdata *sect;
	unsigned int str_offset, flags;

	/* We can be confident at this point that the section headers has been
	 * validated and that the string table has been read in and stored
	 * in our private data record. This relies on what coffgen does
	 * at the moment, perhaps this will change in the future. */
	scnhdr = src;
	out = dst;
	priv = coff_data(abfd)->ti_doff_private;
	memset(out, 0, sizeof(*out));

	str_offset = H_GET_32(abfd, &scnhdr->str_offset);
	if (str_offset >= priv->str_sz)
		goto invalid;

	/* We need use the "\idx" method of setting section names - coff
	 * only expects 8 characters, if it doesn't fit we give it an index
	 * number in the string table. So always do this */

	/* Actually set the section "name" */
	snprintf(&out->s_name[0], 7, "/%d", str_offset);

	out->s_vaddr = H_GET_32(abfd, &scnhdr->load_addr);
	out->s_size = H_GET_32(abfd, &scnhdr->size);
	out->s_scnptr = H_GET_32(abfd, &scnhdr->first_pkt_offset);
	out->s_relptr = out->s_scnptr; /* We'll have to hack relocs anyway */
	out->s_lnnoptr = 0;
	out->s_nlnno = 0;

	flags = H_GET_32(abfd, &scnhdr->flags);

	/* NB: from observation (dynbase), the download flag implies alloc */
	if (flags & DOFF_SCN_FLAG_DOWNLOAD)
		out->s_flags = STYP_REG;
	else if (flags & DOFF_SCN_FLAG_ALLOC)
		out->s_flags = STYP_NOLOAD;
	else
		out->s_flags = STYP_INFO | STYP_NOLOAD;

	if ((flags & DOFF_SCN_FLAG_TYPE_MASK) == DOFF_SCN_TYPE_TEXT)
		out->s_flags |= STYP_TEXT;
	else if ((flags & DOFF_SCN_FLAG_TYPE_MASK) == DOFF_SCN_TYPE_DATA)
		out->s_flags |= STYP_DATA;
	else if ((flags & DOFF_SCN_FLAG_TYPE_MASK) == DOFF_SCN_TYPE_BSS)
		out->s_flags |= STYP_BSS;

	/* We need to tell bfd about relocs. Unfortunately TI chose not to store
	 * this information in a sane manner, so we actually have to trawl the
	 * section data looking for them */
	if (flags & DOFF_SCN_FLAG_DOWNLOAD) {
		sect = doff_internalise_sectiondata(abfd, out->s_size,
							out->s_scnptr);
		if (sect == NULL)
			goto invalid;

		out->s_nreloc = sect->num_relocs;
		doff_free_internal_sectiondata(sect);
	} else {
		out->s_nreloc = 0;
	}
	/* Comment in internal.h says s_align is only used by I960 */

	return;

	invalid:
	memset(out, 0, sizeof(*out));
	return;
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
	off_t saved_fileptr;
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
				sizeof(struct doff_filehdr) +
				sizeof(struct doff_checksum_rec) +
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

	/* Validate string table - but first save where we are in the file */
	saved_fileptr = bfd_tell(abfd);

	sz = H_GET_32(abfd, &f_hdr->strtab_size);
	data = bfd_malloc(sz);
	if (data == NULL)
		goto validate_fail;

	if (bfd_bread(data, sz, abfd) != sz) {
		free(data);
		goto validate_fail;
	}

	checksum = doff_checksum(data, sz);
	free(data);
	checksum += checksums->strtable_checksum;
	if (checksum != 0xFFFFFFFF)
		goto validate_fail;

	/* While we're here, read in all the section table and validate it */
	/* Also, tell coff about where it is */
	out->f_scnptr = bfd_tell(abfd);
	sz = sizeof(struct doff_scnhdr) * out->f_nscns;
	data = bfd_malloc(sz);
	if (data == NULL)
		memset(out, 0, sizeof(*out));

	if (bfd_bread(data, sz, abfd) != sz) {
		free(data);
		memset(out, 0, sizeof(*out));
		goto validate_fail;
	}

	checksum = doff_checksum(data, sz);
	free(data);
	checksum += checksums->section_checksum;
	if (checksum != 0xFFFFFFFF)
		goto validate_fail;

	/* Validate symbol table too... */
	sz = sizeof(struct doff_symbol) * out->f_nsyms;
	data  = bfd_malloc(sz);
	if (data == NULL)
		goto validate_fail;

	if(bfd_bread(data, sz, abfd) != sz) {
		free(data);
		goto validate_fail;
	}

	checksum = doff_checksum(data, sz);
	free(data);
	checksum += checksums->symbol_checksum;
	if (checksum != 0xFFFFFFFF)
		goto validate_fail;

	/* Success */
	bfd_seek(abfd, saved_fileptr, SEEK_SET);
	return;

	validate_fail:
	memset(out, 0, sizeof(*out));
	bfd_seek(abfd, saved_fileptr, SEEK_SET);
	return;
}

void
doff_regurgitate_reloc(bfd *abfd, asection *sect, unsigned int idx,
					struct internal_reloc *dst)
{
	struct doff_internal_sectdata *doff_tdata;
	struct doff_internal_reloc *reloc;

	doff_tdata = doff_get_internal_sectdata(abfd, sect, read_direction);

	if (idx >= doff_tdata->num_relocs) {
		fprintf(stderr, "Invalid reloc index %d\n", idx);
		memset(dst, 0, sizeof(*dst));
		return;
	}

	reloc = doff_tdata->relocs + idx;
	dst->r_vaddr = reloc->vaddr;
	dst->r_symndx = reloc->symidx;
	dst->r_type = reloc->type;
	dst->r_size = 0;
	dst->r_extern = 0;
	dst->r_offset = 0;
	return;
}

bfd_boolean
doff_get_section_contents(bfd *abfd, asection *sect, void *data,
					file_ptr offs, bfd_size_type size)
{
	struct doff_internal_sectdata *doff_tdata;
	void *src_data;

	doff_tdata = doff_get_internal_sectdata(abfd, sect, read_direction);
	if (doff_tdata == NULL)
		return FALSE;

	/* Basic validation */
	if (offs >= doff_tdata->size || offs + size > doff_tdata->size) {
		bfd_set_error(bfd_error_invalid_operation);
		return FALSE;
	}

	/* We buffer absolutely everything we can in memory in terms of section
	 * contents, and then actually beat everything into shape at the last
	 * possible moment. */

	src_data = doff_tdata->raw_data + offs;
	memcpy(data, src_data, size);
	return TRUE;
}

bfd_boolean
doff_set_section_contents(bfd *abfd, asection *sect, const void *data,
					file_ptr offs, bfd_size_type size)
{
	struct doff_internal_sectdata *doff_tdata;
	void *dst_data;

	doff_tdata = doff_get_internal_sectdata(abfd, sect, write_direction);
	if (doff_tdata == NULL)
		return FALSE;

	/* Basic validation */
	if (offs >= doff_tdata->size || offs + size > doff_tdata->size) {
		bfd_set_error(bfd_error_invalid_operation);
		return FALSE;
	}

	/* We buffer absolutely everything we can in memory in terms of section
	 * contents, and then actually beat everything into shape at the last
	 * possible moment. */

	dst_data = doff_tdata->raw_data + offs;
	memcpy(dst_data, data, size);
	return TRUE;
}

bfd_boolean
doff_index_str_table(bfd *abfd ATTRIBUTE_UNUSED, struct doff_private_data *priv)
{
	char **str_idx_table, *str_table;
	int bytes_left, i, sz, maxnum;

	maxnum = 100;
	str_idx_table = bfd_malloc(100 * sizeof(char*));
	priv->str_idx_table = str_idx_table;
	if (str_idx_table == NULL)
		return TRUE;

	str_table = priv->str_table;
	bytes_left = priv->str_sz;
	bytes_left--; /* We inject a null byte at the end anyway */

	sz = strlen(str_table);
	bytes_left -= sz;
	for (i = 0; bytes_left > 0; i++) {
		if (i >= maxnum) {
			str_idx_table = realloc(str_idx_table,
						sizeof(char*) * (maxnum + 100));
			if (str_idx_table == NULL) {
				free(priv->str_idx_table);
				return TRUE;
			}

			priv->str_idx_table = str_idx_table;
			maxnum += 100;
		}

		str_idx_table[i] = str_table;

		str_table += sz + 1; /* Next string, including null byte */
		if (bytes_left == 0)
			break;

		sz = strlen(str_table);
		bytes_left -= sz + 1;
	}

	priv->num_strs = i;
	return FALSE;
}

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
struct doff_internal_sectdata *doff_internalise_sectiondata(bfd *abfd,
				bfd_size_type sect_size,
				file_ptr sect_offset)
{
	struct doff_image_packet pkt;
	struct doff_reloc rel;
	struct doff_internal_reloc *reloc;
	struct doff_internal_sectdata *record;
	void *data;
	unsigned int sz_read, pkt_sz, pkt_relocs, i;
	uint32_t checksum;

	record = bfd_malloc(sizeof(*record));
	if (record == NULL)
		return NULL;

	data = bfd_malloc(sect_size);
	if (data == NULL) {
		free(record);
		return NULL;
	}

	reloc = bfd_malloc(sizeof(struct doff_internal_reloc) * 100);
	if (reloc == NULL) {
		free(data);
		free(record);
		return NULL;
	}

	record->raw_data = data;
	record->size = sect_size;
	record->num_relocs = 0;
	record->max_num_relocs = 100;
	record->relocs = reloc;
	
	bfd_seek(abfd, sect_offset, SEEK_SET);

	/* In theory we need the number of packets, but we know the size, and
	 * can just read until we have enough. Exactly why the num_pkts field
	 * exists, I do not know */

	for (sz_read = 0; sz_read < sect_size; ) {
		/* Get header */
		if (bfd_bread(&pkt, sizeof(pkt), abfd) != sizeof(pkt))
			goto fail;

		checksum = doff_checksum(&pkt, sizeof(pkt));
		pkt_sz = H_GET_32(abfd, &pkt.packet_sz);

		if (pkt_sz + sz_read > sect_size) {
			fprintf(stderr, "Unexpected excess data in section\n");
			goto fail;
		}

		if (bfd_bread(data, pkt_sz, abfd) != pkt_sz)
			goto fail;

		checksum += doff_checksum(data, pkt_sz);
		data += pkt_sz;
		sz_read += pkt_sz;

		pkt_relocs = H_GET_32(abfd, &pkt.num_relocs);
		if (record->num_relocs + pkt_relocs > record->max_num_relocs) {
			record->max_num_relocs += MAX(100, pkt_relocs);
			reloc = realloc(record->relocs,
					record->max_num_relocs *
					sizeof(struct doff_internal_reloc));
			if (reloc == NULL)
				goto fail;
			record->relocs = reloc;
			reloc += record->num_relocs;
		}

		for (i = 0; i < pkt_relocs; i++) {
			if (bfd_bread(&rel, sizeof(rel), abfd) != sizeof(rel))
				goto fail;

			checksum += doff_checksum(&rel, sizeof(rel));

			reloc->vaddr = H_GET_32(abfd, &rel.vaddr);
			reloc->vaddr += (sz_read - pkt_sz);
			reloc->type = H_GET_16(abfd, &rel.reloc.r_sym.type);
			reloc->symidx = H_GET_32(abfd,&rel.reloc.r_sym.sym_idx);

			reloc++;
			record->num_relocs++;
		}

		if (checksum != 0xFFFFFFFF) {
			fprintf(stderr, "Instruction packet failed checksum\n");
			goto fail;
		}
	}

	/* Due to the way we're doing things, there isn't actually a case where
	 * we don't read enough data */
	return record;

	fail:
	free(record->raw_data);
	free(record->relocs);
	free(record);
	return NULL;
}

struct doff_internal_sectdata *
doff_blank_sectiondata(bfd *abfd ATTRIBUTE_UNUSED, bfd_size_type size)
{
	struct doff_internal_sectdata *data;

	data = bfd_malloc(sizeof(struct doff_internal_sectdata));
	if (data == NULL)
		return NULL;

	data->raw_data= bfd_malloc(size);
	if (data->raw_data == NULL) {
		free(data);
		return NULL;
	}

	memset(data->raw_data, 0, size);
	data->size = size;
	data->num_relocs = 0;
	data->max_num_relocs = 0;
	data->relocs = NULL;
	return data;
}

void
doff_free_internal_sectiondata(struct doff_internal_sectdata *data)
{

	free(data->raw_data);
	free(data->relocs);
	free(data);
	return;
}

static int
compare_arelent_ptr(const void *a, const void *b)
{
	const arelent **aa = (const arelent **) a;
	const arelent **bb = (const arelent **) b;
	bfd_size_type aaddr = (*aa)->address;
	bfd_size_type baddr = (*bb)->address;

	return (aaddr < baddr ? -1 : baddr < aaddr ? 1 : 0);
}

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
bfd_boolean
doff_externalise_section_data(asection *curscn, struct scn_swapout *output)
{
	bfd *abfd;
	arelent **rels;
	asymbol *sym;
	struct doff_image_packet *ipkt;
	struct doff_reloc *reloc;
	struct doff_internal_sectdata *doff_tdata;
	void *cur_pos;
	uint32_t checksum;
	unsigned int max_sz, sz, cur_data_offs, num_relocs, reloc_idx, i, idx;
	unsigned int put_sz, num_relocs_written;

	rels = NULL;
	output->raw_scn_data = NULL;
	output->num_ipkts = 0;
	output->raw_data_sz = 0;
	cur_data_offs = 0;
	reloc_idx = 0;
	abfd = curscn->owner;

	doff_tdata = doff_get_internal_sectdata(curscn->owner, curscn,
						write_direction);
	if (doff_tdata == NULL)
		return TRUE;

	/* Calculate maximum size - raw data, reloc structs, and image pkt
	 * header structs on the assumption that minimum packet size is 1024 */
	max_sz = doff_tdata->size +
			(curscn->reloc_count * sizeof(struct doff_reloc))
					+ (((doff_tdata->size/1024) + 1) *
					sizeof(struct doff_image_packet));

	output->raw_scn_data = bfd_malloc(max_sz);
	cur_pos = output->raw_scn_data;
	if (cur_pos == NULL)
		return TRUE;

	/* Grab our own copy of relocations and order them by addr */
	sz = sizeof(arelent*) * curscn->reloc_count;
	if (sz != 0) {
		rels = bfd_malloc(sz);
		if (rels == NULL) {
			free(cur_pos);
			return TRUE;
		}

		memcpy(rels, curscn->orelocation, sz);
		qsort(rels, curscn->reloc_count, sizeof(arelent*),
						compare_arelent_ptr);
	}

	/* Now loop through our blocks of data */
	while (cur_data_offs != curscn->size) {
		/* Packet header - num relocs, amount of data, and checksum */
		ipkt = cur_pos;
		put_sz = MIN(1024, (curscn->size - cur_data_offs));
		cur_pos += sizeof(struct doff_image_packet);

		/* Look to see how many relocs we need to work on - they're in
		 * order of vma, so go through list looking for those that are
		 * between the current idx and the end of this instruction
		 * packet, ie those in the packet */
		if (rels != NULL) {
			for (num_relocs = 0;
					(reloc_idx + num_relocs <
					curscn->reloc_count) &&
					(rels[reloc_idx + num_relocs]->address <
					curscn->vma + cur_data_offs + 1024);
					num_relocs++)
			;
		} else {
			num_relocs = 0;
		}

		/* Write part of ipkt header, we leave number of relocs until
		 * we discover how many we actually wrote out */
		H_PUT_32(abfd, put_sz, &ipkt->packet_sz);
		H_PUT_32(abfd, 0, &ipkt->checksum);

		/* Copy in our block of memory */
		memcpy(cur_pos, doff_tdata->raw_data + cur_data_offs, put_sz);
		checksum = doff_checksum(cur_pos, put_sz);
		cur_pos += put_sz;

		/* And write out relocations. Erk */
		num_relocs_written = 0;
		for (i = 0; i < num_relocs; i++) {
			reloc = cur_pos;
			memset(reloc, 0, sizeof(*reloc));

			/* don't write relocs that have in fact been fixed up */
			/* UPDATE: that is, PCR relocs. Loading a pointer to a
			 * function say, still needs to be relocated against
			 * .text and so forth */
			if ((*rels[reloc_idx + i]->sym_ptr_ptr)->
				section->output_section == curscn &&
				rels[reloc_idx + i]->howto->pc_relative)
			continue;

			num_relocs_written++;

			/* NB - the address field is the addr _within_ the
			 * image packet - we always write out 1024 sized pkts,
			 * so we can just mask out the top portion of the addr*/
			H_PUT_32(abfd, rels[reloc_idx + i]->address & 1023,
						&reloc->vaddr);

			/* What kind of relocation? There are a bunch of formats
			 * that TI have, based on what the type field is. For
			 * the moment, afaik, we can just rely on having a
			 * symbol based reloc for everything */

			H_PUT_16(abfd, rels[reloc_idx + i]->howto->type,
						&reloc->reloc.r_sym.type);

			/* Symbol index - we assume that coff_renumber_symbols
			 * has already run and make our lives a lot easier */


			idx = 0;
			sym = *rels[reloc_idx + i]->sym_ptr_ptr;
			if (sym->the_bfd != abfd) {
				/* See coff_write_relocs - this was unresolved,
				 * and didn't have its symbol location
				 * rejiggled at the right time. So it points
				 * into the wrong bfd, so it's the wrong symbol
				 * to get an index from. Lookup in output bfd
				 * symbol table */
				for (idx = 0; idx < abfd->symcount; idx++) {
					if (!strcmp(sym->name,
						abfd->outsymbols[idx]->name))
						break;
				}

				if (idx == abfd->symcount) {
					fprintf(stderr, "doff backend: when "
						"writing file, relocation has "
						"magically gained a nonexistant"
						" symbol\n");
					exit(1);
					/* XXX - correct error handing */
				}

				idx = abfd->outsymbols[idx]->udata.i;
			} else {
				idx = sym->udata.i;
			}

			H_PUT_16(abfd, idx, &reloc->reloc.r_sym.sym_idx);

			/* Evaluate checksum on that */
			checksum += doff_checksum(reloc, sizeof(*reloc));
			cur_pos += sizeof(*reloc);
		}

		reloc_idx += i; /* Move past relocs we just wrote out */

		/* Update header with num relocs, update checksum */
		H_PUT_32(abfd, num_relocs_written, &ipkt->num_relocs);
		checksum += doff_checksum(ipkt, sizeof(*ipkt));

		/* Mkay, thats the entire packet done, write back checksum */
		H_PUT_32(abfd, (0xFFFFFFFF - checksum), &ipkt->checksum);

		output->num_ipkts++;
		cur_data_offs += put_sz;
	}

	output->raw_data_sz = (int)(cur_pos - output->raw_scn_data);

	/* Ok, I think we're done. Ish. */
	return FALSE;
}

/* The big cheese: where we actually perform some doff content writing */
bfd_boolean
doff_write_object_contents(bfd *abfd)
{
	struct doff_filehdr fhdr;
	struct doff_checksum_rec checksums;
	asection *curscn;
	asymbol *sym;
	struct scn_swapout *raw_scns, *cur_raw_scn;
	struct doff_symbol *dsym, *dsymbols;
	coff_symbol_type *coff_sym;
	void *tmp_ptr;
	char *str_block, *str_block_pos, *file_name;
	int dummy;
	uint32_t checksum;
	unsigned int nscns, max_scn_data, str_block_len, max_str_sz, tmp, i;
	unsigned int scn_name_sz, largest_str, num_loadable_scns, entry_scn;
	uint16_t flags, sclass;

	num_loadable_scns = 0;
	entry_scn = 0xFFFFFFFF;
	str_block = NULL;
	raw_scns = NULL;
	dsymbols = NULL;

	/* Construct string table as we walk through things - means no time
	 * glaring at the string table to work out what index we need. */
	largest_str = 0;
	max_str_sz = 0x2000;
	str_block_len = 0;
	str_block = bfd_malloc(max_str_sz);
	str_block_pos = str_block;
	if (str_block == NULL)
		return FALSE;

	/* Start off by inserting the source file name. Which is a rather
	 * pointless feature of doff that doesn't map very well onto what
	 * ld does. If you're going to have a silly field, I'll fill it with
	 * a silly value */
	file_name = "myfaceisonfire.o";
	strcpy(str_block_pos, file_name);
	str_block_pos += strlen(file_name) + 1;
	str_block_len += strlen(file_name) + 1;

	/* This munges the symbol table and works out where symbols will
	 * eventually lie in the symbol table (in terms of indexes) */
	coff_renumber_symbols(abfd, &dummy);
	coff_mangle_symbols(abfd);

	max_scn_data = 10;
	raw_scns = bfd_malloc(sizeof(*raw_scns) * max_scn_data);
	if (raw_scns == NULL)
		return FALSE;

	/* Loop through all sections, converting raw contents to the benighted
	 * doff format (packets with relocs squirted in) */
	for (nscns = 0, curscn = abfd->sections; curscn != NULL;
					curscn = curscn->next, nscns++) {
		/* Ensure our scn data array is large enough */
		if (nscns >= max_scn_data) {
			max_scn_data += 10;
			tmp_ptr = bfd_realloc(raw_scns, sizeof(*raw_scns) *
								max_scn_data);
			if (tmp_ptr == NULL)
				goto fail;

			raw_scns = tmp_ptr;
		}

		/* Append section name into string table */
		if (strlen(curscn->name) + str_block_len >= max_str_sz) {
			max_str_sz += 0x2000;
			tmp_ptr = bfd_realloc(str_block, max_str_sz);
			if (tmp_ptr == NULL)
				goto fail;

			str_block = tmp_ptr;
			str_block_pos = str_block + str_block_len;
		}
		tmp = str_block_len;
		strcpy(str_block_pos, curscn->name);
		str_block_pos += strlen(curscn->name) + 1;
		str_block_len += strlen(curscn->name) + 1;
		largest_str = MAX(strlen(curscn->name), largest_str);

		/* Externalise section contents */
		cur_raw_scn = raw_scns + nscns;
		if (doff_externalise_section_data(curscn, cur_raw_scn))
			goto fail;

		/* Throw together section header details too - those we have */
		/* Name index in string table, which we just grabbed */
		H_PUT_32(abfd, tmp, &cur_raw_scn->hdr.str_offset);
		/* "prog" address - this doesn't appear to be as simple as the
		 * the name suggests, as many symbols have their address
		 * adjusted by this value in TIs doff loader - see
		 * dynload/cload.c, dload_symbols in the dspbridge driver.
		 * So for now make it the same as the load address */
		H_PUT_32(abfd, curscn->vma, &cur_raw_scn->hdr.prog_addr);
		H_PUT_32(abfd, curscn->vma, &cur_raw_scn->hdr.load_addr);
		H_PUT_32(abfd, curscn->size, &cur_raw_scn->hdr.size);
		H_PUT_16(abfd, 0, &cur_raw_scn->hdr.page); /* No idea */
		H_PUT_32(abfd, cur_raw_scn->num_ipkts,
					&cur_raw_scn->hdr.num_pkts);

		/* Generate some flags */
		flags = 0;
		if (curscn->flags & SEC_CODE)
			flags |= DOFF_SCN_TYPE_TEXT;
		else if (curscn->flags & SEC_DATA)
			flags |= DOFF_SCN_TYPE_DATA;
		else if (!(curscn->flags & SEC_LOAD))
			flags |= DOFF_SCN_TYPE_BSS;
		/* And we'll never generate any CINIT stuff, I think */

		if (curscn->flags & SEC_ALLOC)
			flags |= DOFF_SCN_FLAG_ALLOC;
		if (curscn->flags & SEC_LOAD) {
			flags |= DOFF_SCN_FLAG_DOWNLOAD;
			num_loadable_scns++;
		}

		/* For the hell of it, align to 4096 byte pages. Dunno what the
		 * c64x mmu can handle though */
		flags |= 12 << DOFF_SCN_FLAGS_ALIGN_SHIFT;
		H_PUT_16(abfd, flags, &cur_raw_scn->hdr.flags);

		/* We don't have the file position yet */
		H_PUT_32(abfd, 0, &cur_raw_scn->hdr.first_pkt_offset);
	}

	/* Store length of section string table for file hdr */
	scn_name_sz = str_block_len;

	/* Ok, we now have almost all of our data tied down, save juggling
	 * some offsets, the file headers and the symbol table */

	dsymbols = bfd_malloc(sizeof(*dsymbols) * abfd->symcount);
	if (dsymbols == NULL)
		goto fail;
	dsym = dsymbols;

	/* XXX - I assume that these are going to have been sorted at some
	 * point to match the indexing generated by coff_mangle_symbols */
	for (i = 0; i < abfd->symcount; i++) {
		sym = abfd->outsymbols[i];

		/* largely copy what coff_write_symbols does, and
		 * coff_write_alien_symbol */
		coff_sym = coff_symbol_from(abfd, sym);
		if (coff_sym == NULL || coff_sym->native == NULL) {
			/* Non-coff symbol, guess what storage class it should
			 * be in. Rules derived from doff_write_alien_symbol */
			if (sym->flags & BSF_LOCAL)
				sclass = C_STAT;
			else if (sym->flags & BSF_WEAK)
				sclass = C_WEAKEXT;
			else
				sclass = C_EXT;
			H_PUT_16(abfd, sclass, &dsym->storage_class);
		} else {
			H_PUT_16(abfd, coff_sym->native->u.syment.n_sclass,
						&dsym->storage_class);
		}

		/* Section index: bfd and doff are one-based, but check for
		 * cases where have an absolute symbol */
		if (sym->section == bfd_abs_section_ptr)
			H_PUT_16(abfd, -1, &dsym->scn_num);
		else if (sym->section == bfd_und_section_ptr)
			H_PUT_16(abfd, 0, &dsym->scn_num);
		else
			H_PUT_16(abfd, sym->section->target_index,
						&dsym->scn_num);

		H_PUT_32(abfd, sym->value + sym->section->output_offset,
							&dsym->value);

		/* Squirt string into string table too */
		if (strlen(sym->name) + str_block_len >= max_str_sz) {
			max_str_sz += 0x2000;
			tmp_ptr = bfd_realloc(str_block, max_str_sz);
			if (tmp_ptr == NULL)
				goto fail;

			str_block = tmp_ptr;
			str_block_pos = str_block + str_block_len;
		}
		tmp = str_block_len;
		strcpy(str_block_pos, sym->name);
		str_block_pos += strlen(sym->name) + 1;
		str_block_len += strlen(sym->name) + 1;
		largest_str = MAX(strlen(sym->name), largest_str);

		H_PUT_32(abfd, tmp, &dsym->str_offset);

		dsym++; /* Next symbol */
	}

	/* Ok, we now have all blocks of significant data, we now need to
	 * stitch it together with headers. First, lets round the string table
	 * up to four bytes. (We shouldn't need to worry about allocating more)
	 */
	if (str_block_len & 3) {
		tmp = str_block_len & 3;
		str_block_len &= ~3;
		str_block_len += 4;
		memset(str_block_pos, 0, tmp);
	}

	/* We need to know the offsets of section data - first size of headers*/
	tmp = sizeof(struct doff_filehdr) + sizeof(struct doff_checksum_rec);
	tmp += str_block_len;
	tmp += sizeof(struct doff_scnhdr) * nscns;
	tmp += sizeof(struct doff_symbol) * abfd->symcount;

	/* Next start fixing section positions */
	for (i = 0; i < nscns; i++) {
		H_PUT_32(abfd, tmp, &(raw_scns + i)->hdr.first_pkt_offset);
		tmp += (raw_scns + i)->raw_data_sz;
		/* Align in file up to 4 bytes */
		if (tmp & 3)
			tmp = (tmp + 4) & ~3;
	}

	/* Calculate header checksums */
	checksums.timestamp = 0; /* Nuts to that */
	checksums.self_checksum = 0;
	checksum = 0;
	for (i = 0; i < nscns; i++) {
		checksum += doff_checksum(&(raw_scns + i)->hdr,
						sizeof(struct doff_scnhdr));
	}
	H_PUT_32(abfd, 0xFFFFFFFF - checksum, &checksums.section_checksum);

	H_PUT_32(abfd, 0xFFFFFFFF - doff_checksum(str_block, str_block_len),
				&checksums.strtable_checksum);
	H_PUT_32(abfd, 0xFFFFFFFF - doff_checksum(dsymbols, sizeof(*dsymbols) *
				abfd->symcount), &checksums.symbol_checksum);
	H_PUT_32(abfd, 0xFFFFFFFF - doff_checksum(&checksums,sizeof(checksums)),
				&checksums.self_checksum);

	/* Right, now produce the main file header */
	H_PUT_32(abfd, str_block_len, &fhdr.strtab_size);
	H_PUT_32(abfd, abfd->start_address, &fhdr.entry_point);
	H_PUT_32(abfd, DOFF_BYTE_RESHUFFLE, &fhdr.byte_reshuffle);
	H_PUT_32(abfd, scn_name_sz, &fhdr.scn_name_sz);
	H_PUT_16(abfd, abfd->symcount, &fhdr.num_syms);
	H_PUT_16(abfd, largest_str, &fhdr.max_str_len);
	H_PUT_16(abfd, nscns, &fhdr.num_scns);
	H_PUT_16(abfd, num_loadable_scns, &fhdr.target_scns);
	H_PUT_16(abfd, 0, &fhdr.doff_version);
	/* XXX - need to pull this out of the backend specific data somehow */
	H_PUT_16(abfd, DOFF_PROC_TMS320C6000, &fhdr.target_id);
	/* XXX - the same */
	H_PUT_16(abfd, DOFF_LITTLE_ENDIAN, &fhdr.flags);
	if (entry_scn == 0xFFFFFFFF)
		H_PUT_16(abfd, DOFF_ENTRY_SCN_UNDEF, &fhdr.entry_scn);
	else
		H_PUT_16(abfd, entry_scn, &fhdr.entry_scn);

	/* Checksum all of that */
	H_PUT_32(abfd, 0, &fhdr.checksum);
	checksum = doff_checksum(&fhdr, sizeof(fhdr));
	H_PUT_32(abfd, ~checksum, &fhdr.checksum);

	/* Pump everything out */
	bfd_seek(abfd, 0, SEEK_SET);

	if (bfd_bwrite(&fhdr, sizeof(fhdr), abfd) != sizeof(fhdr))
		return FALSE;

	if (bfd_bwrite(&checksums, sizeof(checksums), abfd) !=sizeof(checksums))
		return FALSE;

	if (bfd_bwrite(str_block, str_block_len, abfd) != str_block_len)
		return FALSE;

	/* Dump section hdrs */
	for (i = 0; i < nscns; i++) {
		if (bfd_bwrite(&(raw_scns + i)->hdr, sizeof(struct doff_scnhdr),
					abfd) != sizeof(struct doff_scnhdr))
			return FALSE;
	}

	if (bfd_bwrite(dsymbols, sizeof(*dsymbols) * abfd->symcount, abfd) !=
					sizeof(*dsymbols) * abfd->symcount)
		return FALSE;

	/* Write section contents, 4 byte padded */
	checksum = 0; /* We'll re-use these 4 byte */
	for (i = 0; i < nscns; i++) {
		cur_raw_scn = (raw_scns + i);
		tmp = cur_raw_scn->raw_data_sz;
		if (bfd_bwrite(cur_raw_scn->raw_scn_data, tmp, abfd) != tmp)
			return FALSE;

		if ((tmp & 3) == 0)
			continue;

		tmp &= 3;
		tmp = 4 - tmp;
		if (bfd_bwrite(&checksum, tmp, abfd) != tmp)
			return FALSE;
	}

	/* Cleanup */
	free(str_block);
	free(dsymbols);
	for (i = 0; i < nscns; i++) {
		cur_raw_scn = (raw_scns + i);
		free(cur_raw_scn->raw_scn_data);
	}
	free(raw_scns);

	return TRUE;

	fail:
	if (str_block)
		free(str_block);
	if (dsymbols)
		free(dsymbols);
	if (raw_scns) {
		for (i = 0; i < nscns; i++) {
			cur_raw_scn = (raw_scns + i);
			free(cur_raw_scn->raw_scn_data);
		}
		free(raw_scns);
	}
	return FALSE;
}
