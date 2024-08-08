OUTPUT_FORMAT("elf32-littleriscv")
ENTRY(_start)
MEMORY
{
    ram :
 ORIGIN = 0x00000000,
 LENGTH = 65536 - 2048 - 0
    datacopy :
        ORIGIN = 65536 - 2048 - 0
 LENGTH = 0
    stack :
 ORIGIN = 65536 - 2048
 LENGTH = 2048

    mbox  : ORIGIN = 0x100000, LENGTH = 0x1000
    stats : ORIGIN = 0x101000, LENGTH = 0x800

}
SECTIONS
{
    .text :
    {
 *(.boot)
 *(.text)
 *(.text.*)
 *(.gnu.linkonce.t.*)
    } > ram
    .rodata :
    {
 *(.rdata)
 *(.rodata)
 *(.rodata.*)
 *(.srodata.*)
 *(.gnu.linkonce.r.*)
    } > ram
    .data :
    {
 *(.data)
 *(.data.*)
 *(.gnu.linkonce.d.*)
    } > ram
    . = ALIGN(4);
    _gp = . + 0x800;
    .sdata :
    {
 *(.sdata)
 *(.sdata.*)
 *(.gnu.linkonce.s.*)
 . = ALIGN(4);
    } > ram
    _fdata = ADDR(.data);
    _edata = .;
    .sbss :
    {
 *(.sbss)
 *(.sbss.*)
 *(.gnu.linkonce.sb.*)
    } > ram
    .bss :
    {
 *(.bss)
 *(.bss.*)
 *(.gnu.linkonce.b.*)
 *(COMMON)
    } > ram
    _fbss = ADDR(.sbss);
    . = ALIGN(4);
    _end = .;
    PROVIDE(_endram = ORIGIN(stack));
    PROVIDE(_fstack = ORIGIN(stack) + LENGTH(stack));

   .mbox : {
      *(.mbox)
   } > mbox
   
   .stats : {
      *(.stats)
   } > stats
   
}
