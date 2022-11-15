/****************************************************************************************[Dense.cc]
Copyright (c) 2013, Norbert Manthey, LGPL v2, see LICENSE
**************************************************************************************************/

#include "Dense.h"

#include <fstream>
#include <iterator> // istream_iterator

using namespace std;
using namespace Riss;

namespace Coprocessor {

    Dense::Dense(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation)
        : Technique(_config, _ca, _controller)
        , data(_data)
        , propagation(_propagation)
        , compression(_data.getCompression()) {
    }

    void Dense::compress(bool addClausesToLists, const char* newWhiteFile) {
        if (propagation.process(data) == l_False) {
            data.setFailed();
        }
        if (!data.ok()) {
            return;
        }

        DOUT(if (config.dense_debug_out) { cerr << "c dense compress" << endl; });

        // reset literal counter
        count.assign(data.nVars(), 0);

        // count literals occuring in clauses
        countLiterals(count, data.getClauses());
        countLiterals(count, data.getLearnts());

        // resize and clear temporary mappings - the mappings will only be used if the fragmentation is high enough
        mapping.resize(data.nVars());
        trail.resize(data.nVars());
        fill(mapping.begin(), mapping.end(), Compression::UNIT);
        fill(trail.begin(), trail.end(), l_Undef);

        uint32_t diff = 0;
        for (Var var = 0; var < data.nVars(); var++) {
            assert(diff <= var && "there cannot be more variables then variables that have been analyzed so far");

            // keep variable
            if (count[var] != 0                                                   // literal does occure in formula
                || data.doNotTouch(var)                                           // variable must not be modified
                || (config.opt_dense_keep_assigned && data.value(var) != l_Undef) // do not drop assigned variables
            ) {
                mapping[var] = var - diff;

                DOUT(if (config.dense_debug_out && data.doNotTouch(var)) {
                    cerr << "c mapping for variable " << var + 1 << " is: " << mapping[var] + 1 << endl;
                });
            }
            // compress (remove) variable
            else {
                diff++;
                if (data.value(var) != l_Undef) {
                    trail[var] = data.value(var);
                    assert(mapping[var] == Compression::UNIT && "this variable has an assignment");

                    DOUT(if (config.dense_debug_out) {
                        if (data.doNotTouch(var)) {
                            cerr << "c do not touch variable will be dropped: " << var + 1 << " mapping: " << mapping[var] + 1 << endl;
                        } else {
                            cerr << "c variable " << var + 1 << " occurrs " << count[var]
                                 << " times, and is undefined: " << (data.value(mkLit(var, false)) == l_Undef) << endl;
                        }
                    });
                } else {
                    assert(mapping[var] == Compression::UNIT &&
                           "variables that are not present in the formula, and that do not have a truth value, are treated as gap");
                }
            }
        }

        // formula is already compact or not too loose
        if (isCompact(diff)) {
            DOUT(if (config.dense_debug_out > 0) cerr << "c [DENSE] no fragmentation, do not compress!" << endl;);
            return;
        }

        compression.update(mapping, trail);

        // replace everything in the clauses
        DOUT(if (config.dense_debug_out > 0) cerr << "c [DENSE] compress clauses" << endl;);
        compressClauses(data.getClauses());
        compressClauses(data.getLearnts());

        compressLiterals(data.getEquivalences());

        // write white file
        if (newWhiteFile != nullptr) {
            cerr << "c work with newWhiteFile " << newWhiteFile << endl;
            ofstream file(newWhiteFile, ios::out);

            // dump import mapping info
            for (Var v = 0; v < data.nVars(); ++v) {
                if (data.doNotTouch(v)) {
                    file << compression.importVar(v) << endl;
                }
            }

            file.close();
        }

        // compress trail, so that it can still be used!
        DOUT(if (config.dense_debug_out) {
            cerr << "c before trail: ";
            printTrail();
        });

        // erase all top level units after backup!
        // keep the literals that are not removed!
        int j = 0; // count literals that stay on trail!
        vec<Riss::Lit>& _trail = data.getTrail();

        for (int i = 0; i < _trail.size(); ++i) {
            const Lit lit = _trail[i];

            assert(count[var(lit)] == 0 && "all units should be handled here");

            // keep the assigned literal
            if (count[var(lit)] != 0 || data.doNotTouch(var(lit)) || config.opt_dense_keep_assigned) {
                Lit compressed = compression.importLit(lit);
                _trail[j++] = compressed;

                DOUT(if (config.dense_debug_out) {
                    cerr << "c move literal " << lit << " from " << i << " to pos " << j << " as " << compressed << endl;
                });
            }
            // remove literal from trail
            // the assigned value (lbool) is already kept in the compression object
            else {
                data.resetAssignment(var(lit));
            }
        }
        // do not clear, but resize to keep the literals that have not been removed
        _trail.shrink_(_trail.size() - j);

        // reset propagated literals in UP
        propagation.reset(data);

        DOUT(if (config.dense_debug_out) {
            cerr << "c final trail: ";
            printTrail();
        });

        // rewriting everything finnished
        // re-arrange all variable data
        for (Var var = 0; var < data.nVars(); var++) {
            if (!config.opt_dense_keep_assigned) {
                assert((data.doNotTouch(var) || data.value(var) == l_Undef) && "Assigned variable are not allowed");
            }

            // if( v+1 == data.nVars() ) cerr << "c final round with variable " << v+1 << endl;

            const Var compressed = compression.importVar(var);

            // the variable was not removed from the formula
            if (compressed != var_Undef) {
                DOUT(if (config.dense_debug_out) { cerr << "c map " << (var + 1) << " to " << (compressed + 1) << endl; });
                // if( v+1 == data.nVars() ) cerr << "c final round with move" << endl;
                // cerr << "c move intern var " << v << " to " << compression.mapping[v] << endl;

                // map variable, and if done, re-organise everything
                data.moveVar(var, compressed, var + 1 == data.nVars());

                if (!config.opt_dense_keep_assigned) {
                    assert((data.doNotTouch(compressed) || data.value(compressed) == l_Undef) && "Assigned variable are not allowed");
                }
            }
            // variable was removed by compression (that means the variable is a unit)
            else {
                DOUT(if (config.dense_debug_out) cerr << "c remove variable " << var + 1 << endl;);
                // if( v+1 == data.nVars() ) cerr << "c final round without move" << endl;

                // any variable that is not replaced should have l_Undef
                // compress number of variables
                if (var + 1 == data.nVars()) {
                    data.moveVar(var - diff, var - diff, true);
                }
            }
        }

        // after full compression took place, set known assignments again
        for (int i = 0; i < _trail.size(); ++i) {
            // put rewritten literals back on the trail
            assert(var(_trail[i]) <= data.nVars() && "all variables had to be compressed");
            data.enqueue(_trail[i]);
        }

        // ensure we compressed something
        DOUT(if (data.nVars() + diff != compression.nvars()) {
            cerr << "c number of variables does not match: " << endl
                 << "c diff= " << diff << " old= " << compression.nvars() << " new=" << data.nVars() << endl;
        });
        assert(data.nVars() + diff == compression.nvars() && "number of variables has to be reduced");

        // add the clauses to the structures again. as all clauses have been rewritten, we clear all lists and add all clauses again
        if (addClausesToLists) {
            data.cleanOccurrences(); // clear all occurrences
            for (int i = 0; i < data.getClauses().size(); ++i) {
                const Clause& c = ca[data.getClauses()[i]];
                if (!c.can_be_deleted()) {
                    data.addClause(data.getClauses()[i]);
                    for (int j = 0; j < c.size(); ++j) {
                        assert(var(c[j]) <= data.nVars() && "all variables had to be compressed");
                    }
                }
            }
            for (int i = 0; i < data.getLearnts().size(); ++i) {
                const Clause& c = ca[data.getLearnts()[i]];
                if (!c.can_be_deleted()) {
                    data.addClause(data.getLearnts()[i]);
                    for (int j = 0; j < c.size(); ++j) {
                        assert(var(c[j]) <= data.nVars() && "all variables had to be compressed");
                    }
                }
            }
        }

        // get replaced by structure right
        data.replacedBy().clear();
        for (Var v = 0; v < data.nVars(); ++v) {
            data.replacedBy().push(mkLit(v, false));
        }

        // notify data about compression
        data.didCompress();
    }

