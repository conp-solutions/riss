#
# normalize data in data files
#
#   removing all invalid exit codes!
#   add configuration name to head line
#
#

file=$1

if [ "x$file" == "x" ]
then
	file=`ls *.csv`
fi

for f in $file; 
do 
	echo "work on $f"
	bn=`basename $f .csv` # get name of the file

  # add configuration name to first line in file, remove lines that failed
	awk -v fname=$bn '{ 
		if( NR == 1 ) {
  		printf("%s ", $1);
			for(i=2; i<=NF; i++) { printf("%s.%s ", $i, fname)} ;
		} else { 
		  if( $2 == "ok" && ($3 == "10" || $3 == "20" ) && $4 < 900 ) {
		    printf ("%s %s %s %s", $1,$2,$3,$4 ); 
		  } else {
			  printf ("%s %s %s %s", $1, "failed", $3, "-" )
		  }
			for(i=5; i<=NF; i++) { printf(" %s", $i)} ;
		}
		printf("\n");
	}' $f > /tmp/tmp_$$.csv; 
	mv /tmp/tmp_$$.csv $f; 
done
