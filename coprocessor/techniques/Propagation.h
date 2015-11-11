/***********************************************************************************[Propagation.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_PROPAGATION_HH
#define RISS_PROPAGATION_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"

namespace Coprocessor
{

/**
 * This class is used for usual unit propagation, probing and distillation/asyymetric branching.
 *
 * It uses Technique interface but is never used as a separate technique from coprocessor. Instead
 * it is used from the other techniques internally to performe unit propagation. Therefore it implements
 * a different process method. Also it does not use the stepper and penalty system.
 */
class Propagation : public Technique<Propagation>
{
    // TODO: add queues and other attributes here!
    uint32_t lastPropagatedLiteral;  // store, which literal position in the trail has been propagated already to avoid duplicate work

    int removedClauses;  // number of clauses that have been removed due to unit propagation
    int removedLiterals; // number of literals that have been removed due to unit propagation
    double processTime;  // seconds spend on unit propagation

  public:

    Propagation(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller);

    /** will also set back the qhead variable inside the Riss::Solver object */
    void reset(CoprocessorData& data);

    /**
     * This is a special process method that do not implement the Technique interface.
     *
     * perform usual unit propagation, but shrinks clause sizes also physically
     * will run over all clauses with satisfied/unsatisfied literals (that have not been done already)
     *
     * Note:
     *   will share the propagated unit clauses with the other threads in the portfolio solver
     *
     * @return l_Undef, if no conflict has been found, l_False if there has been a conflict
     */
    Riss::lbool process(CoprocessorData& data, bool sort = false, Riss::Heap<VarOrderBVEHeapLt> * heap = nullptr, const Riss::Var ignore = var_Undef);

    void initClause(const Riss::CRef& cr);

    void printStatistics(std::ostream& stream);

    /** give more steps for inprocessing - nothing to be done for UP*/
    void giveMoreSteps() {}

  protected:
};

} // namespace coprocessor

#endif
