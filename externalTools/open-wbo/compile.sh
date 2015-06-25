#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo 'c -------------------------------------'
    echo 'c | Open-WBO: a Modular MaxSAT Solver |'
    echo 'c -------------------------------------'
    echo 'c USAGE: ./compile.sh [sat solver] (options)'
    echo 'c Options: debug, release (default: release)'
    echo 'c SAT solvers: minisat2.0, minisat2.2, glucose2.3, glucose3.0, zenn, sinn, glueminisat, gluh, glue_bit, glucored'
    exit 0
fi

OPTIONS="rs"
MODE="release"

if [[ $# -eq 2 ]] ; then
    case "$2" in 
	debug) OPTIONS=""; MODE="debug"
   	esac
fi

case "$1" in 
    riss)
	echo "Compiling Open-WBO with minisat2.0 ($MODE mode)"
	make clean && make VERSION=core SOLVERNAME="Riss" SOLVERDIR=riss NSPACE=Riss $OPTIONS;;
    minisat2.2)
	echo "Compiling Open-WBO with minisat2.2 ($MODE mode)"
	make clean && make VERSION=core SOLVERNAME="Minisat2.2" SOLVERDIR=minisat2.2 NSPACE=Minisat $OPTIONS;;
    glucose3.0)
	echo "Compiling Open-WBO with glucose3.0 ($MODE mode)"
	make clean && make VERSION=core SOLVERNAME="Glucose3.0" SOLVERDIR=glucose3.0 NSPACE=Glucose $OPTIONS;;
esac
