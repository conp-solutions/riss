/**************************************************************************************[Technique.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_TECHNIQUE_HH
#define RISS_TECHNIQUE_HH

#include <limits> // max int

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
    /**
     * Helper class that controls the budget of computation time a technique is allowd to use
     */
    class Stepper
    {
        int64_t limit;  // number of steps (which should correlate to the computation time) the technique is allowd to run
        int64_t steps;  // number of steps already used (shows the usage)

      public:
        Stepper(int limit) : limit(limit), steps(0) {}

        inline int  getCurrentSteps() const             { return steps; }; // returns the number of consumed steps
        inline void increaseLimit(int additionalBudget) { limit += additionalBudget; }
        inline void increaseSteps(int _steps = 1)       { steps += _steps; }
        inline bool inLimit() const                     { return steps < limit; }
        inline void reset()                             { steps = 0; }
    };


  protected:
    CP3Config& config;            // store the configuration for the whole preprocessor
    Stepper stepper;

    bool modifiedFormula;          // true, if subsumption did something on formula

    bool isInitialized;           // true, if the structures have been initialized and the technique can be used
    uint32_t myDeleteTimer;       // timer to control which deleted variables have been seen already

    int thisPelalty;              // how many attepts will be blocked, before the technique is allowed to perform preprocessing again
    int lastMaxPenalty;           // if the next simplification is unsuccessful, block the simplification for more than the given number

    Riss::ClauseAllocator& ca;          // clause allocator for direct access to clauses
    Riss::ThreadController& controller; // controller for parallel execution

    bool didPrintCannotDrup;      // store whether the drup warning has been reported already
    bool didPrintCannotExtraInfo; // store whether the extraInfo warning has been reported already

  public:

    /**
     * @param budget number of computation steps the technique is allowed to use. Defaults to maximal integer value
     */
    Technique(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, int limit = numeric_limits<int64_t>::max())
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
        , stepper(limit)
    {}

    // TODO Maybe we can use a generic interface for all techniques? This would save us a lot of boiler plate code
    //      Yet, we cannot define a method in this interface that is called from outside to performe a single
    //      technique, because the signatures are too different yet. This also prevents us from building a wrapping
    //      function that do automatically check the penalty and stepper system. Below is a first draft:
    //      "apply()" and "process()"

    // /**
    //  * Applies a certain technique on the given data. It makes use of an internal penalty system, that disables a
    //  * technique for some rounds if it was not successful in the last run.
    //  * Besides it uses a stepper system to track and limit the computation time taken by this technique.
    //  */
    // inline Riss::lbool apply(CoprocessorData& data, const bool doStatistics = true)
    // {
    //     modifiedFormula = false;
    //
    //     if (!performSimplification()) {
    //         return l_Undef;
    //     }
    //
    //     return static_cast<T>(this).process(data, doStatistics);
    // }

    /**
     * Return whether some changes have been applied since last time
     */
    inline bool appliedSomething() const
    {
        return modifiedFormula;
    }

    /** call this method for each clause when the technique is initialized with the formula
     *  This method should be overwritten by all techniques that inherit this class
     */
    void initClause(const Riss::CRef& cr);

    /**
     * Free resources of the technique, which are not needed until the technique is used next time.
     * This method should be overwritten by all techniques that inherit this class and needs to free extra ressources.
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

    // TODO see above @ apply()
    // /**
    //  * This is the internal process method that have to be implemented for each technique. This is the place where
    //  * the simplification will finally applied. It will be called by the apply() method.
    //  *
    //  * The implementation should call the methods for an (un)successful application. The check, if the technique
    //  * is allowd to be executed must not be done here. This is already handeled by the "apply()" call.
    //  *
    //  * @return Returns l_True/l_False if technique found out that the formula is SAT/UNSAT, otherwise l_Undef
    //  */
    // Riss::lbool process(CoprocessorData& data, bool doStatistics = true);

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

    /** Ask whether a simplification should be performed yet. This checks the penalty and a stepper system. */
    inline bool performSimplification()
    {
        // check if budget of computation steps is depleted
        if (!stepper.inLimit()) {
            return false;
        }
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
