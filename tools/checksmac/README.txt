This package uses the CSSC 2013 package to build a simple check tool 
 
             checksmac

to find bugs in configurable SAT solvers.


   ___  __  __    ___   ____   ___  _ _____ 
  / __\/ _\/ _\  / __\ |___ \ / _ \/ |___ / 
 / /   \ \ \ \  / /      __) | | | | | |_ \ 
/ /___ _\ \_\ \/ /___   / __/| |_| | |___) |
\____/ \__/\__/\____/  |_____|\___/|_|____/  

This is a test environment for preparing an entry to the Configurable SAT 
Solver Competition, CSSC 2013.

The folder structure is identical to what we'll use in the competition.
Your submission will go into a subdirectory of solvers/ and you should not
have to change any of the other files. In fact, any changes you make outside
that folder won't be part of your submission, so if you believe something
outside that folder needs to be changed please email us at
cssc.organizers@gmail.com.

We provided a few examples of parameterized solvers as subdirectories of the 
solvers/ directory. Each of these subdirectories would constitute a valid 
submission, so if you use one of them as an example everything should work.

In order to use this test environment, you'll need Linux (sorry Mac users,
the runsolver tool won't work for you) with Ruby installed.

==========================
Components of a submission
==========================
(1) Source code or binary for your solver. 
(2) Parameter configuration space (.pcs) specification file
(3) A short script translating a partial parameter instantiation to a command
    line call to your solver, setting those parameters.

Some information about each of these components:

(1) We use exactly the same input/output format as the 2013 SAT competition 
    (and in fact the same scripts to verify correctness). Only submissions 
    with source code are eligible for medals.

(2) This file specifies your solver's parameters and their domains, as well as 
    dependencies between parameters. Please see pcs-format.pdf for the details
	and the .pcs files of the provided solvers for examples.
	We encourage you to include two versions of this file: one that natively
	handles numerical parameters, and one that uses your domain knowledge
	to pick some interesting values for them and treats them as categorical.
	There are two reasons for this request: (1) ParamILS can only handle 
	categorical parameters and it might be the best configurator for your 
	solver. (2) In some cases, we have noticed that the domain knowledge going
	into the discretization results in *better* performance than when searching
	the "proper" range of the numerical parameter.	
	See solvers/spear/spear-params.pcs for an example of a discretized version 
	of the mixed continuous/discrete file spear-params-mixed.pcs.  

(3) We require your solver directory to contain a Ruby script called 
    callstring_generator.rb that implementes a method with the following 
    signature. 
    Input:  an instance name (String), a seed (int), and a Hashmap mapping 
            parameter names to their values.
    Output: An executable command line callstring to your solver, to run on
            the provided instance, with the provided seed (if you support 
			that, not crucial), and with the provided parameter values.
   Using the provided example solvers for reference, it should not be hard to 
   fill in this function; it typically has about 7 lines.

   However, if you don't want to touch any Ruby code you don't have to; 
   instead, you can provide a script (written in your programming language of 
   choice) that reads the above inputs from a file, creates the command line 
   call using them and outputs the call to stdout. 
   You would then copy callstring_generator.rb from the example solver 
   directory minisat_with_python_wrapper, which writes the above inputs to a 
   file, calls your script and reads its output to get the command line call.

   
=======================
Testing your submission
=======================
Various things can go wrong when you first parameterize your solver:
- Path issues: this is one of the common problems when wrapping a solver for
  configuration. Here, we minimized the risk for this problem by defining the 
  directory structure we will use. Everything should work if you put the files
  for (2) and (3) above in the root of your solver subdirectory, and make sure
  that the command line call generated with the script in (3) is executable 
  from the root of your solver subdirectory (e.g., put a binary of your solver
  there). If you don't change the remaining files there shouldn't be any path 
  issues when we wrap your solver call with runsolver, solution checking, etc.

