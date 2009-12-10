/* Copyright blah */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic64x.h"
#include "coff/tic64x.h"

int
print_insn_tic64x(bfd_vma addr, disassemble_info *info)
{

	fprintf(stderr, "Congratulations, you've got binutils working to a "
			"point where stuff can actually be read in! Now you "
			"ust need to implement the disassembly part...");
	exit(1);
}

int
tic64x_get_insn(disassemble_info *info, bfd_vma addr, unsigned short mdata,
			int *size)
{

	fprintf(stderr, "Congratulations, you've got binutils working to a "
			"point where stuff can actually be read in! Now you "
			"ust need to implement the disassembly part...");
	exit(1);
}
