/* Following the bfd style, implementation code goes in this header, that then
 * gets included into implementation files with customising preprocessor foo.
 * Not that any other arch will use this */

#include "sysdep.h"
#include "bfd.h"
#include "bfdlink.h"
#include "libbfd.h"
#include <coff/internal.h>
#include <coff/doff.h>
#include "libcoff.h"
#include "libdoff.h"
/* inclusion protection please? #include "libbfd.h" */

#define UNUSED(x) ((x) = (x))

static uint32_t
doff_checksum(const void *data, unsigned int len)
{
	const uint32_t *d;
	uint32_t sum;
	int l;

	/* XXX - some assert we can fire if non-multiple-of-4-len given? */
	sum = 0;
	d = data;
	for (l = len; l > 0; l -= sizeof(uint32_t))
		sum += *d++;

	return sum;
}

static bfd_boolean
doff_internalise_strings(bfd *abfd, struct doff_tdata *tdata,
			const char *strings, unsigned int len)
{
	int bytes_left, i, sz, tmp;

	bytes_left = len;

	/* We have a bunch of strings that are used in file - There's nothing
	 * saying how many of them there are, so take a guess and realloc if we
	 * run out of slots */

	sz = bytes_left / 10;
	tdata->max_num_strings = sz;
	tdata->string_table = bfd_alloc(abfd, sz * sizeof(char *));
	tdata->string_idx_table = bfd_alloc(abfd, sz * sizeof(int));
	for (i = 0; bytes_left > 0; i++) {
		tmp = strlen(strings) + 1;
		if (tmp > bytes_left)
			break;

		tdata->string_table[i] = bfd_alloc(abfd, tmp);
		strncpy(tdata->string_table[i], strings, tmp);
		tdata->string_idx_table[i] = len - bytes_left;
		strings += tmp;
		bytes_left -= tmp;

		if (i >= tdata->max_num_strings) {
			sz += 100;
			tdata->max_num_strings = sz;
			tdata->string_table =
				bfd_realloc(tdata->string_table,
						sz * sizeof(char *));
			tdata->string_idx_table =
				bfd_realloc(tdata->string_idx_table,
						sz * sizeof(int));
		}
	}

	tdata->num_strings = i;
	if (bytes_left < 0)
		return TRUE;

	return FALSE;
}

static bfd_boolean
doff_internalise_symbols(bfd *abfd, void *data, struct doff_tdata *tdata)
{
	const struct doff_symbol *symbol;
	asymbol *sym;
	int i, j, tmp, idx;

	symbol = data;

	/* We don't yet have sections; patch that up later */
	tdata->symbols = bfd_zalloc(abfd, sizeof(void *) * tdata->num_syms);
	if (tdata->symbols == NULL)
		return TRUE;

	for (i = 0; i < tdata->num_syms; i++) {
		tdata->symbols[i] = (struct doff_symbol_internal *)
					doff_make_empty_symbol(abfd);
		sym = &tdata->symbols[i]->bfd_symbol;

		tmp = bfd_get_32(abfd, &symbol->str_offset);
		if (tmp != -1)
			for (j = 0; j < tdata->num_strings; j++)
				if (tdata->string_table[j])
					if (tdata->string_idx_table[j] == tmp)
						break;

		/* XXX - symbols that are -1 refer to current section */
		if (tmp != -1 && j == tdata->num_strings) {
			fprintf(stderr, "Unrecognized string index in symbol");
			bfd_set_error(bfd_error_bad_value);
			return TRUE;
		}

		sym->name = (tmp == -1) ? NULL : tdata->string_table[j];
		sym->value = bfd_get_32(abfd, &symbol->value);
		sym->flags = BSF_NO_FLAGS;	/* XXX - pain */

		tdata->symbols[i]->str_table_idx = j;
		idx = bfd_get_16(abfd,&symbol->scn_num);
		if (idx == 0) {
			sym->section = bfd_abs_section_ptr;
			symbol++;
			continue;
		} else if (idx == -1) {
			sym->section = bfd_und_section_ptr;
			symbol++;
			continue;
		}

		/* Section numbering starts from 1 in this field, decrement
		 * to fit with our zero based array */
		idx--;

		if (idx >= tdata->num_sections || idx < 0) {
			fprintf(stderr, "Section number out of range for symbol"
					" %d, section %d\n", i, idx);
			bfd_set_error(bfd_error_wrong_format);
			return TRUE;
		}

		sym->section = tdata->section_data[idx]->section;
		sym->value -= tdata->section_data[idx]->section->vma;

		symbol++;
	}

	return FALSE;
}

/* From tic64x; XXX XXX XXX, should go in backend data as function ptr */
extern reloc_howto_type *doff_rtype2howto(unsigned int reloc_type);

