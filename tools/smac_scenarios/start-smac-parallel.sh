#!/bin/bash
#
# This script starts a parallel smac scenario with the given parameters as a slurm job or without slurm
# 
# example call if the smac_scenarios folder is placed within the smac folder:
#        with slurm: sbatch --exclusive ./start-smac-parallel.sh .. solvers/MiniSATi minisat-scenario.txt
#        local:      ./start-smac-parallel.sh .. solvers/MiniSATi scenarios/example-scenario.txt [--wallclock-limit 30] [--cutoff-time 10] [--executionMoade ROAR]
#

smac_path=$1            # path to smac binary
solver_dir=$2           # name of the solver that should be executed
scenario_name=$3        # name of the scenario file (located in "scenarios" directory)

shift; shift; shift;

usage="
Usage as slurm-job:
    sbatch --exclusive ./start-smac-parallel.sh <smac-path> <solver-dir> <scenario name> <--additional smac-parameter>
Usage as local:
    ./start-smac-parallel.sh <smac-path> <solver-dir> <scenario name> <--additional smac-parameter>"

if [ -z "$smac_path" ]
then
        echo "The path where the smac binary is located must be given!"
        echo "$usage"
        exit 1
fi
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

# directory in which this script is located
path=$PWD

# get create additional command line options for smac, use all options that have been passed additionally to the script
additional_options="$*"

# execute as slurm job on taurus

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
if [ -n "$SLURM_JOB_ID" ]; then
    additional_options="$additional_options --rungroup job_$SLURM_JOB_ID"
else
    echo "------Local run of SMAC with example CNFs---------"
    echo "The output can be found in: [scenarrio-dir]/smac_output/local-$(date +%s)"

    # set local output directory
    additional_options="$additional_options --rungroup local_$(date +%s)"
fi

# the tunerTimeout has to be less than sbatch-time/i
export SMAC MEMORY=4096
cd $smac_path

for i in `seq 1 4`;
do
    echo "start SMAC run: $i"
    ./smac \
    --executionMode SMAC \
    --outdir $path/smac_output \
    --pcs-file $path/$solver_dir/params.pcs \
    --algo $path/$solver_dir/wrapper.py \
    --scenario-file $path/$scenario_name \
    --validation false \
    --tunerTimeout 63000 \
    --shared-model-mode true \
    $additional_options
done

exit 0
