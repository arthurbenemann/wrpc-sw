# and don't touch the rest unless you know what you're doing.
CROSS_COMPILE ?= lm32-elf-

CC =		$(CROSS_COMPILE)gcc
LD =		$(CROSS_COMPILE)ld
OBJDUMP =	$(CROSS_COMPILE)objdump
OBJCOPY =	$(CROSS_COMPILE)objcopy
SIZE =		$(CROSS_COMPILE)size

CFLAGS = -Os -I../common -I.  -mmultiply-enabled -mbarrel-shift-enabled -ffunction-sections -fdata-sections
OBJS = boot-crt0.o uart.o boot.o
LDS = boot.ld
LDFLAGS =  -mmultiply-enabled -mbarrel-shift-enabled -Wl,--gc-sections
OUTPUT=boot

$(OUTPUT): $(LDS) $(OBJS)
	${CC} -o $(OUTPUT).elf -nostartfiles $(OBJS) -T $(LDS) -lgcc -lc
	${OBJCOPY} -O binary $(OUTPUT).elf $(OUTPUT).bin
	$(SIZE) $(OUTPUT).elf
	../genraminit $(OUTPUT).bin 32 0 0 > wrc-bootloader.ram
	../genraminit $(OUTPUT).bin 512 88064 22016 >> wrc-bootloader.ram

clean:
	rm -f $(OBJS) $(OUTPUT).bin