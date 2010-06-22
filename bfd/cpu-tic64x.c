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
