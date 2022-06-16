.PHONY: check shell

KVERSION ?= 5.18.5
KDIR ?= linux-$(KVERSION)
KARCHIVE ?= $(KDIR).tar.xz
KURL ?= https://cdn.kernel.org/pub/linux/kernel/v5.x/$(KARCHIVE)

linux = $(KDIR)/linux mem=64M loglevel=0 quiet \
		root=/dev/root rootfstype=hostfs rootflags=$(abspath root) rw \
		init=$1

all: check

check: uml-check

uml-check: $(KDIR)/linux root/uml-test.sh root/test.sh root/sbin/ubr root/lib/ubr.ko
	$(call linux,/uml-test.sh)

shell: | root
	$(call linux,/bin/sh)

root/uml-test.sh: uml-test.sh | root
	cp $< $@

root/test.sh: test.sh | root
	cp $< $@

root/sbin/ubr: ../src | root
	$(MAKE) -C $< LDFLAGS=-static clean all
	cp ../src/ubr $@

root/lib/ubr.ko: ../kernel | root
	$(MAKE) -C $< KDIR=$(abspath $(KDIR)) ARCH=um clean all
	cp ../kernel/ubr.ko $@

root/usr/src/ubr/src root/usr/src/ubr/kernel: | root
	mkdir -p $@

root:
	alp -C root init busybox iproute2 tcpdump socat

$(KDIR)/linux: $(KDIR)/.config
	$(MAKE) -C $(KDIR) ARCH=um

$(KDIR)/.config: linux.config | $(KDIR)
	cp $< $@

$(KDIR): $(KARCHIVE)
	tar -xaf $<

$(KARCHIVE):
	wget $(KURL)
