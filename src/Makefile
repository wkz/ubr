# For cross-compiling you need a pre-built libmnl and to set the
# following environment variables:
# PATH=/path/to/cross/toolchain/bin:$PATH
# CC=cross-tool-prefix-gcc
# CPPFLAGS=-I/path/to/headers
# LDFLAGS=-L/path/to/libmnl.{a,so}

EXEC     := ubr
OBJS     := ubr.o cmdl.o msg.o fdb.o port.o vlan.o
HDRS     :=       cmdl.h msg.h fdb.h port.h vlan.h private.h
LDLIBS   += -lmnl
CPPFLAGS += -I../kernel

PREFIX   ?= /usr/local
SBINDIR  ?= $(PREFIX)/sbin

all: $(EXEC)

clean:
	$(RM) $(EXEC) $(OBJS)

distclean: clean
	$(RM) *.o *~

install:
	install -d $(DESTDIR)$(SBINDIR)
	install $(EXEC) $(DESTDIR)$(SBINDIR)/$(EXEC)

uninstall:
	$(RM) $(DESTDIR)$(SBINDIR)/$(EXEC)

$(EXEC): $(OBJS)

$(OBJS): Makefile $(HDRS)
