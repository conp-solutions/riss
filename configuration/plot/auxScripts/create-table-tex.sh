#!/bin/bash

tf=table-data-auto.tex

# create headline
echo "instance & solved & avg. run time & avg. cpu utilization & par10 & sent clauses & solver calls & wasted calls & aborted calls"

# also print to screen!
# do not extract received clauses, because this distort the picture!
#echo "Configuration & \\multicolumn{1}{c}{\\begin{sideways} Solved Instances   \\end{sideways}} & "  > $tf
#echo " \\multicolumn{1}{c}{\\begin{sideways} avg. run time      \\end{sideways}} &  "   >> $tf
#echo "\\multicolumn{1}{c}{\\begin{sideways} avg. cpu time      \\end{sideways}} &  "   >> $tf
#echo "\\multicolumn{1}{c}{\\begin{sideways} par10              \\end{sideways}} &  "   >> $tf
#echo "\\multicolumn{1}{c}{\\begin{sideways} send clauses     \\end{sideways}} &  "   >> $tf
## do not use received clauses!
##echo "\\multicolumn{1}{c}{\\begin{sideways} send clauses     \\end{sideways}} &  "   >> $tf
#echo "\\multicolumn{1}{c}{\\begin{sideways} solver calls       \\end{sideways}} & "  >> $tf 
#echo "\\multicolumn{1}{c}{\\begin{sideways} wasted calls       \\end{sideways}} &  "   >> $tf
#echo "\\multicolumn{1}{c}{\\begin{sideways} aborted calls      \\end{sideways}} \\\\"  >> $tf


echo "Configuration & \\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} Solved\\end{sideways}\\,\\begin{sideways} Instances \\end{sideways}} & " > $tf
echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} average \\end{sideways}\\,\\begin{sideways} run time \\end{sideways}} &  "   >> $tf
echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} average cpu \\end{sideways}\\,\\begin{sideways} utilization \\end{sideways}} &  "   >> $tf
echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} penalized \\end{sideways}\\,\\begin{sideways} run time \\end{sideways}} &  "   >> $tf
echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} sent \\end{sideways}\\, \\begin{sideways} clauses \\end{sideways}} &  "   >> $tf
#echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} received \\end{sideways}\\, \\begin{sideways} clauses \\end{sideways}} &  "   >> $tf
echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} SAT solver \\end{sideways}\\,\\begin{sideways} calls \\end{sideways}} & "   >> $tf
echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} wasted \\end{sideways}\\,\\begin{sideways} solver calls \\end{sideways}} &  "   >> $tf
echo "\\multicolumn{1}{c@{\\hspace{2ex}}}{\\begin{sideways} aborted \\end{sideways}\\,\\begin{sideways} solver calls \\end{sideways}} \\\\"   >> $tf


echo "\\midrule" >> $tf
echo "\\multicolumn{9}{c}{\\textbf{Plain MUS Instances ($(( $(cat data-plain/ADC\(4\) | wc -l )- 1 )))}} \\\\" >> $tf 

# to obtain with % symbol, replace
# "\ \ " with "\ \ "
#!!!


# do plain files
cd data-plain;