static bfd_boolean
doff_add_reloc(bfd *abfd, struct doff_section_data *sect, bfd_vma vaddr,
			int reloc_type, int addend)
{
	arelent *r;

	if (sect->relocs == NULL) {
		BFD_ASSERT(sect->num_relocs == 0);
		BFD_ASSERT(sect->max_num_relocs == 0);
		sect->relocs = bfd_alloc(abfd, sizeof(void *) * 100);
		if (sect->relocs == NULL)
			return TRUE;

		sect->num_relocs = 0;
		sect->max_num_relocs = 100;
	}

	if (sect->num_relocs == sect->max_num_relocs) {
		sect->relocs = bfd_realloc(sect->relocs,
				sizeof(void*) * (sect->num_relocs + 100));
		if (sect->relocs == NULL)
			return TRUE;
		sect->max_num_relocs += 100;
	}

	r = bfd_alloc(abfd, sizeof(arelent));
	if (!r)
		return TRUE;

	r->sym_ptr_ptr = NULL; /* It gets patched up later */
	r->address = vaddr;
	r->addend = addend;
	r->howto = doff_rtype2howto(reloc_type);
	if (r->howto == NULL) {
		fprintf(stderr, "Warning, unrecognized reloc type %X\n",
								reloc_type);
	} else {
		fprintf(stderr, "Saw reloc with r_type %X\n", reloc_type);
	}

	return FALSE;
}

static bfd_boolean
doff_load_raw_sect_data(bfd *abfd, struct doff_section_data *sect,
			off_t file_offset, size_t raw_size, int num_pkts)
{
	struct doff_image_packet fpkt;
	struct doff_reloc *relocs;
	asection *section;
	void *raw_data;
	uint32_t checksum;
	int i, j, sz, tmp, num_relocs, reloc_type, vaddr;

	/* The actual data in a section is cut up into instruction "packets",
	 * not execution packets but ~1024 byte chunks which I presume get
	 * pumped into the target processor. They start with a packet header,
	 * followed by the actual section data, then (I believe) reloc data.
	 * It's not written down, but I speculate this is only the case for
	 * stuff that actually get loaded onto the processor -> ie, debug data
	 * doesn't get packet headers */
	section = sect->section;

	if (!(sect->flags & DOFF_SCN_FLAG_DOWNLOAD) &&
				section->flags & SEC_HAS_CONTENTS) {
		/* Doesn't get downloaded to target, only raw data */
		raw_data = bfd_alloc(abfd, raw_size);
		if (raw_data == NULL)
			return TRUE;

		if (bfd_seek(abfd, file_offset, SEEK_SET)) {
			bfd_set_error(bfd_error_file_truncated);
			return TRUE;
		}

		if (bfd_bread(raw_data, raw_size, abfd) != raw_size)
			return TRUE;

		sect->num_relocs = 0;
		sect->max_num_relocs = 0;
		sect->relocs = NULL;
		return bfd_set_section_contents(abfd, section, raw_data,
					file_offset, raw_size);
	} else if (!(sect->flags & DOFF_SCN_FLAG_DOWNLOAD)) {
		/* We have something like .bss, no contents but a size */
		sect->num_relocs = 0;
		sect->max_num_relocs = 0;
		sect->relocs = NULL;

		section->filepos = 0;
		section->rel_filepos = 0;
		section->contents = NULL;
		section->rawsize = 0;
		section->size = raw_size;
		return FALSE;
	}

	raw_data = bfd_zalloc(abfd, raw_size);
	if (raw_data == NULL)
		return TRUE;

	section->contents = raw_data;

	/* Seek to actual section data */
	if (bfd_seek(abfd, file_offset, SEEK_SET)) {
		bfd_set_error(bfd_error_file_truncated);
		return TRUE;
	}

	for (i = 0; i < num_pkts; i++) {
		/* Read instruction packet header */
		if (bfd_bread(&fpkt, sizeof(fpkt), abfd) != sizeof(fpkt))
			return TRUE;

		sz = bfd_get_32(abfd, &fpkt.packet_sz);
		num_relocs = bfd_get_32(abfd, &fpkt.num_relocs);
		checksum = doff_checksum(&fpkt, sizeof(fpkt));

		/* Read actual packet data in */
		if (bfd_bread(raw_data, sz, abfd) != (unsigned int)sz)
			return TRUE;

		/* And collect its checksum */
		checksum += doff_checksum(raw_data, sz);

		if (num_relocs > 0) {
			tmp = sizeof(struct doff_reloc) * num_relocs;
			relocs = bfd_malloc(tmp);

			if (relocs == NULL)
				return TRUE;

			/* read reloc data from file */
			if (bfd_bread(relocs, tmp, abfd) != (unsigned int)tmp) {
				free(relocs);
				return TRUE;
			}

			/* Collect final checksum */
			checksum += doff_checksum(relocs, tmp);
		}

		/* Verify checksum is correct */
		if (~checksum != 0) {
			fprintf(stderr, "Bad checksum for insn packet %d in "
					"section %s\n", i, section->name);
			bfd_set_error(bfd_error_bad_value);
			return TRUE;
		}

		/* Loop through reloc structs, making our own */
		for (j = 0; j < num_relocs; j++) {
			vaddr = bfd_get_32(abfd, &relocs->vaddr);
			vaddr -= section->vma;
			reloc_type = bfd_get_16(abfd,&relocs->reloc.r_sym.type);
			/* XXX - addend */
			if (doff_add_reloc(abfd, sect, vaddr, reloc_type, 0)) {
				relocs -= j;
				free(relocs);
				return TRUE;
			}

			relocs++;
		}

		if (num_relocs > 0) {
			/* Correct reloc pointer for freeing */
			relocs -= num_relocs;
			free(relocs);
		}

		/* Increment raw_data ptr for where next pkt is copied in to */
		raw_data += sz;
	}

	/* End of reading ipkts */
	if (sect->num_relocs != 0) {
		abfd->flags |= HAS_RELOC;
		section->reloc_count = sect->num_relocs;
	}

	section->size = raw_size;
	section->rawsize = raw_size;
	return FALSE;
}

static bfd_boolean
doff_internalise_sections(bfd *abfd, const void *sec_data, int num_sections,
			struct doff_tdata *tdata)
{
	asection *section;
	const char *name;
	const struct doff_scnhdr *scn;
	struct doff_section_data *sect;
	size_t raw_size;
	off_t file_offset;
	int i, j, stroffset, tmp, num_pkts;

	scn = sec_data;
	for (i = 0; i < num_sections; i++) {
		/* Find index of section name in str table */
		stroffset = bfd_get_32(abfd, &scn->str_offset);
		for (j = 0; j < tdata->num_strings; j++)
			if (tdata->string_idx_table[j] == stroffset)
				break;

		name = (j == -1) ? "" : tdata->string_table[j];

		section = bfd_make_section_anyway(abfd, name);
		sect = section->used_by_bfd;

		/* Read other fields */
		sect->flags = bfd_get_16(abfd, &scn->flags);
		raw_size = bfd_get_32(abfd, &scn->size);
		file_offset = bfd_get_32(abfd, &scn->first_pkt_offset);
		num_pkts = bfd_get_32(abfd, &scn->num_pkts);

		if (!bfd_set_section_vma(abfd, sect->section,
					bfd_get_32(abfd, &scn->load_addr)))
			return TRUE;

		if (!bfd_set_section_size(abfd, sect->section, raw_size))
			return TRUE;

		tmp = (sect->flags & DOFF_SCN_FLAG_ALIGN) >> 8;
		if (!bfd_set_section_alignment(abfd, sect->section, tmp))
			return TRUE;

		tmp = SEC_IN_MEMORY; /* We always read everything in */
		if (sect->flags & DOFF_SCN_FLAG_ALLOC)
			tmp |= SEC_ALLOC;
		if (sect->flags & DOFF_SCN_FLAG_DOWNLOAD)
			tmp |= SEC_LOAD;
		if (sect->num_relocs != 0)
			tmp |= SEC_RELOC;
		/* XXX - no readonly flag defined */
		if ((sect->flags & DOFF_SCN_FLAG_TYPE_MASK)==DOFF_SCN_TYPE_TEXT)
			tmp |= SEC_CODE;
		if ((sect->flags & DOFF_SCN_FLAG_TYPE_MASK)==DOFF_SCN_TYPE_DATA)
			tmp |= SEC_DATA;
		/* XXX - constructors, have a type, but I don't fully understand
		 * what goes on here */
		if (raw_size != 0)
			tmp |= SEC_HAS_CONTENTS;
		/* XXX other flags I'm not interested in exist; only one of
		 * particular importance is debugging, for which doff doesn't
		 * have a flag. Also, we could in theory represent the string
		 * table as a section (possibly a bad plan given it has section
		 * names encoded */
		if (!bfd_set_section_flags(abfd, sect->section, tmp))
			return TRUE;

		doff_load_raw_sect_data(abfd, sect, file_offset, raw_size,
								num_pkts);

		scn++;
	}

	return FALSE;
}

const bfd_target *
doff_object_p(bfd *abfd)
{
	struct bfd_preserve preserve;
	struct doff_filehdr d_hdr;
	struct doff_checksum_rec checksums;
	file_ptr saved_pos;
	struct doff_tdata *tdata;
	const bfd_target *target;
	void *data;
	unsigned int size, target_id;
	int num_sections;

	preserve.marker = NULL;
	target = abfd->xvec;

	if (bfd_bread(&d_hdr, sizeof(d_hdr), abfd) != sizeof(d_hdr)) {
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	/* At this point we're supposed to check what kind of file this is.
	 * However, TI didn't deign it necessary to include a magic number in
	 * their file format, so we have to make guesses and conjecture: using
	 * the "byte reshuffle" field, we see it:
	 *"identifies byte ordering of file; always set to BYTE_RESHUFFLE_VALUE"
	 */

	if (bfd_get_32(abfd, &d_hdr.byte_reshuffle) != DOFF_BYTE_RESHUFFLE)
		goto wrong_format;

	if (~doff_checksum(&d_hdr, sizeof(d_hdr))) {
		/* XXX - no erorr code for "bad checksum" or the like? */
		fprintf(stderr, "doff backend: file matches, bad checksum\n");
		goto wrong_format;
	}

	/* That gives us some light assurance. It also ensures that the byte
	 * ordering matches the target, whos get_32 we used. Now try parsing
	 * rest of file */
	if (!bfd_preserve_save(abfd, &preserve))
		goto unwind;

	/* Create something to store our info in */
	if (!(target->_bfd_set_format[bfd_object](abfd)))
		goto unwind;

	preserve.marker = abfd->tdata.doff_obj_data;
	tdata = abfd->tdata.doff_obj_data;

	if (bfd_bread(&checksums, sizeof(checksums), abfd)
					 != sizeof(checksums)) {
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	/* So, there's a checksum of checksum field */
	if (~doff_checksum(&checksums, sizeof(checksums))) {
		fprintf(stderr, "doff backend: bad checksum table\n");
		goto wrong_format;
	}

	/* The string table follows the file header */
	size = bfd_get_32(abfd, &d_hdr.strtab_size);
	if (size > 0x00200000) {
		/* Stupidly sized string table */
		fprintf(stderr, "doff backend: oversized string table\n");
		goto wrong_format;
	} else if (size == 0) {
		fprintf(stderr, "doff file with zero sized string table\n");
		goto wrong_format;
	}

	data = bfd_malloc(size);
	if (!data) {
		bfd_set_error(bfd_error_no_memory);
		goto unwind;
	}

	if (bfd_bread(data, size, abfd) != size) {
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	if (~(doff_checksum(data, size) + checksums.strtable_checksum)) {
		fprintf(stderr, "doff backend: bad string table checksum\n");
		goto wrong_format;
	}

	/* We have a big table of strings. The first is the originating file
	 * name, followed by section names, followed by normal strings */
	if (doff_internalise_strings(abfd, tdata, data, size)) {
		free(data);
		goto wrong_format;
	}
	free(data);

	/* Now read section table - it's immediately after string table */
	tdata->section_data = NULL;
	tdata->num_sections = 0;
	num_sections = (uint16_t)bfd_get_16(abfd, &d_hdr.num_scns);
	if (num_sections > 0x1000) {
		fprintf(stderr, "doff backend: oversized section num\n");
		goto wrong_format;
	} else if (num_sections == 0) {
		fprintf(stderr, "doff file with zero sections\n");
		goto wrong_format;
	}

	size = num_sections * sizeof(struct doff_scnhdr);
	data = bfd_malloc(size);
	if (!data) {
		bfd_set_error(bfd_error_no_memory);
		goto unwind;
	}

	if (bfd_bread(data, size, abfd) != size) {
		free(data);
		if (bfd_get_error() != bfd_error_system_call)
			goto wrong_format;
		else
			goto unwind;
	}

	if (~(doff_checksum(data, size) + checksums.section_checksum)) {
		fprintf(stderr, "doff backend: bad section table checksum\n");
		free(data);
		goto wrong_format;
	}

	/* Save position of end of section table, we need it for reading symbols
	 * later on, and internalise_sections can wander around the file */
	saved_pos = bfd_tell(abfd);

	/* churn through sections */
	if (doff_internalise_sections(abfd, data, num_sections, tdata)) {
		free(data);
		goto unwind;
	}
	free(data);

	/* Read symbols - if there are any. */
	tdata->num_syms = (uint16_t)bfd_get_16(abfd, &d_hdr.num_syms);
	if (tdata->num_syms != 0) {
		abfd->flags |= HAS_SYMS;
		bfd_seek(abfd, saved_pos, SEEK_SET);
		size = sizeof(struct doff_symbol) * tdata->num_syms;
		data = bfd_malloc(size);
		if (data == NULL) {
			goto unwind;
		}

		if (bfd_bread(data, size, abfd) != (unsigned int)size) {
			free(data);
			if (bfd_get_error() != bfd_error_system_call)
				goto wrong_format;
			else
				goto unwind;
		}

		if (~(doff_checksum(data, size) + checksums.symbol_checksum)) {
			fprintf(stderr, "doff backend: bad symbol table "
					"checksum\n");
			free(data);
			goto wrong_format;
		}

		if (doff_internalise_symbols(abfd, data, tdata)) {
			free(data);
			goto wrong_format;
		}
		free(data);
	}

	bfd_set_start_address(abfd, bfd_get_32(abfd, &d_hdr.entry_point));
	/* XXX - how to tell if it's directly executable? No file flag */
	abfd->flags |= EXEC_P;

	target_id = bfd_get_16(abfd, &d_hdr.target_id);
	switch (target_id) {
	case DOFF_PROC_TMS320C6000:
		bfd_default_set_arch_mach(abfd, bfd_arch_tic64x, 0);
		break;
	default:
		fprintf(stderr, "Parsed file but found processor type %X: "
				"chances are it won't work until doff is "
				"cleaned up to support byte != 8 bits\n",
				target_id);
		goto wrong_format;
	}
	bfd_preserve_finish(abfd, &preserve);
	return target;

	wrong_format:
	bfd_set_error(bfd_error_wrong_format);

	unwind:
	if (preserve.marker != NULL) {
		/* As it happens, bfd_releasing the marker, aka the tdata,
		 * will also release anything else allocated after that; so all
		 * the string data, all the section data, is automagically
		 * released by this call.
		 * XXX - do we need to kill/release asection records? */
		bfd_preserve_restore(abfd, &preserve);
	}
	return NULL;
}

bfd_boolean
doff_mkobject(bfd *abfd)
{

	abfd->tdata.doff_obj_data = bfd_zalloc(abfd, sizeof(struct doff_tdata));
	if (abfd->tdata.doff_obj_data == NULL)
		return FALSE;

	abfd->tdata.doff_obj_data->the_bfd = abfd;
	return TRUE;
}

static bfd_boolean
doff_ingest_link_order(bfd *abfd, asection *out_sect, asection * in_sect,
			struct bfd_link_order *lo, struct doff_tdata *tdata)
{
	asymbol **symtab;
	arelent **relocs;
	void *data;
	long symtab_size, reloc_size;

	data = NULL;
	symtab = NULL;
	relocs = NULL;
	symtab_size = bfd_get_symtab_upper_bound(in_sect->owner);
	reloc_size = bfd_get_reloc_upper_bound(in_sect->owner, in_sect);

	if (symtab_size != 0) {
		symtab = bfd_malloc(symtab_size * sizeof(void *));
		goto unwind;

		symtab_size = bfd_canonicalize_symtab(in_sect->owner, symtab);
		if (symtab_size == 0)
			goto unwind;
	}

	if (reloc_size != 0) {
		relocs = bfd_malloc(reloc_size * sizeof(void *));
		if (relocs == NULL)
			goto unwind;

		reloc_size = bfd_canonicalize_reloc(in_sect->owner, in_sect,
							relocs, symtab);
		if (reloc_size == 0)
			goto unwind;
	}

	/* See how (it seems) we copy the entire section, we don't need to
	 * select certain relocations, but just use all from that section,
	 * which we've already canonicalized */

	data = bfd_malloc(lo->size);
	if (!data)
		goto unwind;

	if (!bfd_get_section_contents(abfd, in_sect, data, 0, lo->size))
		goto unwind;

	if (!bfd_set_section_contents(abfd, out_sect, data, lo->offset,
							lo->size))
		goto unwind;

	/* ENOTYET */
	UNUSED(tdata);
	abort();

	unwind:
	if (data)
		free(data);
	if (symtab)
		free(symtab);
	if (relocs)
		free(relocs);
	return TRUE;
}

static void
doff_ingest_section(bfd *abfd, asection *sect, void *t)
{
	struct bfd_link_order *lo;
	struct doff_tdata *tdata;

	tdata = t;
	for (lo = sect->map_head.link_order; lo != NULL; lo = lo->next) {
		switch (lo->type) {
		case bfd_indirect_link_order:
			doff_ingest_link_order(abfd, sect,
					lo->u.indirect.section, lo, tdata);
			break;


		case bfd_undefined_link_order:
		case bfd_data_link_order:
		case bfd_section_reloc_link_order:
		case bfd_symbol_reloc_link_order:
			fprintf(stderr, "Can't cope with link order type of "
					"%d in doff\n", lo->type);
			abort();
		}
	}

	UNUSED(abfd);

	return;
}

bfd_boolean
doff_write_object_contents(bfd *abfd)
{

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_write_object_contents\n");
	abort();
}

bfd_boolean
doff_close_and_cleanup(bfd *abfd)
{
	struct doff_tdata *tdata;

	/* bfd_release released tdata and everything allocated afterwards -
	 * that includes string and section data. XXX - is this reentrant?
	 * also, what about asymbols and asections? */
	tdata = abfd->tdata.doff_obj_data;
	if (tdata->section_data != NULL)
		free(tdata->section_data);

	bfd_release(abfd, abfd->tdata.doff_obj_data);
	return _bfd_generic_close_and_cleanup(abfd);
}

bfd_boolean
doff_bfd_free_cached_info(bfd *abfd)
{

	UNUSED(abfd);
	fprintf(stderr, "Implement doff_bfd_free_cached_info");
	abort();
}

bfd_boolean
doff_new_section_hook(bfd *abfd, sec_ptr section)
{
	struct doff_section_data *sect;
	struct doff_tdata *tdata;
	void *data;
	int num_sects;

	tdata = abfd->tdata.doff_obj_data;
	num_sects = tdata->num_sections + 1;

	if (tdata->section_data == NULL) {
		BFD_ASSERT(num_sects == 1);
		data = bfd_malloc(sizeof(void *));
	} else {
		data = bfd_realloc(tdata->section_data,
					(num_sects + 1) * sizeof (void *));
		if (data == NULL)
			return FALSE;
	}

	tdata->section_data = data;
	tdata->num_sections = num_sects;

	sect = bfd_alloc(abfd, sizeof(struct doff_section_data));
	if (!sect) {
		tdata->num_sections--;
		return FALSE;
	}

	tdata->section_data[num_sects - 1] = sect;

	/* Now, it's becoming obvious that doff doesn't have anything
	 * sufficiently complicated or "rich" enough to live outside of the
	 * canonical bfd format, so we can probably start moving away from using
	 * these internal data structs. For now, fill stuff out with blanks */
	sect->flags = 0;
	sect->section = section;
	sect->num_relocs = 0;
	sect->max_num_relocs = 0;
	sect->relocs = NULL;

	section->used_by_bfd = sect;

	return _bfd_generic_new_section_hook(abfd, section);
}

bfd_boolean
doff_get_section_contents(bfd *abfd ATTRIBUTE_UNUSED,
			sec_ptr section ATTRIBUTE_UNUSED,
			void *data ATTRIBUTE_UNUSED,
			file_ptr offset ATTRIBUTE_UNUSED,
			bfd_size_type size ATTRIBUTE_UNUSED)
{

	abort();
}

bfd_boolean
doff_get_section_contents_in_window(bfd *abfd, sec_ptr section,
			bfd_window *window, file_ptr file, bfd_size_type size)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(window);
	UNUSED(file);
	UNUSED(size);
	fprintf(stderr, "Implement doff_bfd_get_section_contents_in_window");
	abort();
}

bfd_boolean
doff_bfd_copy_private_bfd_data(bfd *abfd, bfd *another_bfd)
{

	UNUSED(abfd);
	UNUSED(another_bfd);
	fprintf(stderr, "Implement doff_bfd_copy_private_bfd_data");
	abort();
}

bfd_boolean
doff_bfd_merge_private_bfd_data(bfd *abfd ATTRIBUTE_UNUSED,
				bfd *another_bfd ATTRIBUTE_UNUSED)
{

	/* On the whole when we're linking, the other bfd is going to be coff
	 * for which we probably should be stiring the internals. If the other
	 * bfd is doff, we still don't care, there's no special data behind
	 * it, save perhaps the source file name */
	/* XXX - save source file name... */
	return TRUE;
}

#if 0
BFD_JUMP_TABLE_COPY initializer declines to use this
bfd_boolean
doff_bfd_init_private_section_data(bfd *abfd, sec_ptr section, bfd *another_bfd,
			sec_ptr another_section, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(another_bfd);
	UNUSED(another_section);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_init_section_private_data");
	abort();
}
#endif

bfd_boolean
doff_bfd_copy_private_section_data(bfd *abfd, sec_ptr section, bfd *another_bfd,
			sec_ptr another_section)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(another_bfd);
	UNUSED(another_section);
	fprintf(stderr, "Implement doff_bfd_copy_private_section_data");
	abort();
}

bfd_boolean
doff_bfd_copy_private_symbol_data(bfd *abfd, asymbol *symbol, bfd *another_bfd,
			asymbol *another_symbol)
{

	UNUSED(abfd);
	UNUSED(symbol);
	UNUSED(another_bfd);
	UNUSED(another_symbol);
	fprintf(stderr, "Implement doff_bfd_copy_private_symbol_data");
	abort();
}

bfd_boolean
doff_bfd_copy_private_header_data(bfd *abfd, bfd *another_bfd)
{

	UNUSED(abfd);
	UNUSED(another_bfd);
	fprintf(stderr, "Implement doff_bfd_copy_private_header_data");
	abort();
}

bfd_boolean
doff_bfd_set_private_flags(bfd *abfd, flagword what)
{

	UNUSED(abfd);
	UNUSED(what);
	fprintf(stderr, "Implement doff_bfd_set_private_flags");
	abort();
}

bfd_boolean
doff_bfd_print_private_bfd_data(bfd *abfd, void *what)
{

	UNUSED(abfd);
	UNUSED(what);
	fprintf(stderr, "Implement doff_bfd_set_private_flags");
	abort();
}

long
doff_get_symtab_upper_bound(bfd *abfd)
{
	struct doff_tdata *tdata;

	tdata = abfd->tdata.doff_obj_data;
	if (tdata->num_syms == 0)
		return 0;

	return (tdata->num_syms + 1) * sizeof(void *);

}

long
doff_canonicalize_symtab(bfd *abfd, struct bfd_symbol **symbol)
{
	struct doff_tdata *tdata;

	tdata = abfd->tdata.doff_obj_data;

	/* With some suprising sanity, we can just copy the list of pointers
	 * from the existing symbol table into the chunk of memory the caller
	 * allocated. The uncertainty is how exactly this translates back into
	 * our internal list */
	memcpy(symbol, tdata->symbols, tdata->num_syms * sizeof(void *));
	symbol[tdata->num_syms] = NULL;
	return tdata->num_syms;
}

struct bfd_symbol *
doff_make_empty_symbol(bfd *abfd)
{
	struct doff_symbol_internal *symbol;

	symbol = bfd_zalloc(abfd, sizeof(struct doff_symbol_internal));
	symbol->str_table_idx = -1;
	symbol->sect_idx = -1;
	symbol->bfd_symbol.the_bfd = abfd;
	return &symbol->bfd_symbol;
}

void
doff_print_symbol(bfd *abfd, void *what, struct bfd_symbol *symbol,
			bfd_print_symbol_type type)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(symbol);
	UNUSED(type);
	fprintf(stderr, "Implement doff_print_symbol");
	abort();
}

void
doff_get_symbol_info(bfd *abfd, struct bfd_symbol *symbol, symbol_info *info)
{

	UNUSED(abfd);
	UNUSED(symbol);
	UNUSED(info);
	fprintf(stderr, "Implement doff_get_symbol_info");
	abort();
}

bfd_boolean
doff_bfd_is_local_label_name(bfd *abfd, const char *name)
{

	UNUSED(abfd);
	UNUSED(name);
	fprintf(stderr, "Implement doff_is_local_label_name");
	abort();
}

bfd_boolean
doff_bfd_is_target_special_symbol(bfd *abfd, asymbol *symbol)
{

	UNUSED(abfd);
	UNUSED(symbol);
	fprintf(stderr, "Implement doff_is_target_special_name");
	abort();
}

alent *
doff_get_lineno(bfd *abfd, struct bfd_symbol *symbol)
{

	UNUSED(abfd);
	UNUSED(symbol);
	fprintf(stderr, "Implement doff_get_lineno");
	abort();
}

bfd_boolean
doff_find_nearest_line(bfd *abfd, struct bfd_section *section,
			struct bfd_symbol **symbol, bfd_vma addr,
			const char **what, const char **what2,
			unsigned int *what3)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(symbol);
	UNUSED(addr);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_find_nearest_line");
	abort();
}

