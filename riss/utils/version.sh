#!/bin/sh
# 
# Generates utils/version.cc file from current git revision
# 

# output file
OUTPUT="utils/version.cc"

# use git to get the current commit SHA hash
if [ -e ".git" ]; then
    GIT_COMMIT=$(git describe --abbrev --dirty --always)
else
    echo "Code not under version control"
    
    # Do nothing if the version file already exists and
    # we are not under version control
    if [ -e $OUTPUT ]; then
        echo "$OUTPUT already exists"
        exit 0
    fi

    echo "Warning: No git commit specified"
    GIT_COMMIT="unknown"
fi

if [ ! $VERSION ]; then
    echo "Warning: no Riss version specified"
    VERSION="unknown"
fi


# render a template configuration file
# expand variables + preserve formatting
cat - > $OUTPUT <<EOF
/*
 * This file was created automatically. Do not change it.
 * If you want to distribute the source code without
 * git, make sure you include this file in your bundle.
 */
#include "utils/version.h"

const char* Riss::signature     = "Riss $VERSION build $GIT_COMMIT";
const float Riss::solverVersion = $VERSION;
const char* Riss::gitCommit     = "$GIT_COMMIT";

EOF