

# variables to setup the build correctly
CORE      = ../core
MTL       = ../mtl
MYCFLAGS    = -I.. -I. -I$(MTL) -I$(CORE) $(ARGS) -Wall -Wextra -ffloat-store
MYLFLAGS    = -lpthread

COPTIMIZE ?= -O3

# build the splitter solver
minisat: always
	cd core;   make INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)"; mv minisat ..

minisatd: always
	cd core;   make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)"; mv minisat_debug ../minisat

coprocessor: always
	cd coprocessor-src;   make INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)"; mv coprocessor ..
	
coprocessord: always
	cd coprocessor-src;   make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)"; mv coprocessor_debug ../coprocessor
	
always:

tar: clean
	tar czvf minisat22.tar.gz core  HOWTO  LICENSE  Makefile mtl  README  simp  splittings  utils

# clean up after solving
clean:
	@cd core; make clean CPDEPEND="" MROOT=..;
	@cd simp; make clean MROOT=..;
	@cd coprocessor-src; make clean MROOT=..;
	@rm -f minisat coprocessor minisatd
	@rm -f *~ */*~
	@echo Done
