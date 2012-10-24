/* Copyright 2010 Armin Biere, Johannes Kepler University, Linz, Austria */

#include "lglib.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <limits.h>
#include <stdint.h> 

#ifndef NLGLPICOSAT
#include "picosat.h"
#endif

#define NCHKSOL	// do not check solution

#ifndef NCHKSOL
#warning "NCHKSOL undefined"
#endif

#define LOCKED		(1 << 30)
#define COLLECT		(LOCKED - 1)
#define REMOVED		(COLLECT - 1)
#define MAXVAR		((LOCKED >> RMSHFT) - 2)
#define SCINCLEXP	(1<<20)
#define GLUESHFT	4
#define GLUE 		(1 << GLUESHFT)
#define MAXGLUE		(GLUE - 1)
#define GLUEMASK	(GLUE - 1)
#define MAXREDLIDX	((1 << (31 - GLUESHFT)) - 2)
#define MAXIRRLIDX	((1 << (31 - RMSHFT)) - 2)
#define FLTPRC 		32
#define EXPMIN 		(0x0000 ## 0000)
#define EXPZRO 		(0x1000 ## 0000)
#define EXPMAX		(0x7fff ## ffff)
#define MNTBIT		(0x0000 ## 0001 ## 0000 ## 0000 ## ull)
#define MNTMAX		(0x0000 ## 0001 ## ffff ## ffff ## ull)
#define FLTMIN		(0x0000 ## 0000 ## 0000 ## 0000 ## ll)
#define FLTMAX		(0x7fff ## ffff ## ffff ## ffff ## ll)
#define MAXLDFW		31		
#define REPMOD 		23
#define MAXPHN		2
#define FALSECNF	(1ll<<32)
#define TRUECNF		0ll
#define FUNVAR		11
#define FUNQUADS	(1<<(FUNVAR - 6))

#define cover(b) assert(!(b))

typedef enum Tag {
  FREEVAR = 0,
  FIXEDVAR = 1,
  EQUIVAR = 2,
  ELIMVAR = 3,

  DECISION = 0,
  UNITCS = 1,
  IRRCS = 1,
  BINCS = 2,
  TRNCS = 3,
  LRGCS = 4,
  MASKCS = 7,

  REDCS = 8,
  RMSHFT = 4,
} Tag;

#ifndef NLGLOG
#include <math.h>
#define LOG(LEVEL,FMT,ARGS...) \
  do { \
    if (LEVEL > lgl->opts.log.val) break; \
    lglogstart (lgl, LEVEL, FMT, ##ARGS); \
    lglogend (lgl); \
  } while (0)
#define LOGCLS(LEVEL,CLS,FMT,ARGS...) \
  do { \
    const int * P; \
    if (LEVEL > lgl->opts.log.val) break; \
    lglogstart (lgl, LEVEL, FMT, ##ARGS); \
    for (P = (CLS); *P; P++) printf (" %d", *P); \
    lglogend (lgl); \
  } while (0)
#define LOGMCLS(LEVEL,CLS,FMT,ARGS...) \
  do { \
    const int * P; \
    if (LEVEL > lgl->opts.log.val) break; \
    lglogstart (lgl, LEVEL, FMT, ##ARGS); \
    for (P = (CLS); *P; P++) printf (" %d", lglm2i (lgl, *P)); \
    lglogend (lgl); \
  } while (0)
#define LOGRESOLVENT(LEVEL,FMT,ARGS...) \
  do { \
    const int * P; \
    if (LEVEL > lgl->opts.log.val) break; \
    lglogstart (lgl, LEVEL, FMT, ##ARGS); \
    for (P = lgl->resolvent.start; P < lgl->resolvent.top; P++) \
      printf (" %d", *P); \
    lglogend (lgl); \
  } while (0)
#define LOGREASON(LEVEL,LIT,REASON0,REASON1,FMT,ARGS...) \
  do { \
    int TAG, TMP, RED, G; \
    const int * C, * P; \
    if (LEVEL > lgl->opts.log.val) break; \
    lglogstart (lgl, LEVEL, FMT, ##ARGS); \
    TMP = (REASON0 >> RMSHFT); \
    RED = (REASON0 & REDCS); \
    TAG = (REASON0 & MASKCS); \
    if (TAG == DECISION) fputs (" decision", stdout); \
    else if (TAG == UNITCS) printf (" unit %d", LIT); \
    else if (TAG == BINCS) { \
      printf (" %s binary clause %d %d", lglred2str (RED), LIT, TMP); \
    } else if (TAG == TRNCS) { \
      printf (" %s ternary clause %d %d %d", \
              lglred2str (RED), LIT, TMP, REASON1); \
    } else { \
      assert (TAG == LRGCS); \
      C = lglidx2lits (lgl, RED, REASON1); \
      for (P = C; *P; P++) \
	; \
      printf (" size %ld", (long)(P - C)); \
      if (RED) { \
	G = (REASON1 & GLUEMASK); \
	printf (" glue %d redundant", G); \
      } else fputs (" irredundant", stdout); \
      fputs (" clause", stdout); \
      for (P = C; *P; P++) { \
	printf (" %d", *P); \
      } \
    } \
    lglogend (lgl); \
  } while (0)
#define LOGDSCHED(LEVEL,LIT,FMT,ARGS...) \
  do { \
    int POS; Scr SCORE; \
    if (LEVEL > lgl->opts.log.val) break; \
    POS = *lgldpos (lgl, LIT); \
    SCORE = lgldscore (lgl, LIT); \
    lglogstart (lgl, LEVEL, "dsched[%d] = %d ", POS, LIT); \
    printf (FMT, ##ARGS); \
    printf (" score "); \
    printf ("%s", lglflt2str (lgl, SCORE)); \
    lglogend (lgl); \
  } while (0)
#define LOGESCHED(LEVEL,LIT,FMT,ARGS...) \
  do { \
    int POS; int SCORE; \
    EVar * EV; \
    if (LEVEL > lgl->opts.log.val) break; \
    POS = *lglepos (lgl, LIT); \
    SCORE = lglescore (lgl, LIT); \
    lglogstart (lgl, LEVEL, "esched[%d] = %d ", POS, LIT); \
    printf (FMT, ##ARGS); \
    EV = lglevar (lgl, LIT); \
    printf (" score %d occ %d %d", SCORE, EV->occ[0], EV->occ[1]); \
    lglogend (lgl); \
  } while (0)
#define LOGFLT(LEVEL,FLT,FMT,ARGS...) \
  do { \
    int EXP; Mnt MNT; \
    if (LEVEL > lgl->opts.log.val) break; \
    lglogstart (lgl, LEVEL, FMT, ##ARGS); \
    EXP = lglexp (FLT); \
    MNT = lglmnt (FLT); \
    printf (" %s %e %d %llu %016llx", \
            lglflt2str (lgl, FLT), lglflt2dbl (FLT), EXP, MNT, FLT); \
    lglogend (lgl); \
  } while (0)
#else
#define LOG(ARGS...) do { } while (0)
#define LOGCLS(ARGS...) do { } while (0)
#define LOGMCLS(ARGS...) do { } while (0)
#ifndef NDEBUG
#define LOGRESOLVENT(ARGS...) do { } while (0)
#endif
#define LOGREASON(ARGS...) do { } while (0)
#define LOGDSCHED(ARGS...) do { } while (0)
#define LOGESCHED(ARGS...) do { } while (0)
#define LOGFLT(ARGS...) do { } while (0)
#endif

#define ABORTIF(COND,FMT,ARGS...) \
  do { \
    if (!(COND)) break; \
    fprintf (stderr, \
             "*** usage error of '%s' in '%s': ", \
             __FILE__, __FUNCTION__); \
    fprintf (stderr, FMT, ##ARGS); \
    fputc ('\n', stderr); \
    abort (); \
  } while (0)

#define ABORTIFNOTINSTATE(STATE) \
  do { \
    ABORTIF(lgl->state != STATE, "not %s", #STATE); \
  } while (0)

#define SWAP(TYPE,A,B) do { TYPE TMP = (A); (A) = (B); (B) = TMP; } while (0)

#define NEW(P,N)\
  do { (P) = lglmalloc (lgl, (N) * sizeof *(P)); } while (0)
#define DEL(P,N) \
  do { lglfree (lgl, (P), (N) * sizeof *(P)); (P) = 0; } while (0)
#define RSZ(P,O,N) \
  do { (P) = lglrealloc (lgl, (P), (O)*sizeof*(P), (N)*sizeof*(P)); } while (0)
#define CLRPTR(P) \
  do { memset ((P), 0, sizeof *(P)); } while (0)
#define CLR(P) \
  do { memset (&(P), 0, sizeof (P)); } while (0)

typedef struct Opt { 
  char shrt;
  const char * lng, * descrp; 
  int val, min, max;
} Opt;

typedef struct Opts {
  Opt beforefirst;
  Opt agile;
  Opt bias;
  Opt block;
  Opt blkmaxeff;
  Opt blkmineff;
  Opt blkreleff;
  Opt blkocclim;
#ifndef NDEBUG
  Opt check;
#endif
  Opt decompose;
  Opt dcpcintinc;
  Opt dcpvintinc;
  Opt defragint;
  Opt defragfree;
  Opt distill;
  Opt dstcintinc;
  Opt dstvintinc;
  Opt dstmaxeff;
  Opt dstmineff;
  Opt dstreleff;
  Opt elim;
  Opt elmcintinc;
  Opt elmvintinc;
  Opt elmaxeff;
  Opt elmineff;
  Opt elmreleff;
  Opt elmreslim;
  Opt elmocclim;
  Opt elmpenlim;
  Opt gccintinc;
  Opt gcvintinc;
  Opt hte;
  Opt htemaxeff;
  Opt htemineff;
  Opt htereleff;
  Opt lhbr;
#ifndef NLGLOG
  Opt log;
#endif
  Opt phase;
  Opt plain;
  Opt probe;
  Opt prbcintinc;
  Opt prbvintinc;
  Opt prbmaxeff;
  Opt prbmineff;
  Opt prbreleff;
  Opt seed;
  Opt smallve;
  Opt smallvevars;
  Opt randec;
  Opt randecint;
  Opt rebias;
  Opt rebiasint;
  Opt redlinit;
  Opt redlinc;
  Opt redlmininc;
  Opt restartint;
  Opt scincinc;
  Opt transred;
  Opt trdcintinc;
  Opt trdvintinc;
  Opt trdmineff;
  Opt trdmaxeff;
  Opt trdreleff;
  Opt verbose; 
  Opt witness;
  Opt afterlast;
} Opts;

#define FIRSTOPT(lgl) (&(lgl)->opts.beforefirst + 1)
#define LASTOPT(lgl) (&(lgl)->opts.afterlast - 1)

typedef struct Stats {
  int defrags, rescored, iterations, skipped, uips;
  int confs, reduced, restarts, rebias, reported, gcs;
  int irr, decomps, decisions, randecs;
  int64_t prgss, enlwchs, rdded, ronflicts, pshwchs, height;
  struct { int64_t search, simp; } props, visits; 
  struct { size_t current, max; } bytes;
  struct { int bin, trn, lrg; } red;
  struct { int cnt, trn, lrg, sub; } hbr;
  struct { int current, sum; } fixed, equivalent;
  struct { 
    struct { double process, phase[MAXPHN], * ptr[MAXPHN]; 
             int nesting; } entered; 
    double all; } time;
  struct { int count, elm; int64_t res; } blk;
  struct { int count, trn, lrg, failed; int64_t steps; } hte;
  struct { int red, lits, clauses, count, failed; } dst;
  struct { int count, failed, lifted; int64_t probed; } prb;
  struct { int count, red, failed; int64_t lits, bins, steps; } trd;
  struct { 
    int count, skipped, elmd, forced, large, sub, str, blkd;
    struct { int elm, tried, failed; } small;
    int64_t resolutions, copies, subchks, strchks, steps; } elm;
  struct { 
    struct { struct { int irr, red; } dyn; int stat; } sub, str; 
    int driving; } otfs;
  struct { 
    double prb,dcp,elm,trd,gc,gcd,gce,gcb,dst,dfg,red,blk,trj,hte; } tms;
  struct { int64_t nonmin, learned; } lits;
  struct { int learned; int64_t glue; } clauses;
} Stats;

typedef struct Limits { 
  int reduce, randec;
  struct { int confs, luby; } rebias;
  struct { int confs, wasmaxdelta, maxdelta, luby; } restart;
  struct { int confs, cinc, vinc; int64_t visits, prgss; } dcp, dst;
  struct { int cinc, vinc, confs; int64_t visits, steps, prgss; } trd;
  struct { int cinc, vinc, confs, skip, penalty;
           int64_t visits, steps, prgss; } elm;
  struct { int cinc, vinc, confs, fixedoreq; int64_t visits; } gc;
  struct { int cinc, vinc, confs; int64_t visits, prgss; } prb;
  struct { int64_t pshwchs, prgss; } dfg;
  struct { int64_t steps; } term, sync, hte;
} Limits;

typedef struct Stk { int * start, * top, * end; } Stk;

typedef int Exp;
typedef uint64_t Mnt;
typedef int64_t Flt;
typedef int64_t Scr;
typedef signed char Val;

typedef struct HTS { int offset, count; }  HTS;
typedef struct DVar { HTS hts[2]; } DVar;

typedef struct AVar {
  Scr score; int pos;
  unsigned type : 3;
#ifndef NDEBUG
  unsigned simp : 1;
  unsigned wasfalse : 1;
#endif
  int phase : 2, bias : 2;
  int mark, level;
  int rsn[2];
} AVar;

typedef struct EVar { int occ[2], score, pos; } EVar;

typedef struct Conf { int lit, rsn[2]; } Conf;

typedef struct Lir { 
  Stk lits; int mapped; 
#ifndef NLGLSTATS
  int64_t added, resolved, forcing, conflicts;
#endif
} Lir;

typedef enum State {
  UNUSED = 0,
  USED = 1,
  READY = 2,
  UNKNOWN = 3,
  SATISFIED = 4,
  UNSATISFIED = 5,
} State;

#if !defined(NDEBUG) || !defined(NLGLOG)
#define RESOLVENT
#endif

struct LGL {
  State state;
  Opts opts;
  DVar * dvars; AVar * avars; Val * vals;  EVar * evars;
  int nvars, szvars, * i2e, * repr;
  Flt scinc, scincf, scincl;
  Stk e2i, clause, dsched, esched, extend;
  Lir red[GLUE]; Stk irr; int mt;
  struct { struct { Stk bin, trn; } red, irr; } dis;
  struct { int pivot, negcls, necls, neglidx;
    Stk lits, next, csigs, lsigs, sizes, occs, noccs, mark, m2i; } elm;
  Stk trail, control, frames, saved, clv;
  Stk wchs; int freewchs[MAXLDFW], nfreewchs;
  int next, level, decision, unassigned;
  Conf conf; Stk seen, stack;
  unsigned flips, rng;
  char decomposing, measureagility, probing, distilling;
  char simp, eliminating, simplifying, blocking;
  char dense, propred, igntrn;
  int ignlidx, ignlits[3];
  int bias;
  int tid;
  Limits limits;
  Stats stats;
  struct { int (*fun)(void*); void * state;  } term;
  struct { void (*fun)(void*,int); void * state; } produce, consumed;
  struct { void(*fun)(void*,int**,int**); void*state; } consume;
  double (*getime)(void);
#ifndef NCHKSOL
  Stk orig;
#endif
#ifdef RESOLVENT
  Stk resolvent;
#endif
  struct { void(*lock)(void*); void (*unlock)(void*); void*state; } msglock;
#ifndef NLGLOG
  char scstr[100];
#endif
#ifndef NLGLPICOSAT
  struct { int res, chk; } picosat;
#endif
};

typedef int64_t Cnf;
typedef uint64_t Fun[FUNQUADS];

static const char lglfloorldtab[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
  -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
  LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
  LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
};

static const uint64_t lglbasevar2funtab[6] = {
  0xaaaaaaaaaaaaaaaaull, 0xccccccccccccccccull, 0xf0f0f0f0f0f0f0f0ull,
  0xff00ff00ff00ff00ull, 0xffff0000ffff0000ull, 0xffffffff00000000ull,
};

static int lglfloorld (int n) {
  assert (n >= 0);
  if (n < (1<<8)) return lglfloorldtab[n];
  if (n < (1<<16)) return 8 + lglfloorldtab[n>>8];
  if (n < (1<<24)) return 16 + lglfloorldtab[n>>16];
  return 24 + lglfloorldtab[n>>24];
}

static int lglispow2 (int n) {
  assert (0 <= n && n <= INT_MAX);
  return !(n & (n - 1));
}

static int lglceilld (int n) {
  int res = lglfloorld (n);
  if (!lglispow2 (n)) res++;
  return res;
}

static void lglchkflt (Flt a) {
#ifndef NDEBUG
  assert (a >= 0);
  assert (FLTMAX >= (uint64_t) a);
#else
  (void) a;
#endif
}

static Exp lglexp (Flt a) { 
  Exp res = a >> FLTPRC;
  assert (0 <= res && res <= EXPMAX);
  res -= EXPZRO;
  return res;
}

static Mnt lglmnt (Flt a) {
  Mnt res = a & MNTMAX;
  res |= MNTBIT;
  assert (res <= MNTMAX);
  return res;
}

static Flt lglflt (Exp e, Mnt m) {
  Flt res;
  if (!m) return FLTMIN;
  if (m < MNTBIT) {
    while (!(m & MNTBIT)) {
      m <<= 1;
      if (e > INT_MIN) e--;
      else break;
    }
  } else  {
    while (m > MNTMAX) {
       m >>= 1;
       if (e > INT_MIN) e++;
       else break;
    }
  }
  if (e < -EXPZRO) return FLTMIN;
  if (e > EXPMAX - EXPZRO) return FLTMAX;
  e += EXPZRO;
  assert (0 <= e && e <= EXPMAX);
  assert (m <= MNTMAX);
  assert (m & MNTBIT);
  res = m & ~MNTBIT;
  res |= ((Flt)e) << FLTPRC;
  return res;
}

static Flt lglrat (unsigned n, unsigned d) {
  Mnt m;
  Exp e;
  if (!n) return FLTMIN;
  if (!d) return FLTMAX;
  m = n;
  e = 0;
  while (!(m & (1ull << 63))) m <<= 1, e--;
  m /= d;
  return lglflt (e, m);
}

#ifndef NLGLOG
double lglflt2dbl (Flt a) {
  return lglmnt (a) * pow (2.0, lglexp (a));
}

static const char * lglflt2str (LGL * lgl, Flt a) {
  double d, e;
  if (a == FLTMIN) return "0";
  if (a == FLTMAX) return "inf";
  d = lglmnt (a);
  d /= 4294967296ll;
  e = lglexp (a);
  e += 32;
  sprintf (lgl->scstr, "%.6fd%+03.0f", d, e);
  return lgl->scstr;
}
#endif

static Flt lgladdflt (Flt a, Flt b) {
  Exp e, f, g;
  Mnt m, n, o;
  lglchkflt (a);
  lglchkflt (b);
  if (a == FLTMAX) return FLTMAX;
  if (b == FLTMAX) return FLTMAX;
  if (a == FLTMIN) return b;
  if (b == FLTMIN) return a;
  e = lglexp (a);
  f = lglexp (b);
  if (e < f) g = e, e = f, f = g, o = a, a = b, b = o;
  m = lglmnt (a);
  n = lglmnt (b);
  m += n >> (e - f);
  return lglflt (e, m);
}

static Flt lglmulflt (Flt a, Flt b) {
  Exp e, ea, eb;
  Mnt m, ma, mb;
  lglchkflt (a);
  lglchkflt (b);
  if (a == FLTMAX) return FLTMAX;
  if (b == FLTMAX) return FLTMAX;
  if (a == FLTMIN) return FLTMIN;
  if (b == FLTMIN) return FLTMIN;
  ea = lglexp (a); eb = lglexp (b);
  if (ea > 0 && eb > 0 && (INT_MAX - ea < eb)) return FLTMAX;
  e = ea + eb;
  if (e > EXPMAX - EXPZRO - 32) return FLTMAX;
  e += 32;
  ma = lglmnt (a); mb = lglmnt (b);
  ma >>= 1; mb >>= 1;
  m = ma * mb;
  assert (3ull << 62);
  m >>= 30;
  return lglflt (e, m);
}

static Flt lglshflt (Flt a, int s) {
  Exp e;
  Mnt m;
  if (a == FLTMAX) return FLTMAX;
  if (a == FLTMIN) return FLTMIN;
  assert (0 <= s);
  e = lglexp (a);
  if (e < INT_MIN + s) return FLTMIN;
  e -= s;
  m = lglmnt (a);
  return lglflt (e, m);
}

static void lgldie (const char * msg, ...) {
  va_list ap;
  printf ("*** internal error in '%s': ", __FILE__);
  va_start (ap, msg);
  vprintf (msg, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
  exit (0);
}

static void lglmsgstart (LGL * lgl, int level) {
  assert (lgl->opts.verbose.val >= level); 
  if (lgl->msglock.lock) lgl->msglock.lock (lgl->msglock.state);
  fputc ('c', stdout);
  if (lgl->tid >= 0) printf (" %d", lgl->tid);
  fputc (' ', stdout);
}

static void lglmsgend (LGL * lgl) {
  fputc ('\n', stdout);
  fflush (stdout);
  if (lgl->msglock.unlock) lgl->msglock.unlock (lgl->msglock.state);
}

static void lglprt (LGL * lgl, int level, const char * msg, ...) {
  va_list ap;
  if (lgl->opts.verbose.val < level) return;
  lglmsgstart (lgl, level);
  va_start (ap, msg);
  vprintf (msg, ap);
  va_end (ap);
  lglmsgend (lgl);
}

#ifndef NLGLOG
static void lglogstart (LGL * lgl, int level, const char * msg, ...) {
  va_list ap;
  assert (lgl->opts.log.val >= level); 
  if (lgl->msglock.lock) lgl->msglock.lock (lgl->msglock.state);
  fputs ("c ", stdout);
  if (lgl->tid >= 0) printf ("%d ", lgl->tid);
  printf ("LOG%d %d ", level, lgl->level);
  va_start (ap, msg);
  vprintf (msg, ap);
  va_end (ap);
}

#define lglogend lglmsgend
#endif

void lglsetid (LGL * lgl, int tid) { assert (lgl->tid < 0); lgl->tid = tid; }

void lglseterm (LGL * lgl, int (*fun)(void*), void * state) {
  lgl->term.fun = fun;
  lgl->term.state = state;
}

void lglsetproduce (LGL * lgl, void (*produce) (void*, int), void * state) {
  lgl->produce.fun = produce;
  lgl->produce.state = state;
}

void lglsetconsume (LGL * lgl,
                    void (*consume) (void*, int **, int **),
		    void * state) {  
  lgl->consume.fun =  consume;
  lgl->consume.state = state;
}

void lglsetconsumed (LGL * lgl, void (*consumed) (void*, int), void * state) {  
  lgl->consumed.fun =  consumed;
  lgl->consumed.state = state;
}

void lglsetmsglock (LGL * lgl,
		    void (*lock)(void*), void (*unlock)(void*),
		    void * state) {
  lgl->msglock.lock = lock;
  lgl->msglock.unlock = unlock;
  lgl->msglock.state = state;
}

void lglsetime (LGL * lgl, double (*time)(void)) { lgl->getime = time; }

static void lglinc (LGL * lgl, size_t bytes) {
  lgl->stats.bytes.current += bytes;
  if (lgl->stats.bytes.max < lgl->stats.bytes.current) {
    lgl->stats.bytes.max = lgl->stats.bytes.current;
    LOG (5, "maximum allocated %ld bytes", lgl->stats.bytes.max);
  }
}

static void lgldec (LGL * lgl, size_t bytes) {
  assert (lgl->stats.bytes.current >= bytes);
  lgl->stats.bytes.current -= bytes;
}

static void * lglmalloc (LGL * lgl, size_t bytes) {
  void * res;
  res = malloc (bytes);
  if (!res) lgldie ("out of memory allocating %ld bytes", bytes);
  LOG (5, "allocating %p with %ld bytes", res, bytes);
  lglinc (lgl, bytes);
  memset (res, 0, bytes);
  return res;
}

static void lglfree (LGL * lgl, void * ptr, size_t bytes) {
  if (!ptr) { assert (!bytes); return; }
  lgldec (lgl, bytes);
  LOG (5, "freeing %p with %ld bytes", ptr, bytes);
  free (ptr);
}

static void * lglrealloc (LGL * lgl, void * ptr, size_t old, size_t new) {
  void * res;
  assert (!ptr == !old);
  if (!ptr) return lglmalloc (lgl, new);
  if (!new) { lglfree (lgl, ptr, old); return 0; }
  lgldec (lgl, old);
  res = realloc (ptr, new);
  if (!res) lgldie ("out of memory reallocating %ld to %ld bytes", old, new);
  LOG (5, "reallocating %p to %p from %ld to %ld bytes", ptr, res, old, new);
  lglinc (lgl, new);
  if (new > old) memset (res + old, 0, new - old);
  return res;
}

static int lglfullstk (Stk * s) { return s->top == s->end; }
static int lglmtstk (Stk * s) { return s->top == s->start; }
static size_t lglcntstk (Stk * s) { return s->top - s->start; }
static size_t lglszstk (Stk * s) { return s->end - s->start; }

static int lglpeek (Stk * s, int pos) {
  assert (0 <= pos && pos < lglszstk (s));
  return s->start[pos];
}

static void lglpoke (Stk * s, int pos, int val) {
  assert (0 <= pos && pos <= lglszstk (s));
  s->start[pos] = val;
}

static void lglenlstk (LGL * lgl, Stk * s) {
  size_t old_size = lglszstk (s);
  size_t new_size = old_size ? 2 * old_size : 1;
  size_t count = lglcntstk (s);
  RSZ (s->start, old_size, new_size);
  s->top = s->start + count;
  s->end = s->start + new_size;
}

static void lglrelstk (LGL * lgl, Stk * s) {
  DEL (s->start, lglszstk (s));
  s->start = s->top = s->end = 0;
}

static void lglshrstk (LGL * lgl, Stk * s, int new_size) {
  size_t old_size, count = lglcntstk (s);
  assert (new_size >= 0);
  assert (count <= new_size);
  if (new_size > 0) {
    old_size = lglszstk (s);
    RSZ (s->start, old_size, new_size);
    s->top = s->start + count;
    s->end = s->start + new_size;
  } else lglrelstk (lgl, s);
}

static void lglfitstk (LGL * lgl, Stk * s) {
  lglshrstk (lgl, s, lglcntstk (s));
}

static void lglpushstk (LGL * lgl, Stk * s, int elem) {
  if (lglfullstk (s)) lglenlstk (lgl, s);
  *s->top++ = elem;
}

#if !defined(NDEBUG) || !defined(NLGLOG)
static void lglrmstk (Stk * s, int elem) {
  int * p, * q;
  for (p = s->start; p < s->top; p++)
    if (*p == elem) break;
  assert (p < s->top);
  q = p++;
  while (p < s->top)
    *q++ = *p++;
  s->top = q;
}
#endif

static int lglpopstk (Stk * s) { assert (!lglmtstk (s)); return *--s->top; }

static int lgltopstk (Stk * s) { assert (!lglmtstk (s)); return s->top[-1]; }

static void lglrststk (Stk * s, int newsz) {
  assert (0 <= newsz && newsz <= lglcntstk (s));
  s->top = s->start + newsz;
}

static void lglredstk (LGL * lgl, Stk * s, int minsize, int pow2smaller) {
  size_t oldsize, count, limit, newsize;
  assert (pow2smaller >= 2);
  oldsize = lglszstk (s);
  if (oldsize <= minsize) return;
  count = lglcntstk (s);
  limit = oldsize >> pow2smaller;
  if (count > limit) return;
  newsize = oldsize / 2;
  if (newsize > 0) {
    RSZ (s->start, oldsize, newsize);
    s->top = s->start + count;
    s->end = s->start + newsize;
  } else lglrelstk (lgl, s);
}

static void lglclnstk (Stk * s) { lglrststk (s, 0); }

#define OPT(SHRT,LNG,VAL,MIN,MAX,DESCRP) \
do { \
  Opt * opt = &res->opts.LNG; \
  opt->shrt = SHRT; \
  opt->lng = #LNG; \
  opt->val = VAL; \
  assert (MIN <= VAL); \
  opt->min = MIN; \
  assert (VAL <= MAX); \
  opt->max = MAX; \
  opt->descrp = DESCRP; \
} while (0)

LGL * lglinit (void) {
  const int K = 1000, M = K * K, I = INT_MAX;
  LGL * res;
  int i;

  assert (sizeof (long) == sizeof (void*));

  assert (INT_MAX > LOCKED);
  assert (LOCKED > COLLECT);
  assert (COLLECT > REMOVED);
  assert (REMOVED > ((MAXVAR << RMSHFT) | MASKCS | REDCS));
  assert (REMOVED > MAXREDLIDX);
  assert (REMOVED > MAXIRRLIDX);

  assert (INT_MAX > ((MAXREDLIDX << GLUESHFT) | GLUEMASK));
  assert (INT_MAX > ((MAXIRRLIDX << RMSHFT) | MASKCS | REDCS));

  res = malloc (sizeof *res);
  CLRPTR (res);

  for (i = 0; i < MAXLDFW; i++) res->freewchs[i] = INT_MAX;

  lglpushstk (res, &res->control, 0);
  lglpushstk (res, &res->wchs, INT_MAX);
  lglpushstk (res, &res->wchs, INT_MAX);

  res->bias = 1;
  res->measureagility = 1;
  res->propred = 1;
  res->ignlidx = -1;
  res->tid = -1;

  OPT(0,agile,23,0,100,"agility limit for restarts");
  OPT(0,bias,0,-1,1,"variable decision order initial bias");
  OPT(0,block,1,0,1,"enable initial blocked clause elimination");
  OPT(0,blkmaxeff,100*M,0,I,"maximal effort in blocked clause elimination");
  OPT(0,blkmineff,4*M,0,I,"minimal effort in blocked clause elimination");
  OPT(0,blkreleff,40,0,K,"relative effort in blocked clause elimination");
  OPT(0,blkocclim,10000,3,I,
      "maximum number of occurrences in blocked clause elimination");
#ifndef NDEBUG
  OPT('c',check,0,0,2,"check level");
#endif
  OPT(0,decompose,1,0,1,"enable decompose");
  OPT(0,dcpcintinc,13*K,10,100*K,"decompose conflict interval increment");
  OPT(0,dcpvintinc,10*M,10,200*M,"decompose prop interval increment");
  OPT(0,distill,1,0,1,"enable distillation");
  OPT(0,dstcintinc,10*K,10,100*K,"distill conflict interval increment");
  OPT(0,dstvintinc,10*M,10,200*M,"distill prop interval increment");
  OPT(0,dstmaxeff,50*M,0,I,"maximal effort in distilling");
  OPT(0,dstmineff,50*K,0,I,"minimal effort in distilling");
  OPT(0,dstreleff,10,0,K,"relative effort in distilling");
  OPT(0,defragfree,100,10,K,"defragmentation free watches limit");
  OPT(0,defragint,M,100,I,"defragmentation pushed watches interval");
  OPT(0,elim,1,0,1,"enable eliminiation");
  OPT(0,elmcintinc,20*K,10,M,"eliminiation conflict interval increment");
  OPT(0,elmvintinc,10*M,1,200*M,"eliminiation prop interval increment");
  OPT(0,elmaxeff,60*M,0,I,"maximal effort in eliminiation");
  OPT(0,elmineff,1*M,0,I,"minimal effort in eliminiation");
  OPT(0,elmreleff,30,0,10*K,"relative effort in eliminiation");
  OPT(0,elmocclim,20000,3,I,"maximum number of occurences in elimination");
  OPT(0,elmreslim,10000,2,I,"maximum resolvent size in elimination");
  OPT(0,elmpenlim,2,0,100,"minimum eliminated variables penalty limit");
  OPT(0,gccintinc,K,10,M, "gc conflict interval increment");
  OPT(0,gcvintinc,500*K,10,10*M, "gc prop interval increment");
  OPT(0,hte,1,0,1,"enable hidden tautology elimination");
  OPT(0,htemaxeff,500*M,0,I,"maximal effort in hidden tautology elimination");
  OPT(0,htemineff,4*M,0,I,"minimal effort in hidden tautology elimination");
  OPT(0,htereleff,200,0,K,"relative effort in hidden tautology elimination");
  OPT(0,lhbr,1,0,1,"enable lazy hyber binary reasoning");
#ifndef NLGLOG
  OPT('l',log,0,0,5,"log level");
#endif
  OPT(0,phase,0,-1,1,"default phase");
  OPT(0,plain,0,0,1,"plain mode disables all preprocessing");
  OPT(0,probe,1,0,1,"enable probing");
  OPT(0,prbcintinc,K,10,100*K,"probing conflict interval increment");
  OPT(0,prbvintinc,5*M,1,100*M,"probing prop interval increment");
  OPT(0,prbmaxeff,200*M,0,I,"maximal effort in probing");
  OPT(0,prbmineff,100*K,0,I,"minimal effort in probing");
  OPT(0,prbreleff,200,0,10*K,"relative effort in probing");
  OPT(0,seed,0,0,I,"random number generator seed");
  OPT(0,smallve,1,0,1,"enable small number variables elimination");
  OPT(0,smallvevars,FUNVAR,4,FUNVAR,
      "variables in small number variables elimination");
  OPT(0,rebias,1,0,1,"enable rebiasing phases");
  OPT(0,rebiasint,10000,100,I/2,"rebias interval");
  OPT(0,randec,1,0,1,"enable random decisions");
  OPT(0,randecint,1000,2,I/2,"random decision interval");
  OPT(0,redlinit,10*K,10,100*K,"initial reduce limit");
  OPT(0,redlinc,K,2,10*K,"reduce limit increment");
  OPT(0,redlmininc,100,10,100*K,"minimum reduce limit increment");
  OPT(0,restartint,100,10,I,"restart interval");
  OPT(0,scincinc,50,0,10*K,"score increment increment in per mille");
  OPT(0,transred,1,0,1,"enable transitive reduction");
  OPT(0,trdcintinc,2*K,1,M,"transitive reduction prop interval increment");
  OPT(0,trdvintinc,M,1,200*M,"transitive reduction prop interval increment");
  OPT(0,trdmaxeff,10*M,0,I,"maximal effort in transitive reduction");
  OPT(0,trdmineff,100*K,0,I,"minimial effort in transitive reduction");
  OPT(0,trdreleff,4,0,10*K,"relative effort in transitive reduction");
  OPT('v',verbose,0,0,3,"verbosity level");
  OPT('w',witness,1,0,1,"print witness");

  res->scinc = lglflt (0, 1);
  res->scincf = lglrat (1000 + res->opts.scincinc.val, 1000);
  res->scincl = lglflt (SCINCLEXP, 1);

  res->getime = lglprocesstime;

#ifndef NLGLPICOSAT
#ifndef NDEBUG
  picosat_init ();
  picosat_set_prefix ("c PST ");
#endif
  res->picosat.chk = 1;
#endif
  return res;
}

static int lglmaxoptnamelen (LGL * lgl) {
  int res = 0, len;
  Opt * o;
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++)
    if ((len = strlen (o->lng)) > res) 
      res = len;
  return res;
}

void lglusage (LGL * lgl) {
  int len = lglmaxoptnamelen (lgl);
  char fmt[20];
  Opt * o;
  sprintf (fmt, "--%%-%ds", len);
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    printf (fmt, o->lng);
    if (o->shrt) printf (" | -%c", o->shrt); else printf ("     ");
    printf ("    %s\n", o->descrp);
  }
}

void lglopts (LGL * lgl, const char * prefix) {
  Opt * o;
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++)
    printf ("%s--%s=%d\n", prefix, o->lng, o->val);
}

void lglrgopts (LGL * lgl) {
  Opt * o;
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++)
    printf ("%s %d %d %d\n", o->lng, o->val, o->min, o->max);
}

int lglhasopt (LGL * lgl, const char * opt) {
  Opt * o;
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    if (!opt[1] && o->shrt == opt[0]) return 1;
    if (!strcmp (o->lng, opt)) return 1;
  }
  return 0;
}

void lglsetopt (LGL * lgl, const char * opt, int val) {
  Opt * o;
  ABORTIFNOTINSTATE (UNUSED);
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    if (!opt[1] && o->shrt == opt[0]) break;
    if (!strcmp (o->lng, opt)) break;
  }
  if (o > LASTOPT (lgl)) return;
  if (val < o->min) val = o->min;
  if (o->max < val) val = o->max;
  o->val = val;
}

int lglgetopt (LGL * lgl, const char * opt) {
  Opt * o;
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    if (!opt[1] && o->shrt == opt[0]) return o->val;
    if (!strcmp (o->lng, opt)) return o->val;
  }
  return 0;
}

static unsigned lglrand (LGL * lgl) {
  unsigned rng = lgl->rng, res = rng;
  rng *= 1664525u;
  rng += 1013904223u;
  LOG (4, "rng %u", rng);
  lgl->rng = rng;
  return res;
}

static unsigned lglgcd (unsigned a, unsigned b) {
  unsigned tmp;
  assert (a), assert (b);
  if (a < b) SWAP (unsigned, a, b);
  while (b) tmp = b, b = a % b, a = tmp;
  return a;
}

static void lglrszvars (LGL * lgl, int new_size) {
  int old_size = lgl->szvars;
  assert (lgl->nvars <= new_size);
  RSZ (lgl->vals, old_size, new_size);
  RSZ (lgl->i2e, old_size, new_size);
  RSZ (lgl->dvars, old_size, new_size);
  RSZ (lgl->avars, old_size, new_size);
  lgl->szvars = new_size;
}

static void lglenlvars (LGL * lgl) {
  size_t old_size, new_size;
  old_size = lgl->szvars;
  new_size = old_size ? 2 * old_size : 4;
  LOG (3, "enlarging variables from %ld to %ld", old_size, new_size);
  lglrszvars (lgl, new_size);
}

static void lglredvars (LGL * lgl) {
  size_t old_size, new_size;
  old_size = lgl->szvars;
  new_size = lgl->nvars;
  if (new_size == old_size) return;
  LOG (3, "reducing variables from %ld to %ld", old_size, new_size);
  lglrszvars (lgl, new_size);
}

static int lglabs (int a) { return a < 0 ? -a : a; }
// static int lglmin (int a, int b) { return a < b ? a : b; }
static int lglmax (int a, int b) { return a > b ? a : b; }

static DVar * lgldvar (LGL * lgl, int lit) {
  assert (2 <= lglabs (lit) && lglabs (lit) < lgl->nvars);
  return lgl->dvars + lglabs (lit);
}

static AVar * lglavar (LGL * lgl, int lit) {
  assert (2 <= lglabs (lit) && lglabs (lit) < lgl->nvars);
  return lgl->avars + lglabs (lit);
}

static int * lgldpos (LGL * lgl, int lit) {
  AVar * av;
  int * res;
  av = lglavar (lgl, lit);
  res = &av->pos;
  return res;
}

static Scr lgldscore (LGL * lgl, int lit) {
  AVar * av;
  Scr res;
  av = lglavar (lgl, lit);
  res = av->score;
  return res;
}

static int lgldcmp (LGL * lgl, Scr s, int l, Scr t, int k) {
  int res;
  if (s < t) return -1;
  if (s > t) return 1;
  res = l - k;
  res *= lgl->bias;
  return res;
}
 
#ifndef NDEBUG
static void lglchkdsched (LGL * lgl) {
  int * p, parent, child, ppos, cpos, size, i, tmp;
  Stk * s = &lgl->dsched;
  Scr pscr, cscr;
  size = lglcntstk (s);
  p = s->start;
  for (ppos = 0; ppos < size; ppos++) {
    parent = p[ppos];
    tmp = *lgldpos (lgl, parent);
    assert (ppos == tmp);
    pscr = lgldscore (lgl, parent);
    for (i = 0; i <= 1; i++) {
      cpos = 2*ppos + 1 + i;
      if (cpos >= size) continue;
      child = p[cpos];
      tmp = *lgldpos (lgl, child);
      assert (cpos == tmp);
      cscr = lgldscore (lgl, child);
      assert (lgldcmp (lgl, pscr, parent, cscr, child) >= 0);
    }
  }
}
#endif

static void lgldup (LGL * lgl, int lit) {
  int child = lit, parent, cpos, ppos, * p, * cposptr, * pposptr;
  Stk * s = &lgl->dsched;
  Scr lscr, pscr;
  p = s->start;
  lscr = lgldscore (lgl, child);
  cposptr = lgldpos (lgl, child);
  cpos = *cposptr;
  assert (cpos >= 0);
  while (cpos > 0) {
    ppos = (cpos - 1)/2;
    parent = p[ppos];
    pscr = lgldscore (lgl, parent);
    if (lgldcmp (lgl, pscr, parent, lscr, lit) >= 0) break;
    pposptr = lgldpos (lgl, parent);
    assert (*pposptr == ppos);
    p[cpos] = parent;
    *pposptr = cpos;
    LOGDSCHED (5, parent, "down from %d", ppos);
    cpos = ppos;
    child = parent;
  }
  if (*cposptr == cpos) return;
#ifndef NLGLOG
  ppos = *cposptr;
#endif
  *cposptr = cpos;
  p[cpos] = lit;
  LOGDSCHED (5, lit, "up from %d", ppos);
#ifndef NDEBUG
  if (lgl->opts.check.val >= 2) lglchkdsched (lgl);
#endif
}

static void lglddown (LGL * lgl, int lit) {
  int parent = lit, child, right, ppos, cpos;
  int * p, * pposptr, * cposptr, size;
  Scr lscr, cscr, rscr;
  Stk * s = &lgl->dsched;
  size = lglcntstk (s);
  p = s->start;
  lscr = lgldscore (lgl, parent);
  pposptr = lgldpos (lgl, parent);
  ppos = *pposptr;
  assert (0 <= ppos);
  for (;;) {
    cpos = 2*ppos + 1;
    if (cpos >= size) break;
    child = p[cpos];
    cscr = lgldscore (lgl, child);
    if (cpos + 1 < size) {
      right = p[cpos + 1];
      rscr = lgldscore (lgl, right);
      if (lgldcmp (lgl, cscr, child, rscr, right) < 0)
        cpos++, child = right, cscr = rscr;
    }
    if (lgldcmp (lgl, cscr, child, lscr, lit) <= 0) break;
    cposptr = lgldpos (lgl, child);
    assert (*cposptr = cpos);
    p[ppos] = child;
    *cposptr = ppos;
    LOGDSCHED (5, child, "up from %d", cpos);
    ppos = cpos;
    parent = child;
  }
  if (*pposptr == ppos) return;
#ifndef NLGLOG
  cpos = *pposptr;
#endif
  *pposptr = ppos;
  p[ppos] = lit;
  LOGDSCHED (5, lit, "down from %d", cpos);
#ifndef NDEBUG
  if (lgl->opts.check.val >= 2) lglchkdsched (lgl);
#endif
}

static void lgldsched (LGL * lgl, int lit) {
  int * p = lgldpos (lgl, lit);
  Stk * s = &lgl->dsched;
  assert (*p < 0);
  *p = lglcntstk (s);
  lglpushstk (lgl, s, lit);
  lgldup (lgl, lit);
  lglddown (lgl, lit);
  LOGDSCHED (4, lit, "pushed");
}

static int lglpopdsched (LGL * lgl) {
  Stk * s = &lgl->dsched;
  int res, last, cnt, * p;
  AVar * av;
  assert (!lglmtstk (s));
  res = *s->start;
  LOGDSCHED (4, res, "popped");
  av = lglavar (lgl, res);
  assert (!av->pos);
  av->pos = -1;
  last = lglpopstk (s);
  cnt = lglcntstk (s);
  if (!cnt) { assert (last == res); return res; }
  p = lgldpos (lgl, last);
  assert (*p == cnt);
  *p = 0;
  *s->start = last;
  lglddown (lgl, last);
  return res;
}

static void lgldreschedule (LGL * lgl) {
  Stk * s = &lgl->dsched;
  int idx, i, pos, cnt = lglcntstk (s);
  AVar * av;
  LOG (1, "rescheduling %d variables", cnt);
  pos = 0;
  s->top = s->start;
  for (i = 0; i < cnt; i++) {
    assert (pos <= i);
    assert (s->start + pos == s->top);
    idx = s->start[i];
    av = lglavar (lgl, idx);
    if (av->type != FREEVAR) { av->pos = -1; continue; }
    assert (av->pos == i);
    s->start[pos] = idx;
    av->pos = pos++;
    s->top++;
    lgldup (lgl, idx);
    lglddown (lgl, idx);
  }
  LOG (1, "new schedule with %d variables", lglcntstk (s));
  lglfitstk (lgl, s);
#ifndef NDEBUG
  if (lgl->opts.check.val >= 1) lglchkdsched (lgl);
#endif
}

static void lglrescore (LGL * lgl) {
  Stk * s = &lgl->dsched;
  int idx, pos, cnt;
  AVar * av;
  lgl->stats.rescored++;
  cnt = lglcntstk (s);
  for (pos = 0; pos < cnt; pos++) {
    idx = s->start[pos];
    av = lglavar (lgl, idx);
    assert (av->pos == pos);
    av->score = lglshflt (av->score, SCINCLEXP + 1);
    LOG (3, "rescored score %s of scheduled %d", 
         lglflt2str (lgl, av->score), idx);
  }
  for (idx = 2; idx < lgl->nvars; idx++) {
    av = lglavar (lgl, idx);
    if (av->pos >= 0) continue;
    av->score = lglshflt (av->score, SCINCLEXP + 1);
    LOG (3, "rescored score %s of unscheduled %d", 
         lglflt2str (lgl, av->score), idx);
  }
  lgldreschedule (lgl);
  lgl->scinc = lglshflt (lgl->scinc, SCINCLEXP + 1);
  LOG (2, "new scinc %s", lglflt2str (lgl, lgl->scinc));
}

static void lglbumpscinc (LGL * lgl) {
  lgl->scinc = lglmulflt (lgl->scinc, lgl->scincf);
  LOG (3, "bumped scinc %s", lglflt2str (lgl, lgl->scinc));
  if (lgl->scinc >= lgl->scincl) {
    LOG (2, "rescore limit of %s hit", lglflt2str (lgl, lgl->scincl));
    lglrescore (lgl);
  }
}

static int lglnewvar (LGL * lgl) {
  int res;
  AVar * av;
  DVar * dv;
  if (lgl->nvars == lgl->szvars) lglenlvars (lgl);
  if (lgl->nvars) res = lgl->nvars++;
  else res = 2, lgl->nvars = 3;
  assert (res < lgl->szvars);
  if (res > MAXVAR) lgldie ("more than %d variables", MAXVAR - 1);
  assert (((res << RMSHFT) >> RMSHFT) == res);
  assert (((-res << RMSHFT) >> RMSHFT) == -res);
  LOG (3, "new internal variable %d", res);
  dv = lgl->dvars + res;
  CLRPTR (dv);
  av = lgl->avars + res;
  CLRPTR (av);
  av->pos = -1;
  av->score = FLTMIN;
  lgldsched (lgl, res);
  lgl->unassigned++;
  return res;
}

static Val lglval (LGL * lgl, int lit) {
  int idx = lglabs (lit);
  Val res;
  assert (2 <= idx && idx < lgl->nvars);
  res = lgl->vals[idx];
  if (lit < 0) res = -res;
  return res;
}

static int lglsgn (int lit) { return (lit < 0) ? -1 : 1; }

static int lglerepr (LGL * lgl, int elit) {
  int res, idx, next;
  assert (lglabs (elit) <= (INT_MAX >> 1));
  next = 2*elit;
  do {
    res = next/2;
    idx = lglabs (res);
    assert (idx);
    next = lglpeek (&lgl->e2i, idx);
    if (res < 0) next = -next;
  } while (next & 1);
  return res;
}

static int lglimport (LGL * lgl, int elit) {
  int res, repr, eidx = lglabs (elit), repridx;
  assert (elit);
  while (eidx >= lglcntstk (&lgl->e2i)) 
    lglpushstk (lgl, &lgl->e2i, 0);
  repr = lglerepr (lgl, elit);
  repridx = lglabs (repr);
  res = lglpeek (&lgl->e2i, repridx);
  if (res) {
    assert (!(res & 1));
    res /= 2;
  } else {
    res = lglnewvar (lgl);
    assert (lglabs (res) <= (INT_MAX/2));
    lglpoke (&lgl->e2i, repridx, 2*res);
    lgl->i2e[res] = eidx;
    LOG (3, "mapping external variable %d to %d", eidx, res);
  }
  if (repr < 0) res = -res;
  LOG (2, "importing %d as %d", elit, res);
  return res;
}

static int * lglidx2lits (LGL * lgl, int red, int lidx) {
  int * res, glue = 0;
  Stk * s;
  assert (red == 0 || red == REDCS);
  assert (0 <= lidx);
  if (red) {
    glue = lidx & GLUEMASK;
    lidx >>= GLUESHFT;
    assert (lidx <= MAXREDLIDX);
    s = &lgl->red[glue].lits;
  } else s = &lgl->irr;
  res = s->start + lidx;
#ifndef NDEBUG
  if (red && glue == MAXGLUE) assert (res < s->end);
  else assert (res < s->top);
#endif
  return res;
}

#ifndef NLGLOG
static const char * lglred2str (int red) {
  assert (!red || red == REDCS);
  return red ? "redundant" : "irredundant";
}
#endif

static int lglisfree (LGL * lgl, int lit) {
  return lglavar (lgl, lit)->type == FREEVAR;
}

static int lgliselim (LGL * lgl, int lit) {
  return lglavar (lgl, lit)->type == ELIMVAR;
}

static int lglexport (LGL * lgl, int ilit) {
  assert (2 <= lglabs (ilit) && lglabs (ilit) < lgl->nvars);
  return lgl->i2e[lglabs (ilit)] * lglsgn (ilit);
}

static void lglassign (LGL * lgl, int lit, int r0, int r1) {
  AVar * av = lglavar (lgl, lit);
  int idx, phase;
  LOGREASON (2, lit, r0, r1, "assign %d through", lit);
#ifndef NDEBUG
  {
    int * p, tag, red, other, other2, * c, lidx, found;
    tag = r0 & MASKCS;
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      assert (lglval (lgl, other) < 0);
      if (tag == TRNCS) {
	other2 = r1;
	assert (lglval (lgl, other2) < 0);
      }
    } else if (tag == LRGCS) {
      red = r0 & REDCS;
      lidx = r1;
      c = lglidx2lits (lgl, red, lidx);
      found = 0;
      for (p = c; (other = *p); p++)
	if (other == lit) found++;
	else assert (lglval (lgl, other) < 0);
      assert (found == 1);
    } else assert (tag == DECISION || tag == UNITCS);
  }
  assert (!lglval (lgl, lit));
  assert (lgl->unassigned > 0);
  assert (!lgliselim (lgl, lit));
#endif
  idx = lglabs (lit);
  phase = lglsgn (lit);
  lgl->vals[idx] = phase;
  if (lgl->measureagility) {
    lgl->flips -= lgl->flips/10000;
    if (av->phase && av->phase != phase) lgl->flips += 1000;
    av->phase = phase;
  }
#ifndef NDEBUG
  if (phase < 0) av->wasfalse = 1; else av->wasfalse = 0;
#endif
  av->level = lgl->level;
  if (!lgl->level) {
    if (av->type == EQUIVAR) {
      assert (lgl->stats.equivalent.current > 0);
      lgl->stats.equivalent.current--;
    } else {
      assert (av->type == FREEVAR);
      av->type = FIXEDVAR;
    }
    lgl->stats.fixed.sum++;
    lgl->stats.fixed.current++;
    lgl->stats.prgss++;
    av->rsn[0] = UNITCS | (lit << RMSHFT);
    av->rsn[1] = 0;
    if (lgl->produce.fun)  {
      LOG (2, "trying to export internal unit %d external %d\n",
           lgl->tid, lit, lglexport (lgl, lit));
      lgl->produce.fun (lgl->produce.state, lglexport (lgl, lit));
      LOG (2, "exporting internal unit %d external %d\n",
              lgl->tid, lit, lglexport (lgl, lit));
    }
  } else {
    av->rsn[0] = r0;
    av->rsn[1] = r1;
  }
  lglpushstk (lgl, &lgl->trail, lit);
  lgl->unassigned--;
#ifndef NLGLSTATS
  if ((r0 & REDCS) && (r0 & MASKCS) == LRGCS) {
    int glue = r1 & GLUEMASK;
    Lir * lir = lgl->red + glue;
    lir->forcing++;
    assert (lir->forcing > 0);
  }
#endif
}

static void lglf2rce (LGL * lgl, int lit, int other, int red) {
  assert (lglval (lgl, other) < 0);
  assert (!red || red == REDCS);
  assert (!lgliselim (lgl, other));
  lglassign (lgl, lit, ((other << RMSHFT) | BINCS | red), 0);
}

static void lglf3rce (LGL * lgl, int lit, int other, int other2, int red) {
  assert (lglval (lgl, other) < 0);
  assert (lglval (lgl, other2) < 0);
  assert (!lgliselim (lgl, other));
  assert (!lgliselim (lgl, other2));
  assert (!red || red == REDCS);
  lglassign (lgl, lit, ((other << RMSHFT) | TRNCS | red), other2);
}

static void lglflrce (LGL * lgl, int lit, int red, int lidx) {
#ifndef NDEBUG
  int * p = lglidx2lits (lgl, red, lidx), other;
  while ((other = *p++)) {
    assert (!lgliselim (lgl, other));
    if (other != lit) assert (lglval (lgl, other) < 0);
  }
  assert (red == 0 || red == REDCS);
#endif
  lglassign (lgl, lit, red | LRGCS, lidx);
}

static void lglunassign (LGL * lgl, int lit) {
  int idx = lglabs (lit), r0, r1, tag, lidx, glue;
  AVar * av = lglavar (lgl, lit);
  LOG (2, "unassign %d", lit);
  assert (lglval (lgl, lit) > 0);
  assert (lgl->vals[idx] == lglsgn (lit));
  lgl->vals[idx] = 0;
  lgl->unassigned++;
  assert (lgl->unassigned > 0);
  if (av->pos < 0) lgldsched (lgl, idx);
  r0 = av->rsn[0];
  if (!(r0 & REDCS)) return;
  tag = r0 & MASKCS;
  if (tag != LRGCS) return;
  r1 = av->rsn[1];
  glue = r1 & GLUEMASK;
  if (glue < MAXGLUE) return;
  lidx = r1 >> GLUESHFT;
  LOG (2, "eagerly deleting maximum glue clause at %d", lidx);
  lglrststk (&lgl->red[glue].lits, lidx);
  lglredstk (lgl, &lgl->red[glue].lits, (1<<20), 3);
}

static int lglevel (LGL * lgl, int lit) { return lglavar (lgl, lit)->level; }

static Val lglfixed (LGL * lgl, int lit) {
  if (lglevel (lgl, lit) > 0) return 0;
  return lglval (lgl, lit);
}

static void lglbacktrack (LGL * lgl, int level) {
  int lit;
  assert (level >= 0);
  LOG (2, "backtracking to level %d", level);
  assert (level <= lgl->level);
  while (!lglmtstk (&lgl->trail)) {
    lit = lgltopstk (&lgl->trail);
    assert (lglabs (lit) > 1);
    if (lglevel (lgl, lit) <= level) break;
    lglunassign (lgl, lit);
    lgl->trail.top--;
  }
  lgl->level = level;
  lglrststk (&lgl->control, level + 1);
  lgl->conf.lit = 0;
  lgl->conf.rsn[0] = lgl->conf.rsn[1] = 0;
  lgl->next = lglcntstk (&lgl->trail);
  LOG (2, "backtracked ");
}

static int lglmarked (LGL * lgl, int lit) {
  int res = lglavar (lgl, lit)->mark;
  if (lit < 0) res = -res;
  return res;
}

#ifndef NLGLPICOSAT

#ifndef NDEBUG
static void lglpicosataddcls (const int * c){
  const int * p;
  for (p = c; *p; p++) picosat_add (*p);
  picosat_add (0);
}
#endif

static void lglpicosatchkclsaux (LGL * lgl, int * c) {
#ifndef NDEBUG
  int * p, other;
  if (!lgl->opts.check.val) return;
  if (picosat_inconsistent ()) {
    LOG (3, "no need to check since PicoSAT is already inconsistent");
    return;
  }
  LOGCLS (3, c, "checking consistency with PicoSAT of clause");
  for (p = c; (other = *p); p++)
    picosat_assume (-other);
  (void) picosat_sat (-1);
  assert (picosat_res() == 20);
#endif
}

static void lglpicosatchkcls (LGL * lgl) {
  lglpicosatchkclsaux (lgl, lgl->clause.start);
}

static void lglpicosatchkclsarg (LGL * lgl, int first, ...) {
#ifndef NDEBUG
  va_list ap;
  Stk stk;
  int lit;
  if (!lgl->opts.check.val) return;
  CLR (stk);
  if (first) {
    va_start (ap, first);
    lglpushstk (lgl, &stk, first);
    for (;;) {
      lit = va_arg (ap, int);
      if (!lit) break;
      lglpushstk (lgl, &stk, lit);
    }
    va_end (ap);
  }
  lglpushstk (lgl, &stk, 0);
  lglpicosatchkclsaux (lgl, stk.start);
  lglrelstk (lgl, &stk);
#endif
}

static void lglpicosatchkunsat (LGL * lgl) {
#ifndef NDEBUG
  int res;
  if (!lgl->opts.check.val) return;
  if (lgl->picosat.res) {
    LOG (1, "determined earlier that PicoSAT proved unsatisfiability");
    assert (lgl->picosat.res == 20);
  }
  if (picosat_inconsistent ()) {
    LOG (1, "PicoSAT is already in inconsistent state");
    return;
  }
  res = picosat_sat (-1);
  LOG (1, "PicoSAT proved unsatisfiability");
  assert (res == 20);
#endif
}
#endif

static void lglunit (LGL * lgl, int lit) {
  assert (!lgl->level);
#ifndef NLGLPICOSAT
  if (lgl->picosat.chk) lglpicosatchkclsarg (lgl, lit, 0);
#endif
  LOG (1, "unit %d", lit);
  lglassign (lgl, lit, (lit << RMSHFT) | UNITCS, 0);
}

static HTS * lglhts (LGL * lgl, int lit) {
  return lgldvar (lgl, lit)->hts + (lit < 0);
}

static int * lglhts2wchs (LGL * lgl, HTS * hts) {
  int * res = lgl->wchs.start + hts->offset;
  assert (res < lgl->wchs.top);
  assert (res + hts->count < lgl->wchs.top);
  assert (res + hts->count < lgl->wchs.top);
  return res;
}

static void lglmark (LGL * lgl, int lit) {
  lglavar (lgl, lit)->mark = lit;
}

static void lglunmark (LGL * lgl, int lit) { lglavar (lgl, lit)->mark = 0; }

static void lglchksimpcls (LGL * lgl) {
#ifndef NDEBUG
  int *p, tmp, lit;
  AVar * av;
  for (p = lgl->clause.start; (lit = *p); p++) {
    tmp = lglfixed (lgl, lit);
    assert (!tmp);
    av = lglavar (lgl, lit);
    assert (!av->simp);
    av->simp = 1;
  }
  while (p > lgl->clause.start)
    lglavar (lgl,  *--p)->simp = 0;
#endif
}

static int lglcval (LGL * lgl, int litorval) {
  assert (litorval);
  if (litorval == 1 || litorval == -1) return litorval;
  return lglval (lgl, litorval);
}

static int lglsimpcls (LGL * lgl) {
  int * p, * q = lgl->clause.start, lit, tmp, mark;
  for (p = lgl->clause.start; (lit = *p); p++) {
    tmp = lglcval (lgl, lit);
    if (tmp == 1) { LOG (4, "literal %d satisfies clauses", lit); break; }
    if (tmp == -1) { LOG (4, "removing false literal %d", lit); continue; }
    mark = lglmarked (lgl, lit);
    if (mark > 0) 
      { LOG (4, "removing duplicated literal %d", lit); continue; }
    if (mark < 0) 
      { LOG (4, "literals %d and %d occur both", -lit, lit); break; }
    *q++ = lit;
    lglmark (lgl, lit);
  }

  *q = 0;
  lgl->clause.top = q + 1;

  while (q > lgl->clause.start) lglunmark (lgl, *--q);

  if (lit) LOG (2, "simplified clause is trivial");
  else LOGCLS (2, lgl->clause.start, "simplified clause");

  return lit;
}

static void lglorderclsaux (LGL * lgl, int * start) {
  int * p, max = 0, level, lit;
  for (p = start; (lit = *p); p++) {
    level = lglevel (lgl, lit);
    if (level <= max) continue;
    max = level;
    *p = start[0];
    start[0] = lit;
  }
}

static void lglordercls (LGL * lgl) {
  assert (lglcntstk (&lgl->clause) > 2);
  lglorderclsaux (lgl, lgl->clause.start);
  LOG (3, "head literal %d", lgl->clause.start[0]);
  lglorderclsaux (lgl, lgl->clause.start  + 1);
  LOG (3, "tail literal %d", lgl->clause.start[1]);
  LOGCLS (3, lgl->clause.start, "ordered clause");
}

static void lglfreewch (LGL * lgl, int oldoffset, int oldhcount) {
  int ldoldhcount = lglceilld (oldhcount);
  // assert ((1 << ldoldhcount) <= oldhcount);
  lgl->wchs.start[oldoffset] = lgl->freewchs[ldoldhcount];
  assert (oldoffset);
  lgl->freewchs[ldoldhcount] = oldoffset;
  lgl->nfreewchs++;
  assert (lgl->nfreewchs > 0);
  LOG (4, "saving watch stack at %d of size %d on free list %d",
       oldoffset, oldhcount, ldoldhcount);
}

static void lglshrinkhts (LGL * lgl, HTS * hts, int newcount) {
  int * p, i, oldcount = hts->count;
  assert (newcount <= oldcount);
  if (newcount == oldcount) return;
  p = lglhts2wchs (lgl, hts);
  for (i = newcount; i < oldcount; i++) p[i] = 0;
  hts->count = newcount;
  if (newcount) return;
  lglfreewch (lgl, hts->offset, oldcount);
  hts->offset = 0;
}

static long lglenlwchs (LGL * lgl, HTS * hts) {
  int oldhcount = hts->count, oldoffset = hts->offset, newoffset;
  int oldwcount, newwcount, oldwsize, newwsize, i, j;
  int newhcount = oldhcount ? 2*oldhcount : 1;
  int * oldwstart, * newwstart, * start;
  int ldnewhcount = lglfloorld (newhcount);
  long res = 0;

  newhcount = (1<<ldnewhcount);
  assert (newhcount > oldhcount);

  LOG (4, "increasing watch stack at %d from %d to %d", 
       oldoffset, oldhcount, newhcount);

  assert (!oldoffset == !oldhcount);

  lgl->stats.enlwchs++;

  newoffset = lgl->freewchs[ldnewhcount];
  start = lgl->wchs.start;
  if (newoffset != INT_MAX) {
    lgl->freewchs[ldnewhcount] = start[newoffset];
    start[newoffset] = 0;
    assert (lgl->nfreewchs > 0);
    lgl->nfreewchs--;
    LOG (4, "reusing free watch stack at %d of size %d",
         newoffset, (1 << ldnewhcount));
  } else {
    assert (lgl->wchs.start[hts->offset]);
    assert (lgl->wchs.top[-1] == INT_MAX);

    oldwcount = lglcntstk (&lgl->wchs);
    newwcount = oldwcount + newhcount;
    oldwsize = lglszstk (&lgl->wchs);
    newwsize = oldwsize;

    assert (lgl->wchs.top == lgl->wchs.start + oldwcount);
    assert (oldwcount > 0);

    while (newwsize < newwcount) newwsize *= 2;
    if (newwsize > oldwsize) {
      newwstart = oldwstart = lgl->wchs.start;
      RSZ (newwstart, oldwsize, newwsize);
      LOG (3, "resized global watcher stack from %d to %d",
           oldwsize, newwsize);
      res = newwstart - oldwstart;
      if (res) {
	LOG (3, "moved global watcher stack by %ld", res);
	start = lgl->wchs.start = newwstart;
      }
      lgl->wchs.end = start + newwsize;
    }
    lgl->wchs.top = start + newwcount;
    lgl->wchs.top[-1] = INT_MAX;
    newoffset = oldwcount - 1;
    LOG (4, 
         "new watch stack of size %d at end of global watcher stack at %d",
         newhcount, newoffset);
  }
  assert (start == lgl->wchs.start);
  assert (start[0]);
  j = newoffset;
  for (i = oldoffset; i < oldoffset + oldhcount; i++) {
    start[j++] = start[i];
    start[i] = 0;
  }
  while (j < newoffset + newhcount)
    start[j++] = 0;
  assert (start + j <= lgl->wchs.top);
  hts->offset = newoffset;
  if (oldhcount > 0) lglfreewch (lgl, oldoffset, oldhcount);
  return res;
}

static long lglpushwch (LGL * lgl, HTS * hts, int wch) {
  long res = 0;
  int * wchs = lglhts2wchs (lgl, hts);
  assert (sizeof (res) == sizeof (void*));
  assert (hts->count >= 0);
  if (wchs[hts->count]) {
    res = lglenlwchs (lgl, hts);
    wchs = lglhts2wchs (lgl, hts);
  }
  assert (!wchs[hts->count]);
  assert (wch != INT_MAX);
  wchs[hts->count++] = wch;
  lgl->stats.pshwchs++;
  assert (lgl->stats.pshwchs > 0);
  return res;
}

static long lglwchbin (LGL * lgl, int lit, int other, int red) {
  HTS * hts = lglhts (lgl, lit);
  int cs = ((other << RMSHFT) | BINCS | red);
  long res;
  assert (red == 0 || red == REDCS);
  res = lglpushwch (lgl, hts, cs);
  LOG (3, "%s binary watching %d blit %d", lglred2str (red), lit, other);
  return res;
}

static void lglwchtrn (LGL * lgl, int a, int b, int c, int red) {
  HTS * hts = lglhts (lgl, a);
  int cs = ((b << RMSHFT) | TRNCS | red);
  assert (red == 0 || red == REDCS);
  lglpushwch (lgl, hts, cs);
  lglpushwch (lgl, hts, c);
  LOG (3, "%s ternary watching %d blits %d %d", lglred2str (red), a, b, c);
}

static long lglwchlrg (LGL * lgl, int lit, int blit, int red, int lidx) {
  HTS * hts = lglhts (lgl, lit);
  int cs = ((blit << RMSHFT) | LRGCS | red);
  long res = 0;
  assert (red == 0 || red == REDCS);
  res += lglpushwch (lgl, hts, cs);
  res += lglpushwch (lgl, hts, lidx);
#ifndef NLGLOG
  {
    int * p = lglidx2lits (lgl, red, lidx);
    LOG (3, 
	 "watching %d with blit %d in %s[%d] %d %d %d %d%s",
	 lit, blit,
	 (red ? "red" : "irr"), lidx, p[0], p[1], p[2], p[3],
	 (p[4] ? " ..." : ""));
  }
#endif
  assert (sizeof (res) == sizeof (void*));
  return res;
}

static EVar * lglevar (LGL * lgl, int lit) {
  int idx = lglabs (lit);
  assert (1 < idx && idx < lgl->nvars);
  return lgl->evars + idx;
}

static int * lglepos (LGL * lgl, int lit) {
  EVar * ev;
  int * res;
  ev = lglevar (lgl, lit);
  res = &ev->pos;
  return res;
}

static int lglescore (LGL * lgl, int lit) {
  EVar * ev;
  int res;
  ev = lglevar (lgl, lit);
  res = ev->score;
  return res;
}

static int lglecmp (LGL * lgl, int s, int l, int t, int k) {
  if (s > t) return -1;
  if (s < t) return 1;
  return k - l;
}
 
#ifndef NDEBUG
static void lglchkesched (LGL * lgl) {
  int * p, parent, child, ppos, cpos, size, i, tmp;
  Stk * s = &lgl->esched;
  int pscr, cscr;
  size = lglcntstk (s);
  p = s->start;
  for (ppos = 0; ppos < size; ppos++) {
    parent = p[ppos];
    tmp = *lglepos (lgl, parent);
    assert (ppos == tmp);
    pscr = lglescore (lgl, parent);
    for (i = 0; i <= 1; i++) {
      cpos = 2*ppos + 1 + i;
      if (cpos >= size) continue;
      child = p[cpos];
      tmp = *lglepos (lgl, child);
      assert (cpos == tmp);
      cscr = lglescore (lgl, child);
      assert (lglecmp (lgl, pscr, parent, cscr, child) >= 0);
    }
  }
}
#endif

static void lgleup (LGL * lgl, int lit) {
  int child = lit, parent, cpos, ppos, * p, * cposptr, * pposptr;
  Stk * s = &lgl->esched;
  int lscr, pscr;
  p = s->start;
  lscr = lglescore (lgl, child);
  cposptr = lglepos (lgl, child);
  cpos = *cposptr;
  assert (cpos >= 0);
  while (cpos > 0) {
    ppos = (cpos - 1)/2;
    parent = p[ppos];
    pscr = lglescore (lgl, parent);
    if (lglecmp (lgl, pscr, parent, lscr, lit) >= 0) break;
    pposptr = lglepos (lgl, parent);
    assert (*pposptr == ppos);
    p[cpos] = parent;
    *pposptr = cpos;
    LOGESCHED (5, parent, "down from %d", ppos);
    cpos = ppos;
    child = parent;
  }
  if (*cposptr == cpos) return;
#ifndef NLGLOG
  ppos = *cposptr;
#endif
  *cposptr = cpos;
  p[cpos] = lit;
  LOGESCHED (5, lit, "up from %d", ppos);
#ifndef NDEBUG
  if (lgl->opts.check.val >= 2) lglchkesched (lgl);
#endif
}

static void lgledown (LGL * lgl, int lit) {
  int parent = lit, child, right, ppos, cpos;
  int * p, * pposptr, * cposptr, size;
  int lscr, cscr, rscr;
  Stk * s = &lgl->esched;
  size = lglcntstk (s);
  p = s->start;
  lscr = lglescore (lgl, parent);
  pposptr = lglepos (lgl, parent);
  ppos = *pposptr;
  assert (0 <= ppos);
  for (;;) {
    cpos = 2*ppos + 1;
    if (cpos >= size) break;
    child = p[cpos];
    cscr = lglescore (lgl, child);
    if (cpos + 1 < size) {
      right = p[cpos + 1];
      rscr = lglescore (lgl, right);
      if (lglecmp (lgl, cscr, child, rscr, right) < 0)
        cpos++, child = right, cscr = rscr;
    }
    if (lglecmp (lgl, cscr, child, lscr, lit) <= 0) break;
    cposptr = lglepos (lgl, child);
    assert (*cposptr = cpos);
    p[ppos] = child;
    *cposptr = ppos;
    LOGESCHED (5, child, "up from %d", cpos);
    ppos = cpos;
    parent = child;
  }
  if (*pposptr == ppos) return;
#ifndef NLGLOG
  cpos = *pposptr;
#endif
  *pposptr = ppos;
  p[ppos] = lit;
  LOGESCHED (5, lit, "down from %d", cpos);
#ifndef NDEBUG
  if (lgl->opts.check.val >= 2) lglchkesched (lgl);
#endif
}

static void lglesched (LGL * lgl, int lit) {
  int * p = lglepos (lgl, lit);
  Stk * s = &lgl->esched;
  if (*p >= 0) return;
  *p = lglcntstk (s);
  lglpushstk (lgl, s, lit);
  lgleup (lgl, lit);
  lgledown (lgl, lit);
  LOGESCHED (4, lit, "pushed");
}

static void lgleschedall (LGL * lgl) {
  int lit;
  for (lit = 2; lit < lgl->nvars; lit++)
    lglesched (lgl, lit);
}

static int lglecalc (LGL * lgl, EVar * ev) {
  int res = ev->score, o0 = ev->occ[0], o1 = ev->occ[1];
  if (!o0 || !o1) ev->score = 0;
  else ev->score = o0 + o1;
  return res;
}

static void lglincocc (LGL * lgl, int lit) {
  int idx = lglabs (lit), old, sign = (lit < 0);
  EVar * ev = lglevar (lgl, lit);
  assert (lglisfree (lgl, lit));
  ev->occ[sign] += 1;
  assert (ev->occ[sign] > 0);
  old = lglecalc (lgl, ev);
  assert (ev->score >= old);
  LOG (3, "inc occ of %d gives escore[%d] = %d with occs %d %d", 
       lit, idx, ev->score, ev->occ[0], ev->occ[1], ev->score);
  if (ev->pos < 0) {
    if (lgl->elm.pivot != idx) lglesched (lgl, idx);
  } else if (old < ev->score) lgledown (lgl, idx);
}

static void lgladdcls (LGL * lgl, int red, int glue) {
  int size, lit, other, other2, * p, lidx, unit, blit;
  Lir * lir;
  Val val;
  Stk * w;
  lgl->stats.prgss++;
  assert (!lglmtstk (&lgl->clause));
  assert (!lgl->clause.top[-1]);
  lglchksimpcls (lgl);
#if !defined(NLGLPICOSAT) && !defined(NDEBUG)
  if (lgl->opts.check.val)
    lglpicosataddcls (lgl->clause.start);
#endif
  size = lglcntstk (&lgl->clause) - 1;
  if (!red) lgl->stats.irr++, assert (lgl->stats.irr > 0);
  else if (size == 2) lgl->stats.red.bin++, assert (lgl->stats.red.bin > 0);
  else if (size == 3) lgl->stats.red.trn++, assert (lgl->stats.red.trn > 0);
  assert (size >= 0);
  if (!size) {
    LOG (1, "found empty clause");
    lgl->mt = 1;
    return;
  }
  lit = lgl->clause.start[0];
  if (size == 1) {
    lglunit (lgl, lit);
    return;
  }
  other = lgl->clause.start[1];
  if (size == 2) {
    lglwchbin (lgl, lit, other, red);
    lglwchbin (lgl, other, lit, red);
    if (red) {
      if (lglval (lgl, lit) < 0) lglf2rce (lgl, other, lit, REDCS);
      if (lglval (lgl, other) < 0) lglf2rce (lgl, lit, other, REDCS);
    } else if (lgl->dense) {
      assert (!red);
      assert (!lgl->level);
      lglincocc (lgl, lit);
      lglincocc (lgl, other);
    }
    return;
  }
  lglordercls (lgl);
  lit = lgl->clause.start[0];
  other = lgl->clause.start[1];
  if (size == 3) {
    other2 = lgl->clause.start[2];
    lglwchtrn (lgl, lit, other, other2, red);
    lglwchtrn (lgl, other, lit, other2, red);
    lglwchtrn (lgl, other2, lit, other, red);
    if (red) {
      if (lglval (lgl, lit) < 0 && lglval (lgl, other) < 0)
	lglf3rce (lgl, other2, lit, other, REDCS);
      if (lglval (lgl, lit) < 0 && lglval (lgl, other2) < 0)
	lglf3rce (lgl, other, lit, other2, REDCS);
      if (lglval (lgl, other) < 0 && lglval (lgl, other2) < 0)
	lglf3rce (lgl, lit, other, other2, REDCS);
    } else if (lgl->dense) {
      assert (!red);
      assert (!lgl->level);
      lglincocc (lgl, lit);
      lglincocc (lgl, other);
      lglincocc (lgl, other2);
    }
    return;
  }
  assert (size > 3);
  if (red) {
    assert (0 <= glue);
    if (glue > GLUE/2) glue = (glue - GLUE/2)/2 + GLUE/2;
    if (glue >= GLUE) glue = GLUEMASK;
    if (glue == MAXGLUE && !(lgl->stats.confs & 63)) glue--;
    lir = lgl->red + glue;
    w = &lir->lits;
    lidx = lglcntstk (w);
    if (lidx > MAXREDLIDX) {
      if (glue == MAXGLUE) {
	lglbacktrack (lgl, 0);
	lidx = lglcntstk (w);
	assert (!lidx);
      } 
      while (lidx > MAXREDLIDX) {
	assert (glue > 0);
	lir = lgl->red + --glue;
	w = &lir->lits;
	lidx = lglcntstk (w);
      }
      if (lidx > MAXREDLIDX)
	lgldie ("number of redundant large clause literals exhausted");
    }
    lidx <<= GLUESHFT;
    assert (0 <= lidx);
    lidx |= glue;
#ifndef NLGLSTATS
    lir->added++; assert (lir->added > 0);
#endif
  } else {
    w = &lgl->irr;
    lidx = lglcntstk (w);
    if (lidx <= 0 && !lglmtstk (w))
      lgldie ("number of irredundant large clause literals exhausted");
  }
  for (p = lgl->clause.start; (other2 = *p); p++)
    lglpushstk (lgl, w, other2);
  lglpushstk (lgl, w, 0);
  if (red) {
    unit = 0;
    for (p = lgl->clause.start; (other2 = *p); p++) {
      val = lglval (lgl, other2);
      assert (val <= 0);
      if (val < 0) continue;
      if (unit) unit = INT_MAX;
      else unit = other2;
    }
    if (unit && unit != INT_MAX) lglflrce (lgl, unit, red, lidx);
  }
  assert (red == 0 || red == REDCS);
  if ((!red && !lgl->dense) || (red && glue < MAXGLUE)) {
    lglwchlrg (lgl, lit, other, red, lidx);
    lglwchlrg (lgl, other, lit, red, lidx);
  }
  if (red && glue != MAXGLUE) {
    lgl->stats.red.lrg++;
    lgl->stats.rdded++;
  }
  if (!red && lgl->dense) {
    assert (!lgl->level);
    if (lidx > MAXIRRLIDX)
      lgldie ("number of irredundant large clause literals exhausted");
    blit = (lidx << RMSHFT) | IRRCS;
    for (p = lgl->clause.start; (other2 = *p); p++) {
      lglincocc (lgl, other2);
      lglpushwch (lgl, lglhts (lgl, other2), blit);
    }
  }
}

static void lgliadd (LGL * lgl, int ilit) {
#ifndef NLGLOG
  if (lglmtstk (&lgl->clause)) LOG (4, "opening irredundant clause");
#endif
  assert (lglabs (ilit) < 2 || lglabs (ilit) < lgl->nvars);
  lglpushstk (lgl, &lgl->clause, ilit);
  if (ilit) {
    LOG (4, "added literal %d", ilit);
  } else {
    LOG (4, "closing irredundant clause");
    LOGCLS (3, lgl->clause.start, "unsimplified irredundant clause");
    if (!lglsimpcls (lgl)) lgladdcls (lgl, 0, 0);
    lglclnstk (&lgl->clause);
  }
}

void lgladd (LGL * lgl, int elit) {
  int ilit;
  if (!lgl->state) lgl->state = USED;
  if (elit) {
    ilit = lglimport (lgl, elit);
    LOG (4, "adding external literal %d as %d", elit, ilit);
  } else {
    ilit = 0;
    LOG (4, "closing external clause");
  }
  lgliadd (lgl, ilit);
#ifndef NCHKSOL
  lglpushstk (lgl, &lgl->orig, elit);
#endif
}

static void lglbonflict (LGL * lgl, int lit, int blit) {
  assert (lglevel (lgl, lit) >= lglevel (lgl, blit >> RMSHFT));
  assert (!lgliselim (lgl, blit >> RMSHFT));
  assert (!lgliselim (lgl, lit));
  lgl->conf.lit = lit;
  lgl->conf.rsn[0] = blit;
  LOG (2, "inconsistent %s binary clause %d %d", 
       lglred2str (blit & REDCS), lit, (blit >> RMSHFT));
}

static void lgltonflict (LGL * lgl, int lit, int blit, int other2) {
  assert ((blit & MASKCS) == TRNCS);
  assert (lglevel (lgl, lit) >= lglevel (lgl, blit >> RMSHFT));
  assert (lglevel (lgl, lit) >= lglevel (lgl, other2));
  assert (!lgliselim (lgl, blit >> RMSHFT));
  assert (!lgliselim (lgl, other2));
  assert (!lgliselim (lgl, lit));
  lgl->conf.lit = lit;
  lgl->conf.rsn[0] = blit;
  lgl->conf.rsn[1] = other2;
  LOG (2, "inconsistent %s ternary clause %d %d %d",
       lglred2str (blit & REDCS), lit, (blit>>RMSHFT), other2);
}

static void lglonflict (LGL * lgl, 
                         int check,
                         int lit, int red, int lidx) {
  int glue;
#ifndef NLGLSTATS
  Lir * lir;
#endif
#if !defined (NLGLOG) || !defined (NDEBUG)
  int * p, * c = lglidx2lits (lgl, red, lidx);
#endif
  assert (red == REDCS || !red);
#ifndef NDEBUG
  {
    int found = 0;
    for (p = c; *p; p++) {
      if(*p == lit) found++;
      assert (lglval (lgl, *p) <= -check);
      assert (lglevel (lgl, lit) >= lglevel (lgl, *p));
      assert (!lgliselim (lgl, lit));
    }
    assert (found == 1);
  }
#endif
  lgl->conf.lit = lit;
  lgl->conf.rsn[0] = red | LRGCS;
  lgl->conf.rsn[1] = lidx;
#ifndef NLGLOG
  if (lgl->opts.log.val >= 2) {
    lglogstart (lgl, 2, "inconsistent %s large clause", lglred2str (red));
    for (p = c ; *p; p++)
      printf (" %d", *p);
    lglogend (lgl);
  }
#endif
  if (red) {
    glue = lidx & GLUEMASK;
#ifndef NLGLSTATS
    lir = lgl->red + glue;
    lir->conflicts++;
    assert (lir->conflicts > 0);
#endif
    if (glue != MAXGLUE) lgl->stats.ronflicts++;
  }
}

static void lgldeclscnt (LGL * lgl, int red, int size) {
  assert (!red || red == REDCS);
  if (!red) { assert (lgl->stats.irr); lgl->stats.irr--; }
  else if (size == 2) { assert (lgl->stats.red.bin); lgl->stats.red.bin--; } 
  else if (size == 3) { assert (lgl->stats.red.trn); lgl->stats.red.trn--; } 
  else { assert (lgl->stats.red.lrg); lgl->stats.red.lrg--; } 
}

static void lglrmtwch (LGL * lgl, int lit, int other1, int other2, int red) {
  int * p, blit, other, blit1, blit2, * w, * eow, tag;
  HTS * hts;
  assert (!red || red == REDCS);
  LOG (3, "removing %s ternary watch %d blits %d %d", 
       lglred2str (red), lit, other1, other2);
  hts = lglhts (lgl, lit);
  assert (hts->count >= 2);
  p = w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  blit1 = (other1 << RMSHFT) | red | TRNCS;
  blit2 = (other2 << RMSHFT) | red | TRNCS;
  for (;;) {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == BINCS) continue;
    if (tag == IRRCS) { assert (lgl->dense); continue; }
    other = *p++;
    if (blit == blit1 && other == other2) break;
    if (blit == blit2 && other == other1) break;
  }
  while (p < eow) p[-2] = p[0], p++;
  lglshrinkhts (lgl, hts, p - w - 2);
}

static int lglca (LGL * lgl, int a, int b) {
  int c, res, prev, r0, tag;
  AVar * v;
  assert (lglabs (a) != lglabs (b));
  assert (lglval (lgl, a) > 0 && lglval (lgl, b) > 0);
  // NOTE: the following is a specialized assertion
  assert (lglevel (lgl, a) == 1 && lglevel (lgl, b) == 1);
  res = 0;
  for (c = a; c; c = -prev) {
    assert (lglval (lgl, c) > 0);
    v = lglavar (lgl, c);
    assert (!v->mark);
    v->mark = 1;
    r0 = v->rsn[0];
    tag = r0 & MASKCS;
    if (tag != BINCS) break;
    prev = r0 >> RMSHFT;
    assert (prev || c == lgl->decision);
  }
  for (res = b; res; res = -prev) {
    assert (lglval (lgl, res) > 0);
    v = lglavar (lgl, res);
    if (v->mark) break;
    r0 = v->rsn[0];
    tag = r0 & MASKCS;
    if (tag != BINCS) { res = 0; break; }
    prev = r0 >> RMSHFT;
  }
  for (c = a; c; c = -prev) {
    assert (lglval (lgl, c) > 0);
    v = lglavar (lgl, c);
    assert (v->mark);
    v->mark = 0;
    r0 = v->rsn[0];
    tag = r0 & MASKCS;
    if (tag != BINCS) break;
    prev = r0 >> RMSHFT;
  }
  assert (res);//NOTE: specialized
  LOG (3, "least commong ancestor of %d and %d is %d", a, b, res);
  // TODO specialize this test and some code above?
  if (!res && lgl->level == 1) res = lgl->decision;
  return res;
}

static void lglrmlwch (LGL * lgl, int lit, int red, int lidx) {
  int blit, tag, * p, * q, * w, * eow, ored, olidx;
  HTS * hts;
  assert (!lgl->dense || red);
#ifndef NLGLOG
  p = lglidx2lits (lgl, red, lidx);
  LOG (3, "removing watch %d in %s[%d] %d %d %d %d%s",
       lit, (red ? "red" : "irr"), lidx, p[0], p[1], p[2], p[3],
       (p[4] ? " ..." : ""));
#endif
  hts = lglhts (lgl, lit);
  assert (hts->count >= 2);
  p = w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (;;) {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == BINCS) continue;
    if (tag == IRRCS) { assert (lgl->dense); continue; }
    olidx = *p++;
    if (tag == TRNCS) continue;
    assert (tag == LRGCS);
    ored = blit & REDCS;
    if (ored != red) continue;
    if (olidx == lidx) break;
  }
  assert ((p[-2] & REDCS) == red);
  assert (p[-1] == lidx);
  for (q = p; q < eow; q++)
    q[-2] = q[0];

  lglshrinkhts (lgl, hts, q - w - 2);
}

static void lglprop2 (LGL * lgl, int lit) {
  int other, blit, tag, val;
  const int * p, * w, * eow;
  HTS * hts;
  assert (lgl->propred);
  LOG (3, "propagating %d over binary clauses", lit);
  assert (!lgliselim (lgl, lit));
  assert (lglval (lgl, lit) == 1);
  hts = lglhts (lgl, -lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag != BINCS) continue;
    other = blit >> RMSHFT;
    assert (!lgliselim (lgl, other));
    val = lglval (lgl, other);
    if (val > 0) continue;
    if (val < 0) { lglbonflict (lgl, -lit, blit); break; }
    lglf2rce (lgl, other, -lit, blit & REDCS);
  }
  if (lgl->simp) lgl->stats.visits.simp += p - w;
  else lgl->stats.visits.search += p - w;
}


