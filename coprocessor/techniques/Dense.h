/*****************************************************************************************[Dense.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_DENSE_H
#define RISS_DENSE_H

#include "coprocessor/CoprocessorTypes.h"
#include "Propagation.h"

#include "coprocessor/Technique.h"


namespace Coprocessor
{

/**
 * The Dense technique compresses a formula. It does not implement the standard stepper / penalty
 * mechanism.
 */
class Dense : public Technique<Dense>
{
    CoprocessorData&   data;
    Propagation&       propagation;
    Riss::Compression& compression;

  public:
    Dense(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation);


    /** compress the formula - if necessary, output a new whiteFile
     * calls adoptUndoStack before it actually modifies the formula
     */
    void compress(const char *newWhiteFile = 0);

    /** decompress the most recent additions of the extend model std::vector,
     *  does not touch lit_Undefs */
    void adoptUndoStack();

    /** undo variable mapping, so that model is a model for the original formula
     * adoptUndoStack should be called before this method!
     */
    void decompress(Riss::vec< Riss::lbool >& model);

    /** inherited from @see Technique */
    void printStatistics(std::ostream& stream);

    /** write dense information to file, so that it can be loaded afterwards again */
    bool writeUndoInfo(const std::string& filename);

    /** read dense information from file */
    bool readUndoInfo(const std::string& filename);

    /** return the new variable for the old variable */
    Riss::Lit giveNewLit(const Riss::Lit& l) const ;


  /*
   * Helper members and methods
   */
  private:

    std::vector<Riss::Var>   mapping; // temporary mapping that is used in the compress method
    std::vector<Riss::lbool> trail;   // temporary mapping of assigned literals (units)

    /**
     * Count literal occurings in the given clauses and store the number of occurences in the
     * passed vector
     */
    inline void countLiterals(std::vector<uint32_t>& count, Riss::vec<Riss::CRef>& list)
    {
        for (uint32_t i = 0 ; i < list.size(); ++i) {
            Riss::Clause& clause = ca[ list[i] ];

            // consider only clauses in the formula!
            if (!clause.can_be_deleted() && !clause.learnt()) {
                for (uint32_t j = 0; j < clause.size(); ++j) {
                    const Riss::Lit l = clause[j];

                    DOUT(if (config.dense_debug_out && l_Undef != data.value(l)) {
                        std:: cerr << "c DENSE found assigned literal " << l << " in clause ["
                                   << data.getClauses()[i] << "] : " << clause << " learned?: "
                                   << clause.learnt() << endl;
                    });

                    assert(l_Undef == data.value(l) && "there cannot be assigned literals");
                    assert(var(l) < data.nVars());

                    count[ var(l) ] ++;
                }
                // found empty clause?
                if (clause.size() == 0) {
                    data.setFailed();
                }
            }
        }
    }

    /**
     * @return true, if the formula is already compact or not too loose
     */
    inline bool isCompact(uint32_t diff) const
    {
        return diff == 0 || (config.opt_dense_fragmentation > 1.0 + ((diff * 100)  / data.nVars()));
    }

};



}

#endif // RESOLVING_H
