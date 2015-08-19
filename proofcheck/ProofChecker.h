/***********************************************************************************[ProofChecker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
***************************************************************************************************/

#ifndef PROOFCHECKER_H
#define PROOFCHECKER_H

#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"
#include "riss/utils/System.h"

namespace Riss {
  
class OnlineProofChecker;             // extra class declaration to avoid cycles
class BackwardChecker;                // class declaration of backward checker

/** verify a given proof(s) with respect to a given formula
 */
class ProofChecker 
{
  bool checkDrat;                     // check DRAT proof format
  bool checkBackwards;                // perform backward check
  bool testRATall;                    // test all literals for the RAT property if RUP fails
  int  threads;                       // number of threads that should be used for verifying the proof
  
  bool isInterrupted;                 // indicate that there was an interupt from the outside
  int verbosityLevel;                 // verbosity level of the tool
  
  int variables;                      // number of current variables
  
  bool receiveFormula;                // indicate whether the current input clauses belong to the input formula
  
  OnlineProofChecker* forwardChecker; // algorithm implemented for forward checking (same class that is used for online checking during solving)
  BackwardChecker* backwardChecker;   // algorithm implemented for backward checking (can be done only with the full proof)
  
  bool ok;                            // indicate whether the state of the proof is still ok
  
  bool parsedEmptyClause;             // indicate that we saw the empty clause in the input formula / proof
  
  Clock checkClock;                   // clock that measures the full time
  int addedClauses;                   // number of clauses that have been added
  int lastAddedClauses;	              // number of added clauses during last report
  double lastCpuT;                    // cpu time of last report
  
public:
  
  /** setup the object with the required options */
  ProofChecker( bool opt_drat, bool opt_backward , int opt_threads , bool opt_first  = true);
  ~ProofChecker();
  
  /** receive interupt from the outside */
  void interupt();
  
  /** tell object that we are checking a DRUP proof (independently of the option of the binary) */
  void setDRUPproof();
  
  /** all further clauses that are added to the checker are considered to be part of the proof (not part of the specification) 
   * @param nextIsFormula indicate whether future clauses have to be checked (not checked, if they belong to the formula)
   */
  void setReveiceFormula(bool nextIsFormula);
  
  /** indicate whether parsing the proof was ok, and that no error occured
   * @return true, if parsing was ok
   */
  bool parsingOk() const;
  
  /** check whether the current clause can be added to the current proof (do not add the clause) 
   *  Note: uses the set proof format of the checker, does not add the clause to the proof
   *  @clause clause to be checked
   *  @add indicate whether the clause should be added to the proof after the successful check
   *  @return true, if the clause could be added
   */
  bool checkClauseDRUP( vec<Lit>& clause, bool add=true );
  
  /** indicate whether the empty clause has been added to the proof while creating the proof
   * @return true, if the empty clause was added to the proof (and is still present)
   */
  bool emptyPresent();
  
  /** verify the given proof. 
   *  @return true, if the proof can be verified
   */
  bool verifyProof ();
  
  /** reserve storage for the given number of variables 
   * @param newVariables number of variables for which the data strucutures are set up
   */
  void reserveVars(int newVariables);
  
  /** Add a clause to the solver without making superflous internal copy. Will change the passed vector 'ps'
   * @param ps clause to be added to the checker
   * @param isDelete indicate whether the clause should be removed instead of being added
   * @return true, if no error with the current clause could be found yet (e.g. due to backward checking)
   */
  bool    addClause_( vec< Lit >& ps, bool isDelete = false);                     
  
  /** dummy method, not needed here (but required to re-use code), will not do anything */
  void addInputClause_(vec< Lit >& ps) {}
  
  /** Add a new variable, increase all necessary data structures */
  Var     newVar    (); 
  
  /** return the number of variables that are currently present in the object */
  int     nVars      ()      const;
  
  /** set verbosity level */
  void setVerbosity( int verbosity );
  
protected:
  
  /** test whether we received an interupt */
  bool receivedInterupt();
  
};


};

#endif
