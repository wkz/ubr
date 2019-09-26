# Build kernel module ubr.ko and user-space ubr tool
#
# Requirements: kernel headers and libmnl
.PHONY: all clean distclean

DIRS := kernel src

all: $(DIRS)

check:
	$(MAKE) -C test $@

.PHONY: $(DIRS)
$(DIRS):
	$(MAKE) -C $@

clean distclean:
	for dir in $(DIRS); do			\
		$(MAKE) -C $$dir $@;		\
	done

