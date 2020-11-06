-include Makefile.specific
-include $(REPO_PARENT)/parent_common.mk

TOP_DIR ?= $(shell pwd)/../..
HDL_DIR ?= $(TOP_DIR)/hdl

KVERSION ?= $(shell uname -r)
LINUX ?= /lib/modules/$(KVERSION)/build

VERSION = $(shell git describe --dirty --long --tags)

all: modules

.PHONY: all modules clean help install modules_install coccicheck

install: modules_install
modules help coccicheck modules_install clean:
	$(MAKE) -C $(LINUX) M=$(shell pwd) VERSION=$(VERSION) $@