bfd_boolean
doff_find_inliner_info(bfd *abfd, const char **what, const char **what2,
			unsigned int *what3)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_find_inliner_info");
	abort();
}

asymbol *
doff_bfd_make_debug_symbol(bfd *abfd, void *what, unsigned long size)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(size);
	fprintf(stderr, "Implement doff_bfd_make_debug_symbol");
	abort();
}

long
doff_read_minisymbols(bfd *abfd, bfd_boolean what, void **what2,
			unsigned int *what3)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_read_minisymbols");
	abort();
}

asymbol *
doff_minisymbol_to_symbol(bfd *abfd, bfd_boolean what, const void *what2,
			asymbol *minisymbol)
{

	UNUSED(abfd);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(minisymbol);
	fprintf(stderr, "Implement doff_minisymbol_to_symbol");
	abort();
}

long
doff_get_reloc_upper_bound(bfd *abfd, sec_ptr section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_get_reloc_upper_bound");
	abort();
}

long
doff_canonicalize_reloc(bfd *abfd, sec_ptr section, arelent **relocs,
			struct bfd_symbol **symbols)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(relocs);
	UNUSED(symbols);
	fprintf(stderr, "Implement doff_canonicalize_reloc");
	abort();
}

reloc_howto_type *
doff_bfd_reloc_type_lookup(bfd *abfd, bfd_reloc_code_real_type type)
{

	UNUSED(abfd);
	UNUSED(type);
	fprintf(stderr, "Implement doff_reloc_type_lookup");
	abort();
}

