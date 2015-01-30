#!/bin/bash
# 
# start a slurm job for a SMAC scenario
# usage:
#        sbatch ./start-smac.sh <path/to/smac> <scenario dir> <execute local (no slurm)>
# 
# example call if the smac_scenarios folder is placed within the smac folder:
#        sbatch ./start-smac.sh .. MiniSATi       -- with slurm
#        ./start-smac.sh .. MiniSATi local        -- without slurm
#

smac_path=$1
scenario_dir=$2
execute_local=$3

if [ -z "$smac_path" ]
then
        echo "Path to smac-binary must be given!"
        exit 1
fi
if [ -z "$scenario_dir" ]
then
        echo "The scenario dir must be given!"
        exit 1
fi


path=$PWD/$scenario_dir

# get create optional command line options for smac (maybe expanded)
optional_options=""

# execute as slurm job on taurus
if [ -z "$execute_local" ]
then
    #SBATCH --time=00:01:00           # walltime
    #SBATCH --ntasks=1                # number of processor cores
    #SBATCH --mem-per-cpu=1024M       # memory per CPU core
    #SBATCH --partition=sandy         # smp, sandy, west, and gpu available 
    #SBATCH -A p_sat                  # charge resources to specified project

    # load necessary modules on taurus
    module load java/jdk1.7.0_25; module load python/2.7
    
    optional_options="--rungroup job_$SLURM_JOB_ID"
fi

# start smac
cd $smac_path
./smac  --experiment-dir $path \
        --outdir $path/smac_output \
        --pcs-file $path/params.pcs \
        --instance_file $path/instances-train.txt \
        --test_instance_file $path/instances-test.txt \
        --algo $path/wrapper.py \
        --scenario-file $path/scenario.txt \
        $optional_options

echo $SLURM_JOB_ID

exit 0