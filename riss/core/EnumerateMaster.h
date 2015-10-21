/********************************************************************************[EnumerateMaster.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey

 **************************************************************************************************/

#ifndef ENUMERATEMASTER_H
#define ENUMERATEMASTER_H


#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"
#include "riss/utils/LockCollection.h"

#include "riss/utils/VarFileParser.h"

#include <cstdio>
#include <iostream>
#include <sstream>

/** forward declaration, so that formula simplification can be used later on */
namespace Coprocessor
{
class Preprocessor;
}

namespace Riss
{

/** class that controls enumerating models (with or without projection)
 *  can be used for parallel enumeration
 */
class EnumerateMaster
{

  public:

  private:

    SleepLock ownLock;       // lock for shared access
    bool enumerateParallel;  // use shared access
    bool enumerateWithBacktracking; // use backtracking for enumeration

    Coprocessor::Preprocessor* coprocessor; // pointer to formula simplification tool, that might be used for model extension

    std::string projectionFileName; // name of the file with the projection variables
    std::string outputFileName;     // name of the file to write the found models to
    std::string DNFfileName;        // name of the file to write the DNF to
    std::string fullModelsFileName; // name of the file to write the full models to

    int nVars;                       // number of variables in formula
    bool useProjection;              // whether projection is active
    std::vector<int> projectionVariables; // all variables in the projection
    int64_t maximalModels;           // number of models that should be found
    int mType;                       // minimization that is allowed for the model
    bool printEagerly;               // print model already when found, or later

    struct SharedClause {                      // also store the author for each blocking clause!
        vec<Riss::Lit>* clause;
        int authorID;
    };

    int nextClientID;                       // assign IDs to clients, such that duplicate blocking clauses for the same model can be avoided

    vec< vec<lbool>* > models;              // stores all currently found models (wrt projection, if activated)
    vec< uint64_t > modelHashes;            // store hashes for models for faster comparison
    vec< SharedClause > blockingClauses;    // stores all blocking clauses, if shared flag is set (in same order as models are stored)
    vec< vec<lbool>* > fullModels;          // stores all currently found full models (if projection is used)
    int64_t successfulBloom;                // number of times the bloom filter rejected successfully

    bool shareBlockingClauses;              // share blocking clauses (should be enabled, but for testing performance)
    int minimizeReceivedBlockingClauses;    // should clients minimize received blocking clauses (0=no, 1=from full, 2=from blocking)
    uint64_t checkForModelsEveryX;          // after how many decisions should be checked for new models

    // to be used in parallel setup
    vec< lbool > thisModel;

    /** write literals of the given set to the fileMemory stream*/
    void writeModelToStream(std::ostream& outputStream, const Riss::vec< Riss::lbool >& truthValues);    // write the current "disallow-clause" as model into the stream

    /** lock, if object has been set shared before */
    void lock() { if (enumerateParallel) { ownLock.lock(); } };

    /** unlock, if object has been set shared before */
    void unlock() { if (enumerateParallel) { ownLock.unlock(); } };

    /** add full model to storage */
    void storeFullModel(const Riss::vec< Riss::Lit >& model);

    /** add blocking clause to storage*/
    void storeBlockingClause(int authorID, Riss::vec< Riss::Lit >& clause);

  public:

    /** set up the enumeration master for the given number of variables */
    EnumerateMaster(int _nVars);

    /** free resources again */
    ~EnumerateMaster();


    /** set up all the data structures necessary for model enumeration
     * Note: projection file name, and pointer to coprocessor should be set already!
     */
    void initEnumerateModels();

    /** set the object shared (for parallel enumeration)*/
    void setShared() { assert(models.size() == 0 && "cannot set shared after first models have been found already"); enumerateParallel = true; }

    /** do we search for models in a parallel setup ? */
    bool isShared() const { return enumerateParallel; }

    /** use backtracking enumeration instead of naive clause blocking */
    void activateNaiveBacktrackingEnumeration();

    /** indicate whethre backtracking enumeration should be used */
    bool usesBacktrackingEnumeration() const ;

    /** return number of found models (so far), not synchronized */
    int64_t foundModels() const { return models.size(); }

    /** set number of models to be found ( 0 ^= INT64_MAX )*/
    void setMaxModels(const int64_t m) { lock(); maximalModels = (m == 0 ? INT64_MAX : m) ; unlock(); }