# plain instances!
i=0
for f in "ASN(4)" "ASC(4)" "SDN(4)" "SDC(4)" "ADN(4)" "ADC(4)" "ADC+CBS(4)" "ADC+DYN+BUMP(4)" "ADC-PRASS(4)" "ADC+DYN+BUMP+CBS(4)" "MUSer2" "TarmoMUS" "pMUSer2(4)" "TarmoMUS(4)"  "ADC+CBS(8)" "ADC(8)" "ADC+DYN+BUMP+CBS(8)" "pMUSer2(8)" "TarmoMUS(8)"
do

  lastThree=${f:(-3)}
  tarmo="Tarmo"
  
  echo "file $f with last $lastThree"
  
  # extraction does not work on file format of muser2 -> extra script!
  if [ $f == "MUSer2" ]
  then
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                              { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END { CONVFMT = "%.2f"; OFMT="%.2f"; print solved , " & " , wTime / count , " & " , "100" , "\ \  & " , wTime + unsolved * 10 * 1800 , " & ",\
	                         "\\\\multicolumn{1}{c}{--}", " & ",\
	                         (solverCalls == 0 ? "\\\\multicolumn{1}{c}{--}" : solverCalls), " & ",  \
	                         "\\\\multicolumn{1}{c}{--}", " & ",\
	                         "\\\\multicolumn{1}{c}{--}", " \\\\\\\\"\
                           , "% total instances=", count; }'`
# do not use received clauses!
#	                         "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " \\\\\\\\"; }'`
  # extract for tarmo without cpu time
  elif [[ $f = $tarmo*  ]]
  then
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                         { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END { CONVFMT = "%.2f"; OFMT="%.2f"; print solved , " & " , wTime / count , " & " , "\\\\multicolumn{1}{c}{--}" , " & " , wTime + unsolved * 10 * 1800 , " & ",\
	                         (Sclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Sclauses), " & ",  \
	                         (solverCalls == 0 ? "\\\\multicolumn{1}{c}{--}" : solverCalls), " & ",  \
	                         (wasted == 0 ? "\\\\multicolumn{1}{c}{--}" : wasted/solverCalls*100)\
, (wasted == 0 ? " & " : "\ \  & " ),  \
	                         (aborted == 0 ? "\\\\multicolumn{1}{c}{--}" : aborted/solverCalls*100)\
,(aborted == 0 ? " \\\\\\\\ " : "\ \  \\\\\\\\ " )\
                           , "% total instances=", count; }'`

  # extract configurations that have 8 cores
  elif [ $lastThree == "(8)" ]
  then
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                         { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END { CONVFMT = "%.2f"; OFMT="%.2f";\
	                         print solved , " & " , wTime / count , " & " , (cpuTime / count) / (wTime / count ) * 12.5 , "\ \  & " , wTime + unsolved * 10 * 1800 , " & ",\
	                         (Sclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Sclauses), " & ",  \
	                         (solverCalls == 0 ? "\\\\multicolumn{1}{c}{--}" : solverCalls), " & ",  \
	                         (wasted == 0 ? "\\\\multicolumn{1}{c}{--}" : wasted/solverCalls*100)\
, (wasted == 0 ? " & " : "\ \  & " ),  \
	                         (aborted == 0 ? "\\\\multicolumn{1}{c}{--}" : aborted/solverCalls*100)\
,(aborted == 0 ? " \\\\\\\\ " : "\ \  \\\\\\\\ " )\
                           , "% total instances=", count; }'`
# do not use received clauses! (insert after Sclause line, if wanted!)	                         
#	                         (Rclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Rclauses), " & ",  \  
  else
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                              { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END {  CONVFMT = "%.2f"; OFMT="%.2f";\
	                         print\
	                         solved , " & " ,\
	                         wTime / count , " & " ,\
	                         (cpuTime / count) / (wTime / count ) * 25 , "\ \  & " ,\
	                         wTime + unsolved * 10 * 1800 , " & ",\
	                         (Sclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Sclauses), " & ",  \
	                         (solverCalls == 0 ? "\\\\multicolumn{1}{c}{--}" : solverCalls), " & ",  \
	                         (wasted == 0 ? "\\\\multicolumn{1}{c}{--}" : wasted/solverCalls*100), \
  												 (wasted == 0 ? " & " : "\ \  & " ),  \
	                         (aborted == 0 ? "\\\\multicolumn{1}{c}{--}" : aborted/solverCalls*100), \
                           (aborted == 0 ? " \\\\\\\\ " : "\ \  \\\\\\\\ " )\
                           , "% total instances=", count; }'`
  fi

  let "i=i+1"
  if [ $i == 1 -o $i == 7  -o $i == 11 -o $i == 13 -o $i == 15  ]
  then
   echo "\\midrule " >> ../$tf
  fi
	
	echo " $f & $solvedWCpuTimePar10" >> ../$tf
	# also print to screen
  echo " $f & $solvedWCpuTimePar10"


done 
cd ..


echo "\\midrule" >> $tf
echo "\\multicolumn{9}{c}{\\textbf{Group MUS Instances ($(( $(cat data-group/ADC\(4\) | wc -l )- 1 )))}} \\\\" >> $tf 

# group instances!
i=0
cd data-group;
for f in "MUSer2" "ADC(4)" "ADC+CBS(4)" "ADC+DYN+BUMP+CBS(4)" "ADC(8)" "ADC+CBS(8)" "ADC+DYN+BUMP+CBS(8)"
do
  lastThree=${f:(-3)}
  tarmo="Tarmo"
  # extraction does not work on file format of muser2 -> extra script!
  if [ $f == "MUSer2" ]
  then
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                              { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END { CONVFMT = "%.2f"; OFMT="%.2f";print solved , " & " , wTime / count , " & " , "100" , "\ \  & " , wTime + unsolved * 10 * 1800 , " & ",\
	                         "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " \\\\\\\\"; }'`
# do not use received clauses!
#	                         "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " & ", "\\\\multicolumn{1}{c}{--}", " \\\\\\\\"; }'`
  # extract for tarmo without cpu time
  elif [[ $f = $tarmo*  ]]
  then
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                         { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END { CONVFMT = "%.2f"; OFMT="%.2f";print solved , " & " , wTime / count , " & " , "\\\\multicolumn{1}{c}{--}" , " & " , wTime + unsolved * 10 * 1800 , " & ",\
	                         (Sclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Sclauses), " & ",  \
	                         (solverCalls == 0 ? "\\\\multicolumn{1}{c}{--}" : solverCalls), " & ",  \
	                        (wasted == 0 ? "\\\\multicolumn{1}{c}{--}" : wasted/solverCalls*100)\
, (wasted == 0 ? " & " : "\ \  & " ),  \
	                         (aborted == 0 ? "\\\\multicolumn{1}{c}{--}" : aborted/solverCalls*100)\
,(aborted == 0 ? " \\\\\\\\ " : "\ \  \\\\\\\\ " )\
                           , "% total instances=", count; }'`

  # extract configurations that have 8 cores
  elif [ $lastThree == "(8)" ]
  then
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                         { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END { CONVFMT = "%.2f"; OFMT="%.2f";print solved , " & " , wTime / count , " & " , (cpuTime / count) / (wTime / count ) * 12.5 , "\ \  & " , wTime + unsolved * 10 * 1800 , " & ",\
	                         (Sclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Sclauses), " & ",  \
	                         (solverCalls == 0 ? "\\\\multicolumn{1}{c}{--}" : solverCalls), " & ",  \
	                         (wasted == 0 ? "\\\\multicolumn{1}{c}{--}" : wasted/solverCalls*100)\
, (wasted == 0 ? " & " : "\ \  & " ),  \
	                         (aborted == 0 ? "\\\\multicolumn{1}{c}{--}" : aborted/solverCalls*100)\
,(aborted == 0 ? " \\\\\\\\ " : "\ \  \\\\\\\\ " )\
                           , "% total instances=", count; }'`
# do not use received clauses! (insert after Sclause line, if wanted!)	                         
#	                         (Rclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Rclauses), " & ",  \  
  else
	solvedWCpuTimePar10=`cat $f | \
										  awk '{ if( NR != 1 ) { count ++; if($2 != "TO" && $2 != "MO" ) \
	                              { solved+=1; cpuTime+=$2; wTime+=$3;Rclauses+=$16; Sclauses+=$17; solverCalls+=$7;wasted+=$10+$11;aborted+=$15}  \
	                         else {unsolved++}} } \
	                         END { CONVFMT = "%.2f"; OFMT="%.2f";print solved , " & " , wTime / count , " & " , (cpuTime / count) / (wTime / count ) * 25 , "\ \  & " , wTime + unsolved * 10 * 1800 , " & ",\
	                         (Sclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Sclauses), " & ",  \
	                         (solverCalls == 0 ? "\\\\multicolumn{1}{c}{--}" : solverCalls), " & ",  \
	                         (wasted == 0 ? "\\\\multicolumn{1}{c}{--}" : wasted/solverCalls*100   )\
, (wasted == 0 ? " & " : "\ \  & " ),  \
	                         (aborted == 0 ? "\\\\multicolumn{1}{c}{--}" : aborted/solverCalls*100 )\
,(aborted == 0 ? " \\\\\\\\ " : "\ \  \\\\\\\\ " )\
                           , "% total instances=", count; }'`
# do not use received clauses! (insert after Sclause line, if wanted!)	                         
#	                         (Rclauses == 0 ? "\\\\multicolumn{1}{c}{--}" : Rclauses), " & ",  \
  fi

  let "i=i+1"
  if [ $i == 1 -o $i == 2	 -o $i == 5 ]
  then
   echo "\\midrule " >> ../$tf
  fi
	
	echo " $f & $solvedWCpuTimePar10" >> ../$tf
	# also print to screen
  echo " $f & $solvedWCpuTimePar10"


done 
cd ..

sed -i 's/ \\,/\\,/g' $tf

