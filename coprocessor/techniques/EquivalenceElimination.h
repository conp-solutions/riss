/************************************************************************[EquivalenceElimination.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_EQUIVALENCEELIMINATION_HH
#define RISS_EQUIVALENCEELIMINATION_HH

#include "riss/core/Solver.h"

#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Circuit.h"

#include "coprocessor/Technique.h"
#include "Propagation.h"
#include "Subsumption.h"

#include <vector>
#include <deque>

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

/** This class implement hidden tautology elimination
 */
class EquivalenceElimination : public Technique<EquivalenceElimination>
{

    uint64_t gateSteps;
    double gateTime;
    double gateExtractTime;
    double eeTime;
    unsigned equivalentLits;                     // number of equivalent literals
    unsigned removedCls;                         // number of removed clauses due to rewriting
    unsigned removedViaSubsubption;              // number of immediate removed clauses due to subsumption

    uint64_t steps;                              // how many steps is the worker allowed to do

    char* eqLitInStack;                          // mark whether an element is in the stack
    char* eqInSCC;                               // indicate whether a literal has already been found in another SCC (than it cannot be in the current one)
    uint32_t eqIndex;                            // help variable for finding SCCs
    std::vector< Riss::Lit > eqStack;            // stack for the tarjan algorithm
    std::vector< int32_t > eqNodeLowLinks;       // stores the lowest link for a node
    std::vector< int32_t > eqNodeIndex;          // index per node
    std::vector< Riss::Lit > eqCurrentComponent; // literals in the currently searched SCC

    std::vector<char> isToAnalyze;               // stores that a literal has to be analyzed further
    std::vector<Riss::Lit> eqDoAnalyze;          // stores the literals to be analyzed

    CoprocessorData& data;                       // object that can talk to solver
    Propagation& propagation;                    // object that takes care of unit propagation
    Subsumption& subsumption;                    // object that takes care of subsumption and strengthening

    std::vector<Riss::Lit> proofClause;          // set of literals for outputting proofs

  public:

    EquivalenceElimination(CoprocessorData& _data, CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, Propagation& _propagation, Subsumption& _subsumption);

    /** run equivalent literal elimination */
    bool process(Coprocessor::CoprocessorData& data);

    void initClause(const Riss::CRef& cr);  // inherited from Technique

    /** inherited from @see Technique */
    void printStatistics(std::ostream& stream);

    /** apply equivalences stored in data object to formula
     * @param force run subsumption and unit propagation, even if no equivalences are found
     * @return true, if new binary clauses have been created (for completion)
     */
    bool applyEquivalencesToFormula(Coprocessor::CoprocessorData& data, bool force = false);

    void destroy();

    void giveMoreSteps();

  protected:

    /** check based on gates that have been extracted, whether more equivalent literals can be found!
     * @return true, if new equivalent literals have been found
     */
    bool findGateEquivalences(CoprocessorData& data, std::vector< Circuit::Gate > gates);
    bool findGateEquivalencesNew(CoprocessorData& data, std::vector< Circuit::Gate >& gates);

    /** find all strongly connected components on binary implication graph
     * @param externBig use extern big as basis for tarjan algorithm
     */
    void findEquivalencesOnBig(CoprocessorData& data, std::vector< std::vector< Riss::Lit > >* externBig = 0);

    /** find all strongly connected components on binary implication graph
     * Note: executes iterative tarjan algorithm
     * @param externBig use extern big as basis
     */
    void findEquivalencesOnBigFast(CoprocessorData& data, std::vector< std::vector<Riss::Lit> >* externBig = 0);

    /** use the recursive algorithm */
    void findEquivalencesOnBigRec(CoprocessorData& data, std::vector< std::vector<Riss::Lit> >* externBig = 0);

    /** return literals that have to be equivalent because of the two gates
     * @param replacedBy stores for each variable the literal that represents its equivalence class
     */
    bool checkEquivalence(const Circuit::Gate& g1, const Circuit::Gate& g2, Riss::Lit& e1, Riss::Lit& e2);

    /** perform tarjan algorithm to find SCC on binary implication graph */
    void eqTarjan(int depth, Riss::Lit l, Riss::Lit list, CoprocessorData& data, BIG& big, std::vector< std::vector< Riss::Lit > >* externBig = 0);

    /** check whether the clause c has duplicates in the list of literal l (irredundant clause is no duplicate for learned clause! -> deletes learned clause!)
     *  Note: assumes that all clauses are sorted!
     *  @return true, if there are duplicates, so that c can be deleted
     */
    bool hasDuplicate(CoprocessorData& data, std::vector< Riss::CRef >& list, const Riss::Clause& c);

    /** check whether this gate can be processed for equivalence checks */
    bool allInputsStamped(Circuit::Gate& g, std::vector< unsigned int >& bitType);

    /** check the current gate for equivalent literals, enqueue them to the "replacedBy" structure, invalidate the gate */
    void processGate(CoprocessorData& data, Circuit::Gate& g, std::vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, std::vector< std::vector< int32_t > >& varTable);


    void processANDgate(CoprocessorData& data, Circuit::Gate& g, std::vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, std::vector< std::vector< int32_t > >& varTable, Riss::MarkArray* active = 0, std::deque< Riss::Var >* activeVariables = 0);
    void processGenANDgate(CoprocessorData& data, Circuit::Gate& g, std::vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, std::vector< std::vector< int32_t > >& varTable, Riss::MarkArray* active = 0, std::deque< Riss::Var >*  activeVariables = 0);
    void processExOgate(CoprocessorData& data, Circuit::Gate& g, std::vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, std::vector< std::vector< int32_t > >& varTable, Riss::MarkArray* active = 0, std::deque<Riss::Var>* activeVariables = 0);
    void processITEgate(CoprocessorData& data, Circuit::Gate& g, std::vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, std::vector< std::vector< int32_t > >& varTable, Riss::MarkArray* active = 0, std::deque< Riss::Var >*  activeVariables = 0);
    void processXORgate(CoprocessorData& data, Circuit::Gate& g, std::vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, std::vector< std::vector< int32_t > >& varTable, Riss::MarkArray* active = 0, std::deque< Riss::Var >*  activeVariables = 0);
    void processFASUMgate(CoprocessorData& data, Circuit::Gate& g, std::vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, std::vector< std::vector< int32_t > >& varTable, Riss::MarkArray* active = 0, std::deque< Riss::Var >*  activeVariables = 0);

    /** enqueue all successor gates of the given gate g into the queue, stamp output variables, have a limit when to stop?! */
    void enqueueSucessorGates(Circuit::Gate& g, std::deque< int > queue, std::vector<Circuit::Gate>& gates, std::vector< unsigned int >& bitType, std::vector< std::vector<int32_t> >& varTable);

    /** write the AIGER circuit that can be found based on the clauses in the formula to a file in aag format */
    void writeAAGfile(CoprocessorData& data);

    /** returns the literal, that represents the Equivalence-class of l */
    Riss::Lit getReplacement(Riss::Lit l) ;

    /** sets literal replacement, fails if not possible
     * @return false, if this equivalence results in a conflict
     */
    bool setEquivalent(Riss::Lit representative, Riss::Lit toReplace);

    /** structure for iterative tarjan */
    struct Vertex {
        int start;
        int min;
        int seen;
        Vertex() : start(-1), min(-1), seen(-1) {}
    };
};

}

#endif
