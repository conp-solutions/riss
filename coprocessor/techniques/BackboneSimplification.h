/************************************************************************[BackboneSimplification.h]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#pragma once

#include "Propagation.h"
#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Technique.h"
#include "riss/core/Solver.h"

#include <memory>
#include <set>

namespace Coprocessor {

   /**
    * @brief Class implementing Backbone Simplification as a procedure
    * 
    * @details For the original algorithm see: https://www.cril.univ-artois.fr/KC/documents/lagniez-marquis-aaai14.pdf
    * Application of this procedure will result in an equivalent output fourmla
    */
   class BackboneSimplification : public Technique<BackboneSimplification> {

   public:

      /**
       * @brief Construct a new Backbone Simplification procedure
       * 
       * @param solver The solver to use for this
       */
      BackboneSimplification(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller);

      /**
       * @brief Computes a backbone
       * 
       * @param formula The formula to get the backbone for 
       * @return cnf::Literals The literals of the backbone
       */
      cnf::Literals getBackbone(const cnf::CNF& formula) const;

   protected:
   
      /**
       * @brief Apply Backbone Simplification to a formula
       * 
       * @param formula The formula to apply to
       * @return bool True on success
       */
      bool impl(cnf::CNF& formula) override;
   };

}
