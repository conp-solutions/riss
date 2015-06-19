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

find $root \( -name '*.cc' -or -name '*.c' -or -name '*.h' \) -exec astyle --options=.astylerc {} \;
# clean up backup files
find $root \( -name '*.cc.orig' -or -name '*.c.orig' -or -name '*.h.orig' \) -exec rm -v {} \;
