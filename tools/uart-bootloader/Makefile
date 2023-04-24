# and don't touch the rest unless you know what you're doing.
CROSS_COMPILE ?= riscv32-elf-

CC =		$(CROSS_COMPILE)gcc
LD =		$(CROSS_COMPILE)ld
OBJDUMP =	$(CROSS_COMPILE)objdump
OBJCOPY =	$(CROSS_COMPILE)objcopy
SIZE =		$(CROSS_COMPILE)size

AUTOCONF ?= ../../include/generated/autoconf.h

all: bootloader.o

CFLAGS = -include $(AUTOCONF) -I../../ -Os -I../common -I.  -march=rv32im -mabi=ilp32 -I../../include -DCONFIG_TARGET_ERTM14 -I../../pp_printf -DIN_BOOTLOADER
OBJS = boot-crt0.o boot.o timer.o bb_spi.o gpio.o spi_flash.o  simple_uart.o div64.o
LDS = ../../arch/risc-v/boot.ld
LDFLAGS =  -Os -march=rv32im -mabi=ilp32
OUTPUT=wrc-bootloader
BRAM_IMAGE=../../wrc-bootloader.ram

%.o:	%.c
	${CC} $(CFLAGS) -c $^ -o $@

%.o:	%.S
	${CC} $(CFLAGS) -c $^ -o $@

bootloader.o: $(OBJS)
	${CC} -o bootloader-tmp.o -nostartfiles $(LDFLAGS) -r $(OBJS) -lgcc -lc -e _bootloader_entry -Wl,-Map,bootloader.map
	${OBJCOPY} -G _bootloader_entry --prefix-alloc-section loader bootloader-tmp.o $@

boot-crt0.o: ../../arch/risc-v/boot-crt0.S
	${CC} $(CFLAGS) -c $^ -o $@

bb_spi.o: ../../dev/bb_spi.c
	${CC} $(CFLAGS) -c $^ -o $@

gpio.o: ../../dev/gpio.c
	${CC} $(CFLAGS) -c $^ -o $@

spi_flash.o: ../../dev/spi_flash.c
	${CC} $(CFLAGS) -c $^ -o $@

simple_uart.o: ../../dev/simple_uart.c
	${CC} $(CFLAGS) -c $^ -o $@

div64.o: ../../pp_printf/div64.c
	${CC} $(CFLAGS) -c $^ -o $@

$(OUTPUT): $(LDS) $(OBJS)
	${CC} -o $(OUTPUT).elf -nostartfiles $(LDFLAGS) $(OBJS) -T $(LDS) -lgcc -lc
	${OBJCOPY} -O binary $(OUTPUT).elf $(OUTPUT).bin
	${OBJDUMP} -D $(OUTPUT).elf > disasm.S
	$(SIZE) $(OUTPUT).elf
	../genraminit -l $(OUTPUT).bin 32768 > $(BRAM_IMAGE)

clean:
	rm -f $(OBJS) $(OUTPUT).bin bootloader.o bootloader.map
