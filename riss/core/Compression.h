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
 * Container for compression information. It holds the tables that are
 * required for the variable renaming after the formular was compressed
 * and you want to export or import variables / literals to / from the
 * outside environment of the solver.
 *
 * @author Lucas Kahlert <lucas.kahlert@tu-dresden.de>
 */
class Compression
{
    std::vector<Var> mapping;   // mapping from old variables to new ones
    std::vector<Var> forward;   // store to which new variable an old variable has been mapped

    uint32_t postnvars;         // number of variables after compression

    std::vector<lbool> trail;   // already assigned literals (units) - this variables are removed from the formula

  public:

    enum {
        NOT_MAPPED = -2, // variable is not mapped
        UNIT       = -1, // variable is unit and therefore removed in the compressed formula
    };

    Compression() : postnvars(0) {};

    /**
     * @return true, if compression is active and variable renaming was performed
     */
    inline bool isAvailable() const { return mapping.size() == 0; }

    /**
     * Clear and resize all mappings
     */
    void reset(int nvars)
    {
        DOUT(std::cerr << "c Reset compression" << std::endl;);

        // ensure our mapping tables are big enough
        mapping.resize(nvars);
        forward.resize(nvars);

        // remove all units
        trail.clear();

        // reset all mappings
        std::fill(mapping.begin(), mapping.end(), NOT_MAPPED);
        std::fill(forward.begin(), forward.end(), NOT_MAPPED);

        postvariables = nvars;
    }

    /**
     * Use the passed mapping as the new mapping. The current trail and forward mapping will be adjusted
     */
    void update(std::vector<Riss::Var>& _mapping, std::vector<lbool>& _trail)
    {
        if (!isAvailable()) {
            // simply copying the content of the mappings - this is the inital mapping
            mapping = _mapping;
            trail = _trail;
        } else {

        }
    }

    /**
     * Returns a literal from outside and import it into the compressed formula. This means that eventually
     * a smaller number will be returned. If the literal is a unit in the compressed clause and was therefore
     * removed, lit_Undef will be returned.
     *
     * If no compression is available (which means, the formula was never compressed) the literal is returned
     * unchanged (identity map).
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
     * Export a literal from the compressed formula to the outside environment. The literal will be translated
     * to the name / value from the original uncompressed formula.
     *
     * As in the import methods, the passed argument will be returned as a default value if no compression
     * is available (identity map).
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
     * Returns the number of variables reduced by compression
     * between the */
    inline uint32_t diff() const
    {
        return variables - postvariables;
    }

};

} // namespace Riss

#endif // RISS_COMPRESSION_H
