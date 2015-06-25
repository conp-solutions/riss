#ifndef MINISAT_TO_PBLIB_H_
#define MINISAT_TO_PBLIB_H_

#include <vector>
#include <assert.h>

namespace PBLib
{

class MinisatToPBLib
{
  private:
    MinisatToPBLib() = delete;
  public:
    // TODO add this into a cc file!

    static int32_t litToInteger(const NSPACE::Lit& l)
    {
        return NSPACE::sign(l) ? - NSPACE::var(l) - 1  : NSPACE::var(l) + 1 ;
    }

    static NSPACE::Lit integerToLit(const int32_t l)
    {
        return (l > 0) ? NSPACE::mkLit(l - 1, false) : NSPACE::mkLit(-l - 1, true);
    }

    static void integerClauseToLitClause(std::vector<int32_t>& clause, NSPACE::vec<NSPACE::Lit>& output)
    {
        output.clear();
        for (int32_t lit : clause) {
            output.push(integerToLit(lit));
        }
    }

    static std::vector<PBLib::WeightedLit> createCardinalityWeightedLiterals(NSPACE::vec< NSPACE::Lit >& minisat_literals)
    {
        std::vector<WeightedLit> wLits;
        for (size_t i = 0; i < minisat_literals.size(); ++i) {
            wLits.push_back(PBLib::WeightedLit(litToInteger(minisat_literals[i]), 1));
        }

        return wLits;
    }

    static std::vector<PBLib::WeightedLit> createWeightedLiterals(NSPACE::vec< NSPACE::Lit >& minisat_literals, NSPACE::vec<int>& weights)
    {
        assert(minisat_literals.size() == weights.size());
        std::vector<WeightedLit> wLits;
        for (size_t i = 0; i < minisat_literals.size(); ++i) {
            wLits.push_back(PBLib::WeightedLit(litToInteger(minisat_literals[i]), weights[i]));
        }

        return wLits;
    }


};
}
#endif // MINISAT_TO_PBLIB_H_
