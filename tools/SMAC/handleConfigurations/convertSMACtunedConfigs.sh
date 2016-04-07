#
#
# given a file with configurations for Riss in the format of SMAC, convert them for Riss, and show only non-default parameters
#
#

#
# parse all lines in the input file
#
cat $1 | while read CONFIG
do
  if [ "$CONFIG" == "" ]; then
      continue
  fi

	# echo "use $CONFIG"

	# get the parameter format for Riss
	config=`python mapCSSCparams.py $CONFIG`

  ./riss -config= -cmd $config

	# remove all parameters of the default configuration and print the configuration to the screen
	#echo "reduce full config: $config"
	./riss -cmd $config 2> /tmp/SMAC_$$
	params=`cat /tmp/SMAC_$$ | awk '/c tool-parameters/ { for( i = 3; i < NF; ++ i ) printf " %s",$i; print "" }'`

	#./riss -showUnusedParam $params < /dev/null
	echo "$params"
	rm -f /tmp/SMAC_$$
	
done
