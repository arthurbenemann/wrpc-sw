BOOTLOADER_CFLAGS = $(CFLAGS) -DIN_BOOTLOADER

BOOTLOADER_OBJS = \
 tools/uart-bootloader/boot-crt0.o \
 tools/uart-bootloader/boot.o \
 tools/uart-bootloader/timer.o \
 tools/uart-bootloader/bb_spi.o \
 tools/uart-bootloader/gpio.o \
 tools/uart-bootloader/spi_flash.o \
 tools/uart-bootloader/simple_uart.o \
 tools/uart-bootloader/div64.o

BOOTLOADER_LDFLAGS =  -Os -march=rv32im -mabi=ilp32

tools/uart-bootloader/bootloader.o: $(BOOTLOADER_OBJS)
	${CC} -o bootloader-tmp.o -nostartfiles $(BOOTLOADER_LDFLAGS) -r $(BOOTLOADER_OBJS) -lgcc -lc -e _bootloader_entry -Wl,-Map,bootloader.map
	${OBJCOPY} -G _bootloader_entry --prefix-alloc-section loader bootloader-tmp.o $@

tools/uart-bootloader/boot-crt0.o: arch/risc-v/boot-crt0.S
	${CC} $(BOOTLOADER_CFLAGS) -c $^ -o $@

tools/uart-bootloader/bb_spi.o: dev/bb_spi.c
	${CC} $(BOOTLOADER_CFLAGS) -c $^ -o $@

tools/uart-bootloader/gpio.o: dev/gpio.c
	${CC} $(BOOTLOADER_CFLAGS) -c $^ -o $@

tools/uart-bootloader/spi_flash.o: dev/spi_flash.c
	${CC} $(BOOTLOADER_CFLAGS) -c $^ -o $@

tools/uart-bootloader/simple_uart.o: dev/simple_uart.c
	${CC} $(BOOTLOADER_CFLAGS) -c $^ -o $@

tools/uart-bootloader/div64.o: pp_printf/div64.c
	${CC} $(BOOTLOADER_CFLAGS) -c $^ -o $@


obj-$(CONFIG_BOOTLOADER) += tools/uart-bootloader/bootloader.o

ifneq ($(CONFIG_BOOTLOADER),y)
# Just in case: allow to build a version without a bootloader just with a
# simple link
obj-$(CONFIG_ARCH_RISCV) += arch/risc-v/no-bootloader.o
endif

