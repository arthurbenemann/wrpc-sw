
AS              = as
LD              = ld
CC              = gcc
CPP             = $(CC) -E
AR              = ar
NM              = nm
STRIP           = strip
OBJCOPY         = objcopy
OBJDUMP         = objdump

CFLAGS = -Wall -ggdb
CFLAGS += -I ./lib -I ./include
LDFLAGS = -L ./lib -lsdbfs

PROG = gensdbfs sdb-read sdb-extract

all: $(PROG)

%: %.c
	$(CC) $(CFLAGS) -o $@ $*.c $(LDFLAGS)

$(PROG): ./lib/libsdbfs.a

./lib/libsdbfs.a:
	$(MAKE) -C ./lib

clean:
	rm -f $(PROG) *.o *~ core
	$(MAKE) -C ./lib clean

# add the other unused targets, so the rule in ../Makefile works
modules install modules_install:
