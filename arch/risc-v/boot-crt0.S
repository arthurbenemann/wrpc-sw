	.section .text, "ax", @progbits

	.globl _bootloader_entry
_bootloader_entry:

    la     gp, _loader_gp                 # Initialize global pointer
    la     sp, _fstack

# clear the bss segment
    la     t0, _loader_fbss
    la     t1, _loader_end
1:
    sw     zero,0(t0)
    addi   t0, t0, 4
    bltu   t0, t1, 1b

    call   boot_main
