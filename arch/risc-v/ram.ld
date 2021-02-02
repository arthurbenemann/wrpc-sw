OUTPUT_FORMAT("elf32-littleriscv")
ENTRY(_start)
MEMORY
{
    ram :
 ORIGIN = 0x00000000,
 LENGTH = 196608 - 2048
    stack :
 ORIGIN = 196608 - 2048,
 LENGTH = 2048
}
SECTIONS
{
  . = 0;
  _ftext = .;
  PROVIDE( eprol = . );
  .text :
  {
    *(.boot)
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)
  } > ram
  .init :
  {
    KEEP( *(.init) )
  } > ram
  .fini :
  {
    KEEP( *(.fini) )
  } > ram
  .rodata :
  {
    *(.rdata)
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
  } > ram
  PROVIDE( etext = . );
  _etext = .;
  . = ALIGN(16);
   _fdata = .;
  .data :
  {
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
  } > ram
  PROVIDE( edata = . );
  _edata = .;
  . = ALIGN(16);
  _gp = . + 0x800;
  .sdata :
  {
    *(.sdata)
    *(.sdata.*)
    *(.srodata.*)
    *(.gnu.linkonce.s.*)
  } > ram
  . = ALIGN(8);
  _fbss = .;
  .sbss :
  {
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
  } > ram
  . = .;
  _bss_start = .;
  .bss :
  {
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
  } > ram2
 PROVIDE(_endram = ORIGIN(stack));
 PROVIDE(_fstack = ORIGIN(stack) + LENGTH(stack) - 0x400);
  PROVIDE(_fexception_stack = ORIGIN(stack) + LENGTH(stack) - 0x4);
  PROVIDE(_end = .);
}