static void lglpropsearch (LGL * lgl, int lit) {
  int * q, * eos, blit, other, other2, other3, red, prev;
  int tag, val, val2, lidx, * c, * l, tmp = INT_MAX;
  const int * p;
  long delta;
  int visits;
  HTS * hts;
  assert (!lgl->simp);
  assert (!lgl->probing);
  assert (!lgl->dense);
  assert (lgl->propred);
  LOG (3, "propagating %d in search", lit);
  assert (!lgliselim (lgl, lit));
  assert (lglval (lgl, lit) == 1);
  hts = lglhts (lgl, -lit);
  if (!hts->offset) return;
  q = lglhts2wchs (lgl, hts);
  assert (hts->count >= 0);
  eos = q + hts->count;
  visits = 0;
  for (p = q; p < eos; p++) {
    visits++;
    *q++ = blit = *p;
    tag = blit & MASKCS;
    if (tag != BINCS) {
      assert (tag == TRNCS || tag == LRGCS);
      *q++ = tmp = *++p;
    } else assert ((tmp = INT_MAX));
    other = (blit >> RMSHFT);
    val = lglval (lgl, other);
    if (val > 0) continue;
    red = blit & REDCS;
    if (tag == BINCS) {
      assert (!red || !lgliselim (lgl, other));
      if (val < 0) {
	lglbonflict (lgl, -lit, blit);
	p++;
	break;
      }
      assert (!val);
      lglf2rce (lgl, other, -lit, red);
    } else if (tag == TRNCS) {
      assert (tmp != INT_MAX);
      other2 = tmp;
      assert (val <= 0);
      assert (!red || !lgliselim (lgl, other));
      val2 = lglval (lgl, other2);
      if (val2 > 0) continue;
      if (!val && !val2) continue;
	assert (!red || !lgliselim (lgl, other2));
      if (val < 0 && val2 < 0) {
	lgltonflict (lgl, -lit, blit, other2);
	p++;
	break;
      }
      if (!val) { tmp = other2; other2 = other; other = tmp; } 
      else assert (val < 0);
      lglf3rce (lgl, other2, -lit, other, red);
    } else {
      assert (tmp != INT_MAX);
      assert (tag == LRGCS);
      assert (val <= 0);
      lidx = tmp;
      c = lglidx2lits (lgl, red, lidx);
      other2 = c[0];
      if (other2 == -lit) other2 = c[0] = c[1], c[1] = -lit;
      if (other2 != other) {
	other = other2;
	val = lglval (lgl, other);
	if (val > 0) {
	  q[-2] = LRGCS | (other2 << RMSHFT) | red;
	  continue;
	}
      }
      assert (!red || !lgliselim (lgl, other));
      val2 = INT_MAX;
      prev = -lit;
      for (l = c + 2; (other2 = *l); l++) {
	*l = prev;
	val2 = lglval (lgl, other2);
	if (val2 >= 0) break;
	assert (!red || !lgliselim (lgl, other));
	prev = other2;
      }
      assert (val2 != INT_MAX);
      if (other2 && val2 >= 0) {
	c[1] = other2;
	assert (other == c[0]);
	delta = lglwchlrg (lgl, other2, other, red, lidx);
	if (delta) p += delta, q += delta, eos += delta;
	q -= 2;
	continue;
      } 
      while (l > c + 2) {
	other3 = *--l;
	*l = prev;
	prev = other3;
      }
      if (other2 && val2 < 0) continue;
      if (val < 0) {
	lglonflict (lgl, 1, -lit, red, lidx);
	p++;
	break;
      }
      assert (!val);
      lglflrce (lgl, other, red, lidx);
    }
  }
  while (p < eos) *q++ = *p++;
  lglshrinkhts (lgl, hts, hts->count - (p - q));
  lgl->stats.visits.search += visits;
}

static int lglbcpsearch (LGL * lgl) {
  int lit, count = 0;
  assert (!lgl->simp);
  assert (!lgl->mt && !lgl->conf.lit);
  while (!lgl->conf.lit && lgl->next < lglcntstk (&lgl->trail)) {
    lit = lglpeek (&lgl->trail, lgl->next++);
    lglpropsearch (lgl, lit);
    count++;
  }
  lgl->stats.props.search += count;
  return !lgl->conf.lit;
}

static void lglprop (LGL * lgl, int lit) {
  int tag, val, val2, lidx, * c, * l, tmp, igd, dom, hbred, subsumed, unit;
  int * p, * q, * eos, blit, other, other2, other3, red, prev, i0, i1, i2;
  int visits;
  long delta;
  HTS * hts;
  LOG (3, "propagating %d", lit);
  assert (!lgliselim (lgl, lit));
  assert (lglval (lgl, lit) == 1);
  hts = lglhts (lgl, -lit);
  if (!hts->offset) return;
  q = lglhts2wchs (lgl, hts);
  assert (hts->count >= 0);
  eos = q + hts->count;
  visits = 0;
  igd = 0;
  for (p = q; p < eos; p++) {
    visits++;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == IRRCS) { 
      assert (lgl->dense);
      *q++ = blit;
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, 0, lidx);
      unit = 0;
      for (l = c; (other = *l); l++) {
	val = lglval (lgl, other);
	if (val > 0) break;
	if (val < 0) continue;
	if (unit) break;
	unit = other;
      }
      if (other) continue;
      if (unit) { lglflrce (lgl, unit, 0, lidx); continue; }
      lglonflict (lgl, 1, -lit, 0, lidx); 
      p++;
      break;
    }
    red = blit & REDCS;
    other = (blit >> RMSHFT);
    val = lglval (lgl, other);
    if (tag == BINCS) {
      *q++ = blit;
      if (val > 0) continue;
      if (red) {
	if (!lgl->propred) continue;
	if (lgliselim (lgl, other)) continue;
      }
      if (val < 0) {
	lglbonflict (lgl, -lit, blit);
	p++;
	break;
      }
      assert (!val);
      lglf2rce (lgl, other, -lit, red);
    } else if (tag == TRNCS) {
      *q++ = blit;
      other2 = *++p;
      *q++ = other2;
      if (val > 0) continue;
      if (red) {
	if (!lgl->propred) continue;
	if (lgliselim (lgl, other)) continue;
      }
      val2 = lglval (lgl, other2);
      if (val2 > 0) continue;
      if (!val && !val2) continue;
      if (red && lgliselim (lgl, other2)) continue;
      if (lgl->igntrn && !igd) {
	i0 = lgl->ignlits[0], i1 = lgl->ignlits[1], i2 = lgl->ignlits[2];
	if (i0 == -lit) {
	  if (i1 == other) { if (i2 == other2) { igd = 1; continue; } }
	  else if (i2 == other) { if (i1 == other2) { igd = 1; continue; } }
	} else if (i1 == -lit) {
	  if (i0 == other) { if (i2 == other2) { igd = 1; continue; } }
	  else if (i2 == other) { if (i0 == other2) { igd = 1; continue; } }
	} else if (i2 == -lit) {
	  if (i0 == other) { if (i1 == other2) { igd = 1; continue; } }
	  else if (i1 == other) { if (i0 == other2) { igd = 1; continue; } }
	}
      }
      if (val < 0 && val2 < 0) {
	lgltonflict (lgl, -lit, blit, other2);
	p++;
	break;
      }
      if (!val) { tmp = other2; other2 = other; other = tmp; } 
      else assert (val < 0);
      if (lgl->opts.lhbr.val && lgl->probing && lgl->level == 1) {
	if (lglevel (lgl, other) < 1) dom = lit;
	else dom = lglca (lgl, lit, -other);
	subsumed = (dom == lit || dom == -other);
	hbred = subsumed ? red : REDCS;
 	LOG (2, "hyper binary resolved %s clause %d %d", 
	     lglred2str (hbred), -dom, other2);
#ifndef NLGLPICOSAT
	lglpicosatchkclsarg (lgl, -dom, other2, 0);
#endif
	if (subsumed) {
	  LOG (2, "subsumes %s ternary clause %d %d %d",
	       lglred2str (red), -lit, other, other2);
	  lglrmtwch (lgl, other2, other, -lit, red);
	  lglrmtwch (lgl, other, other2, -lit, red);
	  lgl->stats.hbr.sub++;
	  if (red) assert (hbred && lgl->stats.red.trn), lgl->stats.red.trn--;
	}
	delta = 0;
	if (dom == lit) {
	  LOG (3, 
	  "replacing %s ternary watch %d blits %d %d with binary %d blit %d",
	  lglred2str (red), -lit, other, other2, -lit, -dom);
	  assert (subsumed);
	  blit = (other2 << RMSHFT) | BINCS | hbred;
	  q[-2] = blit;
	  q--;
	} else {
	  if (dom == -other) {
	    LOG (3, "removing %s ternary watch %d blits %d %d", 
		 lglred2str (red), -lit, other, other2);
	    assert (subsumed);
	    q -= 2;
	  } else {
	    LOG (2, "replaces %s ternary clause %d %d %d as reason for %d",
		 lglred2str (red), -lit, other, other2, other2);
	    assert (!subsumed);
	    assert (lglabs (dom) != lglabs (lit));
	    assert (lglabs (other2) != lglabs (lit));
	    assert (hbred == REDCS);
	  }
	  delta += lglwchbin (lgl, -dom, other2, hbred);
	}
	delta += lglwchbin (lgl, other2, -dom, hbred);
	if (delta) p += delta, q += delta, eos += delta;
	if (hbred) lgl->stats.red.bin++, assert (lgl->stats.red.bin > 0);
	lglf2rce (lgl, other2, -dom, red);
	lgl->stats.hbr.trn++;
	lgl->stats.hbr.cnt++;
      } else lglf3rce (lgl, other2, -lit, other, red);
    } else {
      assert (tag == LRGCS);
      assert (!lgl->dense || red);
      if (val > 0) goto COPYL;
      if (red && !lgl->propred) goto COPYL;
      lidx = p[1];
      if (lidx == lgl->ignlidx) goto COPYL;
      c = lglidx2lits (lgl, red, lidx);
      other2 = c[0];
      if (other2 == -lit) other2 = c[0] = c[1], c[1] = -lit;
      if (other2 != other) {
	other = other2;
	val = lglval (lgl, other);
	blit = red; 
	blit |= LRGCS; 
	blit |= other2 << RMSHFT; 
	if (val > 0) goto COPYL;
      }
      if (red && lgliselim (lgl, other)) goto COPYL;
      val2 = INT_MAX;
      prev = -lit;
      for (l = c + 2; (other2 = *l); l++) {
	*l = prev;
	val2 = lglval (lgl, other2);
	if (val2 >= 0) break;
	if (red && lgliselim (lgl, other2)) break;
	prev = other2;
      }

      assert (val2 != INT_MAX);
      if (other2 && val2 >= 0) {
	c[1] = other2;
	assert (other == c[0]);
	delta = lglwchlrg (lgl, other2, other, red, lidx);
	if (delta) p += delta, q += delta, eos += delta;
	p++;
	continue;
      } 

      while (l > c + 2) {
	other3 = *--l;
	*l = prev;
	prev = other3;
      }

      if (other2 && val2 < 0) goto COPYL;

      if (val < 0) {
	lglonflict (lgl, 1, -lit, red, lidx);
	break;
      }

      assert (!val);
      if (lgl->opts.lhbr.val && lgl->probing && lgl->level == 1) {
	dom = lit;
	for (l = c; (other2 = *l); l++) {
	  if (other2 == other) continue;
	  assert (lglval (lgl, other2) < 0);
	  if (other2 == -lit) continue;
	  if (lglevel (lgl, other2) < 1) continue;
	  if (dom == -other2) continue;
	  dom = lglca (lgl, dom, -other2);
	}
	LOGCLS (2, c, "dominator %d for %s clause", dom, lglred2str (red));
	subsumed = 0;
	for (l = c; !subsumed && (other2 = *l); l++)
	  subsumed = (dom == -other2);
	assert (lit != dom || subsumed);
	hbred = subsumed ? red : REDCS;
 	LOG (2, "hyper binary resolved %s clause %d %d", 
	     lglred2str (hbred), -dom, other);
#ifndef NLGLPICOSAT
	lglpicosatchkclsarg (lgl, -dom, other, 0);
#endif
	if (subsumed) {
	  LOGCLS (2, c, "subsumes %s large clause", lglred2str (red));
	  lglrmlwch (lgl, other, red, lidx);
	  lgl->stats.hbr.sub++;
	  if (red) {
	    int glue = lidx & GLUEMASK;
	    assert (hbred);
	    if (glue != MAXGLUE) {
	      assert (lgl->stats.red.lrg);
	      lgl->stats.red.lrg--;
	    }
	  }
	  for (l = c; *l; l++) *l = REMOVED;
	  *l = REMOVED;
	}
	delta = 0;
	if (dom == lit) {
	  assert (subsumed);
	  LOG (3,
	       "replacing %s large watch %d with binary watch %d blit %d",
	       lglred2str (red), -lit, -lit, -dom);
	  blit = (other << RMSHFT) | BINCS | hbred;
	  *q++ = blit, p++;
	} else {
	  if (subsumed) {
	    LOG (3, "removing %s large watch %d", lglred2str (red), -lit);
	    p++;
	  } else {
	    LOGCLS (2, c, 
	            "%s binary clause %d %d becomes reasons "
		    "for %d instead of %s large clause",
	            lglred2str (hbred), -dom, other, other, lglred2str (red));
	    assert (hbred == REDCS);
	  }
	  delta += lglwchbin (lgl, -dom, other, hbred);
	}
	delta += lglwchbin (lgl, other, -dom, hbred);
	if (delta) p += delta, q += delta, eos += delta;
	if (hbred) lgl->stats.red.bin++, assert (lgl->stats.red.bin > 0);
	lglf2rce (lgl, other, -dom, red);
	lgl->stats.hbr.lrg++;
	lgl->stats.hbr.cnt++;
	if (subsumed) continue;
      } else lglflrce (lgl, other, red, lidx);
COPYL:
      *q++ = blit;
      *q++ = *++p; 
    }
  }
  while (p < eos) *q++ = *p++;
  lglshrinkhts (lgl, hts, hts->count - (p - q));
  if (lgl->simp) lgl->stats.visits.simp += visits;
  else lgl->stats.visits.search += visits;
}

static int lglbcp (LGL * lgl) {
  int lit, cnt, next2 = lgl->next, count = 0;
  assert (!lgl->mt && !lgl->conf.lit);
  while (!lgl->conf.lit) {
    cnt = lglcntstk (&lgl->trail);
    if (lgl->opts.lhbr.val && lgl->probing && lgl->level == 1 && next2 < cnt) {
      lit = lglpeek (&lgl->trail, next2++);
      lglprop2 (lgl, lit);
    } else if (lgl->next >= cnt) break;
    else {
      lit = lglpeek (&lgl->trail, lgl->next++);
      assert (lit != 1);
      assert (lit != -1);
      count++;
      assert (lit != 1);
      lglprop (lgl, lit);
    }
  }
  if (lgl->simp) lgl->stats.props.simp += count;
  else lgl->stats.props.search += count;

  return !lgl->conf.lit;
}

#ifndef NDEBUG
static void lglchkclnvar (LGL * lgl) {
  AVar * av;
  int i;
  for (i = 2; i < lgl->nvars; i++) {
    av = lglavar (lgl, i);
    assert (!av->mark);
  }
}
#endif

#ifdef RESOLVENT
static int lglmaintainresolvent (LGL * lgl) {
#ifndef NDEBUG
  if (lgl->opts.check.val >= 1) return 1;
#endif
#ifndef NLGLOG
  if (lgl->opts.log.val >= 2) return 1;
#endif
  return 0;
}
#endif

static void lglbumplit (LGL * lgl, int lit) {
  int idx = lglabs (lit);
  AVar * av = lglavar (lgl, idx);
  av->score = lgladdflt (av->score, lgl->scinc);
  LOG (3, "new decision score %s of %d", lglflt2str (lgl, av->score), idx);
  if (av->pos < 0) return;
  lgldup (lgl, idx);
}

static int lglpull (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int level, res;
  if (av->mark) return 0;
  level = av->level;
  if (!level) return 0;
  av->mark = 1;
  lglpushstk (lgl, &lgl->seen, lit);
#ifdef RESOLVENT
  if (lglmaintainresolvent (lgl)) {
    lglpushstk (lgl, &lgl->resolvent, lit);
    LOG (2, "adding %d to resolvent", lit);
    LOGRESOLVENT (3, "resolvent after adding %d is", lit);
  }
#endif
  if (level == lgl->level) {
    LOG (2, "reason literal %d at same level %d", lit, lgl->level);
    res = 1;
  } else {
    lglpushstk (lgl, &lgl->clause, lit);
    LOG (2, "adding literal %d at upper level %d to FUIP clause",
	 lit, lglevel (lgl, lit));
    if (!lglpeek (&lgl->control, level)) {
      lglpoke (&lgl->control, level, 1);
      lglpushstk (lgl, &lgl->frames, level);
      LOG (2, "pulled in decision level %d", level);
    }
    res = 0;
  }
  lglbumplit (lgl, lit);
  return res;
}

static int lglpoison (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int tag, level;
  if (av->mark) return 0;
  level = av->level;
  if (!level) return 0;
  assert (level < lgl->level);
  tag = av->rsn[0] & MASKCS;
  if (tag == DECISION) return 1;
  if (!lglpeek (&lgl->control, level)) return 1;
  av->mark = 1;
  lglpushstk (lgl, &lgl->seen, lit);
  return 0;
}

static int lglmincls (LGL * lgl, int start) {
  int lit, tag, r0, r1, other, * p, *top, old, next;
  AVar * av;
  assert (lglmarked (lgl, start));
  lit = start;
  av = lglavar (lgl, lit);
  r0 = av->rsn[0];
  tag = (r0 & MASKCS);
  if (tag == DECISION) return 0;
  old = next = lglcntstk (&lgl->seen);
  for (;;) {
    r1 = av->rsn[1];
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      if (lglpoison (lgl, other)) goto FAILED;
      if (tag == TRNCS && lglpoison (lgl, r1)) goto FAILED;
    } else {
      assert (tag == LRGCS);
      p = lglidx2lits (lgl, (r0 & REDCS), r1);
      while ((other = *p++)) 
	if (other != -lit && lglpoison (lgl, other)) goto FAILED;
    }
    if (next == lglcntstk (&lgl->seen)) return 1;
    lit = lglpeek (&lgl->seen, next++);
    av = lglavar (lgl, lit);
    assert (av->mark);
    r0 = av->rsn[0];
    tag = (r0 & MASKCS);
  }
FAILED:
  p = lgl->seen.top;
  top = lgl->seen.top = lgl->seen.start + old;
  while (p > top) lglavar (lgl, *--p)->mark = 0;
  return 0;
}

static double lglpcnt (double n, double d) {
  return d != 0 ? 100.0 * n / d : 0.0;
}

static int lglrem (LGL * lgl) {
  int res = lgl->nvars;
  if (!res) return 0;
  res -= lgl->stats.fixed.current + 1;
  assert (res >= 0);
  return res;
}

double lglprocesstime (void) {
  struct rusage u;
  double res;
  if (getrusage (RUSAGE_SELF, &u)) return 0;
  res = u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec;
  res += u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
  return res;
}

double lglsec (LGL * lgl) {
  double time, delta;
  time = lgl->getime ();
  delta = time - lgl->stats.time.entered.process;
  int i;
  if (delta < 0) delta = 0;
  lgl->stats.time.all += delta;
  lgl->stats.time.entered.process = time;
  for (i = 0; i < lgl->stats.time.entered.nesting; i++) {
    delta = time - lgl->stats.time.entered.phase[i];
    if (delta < 0) delta = 0;
    assert (lgl->stats.time.entered.ptr[i]);
    *lgl->stats.time.entered.ptr[i] += delta;
    lgl->stats.time.entered.phase[i] = time;
  }
  return lgl->stats.time.all;
}

static void lglstart (LGL * lgl, double * timestatsptr) {
  int nesting = lgl->stats.time.entered.nesting;
  assert (timestatsptr);
  assert (nesting < MAXPHN);
  lgl->stats.time.entered.ptr[nesting] = timestatsptr;
  lgl->stats.time.entered.phase[nesting] = lgl->getime ();
  lgl->stats.time.entered.nesting++;
}

static void lglstop (LGL * lgl) {
  double time = lgl->getime (), delta;
  int nesting = --lgl->stats.time.entered.nesting;
  assert (nesting >= 0);
   delta = time - lgl->stats.time.entered.phase[nesting];
  if (delta < 0) delta = 0;
  assert (lgl->stats.time.entered.ptr[nesting]);
  *lgl->stats.time.entered.ptr[nesting] += delta;
}

double lglmaxmb (LGL * lgl) {
  return (lgl->stats.bytes.max + sizeof *lgl) / (double)(1<<20);
}

double lglmb (LGL * lgl) {
  return (lgl->stats.bytes.current + sizeof *lgl) / (double)(1<<20);
}

static double lglagility (LGL * lgl) { return lgl->flips/1e5; }

static double lglavg (double n, double d) {
  return d != 0 ? n / d : 0.0;
}

static double lglheight (LGL * lgl) {
  return lglavg (lgl->stats.height, lgl->stats.decisions);
}

static void lglrephead (LGL * lgl) {
  if (lgl->tid > 0) return;
  assert (lgl->opts.verbose.val >= 1); 
  if (lgl->msglock.lock) lgl->msglock.lock (lgl->msglock.state);
  printf ("c\n");
  printf ("c   "
" seconds         irredundant          redundant clauses  agility"
"  height\n");
  printf ("c   "
"         variables clauses conflicts large binary ternary    hits"
"        MB\n");
  printf ("c\n");
  fflush (stdout);
  if (lgl->msglock.unlock) lgl->msglock.unlock (lgl->msglock.state);
}

static void lglrep (LGL * lgl, int level, char type) {
  if (lgl->opts.verbose.val < level) return;
  if (!(lgl->stats.reported % REPMOD)) lglrephead (lgl);
  lglprt (lgl, 1,
             "%c %6.1f %7d %8d %9d %7d %6d %5d %3.0f %3.0f %5.1f %4.0f",
	     type,
	     lglsec (lgl),
	     lglrem (lgl),
	     lgl->stats.irr,
	     lgl->stats.confs,
	     lgl->stats.red.lrg,
	     lgl->stats.red.bin,
	     lgl->stats.red.trn,
	     lglagility (lgl),
	     lglpcnt (lgl->stats.ronflicts, lgl->stats.rdded),
	     lglheight (lgl),
	     lglmb (lgl));
  lgl->stats.reported++;
}

void lglflshrep (LGL * lgl) {
  if (!lgl->stats.reported) return;
  if (lgl->stats.reported % REPMOD) lglrephead (lgl);
  else lglprt (lgl, 1, "");
}

static int lglitlocked (int lit) { 
  int res = ((lit >> 1) & LOCKED) ^ (lit & LOCKED);
  assert (res == 0 || res == LOCKED);
  return res;
}

static void lglreduce (LGL * lgl) {
  int target, glue, * p, * q, * c, * r, *eow, lit, lidx, locked, dst, r0;
  int cntlocked, collected, moved, sumcollected, sumoved;
  int cnt, mapped, idx, i, red, tag, blit, inc, factor;
  Lir * lir; AVar * av; DVar * dv;
  HTS * hts;
  Stk * s;
  lglstart (lgl, &lgl->stats.tms.red);
  lgl->stats.reduced++;
  cntlocked = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglval (lgl, idx)) continue;
    av = lglavar (lgl, idx);
    r0 = av->rsn[0];
    red = r0 & REDCS;
    if (!red) continue;
    tag = r0 & MASKCS;
    if (tag != LRGCS) continue;
    lidx = av->rsn[1];
    glue = lidx & GLUEMASK;
    if (glue == MAXGLUE) continue;
    c = lglidx2lits (lgl, REDCS, lidx);
    lit = *c;
    assert (!lglitlocked (lit));
    lit ^= LOCKED;
    assert (lglitlocked (lit));
    *c = lit;
    cntlocked++;
  }
  LOG (1, "locked %d redundant large clauses", cntlocked);
  assert (lglmtstk (&lgl->saved));
  target = lgl->stats.red.lrg/2;
  LOG (1, "target is to collect %d clauses", target);
#ifndef NLGLOG
  if (lgl->opts.log.val >= 1) {
    lglogstart (lgl, 1, "no literals with glue:");
    for (glue = MAXGLUE-1; glue >= 0; glue--)
      if (lglmtstk (&lgl->red[glue].lits))
	printf (" %d", glue);
    lglogend (lgl);
  }
#endif
  sumcollected = 0;
  for (glue = MAXGLUE-1; glue >= 0; glue--) {
    lir = lgl->red + glue;
    assert (!lir->mapped);
    s = &lir->lits;
    mapped = collected = cntlocked = 0;
    for (c = s->start; c < s->top; c = p) {
      p = c; 
      lit = *p;
      assert (lit);
      if (lit == REMOVED) { p++; continue; }
      locked = lglitlocked (lit);
      if (locked) cntlocked++;
      assert (mapped < LOCKED);
      if (locked || !target) {
	dst = mapped^locked; 
	lglpushstk (lgl, &lgl->saved, lit^locked);
      } else {
	dst = COLLECT;
	collected++; 
	assert (lgl->stats.red.lrg > 0);
	lgl->stats.red.lrg--;
	target--;
      }
      *p++ = dst;
      while (*p++)
	;
      if (dst != COLLECT) mapped += (p - c);
    }
    lir->mapped = mapped;
    if (!lglmtstk (s))
      LOG (1, "collecting %d redundant large clauses with glue %d",
	   collected, glue);
    if (cntlocked) 
      LOG (1, "keeping %d locked redundant large clauses with glue %d",
           cntlocked, glue);
    sumcollected += collected;
  }
  LOG (1, "collecting %d redundant large clauses altogether", sumcollected);
  LOG (1, "saved %d literals", lglcntstk (&lgl->saved));
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglval (lgl, idx)) continue;
    av = lglavar (lgl, idx);
    r0 = av->rsn[0];
    red = r0 & REDCS;
    if (!red) continue;
    tag = r0 & MASKCS;
    if (tag != LRGCS) continue;
    lidx = av->rsn[1];
    glue = lidx & GLUEMASK;
    if (glue == MAXGLUE) continue;
    c = lglidx2lits (lgl, REDCS, lidx);
    dst = *c;
    assert (lglitlocked (dst));
    dst ^= LOCKED;
    *c = dst;
    dst <<= GLUESHFT;
    dst |= lidx & GLUEMASK;
    av->rsn[1] = dst;
  }
  LOG (1, "unlocked and moved reasons");
  collected = moved = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    dv = lgldvar (lgl, idx);
    for (i = 0; i <= 1; i++) {
      hts = dv->hts + i;
      if (!hts->offset) continue;
      q = lglhts2wchs (lgl, hts);
      assert (hts->count >= 0);
      eow = q + hts->count;
      for (p = q; p < eow; p++) {
	blit = *p;
	red = blit & REDCS;
	tag = blit & MASKCS;
	if (red && tag == LRGCS) {
	  lidx = *++p;
	  glue = lidx & GLUEMASK;
	  if (glue == MAXGLUE) {
	    dst = lidx >> GLUESHFT;
	  } else {
	    c = lglidx2lits (lgl, REDCS, lidx);
	    dst = *c;
	  }
	  if (dst != COLLECT) {
	    moved++;
	    assert (!lglitlocked (dst));
	    *q++ = blit;
	    *q++ = (dst << GLUESHFT) | (lidx & GLUEMASK);
	  } else {
	    collected++;
	  }
	} else {
	  *q++ = blit;
	  if (tag != BINCS) {
	    assert (tag == TRNCS || tag == LRGCS);
	    *q++ = *++p;
	  }
	}
      }
      lglshrinkhts (lgl, hts, hts->count - (p - q));
    }
  }
  LOG (1, "moved %d and collected %d head tail pointers", moved, collected);
  sumcollected = sumoved = 0;
  r = lgl->saved.start;
  for (glue = MAXGLUE-1; glue >= 0; glue--) {
    lir = lgl->red + glue;
    s = &lir->lits;
    mapped = lir->mapped;
    lir->mapped = 0;
    cnt = lglcntstk (s);
    assert (mapped <= cnt);
    if (!cnt) continue;
    moved = collected = 0;
    if (!mapped) {
      collected = cnt;
      s->top = s->start;
      LOG (1, "collected all %d redundant large literals with glue %d",
	   collected, glue);
    } else if (mapped == cnt) {
      for (p = s->start; p < s->top; p++) {
	*p++ = *r++;
	while (*p)
	  p++;
      }
      LOG (1, "nothing to move or collect for %d literals with glue %d",
           cnt, glue);
    } else {
      p = q = s->start;
      while (p < s->top) {
	dst = *p++;
	if (dst == REMOVED) continue;
	if (dst == COLLECT) { while (*p++); continue; }
	*q++ = *r++;
	while ((lit = *p++)) 
	  *q++ = lit;
	*q++ = 0;
      }
      s->top = q;
      collected = p - q;
      moved = q - s->start;
      assert (collected);
      assert (moved);
      LOG (1,
	   "moved %d and collected %d redundant large literals with glue %d", 
	   moved, collected, glue);
    }
    sumcollected += collected;
    sumoved += moved;
    lglredstk (lgl, s, (1<<15), 2);
  }
  assert (r == lgl->saved.top);
  lglclnstk (&lgl->saved);
  LOG (1, "moved %d and collected %d redundant large literals altogether",
       sumoved, sumcollected);
  inc = lgl->opts.redlinc.val;
  factor = (500*lgl->stats.ronflicts) / lgl->stats.rdded;
  if (factor > 200) factor = 200;
  inc = (factor * inc + 99) / 100;
  if (inc < lgl->opts.redlmininc.val) inc = lgl->opts.redlmininc.val;
  LOG (1, "new reduce factor %.2f increment %d\n", factor/100.0, inc);
  lgl->limits.reduce += inc;
  LOG (1, "new reduce limit of %d redundant clauses", lgl->limits.reduce);
  lglrep (lgl, 1, '+');
  lgl->stats.ronflicts /= 4;
  lgl->stats.rdded /= 4;
  lglstop (lgl);
}

static void lglrmbwch (LGL * lgl, int lit, int other, int red) {
  int * p, blit, blit1, * w, * eow, tag;
  HTS * hts;
  assert (!red || red == REDCS);
  LOG (3, "removing %s binary watch %d blit %d", 
       lglred2str (red), lit, other);
  hts = lglhts (lgl, lit);
  assert (hts->count >= 1);
  p = w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  blit1 = (other << RMSHFT) | red | BINCS;
  for (;;) {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) { p++; continue; }
    if (tag == IRRCS) { assert (lgl->dense); continue; }
    assert (tag == BINCS);
    if (blit == blit1) break;
  }
  while (p < eow) p[-1] = p[0], p++;
  lglshrinkhts (lgl, hts, p - w - 1);
}

static int lglpopesched (LGL * lgl) {
  Stk * s = &lgl->esched;
  int res, last, cnt, * p;
  EVar * ev;
  assert (!lglmtstk (s));
  res = *s->start;
  LOGESCHED (4, res, "popped");
  ev = lglevar (lgl, res);
  assert (!ev->pos);
  ev->pos = -1;
  last = lglpopstk (s);
  cnt = lglcntstk (s);
  if (!cnt) { assert (last == res); return res; }
  p = lglepos (lgl, last);
  assert (*p == cnt);
  *p = 0;
  *s->start = last;
  lgledown (lgl, last);
  return res;
}

