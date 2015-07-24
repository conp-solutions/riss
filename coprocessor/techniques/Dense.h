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

class Dense : public Technique<Dense>
{

    CoprocessorData& data;
    Propagation& propagation;
    Riss::Compression& compression;

    std::vector<int> test;

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

    void destroy();

    void initializeTechnique(CoprocessorData& data);

    /** write dense information to file, so that it can be loaded afterwards again */
    bool writeUndoInfo(const std::string& filename);

    /** read dense information from file */
    bool readUndoInfo(const std::string& filename);

    /** return the new variable for the old variable */
    Riss::Lit giveNewLit(const Riss::Lit& l) const ;

  protected:

    unsigned globalDiff;

  // helper functions
  private:

    void countLiterals(std::vector<uint32_t>& count, Riss::vec<Riss::CRef>& list);

};


}

#endif // RESOLVING_H
