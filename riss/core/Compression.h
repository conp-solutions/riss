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
     * @return true, if compression is active and variable renaming was performed
     */
    inline bool isAvailable() const { return mapping.size() == 0; }

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
    void update(std::vector<Riss::Var>& _mapping, std::vector<lbool>& _trail)
    {
        // initialize mapping
        if (!isAvailable()) {
            // simply copying the content of the mappings
            mapping = _mapping;
            trail = _trail;
        }
        // update current mapping
        else {

            assert(_mapping.size() <= forward.size() && "New mapping must be equal or smaller than current one");
            assert(_trail.size() <= trail.size() && "New trail must not be greater than orignal formula");

            // update mapping and trail at once
            for (Var var = 0; var < _mapping.size(); ++var) {
                const Var from = forward[var];  // old variable name translated into name in the original formula
                const Var to   = _mapping[var]; // new variable name in the compressed formula

                mapping[from] = to; // update mapping

                // update trail if the variable is a unit (will be removed in the compressed formula)
                if (to == UNIT) {
                    trail[from] = _trail[var];
                }
            }

            // update forward mapping
            // reduce size of the forward table
            forward.resize(_mapping.size());

            // "to" is the variable in the compressed formula and "from" the name in the original formula
            for (Var from = 0, to = 0; from < mapping.size(); ++from) {
                // we found a variable in the old formula that is not a unit in the compressed
                // store the inverse direction in the forward map (to -> from) and increment
                // the "to" variable to write the next non-unit mapping to the next position
                if (mapping[from] != UNIT) {
                    assert(to < forward.size() && "Compressed variable name must not be larger than forward mapping");

                    forward[to++] = from;
                }
            }
        }
    }

    /**
     * Returns a literal from outside and import it into the compressed formula. This means that eventually
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
        assert(var(lit) <= mapping.size() && "Variable must not be larger than current mapping");
        
        if (isAvailable()) {
           Var compressed = mapping[var(lit)];
        
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

        assert(var <= mapping.size() && "Variable must not be larger than current mapping");

        if (isAvailable()) {
            Var compressed = mapping[var];

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
     * be translated to the name / value from the original uncompressed formula.
     *
     * As in the import methods, the passed argument will be returned as a default value if no
     * compression is available (identity map).
     *
     * @return (old) literal name in the original uncompressed formula
     */
    inline Lit exportLit(const Lit& lit) const
    {
        assert(var(lit) <= forward.size() && "Variable must not be larger than current mapping");

        if (isAvailable()) {
            const Var original = forward[var(lit)];

            // variable was removed by compression, but is requested for export
            assert(original != NOT_MAPPED && "Variable is not in compression map. "
                                             "Do you forget to update compressed variables?");

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

        assert(var <= forward.size() && "Variable must not be larger than current mapping");

        if (isAvailable()) {
            const Var original = forward[var];

            assert(original != NOT_MAPPED && "Variable is not in compression map. "
                                             "Do you forget to update compressed variables?");
            return forward[var];
        } else {
            return var;
        }
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
