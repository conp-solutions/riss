#
# Makefile of the riss and coprocessor tool collection
#
#
#

# variables to setup the build correctly
CORE      = ../core
MTL       = ../mtl
VERSION   = 
MYCFLAGS    = -I.. -I. -I$(MTL) -I$(CORE) $(ARGS) -Wall -Wextra -ffloat-store -Wno-sign-compare -Wno-parentheses $(VERSION)
MYLFLAGS    = -lpthread -lrt $(ARGS)

COPTIMIZE ?= -O3

all: rs

d: rissd
rs: rissRS

simp: rissSimpRS
simpd: rissSimpd

cd: coprocessord
crs: coprocessorRS

q: qprocessorRS
qd: qprocessord
 

# build the splitter solver
riss: always
	cd core;   make INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g ..

rissRS: always
	cd core; make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_static ../riss3g

rissSimpRS: always
	cd simp; make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_static ../riss3g

rissSimpd: always
	cd simp; make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_debug ../riss3g

rissd: always
	cd core;   make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_debug ../riss3g

coprocessor: always
	cd coprocessor-src;  make INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor ..

coprocessorRS: always
	cd coprocessor-src;  make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor_static ../coprocessor
	
coprocessord: always
	cd coprocessor-src;  make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor_debug ../coprocessor
	
# simple qbf preprocessor
qprocessord: always
	cd qprocessor-src;  make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv qprocessor_debug ../qprocessor
	
qprocessorRS: always
	cd qprocessor-src;  make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv qprocessor_static ../qprocessor
	
always:

touch:
	touch core/Solver.cc coprocessor-src/Coprocessor.cc

doc: clean
	cd doc; doxygen solver.config
	touch doc

tar: clean
	tar czvf riss3m.tar.gz core  HOWTO  LICENSE  Makefile mtl  README  simp utils
	
cotar: clean
	tar czvf coprocessor3.tar.gz core LICENSE  Makefile mtl  README  simp  utils coprocessor-src

qtar: clean
	tar czvf qprocessor3.tar.gz core LICENSE  Makefile mtl  README  simp  utils coprocessor-src qprocessor-src
	
# clean up after solving
clean:
	@cd core; make clean CPDEPEND="" MROOT=..;
	@cd simp; make clean MROOT=..;
	@cd coprocessor-src; make clean MROOT=..;
	@cd qprocessor-src; make clean MROOT=..;
	@rm -f riss3m riss3g coprocessor qprocessor
	@rm -f *~ */*~
	@rm -rf doc/html
	@echo Done
