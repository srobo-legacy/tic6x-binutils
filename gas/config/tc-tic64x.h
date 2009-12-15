/* Copyright blah */

#ifndef _TC_TIC64X_H_
#define _TC_TIC64X_H_

/* Set cpu for obj-coff.h */
#define TC_TIC64X

#define TARGET_BYTES_BIG_ENDIAN 0
#define OCTETS_PER_BYTE_POWER 0

#define TARGET_ARCH		bfd_arch_tic64x

/* Compiler won't (shouldn't) emit 16 bit displacements */
#define WORKING_DOT_WORD	0
/* Trailing h or b on constants */
#define NUMBERS_WITH_SUFFIX 1
/* "section program counter" according to tic54x */
#define DOLLAR_DOT 1

/* Enable the "||" specifier on a line to indicate parallel execution */
#define DOUBLEBAR_PARALLEL 1

/* AFAIK no need for special/dynamic operand names */
#define md_operand(x)

/* AFAIK everything we want to deal with will be treated little endianly */
#define md_number_to_chars(b,v,n) number_to_chars_littleendian(b,v,n)

void tic64x_start_line_hook(void);
#define md_start_line_hook() tic64x_start_line_hook()


#endif /* _TC_TIC64X_H_ */
