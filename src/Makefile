# XXX: Relocate strlcat.c to $topdir/lib when autotools arrives
LDLIBS = -lmnl

all: ubr

ubr: ubr.o cmdl.o msg.o vlan.o

clean:
	$(RM) ubr

distclean: clean
	$(RM) *.o *~

ubr.o cmdl.o vlan.o msg.o: Makefile cmdl.h msg.h vlan.h
