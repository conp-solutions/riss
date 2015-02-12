#!/bin/bash
#
# this script must be located in the base directory of the CSSC smac environment
#
# 
# start a slurm job for a SMAC scenario
# usage:
#        sbatch ./start-smac.sh <scenario dir> <wallclock limit (just local)>
#        ./start-smac.sh MiniSATi scenario_name trail_file test_file [30] [ROAR
#    TODO: fill complete list
# 
# example call if the smac_scenarios folder is placed within the smac folder:
#        sbatch ./start-smac.sh MiniSATi scenario_name trail_file test_file       -- with slurm
#        ./start-smac.sh MiniSATi scenario_name trail_file test_file [30] [ROAR ...  -- without slurm
#

solver_dir=$1           # name of the solver that should be executed
scenario_name=$2        # name of the scenario file (located in "scenarios" directory)
train_instance_path=$3  # list of training files (relative path from here, or absolute path) 
test_instance_path=$4   # list of test/validation files (relative path from here, or absolute path)
wallclock_limit=$5      # timeout for local execution

shift; shift; shift; shift; shift;

usage="
Usage as slurm-job:
    sbatch --exclusive ./start-smac.sh <smac-folder> <scenario name>
Usage as local:
    ./start-smac.sh <smac-folder> <scenario name> <wallclock limit (just local)>"

if [ -z "$solver_dir" ]
then
        echo "The name of the solver must be given!"
        echo "$usage"
        exit 1
fi
if [ -z "$scenario_name" ]
then
        echo "The name of the scenario must be given!"
        echo "$usage"
        exit 1
fi
#    TODO: IMPLEMENT CHECKS FOR REMAINING PARAMETERS"

# directory in which this script is located
path=$PWD/solvers/$solver_dir

# get create additional command line options for smac, use all options that have been passed additionally to the script
additional_options="$*"

# execute as slurm job on taurus
if [ -z "$wallclock_limit" ]
then
# if the time for the batch job is changed, change tunertimeout below too
#SBATCH --time=72:00:00           # walltime
#SBATCH --ntasks=1                # number of processor cores
#SBATCH --mem-per-cpu=4500M       # memory per CPU core
#SBATCH --partition=sandy,west    # smp, sandy, west, and gpu available 
#SBATCH -A p_sat                  # charge resources to specified project

    # load necessary modules on taurus
    module unload java; module unload python
    module load java/jdk1.7.0_25; module load python/2.7
    
    # this will be the folder name for smac_output of this smac run
    additional_options="--rungroup job_$SLURM_JOB_ID"

else
    echo "------Local run of SMAC with example CNFs---------"
    echo "The output can be found in: [scenarrio-dir]/smac_output/local-$(date +%s)"

    # use example CNFs for a local run
    # do the same as on the cluster!
    # train_instance_path=$PWD/example_instances/instances-train.txt
    # test_instance_path=$PWD/example_instances/instances-test.txt

    # set local output directory
    additional_options="$additional_options --rungroup local_$(date +%s)"
fi

# start smac, smac is located here
# cd $smac_path

# use optimization mode ( executionMode = SMAC) - ROAR is good to find bugs, but does not optimize

echo "call: \
./configurators/smac-v2.06.00-master-615/smac \
--numRun 0 \
--executionMode SMAC \
--outdir smac_output \
--pcs-file $path/params.pcs \
--algo $path/wrapper.py \
--scenario-file scenarios/$scenario_name \
--instance_file $train_instance_path \
--test_instance_file $test_instance_path \
--tunerTimeout 252000 \
--validation false \
$additional_options"

#disabled options:
# we are starting the script in the actual smac directory
# --experiment-dir $path \  
#
# we do not perform the SAT check, as we do not know the result for all files
#--algo \"python scripts/SATCSSCWrapper.py --script ./solvers/minisati/wrapper.py --sat-checker ./scripts/SAT --sol-file ./instances/true_solubility.txt\"

exit 0
