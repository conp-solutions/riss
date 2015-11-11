/**************************************************************************************[librissc.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include <string>
#include <algorithm> // std::remove
#include "coprocessor/Coprocessor.h"
#include "riss/utils/version.h"

using namespace std;
using namespace Riss;

#include "riss/librissc.h"

/** struct that stores the necessary data for a preprocessor */
struct libriss {
    Riss::Solver* solver;
    Coprocessor::CP3Config* cp3config;
    Riss::CoreConfig* solverconfig;
    Riss::vec<Riss::Lit> currentClause; // current clause that is added to the solver
    Riss::vec<Riss::Lit> assumptions;   // current set of assumptions that are used for the next SAT call
    Riss::vec<char> conflictMap;        // map that stores for the last conflict whether a variable is present in the conflict (result of analyzeFinal)
    Riss::lbool lastResult;
    libriss() : solver(0), cp3config(0), solverconfig(0), lastResult(l_Undef) {}  // default constructor to ensure everything is set to 0
};


/** construct the conflict map that nidicates whether a variable is present in the final conflict
 *  Note: if the map has already a size, its not rebuild (has to be cleared once the last conflict is not valid any longer)
 */
static
void riss_build_conflict_map(libriss* solver)
{
    if (solver->conflictMap.size() != 0) { return; }
    assert(solver->lastResult == l_False && "works only if the last result was unsatisfiable");
    solver->conflictMap.growTo(solver->solver->nVars());   // one spot for each variable

    // set the flag for all variables of the current conflict clause
    const vec<Lit>& finalConflict = solver->solver->conflict;
    for (int i = 0 ; i < finalConflict.size(); ++ i) {
        solver->conflictMap[ var(finalConflict[i]) ] = 1;
    }
}

/** clears the current conflict map, so that the map can be re-created for the next call.
 *  Note: keeps the memory of the map (no need to re-alloc)
 */
static
void riss_reset_conflict_map(libriss* solver)
{
    solver->conflictMap.clear();
}

// #pragma GCC visibility push(hidden)
// #pragma GCC visibility push(default)
// #pragma GCC visibility pop // now we should have default!

//#pragma GCC visibility push(default)