static void lgldecocc (LGL * lgl, int lit) {
  int idx = lglabs (lit), old, sign = (lit < 0);
  EVar * ev = lglevar (lgl, lit);
  if (!lglisfree (lgl, lit)) return;
  assert (ev->occ[sign] > 0);
  ev->occ[sign] -= 1;
  old = lglecalc (lgl, ev);
  assert (ev->score <= old);
  LOG (3, "dec occ of %d gives escore[%d] = %d with occs %d %d", 
       lit, idx, ev->score, ev->occ[0], ev->occ[1], ev->score);
  if (ev->pos < 0) { 
    if (lgl->elm.pivot != idx) lglesched (lgl, idx);
  } else if (old > ev->score) lgleup (lgl, idx);
}

static void lglrmbcls (LGL * lgl, int a, int b, int red) {
  lglrmbwch (lgl, a, b, red);
  lglrmbwch (lgl, b, a, red);
  LOG (2, "removed %s binary clause %d %d", lglred2str (red), a, b);
  lgldeclscnt (lgl, red, 2);
  if (!red && lgl->dense) lgldecocc (lgl, a), lgldecocc (lgl, b);
}

static void lglrmtcls (LGL * lgl, int a, int b, int c, int red) {
  lglrmtwch (lgl, a, b, c, red);
  lglrmtwch (lgl, b, a, c, red);
  lglrmtwch (lgl, c, a, b, red);
  LOG (2, "removed %s ternary clause %d %d %d", lglred2str (red), a, b, c);
  lgldeclscnt (lgl, red, 3);
  if (!red && lgl->dense)
    lgldecocc (lgl, a), lgldecocc (lgl, b), lgldecocc (lgl, c);
}

static void lglrmlocc (LGL * lgl, int lit, int lidx) {
  int search, blit, tag, * p, * q, * w, * eow;
  HTS * hts;
#ifndef NLGLOG
  LOG (3, "removing occurrence %d in irr[%d]", lit, lidx);
#endif
  hts = lglhts (lgl, lit);
  assert (hts->count >= 1);
  assert (lidx <= MAXIRRLIDX);
  search = (lidx << RMSHFT) | IRRCS;
  assert (search >= 0);
  p = w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  do {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
  } while (blit != search);
  assert (p[-1] == search);
  for (q = p ; q < eow; q++)
    q[-1] =q[0];
  lglshrinkhts (lgl, hts, q - w - 1);
}

static void lglrmlcls (LGL * lgl, int lidx, int red) {
  int * c, * p, glue, lit;
  glue = red ? (lidx & GLUEMASK) : 0;
  c = lglidx2lits (lgl, red, lidx);
  if ((red && glue != MAXGLUE) || (!red && !lgl->dense)) {
    lglrmlwch (lgl, c[0], red, lidx);
    lglrmlwch (lgl, c[1], red, lidx);
  }
  if (!red && lgl->dense) {
    for (p = c; (lit = *p); p++) {
      lglrmlocc (lgl, lit, lidx);
      lgldecocc (lgl, lit);
    }
  }
  for (p = c; *p; p++) *p = REMOVED;
  *p = REMOVED;
  if (glue != MAXGLUE) lgldeclscnt (lgl, red, 4);
}

static void lglupdatelwch (LGL * lgl, int lit, int blit, int red, int lidx) {
  LOG (3, "updating blit %d for watch %d", blit, lit);
  lglrmlwch (lgl, lit, red, lidx);
  lglwchlrg (lgl, lit, blit, red, lidx);
}

static void lgldynsub (LGL * lgl, int lit, int r0, int r1) {
  int red, tag;
  tag = r0 & MASKCS;
  LOGREASON (2, lit, r0, r1, "removing subsumed");
  red = r0 & REDCS;
  if (red) lgl->stats.otfs.sub.dyn.red++;
  else lgl->stats.otfs.sub.dyn.irr++;
  if (tag == BINCS) lglrmbcls (lgl, lit, (r0>>RMSHFT), red);
  else if (tag == TRNCS) lglrmtcls (lgl, lit, (r0>>RMSHFT), r1, red);
  else { assert (tag == LRGCS); lglrmlcls (lgl, r1, red); }
}

static void lglunflict (LGL * lgl, int lit) {
  lgl->conf.lit = lit;
  lgl->conf.rsn[0] = (lit << RMSHFT) | UNITCS;
  LOG (2, "inconsistent unary clause %d", lit);
}

static void lgldynstr (LGL * lgl, int del, int lit, int r0, int r1) {
  int * p, * c, lidx, other, red, tag, glue, other2, other3, pos, blit;
  tag = r0 & MASKCS;
  LOGREASON (2, lit, r0, r1, "strengthening by removing %d from", del);
  red = r0 & REDCS;
  if (red) lgl->stats.otfs.str.dyn.red++;
  else lgl->stats.otfs.str.dyn.irr++;
  if (tag == BINCS) {
    other = (del == lit) ? (r0 >> RMSHFT) : lit;
    assert (other != del);
    lglrmbcls (lgl, del, other, red);
#ifndef NLGLPICOSAT
    lglpicosatchkclsarg (lgl, other, 0);
#endif
    lglunflict (lgl, other);
    return;
  }
  if (tag == TRNCS) {
    if (lit == del) other = (r0 >> RMSHFT), other2 = r1;
    else if (del == r1) other = lit, other2 = (r0 >> RMSHFT);
    else other = lit, other2 = r1;
    assert (del != other && del != other2);
    lglrmtcls (lgl, del, other, other2, red);
#ifndef NLGLPICOSAT
    lglpicosatchkclsarg (lgl, other, other2, 0);
#endif
    if (!red) lgl->stats.irr++, assert (lgl->stats.irr > 0);
    else lgl->stats.red.bin++, assert (lgl->stats.red.bin > 0);
    lglwchbin (lgl, other, other2, red);
    lglwchbin (lgl, other2, other, red);
    if (lglevel (lgl, other) < lglevel (lgl, other2)) 
      SWAP (int, other, other2);
    blit = (other2 << RMSHFT) | BINCS | red;
    lglbonflict (lgl, other, blit);
    return;
  }
  assert (tag == LRGCS);
  lidx = r1;
  glue = red ? (lidx & GLUEMASK) : 0;
  c = lglidx2lits (lgl, red, lidx);
  for (p = c; *p != del; p++)
    assert (*p);
  pos = p - c;
  if (pos <= 1 && glue != MAXGLUE) lglrmlwch (lgl, del, red, lidx);
  while ((other = *++p)) p[-1] = other;
  p[-1] = 0, *p = REMOVED;
  lglorderclsaux (lgl, c + 0);
  lglorderclsaux (lgl, c + 1);
#ifndef NLGLPICOSAT
  lglpicosatchkclsaux (lgl, c);
#endif
  assert (p - c > 3);
  if (p - c == 4) {
    assert (glue != MAXGLUE && !c[3] && c[4] == REMOVED);
    other = c[0], other2 = c[1], other3 = c[2];
    lglrmlwch (lgl, other, red, lidx);
    if (pos >= 2) lglrmlwch (lgl, other2, red, lidx);
    c[0] = c[1] = c[2] = c[3] = REMOVED;
    if (lglevel (lgl, other2) < lglevel (lgl, other3)) 
      SWAP (int, other2, other3);
    if (lglevel (lgl, other) < lglevel (lgl, other2)) 
      SWAP (int, other, other2);
    lglwchtrn (lgl, other, other2, other3, red);
    lglwchtrn (lgl, other2, other, other3, red);
    lglwchtrn (lgl, other3, other, other2, red);
    if (red) {
      assert (lgl->stats.red.lrg > 0);
      lgl->stats.red.lrg--;
      lgl->stats.red.trn++;
      assert (lgl->stats.red.trn > 0);
    }
    lgltonflict (lgl, other, (other2 << RMSHFT) | red | TRNCS, other3);
  } else {
    if (pos <= 1 && glue != MAXGLUE) {
      LOG (3, "keeping head literal %d", c[0]);
      lglupdatelwch (lgl, c[0], c[1], red, lidx);
      LOG (3, "new tail literal %d", c[1]);
      (void) lglwchlrg (lgl, c[1], c[0], red, lidx);
    }
    lglonflict (lgl, 0, c[0], red, lidx);
  }
  lgl->stats.prgss++;
}

static void lglpopnunmarkstk (LGL * lgl, Stk * stk) {
  while (!lglmtstk (stk))
    lglavar (lgl, lglpopstk (stk))->mark = 0;
}

static int lglana (LGL * lgl) {
  int size, savedsize, resolventsize, level, mlevel, jlevel, glue;
  int open, resolved, tag, lit, uip, r0, r1, other, * p, * q;
  int del, cl, c0, c1, sl, s0, s1;
  AVar * av;
  if (lgl->mt) return 0;
  if (!lgl->conf.lit) return 1;
  if (!lgl->level) {
#ifndef NLGLPICOSAT
    lglpicosatchkcls (lgl);
#endif
    return 0;
  }
  lgl->stats.confs++;
  assert (lgl->conf.lit);
  assert (lglmtstk (&lgl->seen));
  assert (lglmtstk (&lgl->clause));
  assert (lglmtstk (&lgl->resolvent));
#ifndef NDEBUG
  if (lgl->opts.check.val > 0) lglchkclnvar (lgl);
#endif
  open = 0;
  lit = lgl->conf.lit, r0 = lgl->conf.rsn[0], r1 = lgl->conf.rsn[1];
RESTART:
  uip = savedsize = resolved = 0;
  LOGREASON (2, lit, r0, r1, "starting analysis with");
  open += lglpull (lgl, lit);
  for (;;) {
    LOGREASON (2, lit, r0, r1, "analyzing");
    if (resolved++) {
#ifdef RESOLVENT
      if (lglmaintainresolvent (lgl)) {
	LOG (2, "removing %d from resolvent", -lit);
	lglrmstk (&lgl->resolvent, -lit);
	LOGRESOLVENT (3, "resolvent after removing %d is", -lit);
      }
#endif
    }
    assert (lglevel (lgl, lit) == lgl->level);
    tag = r0 & MASKCS;
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      size = lglevel (lgl, other) ? 2 : 1;
      if (lglpull (lgl, other)) open++;
      if (tag == TRNCS) {
	if (lglevel (lgl, r1)) size++;
        if (lglpull (lgl, r1)) open++;
      }
    } else {
      assert (tag == LRGCS);
      p = lglidx2lits (lgl, (r0 & REDCS), r1);
      size = 0;
      while ((other = *p++)) {
	if (lglevel (lgl, other)) size++;
	if (lglpull (lgl, other)) open++;
      }
#ifndef NLGLSTATS
      if (r0 & REDCS) {
	int glue = r1 & GLUEMASK;
	Lir * lir = lgl->red + glue;
	lir->resolved++;
	assert (lir->resolved > 0);
      }
#endif
    }
    assert (open > 0);
    LOG (2, "open %d antecendents %d learned %d resolved %d", 
         open, size, lglcntstk (&lgl->clause), lglcntstk (&lgl->resolvent));
    resolventsize = open + lglcntstk (&lgl->clause);
#ifdef RESOLVENT
    LOGRESOLVENT (2, "resolvent");
    if (lglmaintainresolvent (lgl))
      assert (lglcntstk (&lgl->resolvent) == resolventsize);
#endif
    if (resolventsize < size ||
        (resolved == 2 && resolventsize < savedsize)) {
      cl = lgl->conf.lit;
      c0 = lgl->conf.rsn[0];
      c1 = lgl->conf.rsn[1];
      assert (resolved >= 2);
      del = lit;
      if (resolved > 2) ;
      else if (resolventsize >= size) del = -lit, lit = cl, r0 = c0, r1 = c1;
      else if (resolventsize >= savedsize) ;
      else {
	if (r0 & REDCS) {
	  sl = lit, s0 = r0, s1 = r1;
	  del = -lit, lit = cl, r0 = c0, r1 = c1;
	} else sl = cl, s0 = c0, s1 = c1;
	lgldynsub (lgl, sl, s0, s1);
      }
      lgldynstr (lgl, del, lit, r0, r1);
      lit = lgl->conf.lit;
      r0 = lgl->conf.rsn[0];
      r1 = lgl->conf.rsn[1];
      assert (lglevel (lgl, lit) == lgl->level);
      jlevel = 0;
      tag = r0 & MASKCS;
      if (tag == UNITCS) ;
      else if (tag == BINCS) {
	other = r0 >> RMSHFT;
	level = lglevel (lgl, other);
	if (level > jlevel) jlevel = level;
      } else if (tag== TRNCS) {
	other = r0 >> RMSHFT;
	level = lglevel (lgl, other);
	if (level > jlevel) jlevel = level;
	if (jlevel < lgl->level) {
	  other = r1;
	  level = lglevel (lgl, other);
	  if (level > jlevel) jlevel = level;
	}
      } else {
	assert (tag == LRGCS);
	p = lglidx2lits (lgl, (r0 & REDCS), r1);
	while (jlevel < lgl->level && (other = *p++)) {
	  level = lglevel (lgl, other);
	  if (level > jlevel) jlevel = level;
	}
      }
      if (jlevel >= lgl->level) goto RESTART;
      LOGREASON (2, lit, r0, r1, 
		 "driving %d at level %d through strengthened", 
		 lit, jlevel);
      lgl->stats.otfs.driving++;
      lglbumplit (lgl, lit);
      lglbacktrack (lgl, jlevel);
      lglassign (lgl, lit, r0, r1);
      goto DONE;
    }
    savedsize = size;
    while (!lglmarked (lgl, lit = lglpopstk (&lgl->trail)))
      lglunassign (lgl, lit);
    lglunassign (lgl, lit);
    if (!--open) { uip = -lit; break; }
    av = lglavar (lgl, lit);
    r0 = av->rsn[0], r1 = av->rsn[1];
  }
  assert (uip);
  LOG (2, "adding UIP %d at same level %d to FUIP clause", uip, lgl->level);
  lglpushstk (lgl, &lgl->clause, uip);
  lglbumplit (lgl, uip);
#ifdef RESOLVENT
  LOGRESOLVENT (3, "final resolvent before flushing fixed literals");
  if (lglmaintainresolvent  (lgl)) {
    q = lgl->resolvent.start;
    for (p = q; p < lgl->resolvent.top; p++)
      if (lglevel (lgl, (other = *p)))
	*q++ = other;
    lgl->resolvent.top = q;
    LOGRESOLVENT (2, "final resolvent after flushing fixed literals");
    assert (lglcntstk (&lgl->resolvent) == lglcntstk (&lgl->clause));
    for (p = lgl->clause.start; p < lgl->clause.top; p++)
      assert (lglavar (lgl, *p)->mark == 1);
    for (p = lgl->resolvent.start; p < lgl->resolvent.top; p++) {
      av = lglavar (lgl, *p); assert (av->mark == 1); av->mark = 0;
    }
    for (p = lgl->clause.start; p < lgl->clause.top; p++)
      assert (lglavar (lgl, *p)->mark == 0);
    for (p = lgl->resolvent.start; p < lgl->resolvent.top; p++) {
      av = lglavar (lgl, *p); assert (av->mark == 0); av->mark = 1;
    }
    lglclnstk (&lgl->resolvent);
  }
#endif
  lglpushstk (lgl, &lgl->clause, 0);
  LOGCLS (2, lgl->clause.start, "FUIP clause");
  mlevel = lgl->level, jlevel = 0, glue = 0;
  for (p = lgl->frames.start; p < lgl->frames.top; p++) {
    level = *p;
    if (level < mlevel) mlevel = level;
    if (level > jlevel) jlevel = level;
    glue++;
  }
  LOG (2, "jump level %d", jlevel);
  LOG (2, "minimum level %d", mlevel);
  LOG (2, "glue %d covers %.0f%%", glue, 
       (float)(jlevel ? lglpcnt (glue, (jlevel - mlevel) + 1) : 100.0));
#ifndef NLGLPICOSAT
  lglpicosatchkcls (lgl);
#endif
  lgl->stats.lits.nonmin += lglcntstk (&lgl->clause) - 1;
  q = lgl->clause.start;
  for (p = q; (other = *p); p++) 
    if (other != uip && lglmincls (lgl, other)) LOG (2, "removed %d", other);
    else *q++ = other;
  *q++ = 0;
  lgl->clause.top = q;
  LOG (2, "clause minimized by %d literals", (int)(lgl->clause.top - q));
  LOGCLS (2, lgl->clause.start, "minimized clause");
  lgl->stats.lits.learned += lglcntstk (&lgl->clause) - 1;
  lgl->stats.clauses.glue += glue;
  lgl->stats.clauses.learned++;
  if (lglavar (lgl, uip)->rsn[0]) lgl->stats.uips++;
  lglbacktrack (lgl, jlevel);
#ifndef NLGLPICOSAT
  lglpicosatchkcls (lgl);
#endif
  lgladdcls (lgl, REDCS, glue);
DONE:
#ifdef RESOLVENT
  if (lglmaintainresolvent  (lgl)) lglclnstk (&lgl->resolvent);
#endif
  lglclnstk (&lgl->clause);
  lglpopnunmarkstk (lgl, &lgl->seen);
  while (!lglmtstk (&lgl->frames)) 
    lglpoke (&lgl->control, lglpopstk (&lgl->frames), 0);
  lglbumpscinc (lgl);
  if (!lgl->level) { lgl->stats.iterations++; lglrep (lgl, 2, 'i'); }
  return 1;
}

static int lgluby (int i) {
  int k;
  for (k = 1; k < 32; k++)
    if (i == (1 << k) - 1)
      return 1 << (k - 1);

  for (k = 1;; k++)
    if ((1 << (k - 1)) <= i && i < (1 << k) - 1)
      return lgluby (i - (1 << (k-1)) + 1);
}

static void lglincrestartl (LGL * lgl, int skip) {
  int delta = lgl->opts.restartint.val * lgluby (++lgl->limits.restart.luby);
  lgl->limits.restart.confs = lgl->stats.confs + delta;
  if (lgl->limits.restart.wasmaxdelta) lglrep (lgl, 1, skip ? 'N' : 'R');
  else lglrep (lgl, 2, skip ? 'n' : 'r');
  if (delta > lgl->limits.restart.maxdelta) {
    lgl->limits.restart.wasmaxdelta = 1;
    lgl->limits.restart.maxdelta = delta;
  } else lgl->limits.restart.wasmaxdelta = 0;
}

static void lglincrebiasl (LGL * lgl) {
  int delta = lgl->opts.rebiasint.val * lgluby (++lgl->limits.rebias.luby);
  lgl->limits.rebias.confs = lgl->stats.confs + delta;
}

static void lglrestart (LGL * lgl) {
  int glue, lidx, i, lit, skip = 0;
  Val tmp;
  Stk * s;
  if (lgl->limits.restart.wasmaxdelta) skip = 0;
  else skip = (lgl->flips >= lgl->opts.agile.val * 100000);
  if (skip) {
    lgl->stats.skipped++;
    LOG (1, "skipping restart with agility %.0f%%", lglagility (lgl));
  } else {
    lgl->stats.restarts++;
    LOG (1, "restarting with agility %.0f%%", lglagility (lgl));
    s = &lgl->red[MAXGLUE].lits;
    lidx = lglcntstk (s) - 1;
    while (lidx > 0 && lglpeek (s, lidx - 1))
      lidx--;
    if (lidx >= 0) {
      assert (lgl->level > 0);
      glue =  lgl->limits.restart.wasmaxdelta ? 0 : MAXGLUE - 1;
      assert (lglmtstk (&lgl->clause));
      for (i = lidx; (lit = lglpeek (s, i)); i++) {
	tmp = lglfixed (lgl, lit);
	if (tmp < 0) continue;
	if (tmp > 0) break;
	lglpushstk (lgl, &lgl->clause, lit);
      }
      if (lit) lidx = -1;
      else lglpushstk (lgl, &lgl->clause, 0);
    }
    lglbacktrack (lgl, 0);
    if (lidx >= 0) lgladdcls (lgl, REDCS, glue);
    lglclnstk (&lgl->clause);
  }
  lglincrestartl (lgl, skip);
}

static void lgldefrag (LGL * lgl) {
  int * p, * eow, * q, lit, idx, sign, move, recycled, ld, offset;
  int next, cnt, blit;
  HTS * hts;
  lglstart (lgl, &lgl->stats.tms.dfg);
  lgl->stats.defrags++;
  LOG (2, "recycling %d free watch stacks altogether", lgl->nfreewchs);
  for (ld = 0; ld < MAXLDFW; ld++) {
    cnt = 0;
    offset = lgl->freewchs[ld];
    if (offset != INT_MAX) {
      lgl->freewchs[ld] = INT_MAX;
      assert (lgl->nfreewchs > 0);
      lgl->nfreewchs--;
      cnt++;
      while ((next = lglpeek (&lgl->wchs, offset)) != INT_MAX) {
	lglpoke (&lgl->wchs, offset, INT_MAX);
	assert (lgl->nfreewchs > 0);
	lgl->nfreewchs--;
	LOG (3, "recycled watch stack at %d of size %d", offset, (1<<ld));
 	offset = next;
	cnt++;
      }
      if (cnt) LOG (3, "recycled %d watch stacks of size %d", cnt, (1<<ld));
    }
  }
  cnt = 0;
  assert (!lgl->nfreewchs);
  assert (lglmtstk (&lgl->saved));
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = idx * sign;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      p = lglhts2wchs (lgl, hts);
      blit = *p;
      assert (lglabs (blit) != INT_MAX);
      lglpushstk (lgl, &lgl->saved, blit);
      cnt++;
      *p = -INT_MAX;
      offset = (1 << lglceilld (hts->count));
#ifndef NDEBUG
      assert (lglispow2 (offset));
      for (q = p + hts->count; q < p + offset; q++)
	assert (!*q);
#endif
      if (!p[offset]) p[offset] = INT_MAX;
    }
  LOG (3, "saved %d blits", cnt);
  move = 1;
  eow = lgl->wchs.top - 1;
  for (p = lgl->wchs.start + 1;  p < eow; p++) {
    blit = *p;
    if (blit == INT_MAX) {
      while (!p[1]) p++;
    } else {
      assert (blit == -INT_MAX);
      *p = move;
      LOG (3, "moving watch stack at %d to %d", p - lgl->wchs.start, move);
      while (lglabs (p[1]) != INT_MAX) p++, move++;
      move++;
    }
  }
  assert (p == eow);
  assert (*p == INT_MAX);
  recycled = lglcntstk (&lgl->wchs) - move - 1;
  LOG (2, "recycling %d watch stack words", recycled);
  cnt = 0;
  q = lgl->saved.start;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = idx * sign;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      p = lglhts2wchs (lgl, hts);
      hts->offset = *p;
      *p = *q++;
      cnt++;
    }
  assert (q == lgl->saved.top);
  assert (cnt == lglcntstk (&lgl->saved));
  LOG (3, "restored %d blits and moved all HTS offsets", cnt);
  lglclnstk (&lgl->saved);
  q = lgl->wchs.start + 1;
  for (p = q; p < eow; p++) {
    blit = *p;
    if (blit == INT_MAX) {
      while (!p[1]) p++;
    } else {
      while (*p != INT_MAX)
	*q++ = *p++;
      --p;
    }
  }
  assert (p == eow);
  assert (*p == INT_MAX);
  *q++ = INT_MAX;
  LOG (2, "shrinking global watcher stack by %d", lgl->wchs.top - q);
  lgl->wchs.top = q;
  lglredstk (lgl, &lgl->wchs, 0, 2);
  lgl->limits.dfg.pshwchs = lgl->stats.pshwchs + lgl->opts.defragint.val;
  lgl->limits.dfg.prgss = lgl->stats.prgss;
  lglrep (lgl, 2, 'f');
  lglstop (lgl);
}

static void lgldis (LGL * lgl) {
  int blit, nblit, tag, red, * p, * q, * eow, * w;
  int idx, sign, lit, other, other2;
  Stk bins, trns;
  Val val, val2;
  HTS * hts;
  assert (!lgl->level);
  CLR (bins); CLR (trns);
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = idx * sign;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      val = lglval (lgl, lit);
      assert (hts->count > 0);
      if (val || lgliselim (lgl, lit)) 
	{ lglshrinkhts (lgl, hts, 0); continue; }
      assert (lglisfree (lgl, lit));
      assert (lglmtstk (&bins));
      assert (lglmtstk (&trns));
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	red = blit & REDCS;
	if (tag == IRRCS) continue;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag == LRGCS) continue;
	other = blit >> RMSHFT;
	val = lglval (lgl, other);
	if (val > 0) continue;
	if (lgliselim (lgl, other)) continue;
	if (tag == BINCS) {
	  assert (!val);
	  lglpushstk (lgl, &bins, blit);
	  continue;
	}
	assert (tag == TRNCS);
	other2 = *p;
	val2 = lglval (lgl, other2);
	if (val2 > 0) continue;
	if (lgliselim (lgl, other2)) continue;
	if (val < 0) {
	  assert (val < 0 && !val2);
	  nblit = red | (other2<<RMSHFT) | BINCS;
	  lglpushstk (lgl, &bins, nblit);
	  continue;
	}
	if (val2 < 0) {
	  assert (!val && val2 < 0);
	  nblit = red | (other<<RMSHFT) | BINCS;
	  lglpushstk (lgl, &bins, nblit);
	  continue;
	}
	assert (!val && !val2);
	lglpushstk (lgl, &trns, blit);
	lglpushstk (lgl, &trns, other2);
      }
      q = w;
      for (p = bins.start; p != bins.top; p++) *q++ = *p;
      for (p = trns.start; p != trns.top; p++) *q++ = *p;
      lglshrinkhts (lgl, hts, q - w);
      lglclnstk (&bins);
      lglclnstk (&trns);
    }
  lglrelstk (lgl, &bins);
  lglrelstk (lgl, &trns);
}

static void lglconnaux (LGL * lgl, int red, int glue, Stk * stk) {
  const int * p, * c, * start, * top;
  int lit, satisfied, lidx, size;
  int * q, * d;
  Val val;
  assert (red == 0 || red == REDCS);
  assert (0 <= glue && glue < MAXGLUE);
  c = start = q = stk->start;
  top = stk->top;
  while (c < top) {
    p = c; 
    d = q;
    satisfied = 0;
    while (assert (p < top), (lit = *p++)) {
      if (lit == REMOVED) { satisfied = 1; break; } // TODO clearer ...
      if (satisfied) continue;
      val = lglval (lgl, lit);
      if (lgliselim (lgl, lit)) assert (lgl->eliminating), satisfied = 1;
      else if (val > 0) satisfied = 1;
      else if (!val) *q++ = lit;
    }
    if (satisfied || p == c + 1) q = d;
    else {
      size = q - d;
      if (size == 2) {
	q = d;
	lglwchbin (lgl, d[0], d[1], red);
	lglwchbin (lgl, d[1], d[0], red);
      } else if (size == 3) {
	q = d;
	lglwchtrn (lgl, d[0], d[1], d[2], red);
	lglwchtrn (lgl, d[1], d[0], d[2], red);
	lglwchtrn (lgl, d[2], d[0], d[1], red);
      } else {
	assert (size > 3);
	*q++ = 0;
	lidx = d  - start;
	if (red) {
	  assert (lidx <= MAXREDLIDX);
	  lidx <<= GLUESHFT;
	  assert (0 <= lidx);
	  lidx |= glue;
	}
	lglwchlrg (lgl, d[0], d[1], red, lidx);
	lglwchlrg (lgl, d[1], d[0], red, lidx);
      }
    }
    c = p;
  }
  stk->top = q;
}

static void lglcon (LGL * lgl) {
  int glue;
  lglconnaux (lgl, 0, 0, &lgl->irr);
  for (glue = 0; glue < MAXGLUE; glue++)
    lglconnaux (lgl, REDCS, glue, &lgl->red[glue].lits);
}

static void lglcount (LGL * lgl) {
  int idx, sign, lit, tag, blit, red, other, other2, glue, count;
  const int * p, * w, * c, * eow;
  HTS * hts;
  Lir * lir;
  lgl->stats.irr = 0;
  lgl->stats.red.bin = 0;
  lgl->stats.red.trn = 0;
  lgl->stats.red.lrg = 0;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	red = blit & REDCS;
	tag = blit & MASKCS;
	if (tag == LRGCS || tag == TRNCS) p++;
	if (tag == LRGCS) continue;
	assert (tag == BINCS || tag == TRNCS);
	other = blit >> RMSHFT;
	assert (lglabs (other) != lglabs (lit));
	if (lglabs (lit) >= lglabs (other)) continue;
	assert (2 == BINCS && 3 == TRNCS);
	if (tag == TRNCS) {
	  other2 = *p;
	  assert (lglabs (other2) != lglabs (lit));
	  assert (lglabs (other2) != lglabs (other));
	  if (lglabs (lit) >= lglabs (other2)) continue;
	}
	if (!red) lgl->stats.irr++;
	else if (tag == BINCS) lgl->stats.red.bin++;
	else assert (tag == TRNCS), lgl->stats.red.trn++;
      }
    }
  assert (lgl->stats.red.bin >= 0 && lgl->stats.red.trn >= 0);
  for (c = lgl->irr.start; c < lgl->irr.top; c++)
    if (!*c) lgl->stats.irr++;
  assert (lgl->stats.irr >= 0);
  if (lgl->stats.irr)
    LOG (1, "counted %d irredundant clauses", lgl->stats.irr);
  for (glue = 0; glue < MAXGLUE; glue++) {
    lir = lgl->red + glue;
    count = 0;
    for (c = lir->lits.start; c < lir->lits.top; c++)
      if (!*c) count++;
    if (count)
      LOG (1, "counted %d redundant clauses with glue %d", count, glue);
    lgl->stats.red.lrg += count;
  }
  assert (lgl->stats.red.lrg >= 0);
  if (lgl->stats.red.bin)
    LOG (1, "counted %d binary redundant clauses altogether",
         lgl->stats.red.bin);
  if (lgl->stats.red.trn)
    LOG (1, "counted %d ternary redundant clauses altogether",
         lgl->stats.red.trn);
  if (lgl->stats.red.lrg)
    LOG (1, "counted %d large redundant clauses altogether",
         lgl->stats.red.lrg);
}

static int lglulit (int lit) { return 2*lglabs (lit) + (lit < 0); }

static int lglilit (int ulit) {
  int res = ulit/2;
  assert (res >= 1);
  if (ulit & 1) res = -res;
  return res;
}

static void lglincjwh (Flt * jwh, int lit, Flt inc) {
  int ulit = lglulit (lit);
  Flt old = jwh[ulit];
  Flt new = lgladdflt (old, inc);
  jwh[ulit] = new;
}

static void lglbias (LGL * lgl) {
  int idx, sign, lit, tag, blit, other, other2, glue, npos, nneg, bias, size;
  const int *p, * w, * eow, * c;
  Flt * jwh, inc, pos, neg;
  HTS * hts;
  Stk * s;
  for (idx = 2; idx < lgl->nvars; idx++)
    lgl->avars[idx].phase = 0;
  if (lgl->opts.phase.val) return;
  NEW (jwh, 2 * lgl->nvars);
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag == LRGCS) continue;
	other = blit >> RMSHFT;
	if (tag == BINCS) {
	  if (lglabs (other) < lglabs (lit)) continue;
	  inc = lglflt (-2, 1);
	  lglincjwh (jwh, lit, inc);
	  lglincjwh (jwh, other, inc);
	} else {
	  assert (tag == TRNCS);
	  other2 = *p;
	  if (lglabs (other) < lglabs (lit)) continue;
	  if (lglabs (other2) < lglabs (lit)) continue;
	  inc = lglflt (-3, 1);
	  lglincjwh (jwh, lit, inc);
	  lglincjwh (jwh, other, inc);
	  lglincjwh (jwh, other2, inc);
	}
      }
    }
  for (glue = -1; glue < MAXGLUE; glue++) {
    s = (glue < 0) ? &lgl->irr : &lgl->red[glue].lits;
    for (c = s->start; c < s->top; c = p + 1) {
      p = c;
      if (*p == REMOVED) continue;
      while (*p) p++;
      for (; *p; p++)
	;
      size = p - c;
      assert (size > 3);
      inc = lglflt (-size, 1);
      for (p = c; (other = *p); p++)
	lglincjwh (jwh, other, inc);
    }
  }
  nneg = npos = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    pos = jwh[lglulit (idx)];
    neg = jwh[lglulit (-idx)];
    if (pos > neg) { bias = 1; npos++; } else { bias = -1; nneg++; }
    lgl->avars[idx].bias = bias;
  }
  DEL (jwh, 2 * lgl->nvars);
  lglprt (lgl, 2, "%d (%.0f%%) positive phase bias, %d (%.0f%%) negative",
	     npos, lglpcnt (npos, lgl->nvars-2),
	     nneg, lglpcnt (nneg, lgl->nvars-2));
}

static void lglrebias (LGL * lgl) {
  lgl->stats.rebias++;
  LOG (1, "rebias %d", lgl->stats.rebias);
  lglbias (lgl);
  lglincrebiasl (lgl);
}

static void lglchkflushed (LGL * lgl) {
  assert (lgl->next == lglcntstk (&lgl->trail));
  assert (!lgl->conf.lit);
  assert (!lgl->mt);
}

static int lglcutwidth (LGL * lgl) {
  int lidx, res, l4, r4, b4, l10, r10, b10, m, oldbias;
  int idx, sign, lit, blit, tag, red, other, other2;
  const int * p, * w, * eow, * c, * q;
  int * widths, max, cut, min4, min10;
  int64_t sum, avg;
  HTS * hts;
  if (!lgl->nvars) return 0;
  min4 = min10 = INT_MAX;
  sum = max = cut = 0;
  NEW (widths, lgl->nvars);
  l4 = 2 + (lgl->nvars - 2 + 3)/4;
  r4 = 2 + (3*(lgl->nvars - 2)+3)/4;
  assert (2 <= l4 && l4 <= r4 && r4 <= lgl->nvars);
  l10 = 2 + (lgl->nvars - 2 + 9)/10;
  r10 = 2 + (9*(lgl->nvars - 2)+9)/10;
  assert (2 <= l10 && l10 <= r10 && r10 <= lgl->nvars);
  b4 = b10 = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    for (sign = -1; sign <= 1; sign += 2) {
      lit = idx * sign;
      hts = lglhts (lgl, lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	red = blit & REDCS;
	tag = blit & MASKCS;
	other = lglabs (blit >> RMSHFT);
	if (tag == BINCS) {
	  if (red) continue;
	  if (other > idx) widths[other]++, cut++;
	} else if (tag == TRNCS) {
	  other2 = lglabs (*++p);
	  if (red) continue;
	  if (other > idx) widths[other]++, cut++;
	  if (other2 > idx) widths[other2]++, cut++;
	} else {
	  assert (tag == LRGCS);
	  lidx = *++p;
	  if (red) continue;
	  c = lglidx2lits (lgl, red, lidx);
	  for (q = c; (other = lglabs (*q)); q++) {
	    if (other == idx) continue;
	    if (other > idx) widths[other]++, cut++;
	  }
	}
      }
    }
    assert (0 <= cut && 0 <= widths[idx]);
    cut -= widths[idx];
    assert (cut >= 0);
    if (cut > max) max = cut;
    if (l4 <= idx && idx <= r4 && cut < min4) b4 = idx, min4 = cut;
    if (l10 <= idx && idx <= r10 && cut < min10) b10 = idx, min10 = cut;
    sum += cut;
    assert (sum >= 0);
  }
  DEL (widths, lgl->nvars);
  assert (lgl->nvars > 0);
  avg = sum / (long long) lgl->nvars;
  assert (avg <= INT_MAX);
  res = (int) avg;
  lglprt (lgl, 2, 
    "cut width %d, max %d, min4 %d at %.0f%%, min10 %d at %.0f%%", 
    res, max, 
    min4, lglpcnt ((b4 - 2), lgl->nvars - 2),
    min10, lglpcnt ((b10 - 2), lgl->nvars - 2));
  oldbias = lgl->bias;
  assert (oldbias);
  m = (lgl->nvars + 2)/2;
  if (b4 < m && b10 < m) lgl->bias = -1;
  if (b4 > m && b10 > m) lgl->bias = 1;
  if (oldbias != lgl->bias) lgldreschedule (lgl);
  lglprt (lgl, 2, "decision bias %d", lgl->bias);
  return res;
}

static int lglmaplit (int * map, int lit) {
  return map [ lglabs (lit) ] * lglsgn (lit);
}

static void lglmapstk (LGL * lgl, int * map, Stk * lits) {
  int * p, * eol;
  eol = lits->top;
  for (p = lits->start; p < eol; p++)
    *p = lglmaplit (map, *p);
}

static void lglmaplits (LGL * lgl, int * map) {
  int glue;
  lglmapstk (lgl, map, &lgl->irr);
  for (glue = 0; glue < MAXGLUE; glue++)
    lglmapstk (lgl, map, &lgl->red[glue].lits);
}

static void lglmapvars (LGL * lgl, int * map, int nvars) {
  int i, oldnvars = lgl->nvars;
  int * i2e;
  DVar * dvars;
  AVar * avars;
  Val * vals;

  if (nvars > 2) assert (nvars <= oldnvars);
  else nvars = 0;

  NEW (vals, nvars);
  for (i = 2; i < oldnvars; i++)
    if (lglisfree (lgl, i))
      vals[map[i]] = lgl->vals[i];
  DEL (lgl->vals, lgl->szvars);
  lgl->vals = vals;

  NEW (i2e, nvars);
  for (i = 2; i < oldnvars; i++)
    if (lglisfree (lgl, i))
      i2e[map[i]] = lgl->i2e[i];
  DEL (lgl->i2e, lgl->szvars);
  lgl->i2e = i2e;

  NEW (dvars, nvars);
  for (i = 2; i < oldnvars; i++)
    if (lglisfree (lgl, i))
      dvars[map[i]] = lgl->dvars[i];
  DEL (lgl->dvars, lgl->szvars);
  lgl->dvars = dvars;

  NEW (avars, nvars);
  for (i = 2; i < oldnvars; i++)
    if (lglisfree (lgl, i))
      avars[map[i]] = lgl->avars[i];
  DEL (lgl->avars, lgl->szvars);
  lgl->avars = avars;

  lgl->nvars = lgl->szvars = nvars;
  lgl->stats.fixed.current = 0;
}

static void lglmaphts (LGL * lgl, int * map) {
  int idx, sign, lit, * w, *eow, * p, other, other2, blit, tag, red;
  int newblit, newother, newother2;
  HTS * hts;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->count) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	assert (tag == BINCS || tag == TRNCS || tag == LRGCS);
	red = blit & REDCS;
	other = blit >> RMSHFT;
	newother = lglmaplit (map, other);
	newblit = (newother << RMSHFT) | tag | red;
	*p = newblit;
	if (tag == BINCS) continue;
	other2 = *++p;
	if (tag == LRGCS) continue;
	assert (tag == TRNCS);
	newother2 = lglmaplit (map, other2);
	*p = newother2;
      }
    }
}