reloc_howto_type *
doff_bfd_reloc_name_lookup(bfd *abfd, const char *name)
{

	UNUSED(abfd);
	UNUSED(name);
	fprintf(stderr, "Impement doff_reloc_name_lookup");
	abort();
}

int
doff_sizeof_headers(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_sizeof_headers");
	abort();
}

bfd_byte *
doff_bfd_get_relocated_section_contents(bfd *abfd, struct bfd_link_info *info,
			struct bfd_link_order *order, bfd_byte *what,
			bfd_boolean what2, struct bfd_symbol **what3)
{

	UNUSED(abfd);
	UNUSED(info);
	UNUSED(order);
	UNUSED(what);
	UNUSED(what2);
	UNUSED(what3);
	fprintf(stderr, "Implement doff_bfd_get_relocated_section_contents");
	abort();
}

bfd_boolean
doff_bfd_relax_section(bfd *abfd, struct bfd_section *section,
			struct bfd_link_info *info, bfd_boolean *what)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(info);
	UNUSED(what);
	fprintf(stderr, "Implement doff_bfd_relax_section");
	abort();
}

struct bfd_link_hash_table *
doff_bfd_link_hash_table_create(bfd *abfd)
{

	/* We have no file dependant cares about the link hash table */
	return _bfd_generic_link_hash_table_create(abfd);
}

void
doff_bfd_link_hash_table_free(struct bfd_link_hash_table *table)
{

	/* No file dependant concern here either */
	_bfd_generic_link_hash_table_free(table);
	return;
}

bfd_boolean
doff_bfd_link_add_symbols(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_link_add_symbols");
	abort();
}

void
doff_bfd_link_just_syms(asection *section, struct bfd_link_info *info)
{

	UNUSED(section);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_link_just_syms");
	abort();
}

