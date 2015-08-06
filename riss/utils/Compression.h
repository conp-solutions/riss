/***********************************************************************************[Compression.h]

Copyright (c) 2015, Norbert Manthey, Lucas Kahlert

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_COMPRESSION_H
#define RISS_COMPRESSION_H

#include <iostream>
#include <vector>

#include "riss/utils/Debug.h"

namespace Riss
{

/**
 * Container for compression information. It holds the tables that are required for the variable
 * renaming after the formula was compressed and you want to export or import variables / literals
 * to / from the outside environment of the solver.
 *
 * The variable in the compressed formula will always be equal or smaller than in the original
 * formula.
 *
 * @author Lucas Kahlert <lucas.kahlert@tu-dresden.de>
 */
class Compression
{
    std::vector<Var> mapping;   // mapping from old variables to new ones
    std::vector<Var> forward;   // store to which new variable an old variable has been mapped
    std::vector<lbool> trail;   // already assigned literals (units) - this variables are removed from the formula

  public:

    enum {
        UNIT = -1, // variable is unit and therefore removed in the compressed formula
    };

    /**
     * Writes the compression map to a file, so that it can be loaded
     * later.
     *
     * If the verbose flag is present, some additional debug information will
     * be written in comments to the map file.
     */
    bool serialize(const std::string &filename, bool verbose = false) const;

    /**
     * Read the compression map from a file that where previously saved and parses its content
     * as the current mapping.
     */
    bool deserialize(const std::string &filename, bool verbose = false);

    /**
     * @return true, if compression is active and variable renaming was performed
     */
    inline bool isAvailable() const { return mapping.size() > 0; }

    /**
     * Clear all mappings
     */
    void reset()
    {
        DOUT(std::cerr << "c Reset compression" << std::endl;);

        // clear all mappings
        mapping.clear();
        forward.clear();
        trail.clear();
    }

    /**
     * Use the passed mapping as the new mapping. The current trail and forward mapping will be adjusted
     */
    void update(std::vector<Riss::Var>& _mapping, std::vector<lbool>& _trail);

    /**
     * Takes a literal from outside and import it into the compressed formula. This means that eventually
     * a smaller number will be returned. If the literal is a unit in the compressed clause and was therefore
     * removed, lit_Undef will be returned.
     *
     * If no compression is available (which means, the formula was never compressed) the literal is returned
     * unchanged (identity map).
     *
     * @return (new) literal name in the compressed formula
     */
    inline Lit importLit(const Lit& lit) const
    {
        if (isAvailable()) {
            assert(var(lit) < mapping.size() && "Variable must not be larger than current mapping");

            const Var compressed = mapping[var(lit)];
        
           // if the variable is unit, it is removed in the compressed formula. Therefore
           // we return undefined
           if (compressed == UNIT) {
               return lit_Undef;
           } else {
               return mkLit(compressed, sign(lit));
           }
        }
        // no compression available, nothing to do! :)
        else {
           return lit;
        }
    }

    /**
     * The same import method as above but for variables. var_Undef is returned, if the variable
     * is a unit in the compressed formula.
     */
    inline Var importVar(const Var& var) const
    {
        // @see import for literals above for comments

        if (isAvailable()) {
            assert(var < mapping.size() && "Variable must not be larger than current mapping");
            
            const Var compressed = mapping[var];

            if (compressed == UNIT) {
                return var_Undef;
            } else {
                return compressed;
            }
        } else {
            return var;
        }
    }

    /**
     * Export a literal from the compressed formula to the outside environment. The literal will
     * be translated to the name / value in the original uncompressed formula.
     *
     * As in the import methods, the passed argument will be returned as a default value if no
     * compression is available (identity map).
     *
     * @return (old) literal name in the original uncompressed formula
     */
    inline Lit exportLit(const Lit& lit) const
    {
        if (isAvailable()) {
            assert(var(lit) < forward.size() && "Variable must not be larger than current mapping. "
                                                "Did you forget to update the compression?");
            return mkLit(forward[var(lit)], sign(lit));
        }
        // no compression available, nothing to do! :)
        else {
            return lit;
        }
    }

    /**
     * The same export method as above but for variables
     */
    inline Var exportVar(const Var& var) const
    {
        // @see export for literals above for comments

        if (isAvailable()) {
            assert(var < forward.size() && "Variable must not be larger than current mapping. "
                                           "Did you forget to update the compression?");

            return forward[var];
        } else {
            return var;
        }
    }

    /**
     * Returns value of an assigned variable (aka. unit) that was removed from the formula
     * and is therefore in the trail of the compression.
     *
     * If the variable is not a unit, l_Undef will be returned.
     */
    inline lbool value(const Var& var) const
    {
        assert(var < trail.size() && "Variable must not be greater than current mapping");

        return trail[var];
    }

    /**
     * @return number of variables in the original uncompressed formula
     */
    inline uint32_t nvars() const { return mapping.size(); }

    /**
     * @return number of variables after compression
     */
    inline uint32_t postvars() const { return forward.size(); }

    /**
     * Returns the number of variables reduced by compression
     * between the */
    inline uint32_t diff() const { return nvars() - postvars(); }

};

} // namespace Riss

#endif // RISS_COMPRESSION_H
