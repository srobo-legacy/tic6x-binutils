/* blah blah copyright */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"

const bfd_arch_info_type bfd_tic64x_arch =
{
	32,			/* bits in a word */
	32,			/* bits in an addr */
	8,			/* bits in a byte - XXX, c54x has 16, checkme */
	bfd_arch_tic64x,	/* bfd_architechture */
	0,			/* number of machines (?) */
	"tic64x",		/* arch name */
	"tms320c64x",		/* printable name */
	1,			/* alignment power */
	TRUE,			/* "default machine for arch" */
	bfd_default_compatible,	/* fptr for 'compatible' scan */
	bfd_default_scan,	/* scan func */
	0
};