    /** tell master the client found UNSAT during adding model blocking clauses*/
    void notifyReachedAllModels() ;

    /** write full models to the output file */
    void writeStreamToFile(std::string filename = "", bool toerr = false);

    /** indicate whether enumeration is based on projection */
    bool usesProjection() const { return useProjection ; }

    /** add another model to the master
     @param model full (wrt projection, if used) model, represented by the trail of the solver
     @param modelHash hash of the model (high word: sum of negative literals, low word: sum of positive literals)
     @param blockingClause clause that blocks the given model, generated by the client
     @param fullModel pointer to full model, in case the full model should be printed even if projection is used
     @return true, if this model has not been seen before, false if the model is present already (useful for parallel enumeration)
     Note: assumes that a solver blocks each model itself , so that no book-keeping is performed
    */
    bool addModel(int authorID, Riss::vec< Riss::lbool >& newmodel, uint64_t modelhash, Riss::vec< Riss::Lit >* blockingClause = nullptr, Riss::vec< Riss::Lit >* fullModel = nullptr);

    /** print a given model, and futhermore already extend the model if simplifications have been used
     * NOTE: might change the model
     */
    void printSingleModel(std::ostream& outputStream, Riss::vec< Riss::lbool >& truthValues);

    /** return an unique ID to the asking client
     * Note: should be called by clients only
     */
    int assignClientID();

    /** tell whether enough models have been found */
    bool foundEnoughModels();

    int projectionSize() const;

    /** return projection variables, not synchronized read access */
    Var projectionVariable(int index) const;

    /** return the minimization of the blocking clause, that is allowed */
    int minimizeBlocked() const;

    /** return whether the master stores more models than the given number */
    bool hasMoreModels(uint64_t localModels) const;

    /** return the model with the given index, if its not from the given author
     * @return true, if the model is not from the author (otherwise, no data is copied at all!)
     */
    bool reveiveModel(int authorID, uint64_t modelIndex, Riss::vec< Riss::lbool >& receivedTruthValues, Riss::vec< Riss::Lit >& receivedBlockingClause);

    /** return number of decisions after which thread should look for new models by other workers */
    uint64_t checkNewModelsEvery() const ;

    /** indicate whether received blocking clauses should be minimized as well */
    int minimizeReceived() const ;

    void setReceiveModels(bool r);

    void setModelMinimization(int minimizationType);

    void setMinimizeReceived(int mini);

    void setCheckEvery(uint64_t checkEvery);

    void setModelFile(std::string filename);

    void setFullModelFile(std::string filename);

    void setDNFfile(std::string filename);

    void setProjectionFile(std::string filename);

    void setPreprocessor(Coprocessor::Preprocessor* preprocessor);

