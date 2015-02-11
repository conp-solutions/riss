# Makefile for riss
# Copyright (c) 2012-2015, Norbert Manthey

EXEC      = risslibcheck

# uses only the library - no dependencies
#DEPDIR    = mtl utils core simp 

# extern control whether coprocessor should be integrated
DEPDIR    += $(CPDEPEND)

CFLAGS    += $(INCFLAGS)
LFLAGS    += $(INLDFLAGS) -lpthread -lrt -lz


include $(MROOT)/mtl/template.mk
