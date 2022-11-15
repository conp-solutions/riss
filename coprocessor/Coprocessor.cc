/**********************************************************************************[Coprocessor.cc]
Copyright (c) 2012, Norbert Manthey, LGPL v2, see LICENSE
**************************************************************************************************/

#include "coprocessor/Coprocessor.h"

#include "coprocessor/Shuffler.h"
#include "riss/mtl/Sort.h"
#include "riss/utils/VarFileParser.h"

#include <cstring>
#include <iostream>

using namespace std;
using namespace Riss;

namespace Coprocessor {

    Preprocessor::Preprocessor(Solver* _solver, CP3Config& _config, int32_t _threads)
        : config(_config)
        , threads(_threads < 0 ? config.opt_threads : _threads)
        , solver(_solver)
        , ca(solver->ca)
#ifndef NDEBUG
        , log(config.opt_log)
        , data(solver->ca, solver, log, config.opt_unlimited, config.opt_randomized, config.opt_debug)
#else
        , log(0)
        , data(solver->ca, solver, log, config.opt_unlimited, config.opt_randomized, false)
#endif
        , controller(config.opt_threads)
        // attributes and all that
        , thisClauses(0)
        , thisLearnts(0)
        , lastInpConflicts(0)
        , formulaVariables(-1)
        , inprocessings(0)
        // classes for preprocessing methods
        , propagation(config, solver->ca, controller)
        , subsumption(config, solver->ca, controller, data, propagation)
        , hte(config, solver->ca, controller, propagation)
        , bve(config, solver->ca, controller, propagation, subsumption)
        , bva(config, solver->ca, controller, data, propagation)
        , cce(config, solver->ca, controller, propagation)
        , ee(data, config, solver->ca, controller, propagation, subsumption)
        , unhiding(config, solver->ca, controller, data, propagation, subsumption, ee)
        , probing(config, solver->ca, controller, data, propagation, ee, *solver)
        , backbone(config, solver->ca, controller, data, propagation, *solver)
        , be(config, solver->ca, controller, data, propagation, probing, backbone, *solver)
        , rate(config, solver->ca, controller, data, *solver, propagation)
        , resolving(config, solver->ca, controller, data, propagation)
        , rewriter(config, solver->ca, controller, data, propagation, subsumption)
        , fourierMotzkin(config, solver->ca, controller, data, propagation, *solver)
        , dense(config, solver->ca, controller, data, propagation)
        , symmetry(config, solver->ca, controller, data, *solver)
        , xorReasoning(config, solver->ca, controller, data, propagation, ee)
        , bce(config, solver->ca, controller, data, propagation)
        , la(config, solver->ca, controller, data, propagation)
        , entailedRedundant(config, solver->ca, controller, data)
        , hbr(config, solver->ca, controller, data, propagation)
        , experimental(config, solver->ca, controller, data, *solver)
        , modprep(config, solver->ca, controller, data, *solver)
        , shuffler(config)
        , sls(config, data, solver->ca, controller)
        , shuffleVariable(-1) {
        controller.init();
    }

    Preprocessor::~Preprocessor() {
        if (config.opt_verbose > 2) {
            cerr << "c destruct preprocessor" << endl;
        }
    }

    lbool Preprocessor::performSimplification() {
        if (!config.opt_enabled) {
            return l_Undef;
        }
        if (config.opt_verbose > 4) {
            cerr << "c start simplifying with coprocessor" << endl;
        }

        if (formulaVariables == -1) {
            if (config.opt_verbose > 2) {
                cerr << "c initialize CP3 with " << solver->nVars() << " variables " << endl;
            }
            formulaVariables = solver->nVars();
        }

        MethodClock mc(data.isInprocessing() ? ipTime : ppTime); // measure the time for this iteration!
        MethodClock moh(overheadTime);                           // measure overhead
        DOUT(if (config.printAfter != 0) cerr << "c printAfter " << config.printAfter << endl;);

        // first, remove all satisfied clauses
        if (config.opt_simplify && !solver->simplify()) {
            cout.flush();
            cerr.flush();
            return l_False;
        }

        lbool status = l_Undef;
        // delete clauses from solver

        DOUT(if (config.opt_check) cerr << "present clauses: orig: " << solver->clauses.size() << " learnts: " << solver->learnts.size() << endl;);

        cleanSolver();
        // initialize techniques
        data.init(solver->nVars());
        data.resetPPhead(); // to see all unit propagations also in CP, even if they have been processed inside the solver already

        if (config.opt_shuffle) {
            shuffle();
        }

        DOUT(if (config.opt_check) checkLists("before initializing"););
        initializePreprocessor();
        DOUT(if (config.opt_check) checkLists("after initializing"););

        if (config.opt_verbose > 4) {
            cerr << "c coprocessor finished initialization" << endl;
        }

        const bool printBVE = false, printBVA = false, printProbe = false, printBackbone = false, printBe = false, printUnhide = false,
                   printCCE = false, printRATE = false, printEE = false, printREW = false, printFM = false, printHTE = false, printSusi = false,
                   printUP = false, printTernResolve = false, printAddRedBin = false, printXOR = false, printENT = false, printBCE = false,
                   printLA = false;

        // begin clauses have to be sorted here!!
        sortClauses();
        moh.stop();

        for (int ppIteration = 0; ppIteration < (data.isInprocessing() ? 1 : config.opt_simplifyRounds); ++ppIteration) {

            //      cerr << "c EE replacements: " << endl;
            //  for ( Var v = 0 ; v < data.nVars(); ++ v ) {
            //    if( v != var(data.replacedBy() [ v ]) ) cerr << "c " << v << " <-> " << var(data.replacedBy() [ v ]) << endl;
            //  }

            if (data.isInterupted()) {
                break;
            } // stop here due to signal

            double iterTime = cpuTime();
            DOUT(if (config.opt_verbose > 0 || config.opt_debug) cerr << "c pp iteration " << ppIteration << endl;);
            // do preprocessing
            if (config.opt_up) {
                if (config.opt_verbose > 0) {
                    cerr << "c up ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") propagate" << endl;
                }
                if (status == l_Undef) {
                    status = propagation.process(data, true);
                }
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    propagation.printStatistics(cerr);
                }
            }

