

# variables to setup the build correctly
CORE      = ../core
MTL       = ../mtl
VERSION   = 
MYCFLAGS    = -I.. -I. -I$(MTL) -I$(CORE) $(ARGS) -Wall -Wextra -ffloat-store -Wno-sign-compare -Wno-parentheses $(VERSION)
MYLFLAGS    = -lpthread -lrt $(ARGS)

COPTIMIZE ?= -O3



# build the splitter solver
minisat: always
	cd core;   make INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv minisat ..

minisatRS: always
	cd core; make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv minisat_static ../minisat

minisatd: always
	cd core;   make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv minisat_debug ../minisat

coprocessor: always
	cd coprocessor-src;  make INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor ..

coprocessorRS: always
	cd coprocessor-src;  make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor_static ../coprocessor
	
coprocessord: always
	cd coprocessor-src;  make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor_debug ../coprocessor
	
always:

touch:
	touch core/Solver.cc coprocessor-src/Coprocessor.cc

doc: clean
	cd doc; doxygen solver.config
	touch doc

tar: clean
	tar czvf minisat22.tar.gz core  HOWTO  LICENSE  Makefile mtl  README  simp  splittings  utils
	
cotar: clean
	tar czvf coprocessor3.tar.gz core LICENSE  Makefile mtl  README  simp  utils coprocessor-src

# clean up after solving
clean:
	@cd core; make clean CPDEPEND="" MROOT=..;
	@cd simp; make clean MROOT=..;
	@cd coprocessor-src; make clean MROOT=..;
	@rm -f minisat coprocessor minisatd
	@rm -f *~ */*~
	@rm -rf doc/html
	@echo Done
