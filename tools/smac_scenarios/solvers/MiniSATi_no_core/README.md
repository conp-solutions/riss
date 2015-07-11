## MiniSATi without tuning core parameters

### built

cupacd - MiniSATi - commit 5810bbc04


### wrapper

input:  instance , specifics, cutoff, runlength, seed

Result of this algorithm run: status, runtime, runlength, quality, seed, additional rundata

    status          {CRASHED, TIMEOUT, SAT, UNSAT}
    runtime         runtime of executed solver call
    runlenght       0 - *not measured in this wrapper*
    quality         0 - *not meadured in this wrapper*
    seed            seed-nr