    void Dense::decompress(vec<lbool>& model) {
        // only decompress, if mapping is available -- the compression would map the forumla
        // to itself (identity map), but this would be a waste of resources
        if (!compression.isAvailable()) {
            DOUT(if (config.dense_debug_out) { cerr << "c no decompression needed" << endl; });
            return;
        }

        DOUT(if (config.dense_debug_out) {
            cerr << "c dense decompress, model to work on: ";
            printModel(model);

            if (!compression.isAvailable()) {
                cerr << "c no decompression" << endl;
            } else {
                cerr << "c [DENSE] change number of variables from " << model.size() << " to " << compression.nvars() << endl;
            }
        });

        assert(!compression.isDecompressing() && "compression should be decompressing only in this method");
        compression.setDecompressing(true);

        // extend the assignment, so that it is large enough
        if (model.size() < compression.nvars()) {
            model.growTo(compression.nvars(), l_False);
            while (data.nVars() < compression.nvars()) {
                data.nextFreshVariable('o');
            }
        }

        // backwards, because variables will increase - do not overwrite old values!
        for (int var = compression.nvars() - 1; var >= 0; --var) {
            const Var compressed = compression.importVar(var);

            // units - simply copy, because they do not occure in the compressed formula
            if (compressed == Compression::UNIT) {
                lbool unit = compression.value(var);
                if (unit != l_Undef) {
                    model[var] = unit;
                } // only use, if a value has been set

                DOUT(if (config.dense_debug_out) { cerr << "c satisfy " << var + 1 << "=" << ((unit == l_True) ? "true" : "false") << endl; });
            }
            // renamed variable
            else if (compressed != var) {
                DOUT(if (config.dense_debug_out) { cerr << "c move variable " << compressed + 1 << " to " << var + 1 << endl; });

                // Copy assignment from the compressed variable name to the variable
                // name in the original formula.
                // This works, because the variable name only decreases during
                // compression and we iterate reverse over all variables. Therefore
                // we do not overwrite any assignment.
                model[var] = model[compressed];
            }
            // unchanged variable
            else {
                DOUT(if (config.dense_debug_out) { cerr << "c variable " << var + 1 << " unchanged" << endl; });
            }
        }

        compression.setDecompressing(false);
        assert(!compression.isDecompressing() && "compression should be decompressing only in this method");

        DOUT(if (config.dense_debug_out) {
            cerr << "c decompressed model: ";
            printModel(model);
        });
    }

