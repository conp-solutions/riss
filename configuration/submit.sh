#
# clean the submit directory on atlas
# submit these data files to atlas into submit directory
# clean this directory afterwards!
#

ssh atlas "rm /fastfs/nmanthey/CP3/submit/*"
scp * atlas:/fastfs/nmanthey/CP3/submit/
mv *.txt submitted
