#
#
# given the configuration file $1, and a directory $2, create directories to be submitted to the cluster 
# the only difference to $2 will be the parameter configuration (stored in params.txt)
#
#

# start with config 0
i=0

# iterate over all configurations and create the directories
cat $1 | while read CONFIG
do
  if [ "$CONFIG" == "" ]; then
      continue
  fi

	i=$(($i+1))
	# copy the directory
	cp -r $2 CONF-$i
	# enter the directory
	cd CONF-$i
	# modify the parameter configuration
	echo "$CONFIG" > params.txt
	# leave the directory
	cd ..;

done
