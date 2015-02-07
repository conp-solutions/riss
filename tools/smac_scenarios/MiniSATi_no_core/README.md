## SMAC run for MiniSATi without tuning core parameters

### instances

Instances are used that MiniSAT could solve between 60s and 360s.

- 138 train instances
- 47 test instances


### wrapper

input:  instance , specifics, cutoff, runlength, seed

Result of this algorithm run: status, runtime, runlength, quality, seed, additional rundata

    status          {CRASHED, TIMEOUT, SAT, UNSAT}
    runtime         runtime of executed solver call
    runlenght       0 - *not measured in this wrapper*
    quality         0 - *not meadured in this wrapper*
    seed            seed-nr


### scenario-parameters:

    --runObj RUNTIME                // minimize runtime of solver runs
                                    // Quality can't be used because it isn't measured in 
    --deterministic 0               // If this is set to true, SMAC will never invoke the target algorithm more 
                                    // than once for any given instance and configuration
    --overall_obj mean10            // objective function used to aggregate multiple runs for a single instance
                                    // Unsuccessful runs are counted as 10 × target run cputime limit
    --cutoff_time 360               // The amount of time in seconds that the target algorithm is permitted to run
                                    // It is the responsibility of the callee to ensure that this is obeyed.
                                    // in wrapper.py: this is the -tmout parameter for the solver 

### parameters in start-script:

    --tunerTimeout 252000            // limits the total cpu time allowed between SMAC and the target algorithm 
                                     // runs during the automatic configuration phase.
                                     // This should be less than slurm-job time! Here: 72h for slurm and 70h for smac
    --validation false               // Turned off becaus The offline Validation Phase, depending on the options used 
                                     // this can also take a large fraction of SMAC’s runtime.
                                     // Standalone Validation can be used: smac-validate.
    
and paths to scenario files:

    --experiment-dir                 // scenario folder
    --outdir                         // smac_output in scenario folder
    --pcs-file                       // params.pcs
    --algo                           // wrapper.py
    --scenario-file                  // scenario.txt
    --instance_file                  // instances-train.txt
    --test_instance_file             // instances-test.txt
    --rungroup                       // sub_folder_name of smac_output
