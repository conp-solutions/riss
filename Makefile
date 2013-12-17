#
# Makefile of the riss and coprocessor tool collection
#
#
#

# variables to setup the build correctly
CORE      = ../core
MTL       = ../mtl
VERSION   = 
MYCFLAGS  = -I.. -I. -I$(MTL) -I$(CORE) $(ARGS) -Wall -Wextra -ffloat-store -Wno-unused-parameter -Wno-sign-compare -Wno-parentheses $(VERSION)
LIBRT     = -lrt
MYLFLAGS  = -lpthread $(LIBRT) $(ARGS)

COPTIMIZE ?= -O3

all: rs

# shortcuts

# make a std binary of riss3g, riss3gext or the related preprocessor
riss: MYCFLAGS += -DTOOLVERSION=300 -DNOVERBHELP
riss: rs
rissExt: MYCFLAGS += -DTOOLVERSION=351 -DNOVERBHELP 
rissExt: rs
cp3Ext: MYCFLAGS += -DTOOLVERSION=351 -DNOVERBHELP 
cp3Ext: crs
libsoExt: MYCFLAGS += -DTOOLVERSION=351 -DNOVERBHELP 
libsoExt: libso


# shortcuts to build some targets
d: rissd
rs: rissRS

cld: classifierd
cls: classifiers

simp: rissSimpRS
simpd: rissSimpd

cd: coprocessord
crs: coprocessorRS

q: qprocessorRS
qd: qprocessord

#
# actual commands for building stuff
#
# build the bmc tool
#
aigbmcd: libd
	cd aiger-src; make aigbmc CFLAGS="-O0 -g" ARGS=$(ARGS);  mv aigbmc ..

aigbmcs: libs
	cd aiger-src; make aigbmc ARGS=$(ARGS); mv aigbmc ..

aigbmc-abcd: libd
	cd aiger-src; make aigbmc-abc CFLAGS="-O0 -g" ARGS=$(ARGS);  mv aigbmc-abc ..

aigbmc-abcs: libs
	cd aiger-src; make aigbmc-abc ARGS=$(ARGS) ;  mv aigbmc-abc ..
	
# build the solver
riss: always
	cd core; make r INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_release ../riss

rissRS: always
	cd core; make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_static ../riss

rissSimpRS: always
	cd simp; make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_static ../riss

rissSimpd: always
	cd simp; make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_debug ../riss

rissd: always
	cd core;   make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv riss3g_debug ../riss

# libraries
libd: always
	cd core;   make libd INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; rm lib.a; mv lib_debug.a ../libriss.a
libs: always
	cd core;   make libs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; rm lib.a; mv lib_standard.a ../libriss.a
libso: always
	cd core;   make libso INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv lib_release.so ../libcp.so
	strip -g -S -d --strip-debug -X -w --strip-symbol=*Coprocessor* --strip-symbol=*Minisat* libcp.so

# coprocessor
coprocessor: always
	cd coprocessor-src;  make r INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor_releas ../coprocessor

coprocessorRS: always
	cd coprocessor-src;  make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor_static ../coprocessor
	
coprocessord: always
	cd coprocessor-src;  make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv coprocessor_debug ../coprocessor
	
classifierd: always
	cd classifier-src;  make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv classifier_debug ../classifier
	
classifiers: always
	cd classifier-src;  make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv classifier_static ../classifier
	
	
# simple qbf preprocessor
qprocessord: always
	cd qprocessor-src;  make d INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv qprocessor_debug ../qprocessor

qprocessor: always
	cd qprocessor-src;  make r INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv qprocessor_release ../qprocessor
	
qprocessorRS: always
	cd qprocessor-src;  make rs INCFLAGS='$(MYCFLAGS)' INLDFLAGS='$(MYLFLAGS)' CPDEPEND="coprocessor-src" MROOT=.. COPTIMIZE="$(COPTIMIZE)" -j 4; mv qprocessor_static ../qprocessor
	
always:

touch:
	touch core/Solver.cc coprocessor-src/Coprocessor.cc

touchos:
	touch */*.os

strip: always
	strip  --keep-symbol=cp3add --keep-symbol=cp3destroyPreprocessor --keep-symbol=cp3dumpFormula --keep-symbol=cp3extendModel --keep-symbol=cp3freezeExtern --keep-symbol=cp3giveNewLit --keep-symbol=cp3initPreprocessor --keep-symbol=cp3parseOptions libriss3g.a

doc: clean
	cd doc; doxygen solver.config
	touch doc

# tar balls
tar: clean
	tar czvf riss.tar.gz core   LICENSE  Makefile mtl  README  simp utils
	
cotar: clean
	tar czvf coprocessor.tar.gz core LICENSE  Makefile mtl  README  simp  utils coprocessor-src
	
cltar: clean
	tar czvf classifier.tar.gz core LICENSE  Makefile mtl  README  simp  utils coprocessor-src classifier-src

qtar: clean
	tar czvf qprocessor.tar.gz core LICENSE Makefile mtl  README  simp  utils coprocessor-src qprocessor-src qp.sh 
	
bmctar: clean toolclean
	tar czvf riss-AbmC.tar.gz core LICENSE  Makefile mtl  README  simp  utils coprocessor-src aiger-src abc picosat
	
# clean up after solving - be careful here if some directories are missing!
clean:
	@cd core; make clean CPDEPEND="" MROOT=..;
	@cd simp; make clean MROOT=..;
	@cd coprocessor-src; make clean MROOT=..;
	@cd qprocessor-src; make clean MROOT=..;
	@cd classifier-src; make clean MROOT=..;
	@rm -f riss coprocessor qprocessor libriss.a libcp.so
	@rm -f *~ */*~
	@rm -rf doc/html
	@echo Done
	
# clean up aiger and abc as well!
toolclean:
	@cd aiger-src; make clean;
	@cd abc; make clean;