static void lglmaptrail (LGL * lgl, int * map) {
  int * p, * q, src, dst;
  for (p = lgl->trail.start; p < lgl->trail.top; p++) {
    src = *p;
    if (lglevel (lgl, src) > 0) break;
  }
  for (q = lgl->trail.start; p < lgl->trail.top; p++) {
    src = *p;
    assert (lglevel (lgl, src) > 0);
    dst = lglmaplit (map, src);
    *q++ = dst;
  }
  lgl->trail.top = q;
  lgl->next = lglcntstk (&lgl->trail);
}

static int lglirepr (LGL * lgl, int lit) {
#ifndef NDEBUG
  int prev = 0, count = 0;
#endif
  int next, idx, res, sgn;
  assert (lgl->repr);
  next = lit;
  do {
    res = next;
    idx = lglabs (res);
    sgn = lglsgn (res);
    next = lgl->repr[idx];
    next *= sgn;
#ifndef NDEBUG
    if (prev || next) assert (prev != next);
    prev = res;
    assert (count++ <= lgl->nvars);
#endif
  } while (next);
  while (lit != res) {
    idx = lglabs (lit), sgn = lglsgn (lit);
    next = lgl->repr[idx] * sgn;
    lgl->repr[idx] = sgn * res;
    lit = next;
  }
  return res;
}

static void lglmape2i (LGL * lgl, int * map) {
  int * p, * start, * top, repr, elit, ilit, mappedilit;
  start = lgl->e2i.start;
  top = lgl->e2i.top;
  for (p = start + 1; p < top; p++) {
    elit = p - start;
    repr = lglerepr (lgl, elit);
    if (repr != elit) continue;
    ilit = lglpeek (&lgl->e2i, elit);
    assert (!(ilit & 1));
    ilit /= 2;
    mappedilit = lglmaplit (map, ilit);
    LOG (3, "mapping external %d to internal %d", elit, mappedilit);
    lglpoke (&lgl->e2i, elit, 2*mappedilit);
  }
}

static void lglmap (LGL * lgl) {
  int idx, size, dst, count, * map, oldnvars, repr;
  AVar * av;
  Val val;
  assert (!lgl->level);
  size = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    av = lglavar (lgl, idx);
    if (av->type == ELIMVAR) continue;
    if (av->type != FREEVAR) continue;
    assert (!lglval (lgl, idx));
    size++;
  }
  LOG (1, "mapping %d remaining variables", size);
  oldnvars = lgl->nvars;
  NEW (map, lglmax (oldnvars, 2));
  map[0] = 0, map[1] = 1;
  count = 0;
  dst = 2;
  for (idx = 2; idx < lgl->nvars; idx++) {
    av = lglavar (lgl, idx);
    if (av->type == FREEVAR) {
      assert (idx > 0 && !map[idx]);
      LOG (3, "mapping free %d to %d", idx, count + 2);
      map[idx] = count + 2;
      count++;
    } else if (av->type == EQUIVAR) {
      assert (lgl->repr);
      repr = lglirepr (lgl, idx);
      assert (lglabs (repr) < idx);
      dst = lglmaplit (map, repr);
      LOG (3, "mapping equivalent %d to %d", idx, dst);
      map[idx] = dst;
    } else if (av->type == FIXEDVAR) {
      val = lgl->vals[idx];
      assert (val);
      LOG (3, "mapping assigned %d to %d", idx, (int) val);
      map[idx] = val;
    } else {
      assert (av->type == ELIMVAR);
      map[idx] = 0;
    }
  }
  assert (count == size);
  lglmaptrail (lgl, map);
  lglmapvars (lgl, map, size + 2);
  lglmaplits (lgl, map);
  lglmapstk (lgl, map, &lgl->dsched);
  lglmape2i (lgl, map);
  assert (lglmtstk (&lgl->clause));
  lglmaphts (lgl, map);
  DEL (map, lglmax (oldnvars, 2));
  if (lgl->repr) DEL (lgl->repr, oldnvars);
  lgldreschedule (lgl);
}

#ifndef NLGLPICOSAT
#ifndef NDEBUG
static void lglpicosataddstk (const Stk * stk) {
  const int * p;
  int lit;
  for (p = stk->start; p < stk->top; p++) {
    lit = *p;
    if (lit == REMOVED) continue;
    picosat_add (lit);
  }
}

static void lglpicosataddhts (LGL * lgl) {
  int idx, sign, lit, tag, blit, other, other2;
  const int * w, * p, * eow;
  HTS * hts;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->count) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == BINCS) {
	  other = blit >> RMSHFT;
	  if (lglabs (other) < idx) continue;
	  picosat_add (lit);
	  picosat_add (other);
	  picosat_add (0);
	} else if (tag == TRNCS) {
	  other = blit >> RMSHFT;
	  other2 = *++p;
	  if (lglabs (other) < idx) continue;
	  if (lglabs (other2) < idx) continue;
	  picosat_add (lit);
	  picosat_add (other);
	  picosat_add (other2);
	  picosat_add (0);
	} else if (tag == LRGCS) p++;
	else assert (lgl->dense && tag == IRRCS);
      }
    }
}
#endif

static void lglpicosatchkallandrestart (LGL * lgl) {
#ifndef NDEBUG
  int res;
  if (!lgl->opts.check.val) return;
  if (lgl->opts.check.val >= 2) {
    res = picosat_sat (-1);
    LOG (1, "PicoSAT returns %d", res);
    if (lgl->picosat.res) assert (res = lgl->picosat.res);
    lgl->picosat.res = res;
  }
  if (picosat_inconsistent ()) {
    assert (!lgl->picosat.res || lgl->picosat.res == 20);
    lgl->picosat.res = 20;
  }
  LOG (1, "resetting and reinitializing PicoSAT");
  picosat_reset ();
  picosat_init ();
  lglpicosataddhts (lgl);
  lglpicosataddstk (&lgl->irr);
#endif
}
#endif

static int lglfixedoreq (LGL * lgl) {
  return lgl->stats.fixed.sum + lgl->stats.equivalent.sum;
}

static void lglgc (LGL * lgl) {
  if (!lgl->eliminating && !lgl->blocking &&
      lglfixedoreq (lgl) == lgl->limits.gc.fixedoreq) return;
  if (lgl->decomposing) lglstart (lgl, &lgl->stats.tms.gcd);
  else if (lgl->eliminating) lglstart (lgl, &lgl->stats.tms.gce);
  else if (lgl->blocking) lglstart (lgl, &lgl->stats.tms.gcb);
  else lglstart (lgl, &lgl->stats.tms.gc);
  lglchkflushed (lgl);
  lglrep (lgl, 2, 'g');
  lgl->stats.gcs++;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  lgldis (lgl);
  lglcon (lgl);
  lglcount (lgl);
  lgldreschedule (lgl);
  lglmap (lgl);
#ifndef NLGLPICOSAT
  lglpicosatchkallandrestart (lgl);
#endif
  if (!lgl->simp) {
    lgl->limits.gc.vinc += lgl->opts.gcvintinc.val;
    lgl->limits.gc.cinc += lgl->opts.gccintinc.val;
  }
  lgl->limits.gc.visits = lgl->stats.visits.search + lgl->limits.gc.vinc;
  lgl->limits.gc.confs = lgl->stats.confs + lgl->limits.gc.cinc;
  lgl->limits.gc.fixedoreq = lglfixedoreq (lgl);
  lglrep (lgl, 2, 'c');
  lglstop (lgl);
}

static void lglassume (LGL * lgl, int lit) {
  if (!lgl->level++) lgl->decision = lit;
  lglpushstk (lgl, &lgl->control, 0);
  LOG (2, "assuming %d", lit);
  lglassign (lgl, lit, DECISION, 0);
}

static int lgldecide (LGL * lgl) {
  unsigned start, pos, delta, size;
  int lit, bias;
  AVar * av;
  lglchkflushed (lgl);
  if (!lgl->unassigned) return 0;
  if (lgl->opts.randec.val &&
      lgl->limits.randec <= lgl->stats.decisions) {
    lgl->limits.randec = lgl->stats.decisions;
    lgl->limits.randec += lgl->opts.randecint.val/2;
    lgl->limits.randec += lglrand (lgl) % lgl->opts.randecint.val;
    size = lglcntstk (&lgl->dsched);
    if (!size) return 0;
    pos = start = lglrand (lgl) % size;
    lit = lglpeek (&lgl->dsched, pos);
    assert (lglabs (lit) > 1);
    if (lglval (lgl, lit)) {
      delta = lglrand (lgl) % size;
      if (size == 1) return 0;
      if (!delta) delta++;
      while (lglgcd (delta, size) != 1)
	if (++delta == size) delta = 1;
      do {
	pos += delta;
	if (pos >= size) pos -= size;
	if (pos == start) return 0;
	lit = lglpeek (&lgl->dsched, pos);
	assert (lglabs (lit) > 1);
      } while (lglval (lgl, lit));
    }
    LOG (2, "random decision %d", lit);
    lgl->stats.randecs++;
  } else {
    for (;;) {
      if (lglmtstk (&lgl->dsched)) return 0;
      lit = lglpopdsched (lgl);
      if (lglabs (lit) <= 1) continue;//TODO remove?
      if (!lglval (lgl, lit)) break;
    }
  }
  av = lglavar (lgl, lit);
  if (!av->phase) {
    bias = lgl->opts.phase.val;
    if (!bias) bias = av->bias;
    av->phase = bias;
  }
  if (av->phase < 0) lit = -lit;
  LOG (2, "next decision %d", lit);
  lgl->stats.decisions++;
  lgl->stats.height += lgl->level;
  lglassume (lgl, lit);
  return 1;
}

static void lgldcpdis (LGL * lgl) {
  int idx, sign, lit, tag, blit, red, other, other2, i;
  const int * w, * p, * eow;
  Val val;
  HTS * hts;
  Stk * s;
  assert (lglmtstk (&lgl->dis.irr.bin));
  assert (lglmtstk (&lgl->dis.irr.trn));
  assert (lglmtstk (&lgl->dis.red.bin));
  assert (lglmtstk (&lgl->dis.red.trn));
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      assert (hts->count > 0);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      hts->count = hts->offset = 0;
      val = lglval (lgl, lit);
      if (val > 0) continue;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag == LRGCS) continue;
	other = blit >> RMSHFT;
	if (lglabs (other) < idx) continue;
	val = lglval (lgl, other);
	if (val > 0) continue;
	red = blit & REDCS;
	if (red && !lglisfree (lgl, other)) continue;
	if (tag == BINCS) {
	  s = red ? &lgl->dis.red.bin : &lgl->dis.irr.bin;
	} else {
	  assert (tag == TRNCS);
	  other2 = *p;
	  if (lglabs (other2) < idx) continue;
	  val = lglval (lgl, other2);
	  if (val > 0) continue;
	  if (red && !lglisfree (lgl, other2)) continue;
	  s = red ? &lgl->dis.red.trn : &lgl->dis.irr.trn;
	  lglpushstk (lgl, s, other2);
	}
	lglpushstk (lgl, s, other);
	lglpushstk (lgl, s, lit);
	lglpushstk (lgl, s, 0);
      }
    }
  lglrststk (&lgl->wchs, 2);
  lgl->wchs.top[-1] = INT_MAX;
  for (i = 0; i < MAXLDFW; i++) lgl->freewchs[i] = INT_MAX;
  lgl->nfreewchs = 0;
}

static int lgldcpclnstk (LGL * lgl, Stk * s) {
  int newbins, newunits, mt, oldsz, newsz, lit, mark, satisfied, repr;
  const int * p, * c, * eos = s->top;
  int * q, * d;
  Val val;
  mt = newbins = newunits = 0;
  q = s->start;
  for (c = q; c < eos; c = p + 1) {
    d = q;
    satisfied = 0;
    for (p = c; assert (p < eos), (lit = *p); p++) {
      if (lit == REMOVED) { satisfied = 1; break; }
      if (satisfied) continue;
      repr = lglirepr (lgl, lit);
      val = lglval (lgl, repr);
      if (val > 0) { satisfied = 1; continue; }
      if (val < 0) continue;
      mark = lglmarked (lgl, repr);
      if (mark < 0) { satisfied = 1; continue; }
      if (mark > 0) continue;
      lglmark (lgl, repr);
      *q++ = repr;
    }
    oldsz = p - c;
    for (c = d; c < q; c++) lglunmark (lgl, *c);
    if (satisfied || !oldsz) { q = d; continue; }
    newsz = q - d;
    if (newsz >= 2) {
      *q++ = 0;
      if (newsz == 2 && oldsz > 2) newbins++;
    } else {
      q = d;
      if (!newsz) {
	LOG (1, "found empty clause while cleaning decompostion");
	lgl->mt = 1;
	mt = 1;
      } else {
	assert (newsz == 1);
	LOG (1, "new unit %d while cleaning decomposition", d[0]);
	lglunit (lgl, d[0]);
	newunits++;
      }
    }
  }
  s->top = q;
  return newbins || newunits || mt;
}
  
static void lgldcpconnaux (LGL * lgl, int red, int glue, Stk * s) {
  int * start = s->start, * q, * d, lit, size, lidx;
  const int * p, * c, * eos = s->top;
  assert (red == 0 || red == REDCS);
  assert (!glue || red);
  q = start;
  for (c = q; c < eos; c = p + 1) {
    d = q;
    for (p = c; (lit = *p); p++) {
      assert (!lgl->repr[lglabs (lit)]);
      assert (!lgl->vals[lglabs (lit)]);
      *q++ = lit;
    }
    size = q - d;
    if (size == 2) {
      q = d;
      lglwchbin (lgl, d[0], d[1], red);
      lglwchbin (lgl, d[1], d[0], red);
    } else if (size == 3) {
      q = d;
      lglwchtrn (lgl, d[0], d[1], d[2], red);
      lglwchtrn (lgl, d[1], d[0], d[2], red);
      lglwchtrn (lgl, d[2], d[0], d[1], red);
    } else {
      assert (size > 3);
      *q++ = 0;
      lidx = d - start;
      if (red) {
	assert (lidx <= MAXREDLIDX);
	lidx <<= GLUESHFT;
	assert (0 <= lidx);
	lidx |= glue;
      }
      lglwchlrg (lgl, d[0], d[1], red, lidx);
      lglwchlrg (lgl, d[1], d[0], red, lidx);
    }
  }
  s->top = q;
}

static void lgldcpcon (LGL * lgl) {
  int glue;
  lgldcpconnaux (lgl, 0, 0, &lgl->dis.irr.bin);
  lgldcpconnaux (lgl, REDCS, 0, &lgl->dis.red.bin);
  lgldcpconnaux (lgl, 0, 0, &lgl->dis.irr.trn);
  lgldcpconnaux (lgl, REDCS, 0, &lgl->dis.red.trn);
  lglrelstk (lgl, &lgl->dis.irr.bin);
  lglrelstk (lgl, &lgl->dis.irr.trn);
  lglrelstk (lgl, &lgl->dis.red.bin);
  lglrelstk (lgl, &lgl->dis.red.trn);
  lgldcpconnaux (lgl, 0, 0, &lgl->irr);
  for (glue = 0; glue < MAXGLUE; glue++)
    lgldcpconnaux (lgl, REDCS, glue, &lgl->red[glue].lits);
}

static int lgldcpcln (LGL * lgl) {
  int res = 0, glue, old, rounds = 0;
  do {
    rounds++;
    old = lgl->stats.fixed.current;
    if (lgldcpclnstk (lgl, &lgl->irr)) res = 1;
    if (lgldcpclnstk (lgl, &lgl->dis.irr.bin)) res = 1;
    if (lgldcpclnstk (lgl, &lgl->dis.irr.trn)) res = 1;
    if (lgldcpclnstk (lgl, &lgl->dis.red.bin)) res = 1;
    if (lgldcpclnstk (lgl, &lgl->dis.red.trn)) res = 1;
    for (glue = 0; glue < MAXGLUE; glue++)
      if (lgldcpclnstk (lgl, &lgl->red[glue].lits)) res = 1;
  } while (old < lgl->stats.fixed.current);
  LOG (1, "iterated %d decomposition cleaning rounds", rounds);
  return res;
}

static void lglemerge (LGL * lgl, int ilit0, int ilit1) {
  int elit0 = lgl->i2e[lglabs (ilit0)] * lglsgn (ilit0);
  int elit1 = lgl->i2e[lglabs (ilit1)] * lglsgn (ilit1);
  int repr0 = lglerepr (lgl, elit0);
  int repr1 = lglerepr (lgl, elit1);
#ifndef NDEBUG
  int repr = repr1;
#endif
  int tmp;
  assert (lglabs (repr0) != lglabs (repr1));
  if (repr0 < 0) repr0 *= -1, repr1 *= -1;
  lglpoke (&lgl->e2i, repr0, 2*repr1 + lglsgn (repr1));
  LOG (2, "merging external literals %d and %d", repr0, repr1);
  tmp = lglerepr (lgl, elit0);
  assert (tmp == repr);
  tmp = lglerepr (lgl, elit1);
  assert (tmp == repr);
}

static int lgltarjan (LGL * lgl) {
  int * dfsimap, * mindfsimap, idx, oidx, sign, lit, blit, tag, other;
  int dfsi, mindfsi, ulit, uother, tmp, repr, res, sgn;
  const int * p, * w, * eow;
  Stk stk, component;
  AVar * av;
  HTS * hts;
  lglstart (lgl, &lgl->stats.tms.trj);
  dfsi = 0;
  NEW (dfsimap, 2*lgl->nvars);
  NEW (mindfsimap, 2*lgl->nvars);
  NEW (lgl->repr, lgl->nvars);
  CLR (stk); CLR (component);
  res = 1;
  for (idx = 2; idx < lgl->nvars; idx++) {
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      ulit = lglulit (lit);
      tmp = dfsimap[ulit];
      if (tmp) continue;
      lglpushstk (lgl, &stk, lit);
      while (!lglmtstk (&stk)) {
	lit = lglpopstk (&stk);
	if (lit) {
	  ulit = lglulit (lit);
	  if (dfsimap[ulit]) continue;
	  dfsimap[ulit] = mindfsimap[ulit] = ++dfsi;
	  lglpushstk (lgl, &component, lit);
	  assert (dfsi > 0);
	  lglpushstk (lgl, &stk, lit);
	  lglpushstk (lgl, &stk, 0);
	  hts = lglhts (lgl, -lit);
	  if (!hts->offset) continue;
	  assert (hts->count > 0);
	  w = lglhts2wchs (lgl, hts);
	  eow = w + hts->count;
	  for (p = w; p < eow; p++) {
	    blit = *p;
	    tag = blit & MASKCS;
	    if (tag != BINCS) { p++; continue; }
	    other = blit >> RMSHFT;
	    uother = lglulit (other);
	    tmp = dfsimap[uother];
	    if (tmp) continue;
	    lglpushstk (lgl, &stk, other);
	  }
	} else {
	  assert (!lglmtstk (&stk));
	  lit = lglpopstk (&stk);
	  ulit = lglulit (lit);
	  mindfsi = dfsimap[ulit];
	  assert (mindfsi);
	  hts = lglhts (lgl, -lit);
	  w = lglhts2wchs (lgl, hts);
	  eow = w + hts->count;
	  for (p = w; p < eow; p++) {
	    blit = *p;
	    tag = blit & MASKCS;
	    if (tag != BINCS) { p++; continue; }
	    other = blit >> RMSHFT;
	    uother = lglulit (other);
	    tmp = mindfsimap[uother];
	    if (tmp >= mindfsi) continue;
	    mindfsi = tmp;
	  }
	  if (mindfsi == dfsimap[ulit]) {
	    repr = lit;
	    for (p = component.top - 1; (other = *p) != lit; p--)
	      if (abs (other) < abs (repr)) 
		repr = other;
	    while ((other = lglpopstk (&component)) != lit) {
	      mindfsimap[lglulit (other)] = INT_MAX;
	      if (other == repr) continue;
	      if (other == -repr) {
		LOG (1, "empty clause since repr[%d] = %d", repr, other);
#ifndef NLGLPICOSAT
		lglpicosatchkunsat (lgl);
#endif
		lgl->mt = 1; res = 0; goto DONE;
	      }
	      sgn = lglsgn (other);
	      oidx = lglabs (other);
	      tmp = lgl->repr[oidx];
	      if (tmp == sgn * repr) continue;
	      LOG (2, "repr[%d] = %d", oidx, sgn * repr);
	      if (tmp) {
		LOG (1, "empty clause since repr[%d] = %d and repr[%d] = %d",
		     oidx, tmp, oidx, sgn * repr);
		lgl->mt = 1; res = 0; goto DONE;
	      } else {
		av = lglavar (lgl, oidx);
		if (av->type == FREEVAR) {
		  av->type = EQUIVAR;
		  lgl->repr[oidx] = sgn * repr;
		  lgl->stats.prgss++;
		  lgl->stats.equivalent.sum++;
		  lgl->stats.equivalent.current++;
		  assert (lgl->stats.equivalent.sum > 0);
		  assert (lgl->stats.equivalent.current > 0);
#ifndef NLGLPICOSAT
		  lglpicosatchkclsarg (lgl, repr, -other, 0);
		  lglpicosatchkclsarg (lgl, -repr, other, 0);
#endif
		  lglemerge (lgl, other, repr);
		} else assert (av->type == FIXEDVAR);
	      }
	    }
	    mindfsimap[lglulit (lit)] = INT_MAX;
	  } else mindfsimap[ulit] = mindfsi;
	}
      }
    }
  }
DONE:
  lglrelstk (lgl, &stk);
  lglrelstk (lgl, &component);
  DEL (mindfsimap, 2*lgl->nvars);
  DEL (dfsimap, 2*lgl->nvars);
  if (!res) DEL (lgl->repr, lgl->nvars);
  lglstop (lgl);
  return res;
}

static int64_t lglsteps (LGL * lgl) {
  int64_t steps = lgl->stats.props.simp;
  steps += lgl->stats.props.search;
  steps += lgl->stats.trd.steps;
  steps += lgl->stats.elm.steps;
  return steps;
}

static int lglterminate (LGL * lgl) {
  int64_t steps;
  int res;
  if (!lgl->term.fun) return 0;
  steps = lglsteps (lgl);
  if (steps < lgl->limits.term.steps) return 0;
  res = lgl->term.fun (lgl->term.state);
  if (!res) lgl->limits.term.steps = steps + 100000;
  return  res;
}

static int lglsync (LGL * lgl) {
  int * units, * eou, * p, elit, erepr, ilit, res, count = 0;
  void (*produce)(void*, int);
  int64_t steps;
  Val val;
  assert (!lgl->simplifying || !lgl->level);
  if (!lgl->consume.fun) return 1;
  steps = lglsteps (lgl);
  if (steps < lgl->limits.sync.steps) return 1;
  lgl->limits.sync.steps = steps + 100000;
  lgl->consume.fun (lgl->consume.state, &units, &eou);
  if (units == eou) return 1;
  produce = lgl->produce.fun;
  lgl->produce.fun = 0;
  for (p = units; !lgl->mt && p < eou; p++) {
    elit = *p;
    erepr = lglerepr (lgl, elit);
    ilit = lglpeek (&lgl->e2i, lglabs (erepr));
    assert (!(ilit & 1));
    ilit /= 2;
    if (!ilit) continue;
    if (erepr < 0) ilit = -ilit;
    if (ilit == 1) continue;
    if (ilit == -1) val = -1;
    else {
      assert (lglabs (ilit) > 1);
      val = lglval (lgl, ilit);
      if (val && lglevel (lgl, ilit)) val = 0;
    }
    if (val == 1) continue;
    if (val == -1) {
      LOG (1, "mismatching synchronized external unit %d", elit);
      if (lgl->level > 0) lglbacktrack (lgl, 0);
      lgl->mt = 1;
    } else if (!lglisfree (lgl, ilit)) continue;
    else {
      assert (!val);
      if (lgl->level > 0) lglbacktrack (lgl, 0);
      lglunit (lgl, ilit);
      LOG (2, "importing internal unit %d external %d",
	   lgl->tid, ilit, elit);
      count++;
    }
  }
  lgl->produce.fun = produce;
  if (lgl->consumed.fun) lgl->consumed.fun (lgl->consumed.state, count);
  if (lgl->mt) return 0;
  if (count)
    LOG (1, "imported %d units", lgl->tid, count);
  if (!count) return 1;
  assert (!lgl->level);
  if (lgl->distilling) { assert (!lgl->propred); lgl->propred = 1; }
  res = lglbcp (lgl);
  if (lgl->distilling) { assert (lgl->propred); lgl->propred = 0; }
  if(!res && !lgl->mt) lgl->mt = 1;
  return res;
}

static int lglprobe (LGL * lgl) {
  int idx, sign, lit, blit, tag, root, units, lifted, ok, old, orig, first;
  int nprobes, nvars, probed, fixed, connected, changed;
  const int * w, * eow, * p;
  Stk probes, lift, saved;
  unsigned pos, delta;
  int64_t limit;
  HTS * hts;
  Val val;
  if (!lgl->nvars) return 1;
  if (!lgl->opts.probe.val) return 1;
  lglstart (lgl, &lgl->stats.tms.prb);
  assert (!lgl->simp);
  lgl->simp = 1;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  lgl->stats.prb.count++;
  assert (lgl->measureagility);
  lgl->measureagility = 0;
  assert (!lgl->probing);
  lgl->probing = 1;
#ifndef NLGLPICOSAT
  assert (lgl->picosat.chk);
  lgl->picosat.chk = 0;
#endif
  CLR (lift); CLR (probes); CLR (saved);
  for (idx = 2; idx < lgl->nvars; idx++) {
    connected = 0;
    for (sign = -1; sign <= 1; sign +=2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->count) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == BINCS) break;
	p++;
      }
      if (p < eow) connected++;
    }
    if (connected < 2) continue;
    LOG (1, "new probe %d", idx);
    lglpushstk (lgl, &probes, idx);
  }
  nprobes = lglcntstk (&probes);
  nvars = lgl->nvars - 1;
  LOG (1, "found %d probes out of %d variables %.1f%%", 
       nprobes, nvars, lglpcnt (nprobes, nvars));
  lifted = units = 0;
  probed = 0;
  orig = lglcntstk (&lgl->trail);
  if (!nprobes) goto DONE;
  pos = lglrand (lgl) % nprobes;
  delta = lglrand (lgl) % nprobes;
  if (!delta) delta++;
  while (lglgcd (delta, nprobes) > 1)
    if (++delta == nprobes) delta = 1;
  LOG (1, "probing start %u delta %u mod %u", pos, delta, nprobes);
  if (lgl->stats.prb.count == 1) limit = lgl->opts.prbmaxeff.val/10;
  else limit = (lgl->opts.prbreleff.val*lgl->stats.visits.search)/1000;
  if (limit < lgl->opts.prbmineff.val) limit = lgl->opts.prbmineff.val;
  if (limit > lgl->opts.prbmaxeff.val) limit = lgl->opts.prbmaxeff.val;
  LOG (1, "probing with up to %lld propagations", (long long) limit);
  limit += lgl->stats.visits.simp;
  changed = first = 0;
  for (;;) {
    if (lgl->stats.visits.simp >= limit) break;
    if (lglterminate (lgl)) break;
    assert (pos < (unsigned) nprobes);
    root = probes.start[pos];
    if (root == first) {
       if (changed) changed = 0; else break;
    }
    if (!first) first = root;
    pos += delta;
    if (pos >= nprobes) pos -= nprobes;
    if (lglval (lgl, root)) continue;
    lgl->stats.prb.probed++;
    lglclnstk (&lift);
    LOG (2, "next probe %d positive phase", root);
    assert (!lgl->level);
    lglassume (lgl, root);
    probed++;
    old = lglcntstk (&lgl->trail);
    ok = lglbcp (lgl);
    if (ok) {
      lglclnstk (&saved);
      for (p = lgl->trail.start + old; p < lgl->trail.top; p++)
	lglpushstk (lgl, &saved, *p);
    }
    lglbacktrack (lgl, 0);
    if (!ok) { 
      LOG (1, "failed literal %d", root);
      lglpushstk (lgl, &lift, -root);
      goto MERGE;
    }
    LOG (2, "next probe %d negative phase", -root);
    assert (!lgl->level);
    lglassume (lgl, -root);
    ok = lglbcp (lgl);
    if (ok) {
      for (p = saved.start; p < saved.top; p++) {
	lit = *p;
	val = lglval (lgl, lit);
	if (val <= 0) continue;
	lifted++;
	lgl->stats.prb.lifted++;
	lglpushstk (lgl, &lift, lit);
	LOG (2, "lifted %d", lit);
      }
    }
    lglbacktrack (lgl, 0);
    if (!ok) {
      LOG (1, "failed literal %d", -root);
      lglpushstk (lgl, &lift, root);
    }
MERGE:
    while (!lglmtstk (&lift)) {
      lit = lglpopstk (&lift);
      val = lglval (lgl, lit);
      if (val > 0) continue;
      if (val < 0) goto EMPTY;
      lglunit (lgl, lit);
      changed = 1;
      units++;
      lgl->stats.prb.failed++;
      if (lglbcp (lgl) && lglsync (lgl)) continue;
EMPTY:
      LOG (1, "empty clause after propagating lifted and failed literals");
      lgl->mt = 1;
      goto DONE;
    }
  }
DONE:
  LOG (1, "probed %d out of %d probes %.1f%%", 
       probed, nprobes, lglpcnt (probed, nprobes));
  lglrelstk (lgl, &lift);
  lglrelstk (lgl, &probes);
  lglrelstk (lgl, &saved);
  fixed = lglcntstk (&lgl->trail) - orig;
  LOG (1, "found %d units %.1f%% lifted %d through probing",
       units, lglpcnt (units, probed), lifted);
  lgl->measureagility = 1;
  assert (lgl->probing);
  lgl->probing = 0;
#ifndef NLGLPICOSAT
  assert (!lgl->picosat.chk);
  lgl->picosat.chk = 1;
#endif
  lgl->limits.prb.cinc += lgl->opts.prbcintinc.val;
  lgl->limits.prb.vinc += lgl->opts.prbvintinc.val;
  lgl->limits.prb.visits = lgl->stats.visits.search + lgl->limits.prb.vinc;
  lgl->limits.prb.confs = lgl->stats.confs + lgl->limits.prb.cinc;
  lgl->limits.prb.prgss = lgl->stats.prgss;
  assert (lgl->simp);
  lgl->simp = 0;
  lglrep (lgl, 2, 'p');
  lglstop (lgl);
  return !lgl->mt;
}

static int lglsmall (LGL * lgl) {
  int maxirrlidx = lglcntstk (&lgl->irr);
  if (maxirrlidx > MAXIRRLIDX) return 0;
  return  1;
}

static void lgldense (LGL * lgl) {
  int lit, lidx, count, idx, other, other2, blit, sign, tag, red;
  const int * start, * top, * c, * p, * eow;
  int * q, * w;
  EVar * ev;
  HTS * hts;
  assert (!lgl->dense);
  assert (!lgl->evars);
  assert (lglsmall (lgl));
  assert (lglmtstk (&lgl->esched));
  assert (!lgl->elm.pivot);
  count = 0;
  other2 = 0;
  NEW (lgl->evars, lgl->nvars);
  for (idx = 2; idx < lgl->nvars; idx++) {
    ev = lgl->evars + idx;
    ev->pos = -1;
  }
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->count) continue;
      q = w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	red = blit & REDCS;
	if (!red && tag == LRGCS) continue;
	*q++ = blit;
	if (tag == TRNCS || tag == LRGCS) *q++ = *p;
	if (red) continue;
	other = blit >> RMSHFT;
	if (lglabs (other) < idx) continue;
	if (tag == TRNCS) {
	  other2 = *p;
	  if (lglabs (other2) < idx) continue;
	}
	lglincocc (lgl, lit), count++;
	lglincocc (lgl, other), count++;
	if (tag == BINCS) continue;
	assert (tag == TRNCS);
	lglincocc (lgl, other2), count++;
      }
      lglshrinkhts (lgl, hts, q - w);
    }
  if (count)
    LOG (1, "counted %d occurrences in small irredundant clauses", count);
  count = 0;
  start = lgl->irr.start;
  top = lgl->irr.top;
  for (c = start; c < top; c = p + 1) {
    p = c;
    if (*c == REMOVED) continue;
    lidx = c - start;
    assert (lidx < MAXIRRLIDX);
    blit = (lidx << RMSHFT) | IRRCS;
    for (; (lit = *p); p++) {
      hts = lglhts (lgl, lit);
      lglpushwch (lgl, hts, blit);
      lglincocc (lgl, lit), count++;
    }
  }
  if (count)
    LOG (1, "counted %d occurrences in large irredundant clauses", count);
  count = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    ev = lglevar (lgl, idx);
    if (ev->pos >= 0) continue;
    assert (!ev->score && !ev->occ[0] && !ev->occ[1]);
    lglesched (lgl, idx);
    count++;
  }
  if (count) LOG (1, "scheduled %d zombies", count);
  lgl->dense = 1;
}

static void lglsparse (LGL * lgl) {
  int idx, sign, lit, count, blit, tag;
  int * w, *p, * eow, * q;
  HTS * hts;
  assert (lgl->dense);
  count = 0;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->count) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = q = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == IRRCS) { count++; continue; }
	*q++ = blit;
	if (tag == BINCS) continue;
	assert (tag == LRGCS || tag == TRNCS);
	*q++ = *++p;
      }
      assert (hts->count - (p - q) == q - w);
      lglshrinkhts (lgl, hts, q - w);
    }
  DEL (lgl->evars, lgl->nvars);
  lglrelstk (lgl, &lgl->esched);
  LOG (1, "removed %d full irredundant occurrences", count);
  lgl->dense = 0;
}

static int lglm2i (LGL * lgl, int mlit) {
  int res, midx = lglabs (mlit);
  assert (0 < midx);
  res = lglpeek (&lgl->elm.m2i, midx);
  if (mlit < 0) res = -res;
  return res;
}

static int lgli2m (LGL * lgl, int ilit) {
  AVar * av = lglavar (lgl, ilit);
  int res = av->mark;
  if (!res) {
    res = lglcntstk (&lgl->seen) + 1;
    av->mark = res;
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm.lsigs) - 2);
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm.noccs) - 2);
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm.mark) - 2);
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm.occs) - 2);
    assert (lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm.m2i) - 1);
    lglpushstk (lgl, &lgl->seen, lglabs (ilit));
    lglpushstk (lgl, &lgl->elm.lsigs, 0);
    lglpushstk (lgl, &lgl->elm.lsigs, 0);
    lglpushstk (lgl, &lgl->elm.noccs, 0);
    lglpushstk (lgl, &lgl->elm.noccs, 0);
    lglpushstk (lgl, &lgl->elm.mark, 0);
    lglpushstk (lgl, &lgl->elm.mark, 0);
    lglpushstk (lgl, &lgl->elm.occs, 0);
    lglpushstk (lgl, &lgl->elm.occs, 0);
    lglpushstk (lgl, &lgl->elm.m2i, lglabs (ilit));
    LOG (4, "mapped internal variable %d to marked variable %d", 
         lglabs (ilit), res);
  }
  if (ilit < 0) res = -res;
  return res;
}

static unsigned lglsig (int lit) {
  unsigned ulit = lglulit (lit), res;
  assert (ulit >= 2);
  ulit -= 2;
  res = (1u << (ulit & 31));
  return res;
}

static void lgladdecl (LGL * lgl, const int * c) {
  int ilit, mlit, umlit, size = 0, lidx, next, prev;
  unsigned csig = 0;
  const int * p;
  Val val;
  LOGCLS (3, c, "copying irredundant clause");
  lgl->stats.elm.steps++;
  lgl->stats.elm.copies++;
  size = 0;
  for (p = c; (ilit = *p); p++) {
    val = lglval (lgl, ilit);
    assert (val <= 0);
    if (val < 0) continue;
    size++;
    if (lglabs (ilit) == lgl->elm.pivot) continue;
    mlit = lgli2m (lgl, ilit);
    assert (lglabs (mlit) != 1);
    csig |= lglsig (mlit);
  }
  assert (size >= 1);
  lidx = next = lglcntstk (&lgl->elm.lits);
  assert (next > 0);
  for (p = c; (ilit = *p); p++) {
    val = lglval (lgl, ilit);
    if (val < 0) continue;
    mlit = lgli2m (lgl, ilit);
    lglpushstk (lgl, &lgl->elm.lits, mlit);
    umlit = lglulit (mlit);
    prev = lglpeek (&lgl->elm.occs, umlit);
    lglpushstk (lgl, &lgl->elm.next, prev);
    lglpoke (&lgl->elm.occs, umlit, next++);
    lglpushstk (lgl, &lgl->elm.csigs, csig);
    lglpushstk (lgl, &lgl->elm.sizes, size);
    lgl->elm.noccs.start[umlit]++;
    lgl->elm.lsigs.start[umlit] |= csig;
  }
  lglpushstk (lgl, &lgl->elm.lits, 0);
  lglpushstk (lgl, &lgl->elm.next, 0);
  lglpushstk (lgl, &lgl->elm.csigs, 0);
  lglpushstk (lgl, &lgl->elm.sizes, 0);
  lgl->elm.necls++;
  LOGCLS (4, lgl->elm.lits.start + lidx, "copied and mapped clause");
#ifndef NDEBUG
  LOGMCLS (4, lgl->elm.lits.start + lidx, "copied and remapped clause");
  {
    int i, j = 0;
    for (i = 0; c[i]; i++) {
      Val val = lglval (lgl, c[i]);
      assert (val <= 0);
      if (val < 0) continue;
      assert (c[i] == lglm2i (lgl, lglpeek (&lgl->elm.lits, lidx + j++)));
    }
  }
#endif
}

static int lglecls (LGL * lgl, int lit) {
  int blit, tag, red, other, lidx, count;
  const int * p, * w, * eow, * c;
  int d[4];
  HTS * hts;
  LOG (3, "copying irredundant clauses with %d", lit);
  count = 0;
  hts = lglhts (lgl, lit);
  if (!hts->count) return 0;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    red = blit & REDCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (red) continue;
    if (tag == BINCS || tag == TRNCS) {
      d[0] = lit;
      other = blit >> RMSHFT;
      d[1] = other;
      if (tag == TRNCS) d[2] = *p, d[3] = 0;
      else d[2] = 0;
      c = d;
    } else {
      assert (tag == IRRCS);
      lidx = (tag == IRRCS) ? (blit >> RMSHFT) : *p;
      c = lglidx2lits (lgl, 0, lidx);
    }
    lgladdecl (lgl, c);
    count++;
  }
  return count;
}

static void lglrstecls (LGL * lgl)  {
  assert (lgl->elm.pivot);
  lglclnstk (&lgl->elm.lits);
  lglclnstk (&lgl->elm.next);
  lglclnstk (&lgl->elm.csigs);
  lglclnstk (&lgl->elm.lsigs);
  lglclnstk (&lgl->elm.sizes);
  lglclnstk (&lgl->elm.occs);
  lglclnstk (&lgl->elm.noccs);
  lglclnstk (&lgl->elm.mark);
  lglclnstk (&lgl->elm.m2i);
  lglpopnunmarkstk (lgl, &lgl->seen);
  lgl->elm.pivot = 0;
}

static void lglrelecls (LGL * lgl)  {
  lglrelstk (lgl, &lgl->elm.lits);
  lglrelstk (lgl, &lgl->elm.next);
  lglrelstk (lgl, &lgl->elm.csigs);
  lglrelstk (lgl, &lgl->elm.lsigs);
  lglrelstk (lgl, &lgl->elm.sizes);
  lglrelstk (lgl, &lgl->elm.occs);
  lglrelstk (lgl, &lgl->elm.noccs);
  lglrelstk (lgl, &lgl->elm.mark);
  lglrelstk (lgl, &lgl->elm.m2i);
  lglrelstk (lgl, &lgl->clv);
}

