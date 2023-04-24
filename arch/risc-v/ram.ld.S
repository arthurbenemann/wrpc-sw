OUTPUT_FORMAT("elf32-littleriscv")
ENTRY(_start)

#ifdef CONFIG_BOOTLOADER
#define CONFIG_LOADERSIZE 0x2000
#else
#define CONFIG_LOADERSIZE 0
#endif
	
MEMORY
{
    ram :
	ORIGIN = 0x00000000,
	LENGTH = CONFIG_RAMSIZE - CONFIG_STACKSIZE - CONFIG_DATASIZE - CONFIG_LOADERSIZE
    datacopy :
        ORIGIN = CONFIG_RAMSIZE - CONFIG_STACKSIZE - CONFIG_DATASIZE - CONFIG_LOADERSIZE
	LENGTH = CONFIG_DATASIZE
    stack :
	ORIGIN = CONFIG_RAMSIZE - CONFIG_STACKSIZE - CONFIG_LOADERSIZE
	LENGTH = CONFIG_STACKSIZE
    loader :
	ORIGIN = CONFIG_RAMSIZE - CONFIG_LOADERSIZE
	LENGTH = CONFIG_LOADERSIZE
    none :
	ORIGIN = CONFIG_RAMSIZE
	LENGTH = 0
}

SECTIONS
{
    /*--------------------------------------------------------------------*/
    /* Code and read-only segment                                         */
    /*--------------------------------------------------------------------*/

    /* text: Program code section */
    .text :
    {
	*(.boot)
	*(.text)
	*(.text.*)
	*(.gnu.linkonce.t.*)
    } > ram

    /* rodata: Read-only data */
    .rodata :
    {
	*(.rdata)
	*(.rodata)
	*(.rodata.*)
	*(.srodata.*)
	*(.gnu.linkonce.r.*)
    } > ram

    /*--------------------------------------------------------------------*/
    /* Initialized data segment                                           */
    /*--------------------------------------------------------------------*/

    /* data: Writable data */
    .data :
#if CONFIG_DATASIZE != 0
	AT (ORIGIN(datacopy))
#endif
    {
	*(.data)
	*(.data.*)
	*(.gnu.linkonce.d.*)
    } > ram

    /* Have _gp point to middle of sdata/sbss to maximize displacement range */
    . = ALIGN(4);
    _gp = . + 0x800;

    /* Writable small data segment */
    .sdata :
    {
	*(.sdata)
	*(.sdata.*)
	*(.gnu.linkonce.s.*)
    } > ram

    /* Bounds of data segment.  Be sure the end is aligned.  */
    _fdata = ADDR(.data);
    . = ALIGN(4);
    _edata = .;

    /*--------------------------------------------------------------------*/
    /* Uninitialized data segment                                         */
    /*--------------------------------------------------------------------*/

    /* Writable uninitialized small data segment */
    .sbss :
    {
	*(.sbss)
	*(.sbss.*)
	*(.gnu.linkonce.sb.*)
    } > ram

    /* bss: Uninitialized writeable data section */
    .bss :
    {
	*(.bss)
	*(.bss.*)
	*(.gnu.linkonce.b.*)
	*(COMMON)
    } > ram

    /* Bounds of bss segment. */
    _fbss = ADDR(.sbss);
    . = ALIGN(4);
    _end = .;

#if CONFIG_DATASIZE != 0
    _datacopy = ORIGIN(datacopy);
#endif

    /* End of RAM for checking stack overflows */
    PROVIDE(_endram = ORIGIN(stack));

    /* First location in stack is highest address in STACK */
    PROVIDE(_fstack = ORIGIN(stack) + LENGTH(stack));

    /* Then the bootloader (at end of RAM). */
    loader.text :
    {
	*(loader.text)
	*(loader.rodata)
	*(loader.srodata)
    } > loader

    loader.data :
    {
	*(loader.data)
    } > loader

    /* Have _gp point to middle of sdata/sbss to maximize displacement range */
    . = ALIGN(4);
    _loader_gp = . + 0x800;

    /* Writable small data segment */
    loader.data :
    {
	*(loader.sdata)
	*(loader.data)
    } > none

    loader.bss :
    {
	*(loader.sbss)
	*(loader.bss)
    } > loader

    _loader_fbss = ADDR(loader.bss);
    . = ALIGN(4);
    _loader_end = .;
}
