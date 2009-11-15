/* As binutils decided to visit the sins of the fathers upon their descendents,
 * insert GPL header here. Hi Rob! */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/tic6x.h"
#include "coff/internal.h"
#include "libcoff.h"

#undef F_LSYMS
#define F_LSYMS		F_LSYMS_TICOFF
/* XXX - dictated by coff/ti.h, but ti's docs say F_LSYMS remains 0x8 */
