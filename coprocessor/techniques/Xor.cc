/******************************************************************************************[XOR.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "Xor.h"

using namespace Riss;
using namespace std;

namespace Coprocessor
{

XorReasoning::XorReasoning(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data,  Propagation& _propagation, EquivalenceElimination& _ee)
    : Technique(_config, _ca, _controller)
    , data(_data)
    , propagation(_propagation)
    , ee(_ee)
    , processTime(0)
    , parseTime(0)
    , reasonTime(0)
    , findChecks(0)
    , xors(0)
    , xorClauses(0)
    , resolvedInCls(0)
    , subsFound(0)
    , resFound(0)
    , resTauts(0)
    , resStrength(0)
    , xorLimit(config.opt_xorFindLimit)
    , xorSteps(0)
    , foundEmptyLists(0)
    , xorUnits(0)
    , allUsed(0)
    , xorDeducedUnits(0)
    , eqs(0)
    , addedTernaryXors(0)
    , addedQuadraryXors(0)
    , participatingXorClauses(0)
    , participatingXorVariables(0)
    , clauseRatio(0)
    , variableRatio(0)
    , xorProps(0)
    , clsProps(0)
    , simDecisions(0)
{

}

void XorReasoning::reset()
{

}

bool XorReasoning::process()
{
    MethodTimer mt(&processTime);
    if (! performSimplification()) { return false; }   // do not do anything?!
    modifiedFormula = false;

    // do not simplify, if the formula is considered to be too large!
    if (!data.unlimited() && (data.nVars() > config.opt_xor_vars && data.getClauses().size() + data.getLEarnts().size() > config.opt_xor_cls && data.nTotLits() > config.opt_xor_lits)) { return false; }

    if (data.outputsProof()) { printDRUPwarning(cerr, "XOR cannot produce DRUP/DRAT proofs yet"); }

    data.ma.resize(2 * data.nVars());   // TODO: check whether only for variables!
    reAddedClauses.clear();

    // find xors
    parseTime = cpuTime() - parseTime;
    vector<GaussXor> xorList;
    findXor(xorList); // fills the list with CR of clauses that contains xors
    // perform gauss elimination
    DOUT(if (config.opt_xor_debug > 2) {
    for (int i = 0 ; i < xorList.size(); ++ i) {
            cerr << "c [" << i << "]: "
                 << " + "; for (int j = 0 ; j < xorList[i].vars.size(); ++ j) { cerr << xorList[i].vars[j] + 1 << " + "; }  cerr << " == " << (xorList[i].k ? 1 : 0) << endl;
        }
    });
    parseTime = cpuTime() - parseTime;
    // VE on XOR: if variable occurs only in 2 xors, then xor the two xors, and remove the initial two xors! (only for reasoning)

    reasonTime = cpuTime() - reasonTime;
    vector< vector<int> > occs(data.nVars()); // pointer to xors in xorList
    // add all xors to the data structures
    for (int i = 0 ; i < xorList.size(); ++ i) {
        for (int j = 0 ; j < xorList[i].vars.size(); ++ j) { occs[ xorList[i].vars[j] ].push_back(i); }
    }

    // setup and fill heap
    Heap<VarLt> heap(occs);
    for (Var v = 0 ; v < data.nVars(); ++ v) if (!heap.inHeap(v)) { heap.insert(v); }

    vector<Lit> unitQueue;
    data.ma.resize(2 * data.nVars());
    data.ma.nextStep();

    vector<Var> toRemoveVars, tmpVars, newlyAddedVariables;
    xorOrder.clear(); // new removal
    // do elimination (BVE style)
    while (heap.size() > 0 && (data.unlimited() || xorLimit > xorSteps) && !data.isInterupted()) {
        // if propagate, propagate!
        if (!propagate(unitQueue, data.ma, occs, xorList)) {
            data.setFailed(); break;
        }

        const Var v = heap[0];
        assert(heap.inHeap(v) && "item from the heap has to be on the heap");
        heap.removeMin();
        xorOrder.push(v);   // memorize that we used 'v' next

        DOUT(if (config.opt_xor_debug > 4) {
        cerr << "c check XORs for " << v + 1 << endl;
        cerr << "c full XOR list:" << endl;
        for (int i = 0 ; i < xorList.size(); ++ i) {
                cerr << "c [" << i << "]: "
                     << " + "; for (int j = 0 ; j < xorList[i].vars.size(); ++ j) { cerr << xorList[i].vars[j] + 1 << " + "; }  cerr << " == " << (xorList[i].k ? 1 : 0) << endl;
            }
            cerr << endl << endl;
        }
            );

        // there are no XORs
        if (occs[v].size() == 0) {
            foundEmptyLists ++;
            DOUT(if (config.opt_xor_debug > 4) cerr << "c no occurences for " << v + 1 << endl;);
            continue; // do not work with empty lists!
        }

        // there is only one xor
        if (occs[v].size() == 1) {
            DOUT(if (config.opt_xor_debug > 4) cerr << "c there is a pure XOR on " << v + 1 << " of size " << xorList[occs[v][0]].size() << endl;);
            GaussXor& x = xorList[occs[v][0]];
            if (x.unit()) {   // the XOR is a unit, add it as unit clause
                if (!data.ma.isCurrentStep(toInt(x.getUnitLit()))) {
                    data.ma.setCurrentStep(toInt(x.getUnitLit()));   // memorize the unit
                    xorUnits ++;
                    unitQueue.push_back(x.getUnitLit());
                }
            }
            if (config.opt_xor_dropPure) {
                DOUT(if (config.opt_xor_debug > 4) cerr << "c drop the XOR " << endl;);
                dropXor(occs[v][0], x.vars, occs);
            }
            continue;
        }

        // there are multiple XORs with v

        int selectIndex = 0; // simply select first xor
        while (selectIndex < occs[v].size() && xorList[ occs[v][selectIndex] ].used == true) { selectIndex ++; }   // ignore this XOR
        if (selectIndex == occs[v].size()) {
            allUsed ++;
            DOUT(if (config.opt_xor_debug > 2) cerr << "c all XORs with " << v + 1 << " are already used!!" << endl;);
            continue; // do not use an XOR twice!
        }

        // select an XOR from this list
        if (config.opt_xor_selectX == 1) {   // select smallest xor
            for (int i = selectIndex + 1 ; i < occs[v].size(); ++ i) {
                xorSteps ++;
                if (xorList[ occs[v][i] ].used == true) { continue; }
                if (xorList[ occs[v][selectIndex] ].size() > xorList[occs[v][i]].size()) { selectIndex = i; }
            }
        } // TODO: other strategies to select the XOR?!
        assert(selectIndex < occs[v].size() && "index has to stay in list");
        assert(xorList[ occs[v][selectIndex] ].used == false && "cannot use the same xor twice for simplification!");

        GaussXor& selectedX = xorList[ occs[v][selectIndex] ];
        const int xorIndex  = occs[v][selectIndex];
        selectedX.used      = true; // indicate that this xor was used!

        DOUT(if (config.opt_xor_debug > 1) { cerr << "c eliminate " << v + 1 << " with XOR " <<  " + "; for (int j = 0 ; j < selectedX.vars.size(); ++ j) { cerr << selectedX.vars[j] + 1 << " + "; }  cerr << " == " << (selectedX.k ? 1 : 0) << endl; });

        // if there are XORs to work with
        while (occs[v].size() > 0) {

            if (occs[v][0] == xorIndex) {
                occs[v][0] = occs[v][ occs[v].size() - 1]; occs[v].pop_back(); // drop it from this list!
                continue; // do not simplify xor with itself!
            }

            xorSteps ++;
            GaussXor& simpX = xorList[ occs[v][0] ]; // xor that is simplified
            DOUT(if (config.opt_xor_debug > 1) { cerr << "c change XOR " <<  " + "; for (int j = 0 ; j < simpX.vars.size(); ++ j) { cerr << simpX.vars[j] + 1 << " + "; }  cerr << " == " << (simpX.k ? 1 : 0) << endl; });
            toRemoveVars.clear(); tmpVars.clear(); newlyAddedVariables.clear();
            simpX.add(selectedX, toRemoveVars, tmpVars, newlyAddedVariables);
            DOUT(if (config.opt_xor_debug > 1) {
            cerr << "c       into " <<  " + "; for (int j = 0 ; j < simpX.vars.size(); ++ j) { cerr << simpX.vars[j] + 1 << " + "; }  cerr << " == " << (simpX.k ? 1 : 0) << endl;
                cerr << "c and remove from "; for (int j = 0 ; j < toRemoveVars.size(); ++ j) { cerr << " " << toRemoveVars[j] + 1; }  cerr << " " << endl;
            });

            // drop after all operations, so that refrences stay consistent!
            dropXor(occs[v][0], toRemoveVars, occs); // remvoe the simplified xor from all variables that are not part of it any longer!

            if (config.opt_xor_addOnNewlyAdded) {
                for (int j = 0 ; j < newlyAddedVariables.size(); ++ j) {
                    occs[ newlyAddedVariables[j] ].push_back(occs[v][0]);
                }
            }


            if (simpX.unit()) {
                if (!data.ma.isCurrentStep(toInt(simpX.getUnitLit()))) {
                    data.ma.setCurrentStep(toInt(simpX.getUnitLit()));
                    xorDeducedUnits ++;
                    unitQueue.push_back(simpX.getUnitLit());
                    DOUT(if (config.opt_xor_debug > 1) { cerr << "c created unit " << simpX.getUnitLit() << " from  XOR " <<  " + "; for (int j = 0 ; j < simpX.vars.size(); ++ j) { cerr << simpX.vars[j] + 1 << " + "; }  cerr << " == " << (simpX.k ? 1 : 0) << endl; });
                }
            } else if (simpX.size() == 2) {
                eqs ++;
                data.addEquivalences(mkLit(simpX.vars[0], false), mkLit(simpX.vars[1], simpX.k));   // add found equivalence!
                DOUT(if (config.opt_xor_debug > 2) cerr << "c eq lits: " << mkLit(simpX.vars[0], false) << " == " << mkLit(simpX.vars[1], simpX.k) << endl;);
            } else if (simpX.size() <= config.opt_xor_encodeSize) {  // if clauses for this XOR should be kept
                // for now, encode full XOR
                addXorAsClauses(simpX);
            }



        }

        if (config.opt_xor_keepUsed) {
            occs[v].push_back(selectIndex);
        } else {
            dropXor(selectIndex, selectedX.vars, occs);
        }

    }

    if (config.opt_xor_checkNewSubsume) {   // check whether newly added clauses subsume other clauses
        checkReaddedSubsumption();
    }

    // after final round, propagate once more!
    if (!propagate(unitQueue, data.ma, occs, xorList)) {
        data.setFailed(); goto finishedGauss;
    }

    ee.applyEquivalencesToFormula(data);

    if (config.opt_xor_setPolarity != 0) {

        data.ma.nextStep(); // have new "assignment"
        unitQueue.clear();  // have new "assignment"
        for (int i = 0 ; i < xorOrder.size(); ++ i) {
            const Var& v = xorOrder[i];
            if (data.value(mkLit(v)) != l_Undef) { continue; }    // this variable is set already
            Lit l = mkLit(v, config.opt_xor_setPolarity < 0);   // set literal according to polarity
            unitQueue.push_back(l);
            DOUT(if (config.opt_xor_debug > 4) cerr << "c propagate: " << l << " as decision" << endl;);
            data.ma.setCurrentStep(toInt(l));
            simulatePropagate(unitQueue, data.ma, occs, xorList);
            simDecisions ++;
        }
    }

finishedGauss:;

    DOUT(if (config.opt_xor_debug > 4) {
    cerr << "c order of eliminations: ";
    for (int i = 0 ; i < xorOrder.size(); ++ i) { cerr << " " << xorOrder[i] + 1; }
        cerr << endl;
    });
    reasonTime = cpuTime() - reasonTime;

    return false;
}

bool XorReasoning::simulatePropagate(vector< Lit >& unitQueue, MarkArray& ma, vector< vector< int > >& occs, vector< XorReasoning::GaussXor >& xorList, bool ignoreConflicts)
{
    while (unitQueue.size() > 0) {
        const Lit p = unitQueue.back();
        data.getSolver()->setPolarity(var(p), sign(p));
        unitQueue.pop_back();

        // propagate in all xors that contain var(p)
        vector<int>& indexes = occs[ var(p) ];
        for (int i = 0 ; i < indexes.size(); ++ i) {
            GaussXor& x = xorList[indexes[i]];

            bool k = x.k;
            Lit undefLit = lit_Undef;
            for (int j = 0 ; j < x.size(); ++ j) {
                if (!data.ma.isCurrentStep(toInt(mkLit(x.vars[j], false))) && !data.ma.isCurrentStep(toInt(mkLit(x.vars[j], true)))) {
                    undefLit = (undefLit == lit_Undef) ? mkLit(x.vars[j]) : lit_Error; // be able to track unit clauses
                } else if (data.ma.isCurrentStep(toInt(mkLit(x.vars[j], false)))) {
                    k = ! k; // count variables that have been set to true
                } // else if( l ^= l_False) -> remove variable, is done silently
            }

            if ((undefLit == lit_Undef)) {   // this XOR is falsified
                if (k && !ignoreConflicts) { return false; }
            } else if (undefLit != lit_Error) {    // found a unit constraint, add the newly created unit
                Lit l = k ? undefLit : ~undefLit ;
                if (data.ma.isCurrentStep(toInt(~l))) {      // this case should not be possible
                    if (!ignoreConflicts) { return false; }
                } else if (!data.ma.isCurrentStep(toInt(l))) {
                    data.ma.setCurrentStep(toInt(l));
                    unitQueue.push_back(l);
                    xorProps ++;
                    DOUT(if (config.opt_xor_debug > 4) cerr << "c propagate: " << l << " on XOR" << endl;);
                }
            }
        }

        // propagate on clauses here
        vector<CRef>& negative = data.list(~p);
        for (int i = 0 ; i < negative.size(); ++i) {
            Clause& c = ca[ negative[i] ];
            //what if c can be deleted? -> continue
            if (c.can_be_deleted()) { continue; }
            // sorted propagation, no!

            Lit undefLit = lit_Undef;
            for (int j = 0; j < c.size(); ++ j) {
                if (data.ma.isCurrentStep(toInt(c[j]))) { undefLit = lit_Error; break; }
                else if (data.ma.isCurrentStep(toInt(~c[j]))) {
                    continue; // do not look into
                } else {
                    if (undefLit == lit_Undef) { undefLit = c[j]; }
                    else {
                        undefLit = lit_Error;
                        break;
                    }
                }
            }

            if (undefLit == lit_Undef) {   // its a conflict
                if (!ignoreConflicts) { return false; }
            } else if (undefLit != lit_Error) {  // its a unit
                data.ma.setCurrentStep(toInt(undefLit));
                unitQueue.push_back(undefLit);
                clsProps++;
                DOUT(if (config.opt_xor_debug > 4) cerr << "c propagate: " << undefLit << " on CLS" << endl;);
            } // silently ignore the case that the clause is neither unit nor conflict
        }
    }
    return true;
}


bool XorReasoning::propagate(vector< Lit >& unitQueue, MarkArray& ma, vector< std::vector< int > >& occs, vector< XorReasoning::GaussXor >& xorList)
{
    while (unitQueue.size() > 0) {
        const Lit p = unitQueue.back();
        unitQueue.pop_back();
        if (l_False == data.enqueue(p)) { return false; }   // enqueing next literal failed!
        // propagate in all xors that contain var(p)
        vector<int>& indexes = occs[ var(p) ];
        for (int i = 0 ; i < indexes.size(); ++ i) {
            GaussXor& x = xorList[indexes[i]];
            int k = 0;
            for (int j = 0 ; j < x.size(); ++ j) {
                if (data.value(mkLit(x.vars[j], false)) == l_Undef) {
                    x.vars[k++] = x.vars[j]; // keep this variable!
                } else if (data.value(mkLit(x.vars[j], false)) == l_True) {
                    x.k = ! x.k; // remove this variable, memorize that it has been set to true!
                } // else if( l == l_False) -> remove variable, is done silently
            }
            x.vars.resize(k); // shrink vector
            if (x.failed()) { return false; }
            if (x.unit()) {   // add the newly created unit
                if (!data.ma.isCurrentStep(toInt(x.getUnitLit()))) {
                    data.ma.setCurrentStep(toInt(x.getUnitLit()));
                    xorDeducedUnits ++;
                    unitQueue.push_back(x.getUnitLit());
                    DOUT(if (config.opt_xor_debug > 1) { cerr << "c created unit " << x.getUnitLit() << " from  XOR " <<  " + "; for (int j = 0 ; j < x.vars.size(); ++ j) { cerr << x.vars[j] + 1 << " + "; }  cerr << " == " << (x.k ? 1 : 0) << endl; });
                }
            }
        }
        indexes.clear(); // there cannot be XORs with this variable any more
    }
    return true;
}


void XorReasoning::dropXor(int index, vector<Var>& x, vector< std::vector< int > >& occs)
{
    for (int i = 0 ; i < x.size(); ++i) {
        const Var& v = x[i];
        for (int j = 0; j < occs[v].size(); ++ j) if (occs[v][j] == index) {  occs[v][j] = occs[v][occs[v].size() - 1]; occs[v].pop_back(); break; }
    }
}


// calculate the number that is represented by the polarity of the given clause for an XOR
static uint64_t numberByPolarity(const Clause& clause)
{
    uint64_t nr = 0;
    for (uint32_t i = 0 ; i < clause.size(); ++ i) {
        const Lit literal = clause[i];
        nr = nr << 1;
        if (!sign(literal)) { nr += 1; }
    }
    return nr;
}

bool XorReasoning::findXor(vector<GaussXor>& xorList)
{
    bool didSomething = false;
    // sort all clauses, such that they are ordered according to their length
    uint32_t cL = config.opt_xor_backdoor == 0 ? 2 : 1;    // current length (below 5 there cannot be reduced anything)
    uint32_t cP = 0;    // current moveTo position

    participatingXorClauses = 0;

    // sort clauses
//  cerr << "c before sort: " << endl;
//  for( int i = 0 ; i <data.getClauses().size() ; ++ i ) cerr << ca[ data.getClauses()[i] ] << endl;
    data.sortClauseLists();
    DOUT(if (config.opt_xor_debug > 3) {
    cerr << "c after sort: " << endl;
    for (int i = 0 ; i < data.getClauses().size() ; ++ i) { cerr << ca[ data.getClauses()[i] ] << endl; }
    });

    const vec<CRef>& table = data.getClauses();
    DOUT(if (config.opt_xor_debug > 3)  cerr << "c relevant clauses: " << table.size() << endl;);

    DOUT(if (config.opt_xor_debug > 3) cerr << "c XOR find start at position " << cP << " with " << cL << endl;);
    while (cP < table.size() && ca[ table[cP] ] .size() < cL) { cP ++; }

    DOUT(if (config.opt_xor_debug > 3) cerr << "c XOR continue at position" << cP << endl;);

    BIG big;
    if (config.opt_xor_findResolved) {
        big.create(ca, data.nVars(), data.getClauses()); // create big, so that resolve possibilities can be found
    }

    if (config.opt_xor_backdoor > 0) {
        xorBackdoor.clear();
        backdoorVariables.resize(data.nVars());
        backdoorVariables.nextStep();
    }

    int ignoredRatioClauses = 0;

    vector<char> foundByIndex; // no need to do this on the stack!
    // handle only xor between 3 and 63 literals!!
    while (cP <  table.size() && cL < config.opt_xorMatchLimit) {
        uint32_t start = cP;
        cL = ca[ table[cP] ] .size();
        while (cP < table.size() && ca[ table[cP] ] .size() == cL) {
            cP ++;
        }
        DOUT(if (config.opt_xor_debug > 3) cerr << "c XOR search size " << cL << " until " << cP << endl;);
        // check for each of the clauses, whether it and its successors could be an xor
        for (; start < cP - 1; ++start) {
            DOUT(if (config.opt_xor_debug > 3)  cerr << "c start=" << start << endl;);
            const CRef c = table[start];
            const Clause& cl = ca[c];
            if (cl.size() > config.opt_xorMatchLimit || cl.can_be_deleted()) { break; }  // interrupt before too large size is recognized, or if the first ignored clause pops up
            uint32_t stop = start + 1;
            // find last clause that contains the same variables as the first clause
            for (; stop < cP; ++stop) {
                const CRef c2 = table[stop];
                const Clause& cl2 = ca[c2];
                uint32_t k = 0;
                for (; k < cl2.size(); k ++) {
                    if (var(cl[k]) != var(cl2[k])) { break; }
                }
                if (k != cl2.size()) { break; }
            }

            DOUT(if (config.opt_xor_debug > 3) {
            cerr << "c [XOR] begin clauses with same variables and length:" << endl;
            for (uint32_t j = start; j < stop && j < cP; ++ j) {
                    cerr << ca[ table[j] ] << endl;
                }
                cerr << "c [XOR] end" << endl;
            });

            uint64_t shift = 1;
            shift = shift << (cl.size() - 1);

            DOUT(if (config.opt_xor_debug > 4) cerr << "c [XOR] candidate with size " << cl.size() << endl;);

            // check whether the first clause is odd or even, and go with this value first. Check the other value afterwards
            uint32_t count[2]; count[0] = 0; count[1] = 0;
            data.clss.clear(); // separate between the two clauses

            // check all clauses with the same variables
            for (uint32_t j = start ; j < stop; ++j) {
                const CRef c2 = table[j];
                const Clause& cl2 = ca[c2];
                if (cl2.can_be_deleted()) {  // jump over these clauses, but count them for the final ratio
                    ++ ignoredRatioClauses;
                    continue; // next clause
                }
                uint32_t o = 0;
                for (uint32_t k = 0; k < cl2.size(); k ++)
                    if (!sign(cl2[k])) { o = o ^ 1; }     // count literals that are set to true!
                if (o == 1) {
                    data.clss.push_back(table[j]);
                    data.clss[ data.clss.size() - 1 ] = data.clss[ count[0] ];
                    data.clss[count[0]] = table[j];
                    count[0] ++;
                } else {
                    count[1] ++;
                    data.clss.push_back(table[j]);
                }
            }
            DOUT(if (config.opt_xor_debug > 2) cerr << "c sorted according to polarity: o = 0: " << count[1] << " o=1: " << count [0] << endl;);
            DOUT(if (config.opt_xor_debug > 4) {
            cerr << "c o=1: " << endl;
            int k = 0;
            for (; k < count[0]; ++k) { cerr << "c " << ca[ data.clss[k] ] << endl; }
                cerr << "c o=0: " << endl;
                for (; k < data.clss.size(); ++k) { cerr << "c " << ca[ data.clss[k] ] << endl; }
            });

            DOUT(if (config.opt_xor_debug > 3)  cerr << "c collect XORs" << endl;);
            // try to recover odd and even XORs separately, if the limit can be reached
            for (uint32_t o = 0 ; o < 2; ++o) {
                uint32_t offset = o == 1 ? 0 : count[0];
                DOUT(if (config.opt_xor_debug > 3)  cerr << "c o=" << o << " offset=" << offset << " cL=" << cL << endl;);
                // check, whether there are clauses that are subsumed
                foundByIndex.assign(shift, 0);
                uint32_t foundCount = 0;
                // assign the clauses to the numbers they represent
                for (uint32_t j = 0 ; j < count[1 - o]; ++j) {
                    Clause& cl2 = ca[data.clss[j + offset]];
                    const uint32_t nrByPol = numberByPolarity(cl2);
                    const uint32_t index = nrByPol / 2;
                    DOUT(if (config.opt_xor_debug > 3) { cerr << "c found " << ca[data.clss[j + offset]] << " matchIndex=" << index << "(" << nrByPol << ") original number: " << numberByPolarity(cl2) << endl; });
                    foundCount = foundByIndex[ index ] == 0 ? foundCount + 1 : foundCount;
                    foundByIndex[ index ] = 1;
                    participatingXorClauses ++; // clause is covered
                    findChecks ++;
                }
                DOUT(if (config.opt_xor_debug > 4) cerr << "c final foundCount=" << foundCount << " for o=" << o << endl;);

                if (foundCount == 0) {
                    DOUT(if (config.opt_xor_debug > 2) cerr << "c reject this XOR, since it cannot produce an XOR (there is no large clause! o=" << o << ", found large clauses: " << foundCount << ")" << endl;);
                    continue;
                }

                // these variables are not used as XOR
                if (config.opt_xor_backdoor > 0 && foundCount < shift) {
                    DOUT(if (config.opt_xor_debug > 2) cerr << "c clause in backdoor: " << cl << endl;);
                    for (int j = 0 ; j < cl.size(); ++ j) {
                        if (! backdoorVariables.isCurrentStep(var(cl[j]))) {
                            xorBackdoor.push(var(cl[j]));
                            backdoorVariables.setCurrentStep(var(cl[j]));
                        }
                    }
                }

                // if( offset >= data.clss.size() ) continue;

                // try to find clauses, that subsume parts of the XOR
                if (foundCount < shift && config.opt_xor_findSubsumed) {
                    const Clause& firstClause = ca[ data.clss[offset] ];
                    assert(cL == firstClause.size() && "current clause needs to have the same size as the current candidate block");
                    Var variables [firstClause.size()]; // will be sorted!
                    data.ma.nextStep();
                    for (uint32_t j = 0 ; j < cL; ++j) {
                        variables[ j ] = var(firstClause[j]);
                        data.ma.setCurrentStep(variables[j]);
                    }
                    DOUT(if (config.opt_xor_debug > 3) {
                    cerr << "c [XOR] do expensive search (shift= " << shift << ")" << endl;
                    for (uint32_t match = 0 ; match < 2 * shift; ++match) {
                            if (foundByIndex[ match / 2 ] == 1) {
                                if (config.opt_xor_debug > 3) { cerr << "c match " << match / 2 << " is already covered!" << "(" << match << ") [not yet subsumption checked] " << endl; }
                                continue;
                            }
                        }
                    });

                    // check all smaller clauses (with limit!) whether they contain all the variables, if so, use them only, if the selected variable is the smallest one!
                    for (uint32_t j = 0 ; j < cL ; ++ j) {
                        Var cv = variables[j];
                        DOUT(if (config.opt_xor_debug > 3) cerr << "c [XOR] look for variable " << cv + 1 << endl;);
                        for (uint32_t p = 0 ; p < 2; ++ p) {   // polarity!
                            const Lit current = mkLit(cv, p != 0);   // 0 -> POS, 1 -> NEG
                            vector<CRef>& cls = data.list(current); // clause list of literal that is currently checked!
                            for (uint32_t k = 0 ; k < cls.size(); ++ k) {
                                const Clause& cclause = ca[cls[k]];
                                if (cls[k] == data.clss[offset]) { continue; }   // do not consider the same clause twice!
                                if (cclause.can_be_deleted()) { continue; }
                                if (cclause.size() > config.opt_xorMatchLimit                 // ignore too large clauses
                                        || cclause.size() == 1                                      // ignore unit clauses
                                        || (! config.opt_xor_findResolved && cclause.size() >= cL)
                                        || (config.opt_xor_findResolved && cclause.size() > cL)  // due to resolution, also bigger clauses are allowed!
                                   ) { continue; } // for performance, and it does not make sense to check clauses that are bigger than the current ones
                                DOUT(if (config.opt_xor_debug > 4) cerr << "c consider clause " << cclause << endl;);

                                Lit resolveLit = lit_Undef, foundLit = lit_Error; // if only one variable is wrong, the other literal could be resovled in?
                                for (uint32_t m = 0 ; m < cclause.size(); ++ m) {
                                    const Var ccv = var(cclause[m]);
                                    if (ccv < cv) {
                                        DOUT(if (config.opt_xor_debug > 4) cerr << "c reject " << cclause << " ,because currenct variable " << ccv + 1 << " is smaller than " << cv + 1 << endl;);
                                        resolveLit = lit_Error; break;
                                    } // reject clause, if the current variable is not the smallest variable in the matching
                                    if (! data.ma.isCurrentStep(ccv)) {
                                        if (resolveLit == lit_Undef && config.opt_xor_findResolved) {
                                            resolveLit = cclause[m]; // this literal is used for resolution
                                            // the literal that should be found to be resolved in has to be in the matching -> check all childs in BIG
                                            const int iSize = big.getSize(resolveLit);
                                            for (int q = 0; q < iSize; ++ q)  {
                                                if (data.ma.isCurrentStep(var(big.getArray(resolveLit)[q]))) {     // found variable that can be resolved in, and which is part of the XOR!
                                                    foundLit = big.getArray(resolveLit)[q];
                                                    for (int m = 0; m < cclause.size(); ++m) {   // check whether this literal already appears in the clause
                                                        if (cclause[m] == foundLit) {   // literal is in clause, everything's fine!
                                                            foundLit = lit_Undef; break;
                                                        } else if (cclause[m] == ~foundLit) {   // produce a tautology?
                                                            foundLit = lit_Error;
                                                            resTauts ++; // stats
                                                        }
                                                    }
                                                    if (foundLit != lit_Error) { break; }   // stop only, if the resolvent is not a tautology!
                                                }
                                            }
                                            if (foundLit == lit_Error) {   // not found a candidate!
                                                DOUT(if (config.opt_xor_debug > 4) cerr << "c did not find a literal to pull in for resolveLit " << resolveLit << endl;);
                                                resolveLit = lit_Error; break;
                                            }
                                            DOUT(if (config.opt_xor_debug > 3) cerr << "c found resolveLit " << resolveLit << ", which can pull " << foundLit << " into the clause!" << endl;);
                                        } else {
                                            DOUT(if (config.opt_xor_debug > 4) cerr << "c clause contains non-matching literal " << resolveLit << endl;);
                                            resolveLit = lit_Error;
                                            break;
                                        }
                                    }
                                }
                                if (resolveLit == lit_Error) { // there is a non-matching literal in the clause
                                    continue;
                                }   // next clause!
                                int subsumeSize = (resolveLit == lit_Undef || foundLit != lit_Undef) ? cclause.size() : cclause.size() - 1;
                                if (subsumeSize == firstClause.size()) {
                                    int o0 = 0;
                                    for (int m = 0 ; m < cclause.size(); ++ m) {
                                        const Lit ccl = cclause[m] != resolveLit ? cclause[m] : foundLit; // use the resolvedIn literal, instead of the literal that is resolved
                                        assert(ccl != lit_Undef && "neither a clause literal, nor foundLit can be undefined!") ;
                                        if (!sign(ccl)) { o0 = (o0 ^ 1); }
                                    }
                                    if (o0 != o) {
                                        DOUT(if (config.opt_xor_debug > 4) cerr << "c clause " << cclause << " has all variables, but the odd value is wrong!" << endl;);
                                        continue; // can consider only smaller clauses, or their odd value is the same as for for the first clause!
                                    }
                                }

                                resolvedInCls = resolveLit != lit_Undef ? resolvedInCls + 1 : resolvedInCls; // stats!
                                resStrength = (resolveLit != lit_Undef && foundLit == lit_Undef) ? resStrength + 1 : resStrength; // stats!

                                // generate all indexes it would subsume
                                uint32_t myNumber = 0;
                                uint32_t myMatch = 0;
                                bool subsumeOriginalClause = true; // if big clause is subsumed, working with this XOR is buggy!
                                for (uint32_t m = 0 ; m < cclause.size(); ++ m) {
                                    assert(cclause[m] != lit_Undef && "there should not be a lit_Undef inside a clause!");
                                    const Lit ccl = cclause[m] != resolveLit ? cclause[m] : foundLit; // use the resolvedIn literal, instead of the literal that is resolved
                                    DOUT(if (config.opt_xor_debug > 3) {
                                    if (ccl == lit_Undef) {
                                            cerr << "c continue, because pulled in lit must not be handled!" << endl;
                                        } else if (ccl == foundLit) {
                                            cerr << "c work on pulled in lit " << ccl << endl;
                                        }
                                    });
                                    if (ccl == lit_Undef) { continue; }   // nothing to do here (do not use same literal twice!)
                                    const Var ccv = var(ccl);
                                    bool hitVariable = false;
                                    for (uint32_t mv = 0 ; mv < cL; ++ mv) {
                                        if (variables[mv] == ccv) {
                                            hitVariable = true;
                                            if (ccl != firstClause[mv]) { subsumeOriginalClause = false; }
                                            DOUT(if (config.opt_xor_debug > 3) cerr << "c var " << ccv + 1 << " hits with literal " << ccl << " -> add " << (1 << (cL - mv - 1)) << endl;);
                                            myNumber = !sign(ccl) ? myNumber + (1 << (cL - mv - 1)) : myNumber;
                                            myMatch = myMatch + (1 << (cL - mv - 1));
                                            break;
                                        }
                                    }
                                    assert(hitVariable && "variable has to be in XOR!");
                                }
                                if (subsumeOriginalClause) {   // interrupt working on this XOR!
                                    // skip this xor? no, can still lead to improved reasoning!
                                }
                                DOUT(if (config.opt_xor_debug > 3) { cerr << "c found subsume clause " << ca[cls[k]] << " with number=" << myNumber << " match=" << myMatch << endl;});
                                assert(cclause.size() > 1 && "do not consider unit clauses here!");
                                DOUT(if (config.opt_xor_debug > 4) cerr << "c size of actual clause: " << subsumeSize << endl;);
                                assert(cL >= cclause.size() && "subsume clauses cannot be greater than the current XOR");
                                for (uint32_t match = 0 ; match < 2 * shift; ++match) {

                                    if (foundByIndex[ match / 2 ] == 1) {
                                        DOUT(if (config.opt_xor_debug > 3) cerr << "c match " << match / 2 << " is already covered!" << "(" << match << ") [not yet subsumption checked] " << endl;);
                                        participatingXorClauses ++; // this clause is covered with this XOR!
                                        continue;
                                    }

                                    if ((match & myMatch) == myNumber) {

                                        // reject matches that have the wrong polarity!
                                        uint32_t o1 = 0; // match has to be odd, as initial clause! and has to contain bits of myNumber!
                                        DOUT(if (config.opt_xor_debug > 4) cerr << "c initial odd: " << o1 << ", cL: " << cL << ", match: " << match << endl;);
                                        uint32_t matchBits = match;
                                        for (uint32_t km = 0; km < cL; km ++) {
                                            if ((matchBits & 1) == 1) {
                                                DOUT(if (config.opt_xor_debug > 4) cerr << "c bit " << km << " match, this odd=" << (int)(o1 ^ 1) << endl;);
                                                o1 = (o1 ^ 1);
                                            } else {
                                                DOUT(if (config.opt_xor_debug > 4) cerr << "c bit " << km << " does not match, this odd=" << (int)(o1 ^ 1) << endl;);
                                            }
                                            matchBits = matchBits >> 1;
                                        }
                                        // wrong polarity -> reject
                                        if (o1 != o) {
                                            DOUT(if (config.opt_xor_debug > 4) cerr << "c current odd " << o1 << " does not match XOR odd " << o << endl;);
                                            continue;
                                        }

                                        DOUT(if (config.opt_xor_debug > 3) cerr << "c current clause subsumes match=" << match / 2 << "(" << match << ") - match&myMatch=" << (int)(match & myMatch) << " vs " << myNumber << " with odd=" << o1 << endl;);
                                        if (foundByIndex[ match / 2 ] == 0) { foundCount++; }   // only if this clause sets the value from 0 to 1!
                                        subsFound = resolveLit == lit_Undef ? subsFound + 1 : subsFound; // stats
                                        resFound = resolveLit != lit_Undef ? resFound + 1 : resFound;
                                        foundByIndex[ match / 2 ] = 1;
                                        participatingXorClauses ++; // this clause is covered with this XOR!
                                        DOUT(if (config.opt_xor_debug == 0 && foundCount == shift) goto XorFoundAll;);
                                    }
                                }
                                if (foundCount == shift) { goto XorFoundAll; }
                            }

                        }
                    }
                }
            XorFoundAll:;
                DOUT(if (config.opt_xor_debug > 3) cerr << "c foundCount " << foundCount << " vs shift= " << shift << endl;);
                DOUT(if (config.opt_xor_debug > 4) {   // check whether really all cases of the xor have been found!
                for (uint32_t match = 0 ; match < 2 * shift; ++match) {
                        cerr  << "c [" << match / 2 << "] == " << (int) foundByIndex[ match / 2 ] << endl;
                    }
                });
                // if the whole XOR is entailed by the formula, add the XOR to the recognized XORs
                if (foundCount == shift) {
                    xorList.push_back(GaussXor(ca[data.clss[offset]]));
                    xors ++;
                    DOUT(if (config.opt_xor_debug > 3) cerr << "c found " << xors << " XOR" << endl;);
                    DOUT(if (config.opt_xor_debug > 0) cerr << "c found XOR " << ca[data.clss[offset]] << endl;);
                    didSomething = true;
                }
            }
            start = stop - 1;
        }
        // continue with clauses of a larger size!
        cL ++;
    }

    // calculate ratio
    clauseRatio = data.getClauses().size() - ignoredRatioClauses == 0 ? 1 : (double)participatingXorClauses / (double)(data.getClauses().size() - ignoredRatioClauses);
    variableRatio = data.nVars() == 0 ? 1 : (double)backdoorVariables.size() / (double)data.nVars();
    DOUT(if (config.opt_xor_debug > 0) cerr << "c clauseRatio: " << clauseRatio << " variableRatio: " << variableRatio << endl;);

    DOUT(if (config.opt_xor_debug > 0) cerr << "c [XOR] found " << xors << " non-binary xors encoded with " << xorClauses << " clauses" << endl;);
    return didSomething;
}

void XorReasoning::checkReaddedSubsumption()
{

}

void XorReasoning::addXorAsClauses(XorReasoning::GaussXor& simpX)
{
    if (simpX.size() == 3) {
        Lit lits[3]; // k == 0: a + b + c == 0 -> clause: a \land b \to \neg c ^=  ( \neg a \lor \neg b \lor \neg c )
        lits[0] = mkLit(simpX.vars[0], true);
        lits[1] = mkLit(simpX.vars[1], true);
        lits[2] = mkLit(simpX.vars[2], simpX.k == 0);  // if k == 1, c has to be positive

        addClause(lits, 3, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 1 1 1

        lits[0] = ~lits[0]; lits[1] = ~lits[1];
        addClause(lits, 3, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 0 0 1

        lits[1] = ~lits[1]; lits[2] = ~lits[2];
        addClause(lits, 3, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 0 1 0

        lits[0] = ~lits[0]; lits[1] = ~lits[1];
        addClause(lits, 3, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 1 0 0

        addedTernaryXors ++;

    } else if (simpX.size() == 4) {

        Lit lits[4]; // k == 0: a + b + c + d == 0 -> clause: a \land b \land c \to d ^=  ( \neg a \lor \neg b \lor \neg c \lor d )
        lits[0] = mkLit(simpX.vars[0], true);
        lits[1] = mkLit(simpX.vars[1], true);
        lits[2] = mkLit(simpX.vars[2], true);
        lits[3] = mkLit(simpX.vars[3], simpX.k == 1);  // if k == 0, d has to be positive

        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 1 1 1 0
        lits[2] = ~lits[2]; lits[3] = ~lits[3];
        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 1 1 0 1
        lits[1] = ~lits[1]; lits[2] = ~lits[2];
        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 1 0 1 1
        lits[2] = ~lits[2]; lits[3] = ~lits[3];
        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 1 0 0 0
        lits[0] = ~lits[0]; lits[3] = ~lits[3];
        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 0 0 0 1
        lits[2] = ~lits[2]; lits[3] = ~lits[3];
        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 0 0 1 0
        lits[1] = ~lits[1]; lits[3] = ~lits[3];
        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 0 1 0 1
        lits[2] = ~lits[2]; lits[3] = ~lits[3];
        addClause(lits, 4, config.opt_xor_addAsLearnt);   // adds negative (for k==0): 0 1 1 0

        addedQuadraryXors ++;

    } else {
#warning implement generic XOR encode routine, might allow to use fresh variables!
    }


}

CRef XorReasoning::addClause(const Lit* lits, int size, bool learnt)
{
    CRef cr = ca.alloc(lits, size, learnt); // allocate clause
    DOUT(if (config.opt_xor_debug > 4) cerr << "c added clause by reencoding: [" << cr << "]: " << ca[cr] << endl;);    // print created clause
    data.addClause(cr);               // add to coprocessor data structures
    if (learnt) {                     // add to corresponding clause list
        data.getLEarnts().push(cr);
    } else {
        data.getClauses().push(cr);
    }

    if (config.opt_xor_checkNewSubsume) { reAddedClauses.push(cr); }
    return cr;
}



void XorReasoning::printStatistics(ostream& stream)
{
    stream << "c [STAT] XOR " << processTime << "s, "
           << parseTime << " parse-s, "
           << reasonTime << " reason-s, " << endl
           << "c [STAT] XOR(2) "
           << xors << " xors, "
           << subsFound << " subXors, "
           << resFound << " resXors, "
           << xorClauses << " clauses, "
           << resolvedInCls << " resClauses, "
           << resTauts << " resTauts, "
           << resStrength << " resStrength, "
           << findChecks << " findChecks, " << endl
           << "c [STAT] XOR(3) "
           << xorSteps << " steps, "
           << foundEmptyLists << " empties, "
           << xorUnits  << " listUnits, "
           << allUsed << " useLists, "
           << xorDeducedUnits << " xorUnits, "
           << eqs << " eqs, "
           << endl
           << "c [STAT] XOR(4) " << participatingXorClauses << " participating, "
           << clauseRatio << " ratio, "
           << simDecisions << " simDecisions, "
           << xorProps << " xorProps, "
           << clsProps << " clsProps, "
           << endl
           ;
}

void XorReasoning::giveMoreSteps()
{

}

void XorReasoning::destroy()
{
    backdoorVariables.destroy();
}

} // namespace Coprocessor
