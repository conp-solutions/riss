#!/bin/sh
#
# This script will be executed on the cluster.
# 
#
#
#

#SBATCH --time=00:01:00   # walltime
#SBATCH --ntasks=1   # number of processor cores (i.e. tasks)
#SBATCH --partition=sandy
#SBATCH --mem-per-cpu=1024M   # memory per CPU core
#SBATCH --mail-user=aaron.stephan@tu-dresden.de   # email address
#SBATCH --mail-type=END
#SBATCH -A p_sat

module load gcc/4.8.0 git java/jdk1.7.0_25 ; module unload python ; module load python/2.7; module load valgrind

# cd $(dirname $0) does not work on taurus :( ! 

#~ make d
#~ make coprocessord
#~  
#~ make

cd tools/checker  
./fuzzcheck.sh ./solver.sh

exit
