/********************************************************************************************[BE.h]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#pragma once

#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Technique.h"
#include "coprocessor/techniques/BackboneSimplification.h"
#include "coprocessor/techniques/Probing.h"
#include "coprocessor/techniques/Propagation.h"
#include "riss/core/Solver.h"

#include <memory>
#include <set>

namespace Coprocessor {

    /**
     * @brief Class implementing Bipartition and Elimination Simplification as a procedure
     */
    class BE : public Technique<BE> {

        CoprocessorData& data;
        Propagation& propagation;
        Probing& probing;
        BackboneSimplification& backboneSimpl;
        Solver& solver;
        const int nVar;
        const int maxRes;

        Riss::Solver* ownSolver;
        Coprocessor::CP3Config* cp3config;
        Riss::CoreConfig* solverconfig;
        Riss::vec<Riss::Lit> assumptions; // current set of assumptions that are used for the next SAT call

        int conflictBudget;               // how many conflicts is the solver allowed to have before aborting the search for a model
        std::vector<bool> varUsed;        // "map" from variable to whether it is used in the solver, i.e. whether it is not a unit

        // statistic variables
        int nDeletedVars;
        double copyTime;
        double bipartitionTime;
        double eliminationTime;
        double eliminationTime2;
        double solverTime;
        mutable double subsumptionTime;
        mutable double getResTime;
        int nSolverCalls;
        int eliminationCandidates;  // number of variables that are candidates for elimination, excluding units
        int eliminatedVars;
        int nTopLevelIterations;    // number of top level iterations in elimination
        mutable int nSubsumption;
        mutable int nGetRes;

        mutable bool dirtyCache;

        /**
         * @brief The input and output variables of the bipartition algorithm
         */
        std::vector<Var> outputVariables;
        std::vector<Var> inputVariables;

    public:
        void reset();

        /**
         * @brief applies B+E algorithm
         * @return true, if something has been altered
         */
        bool process();

        void printStatistics(std::ostream& stream);

        void giveMoreSteps();

        void destroy();

        /**
         * @brief Construct a new Backbone Simplification procedure
         *
         * @param solver The solver to use for this
         */
        BE(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation,
           Probing& _probing, BackboneSimplification& _backboneSimpl, Solver& _solver);

        /**
         * @brief Computes a the bipartition
         * @note Resulting input variables are saved in the input member
         */
        void computeBipartition();

        /**
         * @brief The elimination part of the algorithm
         */
        void eliminate();

        /**
         * @brief Tests whether variable x is defined in terms of input variables of the formula and variables that follow it in the sorted list of
         * variables
         *
         * @param index The index of the variable in the sorted list of variables
         * @param vars The list of all variables sorted by occurrence
         * @return bool True if x is defined, False if not or on timeout/error
         */
        bool isDefined(int32_t index, std::vector<Var>& vars);

        /**
         * @brief Copies the state of the solver to the ownSolver
         */
        void copySolver();

    protected:
        /**
         * @brief Find maximum number of possible resolvents on x
         */
        inline std::uint32_t numRes(const Var& x) const;

        /**
         * @brief Get all clauses containing the given literal x
         */
        inline std::vector<Riss::CRef> getClausesContaining(const Lit& x) const;

        /**
         * @brief Generate all resolvents possible on the variable x
         * @return The generated resolvents
         */
        inline std::vector<std::vector<Lit>> getResolvents(const Var& x) const;

        /**
         * @brief Applies subsumption to the clauses in the given set, modifying it inplace
         */
        inline void subsume(std::vector<std::vector<Lit>>& clauses) const;

        /**
         * @brief Checks whether a clause subsumes another
         * @return true if a subsumes b, false otherwise
         */
        inline bool subsumes(std::vector<Lit>& a, std::vector<Lit>& b) const;

        /**
         * @brief for debugging, prints a vector of literals
         */
        inline void printLitVec(const std::vector<Lit>& lits) const;

        /**
         * @brief check if a clause is a tautology
         */
        inline bool isTautology(const std::vector<Lit>& lits) const;
    };

} // namespace Coprocessor
