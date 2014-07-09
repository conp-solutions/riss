#
# create the public source code tar ball
#
# (to be executed frmo the scripts directory)
#

# go to main directory of Riss
cd ..

# set variables
originpath=`pwd`
lastpath=`basename $originpath`

# go to temp directory
cd /tmp/
mkdir $$
cd $$
echo "clone git repository and use public branch..."
git clone $originpath
cd $lastpath
git checkout public

echo "update public branch..."
git pull origin riss
rm -rf .git* 
cd ..
mv $lastpath Riss

echo "create tar ball..."
tar czf $originpath/Riss.tar.gz Riss 
cd ..

echo "clean up..."
rm -rf $$
cd $originpath/scripts

