# XXX: Relocate strlcat.c to $topdir/lib when autotools arrives

LIB     := strlcat.o

all: ubr

ubr: ubr.o vlan.o $(LIB)

clean:
	$(RM) ubr

distclean: clean
	$(RM) *.o *~

ubr.o vlan.o: ubr.h Makefile