extern "C" {

    /** return the name of the solver and its version. The signature must be a
     *  valid C-identifier.
     *  @return string that contains the verison of the solver
     */
    extern const char* riss_signature()
    {
        // create a signature that can be used as C-identifier
        // remove all periods (".") in the version string and use "riss_" as prefix
        string signature = solverVersion;

        // @see http://stackoverflow.com/a/5891643/2467158
        signature.erase(remove(signature.begin(), signature.end(), '.'), signature.end());

        // solver name as prefix
        return ("riss_" + signature).c_str();
    }

    /** initialize a solver instance, and return a pointer to the maintain structure
    */
    void*
    riss_init()
    {
        libriss* riss = new libriss;
        riss->solverconfig = new Riss::CoreConfig("");
        riss->cp3config = new Coprocessor::CP3Config("");
        riss->solver = new Riss::Solver(riss->solverconfig) ;
        riss->solver->setPreprocessor(riss->cp3config);
        return riss;
    }

    /** initialize a solver instance, and return a pointer to the maintain structure
    * @param presetConfig name of a configuration that should be used
    */
    void*
    riss_init_configured(const char* presetConfig)
    {
        libriss* riss = new libriss;
        riss->solverconfig = new Riss::CoreConfig(presetConfig == 0 ? "" : presetConfig);
        riss->cp3config = new Coprocessor::CP3Config(presetConfig == 0 ? "" : presetConfig);
        riss->solver = new Riss::Solver(riss->solverconfig) ;
        riss->solver->setPreprocessor(riss->cp3config);
        return riss;
    }

    /** set the random seed of the solver
     * @param seed random seed for double random generator ( must be between 0 and 1 )
     */
    void riss_set_randomseed(void* riss, double seed)
    {
        libriss* solver = (libriss*) riss;
        solver->solver->setRandomSeed(seed);
    }

    /** free the resources of the solver, set the pointer to 0 afterwards */
    void
    riss_destroy(void** riss)
    {
        libriss* solver = (libriss*) *riss;
        delete solver->solver;
        delete solver->cp3config;
        delete solver->solverconfig;
        delete solver;
        *riss = 0;
    }

    /** add a new variables in the solver
     * @return number of the newly generated variable
     */
    int riss_new_variable(const void* riss)
    {
        libriss* solver = (libriss*) riss;
        return solver->solver->newVar() + 1;
    }

    /** add a literal to the solver, if lit == 0, end the clause and actually add it */
    int riss_add(void* riss, const int lit)
    {
        libriss* solver = (libriss*) riss;
        solver->lastResult = l_Undef; // set state of the solver to l_Undef
        bool ret = false;
        if (lit != 0) { solver->currentClause.push(lit > 0 ? mkLit(lit - 1, false) : mkLit(-lit - 1, true)); }
        else { // add the current clause, and clear the vector
            // reserve variables in the solver
            for (int i = 0 ; i < solver->currentClause.size(); ++i) {
                const Lit l2 = solver->currentClause[i];
                const Var v = var(l2);
                while (solver->solver->nVars() <= v) { solver->solver->newVar(); }
            }
            ret = solver->solver->addClause_(solver->currentClause);
            solver->currentClause.clear();
        }
        return ret ? 1 : 0;
    }

    /** add the given literal to the assumptions for the next solver call */
    void
    riss_assume(void* riss, const int lit)
    {
        libriss* solver = (libriss*) riss;
        solver->lastResult = l_Undef; // set state of the solver to l_Undef
        solver->assumptions.push(lit > 0 ? mkLit(lit - 1, false) : mkLit(-lit - 1, true));
    }


    /** add a variable as prefered search decision (will be decided in this order before deciding other variables) */
    void riss_add_prefered_decision(void* riss, const int variable)
    {
        libriss* solver = (libriss*) riss;
        solver->solver->addPreferred(variable - 1);
    }

    /** clear all prefered decisions that have been added so far */
    void riss_clear_prefered_decisions(void* riss)
    {
        libriss* solver = (libriss*) riss;
        solver->solver->clearPreferred();
    }

    /** set a callback to a function that should be frequently tested by the solver to be noticed that the current search should be interrupted
     * Note: the state has to be used as argument when calling the callback
     * @param state pointer to an external state object that is used in the termination callback
     * @param terminationCallbackMethod pointer to an external callback method that indicates termination (return value is != 0 to terminate)
     */
    void riss_set_termination_callback(void* riss, void* terminationState, int (*terminationCallbackMethod)(void* state))
    {
        libriss* solver = (libriss*) riss;
        solver->solver->setTerminationCallback(terminationState, terminationCallbackMethod);
    }

    /** apply unit propagation (find units, not shrink clauses) and remove satisfied (learned) clauses from solver
     * @return 1, if simplification did not reveal an empty clause, 0 if an empty clause was found (or inconsistency by unit propagation)
     */
    int riss_simplify(const void* riss)
    {
        libriss* solver = (libriss*) riss;
        solver->lastResult = l_Undef; // set state of the solver to l_Undef
        return solver->solver->simplify() ? 1 : 0;
    }


    /** solve the formula that is currently present (riss_add) under the specified assumptions since the last call
     * Note: clears the assumptions after the solver run finished
     * add the nOfConflicts limit -1 (infinite)
     * @return status of the SAT call: 10 = satisfiable, 20 = unsatisfiable, 0 = not finished within number of conflicts
     */
    int
    riss_sat(void* riss)
    {
        return riss_sat_limited(riss, -1);
    }

    /** solve the formula that is currently present (riss_add) under the specified assumptions since the last call
     * Note: clears the assumptions after the solver run finished
     * @param nOfConflicts number of conflicts that are allowed for this SAT solver run (-1 = infinite)
     * @return status of the SAT call: 10 = satisfiable, 20 = unsatisfiable, 0 = not finished within number of conflicts
     */
    int
    riss_sat_limited(void* riss, const int64_t nOfConflicts)
    {
        libriss* solver = (libriss*) riss;
        if (nOfConflicts == -1) { solver->solver->budgetOff(); }
        else { solver->solver->setConfBudget(nOfConflicts); }

        // make sure the assumptions fit into the memory of the solver!
        for (int i = 0 ; i < solver->assumptions.size(); ++ i) {
            while (var(solver->assumptions[i]) >= solver->solver->nVars()) { solver->solver->newVar(); }
        }

        lbool ret = solver->solver->solveLimited(solver->assumptions);
        solver->assumptions.clear();      // clear assumptions after the solver call finished
        solver->lastResult = ret;     // store last solver result
        return ret == l_False ? 20 : (ret == l_Undef ? 0 : 10);  // return UNSAT, UNKNOWN or SAT, depending on solver result
    }

    /** return the polarity of a literal in the model of the last solver run (if the result was sat) */
    int
    riss_deref(const void* riss, const int lit)
    {
        const Var v = lit > 0 ? lit - 1 : -lit - 1;
        libriss* solver = (libriss*) riss;
        assert(solver->lastResult == l_True && "can deref literals only after SAT result");
        lbool vValue = l_Undef;
        if (v < solver->solver->model.size()) { vValue = solver->solver->model[v]; }
        return (lit < 0) ? (vValue == l_False ? 1 : (vValue == l_True ? -1 : 0)) : (vValue == l_False ? -1 : (vValue == l_True ? 1 : 0));
    }

    /** check whether a given assumption literal is present in the current conflict clause (result of analyzeFinal)
    * @return 1 if the assumption is part of the conflict, 0 otherwise.
    */
    int riss_assumption_failed(void* riss, int lit)
    {
        libriss* solver = (libriss*) riss;
        riss_build_conflict_map(solver);  // build map, if it has not been build already
        const int v = lit > 0 ? (lit - 1) : (-lit - 1);
        return solver->conflictMap[ v ]; // return the flag of the corrsponding variable
    }

    /** give number of literals that are present in the conflict clause that has been produced by analyze_final
     *  @return number of literals in the conflict clause
     */
    int riss_conflict_size(const void* riss)
    {
        libriss* solver = (libriss*) riss;
        assert(solver->lastResult == l_False && "can only work with the conflict clause, if the last result was unsatisfiable");
        return solver->solver->conflict.size();
    }

    /** return the literals of the conflict clause at the specified position
     *  @return a literal of the conflict clause
     */
    int riss_conflict_lit(const void* riss, const int position)
    {
        libriss* solver = (libriss*) riss;
        assert(position >= 0 && position < solver->solver->conflict.size() && "can only access existing positions");
        assert(solver->lastResult == l_False && "can only work with the conflict clause, if the last result was unsatisfiable");
        const Lit& l = solver->solver->conflict[ position ];
        return sign(l) ? - var(l) - 1 : var(l) + 1;
    }

    /** returns the number of variables that are currently used by the solver
     * @return number of currently maximal variables
     */
    int riss_variables(const void* riss)
    {
        libriss* solver = (libriss*) riss;
        return solver->solver->nVars();
    }

    /** returns the current number of assumption literals for the next solver call
     * @return number of currently added assumptions for the next solver call
     */
    int riss_assumptions(const void* riss)
    {
        libriss* solver = (libriss*) riss;
        return solver->assumptions.size();
    }

    /** returns the number of (added) clauses that are currently used by the solver (does not include learnt clauses)
     * @return number of clauses (not including learnt clauses)
     */
    int riss_clauses(const void* riss)
    {
        libriss* solver = (libriss*) riss;
        return solver->solver->nClauses();
    }

}



// #pragma GCC visibility pop

//#pragma GCC visibility pop
