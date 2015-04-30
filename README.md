# Riss tool collection. Norbert Manthey. 2014

You can receive the latest copy of the Riss and Coprocessor tool collection from
http://tools.computational-logic.org

Please send bug reports to norbert.manthey@tu-dresden.de


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
cmake -DCMAKE_BUILD_TYPE=Debug ..

# If you want DRAT proof support, configure cmake the following
cmake -DDRATPROOF=ON ..

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
cmake -DOPTION_NAME=value ..
```

| Option          | Description                                            |
| --------------- | ------------------------------------------------------ |
| STATIC_BINARIES |  Build fully statically linked binaries. Default: ON   |
| SHIFTBMC        | Include agier and shiftbmc build targets. Default: OFF |
| WARNINGS        | Set verbose warning flags. Default: OFF                |


## Common Usage

The available parameters can be listed for each tool by calling:

    bin/<tool> --help

Using Riss to solve a formula <input.cnf> use

    bin/riss <input.cnf> [solution] [solverOptions]

Using Riss to solve a formula <input.cnf> and generating a DRUP proof, the 
following call can be used. Depending on the proof verification tool the option
`-no-proofFormat` should be specified. Note, not all formula simplification
techniques support generating a DRUP proof.

    bin/riss <input.cnf> -drup=input.proof -no-proofFormat [solution] [solverOptions]

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
