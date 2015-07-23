#
# create the public source code tar ball
#
# usage: ./publish.sh versionnumber
#
# (to be executed from the scripts directory)
#

version=$1
shift;

# go to main directory of Riss
cd ..

# set variables
originpath=`pwd`
lastpath=`basename $originpath`

# go to temp directory
cd /tmp/
mkdir $$
cd $$

mkdir riss$version
cp -r $originpath/{cmake,coprocessor,pfolio,doc,pcasso,proofcheck,riss,risslibcheck,license.txt,CMakeLists.txt,README.md,mprocessor,qprocessor} riss$version

echo "create tar ball..."
tar czf $originpath/riss$version.tar.gz riss$version

echo "run integrity tests"
cd riss$version
cp -r $originpath/scripts .
./scripts/ci.sh

echo "clean up..."
cd ../..
rm -rf $$

# return to calling path
cd $originpath/scripts

