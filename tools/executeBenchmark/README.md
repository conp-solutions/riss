# How to execute a benchmark:

1. Copy the directory `template`
2. Enter the new directory
3. Add all necessary files to `files.txt`
4. Set the memory and time limit in `overview.dat`
5. Copy the binary into the directory `bin`
6. Adapt the name of the binary in `start.sh`
7. Run `./start.sh` to create `cmds.txt`. This files containes all commands that
   should be executed on the cluster.
8. Copy `cmds.txt` to client directory of your bserver
9. Run your `binit`
   
   ```bash
    ./binit \
        num_threads=4 \
        time_limit_h=1 \
        time_limit_min=20 \
        memory_limit=4400 \
        dummy_timeout_s=100 \
        global_timeout_h=24 \
        num_nodes=60 \
        num_cores_reserved=16 \
        use_every_nth_core=4; \
    ./bview
   ```

10. Collect evaluation data with `./fastEva.sh`



# How to analyze the output for single file

1. Get the hashcode for the run with
   
   ```bash
   ./hashFile.sh <file.cnf>"
   ```

2. Open the output file in the directory `tmp`
   (.err for stderr, .out for stdout, .runlim for environment)