/* Copyright 2010-2012 Armin Biere, Johannes Kepler University, Linz, Austria */

#include "lglib.h"
#include "aiger.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Clause { int * lits; } Clause;

static aiger * model;
static FILE *outfile, *msgfile, *errfile;
static int verbose, xstim, plain, nowitness, nofork, nobrutefork;
static LGL * lgl, * forked, * bforked;
static int nforked, nbforked;
static int * coi, opt = 3;

static Clause * clauses;
static int nclauses, szclauses;

static int * lits;
static int nlits, szlits;

static int maxvar;
static int * litmap;

static size_t maxbytes, curbytes;

static int catchedsig;
static void (*sig_int_handler)(int);
static void (*sig_segv_handler)(int);
static void (*sig_abrt_handler)(int);
static void (*sig_term_handler)(int);

static void resetsighandlers(void)
{
    (void) signal(SIGINT, sig_int_handler);
    (void) signal(SIGSEGV, sig_segv_handler);
    (void) signal(SIGABRT, sig_abrt_handler);
    (void) signal(SIGTERM, sig_term_handler);
}

static void msg(int level, const char * fmt, ...)
{
    va_list ap;
    if (verbose < level) { return; }
    fprintf(msgfile, "[blimc] %.2f ", lglprocesstime());
    va_start(ap, fmt);
    vfprintf(msgfile, fmt, ap);
    va_end(ap);
    fputc('\n', msgfile);
    fflush(msgfile);
}

static void caughtsigmsg(int sig)
{
    if (!verbose) { return; }
    fprintf(errfile, "[blimc]\n[blimc] CAUGHT SIGNAL %d\n[blimc]\n", sig);
    fflush(errfile);
}

static void stats(void)
{
    if (verbose) {
        if (bforked) { lglstats(bforked); }
        if (forked) { lglstats(forked); }
        if (lgl) { lglstats(lgl); }
    }
    msg(1, "%d forked, %d brute forked", nforked, nbforked);
    msg(1, "max %.1f MB", maxbytes / (double)(1 << 20));
}

static void catchsig(int sig)
{
    if (!catchedsig) {
        fputs("s UNKNOWN\n", errfile);
        fflush(errfile);
        catchedsig = 1;
        caughtsigmsg(sig);
        if (verbose) { stats(), caughtsigmsg(sig); }
    }
    resetsighandlers();
    if (!getenv("LGLNABORT")) { raise(sig); }  else { exit(1); }
}

static void setsighandlers(void)
{
    sig_int_handler = signal(SIGINT, catchsig);
    sig_segv_handler = signal(SIGSEGV, catchsig);
    sig_abrt_handler = signal(SIGABRT, catchsig);
    sig_term_handler = signal(SIGTERM, catchsig);
}

static void die(const char * fmt, ...)
{
    va_list ap;
    fputs("*** blimc: ", errfile);
    va_start(ap, fmt);
    vfprintf(errfile, fmt, ap);
    va_end(ap);
    fputc('\n', errfile);
    fflush(errfile);
    exit(1);
}

static void * new(void * state, size_t bytes)
{
    char * res;
    (void) state;
    res = malloc(bytes);
    if (!res) { die("out of memory"); }
    curbytes += bytes;
    if (curbytes > maxbytes) { maxbytes = curbytes; }
    return res;
}

static void del(void * state, void * ptr, size_t bytes)
{
    (void) state;
    assert(curbytes >= bytes);
    curbytes -= bytes;
    free(ptr);
}

static void * rsz(void * state, void * ptr, size_t o, size_t n)
{
    char * res;
    (void) state;
    assert(curbytes >= o);
    curbytes -= o;
    res = realloc(ptr, n);
    if (!res) { die("out of memory"); }
    curbytes += n;
    if (curbytes > maxbytes) { maxbytes = curbytes; }
    return res;
}

