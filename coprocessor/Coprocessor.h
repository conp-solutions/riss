/***********************************************************************************[Coprocessor.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_COPROCESSOR_HH
#define RISS_COPRECESSOR_HH


#include "riss/core/Solver.h"

#include "coprocessor/CoprocessorTypes.h"
#include "riss/utils/ThreadController.h"
#include "coprocessor/CP3Config.h"


#include "coprocessor/Propagation.h"
#include "coprocessor/Subsumption.h"
#include "coprocessor/HiddenTautologyElimination.h"
#include "coprocessor/BVE.h"
#include "coprocessor/ClauseElimination.h"
#include "coprocessor/rate.h"
#include "coprocessor/EquivalenceElimination.h"
#include "coprocessor/BVA.h"
#include "coprocessor/Unhiding.h"
#include "coprocessor/Probing.h"
#include "coprocessor/Resolving.h"
#include "coprocessor/Rewriter.h"
#include "coprocessor/FourierMotzkin.h"
#include "coprocessor/BCE.h"
#include "coprocessor/LiteralAddition.h"
#include "coprocessor/xor.h"
#include "coprocessor/Entailed.h"
#include "coprocessor/Dense.h"
#include "coprocessor/Symmetry.h"

#include "coprocessor/sls.h"
#include "coprocessor/TwoSAT.h"

#include <string>
#include <cstring>

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{
/** Main class that connects all the functionality of the preprocessor Coprocessor
 */
class Preprocessor
{

    // friends


    CP3Config& config;           // configuration of the preprocessor

    // attributes
    int32_t threads;             // number of threads that can be used by the preprocessor
    Riss::Solver* solver;              // handle to the solver object that stores the formula
    Riss::ClauseAllocator& ca;         // reference to clause allocator

    Logger log;                  // log output
    CoprocessorData  data;       // all the data that needs to be accessed by other classes (preprocessing methods)
    Riss::ThreadController controller; // controller for all threads

    Clock ppTime;     // time to do preprocessing
    Clock ipTime;     // time to do inpreprocessing
    Clock overheadTime;       // time for pp overhead (init and all that)
    int thisClauses;      // number of original clauses before current run
    int thisLearnts;      // number of learnt clauses before current run

    int lastInpConflicts;     // number of conflicts when inprocessing has been called last time
    int formulaVariables;     // number of variables in the initial formula

  public:

    Preprocessor(Riss::Solver* solver, CP3Config& _config, int32_t _threads = -1);
    ~Preprocessor();

    // major methods to start preprocessing
    /** perform preprocessing
     * @return status of the formula l_False for UNSAT, l_Undef for unknown, l_True so satisfiable.
     */
    Riss::lbool preprocess();

    /** method to determine whether inprocessing should be done, according to the preprocessors heurisics*/
    bool wantsToInprocess();

    /** perform inprocessing
     * @return status of the formula l_False for UNSAT, l_Undef for unknown, l_True so satisfiable.
     */
    Riss::lbool inprocess();
    Riss::lbool preprocessScheduled();
    Riss::lbool performSimplificationScheduled(std::string techniques);

    void extendModel(Riss::vec<Riss::lbool>& model);

    /* TODO:
     - extra classes for each extra techniques, which are friend of coprocessor class
     - extend model
     - ...
    */

    /** print formula (DIMACs), and dense, if another filename is given */
    void outputFormula(const char *file, const char *varMap = 0);

    // handle model processing

    int getFormulaVariables() const { return formulaVariables; }

    /** parse model, if no file is specified, read from stdin
     * @return false, if some error happened
     */
    int parseModel(const std::string& filename);

    /** parse model extend information
     * @return false, if some error happened
     */
    bool parseUndoInfo(const std::string& filename);

    /** write model extend information to specified file
     * @param originalVariables variables that are present in the actual problem (tool might have added variables from the outside)
     * @return false, if some error happened
     */
    bool writeUndoInfo(const std::string& filename, int originalVariables = -1);

    /** return info about formula to be writtern*/
    void getCNFinfo(int& vars, int& cls);

    /** write formula into file of file descriptor
     * @param clausesOnly: will not print the cnf header (e.g. to print something before)
     */
    void printFormula(FILE* fd, bool clausesOnly = false);

    /** disable the specified variable (external representation) for modelset-changing preprocessing (bve,ee,bce,cce,la,...)
     * @param lit literal in external representation (the whole variable will be frozen!)
     */
    void freezeExtern(int lit);

    /** returns current (irredundant) formula in one std::vector, and external variable representation. all clauses are terminated by a '0'
     * @param outputFormula std::vector that contains the formula afterwards
     */
    void dumpFormula(std::vector<int>& outputFormula);

    /** return the literal, to which the specified literal is mapped to
     * @param l literal in the external world representation
     * @return the new literal, or Riss::lit_Undef if the literal is not present any more, or lit_Error, if the information is not present
     */
    int giveNewLit(const int& l) const;
    Riss::Lit giveNewLit(const Riss::Lit& l) const;

  protected:
    //
    // techniques
    //
    Propagation                 propagation;
    Subsumption                 subsumption;
    HiddenTautologyElimination  hte;
    BoundedVariableElimination  bve;
    BoundedVariableAddition     bva;
    ClauseElimination           cce;
    EquivalenceElimination      ee;
    Unhiding                    unhiding;
    Probing                     probing;
    RATElimination              rate;
    Resolving                   res;
    Rewriter                    rew;
    FourierMotzkin              fourierMotzkin;
    Dense                       dense;
    Symmetry                    symmetry;
    XorReasoning                xorReasoning;
    BlockedClauseElimination    bce;
    LiteralAddition             la;
    EntailedRedundant           entailedRedundant;

    Sls                         sls;
    TwoSatSolver                twoSAT;

    int shuffleVariable;  // number of variables that have been present when the formula has been shuffled
    Riss::vec<Riss::Var> specialFrozenVariables;

    // do the real work
    Riss::lbool performSimplification();
    void printStatistics(std::ostream& stream);

    // own methods:
    void cleanSolver();                // remove all clauses from structures inside the solver
    void reSetupSolver();              // add all clauses back into the solver, remove clauses that can be deleted
    void initializePreprocessor();     // add all clauses from the solver to the preprocessing structures
    void destroyTechniques();        // free resources of all preprocessing techniques

    void giveMoreSteps();

    void shuffle();           // shuffle the formula
    void unshuffle(Riss::vec< Riss::lbool >& model);      // unshuffle the formula

    /** freeze all variables that appear in special search data structures (assumptions, preferred decisions)
     *  Note: does not freeze a variable twice, will not add variables to undo information, if the variable is frozen already
     */
    void freezeSearchVariables();
    /** undo freezing for the special search variables */
    void meltSearchVariables();

    // small helpers
    void sortClauses();                // sort the literals within all clauses
    void delete_clause(const Riss::CRef& cr);  // delete a clause from the solver (clause should not be attached within the solver)

    bool checkLists(const std::string& headline); // check each clause list for duplicate occurrences
    void fullCheck(const std::string& headline);  // check solver state before control is passed to solver
    void scanCheck(const std::string& headline);  // check clauses for duplicate literals

    // print formula
    inline void printClause(FILE * fd, Riss::CRef cr);
    inline void printLit(FILE * fd, int l);
    void printFormula(const std::string& headline);


    /** print current state of the solver */
    void printSolver(std::ostream& s, int verbose);
};

};

#endif