- Input parsing bugs: make sure that your solver actually uses the parameter
  values defined in your command line call. We simply execute that call and
  cannot check the solver's internals.

- Bugs with certain parameter values: non-default solver options are tested
  less frequently than the defaults, so it is likely that there are some bugs.
  The SMAC configurator, which is part of this download, can be used as a 
  rudimentary fuzz testing tool. When called with the option 
  "--executionMode ROAR" as below, it generates random instantiations of 
  parameters, runs your solver with them on easy instances and checks its 
  result. When called without that option, it performs a targeted search for 
  good configurations. In either case, when you use the flag 
  "--abortOnCrash true" it terminates whenever a solver run crashes.


=======================
What exactly to submit:
=======================
To submit, simply zip up your subdirectory of the solvers/ directory and send
it to us. If you want to be eligible for medals also include source and 
information on how to build the binary from it.
Before you submit, please ensure that an example configuration run using SMAC
goes through without errors. We give examples and hints on debugging your
submission below.


=======================================================================
Examples: some calls of SMAC for our example solvers and instance sets.
=======================================================================
We strongly suggest that before you run SMAC on your own solver, you make sure
everything is set up correctly by running it on one of the example solvers we
provide. Below you can find several example calls to execute.
Note that each of these calls will run SMAC for up to 4000 seconds, generating
lots of (somewhat cryptic) output. You don't need to run SMAC to completion  
for every example (Control-C will terminate it, with a "friendly" exception); 
you can also control SMAC's budget with the wallClockLimit parameter.
The important thing is that SMAC doesn't crash and stop; if it crashes for one
of the provided examples something bad and unexpected happened - please email 
us in that case. 

The syntax for calling SMAC and its random sampling variant ROAR is as follows:
./configurators/smac-v2.04.01-master-415/smac --numRun 0 --wallClockLimit 4000 --doValidation false --abortOnCrash true [--executionMode ROAR] --wallClockLimit 4000 --executionMode ROAR --numRun 0 --abortOnCrash true --scenarioFile <scenarioFile, provided by us> --algo "ruby ../scripts/generic_solver_wrapper.rb <name of your solver subdirectory>" --paramfile solvers/<name of your solver subdirectory>/<name of your solver parameters file>

Example 1: MiniSAT on a regressiontest benchmark suite, trying random parameter configurations:
./configurators/smac-v2.04.01-master-415/smac --numRun 0 --wallClockLimit 4000 --doValidation false --abortOnCrash true --executionMode ROAR --scenarioFile ./scenarios/regressiontests-5s-1h.txt --algo "ruby ../scripts/generic_solver_wrapper.rb minisat" --paramfile solvers/minisat/minisat-params.pcs 

Example 2: MiniSAT on a regressiontest benchmark suite, actual search for good parameter configurations:
./configurators/smac-v2.04.01-master-415/smac --numRun 0 --wallClockLimit 4000 --doValidation false --abortOnCrash true --scenarioFile ./scenarios/regressiontests-5s-1h.txt --algo "ruby ../scripts/generic_solver_wrapper.rb minisat" --paramfile solvers/minisat/minisat-params.pcs 

Example 3: MiniSAT on SWV:
./configurators/smac-v2.04.01-master-415/smac --numRun 0 --wallClockLimit 4000 --doValidation false --abortOnCrash true --scenarioFile ./scenarios/CSSC-SWV-GZIP-5s-1h.txt --algo "ruby ../scripts/generic_solver_wrapper.rb minisat" --paramfile solvers/minisat/minisat-params.pcs

Example 4: MiniSAT (with Python wrapper) on SWV:
./configurators/smac-v2.04.01-master-415/smac --numRun 0 --wallClockLimit 4000 --doValidation false --abortOnCrash true --scenarioFile ./scenarios/CSSC-SWV-GZIP-5s-1h.txt --algo "ruby ../scripts/generic_solver_wrapper.rb minisat_with_python_wrapper" --paramfile solvers/minisat_with_python_wrapper/minisat-params.pcs 

