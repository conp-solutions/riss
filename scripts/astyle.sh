#!/bin/bash
# 
# Run astyle for all C++ headers and files in riss-toolbox
#
# Usage: astyle.sh [directory]

if [[ "$1x" == "x" ]]; then
    root=$PWD
else
    root="$1"
fi

folders="
$root/aiger/
$root/classifier/
$root/cmake/
$root/coprocessor/
$root/mprocessor/
$root/pcasso/
$root/pfolio/
$root/proofcheck/
$root/qprocessor/
$root/riss/
$root/risslibcheck/
$root/shiftbmc/
$root/test/
$root/tools/
"

find $root$folders -name '*.cc' -or -name '*.c' -or -name '*.h' -exec astyle --options=.astylerc {} \;
# clean up backup files
find $root$folders -name '*.cc.orig' -or -name '*.c.orig' -or -name '*.h.orig' -exec rm -v {} \;
