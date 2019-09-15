LDLIBS   += -lmnl
CPPFLAGS += -I../kernel

all: ubr

ubr: ubr.o cmdl.o msg.o port.o vlan.o

clean:
	$(RM) ubr

distclean: clean
	$(RM) *.o *~

ubr.o cmdl.o vlan.o msg.o: Makefile cmdl.h msg.h port.h vlan.h