static int lgl2manyoccs4elm (LGL * lgl, int lit) {
  return lglhts (lgl, lit)->count > lgl->opts.elmocclim.val;
}

static int lglchkoccs4elmlit (LGL * lgl, int lit) {
  int blit, tag, red, other, other2, lidx;
  const int * p, * w, * eow, * c, * l;
  HTS * hts;
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    red = blit & REDCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (red) continue;
    if (tag == BINCS || tag == TRNCS) {
      other = blit >> RMSHFT;
      if (lgl2manyoccs4elm (lgl, other)) return 0;
      if (tag == TRNCS) {
	other2 = *p;
	if (lgl2manyoccs4elm (lgl, other2)) return 0;
      }
    } else {
      assert (tag == IRRCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, 0, lidx);
      for (l = c; (other = *l); l++)
	if (lgl2manyoccs4elm (lgl, other)) return 0;
    }
  }
  return 1;
}

static int lglchkoccs4elm (LGL * lgl, int idx) {
  if (lgl2manyoccs4elm (lgl, idx)) return 0;
  if (lgl2manyoccs4elm (lgl, -idx)) return 0;
  if (!lglchkoccs4elmlit (lgl, idx)) return 0;
  return lglchkoccs4elmlit (lgl, -idx);
}

static void lglinitecls (LGL * lgl, int idx) {
  int clauses;
  assert (!lgl->elm.pivot);
  assert (idx >= 2);
  assert (lglmtstk (&lgl->elm.lits));
  assert (lglmtstk (&lgl->elm.next));
  assert (lglmtstk (&lgl->elm.csigs));
  assert (lglmtstk (&lgl->elm.lsigs));
  assert (lglmtstk (&lgl->elm.sizes));
  assert (lglmtstk (&lgl->elm.occs));
  assert (lglmtstk (&lgl->elm.noccs));
  assert (lglmtstk (&lgl->elm.m2i));
  assert (lglmtstk (&lgl->seen));
  lgl->elm.pivot = idx;
  lglpushstk (lgl, &lgl->elm.mark, 0);
  lglpushstk (lgl, &lgl->elm.mark, 0);
  lglpushstk (lgl, &lgl->elm.occs, 0);
  lglpushstk (lgl, &lgl->elm.occs, 0);
  lglpushstk (lgl, &lgl->elm.noccs, 0);
  lglpushstk (lgl, &lgl->elm.noccs, 0);
  lglpushstk (lgl, &lgl->elm.lsigs, 0);
  lglpushstk (lgl, &lgl->elm.lsigs, 0);
  lglpushstk (lgl, &lgl->elm.m2i, 0);
  (void) lgli2m (lgl, idx);
  lglpushstk (lgl, &lgl->elm.lits, 0);
  lglpushstk (lgl, &lgl->elm.next, 0);
  lglpushstk (lgl, &lgl->elm.csigs, 0);
  lglpushstk (lgl, &lgl->elm.sizes, 0);
  lgl->elm.necls = 0;
  clauses = lglecls (lgl, idx);
  lgl->elm.negcls = lgl->elm.necls;
  lgl->elm.neglidx = lglcntstk (&lgl->elm.lits);
  clauses += lglecls (lgl, -idx);
  LOG (2, "found %d variables in %d clauses with %d or %d",
       lglcntstk (&lgl->seen), clauses, idx, -idx);
  assert (lgl->elm.pivot);
}

static void lglelrmcls (LGL * lgl, int lit, int * c, int clidx) {
  int lidx, i, other, ulit, * lits, * csigs, blit, tag, red, other2, size;
  int * p, * eow, * w, count;
  HTS * hts;
  lits = lgl->elm.lits.start;
  csigs = lgl->elm.csigs.start;
  assert (lits < c && c < lgl->elm.lits.top - 1);
  lidx = c - lits;
  for (i = lidx; (other = lits[i]); i++) {
    assert (other != REMOVED);
    lits[i] = REMOVED;
    csigs[i] = 0;
    ulit = lglulit (other);
    assert (ulit < lglcntstk (&lgl->elm.noccs));
    assert (lgl->elm.noccs.start[ulit] > 0);
    lgl->elm.noccs.start[ulit] -= 1;
  }
  size = lglpeek (&lgl->elm.sizes, lidx);
  hts = lglhts (lgl, lit);
  assert (hts->count > 0 && hts->count >= clidx);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  blit = tag = count = 0;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    red = blit & REDCS;
    if (red) continue;
    if (count == clidx) break;
    count++;
  }
  assert (count == clidx);
  assert (blit && tag);
  assert (p < eow);
  if (tag == BINCS) {
    assert (size >= 2);
    other = blit >> RMSHFT;
    lglrmbcls (lgl, lit, other, 0);
  } else if (tag == TRNCS) {
    other = blit >> RMSHFT;
    other2 = *p;
    lglrmtcls (lgl, lit, other, other2, 0);
  } else {
    assert (tag == IRRCS || tag == LRGCS);
    lidx = (tag == IRRCS) ? (blit >> RMSHFT) : *p;
#ifndef NDEBUG
    {
      int * q, * d = lglidx2lits (lgl, 0, lidx);
      for (q = d; *q; q++)
	;
      assert (q - d >= size);
    }
#endif
    lglrmlcls (lgl, lidx, 0);
  }
}

static int lglbacksub (LGL * lgl, int * c, int str) {
  int * start = lgl->elm.lits.start, * p, * q, marked = 0, res, * d;
  int lit, ulit, occ, next, osize, other, uolit, size, plit, phase, clidx;
  unsigned ocsig, lsig, csig = 0;
#ifndef NLGLOG
  const char * mode = str ? "strengthening" : "subsumption";
#endif
  LOGMCLS (3, c, "backward %s check for clause", mode);
  LOGCLS (3, c, "backward %s check for mapped clause", mode);
  phase = (c - start) >= lgl->elm.neglidx;
  for (p = c; (lit = *p); p++)
    if (lglabs (lit) != 1)
      csig |= lglsig (lit);
  size = p - c;
  assert (csig == lglpeek (&lgl->elm.csigs, c - start));
  assert (size == lglpeek (&lgl->elm.sizes, c - start));
  res = 0;

  if (str) phase = !phase;
  lit = phase ? -1 : 1;

  ulit = lglulit (lit);
  occ = lglpeek (&lgl->elm.noccs, ulit);
  if (!str && occ <= 1) return 0;
  if (str && !occ) return 0;
  lsig = lglpeek (&lgl->elm.lsigs, ulit);
  if ((csig & ~lsig)) return 0;
  for (next = lglpeek (&lgl->elm.occs, ulit);
       !res && next;
       next = lglpeek (&lgl->elm.next, next)) {
      if (next == p - start) continue;
      if (phase != (next >= lgl->elm.neglidx)) continue;
      plit = lglpeek (&lgl->elm.lits, next);
      if (plit == REMOVED) continue;
      assert (plit == lit);
      osize = lglpeek (&lgl->elm.sizes, next);
      if (osize > size) continue;
      ocsig = lglpeek (&lgl->elm.csigs, next);
      assert (ocsig);
      if ((ocsig & ~csig)) continue;
      if (!marked) {
	for (q = c; (other = *q); q++) {
	  if (str && lglabs (other) == 1) other = -other;
	  uolit = lglulit (other);
	  assert (!lglpeek (&lgl->elm.mark, uolit));
	  lglpoke (&lgl->elm.mark, uolit, 1);
	}
	marked = 1;
      }
      d = lgl->elm.lits.start + next;
      if (c <= d && d < c + size) continue;
      lgl->stats.elm.steps++;
      if (str) lgl->stats.elm.strchks++; else lgl->stats.elm.subchks++;
      while (d[-1]) d--;
      assert (c != d);
      LOGMCLS (3, d, "backward %s check with clause", mode);
      res = 1;
      for (q = d; res && (other = *q); q++) {
	uolit = lglulit (other);
	res = lglpeek (&lgl->elm.mark, uolit);
      }
      if (!res || !str || osize < size) continue;
      LOGMCLS (2, d, "strengthening by double self-subsuming resolution");
      assert ((c - start) < lgl->elm.neglidx);
      assert ((d - start) >= lgl->elm.neglidx);
      assert (phase);
      clidx = 0;
      q = lgl->elm.lits.start + lgl->elm.neglidx;
      while (q < d) {
	other = *q++;
	if (other == REMOVED) { while (*q++) ; continue; }
	if (!other) clidx++;
      }
      lgl->stats.elm.str++;
      LOGMCLS (2, d,
	"strengthened and subsumed original irredundant clause");
      LOGCLS (3, d, "strengthened and subsumed mapped irredundant clause");
      lglelrmcls (lgl, -lgl->elm.pivot, d, clidx);
  }
  if (marked) {
    for (p = c; (lit = *p); p++) {
      if (str && lglabs (lit) == 1) lit = -lit;
      ulit = lglulit (lit);
      assert (lglpeek (&lgl->elm.mark, ulit));
      lglpoke (&lgl->elm.mark, ulit, 0);
    }
  }
  return res;
}

static void lglelmsub (LGL * lgl) {
  int clidx, count, subsumed, pivot, * c;
  count = clidx = subsumed = 0;
  pivot = lgl->elm.pivot;
  for (c = lgl->elm.lits.start + 1; 
       c < lgl->elm.lits.top && 
         lgl->limits.elm.steps > lgl->stats.elm.steps;
       c++) {
    if (count++ == lgl->elm.negcls) clidx = 0, pivot = -pivot;
    if (lglbacksub (lgl, c, 0)) {
      subsumed++;
      lgl->stats.elm.sub++;
      LOGMCLS (2, c, "subsumed original irredundant clause");
      LOGCLS (3, c, "subsumed mapped irredundant clause");
      lglelrmcls (lgl, pivot, c, clidx);
    } else clidx++;
    while (*c) c++;
  }
  LOG (2, "subsumed %d clauses containing %d or %d", 
       subsumed, lgl->elm.pivot, -lgl->elm.pivot);
}

static int lglelmstr (LGL * lgl) {
  int clidx, count, strengthened, pivot, * c, * p, mlit, ilit, res, found;
  int size;
  count = clidx = strengthened = 0;
  pivot = lgl->elm.pivot;
  res = 0;
  LOG (3, "strengthening with pivot %d", pivot);
  for (c = lgl->elm.lits.start + 1; 
       c < lgl->elm.lits.top && 
         lgl->limits.elm.steps > lgl->stats.elm.steps;
       c++) {
    if (count++ == lgl->elm.negcls) {
      clidx = 0, pivot = -pivot;
      LOG (3, "strengthening with pivot %d", pivot);
    }
    if (*c == REMOVED) {
      while (*c) { assert (*c == REMOVED); c++; }
      continue;
    }
    if (lglbacksub (lgl, c, 1)) {
      strengthened++;
      lgl->stats.elm.str++;
      LOGMCLS (2, c, "strengthening original irredundant clause");
      LOGCLS (3, c, "strengthening mapped irredundant clause");
      assert (lglmtstk (&lgl->clause));
      found = 0;
      size = 0;
      for (p = c; (mlit = *p); p++) {
	ilit = lglm2i (lgl, *p);
	if (ilit == pivot) { found = 1; continue; }
	assert (!lglval (lgl, ilit));
	lglpushstk (lgl, &lgl->clause, ilit);
	size++;
      }
      assert (found);
      lglpushstk (lgl, &lgl->clause, 0);
      LOGCLS (2, lgl->clause.start, "static strengthened irredundant clause");
#ifndef NLGLPICOSAT
      lglpicosatchkclsaux (lgl, lgl->clause.start);
#endif
      lglelrmcls (lgl, pivot, c, clidx);
      lgladdcls (lgl, 0, 0);
      lglclnstk (&lgl->clause);
      if (size == 1) { res = 1; break; }
    } else clidx++;
    while (*c) c++;
  }
  LOG (2, "strengthened %d clauses containing %d or %d", 
       strengthened, lgl->elm.pivot, -lgl->elm.pivot);
  return res;
}

static void lglflushlit (LGL * lgl, int lit) {
  int blit, tag, red, other, other2, lidx, count;
  const int * p, * w, * eow;
  int * c, * q;
  HTS * hts;
  assert (lgl->dense);
  hts = lglhts (lgl, lit);
  if (!hts->count) return;
  LOG (2, "flushing positive occurrences of %d", lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  count = 0;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    red = blit & REDCS;
    other = blit >> RMSHFT;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (!red && tag == LRGCS) continue;
    if (!red) assert (lgl->stats.irr > 0), lgl->stats.irr--;
    if (tag == BINCS) {
      lglrmbwch (lgl, other, lit, red);
      if (red) assert (lgl->stats.red.bin > 0), lgl->stats.red.bin--;
      else lgldecocc (lgl, other);
      LOG (2, "flushed %s binary clause %d %d", lglred2str (red), lit, other);
      count++;
    } else if (tag == TRNCS) {
      other2 = *p;
      lglrmtwch (lgl, other2, lit, other, red);
      lglrmtwch (lgl, other, lit, other2, red);
      if (red) assert (lgl->stats.red.trn > 0), lgl->stats.red.trn--;
      else lgldecocc (lgl, other), lgldecocc (lgl, other2);
      LOG (2, "flushed %s ternary clause %d %d %d",
           lglred2str (red), lit, other, other2);
      count++;
    } else {
      assert (tag == IRRCS || tag == LRGCS);
      lidx = (tag == IRRCS) ? other : *p;
      c = lglidx2lits (lgl, red, lidx);
      assert (!red || (lidx & GLUEMASK) != MAXGLUE);
      if (red) {
	if (c[0] != lit) lglrmlwch (lgl, c[0], red, lidx);
	if (c[1] != lit) lglrmlwch (lgl, c[1], red, lidx);
      }
      for (q = c; (other = *q); q++) {
	*q = REMOVED;
	if (red) continue;
	lgldecocc (lgl, other);
	if (other == lit) continue;
	lglrmlocc (lgl, other, lidx);
      }
      *q = REMOVED;
      count++;
    }
  }
  lglshrinkhts (lgl, hts, 0);
  LOG (2, "flushed %d clauses in which %d occurs", count, lit);
}

static int lglflush (LGL * lgl) {
  int next = lgl->next, lit, count;
  if (lgl->mt) return 0;
  assert (lgl->dense);
  if (next == lglcntstk (&lgl->trail)) return 1;
  if (!lglbcp (lgl)) { lgl->mt = 1; return 0; }
  if (!lglsync (lgl)) { assert (lgl->mt); return 0; }
  count = 0;
  while  (next < lglcntstk (&lgl->trail)) {
    lit = lglpeek (&lgl->trail, next++);
    lglflushlit (lgl, lit);
    count++;
  }
  LOG (2, "flushed %d literals", count);
  assert (!lgl->mt);
  return 1;
}

static void lglepush (LGL * lgl, int ilit) {
  int elit = ilit ? lglexport (lgl, ilit) : 0;
  lglpushstk (lgl, &lgl->extend, elit);
  LOG (4, "pushing external %d internal %d", elit, ilit);
}

static void lglelmfrelit (LGL * lgl, int mpivot,
			  int * sop, int * eop, int * son, int * eon) {
  int ipivot = mpivot * lgl->elm.pivot, clidx, ilit, tmp, cover, maxcover;
  int * c, * d, * e, * p, * q, lit, nontrivial, idx, sgn, clen, reslen;
  EVar * ev;
  assert (mpivot == 1 || mpivot == -1);
  assert (ipivot);
  LOG (3,
       "blocked clause elimination and forced resolution of clauses with %d",
        ipivot);
  clidx = 0;
  ev = lglevar (lgl, ipivot);
  cover = lglpeek (&lgl->elm.noccs, lglulit (-mpivot));
  for (c = sop; c < eop; c = p + 1) {
    if (*c == REMOVED) { for (p = c + 1; *p; p++) ; continue; }
    maxcover = 0;
    for (p = c; (lit = *p); p++) {
      if (lit == mpivot) continue;
      assert (lit != -mpivot);
      maxcover += lglpeek (&lgl->elm.noccs, lglulit (-lit));
    }
    if (maxcover < cover - 1) { clidx++; continue; }
    for (p = c; (lit = *p); p++) {
      if (lit == mpivot) continue;
      assert (lit != -mpivot);
      idx = lglabs (lit);
      assert (!lglpeek (&lgl->elm.mark, idx));
      sgn = lglsgn (lit);
      lglpoke (&lgl->elm.mark, idx, sgn);
    }
    nontrivial = 0;
    clen = p - c;
    e = 0;
    for (d = son; !nontrivial && d < eon; d = q + 1) {
      if (*d == REMOVED) { for (q = d + 1; *q; q++) ; continue; }
      lgl->stats.elm.steps++;
      lgl->stats.elm.resolutions++;
      LOGMCLS (3, c, "trying forced resolution 1st antecedent");
      LOGMCLS (3, d, "trying forced resolution 2nd antecedent");
      reslen = clen;
      for (q = d; (lit = *q); q++) {
	if (lit == -mpivot) continue;
        assert (lit != mpivot);
	idx = lglabs (lit), sgn = lglsgn (lit);
	tmp = lglpeek (&lgl->elm.mark, idx);
	if (tmp == -sgn) break;
	if (tmp != sgn) reslen++;
      }
      if (lit) {
	while (*++q) ;
        LOG (3, "trying forced resolution ends with trivial resolvent");
      } else {
	assert (!e);
	LOG (3, "non trivial resolvent in blocked clause elimination");
	nontrivial = INT_MAX;
      }
    }
    for (p = c; (lit = *p); p++) {
      if (lit == mpivot) continue;
      assert (lit != -mpivot);
      idx = lglabs (lit);
      assert (lglpeek (&lgl->elm.mark, idx) == lglsgn (lit));
      lglpoke (&lgl->elm.mark, idx, 0);
    }
    if (!nontrivial) {
      assert (maxcover >= cover);
      lgl->stats.elm.blkd++;
      LOGMCLS (2, c, "blocked on %d clause", ipivot);
      lglepush (lgl, 0);
      lglepush (lgl, ipivot);
      for (p = c; (lit = *p); p++) {
	if (lit == mpivot) continue;
	assert (lit != -mpivot);
	ilit = lglm2i (lgl, lit);
	lglepush (lgl, ilit);
      }
      lglelrmcls (lgl, ipivot, c, clidx);
      if (!nontrivial) continue;
      LOGCLS (2, lgl->clause.start, "forced resolvent");
      lgladdcls (lgl, 0, 0);
      lglclnstk (&lgl->clause);
    } else clidx++;
    if (lgl->limits.elm.steps <= lgl->stats.elm.steps) {
      LOG (2, "maximum number of steps in elmination exhausted");
      return;
    }
  }
}

static void lglelmfre (LGL * lgl) {
  int * sop, * eop, * son, * eon;
  assert (lgl->elm.pivot);
  sop = lgl->elm.lits.start + 1;
  eop = son = lgl->elm.lits.start + lgl->elm.neglidx;
  eon = lgl->elm.lits.top;
  lglelmfrelit (lgl, 1, sop, eop, son, eon);
  lglelmfrelit (lgl, -1, son, eon, sop, eop);
}

static int lglforcedvechk (LGL * lgl, int idx) {
  EVar * v = lglevar (lgl, idx);
  if (v->occ[0] <= 1) return 1;
  if (v->occ[1] <= 1) return 1;
  if (v->occ[0] > 2) return 0;
  return v->occ[1] <= 2;
}

static void lgleliminated (LGL * lgl, int pivot) {
  AVar * av;
  av = lglavar (lgl, pivot);
  assert (av->type == FREEVAR);
  av->type = ELIMVAR;
  lgl->stats.elm.elmd++;
  assert (lgl->stats.elm.elmd > 0);
  lglflushlit (lgl, pivot);
  lglflushlit (lgl, -pivot);
  LOG (2, "eliminated %d", pivot);
}

static void lglepusheliminated (LGL * lgl, int idx) {
  const int * p, * w, * eow, * c, * l;
  int pivot, blit, tag, red, lit;
  EVar * ev = lglevar (lgl, idx);
  HTS * hts0;
  pivot = (ev->occ[0] <= ev->occ[1]) ? idx : -idx;
  hts0 = lglhts (lgl, pivot);
  w = lglhts2wchs (lgl, hts0);
  eow = w + hts0->count;
  LOG (3, "keeping clauses with %d for extending assignment", pivot);
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    red = blit & REDCS;
    if (red) continue;
    lglepush (lgl, 0);
    lglepush (lgl, pivot);
    if (tag == BINCS || tag == TRNCS) {
      lglepush (lgl, blit >> RMSHFT);
      if (tag == TRNCS)
	lglepush (lgl, *p);
    } else {
      assert (tag == IRRCS);
      c = lglidx2lits (lgl, 0, blit >> RMSHFT);
      for (l = c; (lit = *l); l++)
	if (lit != pivot)
	  lglepush (lgl, lit);
    }
  }
  lglepush (lgl, 0);
  lglepush (lgl, -pivot);
  lgleliminated (lgl, pivot);
}

static void lglforcedve (LGL * lgl, int idx) {
  const int * p0, * p1, * w0, * w1, * eow0, * eow1, * c0, * c1, * l0, * l1;
  int pivot, dummy0[4], dummy1[4], blit0, blit1, tag0, tag1, red0, red1;
  long deltairr, deltawchs;
  HTS * hts0, * hts1;
  int * wchs, * irr;
  int lit0, lit1;
  int unit = 0;
  EVar * ev;
  Val val;
  lglchkflushed (lgl);
  assert (!lgl->level);
  assert (lglforcedvechk (lgl, idx));
  if (lgl->elm.pivot) lglrstecls (lgl);
  LOG (2, "forced variable elimination of %d", idx);
  ev = lglevar (lgl, idx);
  pivot = (ev->occ[0] <= ev->occ[1]) ? idx : -idx;
  hts0 = lglhts (lgl, pivot);
  hts1 = lglhts (lgl, -pivot);
  w0 = lglhts2wchs (lgl, hts0);
  w1 = lglhts2wchs (lgl, hts1);
  eow0 = w0 + hts0->count;
  eow1 = w1 + hts1->count;
  dummy0[0] = pivot;
  dummy1[0] = -pivot;
  for (p0 = w0; !unit && p0 < eow0; p0++) {
    blit0 = *p0;
    tag0 = blit0 & MASKCS;
    if (tag0 == TRNCS || tag0 == LRGCS) p0++;
    red0 = blit0 & REDCS;
    if (red0) continue;
    if (tag0 == BINCS) {
      dummy0[1] = blit0 >> RMSHFT;
      dummy0[2] = 0;
      c0 = dummy0;
    } else if (tag0 == TRNCS) {
      dummy0[1] = blit0 >> RMSHFT;
      dummy0[2] = *p0;
      dummy0[3] = 0;
      c0 = dummy0;
    } else {
      assert (tag0 == IRRCS);
      c0 = lglidx2lits (lgl, 0, blit0 >> RMSHFT);
    }
    for (l0 = c0; (lit0 = *l0); l0++) {
      assert (!lglmarked (lgl, lit0));
      lglmark (lgl, lit0);
    }
    for (p1 = w1; !unit && p1 < eow1; p1++) {
      blit1 = *p1;
      tag1 = blit1 & MASKCS;
      if (tag1 == TRNCS || tag1 == LRGCS) p1++;
      red1 = blit1 & REDCS;
      if (red1) continue;
      if (tag1 == BINCS) {
	dummy1[1] = blit1 >> RMSHFT;
	dummy1[2] = 0;
	c1 = dummy1;
      } else if (tag1 == TRNCS) {
	dummy1[1] = blit1 >> RMSHFT;
	dummy1[2] = *p1;
	dummy1[3] = 0;
	c1 = dummy1;
      } else {
	assert (tag1 == IRRCS);
	c1 = lglidx2lits (lgl, 0, blit1 >> RMSHFT);
      }
      lgl->stats.elm.steps++;
      lgl->stats.elm.resolutions++;
      for (l1 = c1; (lit1 = *l1); l1++)
	if (lit1 != -pivot && lglmarked (lgl, lit1) < 0) break;
      if (lit1) continue;
      LOGCLS (3, c0, "resolving forced variable elimination 1st antecedent");
      LOGCLS (3, c1, "resolving forced variable elimination 2nd antecedent");
      assert (lglmtstk (&lgl->clause));
      for (l0 = c0; (lit0 = *l0); l0++) {
	if (lit0 == pivot) continue;
	val = lglval (lgl, lit0);
	assert (val <= 0);
	if (val < 0) continue;
	lglpushstk (lgl, &lgl->clause, lit0);
      }
      for (l1 = c1; (lit1 = *l1); l1++) {
	if (lit1 == -pivot) continue;
	val = lglval (lgl, lit1);
	assert (val <= 0);
	if (val < 0) continue;
	if (lglmarked (lgl, lit1)) continue;
	lglpushstk (lgl, &lgl->clause, lit1);
      }
      lglpushstk (lgl, &lgl->clause, 0);
      LOGCLS (3, lgl->clause.start, "forced variable elimination resolvent");
      if (lglcntstk (&lgl->clause) >= 3) {
	wchs = lgl->wchs.start;
	irr = lgl->irr.start;
	lgladdcls (lgl, 0, 0);
	deltawchs = lgl->wchs.start - wchs;
	if (deltawchs) {
	  p0 += deltawchs, w0 += deltawchs, eow0 += deltawchs;
	  p1 += deltawchs, w1 += deltawchs, eow1 += deltawchs;
	}
	deltairr = lgl->irr.start - irr;
	if (deltairr && tag0 == IRRCS) c0 += deltairr;
	lglclnstk (&lgl->clause);
      } else {
	assert (lglcntstk (&lgl->clause) == 2);
	lglunit (lgl, lgl->clause.start[0]);
	lglclnstk (&lgl->clause);
	unit = 1;
      }
    }
    for (l0 = c0; (lit0 = *l0); l0++) {
      assert (lglmarked (lgl, lit0));
      lglunmark (lgl, lit0);
    }
  }
  if (unit) return;
  lglepusheliminated (lgl, pivot);
  lgl->stats.elm.forced++;
}

static int lgltryforcedve (LGL * lgl, int idx) {
  if (lgl->limits.elm.steps <= lgl->stats.elm.steps) return 1;
  if (!lglforcedvechk (lgl, idx)) return 0;
  lglforcedve (lgl, idx);
  return 1;
}

static int lgltrylargeve (LGL * lgl) {
  const int * c, * d, * sop, * eop, * son, * eon, * p, * q, * start, * end;
  int lit, idx, sgn, tmp, ip, mp, ilit, npocc, nnocc, limit, count, occ, i;
  int clen, reslen, maxreslen;
  EVar * ev;
  ip = lgl->elm.pivot;
  assert (ip);
  sop = lgl->elm.lits.start + 1;
  eop = son = lgl->elm.lits.start + lgl->elm.neglidx;
  eon = lgl->elm.lits.top;
  npocc = lglpeek (&lgl->elm.noccs, lglulit (1));
  nnocc = lglpeek (&lgl->elm.noccs, lglulit (-1));
  limit = npocc + nnocc;
  count = 0;
  for (i = 0; i <= 1; i++) {
    start = i ? son : sop;
    end = i ? eon : eop;
    for (c = start; c < end; c++) {
      if (*c == REMOVED) { while (*c) c++; continue; }
      while ((lit = *c)) {
	ilit = lglm2i (lgl, lit);
	ev = lglevar (lgl, ilit);
	sgn = lglsgn (ilit);
	occ = ev->occ[sgn];
	if (occ > lgl->opts.elmocclim.val) {
	  LOG (3, "number of occurrences of %d larger than limit", ilit);
	  return 0;
	}
	c++;
      }
      count++;
    }
  }
  assert (count == limit);
  LOG (3, "trying clause distribution for %d with limit %d", ip, limit);
  maxreslen = 0;
  for (c = sop; c < eop && limit >= 0; c = p + 1) {
    if (*c == REMOVED) { for (p = c + 1; *p; p++) ; continue; }
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = lglabs (lit);
      assert (!lglpeek (&lgl->elm.mark, idx));
      sgn = lglsgn (lit);
      lglpoke (&lgl->elm.mark, idx, sgn);
    }
    clen = p - c;
    for (d = son; limit >= 0 && d < eon; d = q + 1) {
      if (*d == REMOVED) { for (q = d + 1; *q; q++) ; continue; }
      lgl->stats.elm.steps++;
      lgl->stats.elm.resolutions++;
      LOGMCLS (3, c, "trying resolution 1st antecedent");
      LOGMCLS (3, d, "trying resolution 2nd antecedent");
      reslen = clen;
      for (q = d; (lit = *q); q++) {
	if (lit == -1) continue;
	assert (lit != 1);
	idx = lglabs (lit), sgn = lglsgn (lit);
	tmp = lglpeek (&lgl->elm.mark, idx);
	if (tmp == -sgn) break;
	if (tmp == sgn) continue;
	reslen++;
      }
      if (lit) {
	while (*++q) ;
        LOG (3, "trying resolution ends with trivial resolvent");
      } else {
        limit--;
        LOG (3, 
	     "trying resolution with non trivial resolvent remaining %d",
	     limit);
	if (reslen > maxreslen) maxreslen = reslen;
      }
      assert (!*q);
    }
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = lglabs (lit);
      assert (lglpeek (&lgl->elm.mark, idx) == lglsgn (lit));
      lglpoke (&lgl->elm.mark, idx, 0);
    }
    if (lgl->limits.elm.steps <= lgl->stats.elm.steps) {
      LOG (2, "maximum number of steps in elmination exhausted");
      return 0;
    }
    if (maxreslen > lgl->opts.elmreslim.val) {
      LOG (3, "maximum resolvent size in elimination reached");
      return 0;
    }
  }
  assert (lglm2i (lgl, 1) == ip);
  if (limit < 0) {
    LOG (3, "resolving away %d would increase number of clauses", ip);
    return 0;
  }
  if (limit) LOG (2, "resolving away %d removes %d clauses", ip, limit);
  else LOG (2, "resolving away %d does not change number of clauses", ip);
  LOG (2, "variable elimination of %d", idx);
  lglflushlit (lgl, ip);
  lglflushlit (lgl, -ip);
  if (npocc < nnocc) start = sop, end = eop, mp = 1;
  else start = son, end = eon, ip = -ip, mp = -1;
  LOG (3, "will save clauses with %d for extending assignment", ip);
  for (c = start; c < end; c = p + 1) {
    if (*c == REMOVED) { for (p = c + 1; *p; p++) ; continue; }
    lglepush (lgl, 0);
    lglepush (lgl, ip);
    for (p = c; (lit = *p); p++)  {
      if (lit == mp) continue;
      assert (lit != -mp);
      ilit = lglm2i (lgl, lit);
      lglepush (lgl, ilit);
    }
  }
  lglepush (lgl, 0);
  lglepush (lgl, -ip);
  for (c = sop; c < eop; c = p + 1) {
    if (*c == REMOVED) { for (p = c + 1; *p; p++) ; continue; }
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = lglabs (lit);
      assert (!lglpeek (&lgl->elm.mark, idx));
      sgn = lglsgn (lit);
      lglpoke (&lgl->elm.mark, idx, sgn);
    }
    for (d = son; limit >= 0 && d < eon; d = q + 1) {
      if (*d == REMOVED) { for (q = d + 1; *q; q++) ; continue; }
      lgl->stats.elm.steps++;
      lgl->stats.elm.resolutions++;
      assert (lglmtstk (&lgl->clause));
      for (q = d; (lit = *q); q++) {
	if (lit == -1) continue;
	assert (lit != 1);
	idx = lglabs (lit), sgn = lglsgn (lit);
	tmp = lglpeek (&lgl->elm.mark, idx);
	if (tmp == sgn) continue;
	if (tmp == -sgn) break;
	ilit = lglm2i (lgl, lit);
	lglpushstk (lgl, &lgl->clause, ilit);
      }
      if (lit) {
	while (*++q) ;
      } else {
	LOGMCLS (3, c, "resolving variable elimination 1st antecedent");
	LOGMCLS (3, d, "resolving variable elimination 2nd antecedent");
	for (p = c; (lit = *p); p++) {
	  if (lit == 1) continue;
	  assert (lit != -1);
	  ilit = lglm2i (lgl, lit);
	  lglpushstk (lgl, &lgl->clause, ilit);
	}
	lglpushstk (lgl, &lgl->clause, 0);
	LOGCLS (3, lgl->clause.start, "variable elimination resolvent");
	lgladdcls (lgl, 0, 0);
      }
      lglclnstk (&lgl->clause);
      assert (!*q);
    }
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = lglabs (lit);
      assert (lglpeek (&lgl->elm.mark, idx) == lglsgn (lit));
      lglpoke (&lgl->elm.mark, idx, 0);
    }
  }
  lgleliminated (lgl, lgl->elm.pivot);
  lgl->stats.elm.large++;
  return 1;
}

static void lglelimlitaux (LGL * lgl, int idx) {
  lglelmsub (lgl);
  if (lglelmstr (lgl)) return;
  if (lgltryforcedve (lgl, idx)) return;
  lglelmfre (lgl);
  if (lgltryforcedve (lgl, idx)) return;
  lgltrylargeve (lgl);
}

static int lgls2m (LGL * lgl, int ilit) {
  AVar * av = lglavar (lgl, ilit);
  int res = av->mark;
  if (!res) {
    res = lglcntstk (&lgl->seen) + 1;
    if (res > lgl->opts.smallvevars.val + 1) return 0;
    av->mark = res;
    assert (lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm.m2i) - 1);
    lglpushstk (lgl, &lgl->seen, lglabs (ilit));
    lglpushstk (lgl, &lgl->elm.m2i, lglabs (ilit));
    LOG (4, "mapped internal variable %d to marked variable %d", 
         lglabs (ilit), res);
  }
  if (ilit < 0) res = -res;
  return res;
}

static void lglvar2funaux (int v, Fun res, int negate) {
  uint64_t tmp;
  int i, j, p;
  assert (0 <= v && v < FUNVAR);
  if (v < 6) {
    tmp = lglbasevar2funtab[v];
    if (negate) tmp = ~tmp;
    for (i = 0; i < FUNQUADS; i++)
      res[i] = tmp;
  } else {
    tmp = negate ? ~0ull : 0ull;
    p = 1 << (v - 6);
    j = 0;
    for (i = 0; i < FUNQUADS; i++) {
      res[i] = tmp;
      if (++j < p) continue;
      tmp = ~tmp;
      j = 0;
    }
  }
}

static void lglvar2fun (int v, Fun res) {
  lglvar2funaux (v, res, 0);
}

static void lglnegvar2fun (int v, Fun res) {
  lglvar2funaux (v, res, 1);
}

static void lglfuncpy (Fun dst, const Fun src) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    dst[i] = src[i];
}

static void lglfalsefun (Fun res) { 
  int i;
  for (i = 0; i < FUNQUADS; i++)
    res[i] = 0ll;
}

static void lgltruefun (Fun res) { 
  int i;
  for (i = 0; i < FUNQUADS; i++)
    res[i] = ~0ll;
}

static int lglisfalsefun (const Fun f) { 
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (f[i] != 0ll) return 0;
  return 1;
}

static int lglistruefun (const Fun f) { 
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (f[i] != ~0ll) return 0;
  return 1;
}

static void lglorfun (Fun a, const Fun b) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] |= b[i];
}

static void lglornegfun (Fun a, const Fun b) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] |= ~b[i];
}

static void lglor3fun (Fun a, const Fun b, const Fun c) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] = b[i] | c[i];
}

static void lglor3negfun (Fun a, const Fun b, const Fun c) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] = b[i] | ~c[i];
}

static void lglandornegfun (Fun a, const Fun b, const Fun c) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] &= b[i] | ~c[i];
}

static void lglandfun (Fun a, const Fun b) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] &= b[i];
}

static void lgland3fun (Fun a, const Fun b, const Fun c) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] = b[i] & c[i];
}

static void lgland3negfun (Fun a, const Fun b, const Fun c) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    a[i] = b[i] & ~c[i];
}

static void lglsrfun (Fun a, int shift) {
  uint64_t rest, tmp;
  int i, j, q, b, l;
  assert (0 <= shift);
  b = shift & 63;
  q = shift >> 6;
  j = 0;
  i = j + q;
  assert (i >= 0);
  l = 64 - b;
  while (j < FUNQUADS) {
    if (i < FUNQUADS) {
      tmp = a[i] >> b;
      rest = (b && i+1 < FUNQUADS) ? (a[i+1] << l) : 0ull;
      a[j] = rest | tmp;
    } else a[j] = 0ull;
    i++, j++;
  }
}

static void lglslfun (Fun a, int shift) {
  uint64_t rest, tmp;
  int i, j, q, b, l;
  assert (0 <= shift);
  b = shift & 63;
  q = shift >> 6;
  j = FUNQUADS - 1;
  i = j - q;
  l = 64 - b;
  while (j >= 0) {
    if (i >= 0) {
      tmp = a[i] << b;
      rest = (b && i > 0) ? (a[i-1] >> l) : 0ll;
      a[j] = rest | tmp;
    } else a[j] = 0ll;
    i--, j--;
  }
}

static void lgls2fun (int mlit, Fun res) {
  int midx = lglabs (mlit), sidx = midx - 2;
  assert (0 <= sidx && sidx < FUNVAR);
  if (mlit < 0) lglnegvar2fun (sidx, res);
  else lglvar2fun (sidx, res);
}

static int lglinitsmallve (LGL * lgl, int lit, Fun res) {
  int blit, tag, red, other, other2, lidx, mlit;
  const int * p, * w, * eow, * c, * q;
  Fun cls, tmp;
  HTS * hts;
  Val val;
  assert (!lglval (lgl, lit));
  LOG (3, "initializing small variable eliminiation for %d", lit);
  mlit = lgls2m (lgl, lit);
  assert (lglabs (mlit) == 1);
  hts = lglhts (lgl, lit);
  lgltruefun (res);
  if (!hts->count) goto DONE;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    red = blit & REDCS;
    if (red) continue;
    lglfalsefun (cls);
    if (tag == BINCS || tag == TRNCS) {
      other = blit >> RMSHFT;
      val = lglval (lgl, other);
      assert (val <= 0);
      if (!val) {
	mlit = lgls2m (lgl, other);
	if (!mlit) return 0;
	lgls2fun (mlit, tmp);
	lglorfun (cls, tmp);
      }
      if (tag == TRNCS) {
        other2 = *p;
	val = lglval (lgl, other2);
	assert (val <= 0);
	if (!val) {
	  mlit = lgls2m (lgl, other2);
	  if (!mlit) return 0;
	  lgls2fun (mlit, tmp);
	  lglorfun (cls, tmp);
	}
      }
    } else {
      assert (tag == IRRCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, 0, lidx);
      for (q = c; (other = *q); q++) {
	if (other == lit) continue;
	assert (other != -lit);
	val = lglval (lgl, other);
	assert (val <= 0);
	if (!val) {
	  mlit = lgls2m (lgl, other);
	  if (!mlit) return 0;
	  lgls2fun (mlit, tmp);
	  lglorfun (cls, tmp);
	}
      }
    }
    assert (!lglisfalsefun (cls));
    assert (!lglistruefun (cls));
    lglandfun (res, cls);
    lgl->stats.elm.steps++;
    lgl->stats.elm.copies++;
  }
DONE:
  return 1;
}

static void lglresetsmallve (LGL * lgl) {
  lglclnstk (&lgl->elm.m2i);
  lglclnstk (&lgl->clv);
  lglpopnunmarkstk (lgl, &lgl->seen);
}

static void lglsmallevalcls (unsigned cls, Fun res) {
  Fun tmp;
  int v;
  lglfalsefun (res);
  for (v = 0; v < FUNVAR; v++) {
    if (cls & (1 << (2*v + 1))) {
      lglvar2fun (v, tmp);
      lglornegfun (res, tmp);
    } else if (cls & (1 << (2*v))) {
      lglvar2fun (v, tmp);
      lglorfun (res, tmp);
    }
  }
}

static Cnf lglpos2cnf (int pos) { assert (pos >=0 ); return pos; }
static Cnf lglsize2cnf (int s) { assert (s >=0 ); return ((Cnf)s) << 32; }
static int lglcnf2pos (Cnf cnf) { return cnf & 0xfffffll; }
static int lglcnf2size (Cnf cnf) { return cnf >> 32; }

static Cnf lglcnf (int pos, int size) {
  return lglpos2cnf (pos) | lglsize2cnf (size);
}

static void lglsmallevalcnf (LGL * lgl, Cnf cnf, Fun res) {
  Fun tmp;
  int i, n, p, cls;
  p = lglcnf2pos (cnf);
  n = lglcnf2size (cnf);
  lgltruefun (res);
  for (i = 0; i < n; i++) {
    cls = lglpeek (&lgl->clv, p + i);
    lglsmallevalcls (cls, tmp);
    lglandfun (res, tmp);
  }
}

static void lglnegcofactorfun (const Fun f, int v, Fun res) {
  Fun mask, masked;
  lglvar2fun (v, mask);
  lgland3negfun (masked, f, mask);
  lglfuncpy (res, masked);
  lglslfun (masked, (1 << v));
  lglorfun (res, masked);
}

static void lglposcofactorfun (const Fun f, int v, Fun res) {
  Fun mask, masked;
  lglvar2fun (v, mask);
  lgland3fun (masked, f, mask);
  lglfuncpy (res, masked);
  lglsrfun (masked, (1 << v));
  lglorfun (res, masked);
}

static int lgleqfun (const Fun a, const Fun b) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (a[i] != b[i]) return 0;
  return 1;
}

static int lglsmalltopvar (const Fun f, int min) {
  Fun p, n;
  int v;
  for (v = min; v < FUNVAR; v++) {
    lglposcofactorfun (f, v, p);
    lglnegcofactorfun (f, v, n);
    if (!lgleqfun (p, n)) return v;
  }
  return v;
}

static Cnf lglsmalladdlit2cnf (LGL * lgl, Cnf cnf, int lit) {
  int p, m, q, n, i, cls;
  Cnf res;
  p = lglcnf2pos (cnf);
  m = lglcnf2size (cnf);
  q = lglcntstk (&lgl->clv);
  for (i = 0; i < m; i++) {
    cls = lglpeek (&lgl->clv, p + i);
    assert (!(cls & lit));
    cls |= lit;
    lglpushstk (lgl, &lgl->clv, cls);
  }
  n = lglcntstk (&lgl->clv) - q;
  res = lglcnf (q, n);
  return res;
}

#ifndef NDEBUG
static int lglefun (const Fun a, const Fun b) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (a[i] & ~b[i]) return 0;
  return 1;
}
#endif

static Cnf lglsmallipos (LGL * lgl, const Fun U, const Fun L, int min) {
  Fun U0, U1, L0, L1, Unew, ftmp;
  Cnf c0, c1, cstar, ctmp, res;
  int x, y, z;
  assert (lglefun (L, U));
  if (lglistruefun (U)) return TRUECNF;
  if (lglisfalsefun (L)) return FALSECNF;
  assert (min < lglcntstk (&lgl->elm.m2i));
  y = lglsmalltopvar (U, min);
  z = lglsmalltopvar (L, min);
  x = (y < z) ? y : z;
  assert (x < FUNVAR);
  lglnegcofactorfun (U, x, U0); lglposcofactorfun (U, x, U1);
  lglnegcofactorfun (L, x, L0); lglposcofactorfun (L, x, L1);
  lglor3negfun (ftmp, U0, L1);
  c0 = lglsmallipos (lgl, ftmp, L0, min+1);
  lglor3negfun (ftmp, U1, L0);
  c1 = lglsmallipos (lgl, ftmp, L1, min+1);
  lglsmallevalcnf (lgl, c0, ftmp);
  lglor3negfun (Unew, U0, ftmp);
  lglsmallevalcnf (lgl, c1, ftmp);
  lglandornegfun (Unew, U1, ftmp);
  lglor3fun (ftmp, L0, L1);
  cstar = lglsmallipos (lgl, Unew, ftmp, min+1);
  assert (cstar != FALSECNF);
  ctmp = lglsmalladdlit2cnf (lgl, c1, (1 << (2*x + 1)));
  res = lglcnf2pos (ctmp);
  ctmp = lglsmalladdlit2cnf (lgl, c0, (1 << (2*x)));
  if (res == TRUECNF) res = lglcnf2pos (ctmp);
  ctmp = lglsmalladdlit2cnf (lgl, cstar, 0);
  if (res == TRUECNF) res = lglcnf2pos (ctmp);
  res |= lglsize2cnf (lglcntstk (&lgl->clv) - res);
  return res;
}

static void lglsmallve (LGL * lgl, Cnf cnf) {
  int * soc = lgl->clv.start + lglcnf2pos (cnf);
  int * eoc = soc + lglcnf2size (cnf);
  int * p, cls, v, lit, trivial;
  Val val;
  for (p = soc; !lgl->mt && p < eoc; p++) {
    cls = *p;
    assert (lglmtstk (&lgl->clause));
    trivial = 0;
    for (v = 0; v < FUNVAR; v++) {
      if (cls & (1 << (2*v + 1))) lit = -lglm2i (lgl, v+2);
      else if (cls & (1 << (2*v))) lit = lglm2i (lgl, v+2);
      else continue;
      val = lglval (lgl, lit);
      if (val < 0) continue;
      if (val > 0) trivial = 1;
      lglpushstk (lgl, &lgl->clause, lit);
    }
    if (!trivial) {
      lgl->stats.elm.steps++;
      lgl->stats.elm.resolutions++;
      lglpushstk (lgl, &lgl->clause, 0);
      LOGCLS (3, lgl->clause.start, "small elimination resolvent");
#ifndef NLGLPICOSAT
      lglpicosatchkcls (lgl);
#endif
      lgladdcls (lgl, 0, 0);
    }
    lglclnstk (&lgl->clause);
  }
}

static int lglsmallisunitcls (LGL * lgl, int cls) {
  int fidx, fsign, flit, mlit, ilit;
  ilit = 0;
  for (fidx = 0; fidx < FUNVAR; fidx++)
    for (fsign = 0; fsign <= 1; fsign++) {
      flit = 1<<(2*fidx + fsign);
      if (!(cls & flit)) continue;
      if (ilit) return 0;
      mlit = (fidx + 2) * (fsign ? -1 : 1);
      ilit = lglm2i (lgl, mlit);
    }
  return ilit;
}

static int lglsmallcnfunits (LGL * lgl, Cnf cnf) {
  int p, m, i, res, cls, ilit;
  p = lglcnf2pos (cnf);
  m = lglcnf2size (cnf);
  res = 0;
  for (i = 0; i < m; i++) {
    cls = lglpeek (&lgl->clv, p + i);
    ilit = lglsmallisunitcls (lgl, cls);
    if (!ilit) continue;
    assert (lglval (lgl, ilit) >= 0);
    lglunit (lgl, ilit);
    res++;
  }
  return res;
}

static int lgltrysmallve (LGL * lgl, int idx) {
  int res = 0, new, old, units;
  Fun pos, neg, fun;
  EVar * ev;
  Cnf cnf;
  assert (lglmtstk (&lgl->elm.m2i));
  assert (lglmtstk (&lgl->seen));
  assert (lglmtstk (&lgl->clv));
  lglpushstk (lgl, &lgl->elm.m2i, 0);
  lglpushstk (lgl, &lgl->clv, 0);
  if (lglinitsmallve (lgl, idx, pos) && lglinitsmallve (lgl, -idx, neg)) {
    lglor3fun (fun, pos, neg);
    cnf = lglsmallipos (lgl, fun, fun, 0); 
    new = lglcnf2size (cnf);
    units = lglsmallcnfunits (lgl, cnf);
    assert (units <= new);
    new -= units;
    ev = lglevar (lgl, idx);
    old = ev->occ[0] + ev->occ[1];
    LOG (2, "small elimination of %d replaces "
            "%d old with %d new clauses and %d units",
         idx, old, new, units);
    lgl->stats.elm.small.tried++;
    if (new <= old) {
      LOG (2, "small elimination of %d removes %d clauses", idx, old - new);
      lglsmallve (lgl, cnf);
      lglepusheliminated (lgl, idx);
      lgl->stats.elm.small.elm++;
      res = 1;
    } else {
      LOG (2, "small elimination of %d would add %d clauses", idx, new - old);
      if (units > 0) res = 1;
      else lgl->stats.elm.small.failed++;
    }
  } else LOG (2, "too many variables for small elimination");
  lglresetsmallve (lgl);
  return res;
}

static void lglelimlit (LGL * lgl, int idx) {
  if (!lglisfree (lgl, idx)) return;
  if (!lglchkoccs4elm (lgl, idx)) return;
  LOG (2, "trying to eliminate %d", idx);
  if (lgl->opts.smallve.val && lgltrysmallve (lgl, idx)) return;
  if (lgltryforcedve (lgl, idx)) return;
  lglinitecls (lgl, idx);
  lglelimlitaux (lgl, idx);
  if (lgl->elm.pivot) lglrstecls (lgl);
}

static int lglblockcls (LGL * lgl, int lit) {
  int blit, tag, red, other, other2, lidx, val, count;
  const int * p, * w, * eow, * c, *l;
  HTS * hts;
  hts = lglhts (lgl, lit);
  hts = lglhts (lgl, lit);
  if (!hts->count) return 1;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  count = 0;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    red = blit & REDCS;
    if (red) continue;
    count++;
    lgl->stats.blk.res++;
    if (tag == BINCS || tag == TRNCS) {
      other = blit >> RMSHFT;
      val = lglmarked (lgl, other);
      if (val < 0) continue;
      if (tag == TRNCS) {
	other2 = *p;
	val = lglmarked (lgl, other2);
	if (val < 0) continue;
      }
    } else {
      assert (tag == IRRCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, 0, lidx);
      for (l = c; (other = *l); l++) {
	val = lglmarked (lgl, other);
	if (val < 0) break;
      }
      if (other) continue;
    }
    return 0;
  }
  LOG (3, "resolved %d trivial resolvents on %d", count, lit);
  return 1;
}

static void lglpushnmarkseen (LGL * lgl, int lit) {
  assert (!lglmarked (lgl, lit));
  lglpushstk (lgl, &lgl->seen, lit);
  lglmark (lgl, lit);
}

static int lgl2manyoccs4blk (LGL * lgl, int lit) {
  return lglhts (lgl, lit)->count > lgl->opts.blkocclim.val;
}

static int lglblocklit (LGL * lgl, int lit, Stk * stk) {
  int blit, tag, red, blocked, other, other2, lidx, count;
  int * p, * w, * eow, * c, * l;
  HTS * hts;
  if (lglval (lgl, lit)) return 0;
  hts = lglhts (lgl, lit);
  if (!hts->count) return 0;
  if (lgl2manyoccs4blk (lgl, lit)) return 0;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  count = 0;
  assert (lglmtstk (stk+2) && lglmtstk (stk+3) && lglmtstk (stk+4));
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    blocked = 0;
    if (tag == TRNCS || tag == LRGCS) p++;
    red = blit & REDCS;
    if (red) continue;
    assert (lglmtstk (&lgl->seen));
    if (tag == BINCS || tag == TRNCS) {
      other = blit >> RMSHFT;
      if (lgl2manyoccs4blk (lgl, other)) continue;
      lglpushnmarkseen (lgl, other);
      if (tag == TRNCS) {
	other2 = *p;
        if (lgl2manyoccs4blk (lgl, other2)) goto CONTINUE;
	lglpushnmarkseen (lgl, other2);
      }
    } else {
      assert (tag == IRRCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, 0, lidx);
      for (l = c; (other = *l); l++) {
	if (other == lit) continue;
        if (lgl2manyoccs4blk (lgl, other)) goto CONTINUE;
	lglpushnmarkseen (lgl, other);
      }
    }
    blocked = lglblockcls (lgl, -lit);
CONTINUE:
    lglpopnunmarkstk (lgl, &lgl->seen);
    if (!blocked) continue;
    if (tag == BINCS) {
      other = blit >> RMSHFT;
      lglpushstk (lgl, stk+2, other);
    } else if (tag == TRNCS) {
      other = blit >> RMSHFT;
      lglpushstk (lgl, stk+3, other);
      other2 = *p;
      lglpushstk (lgl, stk+3, other2);
    } else {
      assert (tag == IRRCS);
      lidx = blit >> RMSHFT;
      lglpushstk (lgl, stk+4, lidx);
    }
  }
  while (!lglmtstk (stk+2)) {
    count++;
    other = lglpopstk (stk+2);
    LOG (2, "blocked binary clause %d %d on %d", lit, other, lit);
    lglrmbcls (lgl, lit, other, 0);
    lglepush (lgl, 0);
    lglepush (lgl, lit);
    lglepush (lgl, other);
  }
  while (!lglmtstk (stk+3)) {
    count++;
    other2 = lglpopstk (stk+3);
    other = lglpopstk (stk+3);
    LOG (2, "blocked ternary clause %d %d %d on %d", lit, other, other2, lit);
    lglrmtcls (lgl, lit, other, other2, 0);
    lglepush (lgl, 0);
    lglepush (lgl, lit);
    lglepush (lgl, other);
    lglepush (lgl, other2);
  }
  while (!lglmtstk (stk+4)) {
    lidx = lglpopstk (stk+4);
    count++;
    c = lglidx2lits (lgl, 0, lidx);
    LOGCLS (2, c, "blocked on %d large clause", lit);
    lglepush (lgl, 0);
    lglepush (lgl, lit);
    for (l = c; (other = *l); l++)
      if (other != lit) lglepush (lgl, other);
    lglrmlcls (lgl, lidx, 0);
  }
  LOG (2, "found %d blocked clauses with %d", count, lit);
  lgl->stats.blk.elm += count;
  return count;
}

static void lglsignedmarknpushseen (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << ((lit < 0) ? 1 : 2);
  if (av->mark & bit) return;
  av->mark |= bit;
  lglpushstk (lgl, &lgl->seen, lit);
}

static int lglsignedmarked (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << ((lit < 0) ? 1 : 2);
  return av->mark & bit;
}

static int lglhla (LGL * lgl, int start) {
  int next, blit, tag, other, red, lit, tmp;
  const int * p, * w, * eow;
  HTS * hts;
  assert (lglmtstk (&lgl->seen));
  lglsignedmarknpushseen (lgl, start);
  next = 0;
  LOG (3, "starting hidden literal addition from %d", start);
  while (next < lglcntstk (&lgl->seen)) {
    lit = lglpeek (&lgl->seen, next++);
    hts = lglhts (lgl, lit);
    if (!hts->count) continue;
    w = lglhts2wchs (lgl, hts);
    eow = w + hts->count;
    for (p = w; p < eow; p++) {
      lgl->stats.hte.steps++;
      blit = *p;
      tag = blit & MASKCS;
      if (tag == TRNCS || tag == LRGCS) p++;
      if (tag != BINCS) continue;
      red = blit & REDCS;
      if (red) continue;
      other = -(blit >> RMSHFT);
      if (lglsignedmarked (lgl, other)) continue;
      if (lglsignedmarked (lgl, -other)) {
	assert (!lgl->level);
	LOG (1, "failed literal %d in hidden tautology elimination", -start);
	lglunit (lgl, start);
	tmp = lglflush (lgl);
	if (!tmp && !lgl->mt) lgl->mt = 1;
	lgl->stats.hte.failed++;
	return 0;
      } 
      LOG (3, "added hidden literal %d", other);
      lglsignedmarknpushseen (lgl, other);
    }
  }
  return 1;
}

static void lglhte (LGL * lgl) {
  int idx, sign, lit, blit, tag, red, other, other2, lidx, count; 
  const int * p, * w, * eow, * c, * l;
  int first, pos, delta, nlits;
  int64_t limit;
  Stk trn, lrg;
  HTS * hts;
  assert (lgl->dense);
  nlits = 2*(lgl->nvars - 2);
  if (nlits <= 0) return;
  lglstart (lgl, &lgl->stats.tms.hte);
  lgl->stats.hte.count++;
  CLR (trn); CLR (lrg);
  first = count = 0;
  pos = lglrand (lgl) % nlits;
  delta = lglrand (lgl) % nlits;
  if (!delta) delta++;
  while (lglgcd (delta, nlits) > 1)
    if (++delta == nlits) delta = 1;
  LOG (1, "hte start %u delta %u mod %u", pos, delta, nlits);
  if (lgl->stats.hte.count == 1) limit = lgl->opts.htemaxeff.val/10;
  else limit = (lgl->opts.htereleff.val*lgl->stats.visits.search)/1000;
  if (limit < lgl->opts.htemineff.val) limit = lgl->opts.htemineff.val;
  if (limit > lgl->opts.htemaxeff.val) limit = lgl->opts.htemaxeff.val;
  LOG (1, "hte search steps limit %lld", (long long) limit);
  limit += lgl->stats.hte.steps;
  while (!lgl->mt) {
    if (lgl->stats.hte.steps >= limit) break;
    sign = (pos & 1) ? -1 : 1;
    idx = pos/2 + 2;
    assert (2 <= idx && idx < lgl->nvars);
    lit = sign * idx;
    if (lglval (lgl, lit)) goto CONTINUE;
    if (!lglhla (lgl, lit)) goto CONTINUE;
    assert (lglmtstk (&trn));
    assert (lglmtstk (&lrg));
    hts = lglhts (lgl, lit);
    w = lglhts2wchs (lgl, hts);
    eow = w + hts->count;
    for (p = w; p < eow; p++) {
      blit = *p;
      tag = blit & MASKCS;
      if (tag == TRNCS || tag == LRGCS) p++;
      if (tag == BINCS || tag == LRGCS) continue;
      red = blit & REDCS;
      if (red) continue;
      if (tag == TRNCS) {
	other = blit >> RMSHFT;
	other2 = *p;
	if (lglsignedmarked (lgl, -other) ||
	    lglsignedmarked (lgl, -other2)) {
	  lglpushstk (lgl, &trn, other);
	  lglpushstk (lgl, &trn, other2);
	}
      } else {
	assert (tag == IRRCS);
	lidx = blit >> RMSHFT;
	c = lglidx2lits (lgl, 0, lidx);
	for (l = c; (other = *l); l++)
	  if (other != lit && lglsignedmarked (lgl, -other))
	    break;
	if (!other) continue;
	lglpushstk (lgl, &lrg, lidx);
      }
    }
CONTINUE:
    lglpopnunmarkstk (lgl, &lgl->seen);
    while (!lglmtstk (&lrg)) {
      lidx = lglpopstk (&lrg);
      lglrmlcls (lgl, lidx, 0);
      lgl->stats.hte.lrg++;
      count++;
    }
    while (!lglmtstk (&trn)) {
      other2 = lglpopstk (&trn);
      other = lglpopstk (&trn);
      lglrmtcls (lgl, lit, other, other2, 0);
      lgl->stats.hte.trn++;
      count++;
    }
    if (first == lit) break;
    if (!first) first = lit;
    pos += delta;
    if (pos >= nlits) pos -= nlits;
  }
  lglprt (lgl, 2, "removed %d hidden tautological clauses", count);
  lglrelstk (lgl, &trn);
  lglrelstk (lgl, &lrg);
  lglstop (lgl);
  lglrep (lgl, 2, 'h');
}

static void lglblock (LGL * lgl) {
  Stk blocked[5];
  int idx, count;
  int64_t limit;
  assert (lglsmall (lgl));
  assert (lgl->simp);
  assert (lgl->dense);
  assert (lgl->eliminating);
  assert (!lgl->blocking);
  assert (!lgl->elm.pivot);
  assert (!lgl->level);
  lglstart (lgl, &lgl->stats.tms.blk);
  lgl->blocking = 1;
  lgl->stats.blk.count++;
  count = 0;
  memset (blocked, 0, sizeof blocked);
  if (lgl->stats.blk.count == 1) limit = lgl->opts.blkmaxeff.val/10;
  else limit = (lgl->opts.blkreleff.val*lgl->stats.visits.search)/1000;
  if (limit < lgl->opts.blkmineff.val) limit = lgl->opts.blkmineff.val;
  if (limit > lgl->opts.blkmaxeff.val) limit = lgl->opts.blkmaxeff.val;
  LOG (1, "blocking resolution steps limit %lld", (long long) limit);
  limit += lgl->stats.blk.res;
  while (!lglterminate (lgl) &&
         lgl->stats.blk.res < limit &&
         !lglmtstk (&lgl->esched)) {
    if (lgl->opts.verbose.val > 2) {
      printf ("c k clauses %d, schedule %ld, limit %lld                 \r", 
              lgl->stats.irr,
	      (long) lglcntstk (&lgl->esched), 
	      (long long)(limit - lgl->stats.blk.res));
      fflush (stdout);
    }
    idx = lglpopesched (lgl);
    lgl->elm.pivot = idx;
    count += lglblocklit (lgl, idx, blocked);
    count += lglblocklit (lgl, -idx, blocked);
  }
  lglrelstk (lgl, blocked+2);
  lglrelstk (lgl, blocked+3);
  lglrelstk (lgl, blocked+4);
  lgl->elm.pivot = 0;
  LOG (1, "blocked clause elimination of %d clauses", count);
  assert (lgl->blocking);
  lgl->blocking = 0;
  lgleschedall (lgl);
  lglstop (lgl);
  lglrep (lgl, 2, 'k');
  assert (!lgl->mt);
}

static int lglelim (LGL * lgl) {
  int res = 1, idx, elmd, relelmd, oldnvars, skip, oldelmd;
  int64_t limit;
  assert (!lgl->eliminating);
  assert (!lgl->simp);
  lgl->eliminating = 1;
  lgl->simp = 1;
  oldnvars = lgl->nvars;
  skip = (!oldnvars || !lglsmall (lgl));
  if (lgl->limits.elm.skip) lgl->limits.elm.skip--, skip = 1;
  if (skip) { lgl->stats.elm.skipped++; goto DONE; }
  assert (lgl->opts.elim.val);
  lglstart (lgl, &lgl->stats.tms.elm);
  lgl->stats.elm.count++;
  lglgc (lgl);
  if (lgl->nvars <= 2) { lglstop (lgl); goto DONE; }
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  lgldense (lgl);
  if (lgl->opts.hte.val) { lglhte (lgl); res = !lgl->mt; }
  if (res && lgl->opts.block.val) lglblock (lgl);
  if (lgl->stats.elm.count == 1) limit = lgl->opts.elmaxeff.val/10;
  else limit = (lgl->opts.elmreleff.val*lgl->stats.visits.search)/1000;
  if (limit < lgl->opts.elmineff.val) limit = lgl->opts.elmineff.val;
  if (limit > lgl->opts.elmaxeff.val) limit = lgl->opts.elmaxeff.val;
  LOG (1, "elimination up to %lld subsumption checks / resolutions", limit);
  lgl->limits.elm.steps = lgl->stats.elm.steps + limit;
  oldelmd = lgl->stats.elm.elmd;
  while (res && 
	 !lglterminate (lgl) &&
         !lglmtstk (&lgl->esched) && 
	 lgl->limits.elm.steps > lgl->stats.elm.steps) {
    if (lgl->opts.verbose.val > 2) {
      printf ("c e %d variables, schedule %ld, limit %lld                 \r", 
              lglrem (lgl) - (lgl->stats.elm.elmd - oldelmd),
	      (long) lglcntstk (&lgl->esched), 
	      (long long)(limit - lgl->stats.elm.steps));
      fflush (stdout);
    }
    idx = lglpopesched (lgl);
    lglelimlit (lgl, idx);
    res = lglflush (lgl);
  }
  lglrelecls (lgl);
  lglsparse (lgl);
  if (res) lglgc (lgl); else assert (lgl->mt);
  elmd = oldnvars - lgl->nvars;
  relelmd = (100*elmd) / oldnvars;
  if (res && relelmd < lgl->opts.elmpenlim.val)
    lgl->limits.elm.penalty++;
  else if (lgl->limits.elm.penalty) lgl->limits.elm.penalty = 0;
  lglprt (lgl, 2, "eliminated %d = %d%% variables out of %d penalty %d",
	  elmd, relelmd, oldnvars, lgl->limits.elm.penalty);
  lgl->limits.elm.skip = lgl->limits.elm.penalty;
  lglstop (lgl);
  lglrep (lgl, 2, 'e');
DONE:
  assert (lgl->eliminating);
  assert (lgl->simp);
  lgl->eliminating = 0;
  lgl->simp = 0;
  lgl->limits.elm.vinc += lgl->opts.elmvintinc.val;
  lgl->limits.elm.cinc += lgl->opts.elmcintinc.val;
  lgl->limits.elm.visits = lgl->stats.visits.search + lgl->limits.elm.vinc;
  lgl->limits.elm.confs = lgl->stats.confs + lgl->limits.elm.cinc;
  lgl->limits.elm.prgss = lgl->stats.prgss;
  return res;
}

static int lgldecomp (LGL * lgl) {
  int res = 0;
  if (!lglsmall (lgl)) return 1;
  assert (lgl->opts.decompose.val);
  assert (!lgl->decomposing);
  lglstart (lgl, &lgl->stats.tms.dcp);
  lgl->decomposing = 1;
  lgl->stats.decomps++;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  assert (!lgl->simp);
  lgl->simp = 1;
  if (!lglbcp (lgl)) goto DONE;
  lglgc (lgl);
  if (!lgltarjan (lgl)) goto DONE;
  lgldcpdis (lgl);
  (void) lgldcpcln (lgl);
  lgldcpcon (lgl);
  lgldreschedule (lgl);
  lglmap (lgl);
  if (lgl->mt) goto DONE;
  if (!lglbcp (lgl)) goto DONE;
  lglgc (lgl);
  lglcount (lgl);
  assert (!lgl->mt);
  if (lgl->mt) goto DONE;
  lgldreschedule (lgl);
#ifndef NLGLPICOSAT
  if (!lgl->mt) lglpicosatchkallandrestart (lgl);
#endif
  lgl->limits.dcp.vinc += lgl->opts.dcpvintinc.val;
  lgl->limits.dcp.cinc += lgl->opts.dcpcintinc.val;
  lgl->limits.dcp.visits = lgl->stats.visits.search + lgl->limits.dcp.vinc;
  lgl->limits.dcp.confs = lgl->stats.confs + lgl->limits.dcp.cinc;
  lgl->limits.dcp.prgss = lgl->stats.prgss;
  lglrep (lgl, 2, 'd');
  res = 1;
DONE:
  assert (lgl->decomposing);
  lgl->decomposing = 0;
  assert (lgl->simp);
  lgl->simp = 0;
  lglstop (lgl);
  return res;
}

static void lgldstpull (LGL * lgl, int lit) {
  AVar * av;
  int tag;
  av = lglavar (lgl, lit);
  assert ((lit > 0) == av->wasfalse);
  if (av->mark) return;
  if (!av->level) return;
  av->mark = 1;
  tag = av->rsn[0] & MASKCS;
  if (tag == DECISION) {
    lglpushstk (lgl, &lgl->clause, lit);
    LOG (3, "added %d to learned clause", lit);
  } else {
    lglpushstk (lgl, &lgl->seen, -lit);
    LOG (3, "pulled in distillation literal %d", -lit);
  }
}

static int lgldstanalit (LGL * lgl, int lit) {
  int r0, r1, antecedents, other, next, tag, * p;
  AVar * av;
  assert (lglmtstk (&lgl->seen));
  assert (lglmtstk (&lgl->clause));
  antecedents = 1;
  av = lglavar (lgl, lit);
  r0 = av->rsn[0], r1 = av->rsn[1];
  LOGREASON (2, lit, r0, r1, 
             "starting literal distillation analysis for %d with", lit);
  LOG (3, "added %d to learned clause", lit);
  lglpushstk (lgl, &lgl->clause, lit);
  assert ((lit < 0) == av->wasfalse);
  assert (!av->mark);
  av->mark = 1;
  next = 0;
  for (;;) {
    tag = r0 & MASKCS;
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      lgldstpull (lgl, other);
      if (tag == TRNCS) lgldstpull (lgl, r1);
    } else {
      assert (tag == LRGCS);
      for (p = lglidx2lits (lgl, (r0 & REDCS), r1); (other = *p); p++)
	if (other != lit) lgldstpull (lgl, *p);
    }
    if (next == lglcntstk (&lgl->seen)) break;
    lit = lglpeek (&lgl->seen, next++);
    av = lglavar (lgl, lit);
    assert ((lit < 0) == av->wasfalse);
    r0 = av->rsn[0], r1 = av->rsn[1];
    LOGREASON (2, lit, r0, r1, "literal distillation analysis of");
    antecedents++;
  } 
  lglpopnunmarkstk (lgl, &lgl->seen);
  LOG (2, "literal distillation analysis used %d antecedents", antecedents);
  assert (lglcntstk (&lgl->clause) >= 2);
  return antecedents;
}

static int lgldstanaconf (LGL * lgl) {
  int lit, r0, r1, unit, other, next, tag, * p;
  AVar * av;
  assert (lgl->conf.lit);
  assert (lglmtstk (&lgl->seen));
  assert (lglmtstk (&lgl->clause));
  lit = lgl->conf.lit, r0 = lgl->conf.rsn[0], r1 = lgl->conf.rsn[1];
  LOGREASON (2, lit, r0, r1, 
             "starting conflict distillation analysis for %d with", lit);
  lgldstpull (lgl, lit);
  next = 0;
  for (;;) {
    tag = r0 & MASKCS;
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      lgldstpull (lgl, other);
      if (tag == TRNCS) lgldstpull (lgl, r1);
    } else {
      assert (tag == LRGCS);
      for (p = lglidx2lits (lgl, (r0 & REDCS), r1); (other = *p); p++)
	if (other != lit) lgldstpull (lgl, *p);
    }
    if (next == lglcntstk (&lgl->seen)) break;
    lit = lglpeek (&lgl->seen, next++);
    av = lglavar (lgl, lit);
    assert ((lit < 0) == av->wasfalse);
    r0 = av->rsn[0], r1 = av->rsn[1];
    LOGREASON (2, lit, r0, r1, "conflict distillation analysis of");
  } 
  unit = (lglcntstk (&lgl->clause) == 1) ? lgl->clause.start[0] : 0;
  lglpopnunmarkstk (lgl, &lgl->seen);
#ifndef NLGLOG
  if (unit) 
    LOG (1, "conflict distillilation analysis produced unit %d", unit);
  else LOG (2, 
            "conflict distillilation analysis clause of size %d",
	    lglcntstk (&lgl->clause));
#endif
  lglpopnunmarkstk (lgl, &lgl->clause);
  return unit;
}

static int lgldistill (LGL * lgl) {
  int lidx, i, * clauses, lit, distilled, size, count, nlrg, ntrn, idx,sign;
  int * c, * p, * q, satisfied, res, newsize, antecedents, * start, * trn;
  unsigned first, pos, delta, mod, last;
  int blit, tag, red, other, other2, ok;
  int * w, * eow;
  int64_t limit;
  HTS * hts;
  Val val;
  assert (lgl->opts.distill.val);
  assert (!lgl->distilling);
  assert (!lgl->simp);
  lglstart (lgl, &lgl->stats.tms.dst);
  lgl->distilling = 1;
  lgl->simp = 1;
  res = 1;
  nlrg = 0;
  for (c = lgl->irr.start; c < lgl->irr.top; c = p) {
    p = c;
    lit = *p++;
    assert (lit);
    if (lit == REMOVED) continue;
    assert (p < lgl->irr.top);
    while (*p++) assert (p < lgl->irr.top);
    nlrg++;
  }
  if (nlrg) LOG (1, "distilling %d large clauses", nlrg);
  else LOG (1, "could not find any large irredundant clause to distill");
  ntrn = 0;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->count) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	red = blit & REDCS;
	other = blit >> RMSHFT;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag != TRNCS) continue;
	if (red) continue;
	if (lglabs (other) < idx) continue;
	other2 = *p;
	if (lglabs (other2) < idx) continue;
	ntrn++;
      }
    }
  if (ntrn) LOG (1, "distilling %d ternary clauses", ntrn);
  else LOG (1, "could not find any ternary irredundant clause to distill");
  mod = nlrg + ntrn;
  if (!mod) {
    LOG (1, "there are no irredundant clauses to distill");
    assert (res);
    goto STATS;
  }
  lgl->stats.dst.count++;
  NEW (trn, 3*ntrn*sizeof *trn);
  i = 0;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      if (!hts->count) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	red = blit & REDCS;
	other = blit >> RMSHFT;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag != TRNCS) continue;
	if (red) continue;
	if (lglabs (other) < idx) continue;
	other2 = *p;
	if (lglabs (other2) < idx) continue;
	trn[i++] = lit;
	trn[i++] = other;
	trn[i++] = other2;
      }
    }
  assert (i == 3*ntrn);
  lglchkflushed (lgl);
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  assert (lgl->measureagility && lgl->propred);
  lgl->measureagility = lgl->propred = 0;
  assert (mod);
  pos = lglrand (lgl) % mod;
  delta = lglrand (lgl) % mod;
  if (!delta) delta++;
  while (lglgcd (delta, mod) > 1)
    if (++delta == mod) delta = 1;
  LOG (1, "distilling start %u delta %u mod %u", pos, delta, mod);
  if (lgl->stats.dst.count == 1) limit = lgl->opts.dstmaxeff.val/10;
  else limit = (lgl->opts.dstreleff.val*lgl->stats.visits.search)/1000;
  if (limit < lgl->opts.dstmineff.val) limit = lgl->opts.dstmineff.val;
  if (limit > lgl->opts.dstmaxeff.val) limit = lgl->opts.dstmaxeff.val;
  LOG (1, "distilling with up to %lld propagations", limit);
  limit += lgl->stats.visits.simp;
  NEW (clauses, mod);
  i = 0;
  start = lgl->irr.start;
  for (c = start; c < lgl->irr.top; c = p) {
    p = c;
    lit = *p++;
    assert (lit);
    if (lit == REMOVED) continue;
    assert (p < lgl->irr.top);
    while (*p++) assert (p < lgl->irr.top);
    assert (i < mod);
    clauses[i++] = c - start;
  }
  first = mod;
  distilled = 0;
  assert (i == nlrg);
  count = 0;
  while (lgl->stats.visits.simp < limit && !lglterminate (lgl)) {
    assert (res);
    if (!(res = lglsync (lgl))) goto DONE;
    lglchkflushed (lgl);
    count++;
    assert (count <= mod);
    if (pos < nlrg) {
      lidx = clauses[pos];
      c = start + lidx;
      if (*c == REMOVED) goto CONTINUE;
      LOGCLS (2, c, "distilling large %d clause", lidx);
      satisfied = (lglval (lgl, c[0]) > 0 || lglval (lgl, c[1]) > 0);
      q = c + 2;
      for (p = q; (lit = *p); p++) {
	if (satisfied) continue;
	val = lglval (lgl, lit);
	if (val < 0) continue;
	if (val > 0) { satisfied = lit; continue; }
	*q++ = lit;
      }
      if (!satisfied) assert (!lglval (lgl, c[0]) && !lglval (lgl, c[1]));
      size = q - c;
      *q++ = 0;
      while (q <= p) *q++ = REMOVED;
      if (satisfied || size <= 3) {
	lglrmlwch (lgl, c[0], 0, lidx);
	lglrmlwch (lgl, c[1], 0, lidx);
      }
      if (satisfied) {
	LOGCLS (2, c, "distilled large %d clause already %d satisfied",
		lidx, satisfied);
      } else if (size == 2) {
	LOG (2, "found new binary clause distilling large %d clause", lidx);
	lglwchbin (lgl, c[0], c[1], 0);
	lglwchbin (lgl, c[1], c[0], 0);
      } else if (size == 3) {
	LOG (2, "found new ternary clause distilling large %d clause", lidx);
	lglwchtrn (lgl, c[0], c[1], c[2], 0);
	lglwchtrn (lgl, c[1], c[0], c[2], 0);
	lglwchtrn (lgl, c[2], c[0], c[1], 0);
      } else assert (size > 3);
      if (satisfied) lgldeclscnt (lgl, 0, size);
      if (satisfied || size <= 3) {
	while (p >= c) *p-- = REMOVED;
	goto CONTINUE;
      }
      distilled++;
      assert (lgl->ignlidx < 0);
      lgl->ignlidx = lidx;
      ok = 1;
      val = 0;
      for (p = c; (lit = *p); p++) {
	val = lglval (lgl, lit);
	if (val) break;
	lglassume (lgl, -lit);
	ok = lglbcp (lgl);
	if (!ok) break;
      }
      lgl->ignlidx = -1;
#ifndef NDEBUG
      if (lit) assert (!val != !lgl->conf.lit);
      else assert (!val && !lgl->conf.lit);
#endif
      if (ok || !lit) lglbacktrack (lgl, 0);
      if (!lit) goto CONTINUE;
      lglrmlwch (lgl, c[0], 0, lidx);
      lglrmlwch (lgl, c[1], 0, lidx);
      if (val < 0) {
	assert (ok);
	assert (size >= 4);
	lgl->stats.dst.lits++;
	lgl->stats.prgss++;
	LOGCLS (2, c,
		"removing literal %d in distilled large %d clause",
		lit, lidx);
	for (p = c; (*p != lit); p++)
	  ;
	for (q = p++; (lit = *p); p++)
	  *q++ = lit;
	*q++ = 0;
	*q = REMOVED;
	if (size == 4) goto LRG2TRN; else goto LRG2LRG;
      } else if (val > 0) {
	assert (ok);
	newsize = p - c + 1;
	assert (2 <= newsize && newsize <= size);
	if (newsize == size) goto LRGRED;
	antecedents = lgldstanalit (lgl, lit);
	if (antecedents > 1) {
	  p = q = c;
	  while (q < c + newsize) {
	    lit = *q++;
	    if (lglmarked (lgl, lit)) *p++ = lit;
	  }
	  p--;
	  assert (p - c + 1 <= newsize);
	  newsize = p - c + 1;
	  assert (2 <= newsize && newsize <= size);
	}
	lglpopnunmarkstk (lgl, &lgl->clause);
	if (antecedents == 1) goto LRGRED;
	lgl->stats.dst.clauses++;
	lgl->stats.prgss++;
	LOGCLS (2, c, "shortening distilled large %d clause by %d literals",
		lidx, size - newsize);
	*++p = 0;
	while (*++p) *p = REMOVED;
	*p = REMOVED;
	if (newsize == 2) {
//LRG2BIN:
	  LOG (2, "distilled large %d clause becomes binary clause %d %d",
	       lidx, c[0], c[1]);
#ifndef NLGLPICOSAT
	  lglpicosatchkclsarg (lgl, c[0], c[1], 0);
#endif
	  lglwchbin (lgl, c[0], c[1], 0);
	  lglwchbin (lgl, c[1], c[0], 0);
	  c[0] = c[1] = c[2] = REMOVED;
	  assert (c[3] == REMOVED);
	} else if (newsize == 3) {
LRG2TRN:
	  LOG (2, 
	       "distilled large %d clause becomes ternary clause %d %d %d",
	       lidx, c[0], c[1], c[2]);
#ifndef NLGLPICOSAT
	  lglpicosatchkclsarg (lgl, c[0], c[1], c[2], 0);
#endif
	  lglwchtrn (lgl, c[0], c[1], c[2], 0);
	  lglwchtrn (lgl, c[1], c[0], c[2], 0);
	  lglwchtrn (lgl, c[2], c[0], c[1], 0);
	  c[0] = c[1] = c[2] = c[3] = REMOVED;
	  assert (c[4] == REMOVED);
	} else {
LRG2LRG:
	  (void) lglwchlrg (lgl, c[1], c[0], 0, lidx);
	  (void) lglwchlrg (lgl, c[0], c[1], 0, lidx);
	  LOGCLS (2, c, "distilled %d clause remains large clause", lidx);
#ifndef NLGLPICOSAT
	  lglpicosatchkclsaux (lgl, c);
#endif
	}
      } else if (p == c) {
	assert (!ok);
	assert (lgl->conf.lit);
	lglbacktrack (lgl, 0);
LRGFAILED:
	lgl->stats.dst.failed++;
	LOG (1, "failed literal %d during distilling large %d clause",
	     -lit, lidx);
	lglunit (lgl, lit);
	assert (!lgl->level);
	assert (!lgl->propred);
	lgl->propred = 1;
	res = lglbcp (lgl);
	lgl->propred = 0;
	if (!res) goto DONE;
	goto LRGREM;
      } else {
	assert (!ok);
	assert (lgl->conf.lit);
	lit = lgldstanaconf (lgl);
	lglbacktrack (lgl, 0);
	if (lit) goto LRGFAILED;
LRGRED:
	LOGCLS (2, c, "redundant distilled large %d clause", lidx);
	lgl->stats.dst.red++;
	lgl->stats.prgss++;
LRGREM:
	for (p = c; *p; p++) *p = REMOVED;
	*p = REMOVED;
	lgldeclscnt (lgl, 0, size);
      }
    } else {
      lidx = pos - nlrg;
      assert (0 <= lidx && lidx < ntrn);
      c = trn + 3*lidx;
      if (*c == REMOVED) goto CONTINUE;
      LOG (2, "distilling ternary clause %d %d %d", c[0], c[1], c[2]);
      for (i = 0; i < 3; i++) {
	if (lglval (lgl, c[i]) <= 0) continue;
	LOG (2, "distilled ternary clause %d %d %d already satisfied by %d",
	     c[0], c[1], c[2], c[i]);
	goto TRNREM;
      }
      for (i = 0; i < 3; i++) {
	val = lglval (lgl, c[i]);
	if (!val) continue;
	assert (val < 0);
	if (i != 2) SWAP (int, c[i], c[2]);
	LOG (2, "removing false %d from distilled ternary clause %d %d %d",
	     c[i], c[0], c[1], c[2]);
	goto TRN2BIN;
      }
      distilled++;
      assert (!lgl->igntrn);
      for (i = 0; i < 3; i++) lgl->ignlits[i] = c[i];
      lgl->igntrn = 1;
      ok = 1;
      val = 0;
      lit = 0;
      for (i = 0; i < 3; i++) {
	lit = c[i];
	val = lglval (lgl, lit);
	if (val) break;
	lglassume (lgl, -lit);
	ok = lglbcp (lgl);
	if (!ok) break;
      }
#ifndef NDEBUG
      if (i < 3) assert (!val != !lgl->conf.lit);
      else assert (!val && !lgl->conf.lit);
#endif
      if (val || i == 3) lglbacktrack (lgl, 0);
      assert (lgl->igntrn);
      lgl->igntrn = 0;
      if (i == 3) goto CONTINUE;
      if (val < 0) {
	assert (ok);
	assert (i >= 1);
	lgl->stats.dst.lits++;
	lgl->stats.prgss++;
	if (i != 2) SWAP (int, c[i], c[2]);
TRN2BIN:
	assert (!lglval (lgl, c[0]));
	assert (!lglval (lgl, c[1]));
	LOG (2, "distilled ternary clause becomes binary clause %d %d",
	     c[0], c[1]);
#ifndef NLGLPICOSAT
	lglpicosatchkclsarg (lgl, c[0], c[1], 0);
#endif
	lgl->stats.irr++; assert (lgl->stats.irr > 0);
	lglwchbin (lgl, c[0], c[1], 0);
	lglwchbin (lgl, c[1], c[0], 0);
	goto TRNREM;
      } else if (val > 0) {
	assert (i > 0);
	if (i == 2) goto TRNRED;
	assert (i == 1);
	antecedents = lgldstanalit (lgl, lit);
	lglpopnunmarkstk (lgl, &lgl->clause);
	if (antecedents == 1) goto TRNRED;
	lgl->stats.dst.clauses++;
	goto TRN2BIN;
      } else if (i == 0) {
	assert (!ok);
	assert (lgl->conf.lit);
	lglbacktrack (lgl, 0);
TRNFAILED:
        lgl->stats.dst.failed++;
	LOG (1, "failed literal %d during distilling ternary clause", -lit);
	lglunit (lgl, lit);
	assert (!lgl->level);
	assert (!lgl->propred);
	lgl->propred = 1;
	res = lglbcp (lgl);
	lgl->propred = 0;
	if (!res) goto DONE;
	else goto TRNREM;
      } else {
	assert (!ok);
	assert (lgl->conf.lit);
	lit = lgldstanaconf (lgl);
	lglbacktrack (lgl, 0);
	if (lit) goto TRNFAILED;
TRNRED:
	LOG (2,
	     "redundant distilled ternary clause %d %d %d",
	     c[0], c[1], c[2]);
	lgl->stats.dst.red++;
	lgl->stats.prgss++;
TRNREM:
	lglrmtwch (lgl, c[0], c[1], c[2], 0);
	lglrmtwch (lgl, c[1], c[0], c[2], 0);
	lglrmtwch (lgl, c[2], c[0], c[1], 0);
	lgldeclscnt (lgl, 0, 3);
	*c = REMOVED;
      }
    }
CONTINUE:
    last = pos;
    pos += delta;
    if (pos >= mod) pos -= mod;
    if (pos == first) { assert (count == mod); break; }
    if (mod == 1) break;
    if (first == mod) first = last;
  }
DONE:
  DEL (trn, 3*ntrn*sizeof *trn);
  DEL (clauses, mod);
  lglfitstk (lgl, &lgl->irr);
  assert (!lgl->measureagility && !lgl->propred);
  lgl->measureagility = lgl->propred = 1;
  lgl->limits.dst.vinc += lgl->opts.dstvintinc.val;
  lgl->limits.dst.cinc += lgl->opts.dstcintinc.val;
  lgl->limits.dst.visits = lgl->stats.visits.search + lgl->limits.dst.vinc;
  lgl->limits.dst.confs = lgl->stats.confs + lgl->limits.dst.cinc;
  lgl->limits.dst.prgss = lgl->stats.prgss;
  LOG (1, "distilled %d clauses", distilled);
  lglrep (lgl, 2, 'l');
STATS:
  assert (lgl->simp);
  assert (lgl->distilling);
  lgl->distilling = 0;
  lgl->simp = 0;
  lglstop (lgl);
  return res;
}

static int lgltrdbin (LGL * lgl, int start, int target, int irr) {
  int lit, next, blit, tag, red, other, * p, * w, * eow, res, ign, val;
  HTS * hts;
  assert (lglmtstk (&lgl->seen));
  assert (lglabs (start) < lglabs (target));
  LOG (2, "trying transitive reduction of %s binary clause %d %d", 
       lglred2str (irr^REDCS), start, target);
  lgl->stats.trd.bins++;
  lglpushnmarkseen (lgl, -start);
  next = 0;
  res = 0;
  ign = 1;
  while (next < lglcntstk (&lgl->seen)) {
    lit = lglpeek (&lgl->seen, next++);
    lgl->stats.trd.steps++;
    LOG (3, "transitive reduction search step %d", lit);
    val = lglval (lgl, lit);
    if (val) continue;
    hts = lglhts (lgl, -lit);
    if (!hts->count) continue;
    w = lglhts2wchs (lgl, hts);
    eow = w + hts->count;
    for (p = w; p < eow; p++) {
      blit = *p;
      tag = blit & MASKCS;
      if (tag == LRGCS || tag == TRNCS) p++;
      if (tag != BINCS) continue;
      red = blit & REDCS;
      if (irr && red) continue;
      other = blit >> RMSHFT;
      if (other == start) continue;
      if (other == target) {
	if (lit == -start && ign) { ign = 0; continue; }
	LOG (2, "transitive path closed with %s binary clause %d %d",
	     lglred2str (red), -lit, other);
	res = 1;
	goto DONE;
      }
      val = lglmarked (lgl, other);
      if (val > 0) continue;
      if (val < 0) {
	assert (lgl->level == 0);
	lgl->stats.trd.failed++;
        LOG (1, "failed literal %d in transitive reduction", -start);
	lglunit (lgl, start);
	val = lglbcp (lgl);
	if (!val && !lgl->mt) lgl->mt = 1;
	assert (val || lgl->mt);
	res = -1;
	goto DONE;
      }
      lglpushnmarkseen (lgl, other);
      LOG (3, "transitive reduction follows %s binary clause %d %d",
           lglred2str (red), -lit, other);
    }
  }
DONE:
  lglpopnunmarkstk (lgl, &lgl->seen);
  return res;
}

static void lgltrdlit (LGL * lgl, int start) {
  int target, * w, * p, * eow, blit, tag, red, val;
#ifndef NDEBUG
  int unassigned = lgl->unassigned;
#endif
  HTS * hts;
  val = lglval (lgl, start);
  if (val) return;
  LOG (2, "transitive reduction of binary clauses with %d", start);
  assert (lglmtstk (&lgl->seen));
  hts = lglhts (lgl, start);
  if (!hts->count) return;
  lgl->stats.trd.lits++;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w;  
       p < eow && (lgl->stats.trd.steps < lgl->limits.trd.steps);
       p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag != BINCS) continue;
    target = blit >> RMSHFT;
    if (lglabs (start) > lglabs (target)) continue;
    red = blit & REDCS;
    val = lgltrdbin (lgl, start, target, red^REDCS);
    if (!val) continue;
    if (val < 0) { assert (lgl->mt || lgl->unassigned < unassigned); break; }
    LOG (2, "removing transitive redundant %s binary clause %d %d",
         lglred2str (red), start, target);
    lgl->stats.trd.red++;
    lglrmbwch (lgl, start, target, red);
    lglrmbwch (lgl, target, start, red);
    assert (!lgl->dense);
    if (red) break;
    assert (lgl->stats.irr > 0);
    lgl->stats.irr--;
    break;
  }
}

static int lgltrd (LGL * lgl) {
  unsigned pos, delta, mod, ulit, first, last;
  int lit, count;
  int64_t limit;
  if (lgl->nvars <= 2) return 1;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  lglstart (lgl, &lgl->stats.tms.trd);
  mod = 2*(lgl->nvars - 2);
  assert (mod > 0);
  lgl->stats.trd.count++;
  pos = lglrand (lgl) % mod;
  delta = lglrand (lgl) % mod;
  if (!delta) delta++;
  while (lglgcd (delta, mod) > 1)
    if (++delta == mod) delta = 1;
  LOG (1, "transitive reduction start %u delta %u mod %u", pos, delta, mod);
  if (lgl->stats.trd.count == 1) limit = lgl->opts.trdmaxeff.val/10;
  else limit = (lgl->opts.trdreleff.val*lgl->stats.visits.search)/1000;
  if (limit < lgl->opts.trdmineff.val) limit = lgl->opts.trdmineff.val;
  if (limit > lgl->opts.trdmaxeff.val) limit = lgl->opts.trdmaxeff.val;
  LOG (1, "transitive reduction with up to %lld search steps", limit);
  lgl->limits.trd.steps = lgl->stats.trd.steps + limit;
  first = mod;
  count = 0;
  while (lgl->stats.trd.steps < lgl->limits.trd.steps && 
         !lglterminate (lgl) ) {
    ulit = pos + 4;
    lit = lglilit (ulit);
    lgltrdlit (lgl, lit);
    count++;
    assert (count <= mod);
    if (lgl->mt) break;
    last = pos;
    pos += delta;
    if (pos >= mod) pos -= mod;
    if (pos == first) { assert (count == mod); break; }
    if (mod == 1) break;
    if (first == mod) first = last;
  }
  lgl->limits.trd.cinc += lgl->opts.trdcintinc.val;
  lgl->limits.trd.vinc += lgl->opts.trdvintinc.val;
  lgl->limits.trd.visits = lgl->stats.visits.search + lgl->limits.trd.vinc;
  lgl->limits.trd.confs = lgl->stats.confs + lgl->limits.trd.cinc;
  lgl->limits.trd.prgss = lgl->stats.prgss;
  lglrep (lgl, 2, 't');
  lglstop (lgl);
  return !lgl->mt;
}

static int lglcollecting (LGL * lgl) {
  if (lgl->stats.visits.search <= lgl->limits.gc.visits) return 0;
  if (lgl->stats.confs <= lgl->limits.gc.confs) return 0;
  return lgl->stats.fixed.current;
}

static int lglprobing (LGL * lgl) {
  if (!lgl->opts.probe.val) return 0;
  if (lgl->stats.prgss <= lgl->limits.prb.prgss) return 0;
  if (lgl->stats.confs <= lgl->limits.prb.confs) return 0;
  return lgl->stats.visits.search >= lgl->limits.prb.visits;
}

static int lgldistilling (LGL * lgl) {
  if (!lgl->opts.distill.val) return 0;
  if (lgl->stats.confs <= lgl->limits.dst.confs) return 0;
  if (lgl->stats.prgss <= lgl->limits.dst.prgss) return 0;
  return lgl->stats.visits.search >= lgl->limits.dst.visits;
}

static int lgltreducing (LGL * lgl) {
  if (!lgl->opts.transred.val) return 0;
  if (lgl->stats.confs <= lgl->limits.trd.confs) return 0;
  if (lgl->stats.prgss <= lgl->limits.trd.prgss) return 0;
  return lgl->stats.visits.search >= lgl->limits.trd.visits;
}

static int lgldecomposing (LGL * lgl) {
  if (!lgl->opts.decompose.val) return 0;
  if (!lglsmall (lgl)) return 0;
  if (lgl->stats.confs <= lgl->limits.dcp.confs) return 0;
  if (lgl->stats.prgss <= lgl->limits.dcp.prgss) return 0;
  return lgl->stats.visits.search >= lgl->limits.dcp.visits;
}

static int lgleliminating (LGL * lgl) {
  if (!lgl->opts.elim.val) return 0;
  if (!lglsmall (lgl)) return 0;
  if (lgl->stats.confs <= lgl->limits.elm.confs) return 0;
  if (lgl->stats.prgss <= lgl->limits.elm.prgss) return 0;
  return lgl->stats.visits.search >= lgl->limits.elm.visits;
}

static int lglreducing (LGL * lgl) {
  return lgl->stats.red.lrg >= lgl->limits.reduce;
}

static int lgldefragmenting (LGL * lgl) {
  int relfree;
  if (lgl->stats.pshwchs < lgl->limits.dfg.pshwchs) return 0;
  if (lgl->stats.prgss <= lgl->limits.dfg.prgss) return 0;
  if (lgl->nfreewchs < 1000) return 0;
  if (!lgl->nvars) return 0;
  relfree = (100 * lgl->nfreewchs + 99) / lgl->nvars;
  return relfree >= lgl->opts.defragfree.val;
}

static int lglrestarting (LGL * lgl) {
  if (lgl->level <= 1) return 0;
  return lgl->stats.confs >= lgl->limits.restart.confs;
}

static int lglrebiasing (LGL * lgl) {
  if (!lgl->opts.rebias.val) return 0;
  return lgl->stats.confs >= lgl->limits.rebias.confs;
}

static int lglsimpaux (LGL * lgl) {
  if (lglprobing (lgl) && !lglprobe (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  if (lgldistilling (lgl) && !lgldistill (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  if (lgldecomposing (lgl) && !lgldecomp (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  if (lgltreducing (lgl) && !lgltrd (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  if (lgleliminating (lgl) && !lglelim (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  if (lglterminate (lgl)) return 1;
  if (lglcollecting (lgl)) lglgc (lgl);
  if (lglreducing (lgl)) lglreduce (lgl);
  if (lgldefragmenting (lgl)) lgldefrag (lgl);
  if (lglrebiasing (lgl)) lglrebias (lgl);
  return 1;
}

static int lglsimp (LGL * lgl) {
  int res;
  assert (!lgl->simplifying);
  lgl->simplifying = 1;
  res = lglsimpaux (lgl);
  assert (lgl->simplifying);
  lgl->simplifying = 0;
  return res;
}

static int lglcdcl (LGL * lgl) {
  for (;;) {
    if (lglbcpsearch (lgl) && lglsimp (lgl)) {
      if (lglterminate (lgl)) return 0;
      if (!lglsync (lgl)) return 20;
      if (lglrestarting (lgl)) lglrestart (lgl);
      if (!lgldecide (lgl)) return 10;
    } else if (!lglana (lgl)) return 20;
  }
}

static int lglsolve (LGL * lgl) {
  assert (lgl->state == READY);
  if (lgl->mt) return 20;
  assert (!lgl->level);
  if (!lglbcp (lgl)) return 20;
  if (lglterminate (lgl)) return 0;
  if (lgl->opts.probe.val && !lglprobe (lgl)) return 20;
  if (lglterminate (lgl)) return 0;
  if (lglterminate (lgl)) return 0;
  if (lgl->opts.distill.val && !lgldistill (lgl)) return 20;
  if (lglterminate (lgl)) return 0;
  if (lgl->opts.decompose.val && !lgldecomp (lgl)) return 20;
  if (lglterminate (lgl)) return 0;
  if (lgl->opts.transred.val && !lgltrd (lgl)) return 20;
  if (lglterminate (lgl)) return 0;
  if (lgl->opts.elim.val && !lglelim (lgl)) return 20;
  if (lglterminate (lgl)) return 0;
  lglbias (lgl);
  if (lglterminate (lgl)) return 0;
  if (lgl->opts.bias.val) lgl->bias = lgl->opts.bias.val;
  else lglcutwidth (lgl);
  lglrep (lgl, 1, 's');
  return lglcdcl (lgl);
}

static void lglsetup (LGL * lgl) {
  assert (lgl->state < READY);

  lgl->limits.dfg.pshwchs = lgl->stats.pshwchs + lgl->opts.defragint.val;
  lgl->limits.reduce = lgl->opts.redlinit.val;

  lgl->limits.gc.cinc = lgl->opts.gccintinc.val;
  lgl->limits.gc.vinc = lgl->opts.gcvintinc.val;
  lgl->limits.gc.confs = -1;
  lgl->limits.gc.visits = -1;

  lgl->limits.dcp.cinc = lgl->opts.dcpcintinc.val;
  lgl->limits.dcp.vinc = lgl->opts.dcpvintinc.val;
  lgl->limits.dcp.confs = -1;
  lgl->limits.dcp.visits = -1;
  lgl->limits.dcp.prgss = -1;

  lgl->limits.dst.cinc = lgl->opts.dstcintinc.val;
  lgl->limits.dst.vinc = lgl->opts.dstvintinc.val;
  lgl->limits.dst.confs = -1;
  lgl->limits.dst.visits = -1;
  lgl->limits.dst.prgss = -1;

  lgl->limits.elm.cinc = lgl->opts.elmcintinc.val;
  lgl->limits.elm.vinc = lgl->opts.elmvintinc.val;
  lgl->limits.elm.confs = -1;
  lgl->limits.elm.visits = -1;
  lgl->limits.elm.prgss = -1;

  lgl->limits.prb.cinc = lgl->opts.prbcintinc.val;
  lgl->limits.prb.vinc = lgl->opts.prbvintinc.val;
  lgl->limits.prb.confs = -1;
  lgl->limits.prb.visits = -1;
  lgl->limits.prb.prgss = -1;

  lgl->limits.trd.cinc = lgl->opts.trdcintinc.val;
  lgl->limits.trd.vinc = lgl->opts.trdvintinc.val;
  lgl->limits.trd.confs = -1;
  lgl->limits.trd.visits = -1;
  lgl->limits.trd.prgss = -1;

  lgl->limits.term.steps = -1;

  lglincrestartl (lgl, 0);
  lglincrebiasl (lgl);

  lgl->rng = (unsigned) lgl->opts.seed.val;

  assert (!lgl->stats.decisions);
  lgl->limits.randec += lgl->opts.randecint.val/2;
  lgl->limits.randec += lglrand (lgl) % lgl->opts.randecint.val;

  lgl->state = READY;

  if (!lgl->opts.plain.val) return;

  lgl->opts.decompose.val = 0;
  lgl->opts.distill.val = 0;
  lgl->opts.elim.val = 0;
  lgl->opts.probe.val = 0;
  lgl->opts.transred.val = 0;
}

static void lglinitsolve (LGL * lgl) {
  if (lgl->state < READY) lglsetup (lgl);
  lglredvars (lgl);
  lglfitstk (lgl, &lgl->irr);
  lglfitstk (lgl, &lgl->e2i);
  lglfitstk (lgl, &lgl->dsched);
#ifndef NCHKSOL
  lglfitstk (lgl, &lgl->orig);
#endif
  lglrep (lgl, 1, '*');
}

#ifndef NLGLPICOSAT
static void lglpicosatchksol (LGL * lgl) {
#ifndef NDEBUG
  int idx, lit, res;
  Val val;
  if (!lgl->opts.check.val) return;
  if (lgl->picosat.res) assert (lgl->picosat.res == 10);
  assert (!picosat_inconsistent ());
  for (idx = 2; idx < lgl->nvars; idx++) {
    val = lglval (lgl, idx);
    assert (val);
    lit = lglsgn (val) * idx;
    picosat_assume (lit);
  }
  res = picosat_sat (-1);
  assert (res == 10);
  LOG (1, "PicoSAT checked solution");
#endif
}
#endif

#ifndef NCHKSOL
#include <signal.h>
#include <unistd.h>
static void lglchksol (LGL * lgl) {
  int * p, * c, * eoo = lgl->orig.top, lit, satisfied;
  assert (lglmtstk (&lgl->orig) || !eoo[-1]);
  for (c = lgl->orig.start; c < eoo; c = p + 1) {
    satisfied = 0;
    for (p = c; (lit = *p); p++)
      if (!satisfied && lglderef (lgl, lit) > 0) 
	satisfied = 1;
    assert (satisfied);
    if (satisfied) continue;
    sleep (1);
    kill (getpid (), SIGBUS);
    sleep (1);
    abort ();
  }
}
#endif

static void lglextend (LGL * lgl) {
  int * p, lit, next, satisfied, val, * start = lgl->extend.start;
  p = lgl->extend.top;
  while (p > start) {
    satisfied = 0;
    next = 0;
    do {
      lit = next;
      next = *--p;
      if (!lit || satisfied) continue;
      val = lglderef (lgl, lit);
      assert (!next || val);
      if (val > 0) satisfied = 1;
    } while (next);
    assert (lit);
    if (satisfied) continue;
    LOG (3, "%s external assign %d",
         lglpeek (&lgl->e2i, lglabs (lit)) ? "flipping" : "extending", lit);
    lglpoke (&lgl->e2i, lglabs (lit), 2*lglsgn (lit));
  }
}

int lglsat (LGL * lgl) {
  int res;
  lglinitsolve (lgl);
  res = lglsolve (lgl);
  if (!res) lgl->state = UNKNOWN;
  if (res == 10) lgl->state = SATISFIED, lglextend (lgl);
  if (res == 20) lgl->state = UNSATISFIED;
  lglflshrep (lgl);
#ifndef NCHKSOL
  if (res == 10) lglchksol (lgl);
#endif
#ifndef NLGLPICOSAT
  if (res == 10) lglpicosatchksol (lgl);
  if (res == 20) lglpicosatchkunsat (lgl);
#endif
  return res;
}

int lglmaxvar (LGL * lgl) { 
  int res = lglcntstk (&lgl->e2i);
  if (res) res--;
  return res;
}

int lglderef (LGL * lgl, int elit) {
  int ilit, res, repr;
  ABORTIFNOTINSTATE (SATISFIED);
  assert (lglabs (elit) < lglcntstk (&lgl->e2i));
  repr = lglerepr (lgl, elit);
  ilit = lglpeek (&lgl->e2i, lglabs (repr));
  assert (!(ilit & 1));
  ilit /= 2;
  res = ilit ? lglcval (lgl, ilit) : 0;
  if (repr < 0) res = -res;
  return res;
}

static void lglprs (LGL * lgl, const char * fmt, ...) {
  va_list ap;
  fputs ("c ", stdout);
  if (lgl->tid >= 0) printf ("%d ", lgl->tid);
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
}

#ifndef NLGLSTATS
static void lglgluestats (LGL * lgl) {
  int64_t added, forcing, resolved, conflicts;
  int count, glue;
  Lir * lir;
  lglprs (lgl, "");
  lglprs (lgl, "%s    %10s %3s %10s %3s %10s %3s %10s",
          "glue",
	  "added","",
	  "forcing","",
	  "resolved","",
	  "conflicts");
  count = 0;
  added = forcing = resolved = conflicts = 0;
  for (glue = 0; glue < GLUE; glue++) {
    lir = lgl->red + glue;
    added += lir->added;
    forcing += lir->forcing;
    resolved += lir->resolved;
    conflicts += lir->conflicts;
  }
  lglprs (lgl, "");
  lglprs (lgl, "all %10lld %3.0f %10lld %3.0f %10lld %3.0f %10lld %3.0f",
	  (long long) added, 100.0,
	  (long long) forcing, 100.0,
	  (long long) resolved, 100.0,
	  (long long) conflicts, 100.0);
  lglprs (lgl, "");
  for (glue = 0; glue < GLUE; glue++) {
    lir = lgl->red + glue;
    lglprs (lgl,
            "%3d %10lld %3.0f %10lld %3.0f %10lld %3.0f %10lld %3.0f",
            glue, 
	    (long long) lir->added, lglpcnt (lir->added, added),
	    (long long) lir->forcing, lglpcnt (lir->forcing, forcing),
	    (long long) lir->resolved, lglpcnt (lir->resolved, resolved),
	    (long long) lir->conflicts, lglpcnt (lir->conflicts, conflicts));
  }
}
#endif

void lglstats (LGL * lgl) {
  double t = lglsec (lgl), r;
  Stats * s = &lgl->stats;
  int64_t p = s->props.search + s->props.simp;
  int64_t v = s->visits.search + s->visits.simp;
  int red;
  lglprs (lgl, "blkd: %d blocked clauses in %lld resolutions",
          s->blk.elm, (long long) s->blk.res);
  lglprs (lgl, "coll: %d reductions, %d gcs", s->reduced, s->gcs);
  lglprs (lgl, "dcps: %d decompositions, %d equivalent",
          s->decomps, s->equivalent.sum);
  lglprs (lgl, "decs: %d decisions, %d random %.3f%%, %d rescored",
          s->decisions, s->randecs, lglpcnt (s->randecs, s->decisions),
	  s->rescored);
  lglprs (lgl, "dsts: %d distillations, %d red (%d failed, %d sub), %d str",
	  s->dst.count,
	   s->dst.red + s->dst.failed + s->dst.clauses,
	   s->dst.failed, s->dst.clauses,
	   s->dst.lits);
  lglprs (lgl, "elms: %d eliminations, %d skipped, %d eliminated",
          s->elm.count, s->elm.skipped, s->elm.elmd);
  lglprs (lgl, "elms: %d small %.0f%%, %d forced %.0f%%,  %d large %.0f%%",
          s->elm.small.elm, lglpcnt (s->elm.small.elm, s->elm.elmd),
          s->elm.forced, lglpcnt (s->elm.forced, s->elm.elmd),
          s->elm.large, lglpcnt (s->elm.large, s->elm.elmd));
  lglprs (lgl, "elms: %d tried small, %d succeeded %.0f%%, %d failed %.0f%%",
	  s->elm.small.tried, 
	  s->elm.small.tried - s->elm.small.failed,
	    lglpcnt (s->elm.small.tried - s->elm.small.failed,
	             s->elm.small.tried),
	  s->elm.small.failed,
	    lglpcnt (s->elm.small.failed, s->elm.small.tried));
  lglprs (lgl, "elms: %d subsumed, %d strengthened, %d blocked",
          s->elm.sub, s->elm.str, s->elm.blkd);
  lglprs (lgl, "elms: %lld copies, %lld resolutions",
          (long long) s->elm.copies, (long long) s->elm.resolutions);
  lglprs (lgl, "elms: %lld subchks, %lld strchks",
          (long long) s->elm.subchks, (long long) s->elm.strchks);
  lglprs (lgl, "hbrs: %d = %d trn %.0f%% + %d lrg %.0f%%, %d sub %.0f%%",
          s->hbr.cnt,
	  s->hbr.trn, lglpcnt (s->hbr.trn, s->hbr.cnt),
	  s->hbr.lrg, lglpcnt (s->hbr.lrg, s->hbr.cnt),
	  s->hbr.sub, lglpcnt (s->hbr.sub, s->hbr.cnt));
  red = s->hte.lrg + s->hte.trn;
  lglprs (lgl, "htes: %d redundant clauses (%0.f%% ternary), "
          "%lld steps, %d failed",
          red, lglpcnt (s->hte.trn, red), (long long) s->hte.steps,
	  s->hte.failed);
  lglprs (lgl, "lrnd: %d clauses, %d uips %.0f%%, %.1f length, %.1f glue",
          s->clauses.learned,
	  s->uips, lglpcnt (s->uips, s->clauses.learned),
          lglavg (s->lits.learned, s->clauses.learned),
          lglavg (s->clauses.glue, s->clauses.learned));
  lglprs (lgl, "mins: %lld learned literals, %.0f%% minimized away",
          (long long) s->lits.learned,
	  lglpcnt ((s->lits.nonmin - s->lits.learned), s->lits.nonmin));
  assert (s->lits.nonmin >= s->lits.learned);
  lglprs (lgl, "otfs: str %d dyn (%d red, %d irr), %d static", 
          s->otfs.str.dyn.red + s->otfs.str.dyn.irr,
          s->otfs.str.dyn.red, s->otfs.str.dyn.irr,
	  s->otfs.str.stat);
  lglprs (lgl, "otfs: sub %d dyn (%d red, %d irr), %d static, %d drv", 
          s->otfs.sub.dyn.red + s->otfs.sub.dyn.irr,
          s->otfs.sub.dyn.red, s->otfs.sub.dyn.irr,
	  s->otfs.sub.stat, s->otfs.driving);
  lglprs (lgl, "prbs: %d cnt, %lld probed, %d failed, %d lifted",
          s->prb.count, (long long) s->prb.probed,
	  s->prb.failed, s->prb.lifted);
  lglprs (lgl, "prps: %lld props, %.0f%% srch, %.0f%% simp, %.0f props/dec",
          (long long) p, lglpcnt (s->props.search, p), 
	  lglpcnt (s->props.simp, p), lglavg (s->props.search, s->decisions));
  lglprs (lgl, "rsts: %d restarts, %d skipped, %d rebias",
          s->restarts, s->skipped, s->rebias);
  lglprs (lgl, "tops: %d fixed, %d iterations",
          s->fixed.sum, s->iterations);
  lglprs (lgl, "trds: %d transitive reductions, %d removed, %d failed",
	  s->trd.count, s->trd.red, s->trd.failed);
  lglprs (lgl, "trds: %lld nodes, %lld edges, %lld steps",
          (long long) s->trd.lits, (long long) s->trd.bins,
	  (long long) s->trd.steps);
  lglprs (lgl, "vsts: %lld visits, %.0f%% srch, %.0f%% simp, %.1f visits/prop",
          (long long) v, lglpcnt (s->visits.search, v),
	  lglpcnt (s->visits.simp, v), lglavg (v, p));
  lglprs (lgl, "wchs: %lld pushed, %lld enlarged, %d defrags",
           (long long) s->pshwchs, (long long) s->enlwchs, s->defrags);
#ifndef NLGLSTATS
  lglgluestats (lgl);
#endif
  lglprs (lgl, "");
  lglprs (lgl, "%d decisions, %d conflicts", s->decisions, s->confs);
  lglprs (lgl, "%lld propagations, %.1f megaprops/sec", 
          (long long) p, ((t > 0) ? (p / 1e6 / t) : 0));
  lglprs (lgl, "%.1f seconds, %.1f MB", t, lglmaxmb (lgl));
  lglprs (lgl, "");
  r = t;
  r -= s->tms.gc;
  lglprs (lgl, "%8.3f %3.0f%% gc", s->tms.gc, lglpcnt (s->tms.gc, t));
  r -= s->tms.prb;
  lglprs (lgl, "%8.3f %3.0f%% probe", s->tms.prb, lglpcnt (s->tms.prb, t));
  r -= s->tms.dst;
  lglprs (lgl, "%8.3f %3.0f%% distill", s->tms.dst, lglpcnt (s->tms.dst, t));
  r -= s->tms.dfg;
  lglprs (lgl, "%8.3f %3.0f%% defrag", s->tms.dfg , lglpcnt (s->tms.dfg , t));
  r -= s->tms.red;
  lglprs (lgl, "%8.3f %3.0f%% reduce", s->tms.red , lglpcnt (s->tms.red , t));
  r -= s->tms.trd;
  lglprs (lgl, "%8.3f %3.0f%% transred", s->tms.trd, lglpcnt (s->tms.trd, t));
  r -= s->tms.dcp;
  lglprs (lgl, "%8.3f %3.0f%% decomp", s->tms.dcp, lglpcnt (s->tms.dcp, t));
  r -= s->tms.elm;
  lglprs (lgl, "%8.3f %3.0f%% elim", s->tms.elm, lglpcnt (s->tms.elm, t));
  lglprs (lgl, "-----------------------");
  lglprs (lgl, "%8.3f %3.0f%% block", s->tms.blk , lglpcnt (s->tms.blk , t));
  lglprs (lgl, "%8.3f %3.0f%% block gc", s->tms.gcb, lglpcnt (s->tms.gcb, t));
  lglprs (lgl, "%8.3f %3.0f%% hte", s->tms.hte , lglpcnt (s->tms.hte , t));
  lglprs (lgl, "%8.3f %3.0f%% decomp trj", s->tms.trj, lglpcnt (s->tms.trj, t));
  lglprs (lgl, "%8.3f %3.0f%% decomp gc", s->tms.gcd, lglpcnt (s->tms.gcd, t));
  lglprs (lgl, "%8.3f %3.0f%% elim gc", s->tms.gce, lglpcnt (s->tms.gce, t));
  lglprs (lgl, "-----------------------");
  lglprs (lgl, "%8.3f %3.0f%% simplifying",
           t - r, lglpcnt (t - r, t));
  lglprs (lgl, "%8.3f %3.0f%% search", r, lglpcnt (r, t));
  lglprs (lgl, "=======================");
  lglprs (lgl, "%8.3f %3.0f%% all", t, 100.0);
  fflush (stdout);
}

double lglgetprops (LGL * lgl) { 
  return lgl->stats.props.search + lgl->stats.props.simp;
}

int lglgetconfs (LGL * lgl) { return lgl->stats.confs; }
int lglgetdecs (LGL * lgl) { return lgl->stats.decisions; }

void lglsizes (void) {
  printf ("c sizeof (int) == %ld\n", (long) sizeof (int));
  printf ("c sizeof (unsigned) == %ld\n", (long) sizeof (unsigned));
  printf ("c sizeof (void*) == %ld\n", (long) sizeof (void*));
  printf ("c sizeof (Stk) == %ld\n", (long) sizeof (Stk));
  printf ("c sizeof (AVar) == %ld\n", (long) sizeof (AVar));
  printf ("c sizeof (DVar) == %ld\n", (long) sizeof (DVar));
  printf ("c sizeof (EVar) == %ld\n", (long) sizeof (EVar));
  printf ("c sizeof (LGL) == %ld\n", (long) sizeof (LGL));
  printf ("c MAXVAR == %ld\n", (long) MAXVAR);
  printf ("c MAXREDLIDX == %ld\n", (long) MAXREDLIDX);
  printf ("c MAXIRRLIDX == %ld\n", (long) MAXIRRLIDX);
  fflush (stdout);
}

void lglrelease (LGL * lgl) {
  int i;
  DEL (lgl->avars, lgl->szvars);
  DEL (lgl->dvars, lgl->szvars);
  DEL (lgl->vals, lgl->szvars);
  DEL (lgl->i2e, lgl->szvars);
  lglrelstk (lgl, &lgl->wchs);
#if !defined(NDEBUG) || !defined(NLGLOG)
  lglrelstk (lgl, &lgl->resolvent);
#endif
  lglrelstk (lgl, &lgl->e2i);
  lglrelstk (lgl, &lgl->extend);
  lglrelstk (lgl, &lgl->clause);
  lglrelstk (lgl, &lgl->dsched);
  lglrelstk (lgl, &lgl->irr);
  for (i = 0; i < GLUE; i++)
    lglrelstk (lgl, &lgl->red[i].lits);
  lglrelstk (lgl, &lgl->trail);
  lglrelstk (lgl, &lgl->control);
  lglrelstk (lgl, &lgl->frames);
  lglrelstk (lgl, &lgl->saved);
  lglrelstk (lgl, &lgl->seen);
  lglrelstk (lgl, &lgl->stack);
#ifndef NDEBUG
#ifndef NLGLPICOSAT
  if (lgl->opts.verbose.val > 0 && lgl->opts.check.val > 0) picosat_stats ();
  picosat_reset ();
#endif
#endif
#ifndef NCHKSOL
  lglrelstk (lgl, &lgl->orig);
#endif
  assert (getenv ("LGLEAK") || !lgl->stats.bytes.current);
  free (lgl);
}

#ifndef NDEBUG

void lgldump (LGL * lgl) {
  int idx, sign, lit, blit, tag, red, other, other2, glue;
  const int * p, * w, * eow, * c;
  HTS * hts;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	red = blit & REDCS;
	if (tag == TRNCS || tag == LRGCS) p++;
        if (tag == BINCS) {
	  other = blit >> RMSHFT;
	  if (lglabs (other) < idx) continue;
	  other2 = 0;
	} else if (tag == TRNCS) {
	  other = blit >> RMSHFT;
	  if (lglabs (other) < idx) continue;
	  other2 = *p;
	  if (lglabs (other2) < idx) continue;
	} else continue;
	printf ("%s %d %d", red ? "red" : "irr", lit, other);
	if (tag == TRNCS) printf (" %d", other2);
	printf ("\n");
      }
    }
  for (c = lgl->irr.start; c < lgl->irr.top; c = p + 1) {
    p = c;
    if (*p == REMOVED) continue;
    printf ("irr");
    while (*p) printf (" %d", *p++);
    printf ("\n");
  }
  for (glue = 0; glue < MAXGLUE; glue++) {
    for (c = lgl->red[glue].lits.start; 
         c < lgl->red[glue].lits.top;
	 c = p + 1) {
      p = c;
      if (*p == REMOVED) continue;
      printf ("g%02d", glue);
      while (*p) printf (" %d", *p++);
      printf ("\n");
    }
  }
}

#endif
