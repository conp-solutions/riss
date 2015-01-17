## SMAC scenario

Get SMAC from
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/smac-v2.08.00-master-731.tar.gz

manual for SMAC
    http://www.cs.ubc.ca/labs/beta/Projects/SMAC/v2.08.00/manual.pdf

### start one of the SMAC-scenarios

1. Copie the whole smac_scenarios folder into the main smac folder (or create a symlink to the smac_scenario folder)
2. Go into the specific smac scenario folder (e.g. MiniSATi)
3. Add the binary for the sat-solver
4. Rename the binary in wrapper.py (line 19)
5. Add CNFs that should be used to instances-test.txt and instances-train.txt
6. run this in the main smac folder
    ./smac --scenario-file smac_scenarios/MiniSATi/scenario.txt --seed <nr>

### Create new SMAC-scenario

a SMAC-scenario looks like this:

- instances-test.txt
- instances-train.txt
- sat-solver-binary
- params.pcs
- scenario.txt
- wrapper.txt