static bfd_boolean
collect_syms(struct bfd_link_hash_entry *entry, void *t)
{
	struct doff_tdata *tdata;
	struct doff_symbol_internal *sym;
	char *name;
	int len;

	tdata = t;
	switch (entry->type) {
	case bfd_link_hash_defined:
	case bfd_link_hash_undefined:
		sym = (struct doff_symbol_internal *)
					doff_make_empty_symbol(tdata->the_bfd);
		if (sym == NULL)
			return FALSE;

		len = strlen(entry->root.string);
		if (len == 0) {
			fprintf(stderr, "Symbol with no name\n");
			return FALSE;
		}

		len++;
		name = bfd_alloc(tdata->the_bfd, len);
		strncpy(name, entry->root.string, len);
		sym->bfd_symbol.name = name;
		sym->bfd_symbol.flags = BSF_NO_FLAGS;
		/* XXX - set flags */

		if (entry->type == bfd_link_hash_undefined) {
			sym->bfd_symbol.value = 0;
			sym->bfd_symbol.section = bfd_und_section_ptr;
		} else {
			sym->bfd_symbol.value = entry->u.def.value;
			sym->bfd_symbol.section = entry->u.def.section;
		}

		tdata->symbols[tdata->num_syms++] = sym;
		break;

	case bfd_link_hash_defweak:
	case bfd_link_hash_undefweak:
	case bfd_link_hash_common:
	case bfd_link_hash_new:
		fprintf(stderr, "doff: probably should be supported but "
				"unimplemented symbol type %d\n", entry->type);
		return FALSE;

	case bfd_link_hash_warning:
	case bfd_link_hash_indirect:
		fprintf(stderr, "doff: Don't know how to handle warning or "
				"indirect classed symbols\n");
		return FALSE;
	}
	return TRUE;
}

bfd_boolean
doff_bfd_final_link(bfd *abfd, struct bfd_link_info *info)
{
	struct doff_tdata *tdata;
	int sz;
	/* Linker or user has munged symbols and relocs to their liking; nuke
	 * whatever private backend info we have remaining, and then regenerate
	 * it from the remaining symbols, relocs and sections */

	tdata = abfd->tdata.doff_obj_data;
	tdata->num_sections = 0;
	tdata->section_data = NULL; /* bfd_alloc'd, released some other time */
	tdata->num_strings = 0;
	tdata->max_num_strings = 0;
	tdata->string_table = NULL;
	tdata->string_idx_table = NULL; /* The same */
	tdata->num_syms = 0;
	tdata->symbols = NULL; /* And again */

	/* num_syms is used as an index by collect_syms; leave it at zero */
	sz = info->hash->table.count;
	tdata->symbols = bfd_alloc(abfd, sz * sizeof(void *));
	if (tdata->symbols == NULL)
		return FALSE;

	/* Aarrgh. bfd_link_hash_traverse doesn't return the error code, it's
	 * discarded. So, check for errors by telling whether or not
	 * collect_syms has reached the number of existing symbols */
	bfd_link_hash_traverse(info->hash, collect_syms, tdata);
	if (tdata->num_syms != sz)
		return FALSE;

	/* Now, actually do some linking */
	tdata->link_info = info;
	bfd_map_over_sections(abfd, doff_ingest_section, tdata);
	return TRUE;
}

bfd_boolean
doff_bfd_link_split_section(bfd *abfd, struct bfd_section *section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_bfd_link_split_section");
	abort();
}

bfd_boolean
doff_bfd_gc_sections(bfd *abfd, struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(info);
	fprintf(stderr, "Implement doff_bfd_gc_sections");
	abort();
}

bfd_boolean
doff_bfd_merge_sections(bfd *abfd ATTRIBUTE_UNUSED,
			struct bfd_link_info *info ATTRIBUTE_UNUSED)
{

	/* There's also nothing special to do here */
	return TRUE;
}

bfd_boolean
doff_bfd_is_group_section(bfd *abfd, const struct bfd_section *section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_bfd_is_group_section");
	abort();
}

bfd_boolean
doff_bfd_discard_group(bfd *abfd, struct bfd_section *section)
{

	UNUSED(abfd);
	UNUSED(section);
	fprintf(stderr, "Implement doff_bfd_discard_group");
	abort();
}

void
doff_section_already_linked(bfd *abfd, struct bfd_section *section,
			struct bfd_link_info *info)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(info);
	fprintf(stderr, "Implement doff_section_already_linked");
	abort();
}

bfd_boolean
doff_bfd_define_common_symbol(bfd *abfd, struct bfd_link_info *info,
			struct bfd_link_hash_entry *hash)
{

	UNUSED(abfd);
	UNUSED(info);
	UNUSED(hash);
	fprintf(stderr, "Implement doff_bfd_define_common_symbol");
	abort();
}
bfd_boolean
doff_set_section_contents(bfd *abfd, sec_ptr section, const void *location,
			file_ptr file, bfd_size_type type)
{

	UNUSED(abfd);
	UNUSED(section);
	UNUSED(location);
	UNUSED(file);
	UNUSED(type);
	fprintf(stderr, "Implement doff_set_section_contents");
	abort();
}
