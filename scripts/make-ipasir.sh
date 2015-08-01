#
# add all necessary files to a tar ball, stores into a given version
#
# to be called from the solver root directory (e.g. ./scripts/make-ipasir.sh riss_505 )
#

# first parameter of the script is the name of the tarball/directory
version=$1


# copy everything into a temporary directory
wd=`pwd`
tmpd=/tmp/tmp_$$
mkdir -p $tmpd
cd $tmpd

# create version directory
mkdir $version 
cd $version 
# copy necessary content here
cp -r $wd/{riss,cmake,coprocessor,proofcheck,CMakeLists.txt,license.txt,README.md} .

# call cmake to build/update the version files
tmp=tmp$$
mkdir -p $tmp
cd $tmp
cmake ..
cd ..
rm -rf $tmp

# clean files that might have been created during building external tools
rm -f */*.or */*/*.or */*.od */*/*.od

# produce the tar ball, and copy back to calling directory
cd ..
tar czf $version.tar.gz $version
cp $version.tar.gz $wd

# go back to calling directory
cd $wd

# clean up
rm -rf $tmpd