    bool Dense::writeCompressionMap(const string& filename) {
        DOUT(if (config.dense_debug_out) { cerr << "c write compression map" << endl; });

#ifdef NDEBUG
        return compression.serialize(filename, false);
#else
        return compression.serialize(filename, config.dense_debug_out);
#endif
    }

    bool Dense::readCompressionMap(const string& filename) {
        DOUT(if (config.dense_debug_out) { cerr << "c read compression map" << endl; });

#ifdef NDEBUG
        return compression.deserialize(filename, false);
#else
        return compression.deserialize(filename, config.dense_debug_out);
#endif
    }

    /** parse a clause from string and store it in the literals vector
     *
     */
    static int parseClause(const string& line, vector<Lit>& literals) {
        uint32_t ind = 0;
        // skip whitespaces
        while (ind < line.size() && line[ind] == ' ') {
            ++ind;
        }
        // parse numbers
        while (line.size() > ind) { // read each single number
            // cerr << "c FP check(" << lines << "," << ind << "): " << line[ind] << endl;
            int32_t number = 0;
            bool negative = false;

            while (ind < line.size() && line[ind] == '-') {
                negative = true;
                ind++;
            }
            // read next number here
            while (ind < line.size() && line[ind] >= '0' && line[ind] <= '9') {
                number *= 10;
                number += line[ind++] - '0';
            }

            if (number == 0) {
                break;
            } // there may be some symbols behind a 0, but they do not matter

            // put right sign on the number
            // number = (negative) ? 0 - number : number;

            const Lit lit1 = mkLit(number - 1, negative);
            literals.push_back(lit1);

            // skip whitespaces
            while (line[ind] == ' ') {
                ind++;
            }
        }
        return 0;
    }

    void Dense::printStatistics(ostream& stream) {
        cerr << "c [STAT] DENSE " << compression.diff() << " gaps" << endl;
    }

    void Dense::compressLiterals(vec<Riss::Lit>& literals) {
        int j = 0;
        for (uint32_t i = 0; i < literals.size(); ++i) {
            if (literals[i] == lit_Undef) {
                literals[j++] = lit_Undef;
                continue;
            }

            const Lit compressed = compression.importLit(literals[i]);

            if (compressed == lit_Undef) {
                continue;
            }                           // drop literals without copression
            literals[j++] = compressed; // store compressed literal
        }
    }

    void Dense::compressClauses(vec<CRef>& clauses) {
        // iterate over all clauses in the list
        for (uint32_t i = 0; i < clauses.size(); ++i) {
            Clause& clause = ca[clauses[i]];

            // only compress the claues if it is not marked for deletion
            if (!clause.can_be_deleted()) {
                assert(clause.size() > 1 && "do not rewrite unit clauses!");

                DOUT(if (config.dense_debug_out > 1) { cerr << "c [DENSE] rewrite clause [" << clauses[i] << "] " << clause << endl; });

                // iterate over all literals in the clause
                for (uint32_t j = 0; j < clause.size(); ++j) {
                    const Lit lit = clause[j];
                    const Lit compressed = compression.importLit(lit);

                    // drop this clause because the current variable is not present in the formula any more
                    if (clause.learnt() && compressed == lit_Undef) {
                        DOUT(if (config.dense_debug_out > 1) {
                            cerr << "c [DENSE] into deleted clause, because variable " << var(lit) << " does not occur in non-learned clauses"
                                 << endl;
                        });
                        clause.set_delete(true);
                        break;
                    }
                    // if( debug > 1 ) { cerr << "c compress literal " << lit.nr() << endl; }

                    assert(compressed != lit_Undef && "only move variable, if its not a unit");

                    // rewrite clause literal
                    clause[j] = compressed;

                    // polarity of literal has to be kept
                    assert(sign(lit) == sign(compressed) && "sign of literal must not change");
                }

                DOUT(if (config.dense_debug_out > 1) { cerr << "c [DENSE] into [" << clauses[i] << "]           " << clause << endl; });
            }
        }
    }

} // namespace Coprocessor
