# Makefile for parallel portfolio solver priss
# Copyright (c) 2012-2013, Norbert Manthey


EXEC      = priss
DEPDIR    = mtl utils core

# extern control whether coprocessor should be integrated
DEPDIR    += $(CPDEPEND)

CFLAGS    += $(INCFLAGS)
LFLAGS    += $(INLDFLAGS)

include $(MROOT)/mtl/template.mk
