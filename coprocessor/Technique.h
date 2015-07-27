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
    int penalty;               // how many attepts will be blocked, before the technique is allowed to perform preprocessing again
    int peakPenalty;           // if the next simplification is unsuccessful, block the simplification for more than the given number
    const int increasePenalty; // if a technique was unsuccessful, this penalty will be hand out

  protected:

    /**
     * Helper class that controls the budget of computation time a technique is allowd to use
     */
    class Stepper
    {
        int64_t limit;  // number of steps (which should correlate to the computation time) the technique is allowd to run
        int64_t steps;  // number of steps already used (shows the usage)

      public:
        Stepper(int limit, int steps = 0) : limit(limit), steps(steps) {}

        inline int64_t getCurrentSteps() const             { return steps; } // returns the number of consumed steps
        inline int64_t getCurrentLimit() const             { return limit; } // returns budget of steps
        inline void    reset()                             { steps = 0; }    // set step counter to zero, persists current budget
        inline void    increaseLimit(int additionalBudget) { limit += additionalBudget; }
        inline void    increaseSteps(int _steps = 1)       { steps += _steps; } // you can also decrease the steps by using a negative argument
        inline bool    inLimit() const                     { return steps < limit; }

        /**
         * This limit check is useful if you an additional counter (for example for multiple parallel threads) and
         * you want that this extra counters are also inside the step limit.
         *
         * @return true, if the current steps + offset is inside the limit
         */
        inline bool    inLimit(uint64_t offset) const      { return steps + offset < limit; }
    };

    CP3Config& config;            // store the configuration for the whole preprocessor

    bool modifiedFormula;         // true, if subsumption did something on formula

    bool isInitialized;           // true, if the structures have been initialized and the technique can be used
    uint32_t myModTimer;          // timer to control which deleted variables have been seen already

    Riss::ClauseAllocator& ca;          // clause allocator for direct access to clauses
    Riss::ThreadController& controller; // controller for parallel execution

    bool didPrintCannotDrup;      // store whether the drup warning has been reported already
    bool didPrintCannotExtraInfo; // store whether the extraInfo warning has been reported already

  public:

    /**
     * @param budget number of computation steps the technique is allowed to use. Defaults to maximal integer value
     */
    Technique(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, int _increasePenalty = 1)
        : config(_config)
        , modifiedFormula(false)
        , isInitialized(false)
        , myModTimer(0)
        , ca(_ca)
        , controller(_controller)
        , didPrintCannotDrup(false)
        , didPrintCannotExtraInfo(false)
        , penalty(0)
        , peakPenalty(0)
        , increasePenalty(_increasePenalty)
    {}

    /**
     * This method will be called by the Coprocessor for each technique before simplifications are
     * performed. It can be used for bootstrapping tasks.
     *
     * This step is needed, because at the time the constructor of a technique is called, the CoprocessorData
     * object is not initialized. Therefore, if your technique depends on some properties of the data object,
     * access them in this method.
     *
     * Note:
     *   If your technique needs initialization, you can add an assertion to the top of your process()
     *   method. Make sure, that you add your technique in Preprocessor::initializePreprocessor() at the bottom.
     */
    inline void initializeTechnique(CoprocessorData& data)
    {
        // by default, do nothing but setting the initialization flag
        isInitialized = true;
    }

    /** return true, if technique can be used without further initialization */
    inline bool isInitializedTechnique() const
    {
        return isInitialized;
    }


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

    /** reset counter, so that complete propagation is executed next time
     *  This method should be overwritten by all techniques that inherit this class
     */
    void reset();

    /** give delete timer */
    inline uint32_t lastModTime()
    {
        return myModTimer;
    }

    /** update current delete timer */
    inline void updateModTime(const uint32_t modTime)
    {
        myModTimer = modTime;
    }

    /** Ask whether a simplification should be performed yet. This checks the penalty and a stepper system. */
    inline bool performSimplification()
    {
        // technique holds still a penalty. It should not process but the penalty is decreased.
        if (penalty > 0) {
            --penalty;
            return false;
        }

        // no step limit or penalty - go ahead!
        return true;
    }

    /** return whether next time the simplification will be performed */
    inline bool willSimplify() const
    {
        return penalty == 0;
    }

    /** report that the current simplification was unsuccessful */
    inline void unsuccessfulSimplification()
    {
        peakPenalty += increasePenalty;
        penalty = peakPenalty;
    }

    /**
     * Call this method to indicate that the application of the technique was successful and has applied changes
     * to the formula. The "modifiedFormula" flag will be set to true.
     */
    inline void successfulSimplification()
    {
        modifiedFormula = true;
        assert(penalty == 0 && "Penalty must be zero for a technique to be applied");

        // clear penalty - assure the technique runs next time for sure
        peakPenalty = 0;
    }


    /** tell via stream that the technique does not support DRUP proofs */
    inline void printDRUPwarning(std::ostream& stream, const std::string& s)
    {
        if (!didPrintCannotDrup) {
            stream << "c [" << s << "] cannot produce DRUP proofs" << std::endl;
        }
        didPrintCannotDrup = true;
    }

    /** tell via stream that the technique does not support extra clause info proofs */
    inline void printExtraInfowarning(std::ostream& stream, const std::string& s)
    {
        if (!didPrintCannotExtraInfo) {
            stream << "c [" << s << "] cannot handle clause/variable extra information" << std::endl;
        }
        didPrintCannotExtraInfo = true;
    }

};

} // end namespace coprocessor

#endif
