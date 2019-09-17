# For cross-compiling you need a pre-built libmnl and to set the
# following environment variables:
# PATH=/path/to/cross/toolchain/bin:$PATH
# CC=cross-tool-prefix-gcc
# CPPFLAGS=-I/path/to/headers
# LDFLAGS=-L/path/to/libmnl.{a,so}

OBJS     := ubr.o cmdl.o msg.o fdb.o port.o vlan.o
HDRS     :=       cmdl.h msg.h fdb.h port.h vlan.h private.h
LDLIBS   += -lmnl
CPPFLAGS += -I../kernel

all: ubr

ubr: $(OBJS)

clean:
	$(RM) ubr $(OBJS)

distclean: clean
	$(RM) *.o *~

$(OBJS): Makefile $(HDRS)
