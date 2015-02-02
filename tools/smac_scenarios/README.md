## SMAC scenario

#### Get SMAC from
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/smac-v2.08.00-master-731.tar.gz

Easiest way to use this scenarios: copy the smac_scenarios folder als a symlink into the smac-folder.

#### manual for SMAC
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/v2.08.00/manual.pdf


### start a SMAC-scenario

1. *new scenario*: Create a scenario folder as described below in the smac_scenarios folder and continue with 4
   
2. *existing scenario*: Add the binary for the sat-solver in its scenario folder
3. Rename the binary in wrapper.py (line 19)
4. run this in the smac_scenarios folder

    sbatch --exclusive ./start-smac.sh  smac-path  scenario-dir

    sbatch --exclusive ./start-smac-parallel.sh smac-path scenario-dir - - - *start 4 smac runs with shared data*

    ./start-smac.sh  smac-path  scenario-dir  30 - - - - - *nice for local runs: will be executed without starting a slurm job*



### Ouput

All ouput-files from SMAC will be saved in "[scenario-dir]/smac-output/job-nr/".

To get the result of the SMAC run (if it could finish): 
    
    tail log-run[nr].txt


the output looks like this:


    ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    Estimated mean quality of final incumbent config 3 (internal ID: 0x756E) on test set: 0.013973569870014, based on 5 run(s) on 5 test instance(s).
    Sample call for the final incumbent:

    cd [...]/smac-v2.08.00-master-731; [...]/smac-v2.08.00-master-731/smac_scenarios/MiniSATi/wrapper.py smac_scenarios/example_instances/train/qcplin2006.10408.cnf 0 1.0 2147483647 10688796 -ccmin-mode '2' -cla-decay '0.999' -gc-frac '0.3' -luby 'false' -phase-saving '1' -pol-act-init 'true' -prob-limit '2' -prob-step-width '100' -rfirst '1000' -rinc '1.5' -rlevel '2' -rnd-freq '0.2' -rnd-init 'true' -var-decay '0.95' 

    Additional information about run 40044476 in:[...]/smac-v2.08.00-master-731/smac_scenarios/MiniSATi/smac_output/MiniSATi
    ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------



### Create new SMAC-scenario

a SMAC-scenario looks like this:

- instances-test.txt
- instances-train.txt
- sat-solver-binary - - - - - - - *Add the binary to the gitignore file!*
- params.pcs- - - - - - - - - - - *Parameter of the solver that should be tuned [format description](http://aclib.net/cssc2014/pcs-format.pdf)*
- scenario.txt- - - - - - - - - - - *Parameter for SMAC*
- wrapper.txt - - - - - - - - - - - *Translate output from solver to SMAC*
