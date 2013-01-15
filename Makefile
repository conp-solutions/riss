

# variables to setup the build correctly
CORE      = ../core
MTL       = ../mtl
MYCFLAGS    = -I.. -I. -I$(MTL) -I$(CORE) $(ARGS) -Wall -Wextra -ffloat-store
# MYLFLAGS    = -L. -lz -static -lpthread

COPTIMIZE ?= -O3

# build the splitter solver
glucose: always
	cd core;   make INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)"; mv glucose ..

glucosed: always
	cd core;   make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)"; mv glucose_debug ../glucose

always:

doc: clean
	cd doc; doxygen solver.config
	touch doc

tar: clean
	tar czvf minisat22.tar.gz core  HOWTO  LICENSE  Makefile mtl  README  simp  splittings  utils

# clean up after solving
clean:
	@cd core; make clean MROOT=..;
	@cd simp; make clean MROOT=..;
	@rm -f minisat
	@rm -f *~ */*~
	@rm -rf doc/html
	@echo Done
