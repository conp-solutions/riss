# Riss tool collection. Norbert Manthey. 2015

You can receive the latest copy of the Riss and Coprocessor tool collection from
http://tools.computational-logic.org

Please send bug reports to norbert.manthey@tu-dresden.de

All tools in this package are highly configurable. For a simple comparison, most 
techniques are disabled by default. Hence, you should specify options to use. For
both Riss and Coprocessor, using -config=Riss427 is a fair starting point.

## Components

This software package contains the SAT solver Riss, and might contain related 
tools:

| Tool        | Description                                 |
| ----------- | ------------------------------------------- |
| Coprocessor |  CNF simplifier                             |
| Qprocessor  |  QCNF simplifier                            |
| Mprocessor  |  MaxSAT simplifier                          |
| ShiftBMC    |  BMC solver                                 |
| Priss       |  simple portfolio SAT solver                |
| Pcasso      |  parallel search space splitting SAT solver |
| Classifier  |  CNF feature extraction tool                |


## Building

The tool box uses [CMake](http://cmake.org/) as build tool chain. In-source
builds are not recommended. It is better to build the different build types
(release, debug) in a separate directory.

```bash
# Create a directory for the build
mkdir debug
cd debug/

# Create a debug build configuration
cmake -D CMAKE_BUILD_TYPE=Debug ..

# Create release build Intel compiler
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_CXX_COMPILER=icpc ..

# If you want DRAT proof support, configure cmake the following
cmake -D DRATPROOF=ON ..

# You get a list of all targets with
make help

# Two different configurations of the riss solver
# (with and without simplification enabled)
make riss-simp
make riss-core

# Copy all scripts to the bin/ directory
make scripts
```

### Options

To configure your build, pass the described options to cmake like this

```bash
cmake -D OPTION_NAME=value ..
```

| Option             | Description                                            | Default |
| ------------------ | ------------------------------------------------------ | ------- |
| CMAKE_BUILD_TYPE   | "Debug" or "Release"                                   | Release |
| CMAKE_CXX_COMPILER | C++ compiler that should be used                       |     g++ |
| STATIC_BINARIES    | Build fully statically linked binaries                 |      ON |
| SHIFTBMC           | Include build targets for shiftbmc and aiger library   |     OFF |
| AIGER-TOOLSS       | Include build targets for all agier executables        |     OFF |
| WARNINGS           | Set verbose warning flags                              |     OFF |
| QUIET              | Disable Wcpp                                           |     OFF |
| CLASSIFIER         | Use the classifier in Riss and Pcasso                  |     OFF |


## Incremental Solving

Riss supports two different C interfaces, where one is the IPASIR interface, which has been
set up for incremental track of the SAT Race in 2015. The actual interface of Riss supports
a few more routines. Furthermore, Coprocessor's simplification can be used via a C interface.

### Build the solver for the IPASIR interface

After running cmake (see above), build the following library:

```bash
make riss-coprocessor-lib-static
```
Then, include the header file "riss/ipasir.h" into your project, and link against the library.

## Common Usage

The available parameters can be listed for each tool by calling:

    bin/<tool> --help

Due to the large number of parameters, a more helpful alternative is:
    
    bin/<tool> --help -helpLevel=0 --help-verb

The configuration specification can be written to a pcs file automatically. 
Before using this file with an automated configuration framework, please check
that only necessary parameters appear in the file. The procedure will include 
all parameters, also the cpu or memory limit parameters, and is currently considered
experimental.

  bin/<tool> -pcs-file=<pcs-filename>
  

Using Riss to solve a formula <input.cnf> use

    bin/riss <input.cnf> [solution] [solverOptions]

Using Riss to solve a formula <input.cnf> and generating a DRUP proof, the 
following call can be used. Depending on the proof verification tool the option
`-no-proofFormat` should be specified. Note, not all formula simplification
techniques support generating a DRAT proof.

    bin/riss <input.cnf> -proof=input.proof -no-proofFormat [solution] [solverOptions]

The script `cp.sh` provides a simple setup for using Coprocessor as a formula
simplification tool and afterwards running a SAT solver. The used SAT solver can
be exchanged by changing the variable `satsolver` in the head of the script.

    bin/cp.sh <input.cnf> [coprocessorOptions]

The script `qp.sh` provides a simple setup for using Qprocessor as a QBF formula
simplification tool and afterwards running a QBF solver. The used QBF solver can
be exchanged by changing the variable "qbfsolver" in the head of the script. The 
simplification techniques setup of the QBF simplifier is limited at the moment 
and no further simplifications should be enabled.

    bin/qp.sh <input.qcnf> [qbfsolverOptions]

The CNF feature extraction tool classifier can be used to extract features from
a CNF file by calling the tool as follows. Features can be disabled separately.
For more information use the "--help" parameter.

    bin/classifier <input.cnf>

The parallel portfolio solver priss uses incarnations of riss and executes them in
parallel. To obtain a version that executes exact copies of the solver, issue the 
following call, and add the CNF formula as well. Furthermore, you might want to specify
the number of used threads by adding "-threads=X"

    bin/priss -ppconfig= -no-addSetup -no-pr -no-ps -psetup=PLAIN -pAllSetup=-independent


## Third party libraries

The configuration prediction code is based on two external libraries:

 * [Armadillo](http://arma.sourceforge.net/). MLP License. Linear algebra library for C++
 * [libpca](http://sourceforge.net/projects/libpca/). MIT License. C++ library for principal component analysis
