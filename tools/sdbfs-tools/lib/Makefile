
LINUX ?= /lib/modules/$(shell uname -r)/build

# If we compile for the kernel, we need to include real kernel headers.
# The thing is enough a mess that I moved it to a different file
include Makefile.arch


AS              = as
LD              = ld
CC              = gcc
CPP             = $(CC) -E
AR              = ar
NM              = nm
STRIP           = strip
OBJCOPY         = objcopy
OBJDUMP         = objdump

# calculate endianness at compile time
ENDIAN := $(shell ./check-endian $(CC))

CFLAGS = -Wall -ggdb -O2
CFLAGS += -I../include/linux -I../include # for <sdb.h>
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wno-pointer-sign
CFLAGS += $(ENDIAN) $(LINUXINCLUDE)


LIB = libsdbfs.a
OBJS = glue.o access.o

all: $(LIB)

$(OBJS): $(wildcard *.h)

$(LIB): $(OBJS)
	$(AR) r $@ $(OBJS)

clean:
	rm -f $(OBJS) $(LIB) *~ core

# add the other unused targets, so the rule in ../Makefile works
modules install modules_install:
