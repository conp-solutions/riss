#!/bin/bash
# 
# start a slurm job for a SMAC scenario
# usage:
#        sbatch ./start-smac.sh <path/to/smac-folder> <scenario dir> <wallclock limit (just local)>
# 
# example call if the smac_scenarios folder is placed within the smac folder:
#        sbatch ./start-smac.sh .. MiniSATi       -- with slurm
#        ./start-smac.sh .. MiniSATi 30           -- without slurm
#

smac_path=$1
scenario_dir=$2
wallclock_limit=$3

usage="
Usage as slurm-job:
    sbatch --exclusive ./start-smac.sh <smac-folder> <scenario name>
Usage as local:
    ./start-smac.sh <smac-folder> <scenario name> <wallclock limit (just local)>"

if [ -z "$smac_path" ]
then
        echo "Path where the smac-binary is loacted must be given! (Without binary name)"
        echo "$usage"
        exit 1
fi
if [ -z "$scenario_dir" ]
then
        echo "The scenario name must be given!"
        echo "$usage"
        exit 1
fi

# directory in which this script is located
path=$PWD/$scenario_dir

train_instance_path=$path/instances-train.txt
test_instance_path=$path/instances-test.txt

# get create additional command line options for smac
additional_options=""

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
    module load java/jdk1.7.0_25; module load python/2.7
    
    # this will be the folder name for smac_output of this smac run
    additional_options="--rungroup job_$SLURM_JOB_ID"

else
    echo "------Local run of SMAC with example CNFs---------"
    echo "The output can be found in: [scenarrio-dir]/smac_output/local-$(date +%s)"

    # use example CNFs for a local run
    train_instance_path=$PWD/example_instances/instances-train.txt
    test_instance_path=$PWD/example_instances/instances-test.txt

    # set small wallclock time (should overwrite tunerTimeout)
    additional_options="--wallclock-limit $wallclock_limit
                        --rungroup local_$(date +%s)"
fi

# start smac
cd $smac_path

echo "call: \
./smac  --experiment-dir $path \
        --outdir $path/smac_output \
        --pcs-file $path/params.pcs \
        --algo $path/wrapper.py \
        --scenario-file $path/scenario.txt \
        --instance_file $train_instance_path \
        --test_instance_file $test_instance_path \
        --tunerTimeout 252000 \
        --validation false \
        $additional_options"

exit 0
