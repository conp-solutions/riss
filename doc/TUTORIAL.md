# Riss Solver Tutorial

This file gives a simple example on how to retrieve the solver, build it on a
Linux system, and run the solver with a basic example. All steps listed in this
tutorial can be executed consecutively.

To have a look how the solver could be used, check the file "scripts/ci.sh",
which calls many different use cases of the solver and implemented tools. If
you only want to use Riss to solver SAT problems from input files, this
tutorial is the right starting point.

## Retrieving the Source

The source code of the solver is available on github. Just get a copy to build
the tool:

```bash
git clone https://github.com/nmanthey/riss-solver.git
cd riss-solver
```

## Building the Solver

The build system uses cmake, where some variables can be used to build the
solver with support for logging proofs or other features. Here, we just use
the default setup:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make riss-core -j $(nproc)
```

After the build completes, there should be a binary riss-core in the directory
bin.

```bash
cd bin
ls -l riss-core
```

## Using the solver

### Accepted Input Format

Riss, as well as many other SAT solvers, read CNF formulas. These formulas are
usually encoded in the DIMACS format, which is a simple plain text file format
which is based on numbers. The file has a header "p cnf #vars #clauses", and
comments can be added on extra lines that start with a 'c'. All other lines
contain clauses, where each clause is terminated with a '0', where a clause
might be spread over multiple lines.

Riss also accepts files that do not have the header. Furthermore, Riss can
parse gzipped DIMACS files.

### An Actual Example

In the following, we create a simple satisfiable formula that encodes that
clause "1 or (not 2) or 3", and store it in the file "input.cnf". Next,
we run Riss and store the output in the file "output.txt". The exit code of
the solver indicates whether the formula was satisfiable (10) or
unsatisfiable (20). If the exit code is 0, then the formula was not solved and
Riss cannot determine the status of the formula.

```bash
echo "1 -2 3 0" > input.cnf
./riss-core input.cnf output.txt
STATUS=$?
echo $STATUS
```

The solver is pretty verbose, and prints lots of information on stderr. This
output can be reduced by adding "-verb=0".

### Parsing the Output

The output of the solver has two important lines. The line starting with 's'
indicates whether the formula has been satisfiable ("s SATISFIABLE") or
unsatisfiable ("s UNSATISFIABLE"). In the former case, a model for the formula
is printed on line(s) starting with a 'v'. For the above input, the following
output is generated.

```bash
cat output.txt
grep "^s" output.txt
grep "^v" output.txt
```

## Other Features

Riss and the implemented tools can be used to solver multiple other problems
(run "riss-core --help -helpLevel=0 --help-verb" for details):

 * Riss can enumerate multiple/all models of a formula
 * Riss can print DRAT proofs to prove unsatisfiability
 * Riss can be used as a library, as it implements the IPASIR interface
 * Riss can print its parameter specification to automatically tune it for a
   benchmark using tools like ParamILS or SMAC
 * Coprocessor can be used to simplify CNF formulas

# Using Coprocessor

**How to run the tools?** For some of the tools, there exists a calling script in the directory scripts.

**How to find first help?** Run the tool with the parameter --help (will print a very long list).

Coprocessor is the build-in preprocessor of the SAT solver riss and has been extended such that it can be used as a standalone binary. This binary can be used to preprocess a formula in CNF and output the undo information as well. The tool can also be used to postprocess the model for the preprocessed formula to obtain a model for the original formula again.

Techniques that are implemented, but not present in SatElite include Failed Literal Probing, Blocked Clause Elimination, Hidden Tautology Elimination and some more. The aim of Coprocessor is to provide a simplifier that is able to run each technique independently or in a user order. This way, preprocessing techniques can be analyzed and optimized for certain use cases.

Since it is also valueable to preprocess SAT formulae, that should be used for optimization procedures (e.g. MaxSAT) or where clauses should be added afterwards, simplifying the formula can be very helpful to increase the overall speed. Coprocessor provides an interface that disallows to touch variables that are influenced by the optimization or by adding new clauses. This way, the simplification can still be executed on the remaining set of variables. Preliminary studies on PB and MaxSAT showed that this technique is valueable.

In this section a brief overview of the usage for the standalone preprocessor is given to enable a quick and easy start with the tool. Most of the presented parameters can also be used for the preprocessor when used before or during search.

**How to compile the tool?** See detailed instructions of the included README.md file

**How to simplify a formula?** By default the whole preprocessor is disabled and no simplification is executed. Depending on the parameters, Coprocessor 3 can output the simplified formula, and a file that stores the information to extend a model of the simplified formula into a model for the original input formula. A typical commandline looks like ./coprocessor INSTANCE -dimacs=OUTPUTFORMULA -cp3_undo=UNDOINFO

**What is a good parameter setup?** Additionally to the above parameters, the following command line seems to well for application instances. If Coprocessor, or Riss is executed with
-enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -unhide -no-cp3_uhdUHLE -cp3_uhdIters=5 -dense
This commandline enabled executes unit propagation, unhiding (Unhide), bounded variable addition (BVA) and bounded variable elimination (BVE) in this order. Finally, the gaps in the variables of the formula are closed, which reduces the maximum variable of the formula. Furthermore, for ternary clauses all self-subsuming resolvents are produced. BVA is limited to touch 120000 clauses. BVE reduces the number of clauses, and does not apply blocked clause elimination. Finally, unhiding is not executing unhiding literal elimination and uses five iterations of the unhiding algorithm. After all simplifications have been executed, the preprocessor prints statistics about the execution and effects of single simpification techniques.

**How to create a model for the input formula?** If the simplified formula has been solved by some SAT solver, the model can be extended for a model of the input formula again. The model needs to be given to the preprocessor via a file. A possible command line can look like ./coprocessor -cp3_post -cp3_undo=UNDOINFO -cp3_model=MODELFILE
Note Coprocessor accepts both the format of SAT competitions and the format of Minisat as models.
Note that you do not need to extend a model, if the formula is unsatisfiable.
Note that you do need the UNDOINFO file, if a correct model should be created.

**How to run variable elimination in parallel?** To run variable elimination in parallel, a number of threads needs to be specified. An example command line is./coprocessor INSTANCE -dimacs=MODEL -enabled_cp3 -bve -cp3_threads=N , where N is the number of threads that should be used.

**How to run single techniques?** The preprocessor enables access to the implemented techniques. Each technique is represented by a single letter. The following list shows the letters and the associated techniques:
u - Boolean Constraint Propagation
s - Self-Subsuming Resolution
v - Variable Elimination
w - Variable Addition
e - Equivalent Literal Elimination
h - Hidden Tautology Elimination
c - Covered Clause Elimination
p - Probing and Failed Literal Detection
x - XOR detection and gaussian elimination
f - Cardinality constraint detection and Fourier-Motzkin reasoning
To choose a technique that should be executed, the parameter -cp3_ptechs has to be specified on the commandline. For running a set of techniques until completion, the corresponding set of letters can be surrounded by '[' and ']+'.
Note, that nested brackets cannot be handled correctly.
Note, each technique that should be used has to be enabled also via the commandline.
This syntax tells the simplifier that it should run the sourrounded techniques until no simplifications can be executed any more. To simplify the formula with Boolean Constraint Propagation, Self-Subsuming Resolution and Equivalent Literal Elimination, type -cp3_ptechs=[use]+.

