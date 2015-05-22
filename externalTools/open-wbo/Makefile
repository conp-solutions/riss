# VERSION    = core or simp 
# SOLVERNAME = name of the SAT solver
# SOLVERDIR  = subdirectory of the SAT solver
# NSPACE     = namespace of the SAT solver
# 
# e.g. minisat compilation with core version:
#
# VERSION    = core
# SOLVERNAME = "Minisat"
# SOLVERDIR  = minisat
# NSPACE     = Minisat
#
#VERSION    = core
#SOLVERNAME = "Glucose3.0"
#SOLVERDIR  = glucose3.0
#NSPACE     = Glucose


VERSION    = core
SOLVERNAME = "Riss"
SOLVERDIR  = riss
NSPACE     = Riss

# THE REMAINING OF THE MAKEFILE SHOULD BE LEFT UNCHANGED
EXEC       = open-wbo
DEPDIR     = mtl utils core 

# For the original Minisat based systems
DEPDIR     +=  ../../encodings ../../algorithms
MROOT      = $(PWD)/solvers/$(SOLVERDIR)
CFLAGS     = -Wall -Wno-parentheses -g3 -DNSPACE=$(NSPACE) -DSOLVERNAME=$(SOLVERNAME) -DVERSION=$(VERSION)

# Riss is based on cmake, and located somewhere else, and relies on coprocessor, and requires more libraries
ifeq ($(SOLVERDIR),riss)
DEPDIR     = mtl utils core 
DEPDIR     += ../externalTools/open-wbo/encodings ../externalTools/open-wbo/algorithms ../coprocessor
MROOT      = ../../$(SOLVERDIR)
CFLAGS     = -I .. -I ../.. -I ../externalTools/open-wbo -I ../coprocessor  -Wall -Wno-parentheses -g3 -DNSPACE=$(NSPACE) -DSOLVERNAME=$(SOLVERNAME) -DVERSION=$(VERSION)
LFLAGS     = -lpthread -lz -lrt
endif

ifeq ($(VERSION),simp)
DEPDIR     += simp
CFLAGS     += -DSIMP=1 
endif

include mtl/template.mk
