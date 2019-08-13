# and don't touch the rest unless you know what you're doing.
CROSS_COMPILE ?= lm32-elf-

CC =		$(CROSS_COMPILE)gcc
LD =		$(CROSS_COMPILE)ld
OBJDUMP =	$(CROSS_COMPILE)objdump
OBJCOPY =	$(CROSS_COMPILE)objcopy
SIZE =		$(CROSS_COMPILE)size

CFLAGS = -DCONFIG_ERTM14 -Os -I../common -I.  -mmultiply-enabled -mbarrel-shift-enabled -ffunction-sections -fdata-sections -I../../include
OBJS = boot-crt0.o boot.o timer.o ../../dev/bb_spi.o ../../dev/gpio.o ../../dev/spi_flash.o  ../../dev/uart.o
LDS = boot.ld
LDFLAGS =  -Os -mmultiply-enabled -mbarrel-shift-enabled -Wl,--gc-sections
OUTPUT=boot

$(OUTPUT): $(LDS) $(OBJS)
	${CC} -o $(OUTPUT).elf -nostartfiles $(LDFLAGS) $(OBJS) -T $(LDS) -lgcc -lc
	${OBJCOPY} -O binary $(OUTPUT).elf $(OUTPUT).bin
	${OBJDUMP} -D $(OUTPUT).elf > disasm.S
	$(SIZE) $(OUTPUT).elf
	../genraminit $(OUTPUT).bin 32768 > wrc-bootloader.ram

clean:
	rm -f $(OBJS) $(OUTPUT).bin