OUTPUT_FORMAT("elf32-lm32")
ENTRY(_start)
MEMORY
{
    preboot :
    ORIGIN = 0x00000000,
    LENGTH = 0x100

    ram :
 ORIGIN = 131072 - 4096,
 LENGTH = 2048
    stack :
 ORIGIN = 131072 - 2048,
 LENGTH = 2038
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
