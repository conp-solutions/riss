#!/bin/sh 
#
# This is a shell, which automatically runs a user-specific job on a remote machine (in this case 'taurus').
#
# ./runhpc <ssh-login> <local-data> <remote-shell> <local-shell>
#
# The local-shell is optional and should named 'local.sh'. The remote-shell has to be in local-data directory!
# This script creates a new folder named 'fuzzerjobs' in which includes the jobfolders. 
#
# arguments and checking for absolute or regular paths

login=$1
relativePath=$2

if [ -n "$2" ]
then
	localPath=$2

	if [[ "$localPath" != /* ]]
	then
		localPath=$(pwd)"/"$localPath
	fi

else
	echo "Local directory not known! \n
	Creating one if the local-script serves the data! ToDo"
	exit

fi

if [ -n "$3" ]
then
	remoteShell=$3

	if [[ "$remoteShell" == /* ]]
	then
		remoteShell=${remoteShell#*$localPath/}	
	else
		remoteShell=${remoteShell#*$relativePath/}
	fi

else
	echo "No remote-shell is selected!"
	exit

fi

if [ -n "$4" ]
then
	specificPath=$4
	
	if [[ "$specificPath" != /* ]] 
	then
		specificPath=$(pwd)"/"$specificPath
	fi
else
	echo "No local shell-skript to execute! Skip."
fi

# execute shell on the local machine (if there is any) in the specific directory
if [ -n "$specificPath" ]
then
	sh $specificPath $localPath
fi

# create the job directory
remote=$(ssh $login 'path='job'$(date +%N | sed -e 's/000$//' -e 's/^0//')$$ && mkdir -p fuzzerjobs/$path && cd fuzzerjobs/$path && echo fuzzerjobs/$path && exit')
remotePath=$login:'~/'$remote

# writes logs ToDo:  more & better ;)
echo  "Local directory: " $localPath > $localPath/local.log
echo  "Remote directory: " $remotePath >> $localPath/local.log
echo  "Remote directory: " $remotePath

# copy local data to the cluster
scp $localPath/* $remotePath

# execute shell on the cluster (recommendend to start the job) and close the session

ssh $login "cd $remote && sbatch ./$remoteShell && exit" 
			
			
			#~ tar -xf riss.tar.gz &&
			#~ tar -xf coprocessor.tar.gz &&
			#~ mkdir tools &&
			#~ cd tools &&
			#~ mkdir checker &&
			#~ cd checker &&
			#~ mv ../../checker.tar.gz . &&
			#~ tar -xf checker.tar.gz &&
			#~ cd .. &&
			#~ cd .. &&
