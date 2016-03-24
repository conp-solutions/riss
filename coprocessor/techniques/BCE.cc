/******************************************************************************************[BCE.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "BCE.h"

using namespace Riss;
using namespace std;

namespace Coprocessor
{

BlockedClauseElimination::BlockedClauseElimination(CP3Config& _config, ClauseAllocator& _ca,
        ThreadController& _controller, CoprocessorData& _data,
        Coprocessor::Propagation& _propagation)
    : Technique(_config, _ca, _controller), data(_data), propagation(_propagation), bceSteps(0), testedLits(0),
      cleCandidates(0), remBCE(0), remCLE(0), cleUnits(0), bcm_cls(0), bcm_cls_cands(0), bcm_lits(0),
      claTestedLits(0), claSteps(0), claExtendedClauses(0), claExtensions(0), possibleClaExtensions(0)
{

}


void BlockedClauseElimination::reset()
{

}

void BlockedClauseElimination::coverdLiteralAddition()
{
    MethodClock mc(claTime);
    LitOrderBCEHeapLt comp(data, config.orderComplements); // use this sort criteria!
    Heap<LitOrderBCEHeapLt> bceHeap(
        comp);  // heap that stores the variables according to their frequency (dedicated for BCE)

    // setup own structures
    bceHeap.addNewElement(data.nVars() * 2); // set up storage, does not add the element
    bceHeap.clear();

    DOUT(if (config.opt_bce_debug) {
    cerr << "c before CLA formula" << endl;
    for (int i = 0; i < data.getClauses().size(); ++i) {
            cerr << "(" << i << ") (" << data.getClauses()[i] << ")";
            if (ca[data.getClauses()[i]].mark() != 0) { cerr << " (ign) "; }
            if (ca[data.getClauses()[i]].can_be_deleted() != 0) { cerr << " (del) "; }
            cerr << " " << ca[data.getClauses()[i]] << endl;
        }
    });

    // init
    for (Var v = 0; v < data.nVars(); ++v) {
        if (data.doNotTouch(v)) { continue; } // do not consider variables that have to stay fixed!
        if (data[mkLit(v, false)] > 0) if (!bceHeap.inHeap(toInt(mkLit(v, false)))) {
                bceHeap.insert(toInt(mkLit(v, false)));
            }
        if (data[mkLit(v, true)] > 0) if (!bceHeap.inHeap(toInt(mkLit(v, true)))) {
                bceHeap.insert(toInt(mkLit(v, true)));
            }
    }
    data.ma.resize(2 * data.nVars());
    data.ma.nextStep();

    // vector<ClaStore> claStorage; // tmp. storage for CLA clauses // odd is old reference, even is new reference

    // do BCE on all the literals of the heap
    while (bceHeap.size() > 0 && (data.unlimited() || config.bceLimit > bceSteps) && !data.isInterupted()) {
        // interupted ?
        if (data.isInterupted()) { break; }

        const Lit right = toLit(bceHeap[0]);
        assert(bceHeap.inHeap(toInt(right)) && "item from the heap has to be on the heap");
        bceHeap.removeMin();

        if (data.doNotTouch(var(right))) { continue; } // do not consider variables that have to stay fixed!

        claTestedLits++; // count number of literals that have been tested for BCE
        // check whether a clause is a tautology wrt. the other clauses
        const Lit left = ~right; // complement
        DOUT(if (config.opt_bce_debug)
             cerr << "c CLA work on literal " << right << " with " << data.list(right).size() << " clauses " <<
             endl;);
        data.lits.clear(); // used for covered literal elimination
        const int listSize = data.list(right).size(); // do not process the newly generated clause here as well!
        for (int i = 0; i < listSize; ++i) {
            Clause& c = ca[data.list(right)[i]];
            if (c.can_be_deleted()) { continue; } // do not work on uninteresting clauses!

            int rightOcc = data.list(right).size();
            bool isLeastFrequent = true;
            for (int k = 0; k < c.size(); ++k) {
                if (data.list(c[k]).size() < rightOcc) {
                    isLeastFrequent = false;
                    break;
                }
            }
            if (!isLeastFrequent) { continue; } // do not work with this clause on that literal, because its not among the least frequent literals!

            data.lits.clear(); // collect the literals that could be added by CLA

            bool canCla = false; // yet, no resolvent has been produced -> cannot perform CLA
            for (int j = 0; j < data.list(left).size(); ++j) {
                Clause& d = ca[data.list(left)[j]];
                if (d.can_be_deleted()) { continue; } // do not work on uninteresting clauses!
                claSteps++;
                const Lit tautLit = tautologicResolvent(c, d, right);
                if (tautLit == lit_Undef) { // the resolvent is not a tautology
                    if (!canCla) { // simply copy all literals from d except right into data.lits
                        for (int k = 0; k < d.size(); ++k) {
                            if (d[k] != left) { data.lits.push_back(d[k]); }
                        }
                        canCla = true; // remember that we added some literals
                    } else {
                        // build intersection of data.lits and d
                        data.ma.nextStep();
                        for (int k = 0; k < d.size(); ++k) {
                            data.ma.setCurrentStep(toInt(d[k]));    // mark all literals of this clause
                        }
                        // keep literals, that occurred before, and in this clause d
                        int keptCle = 0;
                        for (int k = 0; k < data.lits.size(); ++k) {
                            if (data.ma.isCurrentStep(toInt(data.lits[k]))) {
                                data.lits[keptCle++] = data.lits[k];
                            }
                        }
                        data.lits.resize(keptCle); // remove all the other literals
                        // if intersection is empty, drop the clause!
                        if (data.lits.size() == 0) { break; }
                    }
                } else {
                    // tautologic resolvent, nothing special here!
                }
            }

            if (data.lits.size() > 0 && canCla) { // there is something to be added for the clause c!
                // add all literals of c to data.lits, sort, add as clause
                data.ma.nextStep();

                const int oldPossibleClaExtensions = possibleClaExtensions;
                possibleClaExtensions += data.lits.size();

                // have a filter here that removes some of the literals, if data.lits is too large!
		// removing arbitrary literals might be unsound, as the order matters?
                if (false && data.lits.size() > config.claStepSize) { // reduce number of literals somehow
                    int keptLiterals = 0;
                    for (int k = 0; k < data.lits.size(); k++) {
                        if (rand() % 1000 < 600) { // keep some 60 %
                            data.lits[keptLiterals++] = data.lits[k];
                        }
                    }
                    if (keptLiterals > config.claStepMax) {
                        keptLiterals = config.claStepMax;    // cut off the remaining literals
                    }
                    if (keptLiterals == 0) { // have at least one literal!
                        data.lits[0] = data.lits[rand() % data.lits.size()]; // select one randomly!
                        keptLiterals = 1;
                    }
                    data.lits.resize(keptLiterals);
                }


                for (int k = 0; k < data.lits.size(); k++) { data.ma.setCurrentStep(toInt(data.lits[k])); }
                const int oldClaExtensions = claExtensions;
                claExtensions += data.lits.size(); // size of extension
                bool isTaut = false;
                for (int k = 0; k < c.size(); k++) {
                    if (data.ma.isCurrentStep(toInt(~c[k]))) {
                        isTaut = true;
                        data.lits.push_back(c[k]);
			break;
                    } else if (!data.ma.isCurrentStep(toInt(c[k]))) {
                        data.lits.push_back(c[k]);
                    }
                }

                if (!isTaut) { // do not want to perform CCE here!
                    claExtendedClauses++;
		    assert( data.lits.size() >= c.size() && "we should have added literals!" );
                    CRef newClause = ca.alloc(data.lits, false); // destroys reference for clause c!
                    ca[newClause].sort();
                    //claStorage.push_back( ClaStore(data.list(right)[i], newClause, right ) );

		    DOUT( if( config.opt_bce_debug ) cerr << "c add clause " << data.lits << " for clause " << ca[data.list(right)[i]] << endl; );
                    // add new clause to proof (subsumed by the other, so should be fine!)
		    assert( !ca[data.list(right)[i]].can_be_deleted() && "clause should still be active!" );
		    
                    ca[data.list(right)[i]].set_delete(true);
                    data.addCommentToProof("extended by applying CLA");
                    data.addToProof(ca[newClause]); // add the new longer clause!
                    data.addToProof(ca[data.list(right)[i]], true); // delete this clause from the proof!
                    // add old clause to extension stack
                    data.addToExtension(data.list(right)[i], right);
                    // remove old clause, since it should not subsume the other clause!
                    DOUT(if (config.opt_bce_debug)
                         cerr << "c CLA turned clause " << ca[data.list(right)[i]] << " into the new clause " <<
                         ca[newClause] << endl;);
                    // add new clause
                    data.addClause(newClause);
                    data.getClauses().push(newClause);
                } else {
                    claExtensions = oldClaExtensions; // undo calculation
                    possibleClaExtensions = oldPossibleClaExtensions;
                    // CCE could be applied!
                }
            }
        }

        // perform garbage collection
        data.checkGarbage();
    }

    DOUT(if (config.opt_bce_debug) {
    cerr << "c after CLA formula" << endl;
    for (int i = 0; i < data.getClauses().size(); ++i) {
            cerr << "(" << i << ") (" << data.getClauses()[i] << ")";
            if (ca[data.getClauses()[i]].mark() != 0) { cerr << " (ign) "; }
            if (ca[data.getClauses()[i]].can_be_deleted() != 0) { cerr << " (del) "; }
            cerr << " " << ca[data.getClauses()[i]] << endl;
        }
    });

}

void BlockedClauseElimination::blockedClauseElimination()
{


    LitOrderBCEHeapLt comp(data, config.orderComplements); // use this sort criteria!
    Heap<LitOrderBCEHeapLt> bceHeap(
        comp);  // heap that stores the variables according to their frequency (dedicated for BCE)

    // setup own structures
    bceHeap.addNewElement(data.nVars() * 2); // set up storage, does not add the element
    bceHeap.clear();

    // structures to have inner and outer round
    MarkArray nextRound;
    vector<Lit> nextRoundLits;
    nextRound.create(2 * data.nVars());

    MarkArray tautologyReasonLiterals;
    if (config.opt_bce_bcm) {                        // perform blocked clause minimization instead of elimination
        tautologyReasonLiterals.create(2 * data.nVars()); // collect the literals that produce the blocked clause
    }

    // init
    for (Var v = 0; v < data.nVars(); ++v) {
        if (data.doNotTouch(v)) { continue; } // do not consider variables that have to stay fixed!
        if (data[mkLit(v, false)] > 0) if (!bceHeap.inHeap(toInt(mkLit(v, false)))) {
                nextRoundLits.push_back(mkLit(v, false));
            }
        if (data[mkLit(v, true)] > 0) if (!bceHeap.inHeap(toInt(mkLit(v, true)))) {
                nextRoundLits.push_back(mkLit(v, true));
            }
    }
    data.ma.resize(2 * data.nVars());
    data.ma.nextStep();

    do {

        // re-init heap
        for (int i = 0; i < nextRoundLits.size(); ++i) {
            const Lit l = nextRoundLits[i];
            if (!nextRound.isCurrentStep(toInt(l))) { continue; } // has been processed before already
            assert(!bceHeap.inHeap(toInt(l)) && "literal should not be in the heap already!");
            bceHeap.insert(toInt(l));
        }
        nextRoundLits.clear();
        nextRound.nextStep();


        // do BCE on all the literals of the heap
        while (bceHeap.size() > 0 && (data.unlimited() || config.bceLimit > bceSteps) && !data.isInterupted()) {
            // interupted ?
            if (data.isInterupted()) { break; }

            const Lit right = toLit(bceHeap[0]);
            assert(bceHeap.inHeap(toInt(right)) && "item from the heap has to be on the heap");
            bceHeap.removeMin();

            if (data.doNotTouch(var(right)) || data.value( right ) != l_Undef ) {
                continue;    // do not consider variables that have to stay fixed! // TODO: might be put into re-init loop for heap
            }

            testedLits++; // count number of literals that have been tested for BCE
            // check whether a clause is a tautology wrt. the other clauses
            const Lit left = ~right; // complement
            bool learntsInLeft = false;
            DOUT(if (config.opt_bce_debug)
                 cerr << "c BCE work on literal " << right << " with " << data.list(right).size() <<
                 " clauses " << endl;);
            data.lits.clear(); // used for covered literal elimination
            for (int i = 0; i < data.list(right).size(); ++i) {
		if( data.hasToPropagate() ) break; // we found a unit, stop working on BCE for now
		  Clause& c = ca[data.list(right)[i]];
                if (c.can_be_deleted() || (!config.bceBinary && c.size() == 2 && !config.opt_bce_cle)) {
                    continue;    // do not work on uninteresting clauses!
                }
                
                

                if (config.opt_bce_cle) {
                    data.lits.clear(); // collect the literals that could be removed by CLE
                    for (int k = 0; k < c.size(); ++k) if (right != c[k]) { data.lits.push_back(c[k]); }
                }

                bool canCle = false; // yet, no resolvent has been produced -> cannot perform CLE
                bool isBlocked = (c.size() > 2 ||
                                  config.bceBinary); // yet, no resolvent has been produced -> has to be blocked

                if (!isBlocked && !config.opt_bce_cle) { break; } // early abort

                // its a new clause, hence the array has to be renewed
                if (config.opt_bce_bcm) { tautologyReasonLiterals.nextStep(); }

                DOUT(if (config.opt_bce_debug) cerr << "c" << endl << "c BCE/CLE analyze clause " << c << endl;);

                for (int j = 0; j < data.list(left).size(); ++j) {
		  
		    if( data.hasToPropagate() ) break; // we found a unit, stop working on BCE for now
		  
                    Clause& d = ca[data.list(left)[j]];
                    if (d.can_be_deleted()) { 
		      DOUT(if (config.opt_bce_debug) cerr << "c BCE/CLE do not compare with clause " << d << " as it is already deleted" << endl;);
		      continue;
		    } // do not work on uninteresting clauses!
                    bceSteps++;
		    
		    learntsInLeft = learntsInLeft || d.learnt();
		    
                    DOUT(if (config.opt_bce_debug) cerr << "c BCE/CLE compare with clause " << d << " (" << j << "/" << data.list(left).size() << ")" << endl;);
                    const Lit tautLit = tautologicResolvent(c, d,
                                                            right);        // check whether there is a literal that produces a tautologic resolvent
		    DOUT(if (config.opt_bce_debug) cerr << "c              returned with tautlit = " << tautLit << endl;);
                    if (tautLit == lit_Undef) {                                 // the resolvent is not a tautology
                        isBlocked = false;                                         // from here on, the given clause is not a blocked clause since a non-tautologic resolvent has been produced
                        DOUT(if (config.opt_bce_debug) cerr << "c    clause is not BCE, CLE lits: " << data.lits.size() << " , canCLE: " << canCle << endl;);
                        if (!config.opt_bce_cle || data.lits.size() == 0) {
			    DOUT(if (config.opt_bce_debug) cerr << "c    stop as we are not BCE, and there are no CLE literals left" << endl;);
                            break;    // however, for cle further checks have to be/can be done!
                        }

                        if (config.opt_bce_cle) { // compute CLE of the clause
                            // build intersection of all non-tautologic resolvents
                            canCle = true; // there was some clause that could be used for resolution
                            data.ma.nextStep();
                            for (int k = 0; k < d.size(); ++k) {
                                data.ma.setCurrentStep(toInt(d[k]));    // mark all literals of this clause
                            }
                            // keep literals, that occurred before, and in this clause
                            int keptCle = 0;
                            for (int k = 0; k < data.lits.size(); ++k) {
                                if (data.ma.isCurrentStep(toInt(data.lits[k]))) {
                                    data.lits[keptCle++] = data.lits[k];
                                }
                            }
                            data.lits.resize(keptCle); // remove all the other literals
                            DOUT(if (config.opt_bce_debug)
                                 cerr << "c resolving " << c << " with " << d << " keeps the CLE literals " <<
                                 data.lits << endl;);
                            if (keptCle == 0) { break; } // can stop here, because no CLE, and not blocked!
                        }
                    } else {
                        // compute cle?
                        if (config.opt_bce_cle) {
			    DOUT(if (config.opt_bce_debug) cerr << "c      handle tautologic resolvent during CLE with " << data.lits << " CLE lits left " << endl;);
                            if (data.lits.size() > 0) { // we have found a tautologic resolvent, take care of the set of literals for CLE!
                                // very conservative, but cheap:
                                if (config.opt_bce_cle_conservative) {
				    DOUT(if (config.opt_bce_debug) cerr << "c      conservative, abort CLE" << endl;);
                                    data.lits.clear(); // ensure that no CLE is performed!
                                } else {
                                    // less conservative, but more expensive data.lits = data.lits \ \ngt{D}
                                    int k = 0, l = 0, keptLiterals = 0;
                                    while (k < data.lits.size() && l <
                                            d.size()) { // since both data.lits and d are sorted, intersection can be calculated like this
                                        if (data.lits[k] ==
                                                ~d[l]) { // remove literal, when negation is present in clause d!
                                            k++;
                                            l++;
                                        } else if (data.lits[k] == d[l]) {
                                            data.lits[keptLiterals++] = data.lits[k++]; // increase pointer, keep literal!
                                            l++;
                                        } else if (data.lits[k] < d[l]) {
                                            data.lits[keptLiterals++] = data.lits[k++]; // increase pointer, keep literal!
                                        } else { // only one more case possible: d[l] < data.lits[k]
                                            l++;
                                        }
                                    }
                                    for (; k < data.lits.size();) {
                                        data.lits[keptLiterals++] = data.lits[k++];    // increase pointer, keep literal!
                                    }
                                    data.lits.resize(keptLiterals); // remove the other literals
				    DOUT(if (config.opt_bce_debug) cerr << "c      non-conservative, keep literals " << data.lits << endl;);
                                }
                            }
                        }

                        // compute blocked clause minimization
                        if (config.opt_bce_bcm && ! d.learnt() ) {
                            tautologyReasonLiterals.setCurrentStep(toInt(
                                    tautLit)); // store that this literal is necessary for the clause to be blocked
                        }
                    }
                    DOUT(if (config.opt_bce_debug) cerr << "c BCE/CLE finished iteration with clause " << d << " (" << j << "/" << data.list(left).size() << ")" << endl;);
                }
                DOUT(if (config.opt_bce_debug)
                     cerr << "c resolved " << c << " with about " << data.list(left).size() <<
                     " clauses, blocked: " << isBlocked << endl;);
                if (config.opt_bce_bce && isBlocked) {  // the clause c is blocked wrt. the current formula
                    if (!config.opt_bce_bcm) {            // perform usual BCE
                        // add the clause to the stack
                        c.set_delete(true);
                        data.addCommentToProof("blocked clause during BCE");
                        data.addToProof(c, true);
                        data.removedClause(data.list(right)[i]);
                        remBCE++; // stats
                        for (int k = 0; k < c.size(); ++k) {
                            if (bceHeap.inHeap(toInt(~c[k]))) {
                                bceHeap.update(toInt(~c[k]));    // update entries in heap
                            } else {
                                // if the complementary literal is not yet in the set for the next iteration, add it
                                if (!nextRound.isCurrentStep(toInt(~c[k]))) {
                                    nextRoundLits.push_back(~c[k]);
                                    nextRound.setCurrentStep(toInt(~c[k]));
                                }
                            }
                        }
                        successfulSimplification();
                        if (!c.learnt()) {
                            DOUT( if (config.opt_bce_verbose > 2) cerr << "c remove with blocking literal " << right << " blocked clause " << ca[data.list(right)[i]] << endl; );
                            data.addToExtension(data.list(right)[i],
                                                right); // to reconstruct the actual model of the formula, add the clause and literal to the repair stack
                        }
                    } else if (c.size() >
                               2) { // perform blocked clause minimization (BCM) (remove all literals that do not contribute to being blocked). is the same as adding a blocked clause and performing subsumption
                        // does not work on binary clauses, hence skip binary clauses immediately
                        bool deletedSomeLiteral = false;  // yet, no literal got removed

                        bcm_cls_cands++; // count the candidates
                        // check the clause whether it contains literals that could be eliminated
                        int keptLiterals = 0;
                        for (int k = 0; k < c.size(); ++k) {
                            if (c[k] == right || (deletedSomeLiteral && data.outputsProof() )) { // make sure we only remove one literal in a step ... TODO teach proof printing how to handle multi-BCM clauses
                                c[keptLiterals++] = c[k];  // keep the literal
                                continue;                  // the blocking literal needs to remain in the clause
                            }
                            if (!tautologyReasonLiterals.isCurrentStep( toInt(c[k]))) {  // found a literal that can be removed
                                if (!deletedSomeLiteral) { // tell proof about removed blocked clause (the other clause will be added below, after minimization)
                                    data.addCommentToProof("blocked clause during BCM");
                                    data.addToProof(c, true);  // delete the original clause - the minimized clause can still be added by blocked clause addition
				    // collect literals to delete clause from proof afterwards
				    data.lits.clear(); // collect the literals that could be removed by CLE
				    for (int k = 0; k < c.size(); ++k) if (right != c[k]) { data.lits.push_back(c[k]); }
                                }
                                deletedSomeLiteral = true;                          // indicate that the clause has been modified
                                data.removeClauseFrom(data.list(right)[i],
                                                      c[k]); // remove the clause from the corresponding list
                                data.removedLiteral(
                                    c[k]);                          // update counters for the literal

                                // as usual, use the literal (with the updated counter) for the next iteration
                                if (bceHeap.inHeap(toInt(~c[k]))) {
                                    bceHeap.update(toInt(~c[k]));    // update entries in heap
                                } else {
                                    // if the complementary literal is not yet in the set for the next iteration, add it
                                    if (!nextRound.isCurrentStep(toInt(~c[k]))) {
                                        nextRoundLits.push_back(~c[k]);
                                        nextRound.setCurrentStep(toInt(~c[k]));
                                    }
                                }
                            } else {
                                c[keptLiterals++] = c[k];  // keep the literal, do not update counters
                            }
                        }

                        // there are unnecessary literals
                        if (deletedSomeLiteral) {
                            successfulSimplification();                                                     // tell penalty system about modification
                            bcm_cls++;
                            bcm_lits += (c.size() - keptLiterals);             // counters
                            assert(keptLiterals < c.size() &&
                                   "some literal was removed"); // something has to be removed
			    const int oldSize = c.size();
                            c.shrink(c.size() - keptLiterals);                             // remove the literals
                            DOUT(if (config.opt_bce_debug) cerr << "c create clause " << c << " during BCM with old size: " << oldSize << endl;);
                            data.addCommentToProof( "found a blocked clause during BCM");     // tell the proof about the clause
                            data.addToProof(c, false, right);                                // add the clause to the proof (its a RAT clause)

                            if (c.size() == 1) {
                                if (l_False == data.enqueue(c[0])) {
                                    return;
                                }
                                c.set_delete(true);
                            }
                        }
                    }

                } else { // the clause is not blocked, maybe it an be shrinked by CLE?

                    if (config.opt_bce_cle && canCle && data.lits.size() > 0) {
                        // cle can actually be performed:
                        successfulSimplification();
                        int k = 0, l = 0; // data.lits is a sub set of c, both c and data.lits are ordered!
                        int keptLiterals = 0;
			bool modifiedClause = false;
                        DOUT(if (config.opt_bce_debug) cerr << "c cle turns clause " << c << endl;);
                        if (data.outputsProof()) { // store the original clause for deleting it from the proof
                            data.getSolver()->oc.clear();
                            for (int m = 0; m < c.size(); ++m) { data.getSolver()->oc.push(c[m]); }
                            // only remove a single literal, as DRAT does not support multiple literals ... // TODO do a step by step procedure in future for DRAT!
                            data.lits.resize( 1 );
                        }
                        remCLE += data.lits.size();
                        while (k < c.size() && l < data.lits.size()) {
                            if (c[k] == data.lits[l]) {
                                // remove the literal from the clause and remove the clause from that literals structures, as well as decrease the occurrence counter
                                data.removedLiteral(c[k]);
                                data.removeClauseFrom(data.list(right)[i],
                                                      c[k]); // remove the clause from the list of c[k]
                                k++;
                                l++;
                            } else if (c[k] < data.lits[l]) {
                                c[keptLiterals++] = c[k]; // keep this literal, since its not removed!
                                k++;
                            } else { l++; } // if ( data.lits[l] < c[k] ) // the only left possibility!
                        }
                        for (; k < c.size(); ++k) { c[keptLiterals++] = c[k]; } // keep the remaining literals as well!
                        assert((keptLiterals + data.lits.size() == c.size()) &&
                               "the size of the clause should not shrink without a reason");
			if( c.size() != data.lits.size() ) modifiedClause = true;
                        c.shrink(data.lits.size()); // remvoe the other literals from this clause!
                        data.addCommentToProof("apply CLE to a clause"); // TODO also delete the original clause!
                        data.addToProof(c, false,
                                        right); // add the new clause after c[k] has been removed - has been resolved on literal "right", hence, do write that literal to the first position!
                        data.addToProof(data.getSolver()->oc, true); // remove the clause of the shape it had before
                        DOUT(if (config.opt_bce_debug)
                             cerr << "c into clause " << c << " by removing " << data.lits.size() <<
                             " literals, namely: " << data.lits << endl;);
                        if (c.size() == 1) {
                            cleUnits++;
                            if (l_False == data.enqueue(c[0])) {
                                return;
                            }
                            c.set_delete(true);
                        }
                        for (l = 0; l < data.lits.size(); ++l) {
                            if (bceHeap.inHeap(toInt(~data.lits[l])))
                                bceHeap.update(
                                    toInt(~data.lits[l])); // there is the chance for ~right to become blocked!
                            else {
                                if (!nextRound.isCurrentStep(toInt(~data.lits[l]))) {
                                    nextRoundLits.push_back(~data.lits[l]);
                                    nextRound.setCurrentStep(toInt(~data.lits[l]));
                                }
                            }
                        }
                        
                        if( !c.learnt() && learntsInLeft && modifiedClause ) { // turn all used learned clauses into original clauses
			  for (int j = 0; j < data.list(left).size(); ++j) {
			      Clause& d = ca[data.list(left)[j]];
			      if (d.can_be_deleted()) continue;
			      if( d.learnt() ) d.set_learnt( false ); // turn into original clause
			  }
			}
                    }
                }

                // if cle took place, there might be something to be propagated
                if (data.hasToPropagate()) {
                    int prevSize = data.list(right).size();
                    propagation.process(data, true); // propagate, if there's something to be propagated
                    modifiedFormula = modifiedFormula || propagation.appliedSomething();
                    if (!data.ok()) { return; }
                    if (prevSize != data.list(right).size()) {
                        i = -1;    // start the current list over, if propagation did something to the list size (do not check all clauses of the list!)!
                    }
                }

            } // end iterating over all clauses that contain (right)


            // in case we aborted due to a unit clause, take care of that before processing more clauses!
            if( data.hasToPropagate() ) {
	      propagation.process(data, true); // propagate, if there's something to be propagated
	      modifiedFormula = modifiedFormula || propagation.appliedSomething();
	      if (!data.ok()) { return ; }
	    }
            
        } // end iterating over all literals of the heap

        // perform garbage collection
        data.checkGarbage();

    } while (nextRoundLits.size() > 0 && (data.unlimited() || config.bceLimit > bceSteps) &&
             !data.isInterupted()); // repeat until all literals have been seen until a fixed point is reached!
}


bool BlockedClauseElimination::process()
{
    if (!config.opt_bce_bce && !config.opt_bce_cle && !config.opt_bce_cla) {
        return false;    // return that nothing has been done
    }

    MethodClock mc(bceTime);

    if (!performSimplification()) { return false; } // do not do anything?!
    modifiedFormula = false;

    // do not simplify, if the formula is considered to be too large!
    if (!data.unlimited() && (data.nVars() > config.opt_bce_vars &&
                              data.getClauses().size() + data.getLEarnts().size() > config.opt_bce_cls &&
                              data.nTotLits() > config.opt_bce_lits)) {
        return false;
    }

    // run UP first!
    DOUT(if (config.opt_bce_debug) cerr << "c BCE run unit propagation" << endl;);
    propagation.process(data, true); // propagate, if there's something to be propagated
    modifiedFormula = modifiedFormula || propagation.appliedSomething();
    if (!data.ok()) { return modifiedFormula; }

    if (config.opt_bce_bce || config.opt_bce_cle) { blockedClauseElimination(); }

    if (config.opt_bce_cla) { coverdLiteralAddition(); }



    // run BCE here again to remove the new blocked clauses, if there have been any!
    return modifiedFormula;
}

Lit BlockedClauseElimination::tautologicResolvent(const Clause& c, const Clause& d, const Lit& l) const
{
    int i = 0, j = 0;
    while (i < c.size() && j < d.size()) {
        if (c[i] == l) { // skip this literal! (its used for resolution)
            i++;
        } else if (d[j] == ~l) { // skip this literal! (its used for resolution)
            j++;
        } else if (c[i] == d[j]) { // same literal
            i++;
            j++;
        } else if (c[i] == ~d[j]) { // complementary literal -> tautology!
            return c[i];
        } else if (c[i] < d[j]) {
            i++;
        } else { // if( d[j] < c[i]  ) { // its the only case that is left
            j++;
        }
    }
    return lit_Undef; // a complementarly literal was not found in both clauses
}


void BlockedClauseElimination::printStatistics(ostream& stream)
{
    cerr << "c [STAT] BCE " << bceTime.getCpuTime() << " seconds, " << bceSteps << " steps, " << testedLits <<
         " testLits, " << remBCE << " remBCE, " << bcm_cls << " BCMs, " << bcm_cls_cands << " BCMcands, " << bcm_lits <<
         " BCMlits, " << endl;
    cerr << "c [STAT] CLE " << remCLE << " remCLE, " << cleUnits << " cleUnits, " << endl;
    cerr << "c [STAT] BCE-CLA " << claTime.getCpuTime() << " seconds, " << claSteps << " steps, " <<
         claTestedLits << " testLits, " << claExtendedClauses << " extClss, " << claExtensions << " extLits, " <<
         possibleClaExtensions << " possibles, " << endl;
}

void BlockedClauseElimination::giveMoreSteps()
{
    bceSteps = bceSteps < config.opt_bceInpStepInc ? 0 : bceSteps - config.opt_bceInpStepInc;
}

void BlockedClauseElimination::destroy()
{

}

}
