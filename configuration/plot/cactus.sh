#!/bin/bash

#
#
# This script plots data about experiments, based on the parameter
# If only one experiment is specified, data for this experiment is plotted
# If you specifiy two parameters, the two experiments are compared to each other
#
#

#get number of experiments to compare

cd plot;

# cactus-plot(real time)
./plotCactusDataFiles.sh ../pdf/cactus ../glucose21 4 ../minisat22 4  ../penelope 4 ../glu21SatElite 4 ../splitterLA 4 ../splitterSHARE 4  ../splitterFULL 4  ../splitterLONG 4  ../splitterLALONG 4   ../splitterGluLALONG 4 
rm -f ../pdf/cactus.eps
	
./plotCactusDataFiles.sh ../pdf/cp3-time ../glucose21 4 ../minisat22 4  ../penelope 4 ../glu21SatElite 4 ../glLearn 4 ../glSelect 4 ../gluCP3 4 ../glLearnSatElite 4 ../glSelectSatelite 4
rm -f ../pdf/preprocessors.eps
	
./plotCactusDataFiles.sh ../pdf/splitter ../penelope 4 ../splitterLA 4 ../splitterSHARE 4  ../splitterFULL 4  ../splitterLONG 4  ../splitterLALONG 4   ../splitterGluLALONG 4   ../splitterGluLALONG-PP 4 ../splitterGluLA-SH-LONG 4  ../splitterGluLA-SH-PP-LONG 4
rm -f ../pdf/splitter.eps
	
./plotCactusDataFiles.sh ../pdf/glucoses ../glucose21 4 ../glu21SatElite 4  ../penelope 4 ../glLearn 4 ../glLearn1 4 ../glLearnSatElite 4 ../glSelect 4 ../glSelectSatelite 4 ../glucose85 4 ../gluCP3 4 ../minisat22 4
rm -f ../pdf/glucoses.eps



# slides

f=plain
./plotCactusDataFiles.sh ../pdf/$f ../glucose21 4 ../minisat22 4 ../glu21SatElite 4 ../gluCP3 4 ../glu21PP-again 4
rm -f ../pdf/$f.eps

f=select
./plotCactusDataFiles.sh ../pdf/$f ../glucose21 4 ../glu21SatElite 4  ../glu21PP-again 4 ../glLearnSelect 4 ../glSelect 4  ../glSelectSatelite 4  ../glSelectSizeSatelite 4
rm -f ../pdf/$f.eps

f=learn
./plotCactusDataFiles.sh ../pdf/$f ../glucose21 4 ../glLearn 4 ../glLearn1 4 ../glSelect 4  ../glLearnSelect 4 
rm -f ../pdf/$f.eps
	
f=learnPP
./plotCactusDataFiles.sh ../pdf/$f ../glu21SatElite 4  ../glu21PP-again 4 ../glLearnSatElite 4 ../glSelectSatelite 4 ../gluCP3 4
rm -f ../pdf/$f.eps
	
f=parallel
./plotCactusDataFiles.sh ../pdf/$f ../minisat22 4 ../glucose21 4 ../splitterLA 4 ../splitterLALONG 4 ../splitterGluLALONG-PP 4 ../splitterGluLALONG-PP 4 ../penelope 4
rm -f ../pdf/$f.eps
	
f=related
./plotCactusDataFiles.sh ../pdf/$f ../minisat22 4 ../glucose21 4  ../glu21SatElite 4  ../glu21PP-again 4 ../penelope 4 ../lingeling 4 ../plingeling 4
rm -f ../pdf/$f.eps
	
f=modifiedGlucose
./plotCactusDataFiles.sh ../pdf/$f ../glucose21 4  ../glu21SatElite 4 ../glu21PP-again 4 ../gluLessCleaning-PP 4 ../gluLessCleaning-2-PP 4 ../minisatGH 4 ../gluSATrestart 4 ../gluUNSATrestart 4
rm -f ../pdf/$f.eps

f=penelopes
./plotCactusDataFiles.sh ../pdf/$f ../penelope2 4  ../penelope4 4 ../penelope 4 ../penelope12 4 ../penelope16 4
rm -f ../pdf/$f.eps

cd ..

