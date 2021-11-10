/***********************************************************************[BackboneSimplification.cc]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#include "BackboneSimplification.h"

#include <algorithm>
#include <cmath>

using namespace Riss;

namespace Coprocessor {

   BackboneSimplification::BackboneSimplification(CP3Config& _config, ClauseAllocator& _ca,
        ThreadController& _controller) 
      : Technique(_config, _ca, _controller) {

   }

   bool BackboneSimplification::impl(cnf::CNF& formula) {
      cnf::Literals backbone = this->getBackbone(formula);

      const uint64_t variablesWithoutBackbone = formula.getVariables() - backbone.size();
      util::Utility::logInfo("Expected number of variables after removing backbone: ", variablesWithoutBackbone);

      BooleanConstraintPropagation bcp;
      bcp.applyLiterals(formula, backbone);

      //every non-backbone variable that disappears in this process can have any value -> halving number of possible models
      util::Utility::logInfo("Actual number of variables after removing backbone:   ", formula.getVariables());

      const uint64_t independentRemovedVariabels = variablesWithoutBackbone - formula.getVariables();

      util::Utility::logInfo("That means a factor of ", std::pow(2, independentRemovedVariabels));

      formula.compress();

      return true;
   }

   cnf::Literals BackboneSimplification::getBackbone(const cnf::CNF& formula) const {
      util::Utility::logDebug("Computing Backbone");

      util::Utility::startTimer("backbone calculation");

      // create copy of the formula to work on
      cnf::CNF workingFormula = formula;
      cnf::Literals backbone;

      // Compute a model
      cnf::Model startingModel = this->solver->getModel(workingFormula);

      if (startingModel.empty()) {
         // If the formula is unsatisfiable, the backbone is empty
         return {};
      }

      // Convert Model to a literalSet, this will stay uncompressed the entire time
      cnf::Literals remainingLiterals;
      for (size_t i = 1; i < startingModel.size(); ++i) {
         if (startingModel[i]) {
            remainingLiterals.push_back(i);
         }
         else {
            remainingLiterals.push_back(-i);
         }
      }

      size_t i = 0;

      BooleanConstraintPropagation bcp;

      // Main loop
      while (!remainingLiterals.empty()) {
         ++i;
         
         // currently used literal, compress
         const int currentLiteral = /*workingFormula.compress*/(remainingLiterals[0]);

         workingFormula.push_back(std::make_unique<cnf::Clause>(std::initializer_list<int>({-currentLiteral})));     // add negated literal
         cnf::Model model = this->solver->getModel(workingFormula);
         
         if (model.empty()) {
            // if there's no model then the literal is in the backbone
            // use the uncompressed remaining Literal for the backbone
            backbone.push_back(remainingLiterals[0]);
            workingFormula.erase(workingFormula.end());                       // remove last clause again
            
            bcp.applySingleLiteralEq(workingFormula, currentLiteral);         // can propagate the literal we learned
            //workingFormula.setLiteralBackpropagated(currentLiteral);

            remainingLiterals.erase(remainingLiterals.begin());      // remove the literal that was tried
         }
         else {
            //workingFormula.decompress(model);
            
            remainingLiterals.erase(remainingLiterals.begin());      // remove the literal that was tried

            remainingLiterals.erase(
               std::remove_if(remainingLiterals.begin(), remainingLiterals.end(), 
                  [&](int lit) {
                     return model[std::abs(lit)] != (lit > 0);
                  }
               ),
               remainingLiterals.end()
            );

            workingFormula.erase(workingFormula.end());           // remove last clause again

            if (remainingLiterals.empty()) {                      // just in case remaining Literals has been emptied by remove_if
               break;
            }
         }

      }

      util::Utility::logInfo("Backbone computation took ", util::Utility::durationToString(util::Utility::stopTimer("backbone calculation")));

      return backbone;
   }

}