static int prepilit(unsigned ulit)
{
    int res;
    assert(aiger_lit2var(ulit) <= model->maxvar);
    assert(coi[aiger_lit2var(ulit)]);
    assert(ulit <= 2 * model->maxvar + 1);
    res = (ulit >> 1) + 1;
    if (ulit & 1) { res = -res; }
    return res;
}

static unsigned aiginput(unsigned idx)
{
    assert(0 <= idx && idx < model->num_inputs);
    return model->inputs[idx].lit;
}

static int prepinput(unsigned idx) { return prepilit(aiginput(idx)); }

static unsigned aiglatch(unsigned idx)
{
    assert(0 <= idx && idx < model->num_latches);
    return model->latches[idx].lit;
}

static unsigned aigreset(unsigned idx)
{
    assert(0 <= idx && idx < model->num_latches);
    return model->latches[idx].reset;
}

static int preplatch(unsigned idx) { return prepilit(aiglatch(idx)); }

static unsigned aignext(unsigned idx)
{
    assert(0 <= idx && idx < model->num_latches);
    return model->latches[idx].next;
}

static int prepnext(unsigned idx) { return prepilit(aignext(idx)); }

static unsigned aigoutput(unsigned idx)
{
    assert(0 <= idx && idx < model->num_outputs);
    return model->outputs[idx].lit;
}

static int prepoutput(unsigned idx) { return prepilit(aigoutput(idx)); }

static int preplhs(unsigned idx)
{
    assert(0 <= idx && idx < model->num_ands);
    return prepilit(model->ands[idx].lhs);
}

static int prepleft(unsigned idx)
{
    assert(0 <= idx && idx < model->num_ands);
    return prepilit(model->ands[idx].rhs0);
}

static int prepright(unsigned idx)
{
    assert(0 <= idx && idx < model->num_ands);
    return prepilit(model->ands[idx].rhs1);
}

static void unit(int ilit) { lgladd(lgl, ilit); lgladd(lgl, 0); }

static void binary(int a, int b)
{
    lgladd(lgl, a);
    lgladd(lgl, b);
    lgladd(lgl, 0);
}

static void ternary(int a, int b, int c)
{
    lgladd(lgl, a);
    lgladd(lgl, b);
    lgladd(lgl, c);
    lgladd(lgl, 0);
}

static int ulitincoi(unsigned ulit)
{
    unsigned idx;
    assert(ulit <= 2 * model->maxvar + 1);
    idx = aiger_lit2var(ulit);
    assert(idx <= model->maxvar);
    return coi[idx];
}

static void prepfreeze(void)
{
    unsigned i;
    msg(2, "freeze");
    for (i = 0; i < model->num_inputs; i++)
        if (ulitincoi(aiginput(i))) {
            lglfreeze(lgl, prepinput(i));
        }
    for (i = 0; i < model->num_latches; i++) {
        if (!ulitincoi(aiglatch(i))) { continue; }
        lglfreeze(lgl, preplatch(i));
        lglfreeze(lgl, prepnext(i));
    }
    for (i = 0; i < model->num_outputs; i++) {
        assert(ulitincoi(model->outputs[i].lit));
        lglfreeze(lgl, prepoutput(i));
    }
    if (ulitincoi(0)) { lglfreeze(lgl, 1); }
}

static void and (int lhs, int rhs0, int rhs1)
{
    binary(-lhs, rhs0);
    binary(-lhs, rhs1);
    ternary(-rhs0, -rhs1, lhs);
}

static void logic(void)
{
    unsigned i;
    msg(2, "logic");
    if (ulitincoi(0)) { unit(-1); }
    for (i = 0; i < model->num_ands; i++)
        if (ulitincoi(model->ands[i].lhs)) {
            and (preplhs(i), prepleft(i), prepright(i));
        }
}

static const char * usage =
    "usage: blimc [-h][-v][-x][-n][-p][<maxk>][<aiger>]\n"
    ;

static int isnumstr(const char * str)
{
    const char * p = str;
    int ch;
    if (!isdigit(*p++)) { return 0; }
    while (isdigit((ch = *p))) { p++; }
    return !ch;
}

static double percent(double a, double b)
{
    return b ? 100.0 * (a / b) : 0.0;
}

static void coimsg(const char * name, unsigned remaining, unsigned all)
{
    msg(1, "%-9s in COI: %10u = %3.0f%% out of %u",
        name, remaining, percent(remaining, all), all);
}

static void travcoi(void)
{
    unsigned next, top, size, latches, inputs, ands, constants;
    unsigned lit, * stack, idx, stripped;
    aiger_symbol * s;
    aiger_and * a;
    size = model->maxvar + 1;
    stack = new(0, size * sizeof * stack);
    latches = inputs = ands = constants = 0;
    assert(model->num_outputs == 1);
    lit = model->outputs[0].lit;
    idx = aiger_lit2var(lit);
    coi[idx] = top = next = 1;
    for (;;) {
        assert(idx <= model->maxvar);
        assert(coi[idx]);
        stripped = aiger_strip(lit);
        if (aiger_is_input(model, stripped)) { inputs++; }
        else if ((s = aiger_is_latch(model, stripped))) {
            latches++;
            lit = s->next;
            idx = aiger_lit2var(lit);
            assert(idx <= model->maxvar);
            if (!coi[idx]) { stack[top++] = lit, coi[idx] = top; }
        } else if ((a = aiger_is_and(model, stripped))) {
            ands++;
            lit = a->rhs0;
            idx = aiger_lit2var(lit);
            assert(idx <= model->maxvar);
            if (!coi[idx]) { stack[top++] = lit, coi[idx] = top; }
            lit = a->rhs1;
            idx = aiger_lit2var(lit);
            assert(idx <= model->maxvar);
            if (!coi[idx]) { stack[top++] = lit, coi[idx] = top; }
        } else { assert(!stripped), constants++; }
        if (next == top) { break; }
        lit = stack[next++];
    }
    del(0, stack, size * sizeof * stack);
    coimsg("literals", top, model->maxvar);
    coimsg("inputs", inputs, model->num_inputs);
    coimsg("latches", latches, model->num_latches);
    coimsg("constants", constants, 1);
    coimsg("ands", ands, model->num_ands);
}

static void setopts(LGL * local, const char * prefix)
{
    lglsetout(local, msgfile);
    lglsetprefix(local, prefix);
}

static void newlgl(int tid)
{
    char prefix[80];
    msg(1, "new lgl %d", tid);
    sprintf(prefix, "[lglib%d] ", tid);
    lgl = lglminit(0, new, rsz, del);
    setopts(lgl, prefix);
    if (verbose >= 2) { lglsetopt(lgl, "verbose", verbose - 1); }
    if (plain) { lglsetopt(lgl, "plain", 1); }
}

static void extract(void * dummy, int lit)
{
    size_t oldbytes, newbytes;
    Clause * c;
    int i;
    (void) dummy;
    if (lit) {
        if (nlits == szlits) {
            oldbytes = szlits * sizeof * lits;
            szlits = szlits ? 2 * szlits : 1;
            newbytes = szlits * sizeof * lits;
            lits = rsz(0, lits, oldbytes, newbytes);
        }
        lits[nlits++] = lit;
    } else {
        if (nclauses == szclauses) {
            oldbytes = szclauses * sizeof * clauses;
            szclauses = szclauses ? 2 * szclauses : 1;
            newbytes = szclauses * sizeof * clauses;
            clauses = rsz(0, clauses, oldbytes, newbytes);
        }
        c = clauses + nclauses++;
        newbytes = (nlits + 1) * sizeof * c->lits;
        c->lits = new(0, newbytes);
        for (i = 0; i < nlits; i++) { c->lits[i] = lits[i]; }
        c->lits[i] = 0;
        nlits = 0;
    }
}

static int mapuntimedlit(int lit)
{
    int idx = abs(lit), res;
    assert(1 <= idx && idx <= model->maxvar + 1);
    if (!(res = litmap[idx])) { litmap[idx] = res = ++maxvar; }
    if (lit < 0) { res = -res; }
    return res;
}

static void mapcnf(void)
{
    int * p, lit;
    Clause * c;
    for (c = clauses; c < clauses + nclauses; c++)
        for (p = c->lits; (lit = *p); p++) {
            *p = mapuntimedlit(lit);
        }
    msg(1, "mapped %d variables", maxvar);
}

static int mainilit(unsigned ulit)
{
    int tmp = prepilit(ulit), idx = abs(tmp), res;
    assert(1 <= idx && idx <= model->maxvar + 1);
    res = litmap[idx];
    if (tmp < 0) { res = -res; }
    return res;
}

static int shift(int ilit, int time)
{
    int idx = abs(ilit), res;
    assert(1 <= idx && idx <= maxvar);
    res = idx + time * maxvar;
    if (ilit < 0) { res = -res; }
    return res;
}

static void equiv(int a, int b) { binary(-a, b); binary(a, -b); }

static void shiftcnf(int time)
{
    int * p, lit, prev;
    unsigned i;
    Clause * c;
    assert(0 <= time);
    if (time > 0) {
        for (i = 0; i < model->num_latches; i++) {
            if (!ulitincoi(aiglatch(i))) { continue; }
            prev = shift(mainilit(aignext(i)), time - 1);
            lit = shift(mainilit(aiglatch(i)), time);
            equiv(prev, lit);
            lglmelt(lgl, prev);
        }
    }
    for (c = clauses; c < clauses + nclauses; c++) {
        for (p = c->lits; (lit = *p); p++) { lgladd(lgl, shift(lit, time)); }
        lgladd(lgl, 0);
    }
    for (i = 0; i < model->num_latches; i++)
        if (ulitincoi(aiglatch(i))) {
            lglfreeze(lgl, shift(mainilit(aignext(i)), time));
        }
}

static void bad(int time)
{
    lglassume(lgl, shift(mainilit(aigoutput(0)), time));
}

static void init(void)
{
    size_t bytes;
    unsigned i;
    int lit;
    bytes = (model->maxvar + 2) * sizeof * litmap;
    litmap = new(0, bytes);
    memset(litmap, 0, bytes);
    assert(!maxvar);
    maxvar = 0;           // true lit
    for (i = 0; i < model->num_latches; i++) {
        if (!ulitincoi(aiglatch(i))) { continue; }
        lit = ++maxvar;
        litmap[preplatch(i)] = lit;
    }
    for (i = 0; i < model->num_inputs; i++) {
        if (!ulitincoi(aiginput(i))) { continue; }
        lit = ++maxvar;
        litmap[prepinput(i)] = lit;
    }
    mapcnf();
    #ifndef NDEBUG
    for (i = 0; i < model->num_outputs; i++)
        if (ulitincoi(aigoutput(i))) {
            assert(litmap[abs(prepoutput(i))]);
        }
    for (i = 0; i < model->num_latches; i++)
        if (ulitincoi(aiglatch(i))) {
            assert(litmap[abs(prepnext(i))]);
        }
    #endif
    for (i = 0; i < model->num_latches; i++) {
        if (!ulitincoi(aiglatch(i))) { continue; }
        if (aiglatch(i) == aigreset(i)) { continue; }
        if (aiger_false == aigreset(i)) { unit(-mainilit(aiglatch(i))); }
        if (aiger_true == aigreset(i)) { unit(mainilit(aiglatch(i))); }
    }
}

static int length(const int * p)
{
    const int * q;
    for (q = p; *q; q++)
        ;
    return q - p;
}

int main(int argc, char ** argv)
{
    int res, fres, bfres, i, k, lit, maxk, val;
    unsigned j, init0, init1, initx;
    const char * iname, * err;
    size_t coibytes;
    char prefix[80];
    LGL * tmp;
    char vch;
    outfile = stdout, msgfile = errfile = stderr;
    iname = 0;
    maxk = 0;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) { fputs(usage, outfile); exit(0);}
        else if (!strcmp(argv[i], "-v")) { verbose++; }
        else if (!strcmp(argv[i], "-x")) { xstim = 1; }
        else if (!strcmp(argv[i], "-n")) { nowitness = 1; }
        else if (!strcmp(argv[i], "-p")) { plain = 1; }
        else if (!strcmp(argv[i], "-O")) { opt = 1; }
        else if (!strcmp(argv[i], "-O0")) { opt = 0; }
        else if (!strcmp(argv[i], "-O1")) { opt = 1; }
        else if (!strcmp(argv[i], "-O2")) { opt = 2; }
        else if (!strcmp(argv[i], "-O3")) { opt = 3; }
        else if (!strcmp(argv[i], "-O4")) { opt = 4; }
        else if (!strcmp(argv[i], "-O5")) { opt = 5; }
        else if (!strcmp(argv[i], "-O6")) { opt = 6; }
        else if (!strcmp(argv[i], "--no-fork")) { nofork = 1; }
        else if (!strcmp(argv[i], "--no-brute-fork")) { nobrutefork = 1; }
        else if (argv[i][0] == '-') {
            die("invalid command line option '%s'", argv[i]);
        } else if (isnumstr(argv[i])) { maxk = atoi(argv[i]); }
        else if (iname) {
            die("two files specified '%s' and '%s'", iname, argv[i]);
        } else { iname = argv[i]; }
    }
    if (verbose)
        lglbnr("BLIMC Bounded Lingeling Model Checker",
               "[blimc] ", msgfile);
    setsighandlers();
    msg(1, "reading %s", iname ? iname : "<stdin>");
    model = aiger_init();
    if (iname) { err = aiger_open_and_read_from_file(model, iname); }
    else { err = aiger_read_from_file(model, stdin); }
    if (err) die("parse error in '%s' at %s",
                     iname ? iname : "<stdin>", err);
    msg(1, "M I L O A = %u %u %u %u %u",
        model->maxvar,
        model->num_inputs,
        model->num_latches,
        model->num_outputs,
        model->num_ands);
    if (model->num_outputs != 1) {
        die("expected exactly one output but got %u", model->num_outputs);
    }
    init0 = init1 = initx = 0;
    for (j = 0; j < model->num_latches; j++) {
        unsigned rst = aigreset(j);
        if (rst == aiger_false) { init0++; }
        if (rst == aiger_true) { init1++; }
        if (rst == aiglatch(j)) { initx++; }
    }
    if (model->num_latches) {
        msg(1, "%u latches initialized to 0", init0);
        msg(1, "%u latches initialized to 1", init1);
        msg(1, "%u latches uninitialized", initx);
    } else { msg(1, "no latches, so purely combinational"); }
    coibytes = (model->maxvar + 1) * sizeof * coi;
    coi = new(0, coibytes);
    memset(coi, 0, coibytes);
    travcoi();
    newlgl(0);
    if (!plain) {
        lglsetopt(lgl, "elmrtc", 1);
        lglsetopt(lgl, "blkrtc", 1);
        lglsetopt(lgl, "cliff", 0);
        lglsetopt(lgl, "prbasic", 0);
        lglsetopt(lgl, "prbsimplertc", 1);
        lglsetopt(lgl, "treelookmineff", 10 * lglgetopt(lgl, "treelookmineff"));
        lglsetopt(lgl, "ccemineff", 5 * lglgetopt(lgl, "ccemineff"));
    }
    logic();
    prepfreeze();
    msg(1, "encoded");
    (void) lglsimp(lgl, opt);
    msg(1, "simplified");
    if (lglfixed(lgl, prepoutput(0)) < 0) {
        if (verbose >= 2) { lglstats(lgl); }
        lglrelease(lgl);
        res = 20;
        msg(1, "return 20, because simplification fixed initial output literal %d", prepoutput(0));
        fprintf(outfile, "0\nb0\n.\n");
    } else {
        res = 0;
        lgltravall(lgl, 0, extract);
        msg(1, "extracted");
        if (verbose >= 1) { lglstats(lgl); }
        tmp = lgl, lgl = 0;
        lglrelease(tmp);
        newlgl(1);
        init();
        msg(1, "maxk %d", maxk);
        for (k = 0; k <= maxk; k++) {
            msg(1, "bound %d", k);
            shiftcnf(k);
            bad(k);
            #if 1
            if (!nofork) {
                lglsetopt(lgl, "clim", 100000);
            }
            #else
            lglsetopt(lgl, "clim", 5);
            #endif
            res = lglsat(lgl);
            if (!res) {
                assert(!nofork);
                forked = lglfork(lgl, 0);
                sprintf(prefix, "[lglfork%d] ", nforked++);
                setopts(forked, prefix);
                #if 1
                if (!nobrutefork) {
                    lglsetopt(forked, "clim", 1000000);
                }
                #else
                lglsetopt(forked, "clim", 10);
                #endif
                res = lglsat(forked);
                tmp = forked, forked = 0;
                fres = lgljoin(lgl, tmp);
                if (res) {
                    assert(!res || fres == res);
                    res = fres;
                } else {
                    assert(!nobrutefork);
                    bforked = lglbrutefork(lgl, 0);
                    sprintf(prefix, "[lglbrutefork%d] ", nbforked++);
                    setopts(bforked, prefix);
                    #ifndef NDEBUG
                    res =
                    #endif
                        lglsat(bforked);
                    assert(res);
                    tmp = bforked, bforked = 0;
                    bfres = lgljoin(lgl, tmp);
                    assert(bfres == res);
                    res = bfres;
                }
            }
            assert(res);
            if (res == 10) {
                msg(1, "interrupt at iteration: %d, because result is SAT", k);
                break;
            }
            assert(res == 20);
            printf("u%d\n", k);
            fflush(stdout);
            if (!model->num_latches) {
                msg(1, "interrupt at iteration: %d, because there are no latches (pure combinatorial)", k);
                break;
            }
        }
        if (res == 10) {
            fprintf(outfile, "1\nb0\n");
            if (!nowitness)
                for (i = 0; i < model->num_latches; i++) {
                    if (ulitincoi(aiglatch(i))) {
                        lit = shift(mainilit(aiglatch(i)), 0);
                        val = lglderef(lgl, lit);
                    } else { val = 0; }
                    if (val > 0) { vch = '1'; }
                    else if (!val && xstim) { vch = 'x'; }
                    else { vch = '0'; }
                    fputc(vch, stdout);
                }
            fputc('\n', stdout);
            for (i = 0; i <= k; i++) {
                for (j = 0; j < model->num_inputs; j++) {
                    if (ulitincoi(aiginput(j))) {
                        lit = shift(mainilit(aiginput(j)), i);
                        val = lglderef(lgl, lit);
                    } else { val = 0; }
                    if (val > 0) { vch = '1'; }
                    else if (!val && xstim) { vch = 'x'; }
                    else { vch = '0'; }
                    fputc(vch, stdout);
                }
                fputc('\n', stdout);
            }
            printf(".\n");
        } else if (res == 20 && !model->num_latches) {
            msg(1, "return 20, because SAT result is UNSAT and there are no latches, at iteration: %d", k);
            fprintf(outfile, "0\nb0\n.\n");
        } else {
            msg(1, "return 20, because SAT result is UNSAT and there are latches, at iteration: %d", k);
            fprintf(outfile, "2\nb0\n.\n");
        }
        del(0, litmap, (model->maxvar + 2) * sizeof * litmap);
        if (verbose >= 1) { lglstats(lgl); }
        tmp = lgl, lgl = 0;
        lglrelease(tmp);
    }
    fflush(outfile);
    del(0, coi, (model->maxvar + 1) * sizeof * coi);
    aiger_reset(model);
    for (i = 0; i < nclauses; i++)
        del(0, clauses[i].lits,
            (length(clauses[i].lits) + 1) * sizeof * clauses[i].lits);
    del(0, clauses, szclauses * sizeof * clauses);
    del(0, lits, szlits * sizeof * lits);
    resetsighandlers();
    stats();
    return res;
}