Example 5: MiniSAT on 3-SAT:
./configurators/smac-v2.04.01-master-415/smac --numRun 0 --wallClockLimit 4000 --doValidation false --abortOnCrash true --scenarioFile ./scenarios/CSSC-uf250-5s-1h.txt --algo "ruby ../scripts/generic_solver_wrapper.rb minisat" --paramfile solvers/minisat/minisat-params.pcs

Example 6: SPEAR on SWV:
./configurators/smac-v2.04.01-master-415/smac --numRun 0 --wallClockLimit 4000 --doValidation false --abortOnCrash true --scenarioFile ./scenarios/CSSC-SWV-GZIP-5s-1h.txt --algo "ruby ../scripts/generic_solver_wrapper.rb spear" --paramfile solvers/spear/spear-params.pcs


=========================
Debugging your submission
=========================
If SMAC runs on the above examples you can try it for your own solver.
Simply take one of the callstrings above and replace the solver directory 
and the parameter file with yours.

If SMAC runs on the solver examples we provided but crashes for your own 
solver, then there is a problem with either your parameter file, your command 
line generation method, or your solver. You would get one of the following 
problems:

Problems locating the parameter file yield an exception like the following:
[INFO ] Parsing Parameter Space File
Error occured running SMAC ( ParameterException : Could not find a valid parameter file, please check if there was a previous error)

Syntactical problems with the parameter file yield an exception like the following:
[INFO ] Parsing Parameter Space File
Error occured parsing: <example for an incorrect line in .pcs file>
Error occured running SMAC ( IllegalArgumentException : Cannot parse the following line:<example for an incorrect line in .pcs file>)

Any other errors related to your wrapper function or your solver that lead to 
a crash of SMAC will cause SMAC to output a line like the following as part 
of its error message:
[INFO ] Failed Run Detected Call: cd /global/home/hutter/cssc/solvers ;  ruby ../scripts/generic_solver_wrapper.rb minisat instances/Sat_Data/bench/SW-verification/GZIP_v1.2.4__v1.1/gzip_vc1031.cnf UNSATISFIABLE 5.0 2147483647 10806935 -ccmin-mode '2' -cla-decay '0.999' -gc-frac '0.2' -luby '1' -phase-saving '2' -rfirst '100' -rinc '2' -rnd-freq '0' -rnd-init '0' -var-decay '0.95'

You can debug all of these latter errors by simply copy-pasting the above 
command line call (include the cd statement) and manually executing it on 
the command line. 
These problems can range anywhere from a syntax error in your wrapper function,
a problem locating your solver binary, a problem in the generated command 
line, a segmentation fault in the solver when run with a certain parameter 
configuration, or a bug in the solver (e.g. yielding an incorrect solution
for a certain instance when run with a certain parameter configuration).
Once you fixed the cause of the problem simply run SMAC again and iterate 
until no problems remain.

For a variety of conditions, you should try all the 3 scenarios above before 
submitting:
./scenarios/regressiontests-5s-1h.txt (heterogeneous, thus not too interesting for configuration, but a good scenario for debugging)
./scenarios/CSSC-SWV-GZIP-5s-1h.txt (software verification instances generated with the static checker Calysto on gzip)
./scenarios/CSSC-uf250-5s-1h.txt (satisfiable uniform random 3-SAT with 250 variables at the phase transition)
In all these simple scenarios, each solver run is capped at 5 seconds (as 
opposed to 300 seconds in the competition), and the budget for configuration 
is 1h as opposed to 2 days in the competition. 
Once everything goes through for these 3 cases, you're ready to submit :-)

=====================
Questions & Feedback: 
=====================
Please send us email at cssc.organizers@gmail.com

Credits: the ASCII graphic at the top of this file was generated with the tool at http://www.network-science.de/ascii/
