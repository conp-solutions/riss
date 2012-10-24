#!/bin/bash

# extract number of clauses from preprocessed formula

# run coprocessor with subsumption only (no strengthening)

./coprocessor -CP_techniques=s -CP_pline=1 -CP_strength=0 -file= $1 2>&1 | awk '/^p cnf/ {print $4;}'

# run minisat-subsumption
./minisat $1 /tmp/err /tmp/out |  awk '/^Wrote/ {print $2;}'

