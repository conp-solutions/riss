/***************************************************************************************[PSolver.h]
Copyright (c) 2014-2015,      Norbert Manthey, All rights reserved.
**************************************************************************************************/
#include "pfolio/PSolver.h"

#include "coprocessor/Coprocessor.h"
#include <assert.h>



using namespace Coprocessor;
using namespace std;


// pfolio todos
#warning be able to disable output completely, use inc/decmodel flags
#warning error shrink received clauses with reverseMinimization, share reduced clauses again (cite siert)
#warning have another buffer for equivalences and multi-units, that should not be missed
#warning set communicator settings incarnation specific, have a way to pass options from cmdline to each incarnation (have one long string as option, split that string)

namespace Riss
{



/** main method that is executed by all worker threads */
static void* runWorkerSolver(void* data);

PSolver::PSolver(Riss::PfolioConfig* externalConfig, const char* configName, int externalThreads)
    :
    privateConfig(externalConfig == 0 ? new PfolioConfig(configName) : externalConfig)
    , deleteConfig(externalConfig == 0)
    , pfolioConfig(* privateConfig)
    , initialized(false)
    , threads(pfolioConfig.threads)
    , globalSimplifierConfig(0)
    , globalSimplifier(0)
    , data(0)
    , threadIDs(0)
    , proofMaster(0)
    , opc(0)
    , defaultConfig((const char*) pfolioConfig.opt_defaultSetup == nullptr ? "" : string(pfolioConfig.opt_defaultSetup))   // setup the configuration
    , drupProofFile(0)
    , verbosity(0)
    , verbEveryConflicts(0)
{
    if (externalThreads != -1) { threads = externalThreads; }  // set number of threads from constructor, overwrite command line

    // setup the default configuration for all the solvers!
    configs   = new CoreConfig        [ threads ];
    ppconfigs = new CP3Config         [ threads ];
    communicators = new Communicator* [ threads ];

    incarnationConfigs.resize(threads);   // add a string for each configuration
    if ((const char*)pfolioConfig.opt_incarnationSetups != nullptr) {
        parseConfigurations(string(pfolioConfig.opt_incarnationSetups));
    }

    // set global coprocessor configuratoin
    if ((const char*)pfolioConfig.opt_ppconfig != nullptr) {
        defaultSimplifierConfig = string((const char*)pfolioConfig.opt_ppconfig);
    }

    // set preprocessor, if there is one selected
//     if( (const char*)pfolioConfig.opt_firstPPconfig != 0 ) ppconfigs[0].addPreset(string(pfolioConfig.opt_firstPPconfig));

    for (int i = 0 ; i < threads; ++ i) {
        communicators[i] = 0;
    }

    // set preset configs here
    createThreadConfigs();

    // here, DRUP proofs are created!
    if (pfolioConfig.opt_internalProofCheck) {
        opc = new OnlineProofChecker(drupProof);
    }

    // setup the first solver!
    solvers.push(new Solver(&configs[0]));     // from this point on the address of this configuration is not allowed to be changed any more!
    solvers[0]->setPreprocessor(&ppconfigs[0]);
}

void PSolver::parseConfigurations(const string& combinedConfigurations)
{
    assert(threads == incarnationConfigs.size() && "have enough space for the threads already");

    string workingCopy = combinedConfigurations;
    int pos = 0;
    do { // there might be more
        pos = workingCopy.find("[");     // find opening bracket
        if (pos == string::npos) { break; }  // no more opening bracket
        workingCopy = workingCopy.substr(pos + 1);     // next should be the number of the thread

        int closePos = workingCopy.find("]");
        if (closePos == string::npos) { break; }  // no more opening bracket

        int number = -1;
        stringstream s;
        s << workingCopy.substr(0, closePos); // this is the part where the number is located
        if (s.str().empty()) { break; }  // no valid number
        s >> number;                 // extract the number
        if (number < 1 || number > threads) { continue; }  // invalid thread number

        pos = workingCopy.find("[", closePos + 1); //
        int endPos = (pos == string::npos) ? workingCopy.size() : pos;   // point to end of the string

        incarnationConfigs[ number - 1 ] = workingCopy.substr(closePos + 1, pos - 2);

    } while (pos != string::npos) ;
}


PSolver::~PSolver()
{

    kill();

    sleep(0.2);

    if (globalSimplifier != 0) { delete globalSimplifier; }
    if (globalSimplifierConfig != 0) { delete globalSimplifierConfig; }

    for (int i = 0 ; i < solvers.size(); ++ i) {
        delete solvers[i]; // free all solvers
        if (communicators != 0 && communicators[i] != 0) { delete communicators[i]; }
    }
    if (communicators != 0) { delete [] communicators; communicators = 0; }
    delete [] ppconfigs; ppconfigs = 0;
    delete [] configs; configs = 0;
    solvers.clear(true);

    if (threadIDs != 0) { delete [] threadIDs; threadIDs = 0; }
    if (data != 0) { delete data; data = 0; }

    if (deleteConfig) { delete privateConfig; }
}

CoreConfig& PSolver::getConfig(const int solverID)
{
    return configs[solverID];
}

CP3Config& PSolver::getPPConfig(const int solverID)
{
    return ppconfigs[ solverID ];
}

int PSolver::nVars() const
{
    return solvers[0]->nVars();
}

int PSolver::nClauses() const
{
    return solvers[0]->nClauses();
}

int PSolver::nTotLits() const
{
    return solvers[0]->nTotLits();
}

Var PSolver::newVar(bool polarity, bool dvar, char type)
{
    return solvers[0]->newVar(polarity, dvar, type);
}

void PSolver::reserveVars(Var v)
{
    solvers[0]->reserveVars(v);
}

bool PSolver::simplify()
{
    return solvers[0]->simplify();
}

void PSolver::interrupt()
{
    for (int i = 0 ; i < solvers.size(); ++ i) {
        solvers[i]->interrupt();
    }
}

void PSolver::setConfBudget(int64_t x)
{
    for (int i = 0 ; i < solvers.size(); ++ i) {
        solvers[i]->setConfBudget(x);
    }
}

void PSolver::budgetOff()
{
    for (int i = 0 ; i < solvers.size(); ++ i) {
        solvers[i]->budgetOff();
    }
}


bool PSolver::addClause_(vec< Lit >& ps)
{
    bool ret = true;
    for (int i = 0 ; i < solvers.size(); ++ i) {
//         for (int j = 0 ; j < ps.size(); ++ j) {
//             while (solvers[i]->nVars() <= var(ps[j])) { solvers[i]->newVar(); }
//         }
        bool ret2 = solvers[i]->addClause_(ps, i != 0); // if a solver failed adding the clause, then the state for all solvers is bad as well, avoid redundancy check for all but the first solver
        if (pfolioConfig.opt_verbosePfolio) if (i == 0) { cerr << "c parsed clause " << ps << endl; } // TODO remove after debug
        ret = ret2 && ret;
    }
    return ret;
}

lbool PSolver::solveLimited(const vec< Lit >& assumps)
{
//   // for now, simply let the first solver solve the formula!
//   lbool ret = solvers[0]->solveLimited( assumps );
//
//   int winningSolver = 0;
//   if( ret == l_True ) {
//     model.clear();
//     model.capacity( solvers[winningSolver]->model.size() );
//     for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) model.push(solvers[winningSolver]->model[i]);
//   } else if ( ret == l_False ) {
//     conflict.clear();
//     conflict.capacity( solvers[winningSolver]->conflict.size() );
//     for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) conflict.push(solvers[winningSolver]->conflict[i]);
//   } else {
//
//   }
//
//   return ret;

    int winningSolver = 0;
    lbool ret = l_Undef;
    /*
     * preprocess the formula with the preprocessor of the first solver
     * but only in the very first iteration!
     */
    if (!initialized && defaultSimplifierConfig.size() != 0) {
        assert(globalSimplifierConfig == 0 && globalSimplifier == 0 && "so far we did not setup global solver and simplifier");

        globalSimplifierConfig = new Coprocessor::CP3Config(defaultSimplifierConfig.c_str());
        globalSimplifier = new Coprocessor::Preprocessor(solvers[0] , *globalSimplifierConfig, threads); // use simplifier with the given number of threads


	Coprocessor::Preprocessor* internalPreprocessor = solvers[0]->swapPreprocessor(globalSimplifier); // tell solver about global solver
	
	ret = solvers[0]->solve_(Solver::SolveCallType::simplificationOnly); // solve until preprocessing
	
	solvers[0]->swapPreprocessor(internalPreprocessor); // ignore return value, as we still know this pointer

	if (verbosity > 0) { cerr << "c solver0 with " << solvers[0]->nVars() << " variables, and " << solvers[0]->clauses.size() << " clauses" << endl; }
        // solved by simplification?
        if (ret == l_False) { return ret; }
        else if (ret == l_True) {
            winningSolver = 0;
            model.clear();
            solvers[winningSolver]->model.copyTo(model);
            if (globalSimplifier != 0) { globalSimplifier->extendModel(model); }
            if( ret == l_True && verbosity > 0) cerr << "c solved formula with simplification" << endl;
            return ret;
        }
    }

    if (! initialized) {  // distribute the formula, if this is the first call to this method!

        /* setup all solvers
        * setup the communication system for the solvers, including the number of commonly known variables
        */
        if (initializeThreads()) {
            cerr << "c initialization of " << threads << " threads: failed" << endl;
            return l_Undef;
        } else {
            if (verbosity > 0) { cerr << "c initialization of " << threads << " threads: succeeded" << endl; }
        }
        if (proofMaster != 0 && pfolioConfig.opt_verboseProof > 0) { proofMaster->addCommentToProof("c initialized parallel solvers", -1); }

        /*
         * copy the formula from the first solver to all the other solvers
         */

        for (int i = 1; i < solvers.size(); ++ i) {
            solvers[i]->reserveVars(solvers[0]->nVars());
            while (solvers[i]->nVars() < solvers[0]->nVars()) { solvers[i]->newVar(); }
            communicators[i]->setFormulaVariables(solvers[0]->nVars());   // tell which variables can be shared

	    // pseudo clone solver incarnations
            solvers[i]->addUnitClauses(solvers[0]->trail);   // copy all the unit clauses, adds to the proof
            solvers[0]->ca.copyTo(solvers[i]->ca);             // have information about clauses
            solvers[0]->clauses.copyTo(solvers[i]->clauses);   // copy clauses silently without the proof, no redundancy check required
            solvers[0]->learnts.copyTo(solvers[i]->learnts);   // copy clauses silently without the proof, no redundancy check required
	    solvers[0]->activity.copyTo(solvers[i]->activity); // copy activity
	    solvers[0]->order_heap.copyOrderTo( solvers[i]->order_heap ); // rebuild order heap (use configuration of other heap, but use own acticities)
	    solvers[0]->varFlags.copyTo( solvers[i]->varFlags );
	    solvers[0]->vardata.copyTo( solvers[i]->vardata );

            // attach all clauses
            for (int j = 0 ; j < solvers[i]->clauses.size(); ++ j) {
                solvers[i]->attachClause(solvers[i]->clauses[j]);     // import the clause of solver 0 into solver i; does not add to the proof
            }
            for (int j = 0 ; j < solvers[i]->learnts.size(); ++ j) {
                solvers[i]->attachClause(solvers[i]->learnts[j]);     // import the clause of solver 0 into solver i; does not add to the proof
            }
	    solvers[i]->solve_( Solver::SolveCallType::initializeOnly ); // let solve initialize itself
            if (verbosity > 1) { cerr << "c Solver[" << i << "] has " << solvers[i]->nVars() << " vars, " << solvers[i]->clauses.size() << " cls, " << solvers[i]->learnts.size() << " learnts" << endl; }
            solvers[i]->setPreprocessor(&ppconfigs[i]); // tell solver incarnation about preprocessor
        }

        // copy the formula of the solver 0 number of thread times
        if (proofMaster != 0) {  // if a proof is generated, add all clauses that are currently present in solvers[0]
            if (pfolioConfig.opt_verboseProof > 0) { proofMaster->addCommentToProof("add irredundant clauses multiple times", -1); }
            for (int j = 0 ; j < solvers[0]->clauses.size(); ++ j) {
                proofMaster->addInputToProof(solvers[0]->ca[ solvers[0]->clauses[j] ], -1, threads); // so far, work on global proof
            }
            if (pfolioConfig.opt_verboseProof > 0) { proofMaster->addCommentToProof("add redundant clauses multiple times", -1); }
            for (int j = 0 ; j < solvers[0]->learnts.size(); ++ j) {
                proofMaster->addInputToProof(solvers[0]->ca[ solvers[0]->learnts[j] ], -1, threads);  // so far, work on global proof
            }
            if (pfolioConfig.opt_verboseProof > 0) { proofMaster->addCommentToProof("add unit clauses of solver 0", -1); }
            proofMaster->addUnitsToProof(solvers[0]->trail, 0, false);   // incorporate all the units once more
        }


        initialized = true;
    } else {
        // already initialized -- simply print the summary
        for (int i = 0; i < solvers.size(); ++ i) {
            if (verbosity > 1) { cerr << "c Solver[" << i << "] has " << solvers[i]->nVars() << " vars, " << solvers[i]->clauses.size() << " cls, " << solvers[i]->learnts.size() << " learnts" << endl; }
        }
    }
    /*
     * reset solvers from previous call,
     * run the solvers on the formula
     */
    // add assumptions to all solvers
    for (int i = 0 ; i < threads; ++i) {
        for (int j = 0; j < assumps.size(); ++ j) {  // make sure, everybody knows all the variables
            while (solvers[i]->nVars() <= var(assumps[j])) { solvers[i]->newVar(); }
        }
#warning check whether necessary
//         assumps.copyTo(communicators[i]->assumptions);
        communicators[i]->setFormulaVariables(solvers[i]->nVars());   // for incremental calls, no ER is supported, so that everything should be fine until here! Note: be careful with this!
        communicators[i]->setWinner(false);
        assert((communicators[i]->isFinished() || communicators[i]->isWaiting()) && "all solvers should not touch anything!");
    }

    if (proofMaster != 0 && pfolioConfig.opt_verboseProof > 0) { proofMaster->addCommentToProof("c start all solvers", -1); }
    start(); // allow all solvers to start,
    waitFor(oneFinished);   // and wait until the first solver finishes

    /* interrupt all other solvers
    * clear all interrupts (for incremental solving)
    */
    for (int i = 0 ; i < threads; ++i) { solvers[i]->interrupt(); }

    // wait for the remaining threads to finish, and clear their interrupt again
    waitFor(allFinished);
    for (int i = 0 ; i < threads; ++i) {
        assert(communicators[i]->isFinished() && "all solvers have to be finished");
        solvers[i]->resetLastSolve(); // clear state of the solver (jump to level 0, clear interrupt)
    }

    /*
    * determine the winning solver
    * store results of the winning solver into this solver
    */
    winningSolver = 0;
    for (int i = 0 ; i < threads; ++ i) {
        if (communicators[i]->isWinner() && communicators[i]->getReturnValue() != l_Undef) { winningSolver = i; break; }
    }

    if (verbosity > 0) { if (verbosity > 0) { cerr << "c MASTER found winning thread (" << communicators[winningSolver]->isWinner() << ") as " << winningSolver << " / " << threads << " model= " << solvers[winningSolver]->model.size() << endl; } }

    // return model, if there is a winning thread!
    if (winningSolver < threads) {
        /*
        * return the result state
        */
        ret = communicators[ winningSolver ]->getReturnValue();
        if (ret == l_True) {
            model.clear();
            solvers[winningSolver]->model.copyTo(model);
            if (globalSimplifier != 0) { globalSimplifier->extendModel(model); }

            if (false && verbosity > 2) {
                cerr << "c units: " << endl; for (int i = 0 ; i < solvers[winningSolver]->trail.size(); ++ i) { cerr << " " << solvers[winningSolver]->trail[i] << " 0" << endl; }  cerr << endl;
                cerr << "c clauses: " << endl; for (int i = 0 ; i < solvers[winningSolver]->clauses.size(); ++ i) { cerr << "c " << solvers[winningSolver]->ca[solvers[winningSolver]->clauses[i]] << endl; }
                cerr << "c assumptions: " << endl; for (int i = 0 ; i < assumps.size(); ++ i) { cerr << " " << assumps[i]; }  cerr << endl;
            }

        } else if (ret == l_False) {
            conflict.clear();
            conflict.capacity(solvers[winningSolver]->conflict.size());
            for (int i = 0 ; i < solvers[winningSolver]->conflict.size(); ++ i) { conflict.push(solvers[winningSolver]->conflict[i]); }
        } else {
            if (verbosity > 0) { cerr << "c winning thread returned UNKNOWN" << endl; }
        }
    } else { ret = l_Undef; }

    if (verbosity > 0) {
        if (verbosity > 0) { cerr << "c thread  S  \t|\t R   \t|\t sRej \t|\t lRej \t|\t vivLits \t|" << endl; }
        for (int i = 0 ; i < threads; ++ i) {
            if (verbosity > 0) cerr << "c " << i << " : " << communicators[i]->nrSendCls
                                        <<  "  \t|\t" << communicators[i]->nrReceivedCls
                                        <<  "  \t|\t" << communicators[i]->nrRejectSendSizeCls
                                        <<  "  \t|\t" << communicators[i]->nrRejectSendLbdCls
                                        <<  "  \t|\t" << communicators[i]->vivifiedLiterals << "\t|"
                                        << endl;
        }
        if (verbosity > 0) { cerr << "c thread  S-EE\t|\t R-EE\t|\t SUs\t|\t RUs\t" << endl; }
        for (int i = 0 ; i < threads; ++ i) {
            if (verbosity > 0) cerr << "c " << i << " : " << communicators[i]->nrSendEEs
                                        <<  "  \t|\t" << communicators[i]->nrReceivedEEs
                                        <<  "  \t|\t" << communicators[i]->nrSendMultiUnits
                                        <<  "  \t|\t" << communicators[i]->nrReceivedMultiUnits
                                        << endl;
        }
        if (verbosity > 0) { cerr << "attempts:" << endl; }
        if (verbosity > 0) { cerr << "c thread  SCls\t|\tS-EE\t|\tS-U\t|\tRec\t" << endl; }
        for (int i = 0 ; i < threads; ++ i) {
            if (verbosity > 0) cerr << "c " << i << " : " << communicators[i]->nrSendCattempt
                                        <<  "  \t|\t" << communicators[i]->nrSendMattempt
                                        <<  "  \t|\t" << communicators[i]->nrSendEattempt
                                        <<  "  \t|\t" << communicators[i]->nrReceiveAttempts
                                        << endl;
        }
        if (verbosity > 0) { cerr << "search data:" << endl; }
        for (int i = 0 ; i < threads; ++ i) {
            if (verbosity > 0) cerr << "c " << i << " : cons: " << communicators[i]->getSolver()->conflicts
                                        <<  "  dec: " << communicators[i]->getSolver()->decisions
                                        <<  "  units: " << (communicators[i]->getSolver()->trail_lim.size() == 0 ? communicators[i]->getSolver()->trail.size() : communicators[i]->getSolver()->trail_lim[0])
                                        << endl;
        }
    }



    return ret;
}


void PSolver::createThreadConfigs()
{
    const char* Configs[] = {
        "",         // 0
        "PLAINBIASSERTING", // 1
        "LLA",      // 2
        "AUIP",     // 3
        "SUHD",     // 4
        "OTFSS",        // 5
        "SUHLE",        // 6
        "LHBR",     // 7
        "HACKTWO",      // 8
        "NOTRUST",      // 9
        "DECLEARN",     // 10
        "PLAINBIASSERTING", // 11
        "LBD",      // 12
        "FASTRESTART",  // 13
        "AGILREJECT",   // 14
        "LIGHT",        // 15
        "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 16 - 31
        "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 32 - 47
        "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 48 - 63
    };

    if (defaultConfig.size() > 0) {
        if (verbosity > 1) { cerr << "c setup pfolio with config " << defaultConfig << endl; }
    }

    for (int t = 0 ; t < threads; ++ t) {
        configs[t].opt_verboseProof = 0; // do not print to the proof by default
    }

    if (defaultConfig.size() == 0) {
        for (int t = 0 ; t < threads; ++ t) {
            if (incarnationConfigs[t].size() == 0) {  // assign preset, if no cmdline was specified
	      configs[t].setPreset(Configs[t]);
	      ppconfigs[t].setPreset(Configs[t]);
	    }   
            else { 
	      configs[t].setPreset(incarnationConfigs[t]);
	      ppconfigs[t].setPreset(incarnationConfigs[t]);
	    }                          // otherwise, use commandline configuration
        }
    } else if (defaultConfig == "BMC") {
        // thread 1 runs with empty (default) configurations
        if (threads > 1) { configs[1].setPreset("DECLEARN"); }
        if (threads > 2) { configs[2].setPreset("FASTRESTART"); }
        if (threads > 3) { configs[3].setPreset("SUHLE"); }
        for (int t = 4 ; t < threads; ++ t) {
            configs[t].setPreset(Configs[t]);
            // configs[t].setPreset("-solververb=2"); // set all solvers very verbose
        }
    } else if (defaultConfig == "PLAIN") {
        for (int t = 0 ; t < threads; ++ t) {
            configs[t].setPreset("");   // do not set anything
        }
    } else if (defaultConfig == "DRUP") {
        for (int t = 0 ; t < threads; ++ t) {
            if (pfolioConfig.opt_verboseProof > 1) {
                configs[t].opt_verboseProof = 1;
                //configs[t].pfolioConfig.opt_verboseProof = true;
            }
        }
    } else if (defaultConfig == "RESTART") {
        //configs[1].setPreset("");
        if (threads > 1) { configs[1].setPreset("-K=0.7 -R=1.5"); }
        if (threads > 2) { configs[2].setPreset("-var-decay-b=0.85 var-decay-e=0.85"); }
        if (threads > 3) { configs[3].setPreset("-K=0.7 -R=1.5 -var-decay-b=0.85 var-decay-e=0.85"); }
        // TODO: set more for higher numbers
    } else if (defaultConfig == "FULLSHARE") {
        if (verbosity > 1) { cerr << "c setup FULLSHARE configurations" << endl; }
        if (threads > 1) {
            ppconfigs[1].setPreset("-enabled_cp3 -shareTime=2 -ee -cp3_ee_it -cp3_ee_level=2 -inprocess -cp3_inp_cons=10000");
        }
        if (threads > 2) {
            ppconfigs[2].setPreset("-enabled_cp3 -shareTime=2 -probe -pr-probe -no-pr-vivi -pr-bins -pr-lhbr -inprocess -cp3_inp_cons=10000");
        }
        if (threads > 3) {
            ppconfigs[3].setPreset("-enabled_cp3 -shareTime=2 -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3 -inprocess -cp3_inp_cons=10000");
        }
    } else if (defaultConfig == "alpha") {

        if (threads > 0) {
            ppconfigs[0].setPreset("-revMin -init-act=3 -actStart=2048 -no-receive");
            configs[0].setPreset("-revMin -init-act=3 -actStart=2048 -no-receive -shareTime=1");
        }
        if (threads > 1) {
            ppconfigs[1].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -act-based -refRec -resRefRec");
            configs[1].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -act-based -refRec -resRefRec -shareTime=1");
        }
        if (threads > 2) {
            ppconfigs[2].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=30000 -cp3_itechs=uev -no-dense -up -refRec ");
            configs[2].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=30000 -cp3_itechs=uev -no-dense -up -refRec -shareTime=1");
        }
        if (threads > 3) {
            ppconfigs[3].setPreset("-revMin -init-act=3 -actStart=2048 -keepWorst=0.01 -refRec ");
            configs[3].setPreset("-revMin -init-act=3 -actStart=2048 -keepWorst=0.01 -refRec -shareTime=1");
        }
        if (threads > 4) {
            ppconfigs[4].setPreset("-revMin -init-act=4 -actStart=2048 -refRec ");
            configs[4].setPreset("-revMin -init-act=4 -actStart=2048 -refRec -shareTime=2");
        }
        if (threads > 5) {
            ppconfigs[5].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -refRec ");
            configs[5].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -refRec  -shareTime=2");
        }
        if (threads > 6) {
            ppconfigs[6].setPreset("-revMin -init-act=3 -actStart=2048 -longConflict -refRec ");
            configs[6].setPreset("-revMin -init-act=3 -actStart=2048 -longConflict -refRec  -shareTime=2");
        }
        if (threads > 7) {
            ppconfigs[7].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=1000000 -cp3_itechs=uev -no-dense -up -refRec ");
            configs[7].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=1000000 -cp3_itechs=uev -no-dense -up -refRec  -shareTime=2");
        }
        for (int t = 8 ; t < threads; ++ t) {  // set configurations for remaining (beyond 8)
            if (incarnationConfigs[t].size() == 0) { configs[t].setPreset(Configs[t]); }   // assign preset, if no cmdline was specified
            else { configs[t].setPreset(incarnationConfigs[t]); }                          // otherwise, use commandline configuration
        }
    } else if (defaultConfig == "beta") {
   if (threads > 0) {
            ppconfigs[0].setPreset("-revMin -init-act=3 -actStart=2048 -no-receive");
            configs[0].setPreset("-revMin -init-act=3 -actStart=2048 -no-receive -shareTime=0 -dynLimits"); // sends all
        }
        if (threads > 1) {
            ppconfigs[1].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -act-based -refRec -resRefRec");
            configs[1].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -act-based -refRec -resRefRec -shareTime=1 -sendAll");
        }
        if (threads > 2) {
            ppconfigs[2].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=30000 -cp3_itechs=uev -no-dense -up -refRec ");
            configs[2].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=30000 -cp3_itechs=uev -no-dense -up -refRec -shareTime=1 -sendAll");
        }
        if (threads > 3) {
            ppconfigs[3].setPreset("-revMin -init-act=3 -actStart=2048 -keepWorst=0.01 -refRec ");
            configs[3].setPreset("-revMin -init-act=3 -actStart=2048 -keepWorst=0.01 -refRec -shareTime=1 -sendAll");
        }
        if (threads > 4) {
            ppconfigs[4].setPreset("-revMin -init-act=4 -actStart=2048 -refRec ");
            configs[4].setPreset("-revMin -init-act=4 -actStart=2048 -refRec -shareTime=2 -dynLimits");
        }
        if (threads > 5) {
            ppconfigs[5].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -refRec ");
            configs[5].setPreset("-revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -refRec  -shareTime=0 -dynLimits");
        }
        if (threads > 6) {
            ppconfigs[6].setPreset("-revMin -init-act=3 -actStart=2048 -longConflict -refRec ");
            configs[6].setPreset("-revMin -init-act=3 -actStart=2048 -longConflict -refRec  -shareTime=2 -sendAll");
        }
        if (threads > 7) {
            ppconfigs[7].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=1000000 -cp3_itechs=uev -no-dense -up -refRec ");
            configs[7].setPreset("Riss427:plain_XOR:-no-usePP -cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=1000000 -cp3_itechs=uev -no-dense -up -refRec  -shareTime=2 -sendAll");
        }
        for (int t = 8 ; t < threads; ++ t) {  // set configurations for remaining (beyond 8)
            if (incarnationConfigs[t].size() == 0) { configs[t].setPreset(Configs[t]); }   // assign preset, if no cmdline was specified
            else { configs[t].setPreset(incarnationConfigs[t]); }                          // otherwise, use commandline configuration
        }
    }
    
    
    // assign common setup prefix
    if( (const char*)pfolioConfig.opt_allIncPresets != 0 ) {
      for (int t = 0 ; t < threads; ++ t) {
	configs[t].setPreset(string(   pfolioConfig.opt_allIncPresets) );
	ppconfigs[t].setPreset(string( pfolioConfig.opt_allIncPresets) );
      }
    }
}

void PSolver::addInputClause_(vec< Lit >& ps)
{
    if (opc != 0) {
        // if( verbosity > 1 ) cerr << "c add parsed clause to DRAT-OTFC: " << ps << endl;
        opc->addParsedclause(ps);
    }
    return;
}


bool PSolver::initializeThreads()
{
    bool failed = false;
    // get space for thread ids
    threadIDs = new pthread_t [threads];

    data = new CommunicationData(privateConfig->opt_storageSize == 0 ? 4000 * threads : privateConfig->opt_storageSize);   // space for clauses, dynamic or static


    // the portfolio should print proofs
    if (drupProofFile != 0) {
        proofMaster = new ProofMaster(drupProofFile, threads, nVars(), pfolioConfig.opt_proofCounting, pfolioConfig.opt_verboseProof > 1);  // use a counting proof master
        proofMaster->setOnlineProofChecker(opc);     // tell proof master about the online proof checker
        data->setProofMaster(proofMaster);       // tell shared clauses pool about proof master (so that it adds shared clauses)
    }

    // create all solvers and threads
    for (unsigned i = 0 ; i < threads; ++ i) {
        communicators[i] = new Communicator(i, data);

        // create the solver
        if (i > 0) {
            assert(solvers.size() == i && "next solver is not already created!");
            solvers.push(new Solver(& configs[i]));      // solver 0 should exist already!
        }

        // setup parameters for communication system
        communicators[i]->protectAssumptions = pfolioConfig.opt_protectAssumptions;
        communicators[i]->sendSize = pfolioConfig.opt_sendSize;
        communicators[i]->sendLbd = pfolioConfig.opt_sendLbd;
        communicators[i]->sendMaxSize = pfolioConfig.opt_sendMaxSize;
        communicators[i]->sendMaxLbd = pfolioConfig.opt_sendMaxLbd;
        communicators[i]->sizeChange = pfolioConfig.opt_sizeChange;
        communicators[i]->lbdChange = pfolioConfig.opt_lbdChange;
        communicators[i]->sendRatio = pfolioConfig.opt_sendRatio;
        communicators[i]->doBumpClauseActivity = pfolioConfig.opt_doBumpClauseActivity;

        communicators[i]->sendIncModel = pfolioConfig.opt_sendIncModel;
        communicators[i]->sendDecModel = pfolioConfig.opt_sendDecModel;
        communicators[i]->useDynamicLimits = pfolioConfig.opt_useDynamicLimits;
        communicators[i]->sendEquivalences = pfolioConfig.opt_sendEquivalences;
        // could set receiveEquivalences here, but that should be more up to the actual solver configurations

        // setup thread specific settings
#warning have option for thread specific settings, or have local flags that can be controlled via the CoreConfig object
        if (defaultConfig == "FULLSHARE") {
            if (i == 1) {  // apply received clause vivification, and sent shrinked clauses back

            }
        }

        // tell the communication system about the solver
        communicators[i]->setSolver(solvers[i]);

        if (! pfolioConfig.opt_share) { communicators[i]->setDoSend(false); }   // no sending
        if (!pfolioConfig.opt_receive) { communicators[i]->setDoReceive(false); }  // no sending

        // tell the communicator about the proof master
        communicators[i]->setProofMaster(proofMaster);
        // tell solver about its communication interface
        solvers[i]->setCommunication(communicators[i]);

        if (verbosity > 0) { cerr << "c [MASTER] setup thread " << i << " with data at " << std::hex << communicators[i]  << std::dec << endl; }

        /*
         * launch child threads here (will initially sleep in their run method)
         */
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        // create one thread
        int rc = pthread_create(&(threadIDs[i]), &attr, runWorkerSolver, (void *) communicators[i]);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d for thread %d\n", rc, i);
            failed = failed || true;
            // set thread to abort if creation went wrong
            communicators[i]->setState(Communicator::aborted);
        }
        pthread_attr_destroy(&attr);
    }
    solvers[0]->proofFile = 0; // set to 0, independently of the previous value
    return failed;
}

void PSolver::start()
{
// set all threads to working (they'll have a look for new work on their own)
    for (unsigned i = 0 ; i < threads; ++ i) {
        communicators[i]->ownLock->lock();
        if (verbosity > 1) { cerr << "c [MASTER] set thread " << i << " state to working" << endl; }
        communicators[i]->setState(Communicator::working);
        communicators[i]->ownLock->unlock();
        if (verbosity > 1) { cerr << "c [MASTER] awake thread " << i << endl; }
        communicators[i]->ownLock->awake();
    }
}

void PSolver::waitFor(const WaitState waitState)
{
// evaluate the state of all the threads
    data->getMasterLock().lock();
    // repeat until there is some thread with the state finished
    int waitForIterations = 0;
    while (true) {
        waitForIterations ++;
        // check whether there are free threads
        unsigned threadOfInterest = threads;
        for (unsigned i = 0 ; i < threads; ++ i) {
            if (verbosity > 2) { cerr  << "c [WFI" << waitForIterations << "] MASTER checks thread " << i << " / " << threads << " finished: " << communicators[i]->isFinished() << endl; }
            if (waitState == oneIdle) {
                if (communicators[i]->isIdle()) { threadOfInterest = i; break; }
            } else if (waitState == oneFinished) {
                if (communicators[i]->isFinished()) {
                    threadOfInterest = i;
                    if (verbosity > 2) { cerr << "c [WFI" << waitForIterations << "] Thread " << i << " finished" << endl; }
                    // communicators[i]->setState( Communicator::waiting );
                    break;
                }
            } else if (waitState == allFinished) {
                if (! communicators[i]->isFinished() && ! communicators[i]->isIdle()) {  // be careful with (! communicators[i]->isWaiting() &&)
                    if (verbosity > 2) { cerr << "c [WFI" << waitForIterations << "] Thread " << i << " is not finished and not idle" << endl; }
                    threadOfInterest = i; // notify that there is a thread that is not finished with this index
                    break;
                }
            } else {
                assert(false && "Wait case for the given state has not been implemented");
            }
        }

        if (waitState == allFinished) {
            if (threadOfInterest == threads) {
                data->getMasterLock().unlock(); // leave critical section
                return;                         // there is a free thread - use it!
            }
        } else {
            if (threadOfInterest != threads) {
                data->getMasterLock().unlock(); // leave critical section
                return;                         // there is a free thread - use it!
            }
        }

        if (verbosity > 2) { cerr << "c [WFI" << waitForIterations << "] MASTER sleeps" << endl; }
        data->getMasterLock().sleep();  // wait until some thread notifies the master
    }
    data->getMasterLock().unlock(); // leave critical section
}

void PSolver::kill()
{
    if (!initialized) { return; }  // child threads have never been created - no need to kill them

    if (verbosity > 0) { cerr << "c MASTER kills all child threads ..." << endl; }
    // set all threads to working (they'll have a look for new work on their own)
    for (unsigned i = 0 ; i < threads; ++ i) {
        communicators[i]->ownLock->lock();
        communicators[i]->setState(Communicator::aborted);
        communicators[i]->ownLock->unlock();
        communicators[i]->ownLock->awake();
    }

//   // interrupt all threads!
//   for( unsigned i = 0 ; i<threads; ++ i )
//   {
//     int* status = 0;
//     int err = 0;
//     err = pthread_kill(threadIDs[i], 15);
//     if( err != 0) cerr << "c killing a thread resulted in a failure" << endl;
//   }

    // join all threads!
    for (unsigned i = 0 ; i < threads; ++ i) {
        int* status = 0;
        int err = 0;
        err = pthread_join(threadIDs[i], (void**)&status);
        if (err != 0) { cerr << "c joining a thread resulted in a failure with status " << *status << endl; }
    }
    if (verbosity > 1) { cerr << "c finished killing" << endl; }
}

void* runWorkerSolver(void* data)
{
    const bool verbose = false;

    /* Algorithm:
     */

    // tell thread that it can be interrupted
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    Communicator& info = * ((Communicator*)data);

    if (verbose) { cerr << "c started thread with id " << info.getID() << endl; }
    // initially, wait until master provides work!
    info.ownLock->lock();
    if (verbose) { cerr << "c thread id " << info.getID() << " state= " << (info.isIdle() ? "idle" : "nonIdle") << endl; }
    if (info.isIdle() || info.isWaiting()) {
        info.ownLock->sleep();
    }
    info.ownLock->unlock();
    vec<Lit> assumptions;

    // proceed with the current work item (group) as long as required
    while (! info.isAborted()) {
        if (verbose) { cerr << "c [THREAD] " << info.getID() << " start " <<  endl; }

        // solve with assumptions!
        assumptions.clear();
        info.assumptions.copyTo(assumptions);   // get the current assumptions
        if (verbose) { cerr << "c [THREAD] " << info.getID() << " solve task with " << assumptions.size() << " assumed literals:" << assumptions << endl; }

        // do work
        lbool result l_Undef;
        result = info.getSolver()->solveLimited(assumptions,Solver::SolveCallType::afterSimplification);

        info.setReturnValue(result);
        if (verbose) cerr << "c [THREAD] " << info.getID() << " result " <<
                              (result == l_Undef ? "UNKNOWN" : (result == l_True ? "SAT" : "UNSAT"))
                              << endl;

        if (result != l_Undef) { // solved the formula
            info.setWinner(true); // indicate that this solver has a solution
        }

        // when done, tell master ...
        if (verbose) { cerr << "c [THREAD] " << info.getID() << " change state to finished" << endl; }
        // set own state to finished
        info.ownLock->lock();
        // depending on whether we did something useful, set the state
        info.setState(Communicator::finished);
        info.ownLock->unlock();

        if (verbose) { cerr << "c [THREAD] " << info.getID() << " wake up master" << endl; }
        // wake up master so that the master can do something with the solver
        info.awakeMaster();

        if (verbose) { cerr << "c [THREAD] " << info.getID() << " wait for next round (sleep)" << endl; }
        // wait until master changes the state again to working!
        info.ownLock->lock();
        while (! info.isWorking() && ! info.isAborted()) { // wait for work or being aborted
            if (verbose) { cerr << "c [THREAD] " << info.getID() << " sleeps until next work ... " << endl; }
            info.ownLock->sleep();
            if (verbose) { cerr << "c [THREAD] " << info.getID() << " awakes after work-sleep ... " << endl; }
        }
        info.ownLock->unlock();
    }

    return 0;
}

void PSolver::setDrupFile(FILE* drupFile)
{
    // set file for the first solver
    if (!initialized && solvers.size() > 0) {
        if (verbosity > 1) { cerr << "c set DRUP file for solver 0" << endl; }
        solvers[0]->proofFile = drupFile;
    }
    // set own file handle to initialize the proof master afterwards
    drupProofFile = drupFile;
}


} // namespace Riss
