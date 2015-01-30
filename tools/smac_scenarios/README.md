## SMAC scenario

#### Get SMAC from
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/smac-v2.08.00-master-731.tar.gz

#### manual for SMAC
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/v2.08.00/manual.pdf


### start a SMAC-scenario

1. *new scenario*: Create a scenario folder as described below in the smac_scenarios folder and continue with 4
   
2. *existing scenario*: Add the binary for the sat-solver in its scenario folder
3. Rename the binary in wrapper.py (line 19)
4. run this in the smac_scenarios folder

    sbatch ./start-smac.sh <path to smac> <scenario dir>

    ./start-smac.sh <path to smac> <scenario dir> local - - - - - *will be executed without starting a slurm job*


### Create new SMAC-scenario

a SMAC-scenario looks like this:

- instances-test.txt
- instances-train.txt
- sat-solver-binary - - - - - - - *Add the binary to the gitignore file!*
- params.pcs- - - - - - - - - - - *Parameter of the solver that should be tuned [format description](http://aclib.net/cssc2014/pcs-format.pdf)*
- scenario.txt- - - - - - - - - - - *Parameter for SMAC*
- wrapper.txt - - - - - - - - - - - *Translate output from solver to SMAC*