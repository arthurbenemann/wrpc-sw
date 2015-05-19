OUTPUT_FORMAT("elf32-lm32")
ENTRY(_start)
MEMORY
{
    preboot :
    ORIGIN = 0x00000000,
    LENGTH = 0x100

    ram :
 ORIGIN = 90112 - 2048,
 LENGTH = 1536
    stack :
 ORIGIN = 90112 - 512,
 LENGTH = 512
}

SECTIONS
{
 .boot : { *(.boot) } > preboot
 .text : { *(.text .text.*) } > ram =0
 .rodata : { *(.rodata .rodata.*) } > ram
 .data : {
  *(.data .data.*)
  _gp = ALIGN(16) + 0x7ff0;
 } > ram
 .bss : {
  _fbss = .;
  *(.bss .bss.*)
  *(COMMON)
  _ebss = .;
 } > ram
 PROVIDE(_endram = ORIGIN(stack));
 PROVIDE(_fstack = ORIGIN(stack) + LENGTH(stack) - 4);
}
PROVIDE(mprintf = pp_printf);
