# default linker script for tic64x.
# On the whole copied from tic54x version of this file. We can customise later.
test -z "$ENTRY" && ENTRY=_start

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH("${OUTPUT_ARCH}")

MEMORY
{
	/* Given that tic64x has an MMU, it can configure memory wherever it
	 * likes, and for things like OMAP/dspbridge the loader will relocate
	 * sections to wherever is good - so make all the address space data
	 * and code. If someone uses this to load directly onto the dsp, they
	 * (should) be using their own linker script anyway */
	prog (RXI) : ORIGIN = 0x00000000, LENGTH = 0xFFFFFFFF
	data (W) : ORIGIN = 0x00000000, LENGTH = 0xFFFFFFFF
}

${RELOCATING+ENTRY (${ENTRY})}

SECTIONS
{
	.text :
	{
	} > prog
	.data :
	{
	} > prog
	/* all other initialized sections should be allocated here */
	.cinit :
	{
	} > prog
	.bss :
	{
	} > data
	/* all other uninitialized sections should be allocated here */
}
EOF
