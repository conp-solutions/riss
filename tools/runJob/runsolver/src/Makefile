VERSION=3.2.9a
#SVNVERSION=`svnversion .`
SVNVERSION=$(word 2,$$Rev: 651 $$)
CFLAGS=-Dtmpdebug -DVERSION=\"$(VERSION)\" -DSVNVERSION=\"$(SVNVERSION)\"
#DEBUG=-g
DEBUG=-O3

STATIC=
#STATIC=-static

SRC=runsolver.cc SignalNames.cc SyscallNames.cc
OBJ=$(SRC:.cc=.o)

all:runsolver

install: runsolver
	cp runsolver $(INSTROOT)/usr/bin

include $(SRC:.cc=.d)

.cc.o:
	g++ $(CFLAGS) $(DEBUG) -c $*.cc

runsolver: $(OBJ)
	g++  $(STATIC) $(DEBUG) -o $@ $^ -lpthread

testlimit: testlimit.cc
	g++ -o testlimit testlimit.cc

testthread: testthread.cc
	g++ -o testthread testthread.cc -lpthread

SyscallNames.cc SyscallNames.hh: CreateSyscallsNames.cc /usr/include/asm/unistd.h
	grep __NR /usr/include/asm/unistd.h | grep -v '^/' | awk '{print $$2}' | sed -e 's/^__NR_//' | awk '{printf "list[__NR_%s]=\"%s\";\n",$$1,$$1}' > tmpSyscallList.cc
	g++ -o CreateSyscallsNames CreateSyscallsNames.cc
	./CreateSyscallsNames
	rm tmpSyscallList.cc CreateSyscallsNames 

.PHONY: clean realclean archive

archive: realclean
	tar cvjf /tmp/runsolver-$(VERSION).tar.bz2 -C ../.. runsolver/src --exclude .svn

clean:
	rm -f runsolver $(OBJ) *.class testlimit testtimestamper vlineSplitter testProcessTree runtestlimit testthread

realclean: clean
	rm -f *.d *~ SyscallNames.*


%.d: %.cc
	$(SHELL) -ec '$(CC) -MM $(CFLAGS) $< \
	| sed -e '\''s/$*\.o[ :]*/$@ &/g'\'' > $@'


