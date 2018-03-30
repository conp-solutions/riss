#!/bin/bash
# make-starexec.sh, Norbert Manthey, 2018, LGPL v2, see LICENSE
#
# build the starexec package from the current git branch
# create a Riss7.1.zip archive

# make sure we notice failures early
set -e -x

# make sure we know where the code is
RISSDIR=$(pwd)
BRANCH=$(git rev-parse --abbrev-ref HEAD)

if [ ! -x "$RISSDIR"/scripts/make-starexec.sh ]
then
	echo "Error: script has to be called from base directory, abort!"
	exit 1
fi

# check for being on a branch
if [ -z "$BRANCH" ]
then
	echo "Error: failed to extract a git branch, abort!"
	exit 1
fi

# make sure we clean up
trap 'rm -rf $TMPD' EXIT
TMPD=$(mktemp -d)

# create the project directory
pushd "$TMPD"

# copy template
cp -r $RISSDIR/scripts/starexec_template/Riss7/* .

# copy actual source by using the git tree, only the current branch
git clone "$RISSDIR" --branch "$BRANCH" --single-branch riss7
pushd riss7
git checkout $BRANCH
git gc
git prune
popd

# get the source for cmake 3
wget https://cmake.org/files/v3.0/cmake-3.0.0-1-src.tar.bz2

# compress, include git files
zip -r -9 Riss7.1.zip * .git*

# jump back and move Riss7.1.zip here
popd
mv "$TMPD"/Riss7.1.zip .
