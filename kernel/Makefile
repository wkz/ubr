KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules

clean help modules modules_install:
	$(MAKE) -C $(KDIR) M=$$PWD $@