            DOUT(if (printUP || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'u')) {
                printFormula("after Sorting");
            });

            DOUT(if (config.opt_debug) { scanCheck("after SORT"); });

            if (config.opt_xor) {
                if (config.opt_verbose > 0) {
                    cerr << "c xor ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") XOR" << endl;
                }
                if (status == l_Undef) {
                    xorReasoning.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    xorReasoning.printStatistics(cerr);
                }
                if (!data.ok()) {
                    status = l_False;
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-XOR.cnf").c_str(), 0););
            }
            data.checkGarbage(); // perform garbage collection

            DOUT(if (config.opt_debug) {
                checkLists("after XOR");
                scanCheck("after XOR");
            });
            DOUT(if (printXOR || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'x')) {
                printFormula("after XOR");
            });
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_ent) {
                if (config.opt_verbose > 0) {
                    cerr << "c ent ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") entailed redundancy" << endl;
                }
                if (status == l_Undef) {
                    entailedRedundant.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    entailedRedundant.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-ENT.cnf").c_str(), 0););
            }
            data.checkGarbage(); // perform garbage collection

            DOUT(if (config.opt_debug) {
                checkLists("after ENT");
                scanCheck("after ENT");
            });
            DOUT(if (printENT || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == '1')) {
                printFormula("after ENT");
            });

            if (config.opt_ternResolve) {
                if (config.opt_verbose > 0) {
                    cerr << "c res3 ..." << endl;
                }
                resolving.process(false);
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    resolving.printStatistics(cerr);
                }
                DOUT(if (printTernResolve || config.opt_debug ||
                         (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == '3'))
                         printFormula("after TernResolve"););
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-3RES.cnf").c_str(), 0););
            }
            if (!data.ok()) {
                break;
            } // stop here already

            // clear subsimp stats
            if (true) {
                subsumption.resetStatistics();
            }

            if (config.opt_subsimp) {
                if (config.opt_verbose > 0) {
                    cerr << "c subsimp ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") subsume/strengthen" << endl;
                }
                if (status == l_Undef) {
                    subsumption.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    subsumption.printStatistics(cerr);
                }
                if (!solver->okay()) {
                    status = l_False;
                }
                data.checkGarbage(); // perform garbage collection
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-SUBSIMP.cnf").c_str(), 0););

                DOUT(if (printSusi || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 's')) {
                    printFormula("after Susi");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            DOUT(if (config.opt_debug) {
                checkLists("after SUSI");
                scanCheck("after SUSI");
            });

            if (config.opt_FM) {
                if (config.opt_verbose > 0) {
                    cerr << "c FM ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") fourier motzkin" << endl;
                }
                if (status == l_Undef) {
                    fourierMotzkin.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    fourierMotzkin.printStatistics(cerr);
                }
                if (!data.ok()) {
                    status = l_False;
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-FM.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after FM");
                    scanCheck("after FM");
                });
                DOUT(if (printFM || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'f')) {
                    printFormula("after FM");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_rew) {
                if (config.opt_verbose > 0) {
                    cerr << "c rew ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") rewriting" << endl;
                }
                if (status == l_Undef) {
                    rewriter.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    rewriter.printStatistics(cerr);
                }
                if (!data.ok()) {
                    status = l_False;
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-REW.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after REW");
                    scanCheck("after REW");
                });
                DOUT(if (printREW || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'r')) {
                    printFormula("after REW");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_ee) { // before this technique nothing should be run that alters the structure of the formula (e.g. BVE;BVA)
                if (config.opt_verbose > 0) {
                    cerr << "c ee ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") equivalence elimination" << endl;
                }
                if (status == l_Undef) {
                    ee.process(data);
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    ee.printStatistics(cerr);
                }
                if (!data.ok()) {
                    status = l_False;
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-EE.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after EE");
                    scanCheck("after EE");
                });

                DOUT(if (printEE || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'e')) {
                    printFormula("after EE");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_unhide) {
                if (config.opt_verbose > 0) {
                    cerr << "c unhide ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") unhiding" << endl;
                }
                if (status == l_Undef) {
                    unhiding.process();
                }
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    unhiding.printStatistics(cerr);
                }
                if (!data.ok()) {
                    status = l_False;
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-UNHIDE.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(
                    if (printUnhide || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'g')) {
                        printFormula("after Unhiding");
                    });
                DOUT(if (config.opt_debug) {
                    checkLists("after UNHIDING");
                    scanCheck("after UNHIDING");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_hte) {
                if (config.opt_verbose > 0) {
                    cerr << "c hte ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") hidden tautology elimination" << endl;
                }
                if (status == l_Undef) {
                    hte.process(data);
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    hte.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-HTE.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after HTE");
                    scanCheck("after HTE");
                });
                DOUT(if (printHTE || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'h')) {
                    printFormula("after HTE");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_probe) {
                if (config.opt_verbose > 0) {
                    cerr << "c probe ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") probing" << endl;
                }
                if (status == l_Undef) {
                    probing.process();
                }
                if (!data.ok()) {
                    status = l_False;
                }
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    probing.printStatistics(cerr);
                }

                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-PROBE.cnf").c_str(), 0););
                DOUT(if (config.opt_debug) {
                    checkLists("after PROBE - before GC");
                    scanCheck("after PROBE - before GC");
                });
                DOUT(if (printProbe || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'p')) {
                    printFormula("after Probing");
                });
                data.checkGarbage(); // perform garbage collection
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_bve) {
                if (config.opt_verbose > 0) {
                    cerr << "c bve ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") bounded variable elimination" << endl;
                }
                if (status == l_Undef) {
                    status = bve.process(data);
                } // can change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    bve.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-BVE.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after BVE");
                    scanCheck("after BVE");
                });
                DOUT(if (printBVE || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'v')) {
                    printFormula("after BVE");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_bva) {
                if (config.opt_verbose > 0) {
                    cerr << "c bva ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") blocked variable addition" << endl;
                }
                if (status == l_Undef) {
                    bva.process();
                }
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    bva.printStatistics(cerr);
                }
                if (!data.ok()) {
                    status = l_False;
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-BVA.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after BVA");
                    scanCheck("after BVA");
                });
                DOUT(if (printBVA || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'w')) {
                    printFormula("after BVA");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_bce) {
                if (config.opt_verbose > 0) {
                    cerr << "c bce ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") blocked clause elimination" << endl;
                }
                if (status == l_Undef) {
                    bce.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    bce.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-BCE.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after BCE");
                    scanCheck("after BCE");
                });
                DOUT(if (printBCE || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'b')) {
                    printFormula("after BCE");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_la) {
                if (config.opt_verbose > 0) {
                    cerr << "c la ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") blocked clause elimination" << endl;
                }
                if (status == l_Undef) {
                    la.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    la.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-LA.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after LA");
                    scanCheck("after LA");
                });
                DOUT(if (printBCE || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'l')) {
                    printFormula("after LA");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_cce) {
                if (config.opt_verbose > 0) {
                    cerr << "c cce ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") (covered) clause elimination" << endl;
                }
                if (status == l_Undef) {
                    cce.process(data);
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    cce.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-CCE.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after CCE");
                    scanCheck("after CCE");
                }); // perform only if BCE finished the whole formula?!
                DOUT(if (printCCE || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'c')) {
                    printFormula("after CCE");
                });
            }
            if (!data.ok()) {
                break;
            } // stop here already

            if (config.opt_rate) {
                if (config.opt_verbose > 0) {
                    cerr << "c rate ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") resolution asymmetric tautology elimination" << endl;
                }
                if (status == l_Undef) {
                    rate.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    rate.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-RATE.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after RATE");
                    scanCheck("after RATE");
                }); // perform only if BCE finished the whole formula?!
                DOUT(if (printRATE || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 't')) {
                    printFormula("after RATE");
                });
            }

            if (!data.ok()) {
                break;
            } // stop here already
            if (config.opt_hbr) {
                if (config.opt_verbose > 0) {
                    cerr << "c hbr ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") hyper binary resolution" << endl;
                }
                if (status == l_Undef) {
                    hbr.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    hbr.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-HBR.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after HBR");
                    scanCheck("after HBR");
                }); // perform only if BCE finished the whole formula?!
            }

            if (!data.ok()) {
                break;
            } // stop here already
            if (config.opt_exp) {
                if (config.opt_verbose > 0) {
                    cerr << "c exp ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") experimental techniques" << endl;
                }
                if (status == l_Undef) {
                    experimental.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    experimental.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-EXP.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after EXP");
                    scanCheck("after EXP");
                }); // perform only if BCE finished the whole formula?!
            }

            if (!data.ok()) {
                break;
            } // stop here already
            if (config.opt_modprep) {
                if (config.opt_verbose > 0) {
                    cerr << "c modprep ..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") modprep techniques" << endl;
                }
                if (status == l_Undef) {
                    modprep.process();
                } // cannot change status, can generate new unit clauses
                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    modprep.printStatistics(cerr);
                }
                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-MODPREP.cnf").c_str(), 0););
                data.checkGarbage(); // perform garbage collection

                DOUT(if (config.opt_debug) {
                    checkLists("after MODPREP");
                    scanCheck("after MODPREP");
                }); // perform only if BCE finished the whole formula?!
            }

            if (!data.ok()) {
                break;
            } // stop here already
            if (config.opt_backbone) {
                if (config.opt_verbose > 0) {
                    cerr << "c backbone simplification..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") backbone simplification" << endl;
                }

                backbone.process();
                if (!data.ok()) {
                    status = l_False;
                }

                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    backbone.printStatistics(cerr);
                }

                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-BACKBONE.cnf").c_str(), 0););
                DOUT(if (config.opt_debug) {
                    checkLists("after BACKBONE - before GC");
                    scanCheck("after BACKBONE - before GC");
                });
                DOUT(if (printBackbone || config.opt_debug ||
                         (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'p')) {
                    printFormula("after Backbone");
                });
                data.checkGarbage(); // perform garbage collection
            }

            if (!data.ok()) {
                break;
            } // stop here already
            if (config.opt_be) {
                if (config.opt_verbose > 0) {
                    cerr << "c bipartition and elimination..." << endl;
                }
                if (config.opt_verbose > 4) {
                    cerr << "c coprocessor(" << data.ok() << ") bipartition and elimination" << endl;
                }

                be.process();
                if (!data.ok()) {
                    status = l_False;
                }

                if (config.opt_verbose > 1) {
                    printStatistics(cerr);
                    be.printStatistics(cerr);
                }

                DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                         outputFormula(string(string(config.stepbystepoutput) + "-BIPARTITIONELIMINATION.cnf").c_str(), 0););
                DOUT(if (config.opt_debug) {
                    checkLists("after BIPARTITIONELIMINATION - before GC");
                    scanCheck("after BIPARTITIONELIMINATION - before GC");
                });
                DOUT(if (printBe || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'p')) {
                    printFormula("after Bipartition and Elimination");
                });
                data.checkGarbage(); // perform garbage collection
            }

            //
            // end of simplification iteration
            //
            iterTime = cpuTime() - iterTime;
            DOUT(if (config.opt_verbose > 0 || config.opt_debug) cerr << "c used time in interation " << ppIteration << "  : " << iterTime << " s"
                                                                      << endl;);
        }

        if (config.opt_addRedBins) {
            if (config.opt_verbose > 0) {
                cerr << "c add2 ..." << endl;
            }
            resolving.process(true);
            if (config.opt_verbose > 1) {
                printStatistics(cerr);
                resolving.printStatistics(cerr);
            }
            DOUT(if ((const char*)config.stepbystepoutput != nullptr)
                     outputFormula(string(string(config.stepbystepoutput) + "-ADD2.cnf").c_str(), 0););
            DOUT(if (printAddRedBin || config.opt_debug || (config.printAfter != 0 && strlen(config.printAfter) > 0 && config.printAfter[0] == 'a'))
                     printFormula("after Add2"););
        }

        if (config.opt_sls) {
            if (config.opt_verbose > 0) {
                cerr << "c sls ..." << endl;
            }
            if (config.opt_verbose > 4) {
                cerr << "c coprocessor sls" << endl;
            }
            if (status == l_Undef) {
                bool solvedBySls =
                    sls.solve(data.getClauses(), config.opt_sls_flips == -1
                                                     ? (uint64_t)4000000000000000
                                                     : (uint64_t)config.opt_sls_flips); // cannot change status, can generate new unit clauses
                cerr << "c sls returned " << solvedBySls << endl;
                if (solvedBySls) {
                    cerr << "c formula was solved with SLS!" << endl;
                    cerr // << endl
                        << "c ================================" << endl
                        << "c  use the result of SLS as model " << endl
                        << "c ================================" << endl;
                }
                if (solvedBySls || config.opt_sls_phase) {
                    for (Var v = 0; v < data.nVars(); ++v) {
                        solver->varFlags[v].polarity = sls.getModelPolarity(v) == 1 ? 1 : 0;
                    } // minisat uses sign instead of polarity!
                }
            }
            if (!solver->okay()) {
                status = l_False;
            }
        }

        // clear / update clauses and learnts vectores and statistical counters
        // attach all clauses to their watchers again, call the propagate method to get into a good state again
        if (config.opt_verbose > 4) {
            cerr << "c coprocessor re-setup solver" << endl;
        }
        if (data.ok()) {
            if (data.hasToPropagate())
            // if( config.opt_up )
            {
                propagation.process(data);
            }
            // else cerr << "c should apply UP" << endl;
        }

        DOUT(if (config.opt_check) cerr << "present clauses: orig: " << solver->clauses.size() << " learnts: " << solver->learnts.size()
                                        << " solver.ok: " << data.ok() << endl;);

        // dense only if not inprocessing, or if enabled explicitly
        if (config.opt_dense && (!data.isInprocessing() || config.opt_dense_inprocess)) {
            // do as very last step -- not nice, if there are units on the trail!
            dense.compress(false);
        }

        moh.cont();

        DOUT(if (config.opt_check) fullCheck("final check"););

        destroyTechniques();

        if (data.ok()) {
            reSetupSolver();
        }

        if (config.opt_verbose > 5) {
            printSolver(cerr, 4);
        } // print all details of the solver
        if (config.opt_verbose > 4) {
            printFormula("after full simplification");
        }

        mc.stop();
        moh.stop();

        if (config.opt_printStats && solver->verbosity != 0) {

            printStatistics(cerr);
            propagation.printStatistics(cerr);
            subsumption.printStatistics(cerr);
            ee.printStatistics(cerr);
            if (config.opt_hte) {
                hte.printStatistics(cerr);
            }
            if (config.opt_bve) {
                bve.printStatistics(cerr);
            }
            if (config.opt_bva) {
                bva.printStatistics(cerr);
            }
            if (config.opt_probe) {
                probing.printStatistics(cerr);
            }
            if (config.opt_backbone) {
                backbone.printStatistics(cerr);
            }
            if (config.opt_be) {
                be.printStatistics(cerr);
            }
            if (config.opt_unhide) {
                unhiding.printStatistics(cerr);
            }
            if (config.opt_ternResolve || config.opt_addRedBins) {
                resolving.printStatistics(cerr);
            }
            if (config.opt_xor) {
                xorReasoning.printStatistics(cerr);
            }
            if (config.opt_sls) {
                sls.printStatistics(cerr);
            }
            if (config.opt_bce) {
                bce.printStatistics(cerr);
            }
            if (config.opt_la) {
                la.printStatistics(cerr);
            }
            if (config.opt_cce) {
                cce.printStatistics(cerr);
            }
            if (config.opt_rate) {
                rate.printStatistics(cerr);
            }
            if (config.opt_hbr) {
                hbr.printStatistics(cerr);
            }
            if (config.opt_exp) {
                experimental.printStatistics(cerr);
            }
            if (config.opt_modprep) {
                modprep.printStatistics(cerr);
            }
            if (config.opt_ent) {
                entailedRedundant.printStatistics(cerr);
            }
            if (config.opt_rew) {
                rewriter.printStatistics(cerr);
            }
            if (config.opt_FM) {
                fourierMotzkin.printStatistics(cerr);
            }
            if (config.opt_dense) {
                dense.printStatistics(cerr);
            }
            if (config.opt_symm) {
                symmetry.printStatistics(cerr);
            }
        }

        // destroy preprocessor data
        if (config.opt_verbose > 4) {
            cerr << "c coprocessor free data structures" << endl;
        }
        data.destroy();

        if (!data.ok()) {
            status = l_False;
        } // to fall back, if a technique forgets to do this

        cout.flush();
        cerr.flush();

        return status;
    }

    lbool Preprocessor::performSimplificationScheduled(string techniques) {
        if (!config.opt_enabled) {
            return l_Undef;
        }
        if (config.opt_verbose > 4) {
            cerr << "c start simplifying with coprocessor" << endl;
        }

        if (formulaVariables == -1) {
            if (config.opt_verbose > 2) {
                cerr << "c initialize CP3 with " << solver->nVars() << " variables " << endl;
            }
            formulaVariables = solver->nVars();
        }

        //
        // scan for techniques
        //
        vector<string> grammar;
        grammar.push_back(string());

        char c = 0;
        uint32_t pos = 0;
        uint32_t line = 0;
        uint32_t level = 0;
        while (pos < techniques.size()) {
            c = techniques[pos++];
            if (c == '[') {
                level++;
                if (level > 1) {
                    cerr << "c parser at " << pos - 1 << " warning: too many '[' levels" << endl;
                    exit(-2);
                } else {
                    if (grammar[line].size() > 0) {
                        grammar.push_back(string());
                        line++;
                    }
                }
                continue;
            }
            if (c == ']') {
                if (level < 1) {
                    cerr << "c parser at " << pos - 1 << " warning: ignore ']'" << endl;
                    exit(-2);
                }
                if (level == 1) {
                    // star behing brackets?
                    if (pos < techniques.size() && techniques[pos] == '+' && grammar[line].size() > 0) {
                        grammar[line] += "+";
                        pos++;
                    }
                    if (grammar[line].size() > 0) {
                        grammar.push_back(string());
                        line++;
                    }
                }
                level--;
                continue;
            }
            if (c == '+') {
                if (level == 0) {
                    cerr << "c parser at " << pos - 1 << " ignore '+' at level 0" << endl;
                    continue;
                }
            }
            grammar[line] += c;
        }

        if (config.opt_verbose > 0) {
            cerr << "c parsed grammar:" << endl;
            for (uint32_t i = 0; i < grammar.size(); ++i) {
                cerr << "c " << grammar[i] << endl;
            }
        }

        lbool status = l_Undef;
        if (grammar.size() == 0) {
            cerr << "c warning: set of techniques empty - abort" << endl;
            return status;
        }

        MethodClock mc(data.isInprocessing() ? ipTime : ppTime); // measure the time for this iteration!
        MethodClock moh(overheadTime);

        // first, remove all satisfied clauses
        if (!solver->simplify()) {
            cout.flush();
            cerr.flush();
            return l_False;
        }

        // delete clauses from solver

        DOUT(if (config.opt_check) cerr << "present clauses: orig: " << solver->clauses.size() << " learnts: " << solver->learnts.size() << endl;);
        thisClauses = solver->clauses.size();
        thisLearnts = solver->learnts.size();

        cleanSolver();
        // initialize techniques
        data.init(solver->nVars());
        data.resetPPhead(); // to see all unit propagations also in CP, even if they have been processed inside the solver already

        if (config.opt_shuffle) {
            shuffle();
        }

        DOUT(if (config.opt_check) checkLists("before initializing"););
        initializePreprocessor();
        DOUT(if (config.opt_check) checkLists("after initializing"););

        if (config.opt_verbose > 4) {
            cerr << "c coprocessor finished initialization" << endl;
        }

        moh.stop();

        //
        //  perform scheduled preprocessing
        //
        sortClauses();
        uint32_t currentLine = 0;
        uint32_t currentPosition = 0;
        uint32_t currentSize = grammar[currentLine].size();
        bool change = false;

        while (status == l_Undef && (currentLine < grammar.size() || currentPosition < currentSize)) {

            if (currentPosition >= currentSize) { // start with next line, if current line has been evaluated
                if (config.opt_verbose > 1) {
                    cerr << "c reached end of line " << currentLine << endl;
                }
                currentLine++;
                if (currentLine < grammar.size()) {
                    currentSize = grammar[currentLine].size();
                    currentPosition = 0;
                    if (config.opt_verbose > 1) {
                        cerr << "c new data: current line: " << grammar[currentLine] << " pos: " << currentPosition << endl;
                    }
                    continue;
                } else {
                    break;
                }
            }

            char execute = grammar[currentLine][currentPosition];
            if (config.opt_verbose > 0) {
                cerr << "c current line: " << grammar[currentLine] << " pos: " << currentPosition << " execute=" << execute << endl;
            }
            if (execute == '+') { // if there is a star in a line and there has been change,
                if (change) {
                    currentPosition = 0;
                } else {
                    currentPosition++;
                }
                continue; // start current line in grammar again!
            }

            if (currentLine >= grammar.size()) {
                break;
            } // stop if all lines of the grammar have been evaluated

            if (currentPosition == 0) { // if the line is started from scratch, reset the change flag
                change = false;
            }

            // in the next iteration, the position is increased
            currentPosition++;

            // unit propagation has letter "u"
            if (execute == 'u' && config.opt_up && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c up" << endl;
                }
                propagation.process(data, true);
                change = propagation.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c UP changed formula: " << change << endl;
                }
            }

            // subsumption has letter "s"
            else if (execute == 's' && config.opt_subsimp && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c subsimp" << endl;
                }
                subsumption.process();
                change = subsumption.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c Subsumption changed formula: " << change << endl;
                }
            }

            // addRed2 "a"
            else if (execute == 'a' && config.opt_addRedBins && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c addRed2" << endl;
                }
                resolving.process(true);
                change = resolving.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c AddRed2 changed formula: " << change << endl;
                }
            }

            // ternRes "3"
            else if (execute == '3' && config.opt_ternResolve && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c ternRes" << endl;
                }
                resolving.process(false);
                change = resolving.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c TernRes changed formula: " << change << endl;
                }
            }

            // xorReasoning "x"
            else if (execute == 'x' && config.opt_xor && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c xor" << endl;
                }
                xorReasoning.process();
                change = xorReasoning.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c XOR changed formula: " << change << endl;
                }
            }

            // probing "p"
            else if (execute == 'p' && config.opt_probe && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c probing" << endl;
                }
                probing.process();
                change = probing.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c Probing changed formula: " << change << endl;
                }
            }

            // unhide "g"
            else if (execute == 'g' && config.opt_unhide && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c unhiding" << endl;
                }
                unhiding.process();
                change = unhiding.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c Unhiding changed formula: " << change << endl;
                }
            }

            // bva "w"
            else if (execute == 'w' && config.opt_bva && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c bva" << endl;
                }
                bva.process();
                change = bva.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c BVA changed formula: " << change << endl;
                }
            }

            // bve "v"
            else if (execute == 'v' && config.opt_bve && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c bve" << endl;
                }
                bve.process(data);
                change = bve.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c BVE changed formula: " << change << endl;
                }
            }

            // ee "e"
            else if (execute == 'e' && config.opt_ee && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c ee" << endl;
                }
                ee.process(data);
                change = ee.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c EE changed formula: " << change << endl;
                }
            }

            // bce "b"
            else if (execute == 'b' && config.opt_bce && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c bce" << endl;
                }
                bce.process();
                change = bce.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c BCE changed formula: " << change << endl;
                }
            }

            // literaladdition "l"
            else if (execute == 'l' && config.opt_la && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c la" << endl;
                }
                la.process();
                change = la.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c LA changed formula: " << change << endl;
                }
            }

            // entailedRedundant "1"
            else if (execute == '1' && config.opt_ent && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c ent" << endl;
                }
                entailedRedundant.process();
                change = entailedRedundant.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c ENT changed formula: " << change << endl;
                }
            }

            // cce "c"
            else if (execute == 'c' && config.opt_cce && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c cce" << endl;
                }
                cce.process(data);
                change = cce.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c CCE changed formula: " << change << endl;
                }
            }

            // rate "t"
            else if (execute == 't' && config.opt_rate && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c rate" << endl;
                }
                rate.process();
                change = rate.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c RATE changed formula: " << change << endl;
                }
            }

            // HBR "H"
            else if (execute == 'H' && config.opt_hbr && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c HBR" << endl;
                }
                hbr.process();
                change = hbr.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c HBR changed formula: " << change << endl;
                }
            }

            // EXP "X"
            else if (execute == 'X' && config.opt_exp && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c EXP" << endl;
                }
                experimental.process();
                change = experimental.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c EXP changed formula: " << change << endl;
                }
            }

            // MODPREP "m"
            else if (execute == 'm' && config.opt_modprep && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c MODPREP" << endl;
                }
                modprep.process();
                change = modprep.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c MODPREP changed formula: " << change << endl;
                }
            }

            // hte "h"
            else if (execute == 'h' && config.opt_hte && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c hte" << endl;
                }
                hte.process(data);
                change = hte.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c HTE changed formula: " << change << endl;
                }
            }

            // rewriting "r"
            else if (execute == 'r' && config.opt_rew && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c rew" << endl;
                }
                rewriter.process();
                change = rewriter.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c REW changed formula: " << change << endl;
                }
            }

            // fourier motzkin "f"
            else if (execute == 'f' && config.opt_FM && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c fm" << endl;
                }
                fourierMotzkin.process();
                change = fourierMotzkin.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c FM changed formula: " << change << endl;
                }
            }

            // f "d"
            else if (execute == 'd' && config.opt_dense && status == l_Undef && data.ok()) {
                if (config.opt_verbose > 2) {
                    cerr << "c dense" << endl;
                }
                dense.compress(true);
                change = dense.appliedSomething() || change;
                if (config.opt_verbose > 1) {
                    cerr << "c Dense changed formula: " << change << endl;
                }
            }

            // none left so far
            else {
                char name[2];
                name[0] = execute;
                name[1] = 0;
                DOUT(cerr << "c warning: cannot execute technique related to  " << string(name) << endl;);
            }

            // perform afte reach call
            DOUT(if (config.opt_debug) {
                scanCheck("after iteration");
                printFormula("after iteration");
            });
            data.checkGarbage(); // perform garbage collection
            if (config.opt_verbose > 3) {
                printStatistics(cerr);
            }
            if (config.opt_verbose > 4) {
                cerr << "c intermediate formula: " << endl;
                for (int i = 0; i < solver->trail.size(); ++i) {
                    cerr << " " << solver->trail[i] << endl;
                }
                for (int i = 0; i < data.getClauses().size(); ++i) {
                    cerr << "(" << i << ") (" << data.getClauses()[i] << ")";
                    if (ca[data.getClauses()[i]].mark() != 0) {
                        cerr << " (ign) ";
                    }
                    if (ca[data.getClauses()[i]].can_be_deleted() != 0) {
                        cerr << " (del) ";
                    }
                    cerr << " " << ca[data.getClauses()[i]] << endl;
                }
            }
        }

        if (config.opt_verbose > 4) {
            cerr << "c final formula: " << endl;
            for (int i = 0; i < solver->trail.size(); ++i) {
                cerr << " " << solver->trail[i] << endl;
            }
            for (int i = 0; i < data.getClauses().size(); ++i) {
                cerr << "(" << i << ") (" << data.getClauses()[i] << ")";
                if (ca[data.getClauses()[i]].mark() != 0) {
                    cerr << " (ign) ";
                }
                if (ca[data.getClauses()[i]].can_be_deleted() != 0) {
                    cerr << " (del) ";
                }
                cerr << " " << ca[data.getClauses()[i]] << endl;
            }
        }

        if (false && config.opt_sls) { // TODO: decide whether this should be possible!
            if (config.opt_verbose > 0) {
                cerr << "c sls ..." << endl;
            }
            if (config.opt_verbose > 4) {
                cerr << "c coprocessor sls" << endl;
            }
            if (status == l_Undef) {
                bool solvedBySls =
                    sls.solve(data.getClauses(), config.opt_sls_flips == -1
                                                     ? (uint64_t)4000000000000000
                                                     : (uint64_t)config.opt_sls_flips); // cannot change status, can generate new unit clauses
                if (solvedBySls) {
                    cerr << "c formula was solved with SLS!" << endl;
                    cerr // << endl
                        << "c ================================" << endl
                        << "c  use the result of SLS as model " << endl
                        << "c ================================" << endl;
                }
                if (solvedBySls || config.opt_sls_phase) {
                    for (Var v = 0; v < data.nVars(); ++v) {
                        solver->varFlags[v].polarity = sls.getModelPolarity(v) == 1 ? 1 : 0;
                    } // minisat uses sign instead of polarity!
                }
            }
            if (!solver->okay()) {
                status = l_False;
            }
        }

        // clear / update clauses and learnts vectores and statistical counters
        // attach all clauses to their watchers again, call the propagate method to get into a good state again
        if (config.opt_verbose > 4) {
            cerr << "c coprocessor re-setup solver" << endl;
        }
        if (data.ok()) {
            if (data.hasToPropagate())
            // if( config.opt_up )
            {
                propagation.process(data);
            }
            // else cerr << "c should apply UP" << endl;
        }

        DOUT(if (config.opt_check) cerr << "present clauses: orig: " << solver->clauses.size() << " learnts: " << solver->learnts.size()
                                        << " solver.ok: " << data.ok() << endl;);

        // dense only if not inprocessing, or if enabled explicitly
        if (config.opt_dense && (!data.isInprocessing() || config.opt_dense_inprocess)) {
            // do as very last step -- not nice, if there are units on the trail!
            dense.compress(false);
        }

        moh.cont();
        DOUT(if (config.opt_check) fullCheck("final check"););

        destroyTechniques();

        if (data.ok()) {
            reSetupSolver();
        }

        if (config.opt_verbose > 5) {
            printSolver(cerr, 4);
        } // print all details of the solver
        if (config.opt_verbose > 4) {
            printFormula("after full simplification");
        }
        moh.stop();
        mc.stop();

        if (config.opt_printStats) {

            printStatistics(cerr);
            propagation.printStatistics(cerr);
            subsumption.printStatistics(cerr);
            ee.printStatistics(cerr);
            if (config.opt_hte) {
                hte.printStatistics(cerr);
            }
            if (config.opt_bve) {
                bve.printStatistics(cerr);
            }
            if (config.opt_bva) {
                bva.printStatistics(cerr);
            }
            if (config.opt_probe) {
                probing.printStatistics(cerr);
            }
            if (config.opt_backbone) {
                backbone.printStatistics(cerr);
            }
            if (config.opt_be) {
                be.printStatistics(cerr);
            }
            if (config.opt_unhide) {
                unhiding.printStatistics(cerr);
            }
            if (config.opt_ternResolve || config.opt_addRedBins) {
                resolving.printStatistics(cerr);
            }
            if (config.opt_xor) {
                xorReasoning.printStatistics(cerr);
            }
            if (config.opt_sls) {
                sls.printStatistics(cerr);
            }
            if (config.opt_bce) {
                bce.printStatistics(cerr);
            }
            if (config.opt_la) {
                la.printStatistics(cerr);
            }
            if (config.opt_cce) {
                cce.printStatistics(cerr);
            }
            if (config.opt_rate) {
                rate.printStatistics(cerr);
            }
            if (config.opt_hbr) {
                hbr.printStatistics(cerr);
            }
            if (config.opt_exp) {
                experimental.printStatistics(cerr);
            }
            if (config.opt_modprep) {
                modprep.printStatistics(cerr);
            }
            if (config.opt_ent) {
                entailedRedundant.printStatistics(cerr);
            }
            if (config.opt_rew) {
                rewriter.printStatistics(cerr);
            }
            if (config.opt_FM) {
                fourierMotzkin.printStatistics(cerr);
            }
            if (config.opt_dense) {
                dense.printStatistics(cerr);
            }
            if (config.opt_symm) {
                symmetry.printStatistics(cerr);
            }
        }

        // destroy preprocessor data
        if (config.opt_verbose > 4) {
            cerr << "c coprocessor free data structures" << endl;
        }
        data.destroy();

        if (!data.ok()) {
            status = l_False;
        } // to fall back, if a technique forgets to do this

        cout.flush();
        cerr.flush();

        return status;
    }

    lbool Preprocessor::preprocess() {
        data.preprocessing();
        const bool wasDoingER = solver->getExtendedResolution();

        // do not preprocess, if the formula is considered to be too large!
        if (!data.unlimited() && (data.nVars() > config.opt_cp3_vars || data.getClauses().size() + data.getLearnts().size() > config.opt_cp3_cls ||
                                  data.nTotLits() > config.opt_cp3_lits)) {
            return l_Undef;
        }

        if (config.opt_symm && config.opt_enabled) { // do only if preprocessor is enabled
            symmetry.process();
            if (config.opt_verbose > 1) {
                printStatistics(cerr);
                symmetry.printStatistics(cerr);
            }
        }

        lbool ret = l_Undef;
        if (config.opt_ptechs && string(config.opt_ptechs).size() > 0) {
            ret = performSimplificationScheduled(string(config.opt_ptechs));
        } else {
            ret = performSimplification();
        }

        if (config.opt_exit_pp > 0) {     // exit?
            if (config.opt_exit_pp > 1) { // print? TODO: have a method for this output!
                int cls = 0;
                for (int i = 0; i < data.getTrail().size(); ++i) {
                    cls++;
                }
                for (int i = 0; i < data.getClauses().size(); ++i)
                    if (!ca[data.getClauses()[i]].can_be_deleted()) {
                        cls++;
                    }
                (config.opt_exit_pp == 2 ? cerr : cout) << "p cnf " << data.nVars() << " " << cls << endl;
                for (int i = 0; i < data.getTrail().size(); ++i) {
                    (config.opt_exit_pp == 2 ? cerr : cout) << data.getTrail() << " 0" << endl;
                }
                for (int i = 0; i < data.getClauses().size(); ++i) {
                    if (ca[data.getClauses()[i]].can_be_deleted()) {
                        continue;
                    }
                    for (int j = 0; j < ca[data.getClauses()[i]].size(); ++j) {
                        (config.opt_exit_pp == 2 ? cerr : cout) << ca[data.getClauses()[i]][j] << " ";
                    }
                    (config.opt_exit_pp == 2 ? cerr : cout) << "0" << endl;
                }
            }
            exit(0);
        } else {
            solver->setExtendedResolution(wasDoingER);
            return ret;
        }
    }

    bool Preprocessor::wantsToInprocess() {
        // if no inprocesing enabled, do not do it!
        if (!config.opt_inprocess) {
            return false;
        }

        if (config.opt_verbose > 3) {
            cerr << "c check " << lastInpConflicts << " and " << (int)config.opt_inprocessInt << " vs " << solver->conflicts << endl;
        }
        if (lastInpConflicts + config.opt_inprocessInt > solver->conflicts) {
            return false;
        }
        return true;
    }

    lbool Preprocessor::inprocess() {
        // if no inprocesing enabled, do not do it!
        if (!config.opt_inprocess) {
            return l_Undef;
        }

        // do not inprocess, if the formula is considered to be too large!
        if (!data.unlimited() &&
            (data.nVars() > config.opt_cp3_ipvars || data.getClauses().size() + data.getLearnts().size() > config.opt_cp3_ipcls ||
             data.nTotLits() > config.opt_cp3_iplits)) {
            return l_Undef;
        }

        // TODO: do something before preprocessing? e.g. some extra things with learned / original clauses
        if (config.opt_inprocess) {

            // increase the distance to the next inprocessing according to the parameter
            config.opt_inprocessInt = config.opt_inprocessInt + config.opt_inpIntInc;

            freezeSearchVariables(); // take care that special variables of the search are not destroyed during simplification

            /* make sure the solver is at level 0 - not guarantueed with partial restarts!*/
            solver->cancelUntil(0);

            if (config.opt_verbose > 3) {
                cerr << "c start inprocessing after another " << solver->conflicts - lastInpConflicts << endl;
            }
            data.inprocessing();
            const bool wasDoingER = solver->getExtendedResolution();

            if (inprocessings == 0 && config.opt_remL_inp) {
                int removed = 0;
                for (int i = 0; i < data.getLearnts().size(); ++i) {
                    Clause& c = ca[data.getLearnts()[i]];
                    if (c.mark() == 0) {
                        data.addToProof(c, true); // remove clause from proof
                        c.mark(1);                // mark the clause to be not used next time
                        removed++;
                        c.set_delete(true); // mark clause as deleted for coprocessor as well
                    }
                }
            }
            inprocessings++; // count number of inprocessings

            if (config.opt_randInp) {
                data.randomized();
            }
            if (config.opt_inc_inp) {
                giveMoreSteps();
            }

            lbool ret = l_Undef;
            if (config.opt_itechs && string(config.opt_itechs).size() > 0) {
                ret = performSimplificationScheduled(string(config.opt_itechs));
            } else {
                ret = performSimplification();
            }

            lastInpConflicts = solver->conflicts;
            if (config.opt_verbose > 4) {
                cerr << "c finished inprocessing " << endl;
            }

            meltSearchVariables(); // undo restriction for these variables

            solver->setExtendedResolution(wasDoingER);

            return ret;
        } else {
            return l_Undef;
        }
    }

    void Preprocessor::giveMoreSteps() {
        subsumption.giveMoreSteps();
        propagation.giveMoreSteps();
        hte.giveMoreSteps();
        bve.giveMoreSteps();
        bva.giveMoreSteps();
        cce.giveMoreSteps();
        rate.giveMoreSteps();
        ee.giveMoreSteps();
        unhiding.giveMoreSteps();
        probing.giveMoreSteps();
        backbone.giveMoreSteps();
        resolving.giveMoreSteps();
        rewriter.giveMoreSteps();
        fourierMotzkin.giveMoreSteps();
        rate.giveMoreSteps();
        hbr.giveMoreSteps();
        experimental.giveMoreSteps();
        modprep.giveMoreSteps();
    }

    lbool Preprocessor::preprocessScheduled() {
        // TODO execute preprocessing techniques in specified order
        return l_Undef;
    }

    void Preprocessor::freezeExtern(int lit) {
        if (lit > 0) {
            data.setNotTouch(lit - 1);
        } else if (lit < 0) {
            data.setNotTouch(-lit - 1);
        }
    }

    void Preprocessor::dumpFormula(std::vector<int>& outputFormula) {
        outputFormula.clear(); // remove everything that has been there before
                               //  cerr << "c move " << data.getTrail() << " units, and about " << data.getClauses().size() << " clauses." << endl;
        // top level unit clauses
        for (int i = 0; i < data.getTrail().size(); ++i) {
            const Lit& l = data.getTrail()[i];
            const int nl = sign(l) ? (-var(l) - 1) : (var(l) + 1);
            //  cerr << "c turn lit " << l << " into " << nl << endl;
            outputFormula.push_back(nl);
            outputFormula.push_back(0);
        }
        for (int i = 0; i < data.getClauses().size(); ++i) {
            const Clause& c = ca[data.getClauses()[i]];
            if (c.can_be_deleted()) {
                continue;
            } // do only process valid clauses!
            for (int j = 0; j < c.size(); ++j) {
                const Lit& l = c[j];
                const int nl = sign(l) ? (-var(l) - 1) : (var(l) + 1);
                //   cerr << "c turn lit " << l << " into " << nl << endl;
                outputFormula.push_back(nl);
            }
            outputFormula.push_back(0);
        }
    }

    int Preprocessor::importLit(const int& l) const {
        // conversion to inner representation, check move value, return value
        const Lit lit = l > 0 ? mkLit(l - 1, false) : mkLit(-l - 1, true);
        const Lit compressed = solver->compression.importLit(lit);

        if (compressed == lit_Error) {
            return 0;
        } else if (sign(compressed)) {
            return -var(compressed) - 1;
        } else {
            return var(compressed) + 1;
        }
    }

    Lit Preprocessor::importLit(const Lit& lit) const {
        return solver->compression.importLit(lit);
    }

    void Preprocessor::printStatistics(ostream& stream) {
        stream << "c [STAT] CP3 " << ppTime.getCpuTime() << " s-ppTime, " << ipTime.getCpuTime() << " s-ipTime, " << ppTime.getWallClockTime()
               << " s-ppwTime, " << ipTime.getWallClockTime() << " s-ipwTime, " << memUsedPeak() << " MB, " << (data.ok() ? "ok " : "notok ")
               << overheadTime.getCpuTime() << " s-ohTime, " << endl;

        stream << "c [STAT] CP3(2) " << data.getClauses().size() << " cls, " << data.getLearnts().size() << " learnts, "
               << thisClauses - data.getClauses().size() << " rem-cls, " << thisLearnts - data.getLearnts().size() << " rem-learnts, " << endl;
    }

    void Preprocessor::extendModel(vec<lbool>& model) {
        if (config.opt_verbose > 0) {
            cerr << "c extendModel with " << model.size() << " variables" << endl;
        }

        // for the most recent changes that have not reached a dense.compress yet
        if (config.opt_dense) {
            // order is important!
            dense.decompress(model); // if model has not been compressed before, nothing has to be done!
        }

        if (config.opt_verbose > 0) {
            cerr << "c formula variables: " << formulaVariables << " model: " << model.size() << endl;
        }
        if (formulaVariables > model.size()) {
            model.growTo(formulaVariables);
        }
        // cerr << "c run data extend model" << endl;
        data.extendModel(model);

        // get back the old number of variables inside the model, to be able to unshuffle!
        if (formulaVariables != -1 && formulaVariables < model.size()) {
            DOUT(cerr << "c model size before: " << model.size() << " with formula variables: " << formulaVariables
                      << " and shrink: " << model.size() - formulaVariables << endl;);
            model.shrink_(model.size() - formulaVariables);
            DOUT(cerr << "c model size afterwards: " << model.size() << endl;);
        }
        if (config.opt_shuffle) {
            DOUT(cerr << "c unshuffle model " << model.size() << endl;);
            unshuffle(model);
        }
    }

    void Preprocessor::initializePreprocessor() {
        thisClauses = 0;
        thisLearnts = 0;

        for (int p = 0; p < 2; ++p) {
            vec<CRef>& clss = (p == 0) ? data.getClauses() : data.getLearnts();
            int& thisClss = (p == 0) ? thisClauses : thisLearnts;

            for (int i = 0; i < clss.size(); ++i) {
                const CRef cr = clss[i];
                Clause& c = ca[cr];
                // assert( c.mark() == 0 && "mark of a clause has to be 0 before being put into preprocessor" );
                if (ca[cr].mark() != 0) {
                    continue;
                } // do not use any specially marked clauses!
                // cerr << "c process clause " << cr << endl;

                if (c.size() == 0) {
                    data.setFailed();
                    break;
                } else if (c.size() == 1) {
                    if (data.enqueue(c[0]) == l_False) {
                        break;
                    }
                    c.set_delete(true);
                    thisClss++;
                } else {
                    if (p == 1 && c.isCoreClause()) {
                        continue;
                    } // do not add core clauses twice (only for clauses)
                    assert((p == 1 || c.isCoreClause() || !c.learnt()) &&
                           "core learnts should be in the clauses vector, usual learnts should be in the learnts vector");
#ifndef NDEBUG
                    data.addClause(cr, config.opt_check);
#else
                    data.addClause(cr);
#endif
                    // TODO: decide for which techniques initClause in not necessary!
                    subsumption.initClause(cr);
                    propagation.initClause(cr);
                    hte.initClause(cr);
                    cce.initClause(cr);
                    clss[thisClss++] = cr; // keep this clause!
                }
            }
            clss.shrink_(clss.size() - thisClss); // remove redundant clauses from vector!
        }

        if (config.opt_whiteList != 0 && string(config.opt_whiteList).size() != 0) {
            // parse white list file, and set all existing variables to "do not touch"

            VarFileParser vfp(string(config.opt_whiteList));
            vector<int> whiteVariables;
            vfp.extract(whiteVariables);
            int lockedWhiteVars = 0;
            for (int i = 0; i < whiteVariables.size(); ++i) {
                const Var v = whiteVariables[i] > 0 ? whiteVariables[i] : -whiteVariables[i];
                if (v - 1 >= data.nVars()) {
                    continue;
                } // other file might contain more variables
                Lit thisL = mkLit(v - 1, whiteVariables[i] < 0);
                data.setNotTouch(var(thisL));
                lockedWhiteVars++;
            }

            if (config.opt_verbose > 0) {
                cerr << "c locked " << lockedWhiteVars << " white variables" << endl;
            }

            // do not repeat this process in the future -> delete pointer to file name
            config.opt_whiteList = "";
        }
        /*
        uint32_t clausesSize = (*solver).clauses.size();

        for (int i = 0; i < clausesSize; ++i)
        {
          const CRef cr = solver->clauses[i];
          Clause& c = ca[cr];
          // assert( c.mark() == 0 && "mark of a clause has to be 0 before being put into preprocessor" );
          if( ca[cr].mark() != 0  ) continue; // do not use any specially marked clauses!
          // cerr << "c process clause " << cr << endl;

          if( c.size() == 0 ) {
            data.setFailed();
            break;
          } else if (c.size() == 1 ) {
            if( data.enqueue(c[0]) == l_False ) break;
            c.set_delete(true);
            thisClauses ++;
          } else {
            data.addClause( cr, config.opt_check );
            // TODO: decide for which techniques initClause in not necessary!
            subsumption.initClause( cr );
            propagation.initClause( cr );
            hte.initClause( cr );
            cce.initClause( cr );
            thisClauses ++;
          }
        }

        clausesSize = solver->learnts.size();
        for (int i = 0; i < clausesSize; ++i)
        {
          const CRef cr = solver->learnts[i];
          Clause& c = ca[cr];
          // assert( c.mark() == 0 && "mark of a clause has to be 0 before being put into preprocessor" );
          // cerr << "c process learnt clause " << cr << endl;
          if( ca[cr].mark() != 0  ) continue; // do not use any specially marked clauses!
          if( c.size() == 0 ) {
            data.setFailed();
            break;
          } else if (c.size() == 1 ) {
            if( data.enqueue(c[0]) == l_False ) break;
            c.set_delete(true);
            thisLearnts++;
          } else {
            data.addClause( cr, config.opt_check );
            // TODO: decide for which techniques initClause in not necessary!
            subsumption.initClause( cr );
            propagation.initClause( cr );
            hte.initClause( cr );
            cce.initClause( cr );
            thisLearnts++;
          }
        }
        */
        // initialize techniques
        if (config.opt_bve) {
            bve.initializeTechnique(data);
        }
    }

    void Preprocessor::destroyTechniques() {
        DOUT(if (config.opt_verbose > 2) { cerr << "c destroy techniques" << endl; });
        // propagation.destroy();
        subsumption.destroy();
        ee.destroy();
        if (config.opt_hte) {
            hte.destroy();
        }
        if (config.opt_bve) {
            bve.destroy();
        }
        if (config.opt_bva) {
            bva.destroy();
        }
        if (config.opt_probe) {
            probing.destroy();
        }
        if (config.opt_backbone) {
            backbone.destroy();
        }
        if (config.opt_be) {
            be.destroy();
        }
        if (config.opt_unhide) {
            unhiding.destroy();
        }
        if (config.opt_ternResolve || config.opt_addRedBins) {
            resolving.destroy();
        }
        if (config.opt_xor) {
            xorReasoning.destroy();
        }
        if (config.opt_sls) {
            sls.destroy();
        }
        if (config.opt_bce) {
            bce.destroy();
        }
        if (config.opt_la) {
            la.destroy();
        }
        if (config.opt_cce) {
            cce.destroy();
        }
        if (config.opt_rate) {
            rate.destroy();
        }
        if (config.opt_ent) {
            entailedRedundant.destroy();
        }
        if (config.opt_hbr) {
            hbr.destroy();
        }
        if (config.opt_exp) {
            experimental.destroy();
        }
        if (config.opt_modprep) {
            modprep.destroy();
        }
        if (config.opt_rew) {
            rewriter.destroy();
        }

        // re-init replacedBy information during next round
        assert((!data.ok() || data.getEquivalences().size() == 0) &&
               "all equivalences should have been processed if the formula is not known to be unsatisfiable!");
    }

    void Preprocessor::cleanSolver() {
        solver->watches.cleanAll();

        // clear all watches!
        for (int v = 0; v < solver->nVars(); v++)
            for (int s = 0; s < 2; s++) {
                solver->watches[mkLit(v, s)].clear();
            }

        solver->learnts_literals = 0;
        solver->clauses_literals = 0;
    }

    void Preprocessor::reSetupSolver() {
        data.reSetupSolver();
    }

    void Preprocessor::shuffle() {
        assert(solver->decisionLevel() == 0 && "shuffle only on level 0!");

        // clear all assignments, to not being forced of keeping track of shuffled trail
        for (int i = 0; i < solver->trail.size(); ++i) {
            solver->varFlags[var(solver->trail[i])].assigns = l_Undef;
            solver->vardata[var(solver->trail[i])].reason = CRef_Undef;
        }
        solver->qhead = 0;
        solver->realHead = 0;
        solver->rebuildOrderHeap(); // needs to add other variables as decision variables again!

        // shuffle trail, clauses and learned clauses
        shuffleVariable = data.nVars();
        shuffler.process(data.getClauses(), data.getLearnts(), solver->trail, data.nVars(),
                         ca); // TODO: should also copy all other variable flags over!

        for (Var v = 0; v < data.nVars(); ++v) {
            assert(data.value(v) == l_Undef && "during shuffling no variable can have a value");
        }

        // set all assignments according to the trail!
        for (int i = 0; i < solver->trail.size(); ++i) {
            solver->varFlags[var(solver->trail[i])].assigns = sign(solver->trail[i]) ? l_False : l_True;
        }
    }

    void Preprocessor::unshuffle(vec<lbool>& model) {
        assert((shuffleVariable == -1 || model.size() == shuffleVariable) && "number of variables has to match");
        shuffler.unshuffle(model, model.size());
    }

    void Preprocessor::sortClauses() {
        uint32_t clausesSize = (*solver).clauses.size();
        for (int i = 0; i < clausesSize; ++i) {
            Clause& c = ca[solver->clauses[i]];
            if (c.can_be_deleted()) {
                continue;
            }
            const uint32_t s = c.size();
            for (uint32_t j = 1; j < s; ++j) {
                const Lit key = c[j];
                int32_t i = j - 1;
                while (i >= 0 && toInt(c[i]) > toInt(key)) {
                    c[i + 1] = c[i];
                    i--;
                }
                c[i + 1] = key;
            }
        }

        clausesSize = solver->learnts.size();
        for (int i = 0; i < clausesSize; ++i) {
            Clause& c = ca[solver->learnts[i]];
            if (c.can_be_deleted()) {
                continue;
            }
            const uint32_t s = c.size();
            for (uint32_t j = 1; j < s; ++j) {
                const Lit key = c[j];
                int32_t i = j - 1;
                while (i >= 0 && toInt(c[i]) > toInt(key)) {
                    c[i + 1] = c[i];
                    i--;
                }
                c[i + 1] = key;
            }
        }
    }

    void Preprocessor::delete_clause(const Riss::CRef& cr) {
        Clause& c = ca[cr];
        c.mark(1);
        ca.free(cr);
    }

    void Preprocessor::printFormula(const string& headline) {
        cerr << "=== Formula " << headline << ": " << endl;
        for (int i = 0; i < data.getSolver()->trail.size(); ++i) {
            cerr << "[" << data.getSolver()->trail[i] << "]" << endl;
        }
        cerr << "c clauses " << endl;
        for (int i = 0; i < data.getClauses().size() && !data.isInterupted(); ++i) {
            cerr << "(" << data.getClauses()[i] << ")";
            if (ca[data.getClauses()[i]].can_be_deleted()) {
                cerr << "(ign)";
            }
            if (ca[data.getClauses()[i]].learnt()) {
                cerr << "(red)";
            }
            cerr << ca[data.getClauses()[i]] << endl;
        }
        cerr << "c learnts" << endl;
        for (int i = 0; i < data.getLearnts().size() && !data.isInterupted(); ++i) {
            cerr << "(" << data.getLearnts()[i] << ")";
            if (ca[data.getLearnts()[i]].can_be_deleted()) {
                cerr << "(ign)";
            }
            if (!ca[data.getClauses()[i]].learnt()) {
                cerr << "(irred)";
            }
            cerr << ca[data.getLearnts()[i]] << endl;
        }
        cerr << "==================== " << endl;
        cerr << " actual formula to be copied: " << endl;
        for (int i = 0; i < data.getTrail().size() && !data.isInterupted(); ++i) {
            cerr << data.getTrail()[i] << " 0" << endl;
        }
        for (int i = 0; i < data.getClauses().size() && !data.isInterupted(); ++i) {
            if (ca[data.getClauses()[i]].can_be_deleted() || ca[data.getClauses()[i]].learnt()) {
                continue;
            }
            cerr << ca[data.getClauses()[i]] << " 0" << endl;
        }
        cerr << "==================== " << endl;
    }

    bool Preprocessor::checkLists(const string& headline) {
        bool ret = false;
        cerr << "c check data structures: " << headline << " ... " << endl;
        int foundEmpty = 0;
        for (Var v = 0; v < data.nVars(); ++v) {
            for (int p = 0; p < 2; ++p) {
                const Lit l = p == 0 ? mkLit(v, false) : mkLit(v, true);
                if (data.list(l).size() == 0) {
                    foundEmpty++;
                }
                for (int i = 0; i < data.list(l).size(); ++i) {
                    for (int j = i + 1; j < data.list(l).size(); ++j) {
                        if (data.list(l)[i] == data.list(l)[j]) {
                            if (!ca[data.list(l)[i]].can_be_deleted()) {
                                ret = true;
                                cerr << "c duplicate " << data.list(l)[j] << " for lit " << l << " at " << i << " and " << j << " out of "
                                     << data.list(l).size() << " = " << ca[data.list(l)[j]] << endl;
                                assert(false && "fix duplicates!");
                            }
                        }
                    }
                }
            }
        }
        cerr << "c found " << foundEmpty << " empty lists, out of " << data.nVars() * 2 << endl;

        DOUT(if (config.opt_check > 1) {
            for (int i = 0; i < data.getLearnts().size(); ++i) {
                const Clause& c = ca[data.getLearnts()[i]];
                if (c.can_be_deleted()) {
                    continue;
                }
                assert(data.checkClauseDRAT(c) && "clauses of the current formula should be DRAT wrt the current proof!");
            }
        });

        DOUT(if (config.opt_check > 2) {
            for (int i = 0; i < data.getLearnts().size(); ++i) {
                const Clause& c = ca[data.getLearnts()[i]];
                if (c.can_be_deleted()) {
                    continue;
                }
                for (int j = 0; j < data.getClauses().size(); ++j) {
                    if (data.getLearnts()[i] == data.getClauses()[j]) {
                        cerr << "c found clause " << data.getLearnts()[i] << " in both vectors" << endl;
                        assert(false && "clause index duplicate in lists");
                    }
                }
            }
            int totalClauses = 0;
            for (int i = 0; i < data.getClauses().size(); ++i) {
                const Clause& c = ca[data.getClauses()[i]];
                if (c.can_be_deleted()) {
                    continue;
                }
                totalClauses++;
                for (int j = i + 1; j < data.getClauses().size(); ++j) {
                    if (data.getClauses()[i] == data.getClauses()[j]) {
                        cerr << "c found clause " << data.getClauses()[i] << " in clause vector twice" << endl;
                        assert(false && "clause index duplicate in lists");
                    }
                }
                if (config.opt_check > 3) {
                    if (!data.proofHasClause(ca[data.getClauses()[i]])) {
                        cerr << "c could not find clause " << ca[data.getClauses()[i]] << " in proof" << endl;
                    }
                }
            }

            for (int i = 0; i < data.getLearnts().size(); ++i) {
                const Clause& c = ca[data.getLearnts()[i]];
                if (c.can_be_deleted()) {
                    continue;
                }
                totalClauses++;
                for (int j = i + 1; j < data.getLearnts().size(); ++j) {
                    if (data.getLearnts()[i] == data.getLearnts()[j]) {
                        cerr << "c found clause " << data.getLearnts()[i] << " in learnts vector twice" << endl;
                        assert(false && "clause index duplicate in lists");
                    }
                }
                if (config.opt_check > 3) {
                    if (!data.proofHasClause(ca[data.getLearnts()[i]])) {
                        cerr << "c could not find clause " << ca[data.getLearnts()[i]] << " in proof" << endl;
                    }
                }
            }
        });

        return ret;
    }

    void Preprocessor::scanCheck(const string& headline) {
        cerr << "c perform scan check " << headline << " [ok?: " << data.ok() << "]" << endl;
        // check whether clause is in solver in the right watch lists
        for (int p = 0; p < 2; ++p) {

            const vec<CRef>& clauses = (p == 0 ? data.getClauses() : data.getLearnts());
            for (int i = 0; i < clauses.size(); ++i) {
                const CRef cr = clauses[i];
                const Clause& c = ca[cr];
                if (c.can_be_deleted()) {
                    continue;
                }

                for (int j = 0; j < c.size(); ++j) {

                    // check whether this clause is in its list
                    int k = 0;
                    for (; k < data.list(c[j]).size(); ++k) {
                        if (data.list(c[j])[k] == cr) {
                            break;
                        }
                    }
                    if (k == data.list(c[j]).size()) {
                        cerr << "c clause [" << cr << "](ign=" << c.can_be_deleted() << ") " << c << " not found in list for literal " << c[j]
                             << endl;
                        DOUT(if (config.opt_check > 1) assert(false && "all clauses have to be in the list"););
                    }

                    for (int k = j + 1; k < c.size(); ++k) {
                        if (c[j] == c[k]) {
                            cerr << "c clause [" << clauses[i] << "]" << c << " has duplicate literals" << endl;
                            DOUT(if (config.opt_check > 1) assert(false && "no duplicate literals are allowed in clauses"););
                            j = c.size();
                            break;
                        } else if (c[j] == ~c[k]) {
                            cerr << "c clause [" << clauses[i] << "]" << c << " has complementary literals" << endl;
                            DOUT(if (config.opt_check > 1) assert(false && "no complementary literals are allowed in clauses"););
                        } else {
                            if (toInt(c[j]) > toInt(c[k])) {
                                cerr << "c clause [" << clauses[i] << "]" << c << " is not sorted" << endl;
                                DOUT(if (config.opt_check > 1) assert(false && "clauses have to be sorted"););
                                j = c.size();
                                break;
                            }
                        }
                    }
                }
            }
        }

        // all clauses in a list of literal l should contain that literal!
        for (Var v = 0; v < data.nVars(); ++v) {
            for (int p = 0; p < 2; ++p) {
                const Lit l = mkLit(v, p == 0);
                vector<CRef>& list = data.list(l);
                for (int i = 0; i < list.size(); ++i) {
                    const Clause& c = ca[list[i]];
                    bool found = false;
                    for (int j = 0; j < c.size(); ++j) {
                        if (c[j] == l) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        cerr << "c clause " << c << " is present in list for literal " << l << " but does not contain it!" << endl;
                    }
                }
            }
        }

        cerr << "c finished check " << endl;
    }

    void Preprocessor::fullCheck(const string& headline) {
        cerr << "c perform full solver state check " << headline << endl;
        checkLists(headline);

        // check whether clause is in solver in the right watch lists
        for (int p = 0; p < 2; ++p) {

            const vec<CRef>& clauses = (p == 0 ? data.getClauses() : data.getLearnts());
            for (int i = 0; i < clauses.size(); ++i) {
                const CRef cr = clauses[i];
                const Clause& c = ca[cr];
                if (c.can_be_deleted()) {
                    continue;
                }

                void* end = 0;
                if (c.size() == 1) {
                    cerr << "there should not be unit clauses! [" << cr << "]" << c << endl;
                } else {
                    for (int j = 0; j < 2; ++j) {
                        const Lit l = ~c[j];
                        vec<Watcher>& ws = solver->watches[l];
                        bool didFind = false;
                        for (int j = 0; j < ws.size(); ++j) {
                            CRef wcr = ws[j].cref();
                            if (wcr == cr) {
                                didFind = true;
                                break;
                            }
                        }
                        if (!didFind) {
                            cerr << "could not find clause[" << cr << "] " << c << " in watcher for lit " << l << endl;
                        }
                        //                     assert( didFind && "clause should be in watch list of its two first literals" );
                    }
                }
            }
        }

        for (Var v = 0; v < data.nVars(); ++v) {
            for (int p = 0; p < 2; ++p) {
                const Lit l = mkLit(v, p == 1);
                vec<Watcher>& ws = solver->watches[l];
                for (int j = 0; j < ws.size(); ++j) {
                    CRef wcr = ws[j].cref();
                    const Clause& c = ca[wcr];
                    if (c[0] != ~l && c[1] != ~l) {
                        cerr << "wrong literals for clause [" << wcr << "] " << c << " are watched. Found in list for " << l << endl;
                    }
                }
            }
            if (solver->varFlags[v].seen != 0) {
                cerr << "c seen for variable " << v << " is not 0, but " << (int)solver->varFlags[v].seen << endl;
            }
        }
    }

    void Preprocessor::printSolver(ostream& s, int verbose) {
        s << "Solver state:" << endl << " ok " << solver->ok << endl << " decision level: " << solver->decisionLevel() << endl;
        if (verbose == 0) {
            return;
        }
        s << " trail_lims: ";
        for (int i = 0; i < solver->trail_lim.size(); i++) {
            s << " " << solver->trail_lim[i];
        }
        s << endl;
        s << " trail: ";
        for (int i = 0; i < solver->trail.size(); ++i) {
            s << " " << solver->trail[i];
        }
        s << endl;

        cerr << "c seen variables:";
        for (Var v = 0; v < solver->nVars(); ++v)
            if (solver->varFlags[v].seen != 0) {
                cerr << " " << v + 1;
            }
        cerr << endl;

        cerr << "c assigned variables:";
        for (Var v = 0; v < solver->nVars(); ++v)
            if (solver->varFlags[v].assigns != l_Undef) {
                cerr << " " << v + 1;
            }
        cerr << endl;

        if (verbose == 1) {
            return;
        }
        s << "formula clauses (without unit clauses):" << endl;
        for (int i = 0; i < solver->clauses.size(); ++i) {
            const Clause& c = solver->ca[solver->clauses[i]];
            if (c.mark() != 0) {
                continue;
            }
            s << c << endl; // print the clause, will print the tag as well
        }
        if (verbose == 2) {
            return;
        }
        s << "learnt clauses (without unit clauses):" << endl;
        for (int i = 0; i < solver->learnts.size(); ++i) {
            const Clause& c = solver->ca[solver->learnts[i]];
            if (c.mark() != 0) {
                continue;
            }
            s << c << endl; // print the clause, will print the tag as well
        }
        if (verbose == 3) {
            return;
        }
        for (Var v = 0; v < solver->nVars(); ++v) {
            for (int pl = 0; pl < 2; ++pl) {
                const Lit p = mkLit(v, pl == 1);
                vec<Riss::Watcher>& ws = solver->watches[p];

                for (int i = 0; i < ws.size(); i++) {
                    CRef cr = ws[i].cref();
                    cerr << "c watch for " << p << " clause " << ca[cr] << " with blocker " << ws[i].blocker() << endl;
                }
            }
        }
    }

    void Preprocessor::freezeSearchVariables() {

        specialFrozenVariables.clear();
        // for all special variables in the search
        for (int i = 0; i < solver->assumptions.size(); i++) { // assumptions
            Var v = var(solver->assumptions[i]);
            if (!data.doNotTouch(v)) {          // if the variable is not frozen already
                data.setNotTouch(v);            // freeze it
                specialFrozenVariables.push(v); // memorize that the variable has been frozen due to this call
            }
        }
        for (int i = 0; i < solver->preferredDecisionVariables.size(); i++) { // preferred decision variables
            const Var& v = solver->preferredDecisionVariables[i];
            if (!data.doNotTouch(v)) {          // if the variable is not frozen already
                data.setNotTouch(v);            // freeze it
                specialFrozenVariables.push(v); // memorize that the variable has been frozen due to this call
            }
        }
    }

    void Preprocessor::meltSearchVariables() {
        // release the freezing
        for (int i = 0; i < specialFrozenVariables.size(); ++i) {
            data.unsetNotTouch(specialFrozenVariables[i]);
        }
        // clear the list of variables
        specialFrozenVariables.clear();
    }

} // namespace Coprocessor
