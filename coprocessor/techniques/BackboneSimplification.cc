/***********************************************************************[BackboneSimplification.cc]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#include "BackboneSimplification.h"

#include <algorithm>
#include <cmath>

using namespace Riss;

namespace Coprocessor {

    BackboneSimplification::BackboneSimplification(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data,
                                                   Propagation& _propagation)
        : Technique(_config, _ca, _controller)
        , data(_data)
        , propagation(_propagation) {
    }

    void BackboneSimplification::reset() const {
    }

    bool BackboneSimplification::process() {
        std::vector<Lit> backbone = this->getBackbone();

        // add new unit clauses
        for (const Lit& l : backbone) {
            CRef newClause(l.x);
            data.addClause(newClause);
        }

        propagation.process(data, true);

        return true;
    }

    std::vector<Lit> BackboneSimplification::getBackbone() const {
        // util::Utility::logDebug("Computing Backbone");

        // util::Utility::startTimer("backbone calculation");

        // create copy of the formula to work on
        std::vector<Lit> backbone;

        // Compute a model
        cnf::Model startingModel = this->solver->getModel(workingFormula);

        if (startingModel.empty()) {
            // If the formula is unsatisfiable, the backbone is empty
            return {};
        }

        std::vector<Lit> remainingLiterals;
        for (int32_t i = 1; i < startingModel.size(); ++i) {
            if (startingModel[i]) {
                remainingLiterals.push_back(mkLit(i));
            } else {
                remainingLiterals.push_back(mkLit(-i));
            }
        }

        size_t i = 0;

        BooleanConstraintPropagation bcp;

        // Main loop
        while (!remainingLiterals.empty()) {
            ++i;

            // currently used literal, compress
            const int currentLiteral = /*workingFormula.compress*/ (remainingLiterals[0]);

            workingFormula.push_back(std::make_unique<cnf::Clause>(std::initializer_list<int>({-currentLiteral}))); // add negated literal
            cnf::Model model = this->solver->getModel(workingFormula);

            if (model.empty()) {
                // if there's no model then the literal is in the backbone
                // use the uncompressed remaining Literal for the backbone
                backbone.push_back(remainingLiterals[0]);
                workingFormula.erase(workingFormula.end()); // remove last clause again

                bcp.applySingleLiteralEq(workingFormula, currentLiteral); // can propagate the literal we learned
                // workingFormula.setLiteralBackpropagated(currentLiteral);

                remainingLiterals.erase(remainingLiterals.begin()); // remove the literal that was tried
            } else {
                // workingFormula.decompress(model);

                remainingLiterals.erase(remainingLiterals.begin()); // remove the literal that was tried

                remainingLiterals.erase(std::remove_if(remainingLiterals.begin(), remainingLiterals.end(),
                                                       [&](int lit) {
                                                           return model[std::abs(lit)] != (lit > 0);
                                                       }),
                                        remainingLiterals.end());

                workingFormula.erase(workingFormula.end()); // remove last clause again

                if (remainingLiterals.empty()) { // just in case remaining Literals has been emptied by remove_if
                    break;
                }
            }
        }

        // util::Utility::logInfo("Backbone computation took ", util::Utility::durationToString(util::Utility::stopTimer("backbone calculation")));

        return backbone;
    }

} // namespace Coprocessor
