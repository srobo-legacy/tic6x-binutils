/* Copyright blah */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"
#include "coff/tic64x.h"

#define UNUSED(x) ((x) = (x))

int
print_insn_tic64x(bfd_vma addr, struct disassemble_info *info)
{

	UNUSED(addr);
	UNUSED(info);
	fprintf(stderr, "Congratulations, you've got binutils working to a "
			"point where stuff can actually be read in! Now you "
			"ust need to implement the disassembly part...");
	exit(1);
}

const struct tic64x_op_template *
tic64x_get_insn(struct disassemble_info *info, bfd_vma addr,
		unsigned short mdata, int *size)
{

	UNUSED(info);
	UNUSED(addr);
	UNUSED(mdata);
	UNUSED(size);
	fprintf(stderr, "Congratulations, you've got binutils working to a "
			"point where stuff can actually be read in! Now you "
			"ust need to implement the disassembly part...");
	exit(1);
}
