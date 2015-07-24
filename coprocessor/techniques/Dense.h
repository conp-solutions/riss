/*****************************************************************************************[Dense.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_DENSE_H
#define RISS_DENSE_H

#include "coprocessor/CoprocessorTypes.h"
#include "Propagation.h"

#include "coprocessor/Technique.h"

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

class Dense : public Technique<Dense>
{
    CoprocessorData& data;

    Propagation& propagation;


    class Compression {

        static const NOT_MAPPED = -1; // variable is not mapped
        static const UNIT       = -2; // variable is unit and therefore removed in the compressed formula

        std::vector<int> mapping;         // mapping from old variables to new ones
        std::vector<int> forward;         // store to which new variable an old variable has been mapped

        uint32_t variables;               // number of variables before compression
        uint32_t postvariables;           // number of variables after compression

        std::vector<Riss::Lit> trail;     // already assigned literals - this variables are removed from the formula
        std::vector<int> unitMapping;     // mapping of NOT_MAPPED or UNIT for each variable for faster import look ups

      public:

        Compression() : variables(0), postvariables(0) {};

        /**
         * @return true, if compression is active and variable renaming was performed
         */
        inline bool isAvailable() const { return variables == 0; }

        /**
         * Clear and resize all mappings
         */
        void reset(int nvars)
        {
            // ensure our mapping tables are big enough
            mapping.resize(nvars);
            forward.resize(nvars);

            // reset all mappings
            std::fill(mapping.begin(), mapping.end(), NOT_MAPPED);
            std::fill(forward.begin(), forward.end(), NOT_MAPPED);

            variables = nvars;
            postvariables = nvars;
        }

        /**
         * Get a literal from outside and import it into the compressed formula. This means that eventually a smaller
         * number will be returned.
         *
         * If no compression is available (which means, the formula was never compressed) the passed argument
         * will be returned as a default value.
         */
        inline Riss::Lit import(const Riss::Lit& lit) const
        {
            if (isAvailable()) {

            } else {
                return lit;
            }
        }

        /**
         * The same import method as above but for variables
         */
        inline Riss::Var import(const Riss::Var& var) const
        {

        }

        /**
         * Export a literal from the compressed formula to the outside environment. The literal will be translated
         * to the name / value from the original uncompressed formula.
         *
         * As in the import methods, the passed argument will be returned as a default value if no compression
         * is available.
         */
        inline Riss::Lit export(const Riss::Lit& lit) const
        {
            if (isAvailable()) {

            } else {
                return lit;
            }
        }

        /**
         * The same export method as above but for variables
         */
        inline Riss::Var export(const Riss::Var& var) const
        {

        }

    };

    Compression compression;

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

};


}

#endif // RESOLVING_H
