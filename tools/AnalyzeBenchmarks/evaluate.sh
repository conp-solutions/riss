#
# perform data analysis on benchmarks of the SAT events by year and category
#

# temporary files used during the analysis
tmp=/tmp/eva_features_$$.csv
tmp2=/tmp/eva_features2_$$.csv
uniq=/tmp/eva_uniq_$$.csv
mapping=/tmp/eva_map_$$.csv

# perform for crafted and industrial (for now only application)
for textfile in application.txt # crafted.txt 
do

echo ""; echo ""; echo ""; echo ""; echo ""; echo ""; 
echo "use $textfile"
echo ""; echo ""; echo ""; echo ""; echo ""; echo ""; 

	# loop over the years
	for year in 2002 2003 2004 2005 2006 2007 2008 2009 2010 2011 2012 2013 2014 2015
	do

		hits=$( cat $textfile | grep "^/CNF/$year" | wc -l )
		if [ "$hits" -eq 0 ]
		then
		  echo ""
			echo "$textfile does not contain files for year $year"
			continue
		fi

		echo ""
		# analysis per year
		rm -f $tmp $uniq
		for l in `cat $textfile | grep "^/CNF/$year"`
		do
			grep "$l" features.csv >> $tmp
		done
		echo "$year (used only in this year: `cat $tmp | wc -l`)"
		python sortFilesOnFeatures.py $tmp $uniq $mapping | grep -v "^error:"
		
		echo "some duplicates: (max. 10)"
		echo "============================================"
		awk '{ if( NF > 1 ) print $0 }' $mapping
		echo "============================================" 

		#echo ""
		rm -f $tmp $uniq
		# analysis for all instances up to the given year (analyses the pool)
		
		for y in `seq 2002 $year`
		do
			for l in `cat $textfile | grep "^/CNF/$y"`
			do
				grep "$l" features.csv >> $tmp
			done
		done
		echo "$year (all including the year, `cat $tmp | wc -l` formulas)"
		# create a global uniq and mapping up to this year
		python sortFilesOnFeatures.py $tmp $uniq $tmp2 $mapping # | grep -v "^error:"
		echo "size summary: `cat $tmp | wc -l` u: `cat $uniq | wc -l`  fm: `cat $tmp2 | wc -l`  bm: `cat $mapping | wc -l`"
		
		cp $tmp /tmp/files.txt
		cp $tmp2 /tmp/map1.txt
		cp $mapping /tmp/map2.txt
		
#		less $tmp2
#		less $mapping
		
		
		# analysis per year, collect all instance names, replace by the mapped instance
		rm -r $tmp
		for l in `cat $textfile | grep "^/CNF/$year"`
		do
			grep "$l" features.csv | awk '{print $1}' >> $tmp
		done
		#echo "selected `cat $tmp | wc -l` formulas"
		
		
		# analyze distribution for the given year
		rm -f $tmp2
		
		#echo "renaming ..."
		for fname in `cat $tmp`
		do
			grep "^$fname" $mapping | awk '{print $2}' >> $tmp2
		done
		#echo "renamed `cat $tmp2 | wc -l` formulas"
		
		echo "$year distribution"
		for y in `seq 2002 $year`
		do
			echo "$y: `grep /CNF/$y $tmp2 | wc -l`"
		done
	
		#echo "some duplicates and backward pointers:"
		#awk '{ if( $1 != $2 ) print $0 }' $mapping | head -n 2
		#awk '{ if( $1 != $2 ) print $0 }' $mapping | tail -n 2
		
	done

  continue

	# loop over the years
	for year in `seq 2002 2015`
	do
		hits=$( cat $textfile | grep "^/CNF/$year" | wc -l )
		if [ "$hits" -eq 0 ]; then continue; fi
		
		for y in `seq $(($year+1)) 2015`
		do
			hits=$( cat $textfile | grep "^/CNF/$y" | wc -l )
			if [ "$hits" -eq 0 ]; then continue; fi
			
			rm -f $tmp $uniq
			for l in `cat $textfile | grep "^/CNF/$year"`
			do
				grep "$l" features.csv >> $tmp
			done
			for l in `cat $textfile | grep "^/CNF/$y"`
			do
				grep "$l" features.csv >> $tmp
			done
			
			cat $tmp  | sort -V | uniq > $tmp2
			python sortFilesOnFeatures.py $tmp2 $uniq $mapping | grep -v "^error:" > /dev/null

			rel=$(echo "print str( `cat $uniq | wc -l` / `cat $tmp2 | wc -l`.0 )" | python)
			echo "$year $y $rel"
		done
	done


done #end textfile

ls $uniq

