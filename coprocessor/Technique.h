/**************************************************************************************[Technique.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_TECHNIQUE_HH
#define RISS_TECHNIQUE_HH

#include "riss/core/Solver.h"
#include "riss/utils/System.h"

#include "riss/utils/ThreadController.h"
#include "coprocessor/CP3Config.h"


namespace Coprocessor
{

/**
 * Template class for all techniques that should be implemented into Coprocessor
 * should be inherited by all implementations of classes.
 *
 * It uses the CRTP (Curiously recurring template pattern) technique for static
 * polymorphism. This prevents vtables.
 */
template<class T>
class Technique
{
  protected:
    CP3Config& config;             // store the configuration for the whole preprocessor

    bool modifiedFormula;         // true, if subsumption did something on formula
    bool isInitialized;           // true, if the structures have been initialized and the technique can be used
    uint32_t myDeleteTimer;       // timer to control which deleted variables have been seen already

    int thisPelalty;              // how many attepts will be blocked, before the technique is allowed to perform preprocessing again
    int lastMaxPenalty;           // if the next simplification is unsuccessful, block the simplification for more than the given number

    Riss::ClauseAllocator& ca;          // clause allocator for direct access to clauses
    Riss::ThreadController& controller; // controller for parallel execution

    bool didPrintCannotDrup;      // store whether the drup warning has been reported already
    bool didPrintCannotExtraInfo; // store whether the extraInfo warning has been reported already

  public:

    Technique(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller)
        : config(_config)
        , modifiedFormula(false)
        , isInitialized(false)
        , myDeleteTimer(0)
        , thisPelalty(0)
        , lastMaxPenalty(0)
        , ca(_ca)
        , controller(_controller)
        , didPrintCannotDrup(false)
        , didPrintCannotExtraInfo(false)
    {}

    /** return whether some changes have been applied since last time
     *  resets the counter after call
     */
    inline bool appliedSomething()
    {
        bool modified = modifiedFormula;
        modifiedFormula = false;
        return modified;
    }

    /** call this method for each clause when the technique is initialized with the formula
     *  This method should be overwritten by all techniques that inherit this class
     */
    void initClause(const Riss::CRef& cr);

    /** free resources of the technique, which are not needed until the technique is used next time
     *  This method should be overwritten by all techniques that inherit this class
     */
    void destroy();

    /** This method should be used to print the statistics of the technique that inherits from this class */
    void printStatistics(std::ostream& stream);

    /** per call to the inprocess method of the preprocessor, allow a technique to have this number more steps */
    void giveMoreSteps();

  protected:
    /** call this method to indicate that the technique has applied changes to the formula */
    inline void didChange()
    {
        modifiedFormula = true;
    }


    /** reset counter, so that complete propagation is executed next time
     *  This method should be overwritten by all techniques that inherit this class
     */
    void reset();

    /** indicate that this technique has been initialized (reset if destroy is called) */
    inline void initializedTechnique()
    {
        isInitialized = true;
    }

    /** return true, if technique can be used without further initialization */
    inline bool isInitializedTechnique()
    {
        return isInitialized;
    }

    /** give delete timer */
    inline uint32_t lastDeleteTime()
    {
        return myDeleteTimer;
    }

    /** update current delete timer */
    inline void updateDeleteTime(const uint32_t deleteTime)
    {
        myDeleteTimer = deleteTime;
    }

    /** ask whether a simplification should be performed yet */
    inline bool performSimplification()
    {
        bool ret = (thisPelalty == 0);
        thisPelalty = (thisPelalty == 0) ? thisPelalty : thisPelalty - 1;  // reduce current wait cycle if necessary!
        return ret; // if there is no penalty, apply the simplification!
    }

    /** return whether next time the simplification will be performed */
    inline bool willSimplify() const
    {
        return thisPelalty == 0;
    }

    /** report that the current simplification was unsuccessful */
    inline void unsuccessfulSimplification()
    {
        thisPelalty = ++lastMaxPenalty;
    }


    /** tell via stream that the technique does not support DRUP proofs */
    inline void printDRUPwarning(ostream& stream, const string& s)
    {
        if (!didPrintCannotDrup) {
            stream << "c [" << s << "] cannot produce DRUP proofs" << std::endl;
        }
        didPrintCannotDrup = true;
    }

    /** tell via stream that the technique does not support extra clause info proofs */
    inline void printExtraInfowarning(ostream& stream, const string& s)
    {
        if (!didPrintCannotExtraInfo) {
            stream << "c [" << s << "] cannot handle clause/variable extra information" << std::endl;
        }
        didPrintCannotExtraInfo = true;
    }

};

} // end namespace coprocessor

#endif
