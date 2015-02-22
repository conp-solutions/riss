## SMAC scenario

#### Get SMAC from
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/smac-v2.08.00-master-731.tar.gz


#### manual for SMAC
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/v2.08.00/manual.pdf



### Easiest way to use

1. Copy the smac_scenarios folder als a symlink into the smac-folder (otherwise see note above)
2. Copy the solver binary into the existing solvers/<solver-name> or create a new solver-dir in solvers/
3. start smac as one of the following variants:
    
    sbatch --exclusive ./start-smac.sh .. solvers/MiniSATi scenarios/minisati-scenario.txt [--additional-smac-params]

    sbatch --exclusive ./start-smac-parallel.sh .. solvers/MiniSATi scenarios/minisati-scenario-txt [--additional-smac-params] - - - *start 4 smac runs with shared data*

    ./start-smac.sh  .. solvers/MiniSATi scenarios/example-scenario [--additional-smac-params] - - - - - *nice for local runs: will be executed without starting a slurm job*


You can extend these smac calls with any other additional smac-parameter (and overwrite the value from the script), for example:

    ./start-smac.sh  .. solvers/MiniSATi scenarios/example-scenario --wallclock-limit 30 --cutoff-time 10


**Note**: If you don't copy the smac_scenario folder into the smac folder you have to add the instances-path to the smac call:
    
    ... --instance_file <full-path-to-instance-file> --test_instance_file <full-path-to-instance-file>



### Ouput

All ouput-files from SMAC will be saved in "smac_scenarios/smac-output/job-nr/".

To get the result of the SMAC run (if it could finish): 
    
    tail log-run[nr].txt


the output looks like this:

    ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    Estimated mean quality of final incumbent config 3 (internal ID: 0x756E) on test set: 0.013973569870014, based on 5 run(s) on 5 test instance(s).
    Sample call for the final incumbent:

    cd [...]/smac-v2.08.00-master-731; [...]/smac-v2.08.00-master-731/smac_scenarios/MiniSATi/wrapper.py smac_scenarios/example_instances/train/qcplin2006.10408.cnf 0 1.0 2147483647 10688796 -ccmin-mode '2' -cla-decay '0.999' -gc-frac '0.3' -luby 'false' -phase-saving '1' -pol-act-init 'true' -prob-limit '2' -prob-step-width '100' -rfirst '1000' -rinc '1.5' -rlevel '2' -rnd-freq '0.2' -rnd-init 'true' -var-decay '0.95' 

    Additional information about run 40044476 in:[...]/smac-v2.08.00-master-731/smac_scenarios/MiniSATi/smac_output/MiniSATi
    ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------



### folder structur
This smac_scenarios are structured like the CSSC 2014 Test environment [link](http://aclib.net/cssc2014/cssc14.tar.gz).

- instances: list of test and training instances
- solvers: folder with different solvers (binaries, params.pcs, wrapper.py)
- scenarios: different scenario.txt


#### Create new SMAC-scenario

new solver in solvers/-folder:

- sat-solver-binary - - - - - - - *Add the binary to the gitignore file!*
- params.pcs- - - - - - - - - - - *Parameter of the solver that should be tuned [format description](http://aclib.net/cssc2014/pcs-format.pdf)*
- wrapper.txt - - - - - - - - - - - *Translate output from solver to SMAC*


new scenario in scenarios/-folder:

- scenario.txt- - - - - - - - - - - *Parameter for SMAC*


new instances in instances/-folder:

- <name-instances>-test.txt
- <name-instances>-train.txt