

#!/bin/make

SRC = $(wildcard src/*.cc)
OBJ = $(SRC:src/%.cc=build/%.o)
DEP = $(OBJ:.o=.d)

CXX = g++
CXXFLAGS = -pthread -Wall -Wextra -MMD -std=c++11
LDFLAGS = -pthread

LIBRARIES = -L"lib" -lpca -larmadillo -lboost_program_options
INCLUDES = -I include

all: trainer

trainer: $(OBJ)
	$(CXX) $(LDFLAGS) $(INCLUDES) $(LIBRARIES) $(OBJ) -o $@

build/%.o: src/%.cc Makefile
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBRARIES) -c $< -o $@

clean:
	rm -rf trainer

PHONY: clean

-include $(DEP)

# 
# PROG =  trainer
# 
# # # UNAME := $(shell uname)
# 
# ifeq ($(UNAME), Darwin)
# CXX = clang++ -stdlib=libc++
# else
# CXX = g++
# endiftt
# 
# FLAGS = -O3 -g3 -Wall -pthread 
# DEPS = Trainer.h Parser.h
# #INCS = -I"libpca/inc" 
# LIBS =  -lboost_program_options
# SRCS = Main.cc
# 
# RM = rm -f
# 
# all :
# 	$(CXX) $(FLAGS) $(DEPS) $(SRCS) $(LIBS) -o $(PROG)
# 
# clean :
# 
# 	$(RM) $(PROG)
# TARGET = trainer
# SOURCES = Main.cc Parser.cc Trainer.cc
# OBJECTS = $(SOURCES:.cc=.o)
# DEPENDS = $(SOURCES:.cc=.d) 
# LIBS = -lboost_program_options
# 
# all: $(TARGET)
# 
# $(TARGET): $(OBJECTS)
# 	g++ -o $@ $(OBJECTS) $(LIBS)
# 
# clean:
# 	rm -f $(OBJECTS) $(DEPENDS) $(TARGET)
# 
# -include $(DEPENDS)
