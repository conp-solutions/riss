# ChangeLog for coprocessor

## v3.0.0

 * Persistent heap for sequential BVE (lucas)
 * Technique class with **CRTP** (static polymorphism) (lucas)
 * Generic stepper class for all techniques (lucas)
 * Move all technique implementations into [coprocessor/techniques](techniques/) (lucas)
 * Use Stepper for BVE (lucas)
 * Use Stepper for Subsumption (lucas)
 * Increase strengthening limit in "giveMoreSteps()" call of Subsumption (lucas)
 * Renames `deleteTimer` into `modTimer`
 * Introduce proper technique initialization (firstly used by BVE) (lucas)