    void setPrintEagerly(bool p);
};

inline
void EnumerateMaster::notifyReachedAllModels()
{
    lock();
    maximalModels = models.size();
    unlock();
}

inline
void EnumerateMaster::storeFullModel(const vec< Lit >& fullModel)
{
    // get space for the model
    assert(fullModel.size() >= nVars && "a full model should have at least as many variables as have been present in the formula");
    Riss::vec<Riss::lbool>* newFullModel = new Riss::vec<Riss::lbool>(fullModel.size(), l_Undef);
    // set all values of the model
    for (int i = 0; i < fullModel.size(); i++) {
        Var v = var(fullModel[i]);
        (*newFullModel)[v] = sign(fullModel[i]) ? l_False : l_True;
    }
    // store new full model
    fullModels.push(newFullModel);
}

inline
void EnumerateMaster::storeBlockingClause(int authorID, vec< Lit >& clause)
{
    SharedClause sc;
    sc.authorID = authorID;
    sc.clause = new vec< Lit >(clause.size());
    clause.copyTo(* (sc.clause));
    blockingClauses.push(sc);

    assert(blockingClauses.size() == models.size() && "should have the same amount of entries (add model first!)");
}

inline
bool EnumerateMaster::addModel(int authorID, vec< lbool >& newmodel, uint64_t modelhash, vec< Lit >* blockingClause, vec< Lit >* fullModel)
{
    // if we do not have a string stream yet, get one
    bool newModel = false;
    if (!enumerateParallel) {
        vec<lbool>* modelCopy = new vec<lbool>(newmodel.size());
        newmodel.copyTo(*modelCopy);
        models.push(modelCopy);   // store model provided by the client

        // do not add blocking clause, as we are running sequentially
        // store full model, if requested and we use projection
        if (usesProjection() && fullModel != nullptr && fullModelsFileName != "") {
            storeFullModel(*fullModel);
        }

        if (printEagerly) {
            printSingleModel(std::cout, *models.last());
        }
        newModel = true;
    } else {
        lock() ;

        // check whether model is already present
        const int maxVar = newmodel.size() < nVars ? nVars : newmodel.size();

        // scan data base as well as hashes
        assert(models.size() == modelHashes.size() && "number of models and hashes has to be the same");
        bool foundModel = false;
        for (int i = 0 ; i < modelHashes.size(); ++ i) {
            if (modelHashes[i] != modelhash) { successfulBloom++; continue; }  // bloom filter, quickly check whether two models are not the same
            bool different = false;
            if (useProjection) {
                for (int j = 0 ; j < projectionVariables.size(); ++ j) {  // compare only projection variables
                    if (newmodel[j] != (*models[i])[ projectionVariables[j] ]) { different = true; break; }
                }
            } else {
                for (int j = 0 ; j < maxVar; ++ j) {
                    if (newmodel[j] != (*models[i])[j]) { different = true; break; }
                }
            }
            if (! different) { foundModel = true; break; }
        }

        if (!foundModel) {

            // store new model (converted it already)
            Riss::vec<Riss::lbool>* newModel = new Riss::vec<Riss::lbool>(maxVar, l_Undef);
            newmodel.copyTo(*newModel);
            models.push(newModel);
            modelHashes.push(modelhash);

            storeBlockingClause(authorID, *blockingClause);

            // store full model, if requested and we use projection
            if (usesProjection() && fullModel != nullptr && fullModelsFileName != "") {
                storeFullModel(*fullModel);
            }

            assert(models.size() == modelHashes.size() && models.size() >= fullModels.size() && "the numbers have to be the same, there cannot be more full models");

            if (printEagerly) {
                printSingleModel(std::cout, *models.last());
            }
        }
        unlock();

        newModel = !foundModel;
    }
    return newModel;  // return whether the current model has been new
}

inline
bool EnumerateMaster::foundEnoughModels()
{
    bool result = false;
    lock();
    result = (models.size() >= maximalModels);
    unlock();
    return result;
}

inline
int EnumerateMaster::assignClientID()
{
    lock();
    int returnValue = nextClientID ++;
    unlock();
    return returnValue;
}


inline
int EnumerateMaster::projectionSize() const
{
    return projectionVariables.size();
}

inline
Var EnumerateMaster::projectionVariable(int index) const
{
    assert(index >= 0 && projectionVariables.size() > index && "stay in bounds");
    return projectionVariables[ index ];
}

inline
int EnumerateMaster::minimizeBlocked() const
{
    return mType;
}

inline
bool EnumerateMaster::hasMoreModels(uint64_t localModels) const
{
    // can ly about the number of models, if the parameter is set accordingly (acts as there are never models)
    return shareBlockingClauses && localModels < models.size();
}

inline
bool EnumerateMaster::reveiveModel(int authorID, uint64_t modelIndex, vec< lbool >& receivedTruthValues, vec< Lit >& receivedBlockingClause)
{
    assert(models.size() > modelIndex && "the requested model has to be present");
    if (!enumerateParallel) { return false; }   // tell that there is no blocking clause to be received (as we currently run in sequential mode)
    lock();
    const bool notFromSameAuthor = blockingClauses[ modelIndex ].authorID != authorID;
    if (notFromSameAuthor) {
        assert(blockingClauses.size() > modelIndex && "the requested model has to be present");   // might fail due to race condition
        models[modelIndex]->copyTo(receivedTruthValues);
        assert(models.size() == blockingClauses.size() && "number of stored elements has to be the same");
        blockingClauses[modelIndex].clause->copyTo(receivedBlockingClause);
    }
    unlock();
    return notFromSameAuthor;
}

inline
uint64_t EnumerateMaster::checkNewModelsEvery() const
{
    return checkForModelsEveryX;
}

inline
int EnumerateMaster::minimizeReceived() const
{
    return minimizeReceivedBlockingClauses;
}


}

#endif


struct S;