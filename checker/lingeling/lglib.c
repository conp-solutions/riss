/*-------------------------------------------------------------------------*/
/* Copyright 2010-2012 Armin Biere Johannes Kepler University Linz Austria */
/*-------------------------------------------------------------------------*/

#include "lglib.h"

/*-------------------------------------------------------------------------*/

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

/*-------------------------------------------------------------------------*/

#ifndef NLGLPICOSAT
#include "picosat.h"
#endif

/*-------------------------------------------------------------------------*/

#define REMOVED		INT_MAX
#define NOTALIT		((INT_MAX >> RMSHFT))
#define MAXVAR		((INT_MAX >> RMSHFT) - 2)

#define MAXSCOREXP	(1<<24)
#define SCINCMINEXP	(-(1<<24))

#define GLUESHFT	4
#define POW2GLUE	(1 << GLUESHFT)
#define MAXGLUE		(POW2GLUE - 1)
#define GLUEMASK	(POW2GLUE - 1)
#define MAXREDLIDX	((1 << (31 - GLUESHFT)) - 2)
#define MAXIRRLIDX	((1 << (31 - RMSHFT)) - 2)

#define MAXLDFW		31	
#define REPMOD 		23

#define FUNVAR		12
#define FUNQUADS	(1<<(FUNVAR - 6))

#define MAXPEN		4

#define FALSECNF	(1ll<<32)
#define TRUECNF		0ll

#define FLTPRC 		32
#define EXPMIN 		(0x0000 ## 0000)
#define EXPZRO 		(0x1000 ## 0000)
#define EXPMAX		(0x7fff ## ffff)
#define MNTBIT		(0x0000 ## 0001 ## 0000 ## 0000 ## ull)
#define MNTMAX		(0x0000 ## 0001 ## ffff ## ffff ## ull)
#define FLTMIN		(0x0000 ## 0000 ## 0000 ## 0000 ## ll)
#define FLTMAX		(0x7fff ## ffff ## ffff ## ffff ## ll)

#define LLMAX		FLTMAX

#define MAXFLTSTR	6
#define MAXPHN		10
#define MAXTRAVHIST	32

/*------------------------------------------------------------------------*/
#ifndef NLGLOG
/*------------------------------------------------------------------------*/

#define MAPLOGLEVEL(LEVEL) (LEVEL)

#define LOG(LEVEL,FMT,ARGS...) \
do { \
  if (MAPLOGLEVEL(LEVEL) > lgl->opts->log.val) break; \
  lglogstart (lgl, MAPLOGLEVEL(LEVEL), FMT, ##ARGS); \
  lglogend (lgl); \
} while (0)

#define LOGCLS(LEVEL,CLS,FMT,ARGS...) \
do { \
  const int * P; \
  if (MAPLOGLEVEL(LEVEL) > lgl->opts->log.val) break; \
  lglogstart (lgl, MAPLOGLEVEL(LEVEL), FMT, ##ARGS); \
  for (P = (CLS); *P; P++) fprintf (lgl->out, " %d", *P); \
  lglogend (lgl); \
} while (0)

#define LOGMCLS(LEVEL,CLS,FMT,ARGS...) \
do { \
  const int * P; \
  if (MAPLOGLEVEL(LEVEL) > lgl->opts->log.val) break; \
  lglogstart (lgl, MAPLOGLEVEL(LEVEL), FMT, ##ARGS); \
  for (P = (CLS); *P; P++) fprintf (lgl->out, " %d", lglm2i (lgl, *P)); \
  lglogend (lgl); \
} while (0)

#define LOGRESOLVENT(LEVEL,FMT,ARGS...) \
do { \
  const int * P; \
  if (MAPLOGLEVEL(LEVEL) > lgl->opts->log.val) break; \
  lglogstart (lgl, MAPLOGLEVEL(LEVEL), FMT, ##ARGS); \
  for (P = lgl->resolvent.start; P < lgl->resolvent.top; P++) \
    fprintf (lgl->out, " %d", *P); \
  lglogend (lgl); \
} while (0)

#define LOGREASON(LEVEL,LIT,REASON0,REASON1,FMT,ARGS...) \
do { \
  int TAG, TMP, RED, G; \
  const int * C, * P; \
  if (MAPLOGLEVEL(LEVEL) > lgl->opts->log.val) break; \
  lglogstart (lgl, MAPLOGLEVEL(LEVEL), FMT, ##ARGS); \
  TMP = (REASON0 >> RMSHFT); \
  RED = (REASON0 & REDCS); \
  TAG = (REASON0 & MASKCS); \
  if (TAG == DECISION) fputs (" decision", lgl->out); \
  else if (TAG == UNITCS) fprintf (lgl->out, " unit %d", LIT); \
  else if (TAG == BINCS) { \
    fprintf (lgl->out, \
       " %s binary clause %d %d", lglred2str (RED), LIT, TMP); \
  } else if (TAG == TRNCS) { \
    fprintf (lgl->out, " %s ternary clause %d %d %d", \
	    lglred2str (RED), LIT, TMP, REASON1); \
  } else { \
    assert (TAG == LRGCS); \
    C = lglidx2lits (lgl, TAG, RED, REASON1); \
    for (P = C; *P; P++) \
      ; \
    fprintf (lgl->out, " size %ld", (long)(P - C)); \
    if (RED) { \
      G = (REASON1 & GLUEMASK); \
      fprintf (lgl->out, " glue %d redundant", G); \
    } else fputs (" irredundant", lgl->out); \
    fputs (" clause", lgl->out); \
    for (P = C; *P; P++) { \
      fprintf (lgl->out, " %d", *P); \
    } \
  } \
  lglogend (lgl); \
} while (0)

#define LOGESCHED(LEVEL,LIT,FMT,ARGS...) \
do { \
  int POS; \
  EVar * EV; \
  if (MAPLOGLEVEL(LEVEL) > lgl->opts->log.val) break; \
  POS = *lglepos (lgl, LIT); \
  EV = lglevar (lgl, LIT); \
  lglogstart (lgl, MAPLOGLEVEL(LEVEL), "esched[%d] = %d ", POS, LIT); \
  fprintf (lgl->out, FMT, ##ARGS); \
  fprintf (lgl->out, " score"); \
  fprintf (lgl->out, " occ %d %d", EV->occ[0], EV->occ[1]); \
  lglogend (lgl); \
} while (0)

#define LOGEQN(LEVEL,EQN,FMT,ARGS...) \
do { \
  const int * P, * START; \
  if (MAPLOGLEVEL(LEVEL) > lgl->opts->log.val) break; \
  lglogstart (lgl, MAPLOGLEVEL(LEVEL), FMT, ##ARGS); \
  START = lgl->gauss->xors.start + (EQN); \
  assert (START < lgl->gauss->xors.top); \
  for (P = START; *P > 1; P++) fprintf (lgl->out, " %d", *P); \
  fprintf (lgl->out, " = %d", *P); \
  lglogend (lgl); \
} while (0)

/*------------------------------------------------------------------------*/
#else /* end of then start of else part of 'ifndef NLGLOG' */
/*------------------------------------------------------------------------*/

#define LOG(ARGS...) do { } while (0)
#define LOGCLS(ARGS...) do { } while (0)
#define LOGMCLS(ARGS...) do { } while (0)
#define LOGRESOLVENT(ARGS...) do { } while (0)
#define LOGREASON(ARGS...) do { } while (0)
#define LOGESCHED(ARGS...) do { } while (0)
#define LOGEQN(ARGS...) do { } while (0)

/*------------------------------------------------------------------------*/
#endif /* end of else part of 'ifndef NLGLOG' */
/*------------------------------------------------------------------------*/

#define ABORTIF(COND,FMT,ARGS...) \
do { \
  if (!(COND)) break; \
  fprintf (stderr, "*** API usage error of '%s' in '%s'", \
	   __FILE__, __FUNCTION__); \
  if (lgl && lgl->tid >= 0) fprintf (stderr, " (tid %d)", lgl->tid); \
  fputs (": ", stderr); \
  fprintf (stderr, FMT, ##ARGS); \
  fputc ('\n', stderr); \
  fflush (stderr); \
  lglabort (lgl); \
  exit (1); \
} while (0)

#ifndef NDEBUG
#define ASSERT(COND) \
do { \
  if ((COND)) break; \
  fprintf (stderr, \
    "liblgl.a: %s:%d: %s: Lingeling Assertion `%s' failed.", \
    __FUNCTION__, __LINE__, __FILE__, # COND); \
  if (lgl && lgl->tid >= 0) fprintf (stderr, " (tid %d)", lgl->tid); \
  fputc ('\n', stderr); \
  fflush (stderr); \
  lglabort (lgl); \
  exit (1); \
} while (0)
#else
#define ASSERT(COND) do { } while (0)
#endif

#define COVER(COND) \
do { \
  if (!(COND)) break; \
  fprintf (stderr, \
    "liblgl.a: %s:%d: %s: Coverage target `%s' reached.", \
    __FUNCTION__, __LINE__, __FILE__, # COND); \
  if (lgl && lgl->tid >= 0) fprintf (stderr, " (tid %d)", lgl->tid); \
  fputc ('\n', stderr); \
  fflush (stderr); \
  abort (); /* TODO: why not 'lglabort' */ \
} while (0)

#define REQINIT() \
do { ABORTIF (!lgl, "uninitialized manager"); } while (0)

#define REQUIRE(STATE) \
do { \
  REQINIT (); \
  ABORTIF(!(lgl->state & (STATE)), "!(%s)", #STATE); \
} while (0)

#define TRANS(STATE) \
do { \
  assert (lgl->state != STATE); \
  LOG (1, "transition to state " #STATE); \
  lgl->state = STATE; \
} while (0)

/*------------------------------------------------------------------------*/

#if !defined(NDEBUG) || !defined(NLGLOG)
#define RESOLVENT
#endif

/*------------------------------------------------------------------------*/

#define TRAPI(MSG,ARGS...) \
do { \
  if (!lgl->apitrace) break; \
  lgltrapi (lgl, MSG, ##ARGS); \
} while (0)

#define LGLCHKACT(ACT) \
do { assert (NOTALIT <= (ACT) && (ACT) < REMOVED - 1); } while (0)

/*------------------------------------------------------------------------*/

#define OPT(SHRT,LNG,VAL,MIN,MAX,DESCRP) \
do { \
  Opt * opt = &lgl->opts->LNG; \
  opt->shrt = SHRT; \
  opt->lng = #LNG; \
  opt->val = VAL; \
  assert (MIN <= VAL); \
  opt->min = MIN; \
  assert (VAL <= MAX); \
  opt->max = MAX; \
  opt->descrp = DESCRP; \
  lglgetenv (lgl, opt, #LNG); \
} while (0)

/*------------------------------------------------------------------------*/

#define NEW(P,N) \
do { (P) = lglnew (lgl, (N) * sizeof *(P)); } while (0)

#define DEL(P,N) \
do { lgldel (lgl, (P), (N) * sizeof *(P)); (P) = 0; } while (0)

#define RSZ(P,O,N) \
do { (P) = lglrsz (lgl, (P), (O)*sizeof*(P), (N)*sizeof*(P)); } while (0)

#define CLN(P,N) \
do { memset ((P), 0, (N) * sizeof *(P)); } while (0)

#define CLRPTR(P) \
do { memset ((P), 0, sizeof *(P)); } while (0)

#define CLR(P) \
do { memset (&(P), 0, sizeof (P)); } while (0)

/*------------------------------------------------------------------------*/

#define SWAP(TYPE,A,B) \
do { TYPE TMP = (A); (A) = (B); (B) = TMP; } while (0)

#define ISORTLIM 10

#define CMPSWAP(TYPE,CMP,P,Q) \
do { if (CMP (&(P), &(Q)) > 0) SWAP (TYPE, P, Q); } while(0)

#define QPART(TYPE,CMP,A,L,R) \
do { \
  TYPE PIVOT; \
  int J = (R); \
  I = (L) - 1; \
  PIVOT = (A)[J]; \
  for (;;) { \
    while (CMP (&(A)[++I], &PIVOT) < 0) \
      ; \
    while (CMP (&PIVOT, &(A)[--J]) < 0) \
      if (J == (L)) break; \
    if (I >= J) break; \
    SWAP (TYPE, (A)[I], (A)[J]); \
  } \
  SWAP (TYPE, (A)[I], (A)[R]); \
} while(0)

#define QSORT(TYPE,CMP,A,N) \
do { \
  int L = 0, R = (N) - 1, M, LL, RR, I; \
  assert (lglmtstk (&lgl->sortstk)); \
  if (R - L <= ISORTLIM) break; \
  for (;;) { \
    M = (L + R) / 2; \
    SWAP (TYPE, (A)[M], (A)[R - 1]); \
    CMPSWAP (TYPE, CMP, (A)[L], (A)[R - 1]); \
    CMPSWAP (TYPE, CMP, (A)[L], (A)[R]); \
    CMPSWAP (TYPE, CMP, (A)[R - 1], (A)[R]); \
    QPART (TYPE, CMP, (A), L + 1, R - 1); \
    if (I - L < R - I) { LL = I + 1; RR = R; R = I - 1; } \
    else { LL = L; RR = I - 1; L = I + 1; } \
    if (R - L > ISORTLIM) { \
      assert (RR - LL > ISORTLIM); \
      lglpushstk (lgl, &lgl->sortstk, LL); \
      lglpushstk (lgl, &lgl->sortstk, RR); \
    } else if (RR - LL > ISORTLIM) L = LL, R = RR; \
    else if (!lglmtstk (&lgl->sortstk)) { \
      R = lglpopstk (&lgl->sortstk); \
      L = lglpopstk (&lgl->sortstk); \
    } else break; \
  } \
} while (0)

#define ISORT(TYPE,CMP,A,N) \
do { \
  TYPE PIVOT; \
  int L = 0, R = (N) - 1, I, J; \
  for (I = R; I > L; I--) \
    CMPSWAP (TYPE, CMP, (A)[I - 1], (A)[I]); \
  for (I = L + 2; I <= R; I++) { \
    J = I; \
    PIVOT = (A)[I]; \
    while (CMP (&PIVOT, &(A)[J - 1]) < 0) { \
      (A)[J] = (A)[J - 1]; \
      J--; \
    } \
    (A)[J] = PIVOT; \
  } \
} while (0)

#ifdef NDEBUG
#define CHKSORT(CMP,A,N) do { } while(0)
#else
#define CHKSORT(CMP,A,N) \
do { \
  int I; \
  for (I = 0; I < (N) - 1; I++) \
    assert (CMP (&(A)[I], &(A)[I + 1]) <= 0); \
} while(0)
#endif

#define SORT(TYPE,A,N,CMP) \
do { \
  TYPE * AA = (A); \
  int NN = (N); \
  QSORT (TYPE, CMP, AA, NN); \
  ISORT (TYPE, CMP, AA, NN); \
  CHKSORT (CMP, AA, NN); \
} while (0)

/*------------------------------------------------------------------------*/

#define LGLPOPWTK(WTK,WRAG,LIT,OTHER,RED,REMOVED) \
do { \
  assert (!lglmtwtk (WTK)); \
  (WTK)->top--; \
  (WRAG) = (WTK)->top->wrag; \
  (LIT) = (WTK)->top->lit; \
  (OTHER) = (WTK)->top->other; \
  (RED) = (WTK)->top->red ? REDCS : 0; \
  (REMOVED) = (WTK)->top->removed; \
} while (0)

/*------------------------------------------------------------------------*/

#define CLONE(FIELD,SIZE) \
do { \
  NEW (lgl->FIELD, (SIZE)); \
  memcpy (lgl->FIELD, orig->FIELD, (SIZE) * sizeof *(lgl->FIELD)); \
} while (0)

#define CLONESTK(NAME) \
do { \
  size_t COUNT = orig->NAME.top - orig->NAME.start; \
  size_t SIZE = orig->NAME.end - orig->NAME.start; \
  size_t BYTES = SIZE * sizeof *lgl->NAME.start; \
  NEW (lgl->NAME.start, SIZE); \
  memcpy (lgl->NAME.start, orig->NAME.start, BYTES); \
  lgl->NAME.top = lgl->NAME.start + COUNT; \
  lgl->NAME.end = lgl->NAME.start + SIZE; \
} while (0)

/*-------------------------------------------------------------------------*/

#define LGLL long long
/*-------------------------------------------------------------------------*/

typedef enum Tag {
  FREEVAR = 0,
  FIXEDVAR = 1,
  EQUIVAR = 2,
  ELIMVAR = 3,

  DECISION = 0,
  UNITCS = 1,
  OCCS = 1,
  BINCS = 2,
  TRNCS = 3,
  LRGCS = 4,
  MASKCS = 7,

  REDCS = 8,
  RMSHFT = 4,
} Tag;

typedef enum State {
  UNUSED 	= (1<<0),
  OPTSET 	= (1<<1),
  USED 		= (1<<2),
  READY		= (1<<3),
  UNKNOWN	= (1<<4),
  SATISFIED	= (1<<5),
  EXTENDED      = (1<<6),
  UNSATISFIED	= (1<<7),
  FAILED        = (1<<8),
  LOOKED        = (1<<9),
  RESET		= (1<<10),
} State;

typedef enum Wrag {
  PREFIX = 0,
  BEFORE = 1,
  AFTER = 2,
  POSTFIX = 3,
} Wrag;

typedef enum GTag { ANDTAG, ITETAG, XORTAG } GTag;

/*------------------------------------------------------------------------*/

typedef struct Opt {
  char shrt;
  const char * lng, * descrp;
  int val, min, max;
} Opt;

typedef struct Opts {
  Opt beforefirst;
  Opt abstime;
  Opt acts;
  Opt actavgmax;
  Opt actstdmin;
  Opt actstdmax;
  Opt agile;
  Opt bias;
  Opt block;
  Opt blkrtc;
  Opt blkclslim;
  Opt blkocclim;
  Opt blkmaxeff;
  Opt blkmineff;
  Opt blkreleff;
  Opt card;
  Opt cce;
  Opt ccemaxeff;
  Opt ccemineff;
  Opt ccereleff;
  Opt check;
  Opt cgrclsr;
  Opt cgrmaxority;
  Opt cgrmaxeff;
  Opt cgrmineff;
  Opt cgreleff;
  Opt cgrexteq;
  Opt cgrextand;
  Opt cgrextunits;
  Opt cgrextite;
  Opt cgrextxor;
  Opt cliff;
  Opt cliffreleff;
  Opt cliffmineff;
  Opt cliffmaxeff;
  Opt compact;
  Opt decompose;
  Opt defragint;
  Opt defragfree;
  Opt elim;
  Opt elmrtc;
  Opt elmblk;
  Opt elmclslim;
  Opt elmocclim;
  Opt elmaxeff;
  Opt elmineff;
  Opt elmreleff;
  Opt sleeponabort;
  Opt exitonabort;
  Opt flipping;
  Opt flipint;
  Opt flipdur;
  Opt fliptop;
  Opt force;
  Opt gauss;
  Opt gaussextrall;
  Opt gaussmaxor;
  Opt gaussexptrn;
  Opt gaussmaxeff;
  Opt gaussmineff;
  Opt gaussreleff;
  Opt gluescale;
  Opt gluekeep;
  Opt inprocessing;
  Opt cintinc;
  Opt irrlim;
  Opt lift;
  Opt lftmaxeff;
  Opt lftmineff;
  Opt lftreleff;
  Opt lhbr;
  Opt lkhd;
  Opt clim;
  Opt mocint;
  Opt move;
  Opt log;
  Opt otfs;
  Opt phase;
  Opt phaseneginit;
  Opt plain;
  Opt probe;
  Opt prbasicmaxeff;
  Opt prbasicmineff;
  Opt prbasicreleff;
  Opt prbasic;
  Opt prbasicroundlim;
  Opt queuemergelim;
  Opt queuefactor;
  Opt queueinc;
  Opt rmincpen;
  Opt seed;
  Opt smallirr;
  Opt smallve;
  Opt smallvevars;
  Opt randec;
  Opt randecint;
  Opt redfixed;
  Opt redlbound;
  Opt redlexpfac;
  Opt redldoutfac;
  Opt redloutinc;
  Opt redlinit;
  Opt redlinc;
  Opt redinoutinc;
  Opt redlmininc;
  Opt redlmaxinc;
  Opt redlminrel;
  Opt redlmaxrel;
  Opt redlminabs;
  Opt redlmaxabs;
  Opt reduce;
  Opt restart;
  Opt restartint;
  Opt rstinoutinc;
  Opt simplify;
  Opt simpdelay;
  Opt simpen;
  Opt sizepen;
  Opt sizemaxpen;
  Opt sortlits;
  Opt syncint;
  Opt termint;
  Opt ternres;
  Opt ternresrtc;
  Opt trnrmineff;
  Opt trnrmaxeff;
  Opt trnreleff;
  Opt transred;
  Opt trdmineff;
  Opt trdmaxeff;
  Opt trdreleff;
  Opt unhide;
  Opt unhdextstamp;
  Opt unhdhbr;
  Opt unhdmaxeff;
  Opt unhdmineff;
  Opt unhdreleff;
  Opt unhdlnpr;
  Opt unhdroundlim;
  Opt verbose;
  Opt witness;
  Opt afterlast;
} Opts;

#define FIRSTOPT(lgl) (&(lgl)->opts->beforefirst + 1)
#define LASTOPT(lgl) (&(lgl)->opts->afterlast - 1)

/*------------------------------------------------------------------------*/

typedef int Exp;
typedef uint64_t Mnt;
typedef int64_t Flt;
typedef int64_t Scr;
typedef int64_t Cnf;
typedef uint64_t Fun[FUNQUADS];
typedef signed char Val;
typedef Flt LKHD;

/*------------------------------------------------------------------------*/

typedef struct ASL { int act, size, lidx; } ASL;
typedef struct Conf { int lit, rsn[2]; } Conf;
typedef struct Ctk { struct Ctr * start, * top, * end; } Ctk;
typedef struct DFOPF { int observed, pushed, flag; } DFOPF;
typedef struct DFPR { int discovered, finished, parent, root; } DFPR;
typedef struct EVar { int occ[2], pos, score; } EVar;
typedef struct HTS { int offset, count; }  HTS;
typedef struct ITEC { int other, other2; } ITEC;
typedef struct Lim { int64_t confs, decs; } Lim;
typedef struct PSz { int pos, size; } PSz;
typedef struct Qnd { int prev, next; struct Qln * line; } Qnd;
typedef struct RNG { unsigned z, w; } RNG;
typedef struct Stk { int * start, * top, * end; } Stk;
typedef struct Tmrs { double phase[MAXPHN]; int idx[MAXPHN], nest; } Tmrs;
typedef struct Trv { void * state; void (*trav)(void *, int); } Trv;
typedef struct TVar { signed int val : 30; unsigned mark : 2; } TVar;
typedef struct Wtk { struct Work * start, * top, * end; } Wtk;

/*------------------------------------------------------------------------*/

typedef struct Ctr { 
  signed int decision : 31; 
  unsigned used : 1;
} Ctr;

typedef struct DVar { HTS hts[2]; } DVar;

typedef struct Qln {
  int prior, first, last, unassigned;
  struct Qln * up, * down, * repr;
} Qln;

typedef struct Queue {
  Qln * bottom, * top, * unassigned, * merged, * free;
  Qnd * nodes;
  int nmerged, nlines;
} Queue;

typedef struct TD { signed int level : 30; unsigned lrglue:1; int rsn[2]; } TD;

typedef struct ID { int level, lit, rsn[2]; } ID;

typedef struct Impls { ID * start, * top, * end; } Impls;

typedef struct AVar {
  unsigned type : 4;
#ifndef NDEBUG
  unsigned simp:1, wasfalse:1;
#endif
  unsigned equiv:1, lcamark:4;
  signed int phase:2, bias:2, fase:2;
  unsigned poisoned:1, assumed:2, failed:2, gate:1;
  unsigned donotelm:1, donotblk:1, donotcgrcls:1, donotlft:1, donoternres:1;
  unsigned donotbasicprobe:1, donotcce:1;
  int mark, trail;
} AVar;

typedef struct Ext {
  unsigned equiv:1,melted:1,blocking:2,eliminated:1,tmpfrozen:1,imported:1;
  unsigned assumed:2,failed:2;
  signed int val:2, oldval:2;
  int repr, frozen;
  struct { int count; int64_t sum; } cog;
} Ext;

typedef struct Work {
  unsigned wrag : 2;
  signed int lit : 30, other : 30;
  unsigned red : 1, removed : 1;
} Work;

typedef struct DFL {
  int discovered, finished;
  union { int lit, sign; };
#ifndef NLGLOG
  int lit4logging;
#endif
} DFL;

typedef struct Gat {
  int lhs, minrhs;
  unsigned tag : 2;
  unsigned mark : 1;
  signed int size : 29;
  union {
    int lits[2];
    struct { int * cls, origlhs; };
    struct { int cond, pos, neg; };
  };
} Gat;

/*------------------------------------------------------------------------*/

typedef struct Stats {
  int defrags, iterations, acts, reported, gcs, decomps;
  struct { int64_t count; struct { int max, min; } mincut; } force;
  struct { int clauses; } rescored;
  struct { int count, skipped;
	   struct { int count; int64_t sum; } kept; } restarts;
  struct { int count, reset, geom, arith, arith2; } reduced;
  int64_t prgss, irrprgss, enlwchs, pshwchs, height, dense, sparse;
  int64_t confs, decisions, randecs, flipped, fliphases, uips;
  struct { struct { int cur, max; int64_t add; } clauses, lits; } irr;
  struct { int64_t sat, mosat, simp, deref, fixed, freeze;
	   int64_t melt, add, assume, cassume, failed, repr; } calls;
  struct { int vars;
           struct { int total, pos, neg; } lits;
	   struct { int total, unit, bin, trn, lrg; } clauses; 
	   struct { struct { int min, avg, max; } val, var; } cog;
	 } features;
  struct { int64_t search, hits; } poison;
  struct { int64_t search, simp, lkhd; } props, visits;
  struct { size_t current, max; } bytes;
  struct { int bin, trn, lrg; } red;
  struct { int cnt, trn, lrg, sub; } hbr;
  struct { int current, sum; } fixed, equiv;
  struct { int count, bin, trn; int64_t steps; } trnr;
  struct { int count, clauses, lits, pure; int64_t res, steps; } blk;
  struct { int count, eq, units; int64_t esteps, csteps;
	   struct { int all, and, xor, ite; } matched;
	   struct { int all, and, xor, ite; } simplified;
	   struct { int64_t all, and, xor, ite; } extracted; } cgr;
  struct {
    struct { int count, failed, lifted; int64_t probed, steps; } basic;
  } prb;
  struct { int count, eqs, units, impls; int64_t probed0, probed1; } lift;
  struct { int count, red, failed; int64_t lits, bins, steps; } trd;
  struct { int removed, red; } bindup;
  struct { int count, rounds;
	   struct { int trds, failed, sccs; int64_t sumsccsizes; } stamp;
	   struct { int lits, bin, trn, lrg; } failed;
	   struct { int bin, trn, lrg, red; } tauts;
	   struct { int bin, trn, lrg; } units;
	   struct { int trn, lrg, red; } hbrs;
	   struct { int trn, lrg, red; } str;
	   int64_t steps; } unhd;
  struct {
    int count, elmd, large, sub, str, blkd;
    struct { int elm, tried, failed; } small;
    int64_t resolutions, copies, subchks, strchks, ipos, steps; } elm;
  struct {
    struct { struct { int irr, red; } dyn; } sub, str;
    int driving, restarting; } otfs;
  struct { int64_t nonmin, learned; } lits;
  struct { int64_t learned, glue, nonmaxglue, maxglue, scglue; } clauses;
  struct {
    int clauses;
    int64_t added, reduced, resolved, forcing, conflicts, saved;
  } lir[POW2GLUE];
  struct { int64_t sum; int count; } glues;
  struct { int count; int64_t set, pos, neg; } phase;
  struct { int count, plimhit, ilimhit, climhit; } simp;
  struct { int count; int64_t steps; } luby, inout;
  struct { int64_t new, del, merged, col, gcs;
           struct { int64_t sum; int count; } deprior; int max; } queue;
  struct { int count, gcs, units, equivs, trneqs; 
           struct { int max; int64_t sum; } arity; 
	   struct { int64_t extr, elim; } steps;
	   int64_t extracted; } gauss;
  struct { int count, eliminated, ate, abce, failed, lifted;
           int64_t steps, probed; } cce;
  struct { int count, failed, lifted; int64_t decisions, steps; } cliff;
  struct { int64_t bin, trn; } moved;
} Stats;

/*------------------------------------------------------------------------*/

typedef struct Times {
  double all, dcp, elm, trd, gc, dfg, red, blk, ana, unhd, dec, lkhd;
  double rsts, lft, trn, cgr, phs, srch, prep, inpr, bump, mcls, gauss;
  double card, cce, cliff, ctw, force;
  struct { double all, basic; } prb;
} Times;

/*------------------------------------------------------------------------*/

typedef struct Limits {
  int flipint, lkhdpen;
  int64_t randec;
  struct { int inner, outer, extra; } reduce;
  struct { int pen; int64_t esteps, csteps; } cgr;
  struct { int pen; int64_t steps, irrprgss; } elm, blk, cliff;
  struct { int pen; int64_t steps; } trd, unhd, trnr, lft, cce;
  struct { int pen; struct { int64_t extr, elim; } steps; } gauss;
  struct { int64_t confs; int wasmaxdelta, maxdelta, luby, inout; } restart;
  struct { int64_t steps; struct { int basic; } pen; } prb;
  struct { int64_t irr, prgss, confs, cinc; int pen; } simp;
  struct { int64_t pshwchs, prgss; } dfg;
  struct { int64_t steps; } term, sync;
  struct { int64_t fixed; } gc;
} Limits;

/*------------------------------------------------------------------------*/

typedef struct Cbs {
  struct { int (*fun)(void*); void * state; int done; } term;
  struct {
    struct { void (*fun)(void*,int); void * state; } produce, consumed;
    struct { void(*fun)(void*,int**,int**); void*state; } consume;
  } units;
  struct {
    struct { int * (*fun)(void*); void * state; } lock;
    struct { void (*fun)(void*,int,int); void * state; } unlock;
  } eqs;
  struct { void(*lock)(void*); void (*unlock)(void*); void*state; } msglock;
  double (*getime)(void);
  void (*onabort)(void *); void * abortstate;
} Cbs;

typedef struct Cgr {
  struct { int units, eq, all, and, xor, ite, org; } extracted;
  struct { int all, and, xor, ite, org; } simplified;
  struct { int all, and, xor, ite, org; } matched;
  Stk * goccs, units; Gat * gates; int szgates;
} Cgr;

typedef struct Cliff { Stk lift, lits; } Cliff;

typedef struct Dis { struct { Stk bin, trn; } red, irr; } Dis;

typedef struct Elm {
  int pivot, negcls, necls, neglidx;
  Stk lits, next, clv, csigs, lsigs, sizes, occs, noccs, mark, m2i;
} Elm;

typedef struct FltStr { int current; char str[MAXFLTSTR][100]; } FltStr;

typedef struct SPE { signed int count : 31; unsigned mark : 1, sum; } SPE;

typedef struct Gauss { 
  Stk xors, order, * occs; 
  signed char * eliminated;
  int garbage, next; 
} Gauss;

typedef struct CCE { Stk cla, extend; int * rem; } CCE;

typedef struct Mem {
  void * state;
  lglalloc alloc; lglrealloc realloc; lgldealloc dealloc;
} Mem;

typedef struct Wchs { Stk stk; int start[MAXLDFW], free; } Wchs;

typedef struct Wrk {
  Stk queue;
  int count, head, size, posonly, fifo, * pos;
} Wrk;

/*------------------------------------------------------------------------*/

struct LGL {
  State state;
  int probing, flipping, notflipped, tid, tids, bias, phaseneg;
  int nvars, szvars, maxext, szext, changed, mt;
  int szdrail, bnext, next, next2, flushed, level, alevel;
  int unassigned, lrgluereasons, failed, assumed, cassumed, ncassumed;
  char cgrclosing, searching, simp, allphaseset, flushphases;
  char forked, bruteforked, qscheduling, decomposing;
  char lifting, cceing, gaussing, cliffing;
  char unhiding, basicprobing;
  char eliminating, donotsched, blocking, ternresing, lkhd;
  char blkall, blkrem, elmall, elmrem, cceall, ccerem;
  char frozen, dense, notfullyconnected, forcegc, allowforce;
  unsigned long long flips;
  Conf conf;
  RNG rng;

  Mem * mem;
  Opts * opts;
  Stats * stats;
  Times * times;
  Tmrs * timers;
  Limits * limits;
  Ext * ext;
  int * i2e;
  int * doms;
  DVar * dvars;
  AVar * avars;
  Val * vals;
  Flt * jwh;
  TD * drail;
  Queue queue;
  Stk * red;
  Wchs * wchs;

  Ctk control;
  Stk clause, eclause, extend, irr, trail, frames;
  Stk eassume, assume, cassume, fassume;
#ifndef NCHKSOL
  Stk orig;
#endif

  union { Elm * elm; Cgr * cgr; Gauss * gauss;
          CCE * cce; Cliff * cliff; };
  union { Stk lcaseen, sortstk, resolvent; };
  Stk poisoned, seen, esched;
  EVar * evars;
  Dis * dis;
  Wrk * wrk;
  int * repr;

  char closeapitrace;
  FILE * out, * apitrace;
  char * prefix;
  Cbs * cbs;

  LGL * clone;

  FltStr * fltstr;
#if !defined(NLGLPICOSAT)
  struct { PicoSAT * solver; int res; char chk; } picosat;
#endif
};

/*-------------------------------------------------------------------------*/

#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n

static const char lglfloorldtab[256] =
{
// 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
  -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
  LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
  LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
};

static const uint64_t lglbasevar2funtab[6] = {
  0xaaaaaaaaaaaaaaaaull, 0xccccccccccccccccull, 0xf0f0f0f0f0f0f0f0ull,
  0xff00ff00ff00ff00ull, 0xffff0000ffff0000ull, 0xffffffff00000000ull,
};

/*-------------------------------------------------------------------------*/

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

static int lglceilsqrt32 (int x) {
  int l = 0, m, r, mm, rr;
#ifndef NDEBUG
  int ll = l * l;
#endif
  if (x <= 0) return 0;
  r = 46340; rr = r*r;
  if (x >= rr) return r;
  for (;;) {
    assert (l < r);
    assert (ll < x && x < rr);
    if (r - l == 1) return r;
    m = (l + r)/2;
    mm = m*m;
    if (mm == x) return m;
    if (mm < x) {
      l = m;
#ifndef NDEBUG
      ll = mm;
#endif
    } else r = m, rr = mm;
  }
}

static int lglceilsqrt64 (int x) {
  int64_t l = 0, m, r, mm, rr;
#ifndef NDEBUG
  int64_t ll = l*l;
#endif
  if (x <= 0) return 0;
  r = 3037000499ll; rr = r*r;
  if (x >= rr) return r;
  for (;;) {
    assert (l < r);
    assert (ll < x && x < rr);
    if (r - l == 1) return r;
    m = (l + r)/2;
    mm = m*m;
    if (mm == x) return m;
    if (mm < x) {
      l = m;
#ifndef NDEBUG
      ll = mm;
#endif
    } else r = m, rr = mm;
  }
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
  } else {
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

#ifndef NDEBUG
double lglflt2dbl (Flt a) {
  return lglmnt (a) * pow (2.0, lglexp (a));
}
#endif

static const char * lglflt2str (LGL * lgl, Flt a) {
  double d, e;
  if (a == FLTMIN) return "0";
  if (a == FLTMAX) return "inf";
  d = lglmnt (a);
  d /= 4294967296ll;
  e = lglexp (a);
  e += 32;
  lgl->fltstr->current++;
  if (lgl->fltstr->current == MAXFLTSTR) lgl->fltstr->current = 0;
  sprintf (lgl->fltstr->str[lgl->fltstr->current], "%.6fd%+03.0f", d, e);
  return lgl->fltstr->str[lgl->fltstr->current];
}

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

/*------------------------------------------------------------------------*/

static void lglwrn (LGL * lgl, const char * msg, ...) {
  va_list ap;
  fprintf (lgl->out, "*** warning in '%s': ", __FILE__);
  va_start (ap, msg);
  vfprintf (lgl->out, msg, ap);
  va_end (ap);
  fputc ('\n', lgl->out);
  fflush (lgl->out);
}

static void lgldie (LGL * lgl, const char * msg, ...) {
  va_list ap;
  fprintf (lgl->out, "*** internal error in '%s': ", __FILE__);
  va_start (ap, msg);
  vfprintf (lgl->out, msg, ap);
  va_end (ap);
  fputc ('\n', lgl->out);
  fflush (lgl->out);
  exit (0);
}

static void lglabort (LGL * lgl) {
  if (!lgl) exit (1);
  if (lgl->opts && lgl->opts->sleeponabort.val) {
    fprintf (stderr,
"liblgl.a: Process %d will sleep for %d seconds "
" before continuing with 'lglabort' procedure.\n",
      getpid (), lgl->opts->sleeponabort.val);
    sleep (lgl->opts->sleeponabort.val);
  }
  if (lgl->cbs && lgl->cbs->onabort)
    lgl->cbs->onabort (lgl->cbs->abortstate);
  if (lgl->opts && lgl->opts->exitonabort.val) exit (1);
  abort ();
}

static const char * lglprefix (LGL * lgl) {
  return lgl && lgl->prefix ? lgl->prefix : "c (LGL HAS NO PREFIX YET) ";
}

static int lglmsgstart (LGL * lgl, int level) {
  if (lgl->opts->verbose.val < level) return 0;
  if (lgl->cbs && lgl->cbs->msglock.lock)
    lgl->cbs->msglock.lock (lgl->cbs->msglock.state);
  fputs (lglprefix (lgl), lgl->out);
  if (lgl->tid >= 0) fprintf (lgl->out, "%d ", lgl->tid);
  return 1;
}

static void lglmsgend (LGL * lgl) {
  fputc ('\n', lgl->out);
  fflush (lgl->out);
  if (lgl->cbs && lgl->cbs->msglock.unlock)
    lgl->cbs->msglock.unlock (lgl->cbs->msglock.state);
}

static void lglprt (LGL * lgl, int level, const char * msg, ...) {
  va_list ap;
  if (lgl->opts->verbose.val < level) return;
  lglmsgstart (lgl, level);
  va_start (ap, msg);
  vfprintf (lgl->out, msg, ap);
  va_end (ap);
  lglmsgend (lgl);
}

#ifndef NLGLOG
static void lglogstart (LGL * lgl, int level, const char * msg, ...) {
  va_list ap;
  assert (lgl->opts->log.val >= level);
  if (lgl->cbs && lgl->cbs->msglock.lock)
    lgl->cbs->msglock.lock (lgl->cbs->msglock.state);
  fputs (lglprefix (lgl), lgl->out);
  if (lgl->tid >= 0) fprintf (lgl->out, "%d ", lgl->tid);
  fprintf (lgl->out, "LOG%d %d ", level, lgl->level);
  va_start (ap, msg);
  vfprintf (lgl->out, msg, ap);
  va_end (ap);
}

#define lglogend lglmsgend
#endif

/*------------------------------------------------------------------------*/

void lglsetid (LGL * lgl, int tid, int tids) {
  REQINIT ();
  ABORTIF (tid < 0, "negative id");
  ABORTIF (tid >= tids, "id exceed number of ids");
  lgl->tid = tid;
  lgl->tids = tids;
}

/*------------------------------------------------------------------------*/

static void lglinc (LGL * lgl, size_t bytes) {
  lgl->stats->bytes.current += bytes;
  if (lgl->stats->bytes.max < lgl->stats->bytes.current) {
    lgl->stats->bytes.max = lgl->stats->bytes.current;
    LOG (5, "maximum allocated %ld bytes", lgl->stats->bytes.max);
  }
}

static void lgldec (LGL * lgl, size_t bytes) {
  assert (lgl->stats->bytes.current >= bytes);
  lgl->stats->bytes.current -= bytes;
}

static void * lglnew (LGL * lgl, size_t bytes) {
  void * res;
  if (!bytes) return 0;
  if (lgl->mem->alloc) res = lgl->mem->alloc (lgl->mem->state, bytes);
  else res = malloc (bytes);
  if (!res) lgldie (lgl, "out of memory allocating %ld bytes", bytes);
  assert (res);
  LOG (5, "allocating %p with %ld bytes", res, bytes);
  lglinc (lgl, bytes);
  if (res) memset (res, 0, bytes);
  return res;
}

static void lgldel (LGL * lgl, void * ptr, size_t bytes) {
  if (!ptr) { assert (!bytes); return; }
  lgldec (lgl, bytes);
  LOG (5, "freeing %p with %ld bytes", ptr, bytes);
  if (lgl->mem->dealloc) lgl->mem->dealloc (lgl->mem->state, ptr, bytes);
  else free (ptr);
}

static void * lglrsz (LGL * lgl, void * ptr, size_t old, size_t new) {
  void * res;
  assert (!ptr == !old);
  if (!ptr) return lglnew (lgl, new);
  if (!new) { lgldel (lgl, ptr, old); return 0; }
  if (old == new) return ptr;
  lgldec (lgl, old);
  if (lgl->mem->realloc)
    res = lgl->mem->realloc (lgl->mem->state, ptr, old, new);
  else res = realloc (ptr, new);
  if (!res)
    lgldie (lgl, "out of memory reallocating %ld to %ld bytes", old, new);
  assert (res);
  LOG (5, "reallocating %p to %p from %ld to %ld bytes", ptr, res, old, new);
  lglinc (lgl, new);
  if (new > old) memset (res + old, 0, new - old);
  return res;
}

/*------------------------------------------------------------------------*/

static char * lglstrdup (LGL * lgl, const char * str) {
  char * res;
  NEW (res, strlen (str) + 1);
  return strcpy (res, str);
}

static void lgldelstr (LGL * lgl, char * str) {
  DEL (str, strlen (str) + 1);
}

/*------------------------------------------------------------------------*/

static void lglinitcbs (LGL * lgl) {
  if (!lgl->cbs) NEW (lgl->cbs, 1);
}

void lglonabort (LGL * lgl, void * abortstate, void (*onabort)(void*)) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->abortstate = abortstate;
  lgl->cbs->onabort = onabort;
}

void lglseterm (LGL * lgl, int (*fun)(void*), void * state) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->term.fun = fun;
  lgl->cbs->term.state = state;
}

void lglsetproduceunit (LGL * lgl, void (*fun) (void*, int), void * state) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->units.produce.fun = fun;
  lgl->cbs->units.produce.state = state;
}

void lglsetconsumeunits (LGL * lgl,
			 void (*fun) (void*, int **, int **),
			 void * state) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->units.consume.fun =  fun;
  lgl->cbs->units.consume.state = state;
}

void lglsetlockeq (LGL * lgl, int * (*fun)(void*), void * state) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->eqs.lock.fun = fun;
  lgl->cbs->eqs.lock.state = state;
}

void lglsetunlockeq (LGL * lgl, void (*fun)(void*,int,int), void * state) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->eqs.unlock.fun = fun;
  lgl->cbs->eqs.unlock.state = state;
}

void lglsetconsumedunits (LGL * lgl,
			  void (*fun) (void*, int), void * state) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->units.consumed.fun = fun;
  lgl->cbs->units.consumed.state = state;
}

void lglsetmsglock (LGL * lgl,
		    void (*lock)(void*), void (*unlock)(void*),
		    void * state) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->msglock.lock = lock;
  lgl->cbs->msglock.unlock = unlock;
  lgl->cbs->msglock.state = state;
}

void lglsetime (LGL * lgl, double (*time)(void)) {
  REQINIT ();
  lglinitcbs (lgl);
  lgl->cbs->getime = time;
}

/*------------------------------------------------------------------------*/

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
  CLRPTR (s);
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

/*------------------------------------------------------------------------*/

static void lgltrapi (LGL * lgl, const char * msg, ...) {
  va_list ap;
  assert (lgl->apitrace);
  va_start (ap, msg);
  vfprintf (lgl->apitrace, msg, ap);
  va_end (ap);
  fputc ('\n', lgl->apitrace);
}

static void lglopenapitrace (LGL * lgl, const char * name) {
  FILE * file;
  char * cmd;
  int len;
  len = strlen (name);
  if (len >= 3 && !strcmp (name + len - 3, ".gz")) {
    len += 20;
    NEW (cmd, len);
    sprintf (cmd, "gzip -c > %s", name);
    file = popen (cmd, "w");
    DEL (cmd, len);
    if (file) lgl->closeapitrace = 2;
  } else {
    file = fopen (name, "w");
    if (file) lgl->closeapitrace = 1;
  }
  if (file) lgl->apitrace = file;
  else lglwrn (lgl, "can not write API trace to '%s'", name);
  TRAPI ("init");
}

void lglwtrapi (LGL * lgl, FILE * apitrace) {
  REQUIRE (UNUSED);
  ABORTIF (lgl->apitrace, "can only write one API trace");
  lgl->apitrace = apitrace;
  TRAPI ("init");
}

/*------------------------------------------------------------------------*/
#if !defined(NLGLPICOSAT) && !defined(NDEBUG)

static void lglpicosatinit (LGL * lgl) {
  if (lgl->picosat.solver) return;
  lgl->picosat.solver = picosat_init ();
  picosat_set_prefix (lgl->picosat.solver, "c PST ");
  lgl->picosat.chk = 1;
  picosat_add (lgl->picosat.solver, 1);
  picosat_add (lgl->picosat.solver, 0);
  LOG (1, "PicoSAT initialized");
}

#endif
/*------------------------------------------------------------------------*/

static unsigned lglrand (LGL * lgl) {
  unsigned res;
  lgl->rng.z = 36969 * (lgl->rng.z & 65535) + (lgl->rng.z >> 16);
  lgl->rng.w = 18000 * (lgl->rng.w & 65535) + (lgl->rng.w >> 16);
  res = (lgl->rng.z << 16) + lgl->rng.w;
  LOG (5, "rng %u", res);
  return res;
}

/*------------------------------------------------------------------------*/

static int lglfullctk (Ctk * ctk) { return ctk->top == ctk->end; }

static int lglsizectk (Ctk * ctk) { return ctk->end - ctk->start; }

static int lglcntctk (Ctk * ctk) { return ctk->top - ctk->start; }

static void lglrelctk (LGL * lgl, Ctk * ctk) {
  DEL (ctk->start, lglsizectk (ctk));
  memset (ctk, 0, sizeof *ctk);
}

static void lglenlctk (LGL * lgl, Ctk * ctk) {
  int oldsize = lglsizectk (ctk);
  int newsize = oldsize ? 2*oldsize : 1;
  int count = lglcntctk (ctk);
  RSZ (ctk->start, oldsize, newsize);
  ctk->top = ctk->start + count;
  ctk->end = ctk->start + newsize;
}

static void lglpushcontrol (LGL * lgl, int decision) {
  Ctk * ctk = &lgl->control;
  Ctr * ctr;
  if (lglfullctk (ctk)) lglenlctk (lgl, ctk);
  ctr = ctk->top++;
  ctr->decision = decision;
  ctr->used = 0;
}

static void lglpopcontrol (LGL * lgl) {
  assert (lgl->control.top > lgl->control.start);
  --lgl->control.top;
}

static void lglrstcontrol (LGL * lgl, int count) {
  while (lglcntctk (&lgl->control) > count)
    lglpopcontrol (lgl);
}

static int lglevelused (LGL * lgl, int level) {
  Ctk * ctk = &lgl->control;
  Ctr * ctr;
  assert (0 < level && level < lglcntctk (ctk));
  ctr = ctk->start + level;
  return ctr->used;
}

static void lgluselevel (LGL * lgl, int level) {
  Ctk * ctk = &lgl->control;
  Ctr * ctr;
  assert (0 < level && level < lglcntctk (ctk));
  ctr = ctk->start + level;
  ctr->used = 1;
}

static void lglunuselevel (LGL * lgl, int level) {
  Ctk * ctk = &lgl->control;
  Ctr * ctr;
  assert (0 < level && level < lglcntctk (ctk));
  ctr = ctk->start + level;
  assert (ctr->used);
  ctr->used = 0;
}

/*------------------------------------------------------------------------*/

static void lglgetenv (LGL * lgl, Opt * opt, const char * lname) {
  const char * q, * valstr;
  char uname[40], * p;
  int newval, oldval;
  assert (strlen (lname) + 3 + 1 < sizeof (uname));
  uname[0] = 'L'; uname[1] = 'G'; uname[2] = 'L';
  p = uname + 3;
  for (q = lname; *q; q++) {
    assert (p < uname + sizeof uname);
    *p++ = toupper (*q);
  }
  assert (p < uname + sizeof uname);
  *p = 0;
  valstr = getenv (uname);
  if (!valstr) return;
  oldval = opt->val;
  newval = atoi (valstr);
  if (newval < opt->min) newval = opt->min;
  if (newval > opt->max) newval = opt->max;
  if (newval == oldval) return;
  opt->val = newval;
  TRAPI ("option %s %d", lname, newval);
  COVER (lgl->clone);
  if (lgl->clone) lglsetopt (lgl->clone, lname, newval);
}

static void lglchkenv (LGL * lgl) {
  extern char ** environ;
  char * src, *eos, * dst;
  char ** p, * s, * d;
  int len;
  for (p = environ; (src = *p); p++) {
    if (src[0] != 'L' || src[1] != 'G' || src[2] != 'L') continue;
    for (eos = src; *eos && *eos != '='; eos++)
      ;
    len = eos - (src + 3);
    NEW (dst, len + 1);
    d = dst;
    for (s = src + 3; s < eos; s++) *d++ = tolower (*s);
    *d = 0;
    if (!lglhasopt (lgl, dst) && strcmp (dst, "apitrace"))
      lglwrn (lgl, "invalid 'LGL...' environment '%s'", src);
    DEL (dst, len + 1);
  }
}

static void lglsetplain (LGL * lgl, int val) {
  lgl->opts->block.val = !val;
  lgl->opts->card.val = !val;
  lgl->opts->cce.val = !val;
  lgl->opts->cliff.val = !val;
  lgl->opts->cgrclsr.val = !val;
  lgl->opts->decompose.val = !val;
  lgl->opts->elim.val = !val;
  lgl->opts->gauss.val = !val;
  lgl->opts->lift.val = !val;
  lgl->opts->probe.val = !val;
  lgl->opts->ternres.val = !val;
  lgl->opts->transred.val = !val;
  lgl->opts->unhide.val = !val;
  lglprt (lgl, 1, "[plain] plain solving switched %s", val ? "on" : "off");
}

static LGL * lglnewlgl (void * mem,
			lglalloc alloc,
			lglrealloc realloc,
			lgldealloc dealloc) {
  LGL * lgl = alloc ? alloc (mem, sizeof *lgl) : malloc (sizeof *lgl);
  ABORTIF (!lgl, "out of memory allocating main solver object");
  CLRPTR (lgl);

  lgl->mem = alloc ? alloc (mem, sizeof *lgl->mem) : malloc (sizeof *lgl->mem);
  ABORTIF (!lgl->mem, "out of memory allocating memory manager object");
  lgl->mem->state = mem;
  lgl->mem->alloc = alloc;
  lgl->mem->realloc = realloc;
  lgl->mem->dealloc = dealloc;

  lgl->opts = alloc ? alloc (mem, sizeof (Opts)) : malloc (sizeof (Opts));
  ABORTIF (!lgl->opts, "out of memory allocating option manager object");
  CLRPTR (lgl->opts);

  lgl->stats = alloc ? alloc (mem, sizeof (Stats)) : malloc (sizeof (Stats));
  ABORTIF (!lgl->stats, "out of memory allocating statistic counters");
  CLRPTR (lgl->stats);

  lglinc (lgl, sizeof *lgl);
  lglinc (lgl, sizeof *lgl->mem);
  lglinc (lgl, sizeof *lgl->opts);
  lglinc (lgl, sizeof *lgl->stats);

  return lgl;
}

LGL * lglminit (void * mem,
		lglalloc alloc,
		lglrealloc realloc,
		lgldealloc dealloc) {
  const int K = 1000, M = K*K, I = INT_MAX;
  const char * apitracename;
  LGL * lgl;
  int i;

  lgl = 0;
  ABORTIF (!alloc+!realloc+!dealloc != 0 && !alloc+!realloc+!dealloc != 3,
	   "inconsistent set of external memory handlers");

  assert (sizeof (long) == sizeof (void*));

  assert (REMOVED > ((MAXVAR << RMSHFT) | MASKCS | REDCS));
  assert (REMOVED > MAXREDLIDX);
  assert (REMOVED > MAXIRRLIDX);

  assert (MAXREDLIDX == MAXIRRLIDX);
  assert (GLUESHFT == RMSHFT);

  assert (INT_MAX > ((MAXREDLIDX << GLUESHFT) | GLUEMASK));
  assert (INT_MAX > ((MAXIRRLIDX << RMSHFT) | MASKCS | REDCS));

  assert (MAXGLUE < POW2GLUE);

  lgl = lglnewlgl (mem, alloc, realloc, dealloc);
  lgl->tid = -1;

  lglpushcontrol (lgl, 0);
  assert (lglcntctk (&lgl->control) == lgl->level + 1);

  lgl->out = stdout;
  lgl->prefix = lglstrdup (lgl, "c ");

  apitracename = getenv ("LGLAPITRACE");
  if (apitracename) lglopenapitrace (lgl, apitracename);

  OPT(0,abstime,0,0,1,"print absolute time when reporting");
  OPT(0,acts,2,0,2,"activity based reduction: 0=disable,1=enable,2=dyn");
  OPT(0,actavgmax,120,0,200,"glue average max limit for dyn acts");
  OPT(0,actstdmin,20,0,200,"glue standard deviation min limit for dyn acts");
  OPT(0,actstdmax,80,0,200,"glue standard deviation max limit for dyn acts");
  OPT(0,agile,23,0,100,"agility limit for restarts");
  OPT(0,bias,2,-1,2,"decision order initial bias (0=nobias,2=cutwidth)");
  OPT(0,block,1,0,1,"blocked clause elimination (BCE)");
  OPT(0,blkrtc,0,0,1,"run BCE until completion");
  OPT(0,blkclslim,2000,3,I,"max blocked clause size");
  OPT(0,blkocclim,2000,3,I,"max occurrences in blocked clause elimination");
  OPT(0,blkmaxeff,I,-1,I,"max effort in BCE (-1=unlimited)");
  OPT(0,blkmineff,10*M,0,I,"min effort in BCE");
  OPT(0,blkreleff,20,0,K,"rel effort in BCE");
  OPT(0,card,0,0,1,"extract cardinality constraints");
  OPT(0,cce,3,0,3,"covered clause elimination (1=ate,2=abce,3=cce)");
  OPT(0,ccemaxeff,100*M,-1,I,"max effort in covered clause elimination");
  OPT(0,ccemineff,5*M,0,I,"min effort in covered clause elimination");
  OPT(0,ccereleff,3,0,K,"rel effort in covered clause elimination");
  OPT('c',check,0,0,3,"check level");
  OPT(0,cgrclsr,1,0,1,"gate extraction and congruence closure");
  OPT(0,cgrmaxority,20,2,30,"maximum xor arity to be extracted");
  OPT(0,cgrmaxeff,8*M,-1,I,"max effort in congruence closure");
  OPT(0,cgrmineff,200*K,0,I,"min effort in congruence closure");
  OPT(0,cgreleff,1,0,10*K,"rel effort in congruence closure");
  OPT(0,cgrexteq,1,0,1,"extract equivalences");
  OPT(0,cgrextand,1,0,1,"extract and gates");
  OPT(0,cgrextxor,1,0,1,"extract xor gates");
  OPT(0,cgrextite,1,0,1,"extract ite gates");
  OPT(0,cgrextunits,1,0,1,"extract units");
  OPT(0,cliff,1,0,1,"cliffing");
  OPT(0,cliffmaxeff,100*M,-1,I,"max effort in cliffing");
  OPT(0,cliffmineff,10*M,0,I,"min effort in cliffing");
  OPT(0,cliffreleff,8,0,10*K,"rel effort in cliffing");
  OPT(0,compact,0,0,2,"compactify after 'lglsat' + 'lglsimp' (1=UNS,2=SAT)");
  OPT(0,decompose,1,0,1,"enable decompose");
  OPT(0,defragfree,50,10,K,"defragmentation free watches limit");
  OPT(0,defragint,10*M,100,I,"defragmentation pushed watches interval");
  OPT(0,elim,1,0,1,"bounded variable eliminiation (BVE)");
  OPT(0,elmrtc,0,0,1,"run BVE until completion");
  OPT(0,elmblk,1,0,1,"enable BCE during BVE");
  OPT(0,elmclslim,1000,3,I,"max antecendent size in elimination");
  OPT(0,elmocclim,1000,3,I,"max occurences in elimination");
  OPT(0,elmaxeff,100*M,-1,I,"max effort in BVE (-1=unlimited)");
  OPT(0,elmineff,5*M,0,I,"min effort in BVE");
  OPT(0,elmreleff,10,0,10*K,"rel effort in BVE");
  OPT(0,sleeponabort,0,0,I,"sleep this seconds before abort/exit");
  OPT(0,exitonabort,0,0,1,"exit instead abort after internal error");
  OPT(0,flipping,1,0,1,"enable point flipping");
  OPT(0,flipint,2,0,I,"flipping interval in number of top level decision");
  OPT(0,flipdur,50,1,I,"flipping duration in number of conflicts");
  OPT(0,fliptop,1,0,1,"flipping only at the top level");
  OPT(0,force,0,0,I,"reorder variables with force algorithm");
  OPT(0,gauss,1,0,1,"gaussion elimination");
  OPT(0,gaussextrall,1,0,1,"extract all xors (with duplicates)");
  OPT(0,gaussmaxor,20,2,64,"maximum xor size in gaussian elimination");
  OPT(0,gaussexptrn,1,0,1,"export ternary clauses from gaussian elimination");
  OPT(0,gaussmaxeff,50*M,-1,I,"max effort in gaussian elimination");
  OPT(0,gaussmineff,2*M,0,I,"min effort in gaussian elimination");
  OPT(0,gaussreleff,2,0,10*K,"rel effort in gaussian elimination");
  OPT(0,gluescale,2,1,3,"glue scaling: 1=linear,2=sqrt,3=ld");
  OPT(0,gluekeep,0,0,I,"keep clauses with this original glue");
  OPT(0,inprocessing,1,0,1,"enable inprocessing");
  OPT(0,cintinc,10*K,10,M,"inprocessing conflict interval increment");
  OPT(0,irrlim,20,1,200,"general irredundant added literals limit");
  OPT(0,lift,1,0,1,"enable double lookahead lifting");
  OPT(0,lftmaxeff,20*M,-1,I,"max effort in lifting");
  OPT(0,lftmineff,500*K,0,I,"min effort in lifting");
  OPT(0,lftreleff,6,0,10*K,"rel effort in lifting");
  OPT(0,lhbr,1,0,1, "enable lazy hyber binary reasoning");
  OPT(0,lkhd,1,0,1, "0=LIS,1=JWH");
  OPT(0,clim,-1,-1,I,"conflict limit");
  OPT(0,mocint,1000,1,I,"multiple objectives conflict limit interval");
  OPT(0,move,2,0,2,"move redundant clauses (1=only binary)");
  OPT('l',log,-1,-1,5,"log level");
  OPT(0,otfs,1,0,1,"enable on-the-fly subsumption");
  OPT(0,phase,0,-1,1,"default phase (-1=neg,0=JeroslowWang,1=pos)");
  OPT(0,phaseneginit,0,0,I,"initial zero phase conflict interval");
  OPT(0,plain,0,0,1,"plain mode disables all preprocessing");
  OPT(0,probe,1,0,1,"enable probing");
  OPT(0,prbasic,1,0,2,"enable basic probing procedure (1=roots only)");
  OPT(0,prbasicroundlim,9,1,I,"basic probing round limit");
  OPT(0,prbasicmaxeff,100*M,-1,I,"max effort in basic probing");
  OPT(0,prbasicmineff,M,0,I,"min effort in basic probing");
  OPT(0,prbasicreleff,10,0,10*K,"rel effort in basic probing");
  OPT(0,queuemergelim,10000,1,I,"flush limit on garbage merged queue lines");
  OPT(0,queuefactor,833,1,999,"queue unbump factor in per mille");
  OPT(0,queueinc,20,1,1000,"queue bump increment");
  OPT(0,rmincpen,4,0,32,"logarithm of watcher removal penalty");
  OPT(0,seed,0,0,I,"random number generator seed");
  OPT(0,smallirr,90,0,100,"max percentage irr lits for BCE and VE");
  OPT(0,smallve,1,0,1,"enable small number variables elimination");
  OPT(0,smallvevars,FUNVAR,4,FUNVAR, "variables small variable elimination");
  OPT(0,randec,1,0,1,"enable random decisions");
  OPT(0,randecint,1000,2,I/2,"random decision interval");
  OPT(0,reduce,1,0,4,"clause reduction (1=noouter,2=luby,3=inout,4=arith)");
  OPT(0,redfixed,0,0,1,"keep a fixed size of learned clauses");
  OPT(0,redlbound,0,0,1,"relative and absolute bounds on learned clauses");
  OPT(0,redlexpfac,10,0,1000,"exponential reduce limit increment factor");
  OPT(0,redldoutfac,0,0,32,"outer to inner factor");
  OPT(0,redloutinc,10000,0,1000000,"outer arithmetic reduce increment");
  OPT(0,redlinit,1*K,1,100*M,"initial reduce limit");
  OPT(0,redlinc,1000,1,10*M,"reduce limit increment");
  OPT(0,redinoutinc,100,1,1000,"reduce inner/outer relative increment");
  OPT(0,redlmininc,10,1,100*K,"rel min reduce limit increment");
  OPT(0,redlmaxinc,200,1,100*K,"rel max reduce limit increment");
  OPT(0,redlminrel,10,10,1000,"minimum relative reduce limit");
  OPT(0,redlmaxrel,300,10,10000,"maximum relative reduce limit");
  OPT(0,redlminabs,500,10,1000000,"minimum absolute reduce limit");
  OPT(0,redlmaxabs,1000000,10,I/2,"maximum absolute reduce limit");
  OPT(0,restart,2,0,3,"enable restarting (0=no,1=fixed,2=luby,3=inout)");
  OPT(0,restartint,5,1,I,"restart interval");
  OPT(0,rstinoutinc,110,1,1000,"restart inner/outer relative increment");
  OPT(0,simplify,1,0,1,"enable simplification");
  OPT(0,simpdelay,100,0,INT_MAX,"delay simplification");
  OPT(0,simpen,4,0,24,"logarithmic initial simplification penalty");
  OPT(0,sizepen,1*M,1,I,"number of literals size penalty starting point");
  OPT(0,sizemaxpen,3,0,20,"maximum logarithmic size penalty");
  OPT(0,sortlits,0,0,1,"sort literals of clauses during garbage collection");
  OPT(0,syncint,111111,0,M,"unit synchronization interval");
  OPT(0,termint,122222,0,M,"termination check interval");
  OPT(0,ternres,1,0,1,"generate ternary resolvents");
  OPT(0,ternresrtc,0,0,1,"run ternary resolvents until completion");
  OPT(0,trnrmaxeff,40*M,-1,I,"max effort in ternary resolutions");
  OPT(0,trnrmineff,4*M,0,I,"min effort in ternary resolutions");
  OPT(0,trnreleff,10,0,K,"rel effort in ternary resolutions");
  OPT(0,transred,1,0,1,"enable transitive reduction");
  OPT(0,trdmaxeff,2*M,-1,I,"max effort in transitive reduction");
  OPT(0,trdmineff,100*K,0,I,"min effort in transitive reduction");
  OPT(0,trdreleff,10,0,10*K,"rel effort in transitive reduction");
  OPT(0,unhide,1,0,1,"enable unhiding");
  OPT(0,unhdextstamp,1,0,1,"used extended stamping features");
  OPT(0,unhdhbr,0,0,1,"enable unhiding hidden binary resolution");
  OPT(0,unhdmaxeff,20*M,-1,I,"max effort in unhiding");
  OPT(0,unhdmineff,1*M,0,I,"min effort in unhiding");
  OPT(0,unhdreleff,4,0,10*K,"rel effort in unhiding");
  OPT(0,unhdlnpr,3,0,I,"unhide no progress round limit");
  OPT(0,unhdroundlim,5,0,100,"unhide round limit");
  OPT('v',verbose,0,0,3,"verbosity level");
  OPT(0,witness,1,0,1,"print witness");

  if (lgl->opts->plain.val) lglsetplain (lgl, 1);

  if (abs (lgl->opts->bias.val) <= 1) lgl->bias = lgl->opts->bias.val;
  else lgl->bias = 1;

  NEW (lgl->times, 1);
  NEW (lgl->timers, 1);
  NEW (lgl->limits, 1);
  NEW (lgl->fltstr, 1);
  NEW (lgl->red, MAXGLUE+1);
  NEW (lgl->wchs, 1);
  for (i = 0; i < MAXLDFW; i++) lgl->wchs->start[i] = INT_MAX;
  lglpushstk (lgl, &lgl->wchs->stk, INT_MAX);
  lglpushstk (lgl, &lgl->wchs->stk, INT_MAX);

  TRANS (UNUSED);

  return lgl;
}

static Qnd * lglqnd (LGL * lgl, int lit) {
  int idx = abs (lit);
  assert (2 <= idx && idx < lgl->nvars);
  return lgl->queue.nodes + idx;
}

static void lglqclone (LGL * lgl, LGL * orig) {
  LGL * to = lgl, * from = orig;
  Qln * pl, * fl, * tl;
  Qnd * fn, * tn;
  int idx;
  CLONE (queue.nodes, from->szvars);
  if (!lgl->qscheduling) return;
  assert (!to->queue.free);
  pl = 0;
  for (fl = from->queue.bottom; fl; fl = fl->up) {
    assert (fl->repr == fl);
    NEW (tl, 1);
    tl->prior = fl->prior;
    tl->first = fl->first;
    tl->last = fl->last;
    tl->unassigned = fl->unassigned;
    fl->down = tl;
    if (pl) pl->up = tl; else to->queue.bottom = tl;
    tl->down = pl;
    tl->repr = tl;
    pl = tl;
  }
  to->queue.top = pl;
  pl = 0;
  for (fl = from->queue.merged; fl; fl = fl->up) {
    assert (!fl->down);
    assert (fl->repr);
    assert (fl->repr != fl);
    NEW (tl, 1);
    fl->down = tl;
    if (pl) pl->up = tl; else to->queue.merged = tl;
    pl = tl;
  }
  for (fl = from->queue.merged; fl; fl = fl->up) {
    assert (fl->repr);
    assert (fl->repr != fl);
    tl = fl->down;

    /* There was a strange bug here which I could not root
     * down.  The fix was this conditional assignment,
     * which however does not make sense.  I still keep it
     * as a bad style of defensive programming.
     *
     * TODO remove.
     */
    COVER (!fl->repr);

    /* Clang reports this warning, which I think is
     * spurious.
     *
     * TODO remove.
     */
    COVER (!tl);

    tl->repr = fl->repr ? fl->repr->down : 0;
  }
  for (idx = 2; idx < from->nvars; idx++) {
    fn = lglqnd (from, idx);
    tn = lglqnd (to, idx);
    assert (fn->line);
    tn->line = fn->line->down;
  }
  pl = 0;
  for (fl = from->queue.bottom; fl; fl = fl->up) {
    if (fl == from->queue.unassigned) to->queue.unassigned = fl->down;
    fl->down = pl;
    pl = fl;
  }
  assert (pl == from->queue.top);
  for (fl = from->queue.merged; fl; fl = fl->up)
    fl->down = 0;

  to->queue.nmerged = from->queue.nmerged;
  to->queue.nlines = from->queue.nlines;

  for (fl = from->queue.free; fl; fl = fl->up) {
    NEW (tl, 1);
    tl->up = to->queue.free;
    to->queue.free = tl;
  }
}

static void lglcompact (LGL *);

LGL * lglclone (LGL * orig) {
  size_t max_bytes, current_bytes;
  LGL * lgl = orig;
  int glue;

  if (!orig) return 0;

  lglcompact (orig);

  LOG (1, "cloning");

  lgl = lglnewlgl (orig->mem->state,
		   orig->mem->alloc,
		   orig->mem->realloc,
		   orig->mem->dealloc);

  memcpy (lgl, orig, ((char*)&orig->mem) - (char*) orig);

  max_bytes = lgl->stats->bytes.max;
  current_bytes = lgl->stats->bytes.current;
  memcpy (lgl->stats, orig->stats, sizeof *orig->stats);
  lgl->stats->bytes.current = current_bytes;
  lgl->stats->bytes.max = max_bytes;

  memcpy (lgl->opts, orig->opts, sizeof *orig->opts);

  lgl->out = orig->out;
  lgl->prefix = lglstrdup (lgl, orig->prefix);

  if (orig->cbs) {
    lglinitcbs (lgl);
    if (orig->cbs->onabort) {
      lgl->cbs->abortstate = orig->cbs->abortstate;
      lgl->cbs->onabort = orig->cbs->onabort;
    }
    if (orig->cbs->getime) lgl->cbs->getime = orig->cbs->getime;
  }

  CLONE (limits, 1);
  CLONE (times, 1);
  CLONE (timers, 1);		assert (!lgl->timers->nest);
  CLONE (fltstr, 1);
  CLONE (ext, orig->szext);
  CLONE (i2e, orig->szvars);
  CLONE (doms, 2*orig->szvars);
  CLONE (dvars, orig->szvars);
  CLONE (avars, orig->szvars);
  CLONE (vals, orig->szvars);
  CLONE (jwh, 2*orig->szvars);
  CLONE (drail, orig->szdrail);

  lglqclone (lgl, orig);

  NEW (lgl->red, MAXGLUE+1);
  for (glue = 0; glue <= MAXGLUE; glue++) CLONESTK (red[glue]);

  NEW (lgl->wchs, 1);
  memcpy (lgl->wchs, orig->wchs, sizeof *orig->wchs);
  CLONESTK (wchs->stk);

  CLONESTK (control);
  CLONESTK (clause);
  CLONESTK (eclause);
  CLONESTK (extend);
  CLONESTK (irr);
  CLONESTK (trail);
  CLONESTK (frames);
  CLONESTK (eassume);
  CLONESTK (assume);
  CLONESTK (fassume);
  CLONESTK (cassume);
#ifndef NCHKSOL
  CLONESTK (orig);
#endif
#ifndef NDEBUG
  {
    const char * p;
    for (p = (char*)&orig->elm; p < (char*)(&orig->repr+1); p++) assert (!*p);
  }
#endif
  assert (lgl->stats->bytes.current == orig->stats->bytes.current);
  assert (lgl->stats->bytes.max <= orig->stats->bytes.max);
  lgl->stats->bytes.max = orig->stats->bytes.max;
  return lgl;
}

void lglchkclone (LGL * lgl) {
  REQINIT ();
  TRAPI ("chkclone");
#ifdef NLGLPICOSAT
  if (lgl->clone) lglrelease (lgl->clone);
  lgl->clone = lglclone (lgl);
#endif
}

LGL * lglinit (void) { return lglminit (0, 0, 0, 0); }

static int lglmaxoptnamelen (LGL * lgl) {
  int res = 0, len;
  Opt * o;
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++)
    if ((len = strlen (o->lng)) > res)
      res = len;
  return res;
}

void lglusage (LGL * lgl) {
  char fmt[20];
  int len;
  Opt * o;
  REQINIT ();
  len = lglmaxoptnamelen (lgl);
  sprintf (fmt, "--%%-%ds", len);
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    if (o->shrt) fprintf (lgl->out, "-%c|", o->shrt);
    else fprintf (lgl->out, "   ");
    fprintf (lgl->out, fmt, o->lng);
    fprintf (lgl->out, " %s [%d]\n", o->descrp, o->val);
  }
}

void lglopts (LGL * lgl, const char * prefix, int ignsome) {
  Opt * o;
  REQINIT ();
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    if (ignsome) {
      if (!strcmp (o->lng, "check")) continue;
      if (!strcmp (o->lng, "log")) continue;
      if (!strcmp (o->lng, "verbose")) continue;
      if (!strcmp (o->lng, "witness")) continue;
    }
    fprintf (lgl->out, "%s--%s=%d\n", prefix, o->lng, o->val);
  }
}

void lglrgopts (LGL * lgl) {
  Opt * o;
  REQINIT ();
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++)
    fprintf (lgl->out, "%s %d %d %d\n", o->lng, o->val, o->min, o->max);
}

int lglhasopt (LGL * lgl, const char * opt) {
  Opt * o;
  REQINIT ();
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    if (!opt[1] && o->shrt == opt[0]) return 1;
    if (!strcmp (o->lng, opt)) return 1;
  }
  return 0;
}

void * lglfirstopt (LGL * lgl) { return FIRSTOPT (lgl); }

void * lglnextopt (LGL * lgl,
		   void * current,
		   const char **nameptr,
		   int *valptr, int *minptr, int *maxptr) {
  Opt * opt = current, * res = opt + 1;
  if (res > LASTOPT (lgl)) return 0;
  if (nameptr) *nameptr = opt->lng;
  if (valptr) *valptr = opt->val;
  if (minptr) *minptr = opt->min;
  if (maxptr) *maxptr = opt->max;
  return res;
}

void lglsetopt (LGL * lgl, const char * opt, int val) {
  int oldval;
  Opt * o;
#if 0
       if (!strcmp (opt, "clim"))     o = &lgl->opts->clim;
  else if (!strcmp (opt, "bias"))     o = &lgl->opts->bias;
  else if (!strcmp (opt, "flipping")) o = &lgl->opts->flipping;
  else if (!strcmp (opt, "mocint"))   o = &lgl->opts->mocint;
  else if (!strcmp (opt, "phase"))    o = &lgl->opts->phase;
  else if (!strcmp (opt, "plain"))    o = &lgl->opts->plain;
  else if (!strcmp (opt, "reduce"))   o = &lgl->opts->reduce;
  else if (!strcmp (opt, "seed"))     o = &lgl->opts->seed;
  else if (!strcmp (opt, "verbose"))  o = &lgl->opts->verbose;
#ifndef NLGLOG
  else if (!strcmp (opt, "log"))      o = &lgl->opts->log;
#endif
  else 
#endif
  {
    // REQUIRE (UNUSED | OPTSET); // TODO remove?
    for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
      if (!opt[1] && o->shrt == opt[0]) break;
      if (!strcmp (o->lng, opt)) break;
    }
  }
  if (o > LASTOPT (lgl)) return;
  if (val < o->min) val = o->min;
  if (o->max < val) val = o->max;
  oldval = o->val;
  o->val = val;
  if (o == &lgl->opts->flipping && !oldval)
    lgl->flipping = lgl->notflipped = 0;
  if (o == &lgl->opts->plain) {
    if (val > 0 && !oldval) lglsetplain (lgl, 1);
    if (!val && oldval) lglsetplain (lgl, 0);
  }
  if (o == &lgl->opts->phase && val != oldval) lgl->flushphases = 1;
  if (lgl->state == UNUSED) TRANS (OPTSET);
  TRAPI ("option %s %d", opt, val);
  if (lgl->clone) lglsetopt (lgl->clone, opt, val);
}

static int lglws (int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

int lglreadopts (LGL * lgl, FILE * file) {
  int res, ch, val, nvalbuf, noptbuf;
  char optbuf[40], valbuf[40];
  const char * opt;
  res = 0;
  for (;;) {
    while (lglws (ch = getc (file)))
      ;
    if (ch == EOF) break;
    noptbuf = 0;
    optbuf[noptbuf++] = ch;
    while ((ch = getc (file)) != EOF && !lglws (ch)) {
      if (noptbuf + 1 >= sizeof optbuf) { ch = EOF; break; }
      optbuf[noptbuf++] = ch;
    }
    if (ch == EOF) break;
    assert (noptbuf < sizeof optbuf);
    optbuf[noptbuf++] = 0;
    assert (lglws (ch));
    while (lglws (ch = getc (file)))
      ;
    if (ch == EOF) break;
    nvalbuf = 0;
    valbuf[nvalbuf++] = ch;
    while ((ch = getc (file)) != EOF && !lglws (ch)) {
      if (nvalbuf + 1 >= sizeof valbuf) { ch = EOF; break; }
      valbuf[nvalbuf++] = ch;
    }
    assert (nvalbuf < sizeof valbuf);
    valbuf[nvalbuf++] = 0;
    opt = optbuf;
    val = atoi (valbuf);
    lglprt (lgl, 1, "read option --%s=%d", opt, val);
    lglsetopt (lgl, opt, val);
    res++;
  }
  return res;
}

void lglsetout (LGL * lgl, FILE * out) { lgl->out = out; }

FILE * lglgetout (LGL * lgl) { return lgl->out; }

void lglsetprefix (LGL * lgl, const char * prefix) {
  lgldelstr (lgl, lgl->prefix);
  lgl->prefix = lglstrdup (lgl, prefix);
}

const char * lglgetprefix (LGL * lgl) { return lgl->prefix; }

static Opt * lgligetopt (LGL * lgl, const char * opt) {
  Opt * o;
  REQINIT ();
  for (o = FIRSTOPT (lgl); o <= LASTOPT (lgl); o++) {
    if (!opt[1] && o->shrt == opt[0]) return o;
    if (!strcmp (o->lng, opt)) return o;
  }
  return 0;
}

int lglgetopt (LGL * lgl, const char * opt) {
  Opt * o = lgligetopt (lgl, opt);
  return o ? o->val : 0;
}

int lglgetoptminmax (LGL * lgl, const char * opt,
                     int * min_ptr, int * max_ptr) {
  Opt * o = lgligetopt (lgl, opt);
  if (!o) return 0;
  if (min_ptr) *min_ptr = o->min;
  if (max_ptr) *max_ptr = o->max;
  return o->val;
}

/*------------------------------------------------------------------------*/

static void lglrszvars (LGL * lgl, int new_size) {
  int old_size = lgl->szvars;
  assert (lgl->nvars <= new_size);
  RSZ (lgl->vals, old_size, new_size);
  RSZ (lgl->i2e, old_size, new_size);
  RSZ (lgl->doms, 2*old_size, 2*new_size);
  RSZ (lgl->dvars, old_size, new_size);
  RSZ (lgl->avars, old_size, new_size);
  RSZ (lgl->jwh, 2*old_size, 2*new_size);
  RSZ (lgl->queue.nodes, old_size, new_size);
  lgl->szvars = new_size;
}

static void lglenlvars (LGL * lgl) {
  size_t old_size, new_size;
  old_size = lgl->szvars;
  new_size = old_size ? 2 * old_size : 4;
  LOG (3, "enlarging internal variables from %ld to %ld", old_size, new_size);
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

static int lglmax (int a, int b) { return a > b ? a : b; }

static int lglmin (int a, int b) { return a < b ? a : b; }

static DVar * lgldvar (LGL * lgl, int lit) {
  assert (2 <= abs (lit) && abs (lit) < lgl->nvars);
  return lgl->dvars + abs (lit);
}

static AVar * lglavar (LGL * lgl, int lit) {
  assert (2 <= abs (lit) && abs (lit) < lgl->nvars);
  return lgl->avars + abs (lit);
}

static Val lglval (LGL * lgl, int lit) {
  int idx = abs (lit);
  Val res;
  assert (2 <= idx && idx < lgl->nvars);
  res = lgl->vals[idx];
  if (lit < 0) res = -res;
  return res;
}

static int lgltrail (LGL * lgl, int lit) { return lglavar (lgl, lit)->trail; }

static TD * lgltd (LGL * lgl, int lit) {
  int pos = lgltrail (lgl, lit);
  assert (0 <= pos && pos < lgl->szdrail);
  return lgl->drail + pos;
}

static int lglevel (LGL * lgl, int lit) { return lgltd (lgl, lit)->level; }

static int lglisfree (LGL * lgl, int lit) {
  return lglavar (lgl, lit)->type == FREEVAR;
}

static void lglchkqueue (LGL * lgl) {
#ifndef NDEBUG
  Qln * line;
  int lit;
  if (!lgl->qscheduling) return;
  if (lgl->opts->check.val < 1) return;
  for (line = lgl->queue.bottom; line; line = line->up) {
    assert (line->prior >= 0);
    assert (line->first && line->last);
    assert (!line->down || line->down->prior < line->prior);
  }
  assert (lgl->queue.unassigned);
  if (lgl->opts->check.val < 2) return;
  line = lgl->queue.top;
  while (line != lgl->queue.unassigned) {
    for (lit = line->first; lit; lit = lglqnd (lgl, lit)->next)
      assert (lglval (lgl, lit));
    line = line->down;
  }
  for (lit = line->first;
       lit != line->unassigned;
       lit = lglqnd (lgl, lit)->next)
    assert (lglval (lgl, lit));
#endif
}

static void lglqsched (LGL * lgl, int idx) {
  Qnd * n = lglqnd (lgl, idx), * m;
  Qln * l;
  if (!lgl->qscheduling) return;
  assert (!n->prev && !n->next && !n->line);
  if (!(l = lgl->queue.bottom)) {
    if ((l = lgl->queue.free)) { lgl->queue.free = l->up; CLRPTR (l); }
    else NEW (l, 1);
    lgl->queue.bottom = lgl->queue.top = lgl->queue.unassigned = l->repr = l;
    assert (!lgl->queue.nlines);
    lgl->queue.nlines++;
    lgl->stats->queue.new++;
    LOG (2, "first new queue[0]");
    assert (!l->prior);
  } else if (l->prior) {
    if ((l = lgl->queue.free)) { lgl->queue.free = l->up; CLRPTR (l); }
    else NEW (l, 1);
    lgl->queue.bottom->down = l;
    l->up = lgl->queue.bottom;
    lgl->queue.bottom = l;
    assert (lgl->queue.top);
    assert (lgl->queue.unassigned);
    assert (!l->prior);
    l->repr = l;
    lgl->queue.nlines++;
    assert (lgl->queue.nlines > 1);
    lgl->stats->queue.new++;
    LOG (2, "new queue[0]");
  }
  assert (!l->down);
  if (lgl->bias < 0) {
    if ((n->prev = l->last)) {
      m = lglqnd (lgl, l->last);
      m->next = idx;
    } else l->first = l->unassigned = idx;
    l->last = idx;
    assert (!n->next);
  } else {
    if ((n->next = l->first)) {
      m = lglqnd (lgl, l->first);
      m->prev = idx;
    } else l->last = idx;
    l->first = l->unassigned = idx;
    assert (!n->prev);
  }
  n->line = l;
  if (lgl->opts->check.val >= 3) lglchkqueue (lgl);
}

static void lglqdump (LGL * lgl) {
  Qln * p;
  Qnd * n;
  int i;
  for (p = lgl->queue.top; p; p = p->down) {
    printf ("c queue[%d]", p->prior);
    for (i = p->first; i; i = n->next) {
      n = lglqnd (lgl, i);
      printf (" %d", i);
    }
    printf ("\n");
  }
}

static int lglnewvar (LGL * lgl) {
  AVar * av;
  DVar * dv;
  int res;
  assert (!lgl->dense);
  if (lgl->nvars == lgl->szvars) lglenlvars (lgl);
  if (lgl->nvars) res = lgl->nvars++;
  else res = 2, lgl->nvars = 3;
  assert (res < lgl->szvars);
  if (res > MAXVAR) lgldie (lgl, "more than %d variables", MAXVAR - 1);
  assert (res <= MAXVAR);
  assert (((res << RMSHFT) >> RMSHFT) == res);
  assert (((-res << RMSHFT) >> RMSHFT) == -res);
  LOG (3, "new internal variable %d", res);
  dv = lgl->dvars + res;
  CLRPTR (dv);
  av = lgl->avars + res;
  CLRPTR (av);
  lglqsched (lgl, res);
  lgl->unassigned++;
  lgl->allphaseset = 0;
  return res;
}

static int lglsgn (int lit) { return (lit < 0) ? -1 : 1; }

static Ext * lglelit2ext (LGL * lgl, int elit) {
  int idx = abs (elit);
  assert (0 < idx && idx <= lgl->maxext);
  return lgl->ext + idx;
}

static int lglerepr (LGL * lgl, int elit) {
  int res, next, tmp;
  Ext * ext;
  res = elit;
  for (;;) {
    ext = lglelit2ext (lgl, res);
    if (!ext->equiv) break;
    next = ext->repr;
    if (res < 0) next = -next;
    res = next;
  }
  tmp = elit;
  for (;;) {
    ext = lglelit2ext (lgl, tmp);
    if (!ext->equiv) { assert (tmp == res); break; }
    next = ext->repr;
    ext->repr = (tmp < 0) ? -res : res;
    if (tmp < 0) next = -next;
    tmp = next;
  }
  return res;
}

static void lgladjext (LGL * lgl, int eidx) {
  size_t old, new;
  assert (eidx >= lgl->szext);
  assert (eidx > lgl->maxext);
  assert (lgl->szext >= lgl->maxext);
  old = lgl->szext;
  new = old ? 2*old : 2;
  while (eidx >= new) new *= 2;
  assert (eidx < new && new >= lgl->szext);
  LOG (3, "enlarging external variables from %ld to %ld", old, new);
  RSZ (lgl->ext, old, new);
  lgl->szext = new;
}

static void lglmelter (LGL * lgl) {
  if (!lgl->frozen) return;
  lgl->frozen = 0;
  LOG (2, "melted solver");
}

static int lglimport (LGL * lgl, int elit) {
  // NOTE sync with 'lglcompletefork'
  int res, repr, eidx = abs (elit);
  Ext * ext;
  assert (elit);
  if (eidx >= lgl->szext) lgladjext (lgl, eidx);
  if (eidx > lgl->maxext) {
    lgl->maxext = eidx;
    lglmelter (lgl);
  }
  repr = lglerepr (lgl, elit);
  ext = lglelit2ext (lgl, repr);
  assert (!ext->equiv);
  res = ext->repr;
  if (!ext->imported) {
    res = lglnewvar (lgl);
    assert (!ext->equiv);
    ext->repr = res;
    ext->imported = 1;
    lgl->i2e[res] = eidx;
    LOG (3, "mapping external variable %d to %d", eidx, res);
  }
  if (repr < 0) res = -res;
  LOG (2, "importing %d as %d", elit, res);
  return res;
}

static int * lglidx2lits (LGL * lgl, int tag, int red, int lidx) {
  int * res, glue = 0;
  Stk * s;
  assert (tag == OCCS || tag == LRGCS);
  assert (red == 0 || red == REDCS);
  assert (0 <= lidx);
  COVER (red && tag == OCCS);
  if (!red) s = &lgl->irr;
  else if (tag == OCCS) s = &lgl->red[0];
  else {
    assert (tag == LRGCS);
    glue = lidx & GLUEMASK;
    lidx >>= GLUESHFT;
    assert (lidx <= MAXREDLIDX);
    s = &lgl->red[glue];
  }
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

static int lgliselim (LGL * lgl, int lit) {
  Tag tag = lglavar (lgl, lit)->type;
  return tag == ELIMVAR;
}

static int lglexport (LGL * lgl, int ilit) {
  assert (2 <= abs (ilit) && abs (ilit) < lgl->nvars);
  return lgl->i2e[abs (ilit)] * lglsgn (ilit);
}

static int * lglrsn (LGL * lgl, int lit) { return lgltd (lgl, lit)->rsn; }

static int lglulit (int lit) { return 2*abs (lit) + (lit < 0); }

static void lglsetdom (LGL * lgl, int lit, int dom) {
  assert (2 <= abs (lit)  && abs (lit) < lgl->nvars);
  assert (2 <= abs (dom)  && abs (dom) < lgl->nvars);
  assert (lglval (lgl, lit) >= 0);
  assert (lglval (lgl, dom) >= 0);
  lgl->doms[lglulit (lit)] = dom;
  LOG (3, "literal %d dominated by %d", lit, dom);
}

static int lglgetdom (LGL * lgl, int lit) {
  int res;
  assert (2 <= abs (lit)  && abs (lit) < lgl->nvars);
  assert (lglval (lgl, lit) >= 0);
  res = lgl->doms[lglulit (lit)];
  return res;
}

static HTS * lglhts (LGL * lgl, int lit) {
  return lgldvar (lgl, lit)->hts + (lit < 0);
}

static int * lglhts2wchs (LGL * lgl, HTS * hts) {
  int * res = lgl->wchs->stk.start + hts->offset;
  assert (res < lgl->wchs->stk.top);
  assert (res + hts->count < lgl->wchs->stk.top);
  assert (res + hts->count < lgl->wchs->stk.top);
  return res;
}

static void lglassign (LGL * lgl, int lit, int r0, int r1) {
  AVar * av = lglavar (lgl, lit);
  int idx, phase, glue, tag, dom;
  TD * td;
  LOGREASON (2, lit, r0, r1, "assign %d through", lit);
  av->trail = lglcntstk (&lgl->trail);
  if (av->trail >= lgl->szdrail) {
    int newszdrail = lgl->szdrail ? 2*lgl->szdrail : 1;
    RSZ (lgl->drail, lgl->szdrail, newszdrail);
    lgl->szdrail = newszdrail;
  }
  td = lgltd (lgl, lit);
  tag = r0 & MASKCS;
  dom = (tag == BINCS) ? lglgetdom (lgl, -(r0 >> RMSHFT)) : lit;
  lglsetdom (lgl, lit, dom);
#ifndef NDEBUG
  {
    int * p, red, other, other2, * c, lidx, found;
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
      c = lglidx2lits (lgl, tag, red, lidx);
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
  idx = abs (lit);
  phase = lglsgn (lit);
  lgl->vals[idx] = phase;
  if (!lgl->simp && !lgl->flipping && !lgl->phaseneg) {
    lgl->flips -= lgl->flips/100000ull;
    if (av->phase != phase) lgl->flips += 10000ull;
    av->phase = phase;
  }
#ifndef NDEBUG
  if (phase < 0) av->wasfalse = 1; else av->wasfalse = 0;
#endif
  td->level = lgl->level;
  if (!lgl->level) {
    if (av->type == EQUIVAR) {
      assert (lgl->stats->equiv.current > 0);
      lgl->stats->equiv.current--;
      assert (lgl->stats->equiv.sum > 0);
      lgl->stats->equiv.sum--;
    } else {
      assert (av->type == FREEVAR);
      av->type = FIXEDVAR;
    }
    lgl->stats->fixed.sum++;
    lgl->stats->fixed.current++;
    lgl->stats->prgss++;
    lgl->stats->irrprgss++;
    td->rsn[0] = UNITCS | (lit << RMSHFT);
    td->rsn[1] = 0;
    if (lgl->cbs && lgl->cbs->units.produce.fun)  {
      LOG (2, "trying to export internal unit %d external %d\n",
	   lgl->tid, lit, lglexport (lgl, lit));
      lgl->cbs->units.produce.fun (lgl->cbs->units.produce.state,
                                  lglexport (lgl, lit));
      LOG (2, "exporting internal unit %d external %d\n",
	      lgl->tid, lit, lglexport (lgl, lit));
    }
  } else {
    td->rsn[0] = r0;
    td->rsn[1] = r1;
  }
  lglpushstk (lgl, &lgl->trail, lit);
  if (!lgl->failed && (av->assumed & (1u << (lit > 0)))) {
    LOG (2, "failed assumption %d", -lit);
    lgl->failed = -lit;
  }
  lgl->unassigned--;
  td->lrglue = 0;
  if ((r0 & REDCS) && (r0 & MASKCS) == LRGCS) {
    glue = r1 & GLUEMASK;
    lgl->stats->lir[glue].forcing++;
    assert (lgl->stats->lir[glue].forcing > 0);
    if (lgl->level && 0 < glue && glue < MAXGLUE) {
      lgl->lrgluereasons++;
      assert (lgl->lrgluereasons > 0);
      td->lrglue = 1;
    }
  }
#ifdef __GNUC__
  __builtin_prefetch (lglhts2wchs (lgl, lglhts (lgl, -lit)));
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
  int * p = lglidx2lits (lgl, LRGCS, red, lidx), other;
  while ((other = *p++)) {
    assert (!lgliselim (lgl, other));
    if (other != lit) assert (lglval (lgl, other) < 0);
  }
  assert (red == 0 || red == REDCS);
#endif
  lglassign (lgl, lit, red | LRGCS, lidx);
}

static Qln * lglqln (LGL * lgl, int lit) {
  Qln * res, * line, * next;
  Qnd * node;
  node = lglqnd (lgl, lit);
  for (res = node->line; (line = res->repr) != res; res = line)
    ;
  for (line = node->line; line != res; line = next)
    next = line->repr, line->repr = res;
  node->line = res;
  return res;
}

static int lglqcmp (LGL * lgl, int l, int k) {
  Qln * ln = lglqln (lgl, l);
  Qln * kn = lglqln (lgl, k);
  return ln->prior - kn->prior;
}

static void lglunassign (LGL * lgl, int lit) {
  int idx = abs (lit), r0, r1, tag, lidx, glue;
  TD *  td;
  Qln * n;
  LOG (2, "unassign %d", lit);
  assert (lglval (lgl, lit) > 0);
  assert (lgl->vals[idx] == lglsgn (lit));
  lgl->vals[idx] = 0;
  lgl->unassigned++;
  assert (lgl->unassigned > 0);
  if (lgl->qscheduling) {
    n = lglqln (lgl, idx);
    n->unassigned = n->first;
    if (!lgl->queue.unassigned ||
	lgl->queue.unassigned->prior < n->prior)
      lgl->queue.unassigned = n;
  }
  td = lgltd (lgl, idx);
  r0 = td->rsn[0];
  if (!(r0 & REDCS)) return;
  tag = r0 & MASKCS;
  if (tag != LRGCS) return;
  r1 = td->rsn[1];
  glue = r1 & GLUEMASK;
  if (td->lrglue) {
    assert (lgl->lrgluereasons > 0);
    lgl->lrgluereasons--;
  }
  if (glue < MAXGLUE) return;
  lidx = r1 >> GLUESHFT;
  LOG (2, "eagerly deleting maximum glue clause at %d", lidx);
  lglrststk (&lgl->red[glue], lidx);
  lglredstk (lgl, &lgl->red[glue], (1<<20), 3);
}

static Val lglifixed (LGL * lgl, int lit) {
  int res;
  if (!(res = lglval (lgl, lit))) return 0;
  if (lglevel (lgl, lit) > 0) return 0;
  return res;
}

static void lglbacktrack (LGL * lgl, int level) {
  int lit;
  assert (level >= 0);
  assert (lgl->level > level);
  LOG (2, "backtracking to level %d", level);
  assert (level <= lgl->level);
  assert (abs (lgl->failed) != 1 || lgl->failed == -1);
  if (lgl->failed &&
      lgl->failed != -1 &&
      lglevel (lgl, lgl->failed) > level) {
    LOG (2, "resetting failed assumption %d", lgl->failed);
    lgl->failed = 0;
  }
  while (!lglmtstk (&lgl->trail)) {
    lit = lgltopstk (&lgl->trail);
    assert (abs (lit) > 1);
    if (lglevel (lgl, lit) <= level) break;
    lglunassign (lgl, lit);
    lgl->trail.top--;
  }
  assert (level || !lgl->lrgluereasons);
  if (lgl->alevel > level) {
    LOG (2,
	 "resetting assumption decision level to %d from %d",
	 level, lgl->alevel);
    lgl->alevel = level;
    if (lgl->assumed) {
      LOG (2,
	   "resetting assumption queue level to 0 from %d",
	   lgl->assumed);
      lgl->assumed = 0;
    }
  }
  lgl->level = level;
  lglrstcontrol (lgl, level + 1);
  assert (lglcntctk (&lgl->control) == lgl->level + 1);
  lgl->conf.lit = 0;
  lgl->conf.rsn[0] = lgl->conf.rsn[1] = 0;
  lgl->next2 = lgl->next = lglcntstk (&lgl->trail);
  LOG (2, "backtracked ");
}

static int lglmarked (LGL * lgl, int lit) {
  int res = lglavar (lgl, lit)->mark;
  if (lit < 0) res = -res;
  return res;
}

#ifndef NLGLPICOSAT

#define PICOSAT (assert (lgl && lgl->picosat.solver), lgl->picosat.solver)

#ifndef NDEBUG
static void lglpicosataddcls (LGL* lgl, const int * c){
  const int * p;
  lglpicosatinit (lgl);
  for (p = c; *p; p++) picosat_add (PICOSAT, *p);
  picosat_add (PICOSAT, 0);
}
#endif

static void lglpicosatchkclsaux (LGL * lgl, int * c) {
#ifndef NDEBUG
  int * p;
  lglpicosatinit (lgl);
  if (picosat_inconsistent (PICOSAT)) {
    LOG (3, "no need to check since PicoSAT is already inconsistent");
    return;
  }
  LOGCLS (3, c, "checking consistency with PicoSAT of clause");
  for (p = c; *p; p++) picosat_assume (PICOSAT, -*p);
  (void) picosat_sat (PICOSAT, -1);
  assert (picosat_res (PICOSAT) == 20);
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
  if (!lgl->opts->check.val) return;
  lglpicosatinit (lgl);
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
  int res, * p;
  lglpicosatinit (lgl);
  if (lgl->picosat.res) {
    LOG (1, "determined earlier that PicoSAT proved unsatisfiability");
    assert (lgl->picosat.res == 20);
  }
  if (picosat_inconsistent (PICOSAT)) {
    LOG (1, "PicoSAT is already in inconsistent state");
    return;
  }
  for (p = lgl->eassume.start; p < lgl->eassume.top; p++)
    picosat_assume (PICOSAT, lglimport (lgl, *p));
  res = picosat_sat (PICOSAT, -1);
  assert (res == 20);
  LOG (1, "PicoSAT proved unsatisfiability");
#endif
}
#endif

static void lglunitnocheck (LGL * lgl, int lit) {
  assert (!lgl->level);
  LOG (1, "unit %d", lit);
  lglassign (lgl, lit, (lit << RMSHFT) | UNITCS, 0);
}

static void lglunit (LGL * lgl, int lit) {
#ifndef NLGLPICOSAT
  if (lgl->picosat.chk) lglpicosatchkclsarg (lgl, lit, 0);
#endif
  lglunitnocheck (lgl, lit);
}

static void lglmark (LGL * lgl, int lit) {
  lglavar (lgl, lit)->mark = lglsgn (lit);
}

static void lglunmark (LGL * lgl, int lit) { lglavar (lgl, lit)->mark = 0; }

static void lglchksimpcls (LGL * lgl) {
#ifndef NDEBUG
  int *p, tmp, lit;
  AVar * av;
  for (p = lgl->clause.start; (lit = *p); p++) {
    tmp = lglifixed (lgl, lit);
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

/*------------------------------------------------------------------------*/

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
    if (!lglval (lgl, lit)) continue;
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
/*------------------------------------------------------------------------*/


static void lglfreewch (LGL * lgl, int oldoffset, int oldhcount) {
  int ldoldhcount = lglceilld (oldhcount);
  lgl->wchs->stk.start[oldoffset] = lgl->wchs->start[ldoldhcount];
  assert (oldoffset);
  lgl->wchs->start[ldoldhcount] = oldoffset;
  lgl->wchs->free++;
  assert (lgl->wchs->free > 0);
  LOG (5, "saving watch stack at %d of size %d on free list %d",
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

  LOG (5, "increasing watch stack at %d from %d to %d",
       oldoffset, oldhcount, newhcount);

  assert (!oldoffset == !oldhcount);

  lgl->stats->enlwchs++;

  newoffset = lgl->wchs->start[ldnewhcount];
  start = lgl->wchs->stk.start;
  if (newoffset != INT_MAX) {
    lgl->wchs->start[ldnewhcount] = start[newoffset];
    start[newoffset] = 0;
    assert (lgl->wchs->free > 0);
    lgl->wchs->free--;
    LOG (5, "reusing free watch stack at %d of size %d",
	 newoffset, (1 << ldnewhcount));
  } else {
    assert (lgl->wchs->stk.start[hts->offset]);
    assert (lgl->wchs->stk.top[-1] == INT_MAX);

    oldwcount = lglcntstk (&lgl->wchs->stk);
    newwcount = oldwcount + newhcount;
    oldwsize = lglszstk (&lgl->wchs->stk);
    newwsize = oldwsize;

    assert (lgl->wchs->stk.top == lgl->wchs->stk.start + oldwcount);
    assert (oldwcount > 0);

    while (newwsize < newwcount) newwsize *= 2;
    if (newwsize > oldwsize) {
      newwstart = oldwstart = lgl->wchs->stk.start;
      RSZ (newwstart, oldwsize, newwsize);
      LOG (3, "resized global watcher stack from %d to %d",
	   oldwsize, newwsize);
      res = newwstart - oldwstart;
      if (res) {
	LOG (3, "moved global watcher stack by %ld", res);
	start = lgl->wchs->stk.start = newwstart;
      }
      lgl->wchs->stk.end = start + newwsize;
    }
    lgl->wchs->stk.top = start + newwcount;
    lgl->wchs->stk.top[-1] = INT_MAX;
    newoffset = oldwcount - 1;
    LOG (5,
	 "new watch stack of size %d at end of global watcher stack at %d",
	 newhcount, newoffset);
  }
  assert (start == lgl->wchs->stk.start);
  assert (start[0]);
  j = newoffset;
  for (i = oldoffset; i < oldoffset + oldhcount; i++) {
    start[j++] = start[i];
    start[i] = 0;
  }
  while (j < newoffset + newhcount)
    start[j++] = 0;
  assert (start + j <= lgl->wchs->stk.top);
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
  lgl->stats->pshwchs++;
  assert (lgl->stats->pshwchs > 0);
  return res;
}

static long lglwchbin (LGL * lgl, int lit, int other, int red) {
  HTS * hts = lglhts (lgl, lit);
  int cs = ((other << RMSHFT) | BINCS | red);
  long res;
  assert (red == 0 || red == REDCS);
  res = lglpushwch (lgl, hts, cs);
  LOG (3, "new %s binary watch %d blit %d", lglred2str (red), lit, other);
  return res;
}

static long lglwchtrn (LGL * lgl, int a, int b, int c, int red) {
  HTS * hts = lglhts (lgl, a);
  int cs = ((b << RMSHFT) | TRNCS | red);
  long res;
  assert (red == 0 || red == REDCS);
  res = lglpushwch (lgl, hts, cs);
  res += lglpushwch (lgl, hts, c);
  LOG (3, "new %s ternary watch %d blits %d %d", lglred2str (red), a, b, c);
  return res;
}

static long lglwchlrg (LGL * lgl, int lit, int other, int red, int lidx) {
  HTS * hts = lglhts (lgl, lit);
  int blit = ((other << RMSHFT) | LRGCS | red);
  long res = 0;
  assert (red == 0 || red == REDCS);
  res += lglpushwch (lgl, hts, blit);
  res += lglpushwch (lgl, hts, lidx);
#ifndef NLGLOG
  {
    int * p = lglidx2lits (lgl, LRGCS, red, lidx);
    if (red)
      LOG (3,
	   "watching %d with blit %d in red[%d][%d] %d %d %d %d%s",
	   lit, other, (lidx & GLUEMASK), (lidx >> GLUESHFT),
	   p[0], p[1], p[2], p[3], (p[4] ? " ..." : ""));
    else
      LOG (3,
       "watching %d with blit %d in irr[%d] %d %d %d %d%s",
       lit, other, lidx, p[0], p[1], p[2], p[3], (p[4] ? " ..." : ""));
  }
#endif
  assert (sizeof (res) == sizeof (void*));
  return res;
}

/*------------------------------------------------------------------------*/

static EVar * lglevar (LGL * lgl, int lit) {
  int idx = abs (lit);
  assert (1 <= idx && idx < lgl->nvars);
  return lgl->evars + idx;
}

static int * lglepos (LGL * lgl, int lit) {
  EVar * ev;
  int * res;
  ev = lglevar (lgl, lit);
  res = &ev->pos;
  return res;
}

static int lglecmp (LGL * lgl, int l, int k) {
  return lglevar (lgl,k)->score - lglevar (lgl,l)->score;
}

#ifndef NDEBUG
static void lglchkesched (LGL * lgl) {
  int * p, parent, child, ppos, cpos, size, i, tmp;
  Stk * s = &lgl->esched;
  size = lglcntstk (s);
  p = s->start;
  for (ppos = 0; ppos < size; ppos++) {
    parent = p[ppos];
    tmp = *lglepos (lgl, parent);
    assert (ppos == tmp);
    for (i = 0; i <= 1; i++) {
      cpos = 2*ppos + 1 + i;
      if (cpos >= size) continue;
      child = p[cpos];
      tmp = *lglepos (lgl, child);
      assert (cpos == tmp);
      assert (lglecmp (lgl, parent, child) >= 0);
    }
  }
}
#endif

static void lgleup (LGL * lgl, int lit) {
  int child = lit, parent, cpos, ppos, * p, * cposptr, * pposptr;
  Stk * s = &lgl->esched;
  p = s->start;
  cposptr = lglepos (lgl, child);
  cpos = *cposptr;
  assert (cpos >= 0);
  while (cpos > 0) {
    ppos = (cpos - 1)/2;
    parent = p[ppos];
    if (lglecmp (lgl, parent, lit) >= 0) break;
    pposptr = lglepos (lgl, parent);
    assert (*pposptr == ppos);
    p[cpos] = parent;
    *pposptr = cpos;
    LOGESCHED (5, parent, "down from %d", ppos);
    cpos = ppos;
  }
  if (*cposptr == cpos) return;
#ifndef NLGLOG
  ppos = *cposptr;
#endif
  *cposptr = cpos;
  p[cpos] = lit;
  LOGESCHED (5, lit, "up from %d", ppos);
#ifndef NDEBUG
  if (lgl->opts->check.val >= 2) lglchkesched (lgl);
#endif
}

static void lgledown (LGL * lgl, int lit) {
  int parent = lit, child, right, ppos, cpos;
  int * p, * pposptr, * cposptr, size;
  Stk * s = &lgl->esched;
  size = lglcntstk (s);
  p = s->start;
  pposptr = lglepos (lgl, parent);
  ppos = *pposptr;
  assert (0 <= ppos);
  for (;;) {
    cpos = 2*ppos + 1;
    if (cpos >= size) break;
    child = p[cpos];
    if (cpos + 1 < size) {
      right = p[cpos + 1];
      if (lglecmp (lgl, child, right) < 0)
	cpos++, child = right;
    }
    if (lglecmp (lgl, child, lit) <= 0) break;
    cposptr = lglepos (lgl, child);
    assert (*cposptr = cpos);
    p[ppos] = child;
    *cposptr = ppos;
    LOGESCHED (5, child, "up from %d", cpos);
    ppos = cpos;
  }
  if (*pposptr == ppos) return;
#ifndef NLGLOG
  cpos = *pposptr;
#endif
  *pposptr = ppos;
  p[ppos] = lit;
  LOGESCHED (5, lit, "down from %d", cpos);
#ifndef NDEBUG
  if (lgl->opts->check.val >= 2) lglchkesched (lgl);
#endif
}

static int lglifrozen (LGL * lgl, int ilit) {
  int elit = lglexport (lgl, ilit);
  Ext * ext = lglelit2ext (lgl, elit);
  return ext->frozen || ext->tmpfrozen;
}

static void lglesched (LGL * lgl, int lit) {
  AVar * av;
  int * p;
  Stk * s;
  if (lgl->cgrclosing) return;
  if (lglifrozen (lgl, lit)) return;
  if (!lglisfree (lgl, lit)) return;
  if (lgl->donotsched) {
    av = lglavar (lgl, lit);
    if (lgl->eliminating && av->donotelm) return;
    if (lgl->blocking && av->donotblk) return;
    if (lgl->cceing && av->donotcce) return;
  }
  p = lglepos (lgl, lit);
  s = &lgl->esched;
  if (*p >= 0) return;
  *p = lglcntstk (s);
  lglpushstk (lgl, s, lit);
  lgleup (lgl, lit);
  lgledown (lgl, lit);
  LOGESCHED (4, lit, "pushed");
}

/*------------------------------------------------------------------------*/

static unsigned lglgcd (unsigned a, unsigned b) {
  unsigned tmp;
  assert (a), assert (b);
  if (a < b) SWAP (unsigned, a, b);
  while (b) tmp = b, b = a % b, a = tmp;
  return a;
}

static int lglrandidxtrav (LGL * lgl, int (*fun)(LGL*,int idx)) {
  int idx, delta, mod, prev, first, res;
  first = mod = lglmax (lgl->nvars, 2);
  idx = lglrand (lgl) % mod;
  delta = lglrand (lgl) % mod;
  if (!delta) delta++;
  while (lglgcd (delta, mod) > 1)
    if (++delta == mod) delta = 1;
  res = 1;
  while (res)
    if (idx >= 2 && !fun (lgl, idx)) res = 0;
    else {
      prev = idx;
      idx += delta;
      if (idx >= mod) idx -= mod;
      if (idx == first) break;
      if (first == mod) first = prev;
    }
  return res;
}

static int lglrem (LGL * lgl) {
  int res = lgl->nvars;
  if (!res) return 0;
  assert (res >= 2);
  res -= lgl->stats->fixed.current + 2;
  assert (res >= 0);
  return res;
}

static double lglpcnt (double n, double d) {
  if (d <= 0 || !n) return 0.0;
  return 100.0 * n / d;
}

static int lglecalc (LGL * lgl, EVar * ev) {
  int oldscore = ev->score;
  ev->score = ev->occ[0] + ev->occ[1];
  return ev->score - oldscore;
}

static int lglocc (LGL * lgl, int lit) {
  return lglevar (lgl, lit)->occ[lit < 0];
}

static void lglincocc (LGL * lgl, int lit) {
  int idx, sign, change;
  EVar * ev;
  if (lgl->cgrclosing || lgl->probing || lgl->gaussing) return;
  assert (lgl->blocking || lgl->eliminating || lgl->cceing);
  idx = abs (lit), sign = (lit < 0);
  ev = lglevar (lgl, lit);
  assert (lglisfree (lgl, lit));
  ev->occ[sign] += 1;
  assert (ev->occ[sign] > 0);
  change = lglecalc (lgl, ev);
  LOG (3, "inc occ of %d now occs[%d] = %d %d",
       lit, idx, ev->occ[0], ev->occ[1]);
  if (ev->pos < 0) lglesched (lgl, idx);
  else if (change > 0) lgledown (lgl, idx);
  else if (change < 0) lgleup (lgl, idx);
}

static int lglisact (int act) { return NOTALIT <= act && act < REMOVED-1; }

static void lglrescoreglue (LGL * lgl, int glue) {
  int * c, * p, oldact, newact;
  Stk * lir = lgl->red + glue;
  for (c = lir->start; c < lir->top; c = p + 1) {
    oldact = *c;
    if (oldact == REMOVED) {
      for (p = c + 1; p < lir->top && *p == REMOVED; p++)
	;
      assert (p >= lir->top || *p < NOTALIT || lglisact (*p));
      p--;
    } else {
      assert (NOTALIT <= oldact && oldact <= REMOVED - 1);
      newact = NOTALIT + ((oldact - NOTALIT) + 1) / 2;
      *c++ = newact;
      LOGCLS (5, c, "rescoring activity from %d to %d of clause",
	      oldact - NOTALIT, newact - NOTALIT);
      for (p = c; *p; p++)
	;
    }
  }
}

static void lglrescoreclauses (LGL * lgl) {
  int glue;
  lgl->stats->rescored.clauses++;
  for (glue = 0; glue < MAXGLUE; glue++)
    lglrescoreglue (lgl, glue);
}

static void lglchkirrstats (LGL * lgl) {
#if  0
  int idx, sign, lit, blit, tag, red, other, other2, clauses, lits;
  const int * p, * w, * eow, * c, * top;
  HTS * hts;
  clauses = lits = 0;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	red = blit & REDCS;
	if (red) continue;
	if (tag == BINCS) {
	  other = blit >> RMSHFT;
	  if (abs (other) < idx) continue;
	  lits += 2;
	} else if (tag == TRNCS) {
	  other = blit >> RMSHFT;
	  if (abs (other) < idx) continue;
	  other2 = *p;
	  if (abs (other2) < idx) continue;
	  lits += 3;
	} else continue;
	clauses++;
      }
    }
  top = lgl->irr.top;
  for (c = lgl->irr.start; c < top; c = p + 1) {
    if (*(p = c) >= NOTALIT) continue;
    while (*++p)
      ;
    lits += p - c;
    clauses++;
  }
  assert (clauses == lgl->stats->irr.clauses.cur);
  assert (lits == lgl->stats->irr.lits.cur);
#else
  (void) lgl;
#endif
}

static void lglincirr (LGL * lgl, int size) {
  if (size < 2) return;
  lgl->stats->irr.clauses.cur++;
  assert (lgl->stats->irr.clauses.cur > 0);
  if (lgl->stats->irr.clauses.cur > lgl->stats->irr.clauses.max)
    lgl->stats->irr.clauses.max = lgl->stats->irr.clauses.cur;
  lgl->stats->irr.lits.cur += size;
  assert (lgl->stats->irr.lits.cur >= size);
  if (lgl->stats->irr.lits.cur > lgl->stats->irr.lits.max)
    lgl->stats->irr.lits.max = lgl->stats->irr.lits.cur;
  lgl->stats->irrprgss++;
}

static void lgldecirr (LGL * lgl, int size) {
  assert (size >= 2);
  assert (lgl->stats->irr.clauses.cur > 0);
  lgl->stats->irr.clauses.cur--;
  assert (lgl->stats->irr.lits.cur >= size);
  lgl->stats->irr.lits.cur -= size;
  assert (!lgl->stats->irr.clauses.cur == !lgl->stats->irr.lits.cur);
  lgl->stats->irrprgss++;
}

static int lglbumplidx (LGL * lgl, int lidx) {
  int glue = (lidx & GLUEMASK), * c, *ap, act;
  Stk * lir = lgl->red + glue;
  lidx >>= GLUESHFT;
  c = lir->start + lidx;
  assert (lir->start < c && c < lir->end);
  ap = c - 1;
  act = *ap;
  if (act < REMOVED - 1) {
    LGLCHKACT (act);
    act += 1;
    LGLCHKACT (act);
    *ap = act;
  }
  LOGCLS (4, c, "bumped activity to %d of glue %d clause", act-NOTALIT, glue);
  lgl->stats->lir[glue].resolved++;
  assert (lgl->stats->lir[glue].resolved > 0);
  return (act >= REMOVED - 1);
}

static int lgladdcls (LGL * lgl, int red, int origlue, int force) {
  int size, lit, other, other2, * p, lidx, unit, blit;
  int scaledglue, redglue, prevglue;
  Val val;
  Stk * w;
  lgl->stats->prgss++;
  if (lgl->eliminating) lgl->stats->elm.steps += lglcntstk (&lgl->clause);
  if (!red) lgl->stats->irrprgss++;
  assert (!lglmtstk (&lgl->clause));
  assert (!lgl->clause.top[-1]);
  if (force) lglchksimpcls (lgl);
#if !defined(NLGLPICOSAT) && !defined(NDEBUG)
  lglpicosataddcls (lgl, lgl->clause.start);
#endif
  size = lglcntstk (&lgl->clause) - 1;
  if (!red) lglincirr (lgl, size);
  else if (size == 2) lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
  else if (size == 3) lgl->stats->red.trn++, assert (lgl->stats->red.trn > 0);
  assert (size >= 0);
  if (!size) {
    LOG (1, "found empty clause");
    lgl->mt = 1;
    return 0;
  }
  lit = lgl->clause.start[0];
  if (size == 1) {
    assert (lglval (lgl, lit) >= 0);
    if (!lglval (lgl, lit)) {
      if (red) lglunit (lgl, lit);
      else lglunitnocheck (lgl, lit);
    }
    return 0;
  }
  other = lgl->clause.start[1];
  if (size == 2) {
    lglwchbin (lgl, lit, other, red);
    lglwchbin (lgl, other, lit, red);
    if (red) {
      if (force && lglval (lgl, lit) < 0) lglf2rce (lgl, other, lit, REDCS);
      if (force && lglval (lgl, other) < 0) lglf2rce (lgl, lit, other, REDCS);
    } else if (lgl->dense) {
      assert (!red);
      lglincocc (lgl, lit);
      lglincocc (lgl, other);
    }
    return 0;
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
      if (force && lglval (lgl, lit) < 0 && lglval (lgl, other) < 0)
	lglf3rce (lgl, other2, lit, other, REDCS);
      if (force && lglval (lgl, lit) < 0 && lglval (lgl, other2) < 0)
	lglf3rce (lgl, other, lit, other2, REDCS);
      if (force && lglval (lgl, other) < 0 && lglval (lgl, other2) < 0)
	lglf3rce (lgl, lit, other, other2, REDCS);
    } else if (lgl->dense) {
      assert (!red);
      lglincocc (lgl, lit);
      lglincocc (lgl, other);
      lglincocc (lgl, other2);
    }
    return 0;
  }
  assert (size > 3);
  if (red) {
    assert (0 <= origlue);
    if (origlue <= lgl->opts->gluekeep.val) scaledglue = 0;
    else {
      scaledglue = redglue = origlue - lgl->opts->gluekeep.val;
      assert (redglue >= 1);
      switch (lgl->opts->gluescale.val) {
	case 3: scaledglue = 1 + lglceilld (redglue); break;
	case 2: scaledglue = lglceilsqrt32 (redglue); break;
      }
      assert (scaledglue > 0);
    }
    if (scaledglue >= MAXGLUE) scaledglue = MAXGLUE;
    lgl->stats->clauses.scglue += scaledglue;
    if (scaledglue == MAXGLUE) lgl->stats->clauses.maxglue++;
    else lgl->stats->clauses.nonmaxglue++;
    w = lgl->red + scaledglue;
    lidx = lglcntstk (w) + 1;
    if (lidx > MAXREDLIDX) {
      prevglue = scaledglue;
      if (lidx > MAXREDLIDX) {
	scaledglue = prevglue;
	while (scaledglue + 1 < MAXGLUE && lidx > MAXREDLIDX) {
	  w = lgl->red + ++scaledglue;
	  lidx = lglcntstk (w) + 1;
	}
      }
      if (lidx > MAXREDLIDX) {
	scaledglue = prevglue;
	while (scaledglue > 0 && lidx > MAXREDLIDX) {
	  w = lgl->red + --scaledglue;
	  lidx = lglcntstk (w) + 1;
	}
      }
      if (lidx > MAXREDLIDX && scaledglue < MAXGLUE) {
	w = lgl->red + (scaledglue = MAXGLUE);
	lidx = lglcntstk (w) + 1;
      }
      if (lidx > MAXREDLIDX && scaledglue == MAXGLUE) {
	lglbacktrack (lgl, 0);
	lidx = lglcntstk (w);
	assert (!lidx);
      }
      if (lidx > MAXREDLIDX)
	lgldie (lgl, "number of redundant large clause literals exhausted");
    }
    // fprintf (stderr, "orig %d scaled %d glue\n", origlue, scaledglue);
    lglpushstk (lgl, w, NOTALIT);
    assert (lidx == lglcntstk (w));
    lidx <<= GLUESHFT;
    assert (0 <= lidx);
    lidx |= scaledglue;
    lgl->stats->lir[scaledglue].clauses++;
    assert (lgl->stats->lir[scaledglue].clauses > 0);
    lgl->stats->lir[scaledglue].added++;
    assert (lgl->stats->lir[scaledglue].added > 0);
  } else {
    w = &lgl->irr;
    lidx = lglcntstk (w);
    scaledglue = 0;
    if (lidx <= 0 && !lglmtstk (w))
      lgldie (lgl, "number of irredundant large clause literals exhausted");
  }
  for (p = lgl->clause.start; (other2 = *p); p++)
    lglpushstk (lgl, w, other2);
  lglpushstk (lgl, w, 0);
  if (red) {
    unit = 0;
    for (p = lgl->clause.start; (other2 = *p); p++) {
      val = lglval (lgl, other2);
      assert (!force || val <= 0);
      if (val < 0) continue;
      if (unit) unit = INT_MAX;
      else unit = other2;
    }
    if (force && unit && unit != INT_MAX) lglflrce (lgl, unit, red, lidx);
  }
  assert (red == 0 || red == REDCS);
  if (!red || (red && scaledglue < MAXGLUE)) {
    (void) lglwchlrg (lgl, lit, other, red, lidx);
    (void) lglwchlrg (lgl, other, lit, red, lidx);
  }
  if (red && scaledglue != MAXGLUE) {
#ifndef NDEBUG
    int rescore =
#endif
    lglbumplidx (lgl, lidx);
    assert (!rescore);
    lgl->stats->red.lrg++;
  }
  if (!red && lgl->dense >= 2) {
    if (lidx > MAXIRRLIDX)
      lgldie (lgl, "number of irredundant large clause literals exhausted");
    blit = (lidx << RMSHFT) | OCCS;
    for (p = lgl->clause.start; (other2 = *p); p++) {
      lglincocc (lgl, other2);
      lglpushwch (lgl, lglhts (lgl, other2), blit);
    }
  }
  lglchkirrstats (lgl);
  return lidx;
}

static void lgliadd (LGL * lgl, int ilit) {
  int size;
#ifndef NLGLOG
  if (lglmtstk (&lgl->clause)) LOG (4, "opening irredundant clause");
#endif
  assert (abs (ilit) < 2 || abs (ilit) < lgl->nvars);
  lglpushstk (lgl, &lgl->clause, ilit);
  if (ilit) {
    LOG (4, "added literal %d", ilit);
  } else {
    LOG (4, "closing irredundant clause");
    LOGCLS (3, lgl->clause.start, "unsimplified irredundant clause");
    if (!lglsimpcls (lgl)) {
      lgladdcls (lgl, 0, 0, 1);
      lgl->stats->irr.clauses.add++;
      size = lglcntstk (&lgl->clause) - 1;
      assert (size >= 0);
      lgl->stats->irr.clauses.add += size;
    }
    lglclnstk (&lgl->clause);
  }
}

static void lgleunassignall (LGL * lgl) {
  int eidx;
  for (eidx = 1; eidx <= lgl->maxext; eidx++)
    lglelit2ext (lgl, eidx)->val = 0;
}

static void lglchkeassumeclean (LGL * lgl) {
  assert (lglmtstk (&lgl->eassume));
#ifndef NDEBUG
  int eidx;
  for (eidx = 1; eidx <= lgl->maxext; eidx++) {
    Ext * ext = lglelit2ext (lgl, eidx);
    assert (!ext->assumed);
    assert (!ext->failed);
  }
#endif
}

static void lglchkassumeclean (LGL * lgl) {
  assert (!lgl->failed);
  assert (!lgl->assumed);
  assert (!lgl->cassumed);
  assert (!lgl->ncassumed);
  assert (lglmtstk (&lgl->assume));
#ifndef NDEBUG
  if (lgl->opts->check.val >= 1) {
    int idx;
    for (idx = 2; idx < lgl->nvars; idx++) {
      AVar * av = lglavar (lgl, idx);
      assert (!av->assumed);
      assert (!av->failed);
    }
  }
#endif
}

static void lglreset (LGL * lgl) {
  int elit, ilit, erepr;
  Ext * ext, * rext;
  unsigned bit;
  AVar * av;
  if (lgl->state == RESET) return;
  if (lgl->state <= USED) return;
  assert (lgl->state & (UNKNOWN|SATISFIED|EXTENDED|UNSATISFIED|FAILED|LOOKED));
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  if (!lglmtstk (&lgl->eassume)) {
    LOG (2, "resetting %d external assumptions", lglcntstk (&lgl->eassume));
    while (!lglmtstk (&lgl->eassume)) {
      elit = lglpopstk (&lgl->eassume);
      ext = lglelit2ext (lgl, elit);
      ext->assumed = 0;
      if (ext->failed) {
	ext->failed = 0;
	erepr = lglerepr (lgl, elit);
	if (erepr != elit) {
	  rext = lglelit2ext (lgl, erepr);
	  rext->failed = 0;
	}
      }
    }
  }
  lglchkeassumeclean (lgl);
  if (!lglmtstk (&lgl->assume)) {
    LOG (2, "resetting %d internal assumptions", lglcntstk (&lgl->assume));
    while (!lglmtstk (&lgl->assume)) {
      ilit = lglpopstk (&lgl->assume);
      av = lglavar (lgl, ilit);
      bit = (1u << (ilit < 0));
      assert (av->assumed & bit);
      av->assumed &= ~bit;
      av->failed &= ~bit;
    }
  }
  if (!lglmtstk (&lgl->cassume)) {
    assert (lgl->ncassumed);
    LOG (2, "resetting assumed clause of size %d", lglcntstk (&lgl->cassume));
    lglclnstk (&lgl->cassume);
    lgl->cassumed = lgl->ncassumed = 0;
  } else assert (!lgl->ncassumed), assert (!lgl->cassumed);
  if (lgl->failed) {
    LOG (2, "resetting internal failed assumption %d", lgl->failed);
    lgl->failed = 0;
  }
  if (lgl->assumed) {
    LOG (2, "resetting assumption queue level to 0 from %d", lgl->assumed);
    lgl->assumed = 0;
  }
  lglchkassumeclean (lgl);
#if !defined(NDEBUG) && !defined (NLGLPICOSAT)
  if (lgl->picosat.res) {
    LOG (2, "resetting earlier PicoSAT result %d", lgl->picosat.res);
    lgl->picosat.res = 0;
  }
#endif
  lgleunassignall (lgl);
  TRANS (RESET);
}

static void lgluse (LGL * lgl) {
  if (lgl->state >= USED) return;
  assert (lgl->state == UNUSED || lgl->state == OPTSET);
  TRANS (USED);
}

static void lglfadd (LGL * lgl, int elit) {
  int eidx, size;
  const int * p;
  int64_t sum;
  Ext * ext;
  if (elit) {
    eidx = abs (elit);
    if (eidx > lgl->stats->features.vars) lgl->stats->features.vars = eidx;
    lgl->stats->features.lits.total++;
    if (elit > 0) lgl->stats->features.lits.pos++;
    if (elit < 0) lgl->stats->features.lits.neg++;
    lglpushstk (lgl, &lgl->eclause, elit);
  } else {
    size = lglcntstk (&lgl->eclause);
    lgl->stats->features.clauses.total++;
    if (size > 0) {
      if (size == 1) lgl->stats->features.clauses.unit++;
      if (size == 2) lgl->stats->features.clauses.bin++;
      if (size == 3) lgl->stats->features.clauses.trn++;
      if (size > 3) lgl->stats->features.clauses.lrg++;
      sum = 0;
      for (p = lgl->eclause.start; p < lgl->eclause.top; p++)
	sum += abs (*p);
      sum *= 10;
      sum /= size;
      sum += 5;
      sum /= 10;
      for (p = lgl->eclause.start; p < lgl->eclause.top; p++) {
	ext = lglelit2ext (lgl, *p);
	assert (ext->imported);
	ext->cog.sum += sum;
	ext->cog.count++;
      }
      lglclnstk (&lgl->eclause);
    }
  }
}

static void lgleadd (LGL * lgl, int elit) {
  int ilit;
  lglreset (lgl);
  if (elit) {
    ilit = lglimport (lgl, elit);
    LOG (4, "adding external literal %d as %d", elit, ilit);
  } else {
    ilit = 0;
    LOG (4, "closing external clause");
  }
  lglfadd (lgl, elit);
  lgliadd (lgl, ilit);
#ifndef NCHKSOL
  lglpushstk (lgl, &lgl->orig, elit);
#endif
}

void lgladd (LGL * lgl, int elit) {
  int eidx = abs (elit);
  Ext * ext;
  REQINIT ();
  TRAPI ("add %d", elit);
  ABORTIF (lgl->forked, "can not add literal to forked instance");
  if (0 < eidx && eidx <= lgl->maxext) {
    ext = lglelit2ext (lgl, elit);
    ABORTIF (ext->melted, "adding melted literal %d", elit);
  }
  lgl->stats->calls.add++;
  lgleadd (lgl, elit);
  lgluse (lgl);
  if (lgl->clone) lgladd (lgl->clone, elit);
}

static void lglisetphase (LGL * lgl, int lit, int phase) {
  AVar * av;
  if (lit < 0) lit = -lit, phase = -phase;
  av = lglavar (lgl, lit);
  av->fase = phase;
  LOG (2, "setting phase of internal literal %d to %d", lit, phase);
}

static void lglesetphase (LGL * lgl, int elit, int phase) {
  int ilit = lglimport (lgl, elit);
  if (abs (ilit) >= 2) {
    LOG (2, "setting phase of external literal %d to %d", elit, phase);
    lglisetphase (lgl, ilit, phase);
  } else LOG (2, "setting phase of external literal %d skipped", elit);
}

void lglsetphase (LGL * lgl, int elit) {
  REQINIT ();
  ABORTIF (!elit, "invalid literal argument");
  TRAPI ("setphase %d", elit);
  if (elit < 0) lglesetphase (lgl, -elit, -1);
  else lglesetphase (lgl, elit, 1);
  if (lgl->clone) lglsetphase (lgl->clone, elit);
}

void lglresetphase (LGL * lgl, int elit) {
  REQINIT ();
  ABORTIF (elit, "invalid literal argument");
  TRAPI ("resetphase %d", elit);
  lglesetphase (lgl, elit, 0);
  if (lgl->clone) lglresetphase (lgl->clone, elit);
}

static void lgleassume (LGL * lgl, int elit) {
  int ilit, val;
  unsigned bit;
  AVar * av;
  Ext * ext;
  lglreset (lgl);
  ilit = lglimport (lgl, elit);
  LOG (2, "assuming external literal %d", elit);
  bit = 1u << (elit < 0);
  ext = lglelit2ext (lgl, elit);
  if (!(ext->assumed & bit)) {
    ext->assumed |= bit;
    lglpushstk (lgl, &lgl->eassume, elit);
  }
  assert (!lgl->level);
  if (!(val = lglcval (lgl, ilit))) {
    av = lglavar (lgl, ilit);
    bit = (1u << (ilit < 0));
    if (av->assumed & bit) {
      LOG (2, "internal literal %d already assumed", ilit);
    } else {
      av->assumed |= bit;
      if (av->assumed & (bit^3))
	LOG (2, "negation %d was also already assumed", -ilit);
      lglpushstk (lgl, &lgl->assume, ilit);
    }
  } else if (val > 0) {
    LOG (2, "externally assumed literal %d already fixed to true", elit);
  } else {
    assert (val < 0);
    LOG (2, "externally assumed literal %d already fixed to false", elit);
    if (ilit != -1) {
      av = lglavar (lgl, ilit);
      bit = (1u << (ilit < 0));
      if (!(av->assumed & bit)) {
	av->assumed |= bit;
	lglpushstk (lgl, &lgl->assume, ilit);
      }
    }
    if (!lgl->failed) lgl->failed = ilit;
  }
}

static void lglecassume (LGL * lgl, int elit) {
  LOG (2, "adding external literal %d to assumed clause", elit);
}

void lglassume (LGL * lgl, int elit) {
  int eidx = abs (elit);
  Ext * ext;
  REQINIT ();
  TRAPI ("assume %d", elit);
  ABORTIF (lgl->forked, "can not assume literal in forked instance");
  lgl->stats->calls.assume++;
  ABORTIF (!elit, "can not assume invalid literal 0");
  if (0 < eidx && eidx <= lgl->maxext) {
    ext = lglelit2ext (lgl, elit);
    ABORTIF (ext->melted, "assuming melted literal %d", elit);
  }
  lgleassume (lgl, elit);
  lgluse (lgl);
  if (lgl->clone) lglassume (lgl->clone, elit);
}

void lglcassume (LGL * lgl, int elit) {
  int eidx = abs (elit);
  Ext * ext;
  REQINIT ();
  TRAPI ("cassume %d", elit);
  lgl->stats->calls.cassume++;
  if (0 < eidx && eidx <= lgl->maxext) {
    ext = lglelit2ext (lgl, elit);
    ABORTIF (ext->melted, "assuming melted literal %d", elit);
  }
  lglecassume (lgl, elit);
  lgluse (lgl);
  if (lgl->clone) lglcassume (lgl->clone, elit);
}

void lglfixate (LGL * lgl) {
  const int  * p;
  Stk eassume;
  REQINIT ();
  TRAPI ("fixate");
  if (lgl->mt) return;
  CLR (eassume);
  for (p = lgl->eassume.start; p < lgl->eassume.top; p++)
    lglpushstk (lgl, &eassume, *p);
  for (p = eassume.start; p < eassume.top; p++)
    lgleadd (lgl, *p), lgleadd (lgl, 0);
  lglrelstk (lgl, &eassume);
  lgluse (lgl);
  if (lgl->clone) lglfixate (lgl->clone);
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

static void lglonflict (LGL * lgl, int check, int lit, int red, int lidx) {
  int glue;
#if !defined (NLGLOG) || !defined (NDEBUG)
  int * p, * c = lglidx2lits (lgl, LRGCS, red, lidx);
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
  if (lgl->opts->log.val >= 2) {
    lglogstart (lgl, 2, "inconsistent %s large clause", lglred2str (red));
    for (p = c ; *p; p++)
      fprintf (lgl->out, " %d", *p);
    lglogend (lgl);
  }
#endif
  if (red) {
    glue = lidx & GLUEMASK;
    lgl->stats->lir[glue].conflicts++;
    assert (lgl->stats->lir[glue].conflicts > 0);
  }
}

static void lgldeclscnt (LGL * lgl, int size, int red, int glue) {
  assert (!red || red == REDCS);
  if (!red) lgldecirr (lgl, size);
  else if (size == 2) assert (lgl->stats->red.bin), lgl->stats->red.bin--;
  else if (size == 3) assert (lgl->stats->red.trn), lgl->stats->red.trn--;
  else {
    assert (lgl->stats->red.lrg > 0);
    lgl->stats->red.lrg--;
    assert (lgl->stats->lir[glue].clauses > 0);
    lgl->stats->lir[glue].clauses--;
    if (0 < glue && glue != MAXGLUE) {
    }
  }
}

static void lglrminc (LGL * lgl, const int * w, const int * eow) {
  int inc;
  assert (w <= eow);
  assert (eow <= w + INT_MAX);
  inc = eow - w;
  assert (inc >= 0);
  inc >>= lgl->opts->rmincpen.val;
  inc++;
  assert (lgl->blocking + lgl->eliminating <= 1);
  if (lgl->blocking) lgl->stats->blk.steps += inc;
  else if (lgl->eliminating) lgl->stats->elm.steps += inc;
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
  lglrminc (lgl, w, eow);
  blit1 = (other1 << RMSHFT) | red | TRNCS;
  blit2 = (other2 << RMSHFT) | red | TRNCS;
  for (;;) {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == BINCS || tag == OCCS) continue;
    other = *p++;
    if (tag == LRGCS) continue;
    assert (tag == TRNCS);
    if (blit == blit1 && other == other2) break;
    if (blit == blit2 && other == other1) break;
  }
  while (p < eow) p[-2] = p[0], p++;
  lglshrinkhts (lgl, hts, p - w - 2);
}

static void lglpopnunmarkstk (LGL * lgl, Stk * stk) {
  while (!lglmtstk (stk))
    lglavar (lgl, lglpopstk (stk))->mark = 0;
}

static void lglpopnunlcamarkstk (LGL * lgl, Stk * stk) {
  while (!lglmtstk (stk))
    lglavar (lgl, lglpopstk (stk))->lcamark = 0;
}

static int lglcamarked (LGL * lgl, int lit) {
  switch (lglavar (lgl, lit)->lcamark) {
    case 1: return (lit < 0) ? -1 : 1;
    case 2: return (lit < 0) ? -2 : 2;
    case 4: return (lit < 0) ? 1 : -1;
    case 8: return (lit < 0) ? 2 : -2;
    default: assert (!lglavar (lgl, lit)->lcamark); return 0;
  }
}

static void lglcamark (LGL * lgl, int lit, int mark) {
  int newmark;
  AVar * av;
  assert (mark == 1 || mark == 2);
  av = lglavar (lgl, lit);
  assert (!av->lcamark);
  newmark = mark;
  if (lit < 0) newmark <<= 2;
  av->lcamark = newmark;
  lglpushstk (lgl, &lgl->lcaseen, lit);
  assert (lglcamarked (lgl, lit) == mark);
  assert (lglcamarked (lgl, -lit) == -mark);
}

static int lglca (LGL * lgl, int a, int b) {
  int blit, tag, mark, negmark, prevmark, c, res, prev, next, al, bl;
  const int * p, * w, * eow;
  HTS * hts;
  if (!a) return b;
  if (!b) return a;
  if (a == b) return a;
  if (a == -b) return 0;
  al = lglevel (lgl, a);
  bl = lglevel (lgl, b);
  if (!al) return b;
  if (!bl) return a;
  assert (lglval (lgl, a) > 0);
  assert (lglval (lgl, b) > 0);
  assert (lglmtstk (&lgl->lcaseen));
  lglcamark (lgl, a, 1);
  lglcamark (lgl, b, 2);
  res = next = 0;
  while (next < lglcntstk (&lgl->lcaseen)) {
    c = lglpeek (&lgl->lcaseen, next++);
    assert (lglval (lgl, c) > 0);
    assert (lglevel (lgl, c) > 0);
    mark = lglcamarked (lgl, c);
    assert (mark == 1 || mark == 2);
    negmark = mark ^ 3;
    hts = lglhts (lgl, c);
    if (!hts->count) continue;
    w = lglhts2wchs (lgl, hts);
    eow = w + hts->count;
    for (p = w; p < eow; p++) {
      blit = *p;
      tag = blit & MASKCS;
      if (tag == TRNCS || tag == LRGCS) p++;
      if (tag != BINCS) continue;
      prev = -(blit >> RMSHFT);
      if (!lglevel (lgl, prev)) continue;
      if (lglval (lgl, prev) <= 0) continue;
      if ((prevmark = lglcamarked (lgl, prev)) < 0) continue;
      if (mark == prevmark) continue;
      if (prevmark == negmark) { res = prev; goto DONE; }
      lglcamark (lgl, prev, mark);
    }
  }
DONE:
  lglpopnunlcamarkstk (lgl, &lgl->lcaseen);
  LOG (3, "least common ancestor of %d and %d is %d", a, b, res);
  return res;
}

static void lglrmlwch (LGL * lgl, int lit, int red, int lidx) {
  int blit, tag, * p, * q, * w, * eow, ored, olidx;
  HTS * hts;
#ifndef NLGLOG
  p = lglidx2lits (lgl, LRGCS, red, lidx);
  LOG (3, "removing watch %d in %s[%d] %d %d %d %d%s",
       lit, (red ? "red" : "irr"), lidx, p[0], p[1], p[2], p[3],
       (p[4] ? " ..." : ""));
#endif
  hts = lglhts (lgl, lit);
  assert (hts->count >= 2);
  p = w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  lglrminc (lgl, w, eow);
  for (;;) {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == BINCS) continue;
    if (tag == OCCS) { assert (lgl->dense); continue; }
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

static void lglpropsearch (LGL * lgl, int lit) {
  int * q, * eos, blit, other, other2, other3, red, prev;
  int tag, val, val2, lidx, * c, * l;
  const int * p;
  int visits;
  long delta;
  HTS * hts;

  LOG (3, "propagating %d in search", lit);

  assert (!lgl->simp);
  assert (!lgl->lkhd);
  assert (!lgl->probing);
  assert (!lgl->lifting);
  assert (!lgl->cliffing);
  assert (!lgl->dense);
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
      *q++ = *++p;
    }
    other = (blit >> RMSHFT);
    val = lglval (lgl, other);
    if (val > 0) continue;
    red = blit & REDCS;
    if (tag == BINCS) {
      if (val < 0) { lglbonflict (lgl, -lit, blit); p++; break; }
      lglf2rce (lgl, other, -lit, red);
    } else if (tag == TRNCS) {
      other2 = *p;
      val2 = lglval (lgl, other2);
      if (val2 > 0) continue;
      if (!val && !val2) continue;
      if (val < 0 && val2 < 0) {
	lgltonflict (lgl, -lit, blit, other2);
	p++;
	break;
      }
      if (!val) SWAP (int, other, other2);
      else assert (val < 0);
      lglf3rce (lgl, other2, -lit, other, red);
    } else {
      assert (tag == LRGCS);
      assert (val <= 0);
      lidx = *p;
      c = lglidx2lits (lgl, LRGCS, red, lidx);
      other2 = c[0];
      if (other2 == -lit) other2 = c[0] = c[1], c[1] = -lit;
      else assert (c[1] == -lit);
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
      assert (!other2 || val2 >= 0);
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
  assert (!lgl->simp);

  lgl->stats->visits.search += visits;
}

static int lglhbred (LGL * lgl, int subsumed, int red) {
  int res = subsumed ? red : REDCS;
  LOG (3, "hyber binary learned clause becomes %s", lglred2str (res));
  return res;
}

static void lgldecocc (LGL *, int);	//TODO move scheduling ...

static void lglrmlocc (LGL * lgl, int lit, int red, int lidx) {
  int search, blit, tag, * p, * q, * w, * eow;
  HTS * hts;
  if (lgl->dense < 2) return;
#ifndef NLGLOG
  if (red) LOG (3, "removing occurrence %d in red[0][%d]", lit, lidx);
  else LOG (3, "removing occurrence %d in irr[%d]", lit, lidx);
#endif
  assert (!red || red == REDCS);
  hts = lglhts (lgl, lit);
  assert (hts->count >= 1);
  assert (lidx <= MAXIRRLIDX);
  search = (lidx << RMSHFT) | OCCS | red;
  assert (search >= 0);
  p = w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  lglrminc (lgl, w, eow);
  do {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
  } while (blit != search);
  assert (p[-1] == search);
  for (q = p ; q < eow; q++)
    q[-1] = q[0];
  lglshrinkhts (lgl, hts, q - w - 1);
}

static void lglflushremovedoccs (LGL * lgl, int lit) {
  HTS * hts = lglhts (lgl, lit);
  int * w = lglhts2wchs (lgl, hts);
  int * eow = w + hts->count;
  int blit, tag, red, lidx;
  int * p, * q, * c;
  lglrminc (lgl, w, eow);
  for (p = q = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    red = blit & REDCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == BINCS) *q++ = blit;
    else if (tag == TRNCS) *q++ = blit, *q++ = *p;
    else {
      assert (tag == LRGCS || tag == OCCS);
      if (!red) {
	lidx = (tag == LRGCS) ? *p : (blit >> RMSHFT);
	c = lglidx2lits (lgl, tag, red, lidx);
	if (c[0] == REMOVED) continue;
      }
      *q++ = blit;
      if (tag == LRGCS) *q++ = *p;
    }
  }
  lglshrinkhts (lgl, hts, q - w);
}

static void lglprop (LGL * lgl, int lit) {
  int * p, * q, * eos, blit, other, other2, other3, red, prev;
  int tag, val, val2, lidx, * c, * l, dom, hbred, subsumed;
  int glue, flushoccs;
  long delta;
  HTS * hts;
  LOG (3, "propagating %d over ternary and large clauses", lit);
  assert (!lgliselim (lgl, lit));
  assert (lglval (lgl, lit) == 1);
  hts = lglhts (lgl, -lit);
  if (!hts->offset) return;
  flushoccs = 0;
  q = lglhts2wchs (lgl, hts);
  assert (hts->count >= 0);
  eos = q + hts->count;
  for (p = q; p < eos; p++) {
    blit = *p;
    tag = blit & MASKCS;
    red = blit & REDCS;
    if (tag == OCCS) {
      assert (lgl->dense);
      assert (!red);
      *q++ = blit;
      continue;
    }
    other = (blit >> RMSHFT);
    val = lglval (lgl, other);
    if (tag == BINCS) {
      *q++ = blit;
      if (val > 0) continue;
      if (red && lgliselim (lgl, other)) continue;
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
      if (red && lgliselim (lgl, other)) continue;
      val2 = lglval (lgl, other2);
      if (val2 > 0) continue;
      if (!val && !val2) continue;
      if (red && lgliselim (lgl, other2)) continue;
      if (val < 0 && val2 < 0) {
	lgltonflict (lgl, -lit, blit, other2);
	p++;
	break;
      }
      if (!val) SWAP (int, other, other2); else assert (val < 0);
      if (lgl->level &&
	  lgl->simp &&
	  lgl->opts->lhbr.val &&
	  !lgl->cgrclosing) {
	assert (lgl->simp);
	dom = lglgetdom (lgl, lit);
	if (lglgetdom (lgl, -other) != dom) goto NO_HBR_JUST_F3RCE;
	dom = lglca (lgl, lit, -other);
	if (!dom) goto NO_HBR_JUST_F3RCE;
	subsumed = (dom == lit || dom == -other);
	hbred = lglhbred (lgl, subsumed, red);
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
	  lgl->stats->hbr.sub++;
	  if (red) assert (lgl->stats->red.trn), lgl->stats->red.trn--;
	  else {
	    lgldecirr (lgl, 3);
	    if (lgl->dense) {
	      if (-dom == -lit) lgldecocc (lgl, other);
	      else { assert (-dom == other); lgldecocc (lgl, -lit); }
	    }
	  }
	}
	delta = 0;
	if (dom == lit) {
	  LOG (3,
    "replacing %s ternary watch %d blits %d %d with binary %d blit %d",
	  lglred2str (red), -lit, other, other2, -dom, other2);
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
	    assert (abs (dom) != abs (lit));
	    assert (abs (other2) != abs (lit));
	  }
	  delta += lglwchbin (lgl, -dom, other2, hbred);
	}
	delta += lglwchbin (lgl, other2, -dom, hbred);
	if (delta) p += delta, q += delta, eos += delta;
	if (hbred) lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
	else lglincirr (lgl, 2);
	lglf2rce (lgl, other2, -dom, hbred);
	lgl->stats->hbr.trn++;
	lgl->stats->hbr.cnt++;
	lgl->stats->prgss++;
      } else {
NO_HBR_JUST_F3RCE:
       lglf3rce (lgl, other2, -lit, other, red);
      }
    } else {
      assert (tag == LRGCS);
      if (val > 0) goto COPYL;
      lidx = p[1];
      c = lglidx2lits (lgl, LRGCS, red, lidx);
      other2 = c[0];
      if (other2 >= NOTALIT) {
	p++;
	continue;
      }
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
      if (lgl->level &&
	  lgl->simp &&
	  lgl->opts->lhbr.val &&
	  !lgl->cgrclosing) {
	assert (lgl->simp);
	dom = 0;
	for (l = c; (other2 = *l); l++) {
	  if (other2 == other) continue;
	  if (!lglevel (lgl, other2)) continue;
	  assert (lglval (lgl, other2) < 0);
	  if (!dom) dom = lglgetdom (lgl, -other);
	  if (dom != lglgetdom (lgl, -other2)) goto NO_HBR_JUST_FLRCE;
	}
	LOGCLS (2, c, "dominator %d for %s clause", dom, lglred2str (red));
	dom = 0;
	for (l = c; (other2 = *l); l++) {
	  if (other2 == other) continue;
	  if (!lglevel (lgl, other2)) continue;
	  dom = lglca (lgl, dom, -other2);
	}
	if (!dom) goto NO_HBR_JUST_FLRCE;
	LOGCLS (2, c, "closest dominator %d", dom);
	subsumed = 0;
	for (l = c; !subsumed && (other2 = *l); l++)
	  subsumed = (dom == -other2);
	assert (lit != dom || subsumed);
	hbred = lglhbred (lgl, subsumed, red);
	LOG (2, "hyper binary resolved %s clause %d %d",
	     lglred2str (hbred), -dom, other);
#ifndef NLGLPICOSAT
	lglpicosatchkclsarg (lgl, -dom, other, 0);
#endif
	if (subsumed) {
	  LOGCLS (2, c, "subsumes %s large clause", lglred2str (red));
	  lglrmlwch (lgl, other, red, lidx);
	  lgl->stats->hbr.sub++;
	  if (red) {
	    glue = lidx & GLUEMASK;
	    if (glue != MAXGLUE) {
	      assert (lgl->stats->red.lrg);
	      lgl->stats->red.lrg--;
	      assert (lgl->stats->lir[glue].clauses > 0);
	      lgl->stats->lir[glue].clauses--;
	    }
	  }
	  if (!red && lgl->dense) {
	    for (l = c; (other2 = *l); l++) {
	      if (other2 != -lit) lglrmlocc (lgl, other2, 0, lidx);
	      if (other2 == -dom) continue;
	      if (other2 == other) continue;
	      lgldecocc (lgl, other2);
	    }
	    flushoccs++;
	  }
	  if (red && glue < MAXGLUE) { LGLCHKACT (c[-1]); c[-1] = REMOVED; }
	  for (l = c; *l; l++) *l = REMOVED;
	  if (!red) lgldecirr (lgl, l - c);
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
	if (hbred) lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
	else lglincirr (lgl, 2);
	lglf2rce (lgl, other, -dom, hbred);
	lgl->stats->hbr.lrg++;
	lgl->stats->hbr.cnt++;
	lgl->stats->prgss++;
	if (subsumed) continue;
      } else {
NO_HBR_JUST_FLRCE:
	lglflrce (lgl, other, red, lidx);
      }
COPYL:
      *q++ = blit;
      *q++ = *++p;
    }
  }
  while (p < eos) *q++ = *p++;
  lglshrinkhts (lgl, hts, hts->count - (p - q));
  if (flushoccs) lglflushremovedoccs (lgl, -lit);
}

static void lglprop2 (LGL * lgl, int lit) {
  int other, blit, tag, val, red, visits;
  const int * p, * w, * eow;
  HTS * hts;
  LOG (3, "propagating %d over binary clauses", lit);
  assert (!lgliselim (lgl, lit));
  assert (lglval (lgl, lit) == 1);
  visits = 0;
  hts = lglhts (lgl, -lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    visits++;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag != BINCS) continue;
    red = blit & REDCS;
    other = blit >> RMSHFT;
    if (lgliselim (lgl, other)) { assert (red); continue; }
    val = lglval (lgl, other);
    if (val > 0) continue;
    if (val < 0) { lglbonflict (lgl, -lit, blit); break; }
    lglf2rce (lgl, other, -lit, red);
  }

  if (lgl->lkhd) lgl->stats->visits.lkhd += visits;
  else if (lgl->simp) lgl->stats->visits.simp += visits;
  else lgl->stats->visits.search += visits;

  if (lgl->basicprobing) lgl->stats->prb.basic.steps += visits;
  if (lgl->cceing) lgl->stats->cce.steps += visits;
  if (lgl->cliffing) lgl->stats->cliff.steps += visits;
}

static int lglbcp (LGL * lgl) {
  int lit, trail, count;
  assert (!lgl->mt);
  assert (!lgl->conf.lit);
  assert (!lgl->notfullyconnected);
  count = 0;
  while (!lgl->conf.lit) {
    trail = lglcntstk (&lgl->trail);
    if (lgl->next2 < trail) {
      lit = lglpeek (&lgl->trail, lgl->next2++);
      lglprop2 (lgl, lit);
      continue;
    }
    if (lgl->next >= trail) break;
    count++;
    lit = lglpeek (&lgl->trail, lgl->next++);
    lglprop (lgl, lit);
  }
  if (lgl->lkhd) lgl->stats->props.lkhd += count;
  else if (lgl->simp) lgl->stats->props.simp += count;
  else lgl->stats->props.search += count;
  return !lgl->conf.lit;
}

static int lglbcpsearch (LGL * lgl) {
  int lit, count = 0;
  assert (!lgl->simp);
  assert (!lgl->notfullyconnected);
  while ((!lgl->failed || !lgl->level) &&
	 !lgl->conf.lit &&
	 lgl->next < lglcntstk (&lgl->trail)) {
    lit = lglpeek (&lgl->trail, lgl->next++);
    lglpropsearch (lgl, lit);
    count++;
  }
  lgl->stats->props.search += count;
  lgl->next2 = lgl->next;
  if (lgl->conf.lit && lgl->failed) {
    LOG (2, "inconsistency overwrites failed assumption %d", lgl->failed);
    lgl->failed = 0;
  }
  return !lgl->conf.lit && !lgl->failed;
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
  if (lgl->opts->check.val >= 1) return 1;
#endif
#ifndef NLGLOG
  if (lgl->opts->log.val >= 2) return 1;
#endif
  return 0;
}
#endif

static int lgldecision (LGL * lgl, int lit) {
  int * rsn = lglrsn (lgl, lit);
  int tag = rsn[0] & MASKCS;
  return tag == DECISION;
}

static int lglassumption (LGL * lgl, int lit) {
  return lglavar (lgl, lit)->assumed;
}

static int lglpull (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int level, res;
  level = lglevel (lgl, lit);
  if (!level) return 0;
  if (av->mark) return 0;
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
    LOG (2, "adding literal %d at upper level %d to 1st UIP clause",
	 lit, lglevel (lgl, lit));
    if (!lglevelused (lgl, level)) {
      lgluselevel (lgl, level);
      lglpushstk (lgl, &lgl->frames, level);
      LOG (2, "pulled in decision level %d", level);
    }
    res = 0;
  }
  return res;
}

static int lglpoison (LGL * lgl, int lit, Stk * stk) {
  AVar * av = lglavar (lgl, lit);
  int level, res;
  if (av->mark) res = 0;
  else {
    level = lglevel (lgl, lit);
    if (!level) res = 0;
    else {
      assert (level < lgl->level);
      if (lgldecision (lgl, lit)) res = 1;
      else if (!lglevelused (lgl, level)) res = 1;
      else {
	lgl->stats->poison.search++;
	if (av->poisoned) {
	  lgl->stats->poison.hits++;
	  res = 1;
	} else {
	  av->mark = 1;
	  lglpushstk (lgl, &lgl->seen, lit);
	  lglpushstk (lgl, stk, lit);
	  res = 0;
	}
      }
    }
  }
  if (res && !av->poisoned) {
    av->poisoned = 1;
    lglpushstk (lgl, &lgl->poisoned, lit);
  }
  return res;
}

static int lglminclslit (LGL * lgl, int start) {
  int lit, tag, r0, r1, other, * p, * q, *top, old;
  int poisoned, * rsn, found;
  AVar * av, * bv;
  Stk stk;
  assert (lglmarked (lgl, start));
  lit = start;
  rsn = lglrsn (lgl, lit);
  r0 = rsn[0];
  tag = (r0 & MASKCS);
  if (tag == DECISION) return 0;
  old = lglcntstk (&lgl->seen);
  CLR (stk);
  for (;;) {
    r1 = rsn[1];
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      if (lglpoison (lgl, other, &stk)) goto FAILED;
      if (tag == TRNCS && lglpoison (lgl, r1, &stk)) goto FAILED;
    } else {
      assert (tag == LRGCS);
      p = lglidx2lits (lgl, LRGCS, (r0 & REDCS), r1);
      found = 0;
      while ((other = *p++)) {
	if (other == -lit) found++;
	else if (lglpoison (lgl, other, &stk)) goto FAILED;
      }
      assert (found == 1);
    }
    if (lglmtstk (&stk)) { lglrelstk (lgl, &stk); return 1; }
    lit = lglpopstk (&stk);
    assert (lglavar (lgl, lit)->mark);
    rsn = lglrsn (lgl, lit);
    r0 = rsn[0];
    tag = (r0 & MASKCS);
  }
FAILED:
  lglrelstk (lgl, &stk);
  p = lgl->seen.top;
  top = lgl->seen.top = lgl->seen.start + old;
  while (p > top) {
    lit = *--p;
    av = lglavar (lgl, lit);
    assert (av->mark);
    av->mark = 0;
    poisoned = av->poisoned;
    if (poisoned) continue;
    rsn = lglrsn (lgl, lit);
    r0 = rsn[0];
    tag = (r0 & MASKCS);
    assert (tag != DECISION);
    r1 = rsn[1];
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      bv = lglavar (lgl, other);
      if (bv->poisoned) poisoned = 1;
      else if (tag == TRNCS) {
	bv = lglavar (lgl, r1);
	if (bv->poisoned) poisoned = 1;
      }
    } else {
      assert (tag == LRGCS);
      q = lglidx2lits (lgl, LRGCS, (r0 & REDCS), r1);
      while (!poisoned && (other = *q++))
	poisoned = lglavar (lgl, other)->poisoned;
    }
    if (!poisoned) continue;
    assert (!av->poisoned);
    av->poisoned = 1;
    lglpushstk (lgl, &lgl->poisoned, lit);
  }
  return 0;
}

double lglprocesstime (void) {
  struct rusage u;
  double res;
  if (getrusage (RUSAGE_SELF, &u)) return 0;
  res = u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec;
  res += u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
  return res;
}

static double lglgetime (LGL * lgl) {
  if (lgl->cbs && lgl->cbs->getime) return lgl->cbs->getime ();
  else return lglprocesstime ();
}

static void lglstart (LGL * lgl, double * timestatsptr) {
  int nest = lgl->timers->nest;
  assert (timestatsptr);
  assert (nest < MAXPHN);
  assert ((double*) lgl->times <= timestatsptr);
  assert (timestatsptr < (double*)(sizeof *lgl->times + (char*) lgl->times));
  lgl->timers->idx[nest] = timestatsptr - (double*)lgl->times;
  lgl->timers->phase[nest] = lglgetime (lgl);
  lgl->timers->nest++;
}

void lglflushtimers (LGL * lgl) {
  double time = lglgetime (lgl), delta, entered, * ptr;
  int nest;
  for (nest = 0; nest < lgl->timers->nest; nest++) {
    entered = lgl->timers->phase[nest];
    lgl->timers->phase[nest] = time;
    delta = time - entered;
    if (delta < 0) delta = 0;
    ptr = lgl->timers->idx[nest] + (double*)lgl->times;
    *ptr += delta;
  }
}

double lglsec (LGL * lgl) {
  REQINIT ();
  lglflushtimers (lgl);
  return lgl->times->all;
}

static void lglstop (LGL * lgl) {
  lglflushtimers (lgl);
  lgl->timers->nest--;
  assert (lgl->timers->nest >= 0);
}

double lglmaxmb (LGL * lgl) {
  REQINIT ();
  return (lgl->stats->bytes.max + sizeof *lgl) / (double)(1<<20);
}

size_t lglbytes (LGL * lgl) {
  REQINIT ();
  return lgl->stats->bytes.current;
}

double lglmb (LGL * lgl) {
  REQINIT ();
  return (lgl->stats->bytes.current + sizeof *lgl) / (double)(1<<20);
}

static double lglagility (LGL * lgl) { return lgl->flips/1e7; }

static double lglavg (double n, double d) {
  return d != 0 ? n / d : 0.0;
}

static double lglheight (LGL * lgl) {
  return lglavg (lgl->stats->height, lgl->stats->decisions);
}

static int64_t lglavglue (LGL * lgl) {
  int count = lgl->stats->glues.count;
  if (!count) return 0;
  return (100*lgl->stats->glues.sum) / count;
}

static void lglrephead (LGL * lgl) {
  if (lgl->tid > 0) return;
  if (lgl->cbs && lgl->cbs->msglock.lock)
    lgl->cbs->msglock.lock (lgl->cbs->msglock.state);
  assert (lgl->prefix);
  fprintf (lgl->out, "%s\n", lgl->prefix);
  fprintf (lgl->out, "%s%s"
" seconds         irredundant          redundant clauses agility  "
" height\n", lgl->prefix, !lgl->tid ? "  " : "");
  fprintf (lgl->out, "%s%s"
"         variables clauses conflicts large binary ternary    "
"glue         MB\n", lgl->prefix, !lgl->tid ? "  " : "");
  fprintf (lgl->out, "%s\n", lgl->prefix);
  fflush (lgl->out);
  if (lgl->cbs && lgl->cbs->msglock.unlock)
    lgl->cbs->msglock.unlock (lgl->cbs->msglock.state);
}

static void lglrep (LGL * lgl, int level, char type) {
  if (lgl->opts->verbose.val < level) return;
  if (!(lgl->stats->reported % REPMOD)) lglrephead (lgl);
  lglprt (lgl, 1,
	  "%c %6.1f %7d %8d %9lld %7d %6d %5d %3.0f %4.1f %5.1f %4.0f",
	  type,
	  lgl->opts->abstime.val ? lglgetime (lgl) : lglsec (lgl),
	  lglrem (lgl),
	  lgl->stats->irr.clauses.cur,
	  (LGLL) lgl->stats->confs,
	  lgl->stats->red.lrg,
	  lgl->stats->red.bin,
	  lgl->stats->red.trn,
	  lglagility (lgl),
	  lglavglue (lgl)/100.0,
	  lglheight (lgl),
	  lglmb (lgl));
  lgl->stats->reported++;
}

static void lglflshrep (LGL * lgl) {
  if (!lgl->stats->reported) return;
  if (lgl->stats->reported % REPMOD) lglrephead (lgl);
  else lglprt (lgl, 1, "");
}

static void lglfitlir (LGL * lgl, Stk * lir) {
  lglfitstk (lgl, lir);
}

static void lglchkred (LGL * lgl) {
#ifndef NDEBUG
  int glue, idx, sign, lit, thisum, sum, sum2, sum3;
  int blit, tag, red, other, other2;
  int * p, * c, * w, * eow;
  HTS * hts;
  Stk * lir;
  if (lgl->mt) return;
  sum = 0;
  for (glue = 0; glue < MAXGLUE; glue++) {
    lir = lgl->red + glue;
    thisum = 0;
    for (c = lir->start; c < lir->top; c = p + 1) {
      p = c;
      if (*p >= NOTALIT) continue;
      while (*p) p++;
      assert (p - c >= 4);
      thisum++;
    }
    assert (thisum == lgl->stats->lir[glue].clauses);
    sum += thisum;
  }
  assert (sum == lgl->stats->red.lrg);
  sum2 = sum3 = 0;
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
	if (!red || tag == LRGCS || tag == OCCS) continue;
	other = blit >> RMSHFT;
	if (abs (other) < idx) continue;
	if (tag == BINCS) sum2++;
	else {
	  assert (tag == TRNCS);
	  other2 = *p;
	  if (abs (other2) < idx) continue;
	  sum3++;
	}
      }
    }
  assert (sum2 == lgl->stats->red.bin);
  assert (sum3 == lgl->stats->red.trn);
#endif
}

static int lglcmpagsl (ASL * a, ASL * b) {
  int res;
  if ((res = (a->act - b->act))) return res;
  if ((res = ((b->lidx & GLUEMASK) - (a->lidx & GLUEMASK)))) return res;
  if ((res = (b->size - a->size))) return res;
  return a->lidx - b->lidx;
}

static int lglcmpasgl (ASL * a, ASL * b) {
  int res;
  if ((res = (a->act - b->act))) return res;
  if ((res = (b->size - a->size))) return res;
  if ((res = ((b->lidx & GLUEMASK) - (a->lidx & GLUEMASK)))) return res;
  return a->lidx - b->lidx;
}

static int lglneedacts (LGL * lgl,
			int * glueuselessptr,
			int * needmoreglueptr) {
  int64_t clauses = 0, weighted = 0, tmp, avg, var = 0, std, delta;
  int glue, maxglue = 0;
  for (glue = 0; glue <= MAXGLUE; glue++) {
    tmp = lgl->stats->lir[glue].clauses;
    if (tmp) maxglue = glue;
    clauses += tmp;
    weighted += glue * tmp;
  }
  avg = clauses ? (10*weighted)/clauses : 0;
  lglprt (lgl, 2,
	  "[needacts-%d] existing clauses glue average %.1f",
	  lgl->stats->reduced, avg/10.0);
  for (glue = 1; glue <= MAXGLUE; glue++) {
    delta = (10*glue - avg);
    var += delta*delta * lgl->stats->lir[glue].clauses;
  }
  var = clauses ? var / clauses : 0;
  std = lglceilsqrt64 (var);
  lglprt (lgl, 2,
	  "[needacts-%d] existing clauses glue standard deviation %.1f",
	  lgl->stats->reduced, std/10.0);
  *glueuselessptr = (std < 10);
  if (maxglue <= 3) *needmoreglueptr = 2;
  else if (maxglue <= 5) *needmoreglueptr = 1;
  else *needmoreglueptr = 1;
  if (avg > lgl->opts->actavgmax.val) return 1;
  if (std <= lgl->opts->actstdmin.val) return 1;
  if (std > lgl->opts->actstdmax.val) return 1;
  return 0;
}

static int lglagile (LGL * lgl) {
  return lgl->flips >= lgl->opts->agile.val * 10000000ull;
}

static void lglboundredl (LGL * lgl) {
  int64_t minabs, minrel, maxabs, maxrel, oldlim, newlim;
  // int64_t minfac;

  newlim = oldlim = lgl->limits->reduce.inner;
  if (!lgl->opts->redlbound.val) goto SKIP;

  lglprt (lgl, 2, "preliminary reduce limit %d before bounding", oldlim);

  minrel = lgl->opts->redlminrel.val;
  minrel *= (lgl->stats->irr.clauses.cur + 99ll)/100ll;
  minabs = lgl->opts->redlminabs.val;

  lglprt (lgl, 2, "minrel = %d, minabs = %d", minrel, minabs);

  if (minrel < minabs) {
    if (minrel > lgl->limits->reduce.inner) {
      lglprt (lgl, 2, "relative minimum reduce limit %d hit", minrel);
      newlim = minrel;
    }
  } else {
    if (minabs > lgl->limits->reduce.inner) {
      lglprt (lgl, 2, "absolute minimum reduce limit of %d hit", minabs);
      newlim = minabs;
    }
  }

  maxrel = lgl->opts->redlmaxrel.val;
  maxrel *= (lgl->stats->irr.clauses.cur + 99ll)/100ll;
  maxrel += lgl->opts->redlmininc.val * (int64_t) lgl->limits->reduce.extra;
  maxabs = lgl->opts->redlmaxabs.val;

  lglprt (lgl, 2, "maxrel = %d, maxabs = %d", maxrel, maxabs);

  if (maxrel < maxabs) {
    lgl->limits->reduce.extra++;
    if (maxrel < lgl->limits->reduce.inner) {
      lglprt (lgl, 2, "relative maximum reduce limit %d hit", maxrel);
      newlim = maxrel;
    }
  } else {
    if (maxabs < lgl->limits->reduce.inner) {
      lglprt (lgl, 2, "absolute maximum reduce limit of %d hit", maxabs);
      newlim = maxabs;
    }
  }

SKIP:

  lgl->limits->reduce.inner = newlim;
  lglprt (lgl, 2,
    "new reduce limit of %d redundant clauses after %lld conflicts",
    lgl->limits->reduce.inner, (LGLL) lgl->stats->confs);
}

static int64_t lglubyrec (LGL * lgl, int i) {
  int64_t res = 0, s = 0;
  int k;

  for (k = 1; !res && k < 32; s++, k++) {
    if (i == (1 << k) - 1)
      res = 1 << (k - 1);
  }

  for (k = 1; !res; k++, s++)
    if ((1 << (k - 1)) <= i && i < (1 << k) - 1)
      res = lglubyrec (lgl, i - (1 << (k-1)) + 1);

  lgl->stats->luby.steps += s;

  assert (res > 0);
  return res;
}

static int64_t lgluby (LGL * lgl, int i) {
  assert (i > 0);
  lgl->stats->luby.count++;
  return lglubyrec (lgl, i);
}

static int64_t lglinout (LGL * lgl, int c, int relincpcnt) {
  int64_t i = 1, o = 1;

  assert (c > 0);

  lgl->stats->inout.count++;
  lgl->stats->inout.steps += c;

  while (c-- > 0) {
    i = ((100 + relincpcnt)*i + 99)/100;
    if (i < o) continue;
    i = 1;
    o = ((100 + relincpcnt)*o + 99)/100;
  }

  assert (i > 0);
  return i;
}

static void lglreduce (LGL * lgl, int forced) {
  int * p, * q, * start, * c, ** maps, * sizes, * map, * eow, * rsn;
  int nlocked, collected, sumcollected, nunlocked, moved, act;
  int glue, minredglue, maxredglue, target, rem, nkeep;
  int inc, acts, glueuseless, needmoreclauses, delta;
  ASL * asls, * asl; int nasls, szasls;
  int size, idx, tag, red, i, blit;
  int r0, lidx, src, dst, lit;
  char type = '-';
  int64_t outer;
  HTS * hts;
  DVar * dv;
  Stk * lir;
  lglchkred (lgl);
  lglstart (lgl, &lgl->times->red);
  lgl->stats->reduced.count++;
  LOG (1, "starting reduction %d", lgl->stats->reduced.count);
  acts = lglneedacts (lgl, &glueuseless, &needmoreclauses);
  delta = lgl->stats->red.lrg;
  delta -= lgl->lrgluereasons;
  assert (delta >= 0);
  if (delta > 3*lgl->limits->reduce.inner/2)
    target = delta - lgl->limits->reduce.inner/2;
  else target = delta/2;
  rem = target;
  LOG (2, "target is to collect %d clauses out of %d", target, delta);
  for (maxredglue = MAXGLUE-1; maxredglue >= 0; maxredglue--)
    if (lgl->stats->lir[maxredglue].clauses > 0) break;
  LOG (2, "maximum reduction glue %d", maxredglue);
  if (lgl->opts->acts.val < 2) acts = lgl->opts->acts.val;
  if (acts) {
    lgl->stats->acts++;
    lglprt (lgl, 2,
	    "[needacts-%d] using primarily activities for reduction",
	    lgl->stats->reduced);
    szasls = lgl->stats->red.lrg;
    minredglue = 1;
  } else {
    lglprt (lgl, 2,
	    "[needacts-%d] using primarily glues for reduction",
	    lgl->stats->reduced);
    asls = 0;
    if (maxredglue > 0) {
      for (minredglue = maxredglue;  minredglue > 1;  minredglue--) {
	LOG (2, "%d candidate clauses with glue %d",
	     lgl->stats->lir[minredglue].clauses, minredglue);
	if (lgl->stats->lir[minredglue].clauses >= rem) break;
	rem -= lgl->stats->lir[minredglue].clauses;
      }
    } else minredglue = 1;
    szasls = lgl->stats->lir[minredglue].clauses;
  }
  LOG (2, "minum reduction glue %d with %d remaining target clauses %.0f%%",
       minredglue, rem, lglpcnt (rem, target));
  NEW (maps, maxredglue + 1);
  NEW (sizes, maxredglue + 1);
  for (glue = minredglue; glue <= maxredglue; glue++) {
    lir = lgl->red + glue;
    size = lglcntstk (lir);
    assert (!size || size >= 6);
    size = (size + 5)/6;
    sizes[glue] = size;
    lglfitstk (lgl, lir);
    NEW (maps[glue], size);
    map = maps[glue];
    for (i = 0; i < size; i++) map[i] = -2;
  }
  nlocked = 0;
  for (i = 0; i < lglcntstk (&lgl->trail); i++) {
    lit = lglpeek (&lgl->trail, i);
    idx = abs (lit);
    rsn = lglrsn (lgl, idx);
    r0 = rsn[0];
    red = r0 & REDCS;
    if (!red) continue;
    tag = r0 & MASKCS;
    if (tag != LRGCS) continue;
    lidx = rsn[1];
    glue = lidx & GLUEMASK;
    if (glue == MAXGLUE) continue;
    if (glue < minredglue) continue;
    if (glue > maxredglue) continue;
    lidx >>= GLUESHFT;
#ifndef NLGLOG
    lir = lgl->red + glue;
    LOGCLS (5, lir->start + lidx,
	    "locking reason of literal %d glue %d clause",
	    lit, glue);
#endif
    lidx /= 6;
    assert (lidx < sizes[glue]);
    assert (maps[glue][lidx] == -2);
    maps[glue][lidx] = -1;
    nlocked++;
  }
  LOG (2, "locked %d learned clauses %.0f%%",
       nlocked, lglpcnt (nlocked, lgl->stats->red.lrg));
  NEW (asls, szasls);
  nasls = 0;
  for (glue = minredglue; glue <= maxredglue; glue++) {
    lir = lgl->red + glue;
    start = lir->start;
    for (c = start; c < lir->top; c = p + 1) {
      act = *c;
      if (act == REMOVED) {
	for (p = c + 1; p < lir->top && *p == REMOVED; p++)
	  ;
	p--;
	continue;
      }
      for (p = ++c; *p; p++)
	;
      assert (nasls < szasls);
      asl = asls + nasls++;
      asl->act = act;
      asl->size = p - c;
      asl->lidx = ((c - start) << GLUESHFT) | glue;
    }
    if (!acts) { assert (glue == minredglue); break; }
  }
  assert (nasls <= szasls);
  if (glueuseless) SORT (ASL, asls, nasls, lglcmpasgl);
  else SORT (ASL, asls, nasls, lglcmpagsl);
  LOG (1, "copied and sorted %d activities", nasls);
  nkeep = 0;
  for (idx = rem; idx < nasls; idx++) {
    asl = asls + idx;
    lidx = asl->lidx;
    glue = lidx & GLUEMASK;
    lidx >>= GLUESHFT;
    lidx /= 6;
    assert (lidx < sizes[glue]);
    maps[glue][lidx] = -1;
    nkeep++;
  }
  DEL (asls, szasls);
  LOG (1, "explicity marked %d additional clauses to keep", nkeep);
  sumcollected = 0;
  for (glue = minredglue; glue <= maxredglue; glue++) {
    lir = lgl->red + glue;
    map = maps[glue];
#ifndef NDEBUG
    size = sizes[glue];
#endif
    q = start = lir->start;
    collected = 0;
    for (c = start; c < lir->top; c = p + 1) {
      act = *c++;
      if (act == REMOVED) {
	for (p = c; p < lir->top && *p == REMOVED; p++)
	  ;
	assert (p >= lir->top || *p < NOTALIT || lglisact (*p));
	p--;
	continue;
      }
      p = c;
      LGLCHKACT (act);
      src = (c - start)/6;
      assert (src < size);
      if (map[src] == -2) {
	assert (collected < lgl->stats->lir[glue].clauses);
	collected++;
	LOGCLS (5, c, "collecting glue %d clause", glue);
	while (*p) p++;
      } else {
	dst = q - start + 1;
	map[src] = dst;
	if (p == q) {
	  while (*p) p++;
	  q = p + 1;
	} else {
	  *q++ = act;
	  LOGCLS (5, c, "moving from %d to %d glue %d clause",
		  (c - start), dst, glue);
	  while (*p) *q++ = *p++;
	  *q++ = 0;
	}
      }
    }
    LOG (2, "collected %d glue %d clauses", collected, glue);
    sumcollected += collected;
    assert (sumcollected <= lgl->stats->red.lrg);
    assert (lgl->stats->lir[glue].clauses >= collected);
    lgl->stats->lir[glue].clauses -= collected;
    lgl->stats->lir[glue].reduced += collected;
    lir->top = q;
    lglfitlir  (lgl, lir);
  }
  LOG (2, "collected altogether %d clauses", sumcollected);
  assert (sumcollected <= lgl->stats->red.lrg);
  lgl->stats->red.lrg -= sumcollected;
  nunlocked = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglval (lgl, idx)) continue;
    rsn = lglrsn (lgl, idx);
    r0 = rsn[0];
    red = r0 & REDCS;
    if (!red) continue;
    tag = r0 & MASKCS;
    if (tag != LRGCS) continue;
    lidx = rsn[1];
    glue = lidx & GLUEMASK;
    if (glue < minredglue) continue;
    if (glue > maxredglue) continue;
    src = (lidx >> GLUESHFT);
    assert (src/6 < sizes[glue]);
    dst = maps[glue][src/6];
    assert (dst >= 0);
    dst <<= GLUESHFT;
    dst |= lidx & GLUEMASK;
    rsn[1] = dst;
    nunlocked++;
  }
  LOG (2, "unlocked %d reasons", nunlocked);
  assert (nlocked == nunlocked);
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
	  if (glue < minredglue || glue > maxredglue) {
	    dst = lidx >> GLUESHFT;
	  } else {
	    src = lidx >> GLUESHFT;
	    assert (src/6 < sizes[glue]);
	    dst = maps[glue][src/6];
	  }
	  if (dst >= 0) {
	    moved++;
	    *q++ = blit;
	    *q++ = (dst << GLUESHFT) | (lidx & GLUEMASK);
	  } else collected++;
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
  LOG (1, "moved %d and collected %d occurrences", moved, collected);
  for (glue = minredglue; glue <= maxredglue; glue++)
    DEL (maps[glue], sizes[glue]);
  DEL (sizes, maxredglue+1);
  DEL (maps, maxredglue+1);
  if (lgl->opts->redfixed.val) goto NOINC;
  if (!needmoreclauses) {
    inc = lgl->opts->redlinc.val;
    lglprt (lgl, 2, "arithmetic increase of reduce limit");
    lgl->stats->reduced.arith++;
  } else if (needmoreclauses == 1) {
    inc = 2*lgl->opts->redlinc.val;
    lglprt (lgl, 2, "double arithmetic increase of reduce limit");
    lgl->stats->reduced.arith2++;
    lgl->stats->reduced.arith++;
  } else {
    inc = (lgl->opts->redlexpfac.val * lgl->limits->reduce.inner + 99)/100;
    if (!inc) inc++;
    lglprt (lgl, 2, "exponential increase of reduce limit");
    lgl->stats->reduced.geom++;
  }
  LOG (1, "reduce increment %d", inc);
  lgl->limits->reduce.inner += inc;
  assert (forced || lgl->opts->reduce.val);
  if  (lgl->opts->reduce.val > 1 &&
       lgl->limits->reduce.inner >= lgl->limits->reduce.outer) {
    type = '/';
    lgl->stats->reduced.reset++;
    lgl->limits->reduce.inner = lgl->opts->redlinit.val;
    lglboundredl (lgl);
    if (lgl->opts->reduce.val == 2) {
      outer = lgl->limits->reduce.inner;
      outer <<= lgl->opts->redldoutfac.val;
      outer *= lgluby (lgl, lgl->stats->reduced.reset);
    } else if (lgl->opts->reduce.val == 3) {
      outer = lgl->limits->reduce.inner;
      outer <<= lgl->opts->redldoutfac.val;
      outer *= lglinout (lgl, lgl->stats->reduced.reset,
                         lgl->opts->redinoutinc.val);
    } else {
      assert (lgl->opts->reduce.val == 4);
      outer = lgl->limits->reduce.outer + lgl->opts->redloutinc.val;
      if (outer < lgl->limits->reduce.inner)
	outer = lgl->limits->reduce.inner;
    }
    if (outer > (int64_t) INT_MAX) outer = INT_MAX;
    lgl->limits->reduce.outer = outer;
  } else lglboundredl (lgl);
NOINC:
  lglrep (lgl, 1, type);
  lglchkred (lgl);
  lglstop (lgl);
}

void lglflushcache (LGL * lgl) {
  REQINIT ();
  TRAPI ("flush");
  if (lgl->mt) return;
  lgl->limits->reduce.inner = lgl->opts->redlinit.val;
  lglboundredl (lgl);
  lglreduce (lgl, 1);
  lgl->limits->reduce.inner = lgl->opts->redlinit.val;
  lglboundredl (lgl);
  lgl->limits->reduce.outer = 2*lgl->limits->reduce.inner;
  lglprt (lgl, 1, "[flush-cache] new limit %d", lgl->limits->reduce.inner);
  if (lgl->clone) lglflushcache (lgl->clone);
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
  lglrminc (lgl, w, eow);
  blit1 = (other << RMSHFT) | red | BINCS;
  for (;;) {
    assert (p < eow);
    blit = *p++;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) { p++; continue; }
    if (tag == OCCS) { assert (lgl->dense); continue; }
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
  assert (!lglifrozen (lgl, res));
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
  int idx, sign, change;
  EVar * ev;
  if (lgl->cgrclosing || lgl->probing || lgl->gaussing) return;
  assert (lgl->blocking || lgl->eliminating || lgl->cceing);
  idx = abs (lit), sign = (lit < 0);
  ev = lglevar (lgl, lit);
  assert (ev->occ[sign] > 0);
  ev->occ[sign] -= 1;
  if (!lglisfree (lgl, lit)) return;
  change = lglecalc (lgl, ev);
  LOG (3, "dec occ of %d now occs[%d] = %d %d",
       lit, idx,  ev->occ[0], ev->occ[1]);
  if (ev->pos < 0) lglesched (lgl, idx);
  else if (change < 0) lgleup (lgl, idx);
  else if (change > 0) lgledown (lgl, idx);
}

static void lglrmbcls (LGL * lgl, int a, int b, int red) {
  lglrmbwch (lgl, a, b, red);
  lglrmbwch (lgl, b, a, red);
  LOG (2, "removed %s binary clause %d %d", lglred2str (red), a, b);
  lgldeclscnt (lgl, 2, red, 0);
  if (!red && lgl->dense) lgldecocc (lgl, a), lgldecocc (lgl, b);
}

static void lglrmtcls (LGL * lgl, int a, int b, int c, int red) {
  lglrmtwch (lgl, a, b, c, red);
  lglrmtwch (lgl, b, a, c, red);
  lglrmtwch (lgl, c, a, b, red);
  LOG (2, "removed %s ternary clause %d %d %d", lglred2str (red), a, b, c);
  lgldeclscnt (lgl, 3, red, 0);
  if (!red && lgl->dense)
    lgldecocc (lgl, a), lgldecocc (lgl, b), lgldecocc (lgl, c);
}

static void lglrmlcls (LGL * lgl, int lidx, int red) {
  int * c, * p, glue, lit;
  assert (!red || red == REDCS);
  glue = red ? (lidx & GLUEMASK) : 0;
  c = lglidx2lits (lgl, LRGCS, red, lidx);
  if (!red || glue < MAXGLUE) {
    lglrmlwch (lgl, c[0], red, lidx);
    lglrmlwch (lgl, c[1], red, lidx);
  }
  if (!red && lgl->dense) {
    for (p = c; (lit = *p); p++) {
      lglrmlocc (lgl, lit, red, lidx);
      lgldecocc (lgl, lit);
    }
  }
  if (red && glue < MAXGLUE) { LGLCHKACT (c[-1]); c[-1] = REMOVED; }
  for (p = c; *p; p++) *p = REMOVED;
  *p = REMOVED;
  if (glue != MAXGLUE) lgldeclscnt (lgl, p - c, red, glue);
}

static void lgldynsub (LGL * lgl, int lit, int r0, int r1) {
  int red, tag;
  tag = r0 & MASKCS;
  LOGREASON (2, lit, r0, r1, "removing subsumed");
  red = r0 & REDCS;
  if (red) lgl->stats->otfs.sub.dyn.red++;
  else lgl->stats->otfs.sub.dyn.irr++;
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
  int * p, * c, lidx, other, red, tag, glue, other2, other3, blit;
  tag = r0 & MASKCS;
  LOGREASON (2, lit, r0, r1, "strengthening by removing %d from", del);
  red = r0 & REDCS;
  if (red) lgl->stats->otfs.str.dyn.red++;
  else lgl->stats->otfs.str.dyn.irr++;
  lgl->stats->prgss++;
  if (!red) lgl->stats->irrprgss++;
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
    if (!red) lglincirr (lgl, 2);
    else lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
    lglwchbin (lgl, other, other2, red);
    lglwchbin (lgl, other2, other, red);
    if (lglevel (lgl, other) < lglevel (lgl, other2)) SWAP (int, other, other2);
    blit = (other2 << RMSHFT) | BINCS | red;
    lglbonflict (lgl, other, blit);
    return;
  }
  assert (tag == LRGCS);
  lidx = r1;
  glue = red ? (lidx & GLUEMASK) : 0;
  c = lglidx2lits (lgl, LRGCS, red, lidx);
  for (p = c; *p != del; p++)
    assert (*p);
  if (glue < MAXGLUE) {
    lglrmlwch (lgl, c[0], red, lidx);
    lglrmlwch (lgl, c[1], red, lidx);
  }
  while ((other = *++p)) p[-1] = other;
  p[-1] = 0, *p = REMOVED;
  if (!red) assert (lgl->stats->irr.lits.cur), lgl->stats->irr.lits.cur--;
  lglorderclsaux (lgl, c + 0);
  lglorderclsaux (lgl, c + 1);
#ifndef NLGLPICOSAT
  lglpicosatchkclsaux (lgl, c);
#endif
  assert (p - c > 3);
  if (p - c == 4) {
    assert (glue != MAXGLUE && !c[3] && c[4] >= NOTALIT);
    other = c[0], other2 = c[1], other3 = c[2];
    if (red && glue < MAXGLUE) { LGLCHKACT (c[-1]); c[-1] = REMOVED; }
    c[0] = c[1] = c[2] = c[3] = REMOVED;
    if (lglevel (lgl, other2) < lglevel (lgl, other3))
      SWAP (int, other2, other3);
    if (lglevel (lgl, other) < lglevel (lgl, other2))
      SWAP (int, other, other2);
    lglwchtrn (lgl, other, other2, other3, red);
    lglwchtrn (lgl, other2, other, other3, red);
    lglwchtrn (lgl, other3, other, other2, red);
    if (red) {
      assert (lgl->stats->red.lrg > 0);
      lgl->stats->red.lrg--;
      assert (lgl->stats->lir[glue].clauses > 0);
      lgl->stats->lir[glue].clauses--;
      lgl->stats->red.trn++;
      assert (lgl->stats->red.trn > 0);
    }
    lgltonflict (lgl, other, (other2 << RMSHFT) | red | TRNCS, other3);
  } else {
    if (glue < MAXGLUE) {
      LOG (3, "new head literal %d", c[0]);
      (void) lglwchlrg (lgl, c[0], c[1], red, lidx);
      LOG (3, "new tail literal %d", c[1]);
      (void) lglwchlrg (lgl, c[1], c[0], red, lidx);
    }
    lglonflict (lgl, 0, c[0], red, lidx);
  }
}

static void lglclnframes (LGL * lgl) {
  while (!lglmtstk (&lgl->frames))
    lglunuselevel (lgl, lglpopstk (&lgl->frames));
}

static void lglclnpoisoned (LGL * lgl) {
  AVar * av;
  int lit;
  while (!lglmtstk (&lgl->poisoned)) {
    lit = lglpopstk (&lgl->poisoned);
    av = lglavar (lgl, lit);
    assert (!av->mark);
    assert (av->poisoned);
    av->poisoned = 0;
  }
}

static void lglclnana (LGL * lgl) {
#ifdef RESOLVENT
  if (lglmaintainresolvent  (lgl)) lglclnstk (&lgl->resolvent);
#endif
  lglclnstk (&lgl->clause);
  lglpopnunmarkstk (lgl, &lgl->seen);
  lglclnframes (lgl);
}

static void lglflushqmerged (LGL * lgl) {
  Qln * p, * up;
  int i;
  if (!lgl->qscheduling) return;
  if (!lgl->queue.merged) return;
  assert (lgl->queue.nmerged);
  lgl->stats->queue.gcs++;
  LOG (2, "flushing %d merged queues", lgl->queue.nmerged);
  for (i = 2; i < lgl->nvars; i++) (void) lglqln (lgl, i);
  for (p = lgl->queue.merged; p; p = up) {
    up = p->up;
    assert (lgl->queue.nmerged > 0);
    lgl->stats->queue.col++;
    lgl->queue.nmerged--;
    assert (lgl->queue.nlines > 0);
    lgl->queue.nlines--;
    assert (lgl->queue.unassigned != p);
    DEL (p, 1);
  }
  assert (!lgl->queue.nmerged);
  lgl->queue.merged = 0;
}

static void lgldeprioritize (LGL * lgl) {
  int old_prior, new_prior, first, last;
  Qln * p, * down, * up;
  int64_t new_prior64;
  lgl->stats->queue.deprior.count++;
  for (p = lgl->queue.bottom; p; p = p->up) {
    old_prior = p->prior;
    lgl->stats->queue.deprior.sum++;
    if (!old_prior) continue;
    new_prior64 = old_prior;
    new_prior64 *= lgl->opts->queuefactor.val;
    new_prior64 += 999;
    new_prior64 /= 1000;
    assert (0 < new_prior64 && new_prior64 <= (int64_t) INT_MAX);
    new_prior = new_prior64;
    if (new_prior == old_prior) continue;
    assert (new_prior < old_prior);
    LOG (2, "queue[%d] reprioritized to queue[%d]", old_prior, new_prior);
    p->prior = new_prior;
  }
  for (p = lgl->queue.bottom; p; p = up) {
    up = p->up;
    if (!(down = p->down)) continue;
    if (down->prior < p->prior) continue;
    assert (down->prior == p->prior);
    LOG (2, "merging queue[%d]", down->prior);
    down->up = up;
    if (up) up->down = down;
    else assert (lgl->queue.top == p), lgl->queue.top = down;
    first = down->first;
    assert (first);
    down->first = p->first;
    if (p->unassigned) down->unassigned = p->unassigned;
    if (lgl->queue.unassigned == p) lgl->queue.unassigned = down;
    last = p->last;
    assert (last);
    p->first = p->last = 0;
    lglqnd (lgl, first)->prev = last;
    lglqnd (lgl, last)->next = first;
    assert (p->repr == p);
    p->repr = down;
    p->down = 0;
    p->up = lgl->queue.merged;
    lgl->queue.merged = p;
    lgl->queue.nmerged++;
    lgl->stats->queue.merged++;
    if (lgl->queue.nmerged >= lgl->opts->queuemergelim.val)
      lglflushqmerged (lgl);
  }
  lglchkqueue (lgl);
  if (lgl->opts->log.val >= 5) lglqdump (lgl);
}

static void lglvmtf (LGL * lgl, int lit) {
  int oldprior, newprior, inc;
  Qln * src, * dst, * l;
  Qnd * node;
  assert (lgl->qscheduling);
  lit = abs (lit);
  src = lglqln (lgl, lit);
  oldprior = src->prior;
  inc = lgl->opts->queueinc.val;
  assert (oldprior + inc <= INT_MAX);
  newprior = oldprior + inc;
  if (newprior > lgl->stats->queue.max) {
    lgl->stats->queue.max = newprior;
    lglprt (lgl, 2, "[queue] maximum priority %d", newprior);
  }
  for (dst = src->up; dst && dst->prior < newprior; dst = dst->up)
    ;
  if (!dst || newprior < dst->prior) {
    if ((l = lgl->queue.free)) { lgl->queue.free = l->up; CLRPTR (l); }
    else NEW (l, 1);
    LOG (2, "new queue[%d]", newprior);
    lgl->stats->queue.new++;
    lgl->queue.nlines++;
    l->prior = newprior;
    l->repr = l;
    if (dst) {
      if (dst->down) dst->down->up = l;
      else assert (lgl->queue.bottom == dst), lgl->queue.bottom = l;
      l->down = dst->down;
      l->up = dst;
      dst->down = l;
    } else {
      assert (lgl->queue.top);
      lgl->queue.top->up = l;
      l->down = lgl->queue.top;
      assert (!l->up);
      lgl->queue.top = l;
    }
    dst = l;
  } else assert (dst && newprior == dst->prior);
  assert (src && dst && src != dst && src->prior < dst->prior);
  LOG (2 + 1,  "dequeue %d from queue[%d]", lit, oldprior);
  node = lglqnd (lgl, lit);
  assert (node->line == src);
  if (node->prev) lglqnd (lgl, node->prev)->next = node->next;
  else assert (src->first == lit), src->first = node->next;
  if (node->next) lglqnd (lgl, node->next)->prev = node->prev;
  else assert (src->last == lit), src->last = node->prev;
  if (src->unassigned == lit) src->unassigned = node->next;
  if (!src->first) assert (!src->last && !src->unassigned);
  else assert (src->last);
  LOG (2 + 1, "enqueue %d to queue[%d]", lit, newprior);
  if (dst->last) lglqnd (lgl, dst->last)->next = lit;
  else assert (!dst->first), dst->first = lit;
  node->prev = dst->last;
  node->next = 0;
  node->line = dst;
  dst->last = lit;
  if (!lglval (lgl, lit) &&
      (!lgl->queue.unassigned || lgl->queue.unassigned->prior < newprior))
    lgl->queue.unassigned = dst;
  if (!dst->unassigned) dst->unassigned = lit;
  if (!src->first) {
    assert (!src->last);
    LOG (2, "delete queue[%d]", src->prior);
    if (src->down) src->down->up = src->up;
    else assert (lgl->queue.bottom == src), lgl->queue.bottom = src->up;
    if (src->up) src->up->down = src->down;
    else assert (lgl->queue.top == src), lgl->queue.top = src->down;
    if (lgl->queue.unassigned == src) lgl->queue.unassigned = src->down;
    assert (lgl->queue.nlines > 0);
    lgl->queue.nlines--;
    lgl->stats->queue.del++;
    src->up = lgl->queue.free;
    lgl->queue.free = src;
  }
  if (lgl->opts->check.val >= 3) lglchkqueue (lgl);
}

static void lglbumplits (LGL * lgl) {
  const int * p;
  lglstart (lgl, &lgl->times->bump);
  for (p = lgl->seen.start; p < lgl->seen.top; p++)
    lglvmtf (lgl, *p);
  lgldeprioritize (lgl);
  lglstop (lgl);
}

static void lglmincls (LGL * lgl, int uip) {
  int * p, * q, other, minimized;
  int size;
  lglstart (lgl, &lgl->times->mcls);
  size = lglcntstk (&lgl->clause) - 1;
  assert (size >= 0);
  lgl->stats->lits.nonmin += size;
  q = lgl->clause.start;
  minimized = 0;
  assert (lglmtstk (&lgl->poisoned));
  for (p = q; (other = *p); p++)
    if (other != uip && lglminclslit (lgl, other)) {
      LOG (2, "removed %d", other);
      minimized++;
    } else *q++ = other;
  *q++ = 0;
  lglclnpoisoned (lgl);
  assert (lgl->clause.top - q == minimized);
  LOG (2, "clause minimized by %d literals", minimized);
  LOGCLS (2, lgl->clause.start, "minimized clause");
  lgl->clause.top = q;
  lglstop (lgl);
}

static int lglana (LGL * lgl) {
  int size, savedsize, resolventsize, level, mlevel, jlevel, red, glue;
  int open, resolved, tag, lit, uip, r0, r1, other, * p;
  int del, cl, c0, c1, sl, s0, s1;
  int rescore_clauses;
  int len, * rsn;
#ifdef RESOLVENT
  AVar * av;
#endif
  if (lgl->mt) return 0;
  if (lgl->failed) return 0;
  if (!lgl->conf.lit) return 1;
  if (!lgl->level) {
#ifndef NLGLPICOSAT
    lglpicosatchkcls (lgl);
#endif
    lgl->mt = 1;
    return 0;
  }
  if (lgl->flipping) {
    assert (lgl->flipping > 0);
    if (lgl->flipping == 1) LOG (1, "switching off phase flipping");
    lgl->flipping--;
  }
  if (lgl->phaseneg) {
    assert (lgl->phaseneg > 0);
    if (lgl->phaseneg == 1) LOG (1, "switching off initial negative phase");
    lgl->phaseneg--;
  }
  lglstart (lgl, &lgl->times->ana);
  lgl->stats->confs++;
RESTART:
  assert (lgl->conf.lit);
  assert (lglmtstk (&lgl->seen));
  assert (lglmtstk (&lgl->clause));
  assert (lglmtstk (&lgl->resolvent));
#ifndef NDEBUG
  if (lgl->opts->check.val > 0) lglchkclnvar (lgl);
#endif
  open = 0;
  lit = lgl->conf.lit, r0 = lgl->conf.rsn[0], r1 = lgl->conf.rsn[1];
  rescore_clauses = 0;
  savedsize = resolved = 0;
  open += lglpull (lgl, lit);
  LOG (2, "starting analysis with reason of literal %d", lit);
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
      red = r0 & REDCS;
      p = lglidx2lits (lgl, LRGCS, red, r1);
      size = 0;
      while ((other = *p++)) {
	if (lglevel (lgl, other)) size++;
	if (lglpull (lgl, other)) open++;
      }
      if (red && lglbumplidx (lgl, r1)) rescore_clauses = 1;
    }
    LOG (3, "open %d antecendents %d learned %d resolved %d",
	 open-1, size, lglcntstk (&lgl->clause), lglcntstk (&lgl->resolvent));
    assert (open > 0);
    resolventsize = open + lglcntstk (&lgl->clause);
#ifdef RESOLVENT
    LOGRESOLVENT (2, "resolvent");
    if (lglmaintainresolvent (lgl))
      assert (lglcntstk (&lgl->resolvent) == resolventsize);
#endif
    if (lgl->opts->otfs.val &&
	(resolved >= 2) &&
	(resolventsize < size || (resolved==2 && resolventsize<savedsize))) {
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
	red = r0 & REDCS;
	p = lglidx2lits (lgl, LRGCS, red, r1);
	while (jlevel < lgl->level && (other = *p++)) {
	  level = lglevel (lgl, other);
	  if (level > jlevel) jlevel = level;
	}
	if (red && lglbumplidx (lgl, r1)) rescore_clauses = 1;
      }
      if (jlevel >= lgl->level) {
	LOG (2, "restarting analysis after on-the-fly strengthening");
	lgl->stats->otfs.restarting++;
	lglbumplits (lgl);
	if (rescore_clauses) lglrescoreclauses (lgl);
	lglclnana (lgl);
	goto RESTART;
      }
      LOGREASON (2, lit, r0, r1,
	"driving %d at level %d through strengthened", lit, jlevel);
      lgl->stats->otfs.driving++;
      lglbacktrack (lgl, jlevel);
      lglassign (lgl, lit, r0, r1);
      lglbumplits (lgl);
      goto DONE;
    }
    savedsize = size;
    while (!lglmarked (lgl, lit = lglpopstk (&lgl->trail)))
      lglunassign (lgl, lit);
    lglunassign (lgl, lit);
    if (!--open) { uip = -lit; break; }
    LOG (2, "analyzing reason of literal %d next", lit);
    rsn = lglrsn (lgl, lit);
    r0 = rsn[0], r1 = rsn[1];
  }
  assert (uip);
  LOG (2, "adding UIP %d at same level %d to 1st UIP clause",
       uip, lgl->level);
  lglpushstk (lgl, &lgl->clause, uip);
  assert (lglmarked (lgl, uip));
#ifdef RESOLVENT
  LOGRESOLVENT (3, "final resolvent before flushing fixed literals");
  if (lglmaintainresolvent  (lgl)) {
    int * q = lgl->resolvent.start;
    for (p = q; p < lgl->resolvent.top; p++)
      if (lglevel (lgl, (other = *p)))
	*q++ = other;
    lgl->resolvent.top = q;
    LOGRESOLVENT (2, "final resolvent after flushing fixed literals");
    assert (lglcntstk (&lgl->resolvent) == lglcntstk (&lgl->clause));
    for (p = lgl->clause.start; p < lgl->clause.top; p++)
      assert (lglavar (lgl, *p)->mark > 0);
    for (p = lgl->resolvent.start; p < lgl->resolvent.top; p++) {
      av = lglavar (lgl, *p); assert (av->mark > 0); av->mark = -av->mark;
    }
    for (p = lgl->clause.start; p < lgl->clause.top; p++)
      assert (lglavar (lgl, *p)->mark < 0);
    for (p = lgl->resolvent.start; p < lgl->resolvent.top; p++) {
      av = lglavar (lgl, *p); assert (av->mark < 0); av->mark = -av->mark;
    }
    lglclnstk (&lgl->resolvent);
  }
#endif
  lglpushstk (lgl, &lgl->clause, 0);
  LOGCLS (2, lgl->clause.start, "1st UIP clause");
#ifndef NLGLPICOSAT
  lglpicosatchkcls (lgl);
#endif

  glue = lglcntstk (&lgl->frames);
  lgl->stats->glues.count++;
  lgl->stats->glues.sum += glue;

  lglbumplits (lgl);
  lglmincls (lgl, uip);

  mlevel = lgl->level, jlevel = 0;
  for (p = lgl->frames.start; p < lgl->frames.top; p++) {
    level = *p;
    assert (0 < level && level < lgl->level);
    if (level < mlevel) mlevel = level;
    if (level > jlevel) jlevel = level;
  }

  LOG (2, "jump level %d", jlevel);
  LOG (2, "minimum level %d", mlevel);
  LOG (2, "glue %d covers %.0f%%", glue,
       (float)(jlevel ? lglpcnt (glue, (jlevel - mlevel) + 1) : 100.0));
  if (lglrsn (lgl, uip)[0]) lgl->stats->uips++;
  lglbacktrack (lgl, jlevel);
  len = lglcntstk (&lgl->clause) - 1;
  lgl->stats->clauses.glue += glue;
  lgl->stats->lits.learned += len;
  lgl->stats->clauses.learned++;
#ifndef NLGLPICOSAT
  lglpicosatchkcls (lgl);
#endif
  lgladdcls (lgl, REDCS, glue, 1);
DONE:
  lglclnana (lgl);
  if (!lgl->level && !lgl->simp) {
    lgl->stats->iterations++;
    lglrep (lgl, 1, 'i');
  }
  if (rescore_clauses) lglrescoreclauses (lgl);
  lglstop (lgl);
  return 1;
}

static void lglincrestartlfixed (LGL * lgl) {
  int delta = lgl->opts->restartint.val;
  lgl->limits->restart.confs = lgl->stats->confs + delta;
  lglrep (lgl, 1, 'R');
}

static void lglincrestartaux (LGL * lgl, int skip) {
  int delta = lgl->opts->restartint.val, count;
  if (lgl->opts->restart.val == 2) {
    count = ++lgl->limits->restart.luby;
    delta *= lgluby (lgl, count);
  } else {
    assert (lgl->opts->restart.val == 3);
    count = ++lgl->limits->restart.inout;
    delta *= lglinout (lgl, count, lgl->opts->rstinoutinc.val);
  }
  lgl->limits->restart.confs = lgl->stats->confs + delta;
  if (lgl->limits->restart.wasmaxdelta)
    lglrep (lgl, 1 + skip, skip ? 'N' : 'R');
  else lglrep (lgl, 2, skip ? 'n' : 'r');
  if (delta > lgl->limits->restart.maxdelta) {
    lgl->limits->restart.wasmaxdelta = 1;
    lgl->limits->restart.maxdelta = delta;
  } else lgl->limits->restart.wasmaxdelta = 0;
}

static void lglincrestartl (LGL * lgl, int skip) {
  switch (lgl->opts->restart.val) {
    case 1: lglincrestartlfixed (lgl); break;
    case 2: case 3: lglincrestartaux (lgl, skip); break;
    default: break;
  }
}

static int lglnextdecision (LGL * lgl) {
  int res = 0;
  Qln * line;
  if (!lgl->unassigned) return 0;
  lglstart (lgl, &lgl->times->dec);
  line = lgl->queue.unassigned;
  lglchkqueue (lgl);
  for (;;) {
    Qnd * n;
    assert (line);
    for (res = line->unassigned; res && lglval (lgl, res); res = n->next)
      n = lglqnd (lgl, res);
    if ((line->unassigned = res)) break;
    line = lgl->queue.unassigned = line->down;
  }
  LOG (1, "next decision %d", res);
  lglstop (lgl);
  return res;
}

static int lglreusetrail (LGL * lgl) {
  int next = 0, res = 0, prev, level;
  const Ctr * p;
  if (!(next = lglnextdecision (lgl))) return 0;
  for (p = lgl->control.start + 1; p < lgl->control.top; p++) {
    prev = p->decision;
    assert (lgldecision (lgl, prev));
    if (!lglassumption (lgl, prev) && lglqcmp (lgl, prev, next) < 0) break;
    level = lglevel (lgl, prev);
    assert (level == p - lgl->control.start);
    assert (res + 1 == level);
    res = level;
  }
  if (res)
    lglprt (lgl, 2,
    "reuse trail level %d from current level %d", res, lgl->level);
  return res;
}

static void lglrestart (LGL * lgl) {
  int skip, level;
  int64_t kept;
  assert (lgl->opts->restart.val);
  lglstart (lgl, &lgl->times->rsts);
  skip = lglagile (lgl);
  if (skip) {
    lgl->stats->restarts.skipped++;
    LOG (1, "skipping restart with agility %.0f%%", lglagility (lgl));
  } else {
    LOG (1, "restarting with agility %.0f%%", lglagility (lgl));
    if (lgl->opts->restart.val <= 1) level = 0;
    else level = lglreusetrail (lgl);
    if (level < lgl->alevel) level = lgl->alevel;
    else if (level > lgl->alevel) {
      kept = (100*level) / lgl->level;
      lgl->stats->restarts.kept.sum += kept;
      lgl->stats->restarts.kept.count++;
    }
    if (level < lgl->level) {
      lglbacktrack (lgl, level);
      lgl->stats->restarts.count++;
    } else lgl->stats->restarts.skipped++;
  }
  lglincrestartl (lgl, skip);
  lglstop (lgl);
}

static void lgldefrag (LGL * lgl) {
  int * wchs, nwchs, i, idx, bit, ldsize, size, offset, * start, * q, * end;
  const int * p, * eow, * w;
  HTS * hts;
  Qln * line;
  if (!lgl->qscheduling) return;
  lglstart (lgl, &lgl->times->dfg);
  lgl->stats->defrags++;
  nwchs = lglcntstk (&lgl->wchs->stk);
  NEW (wchs, nwchs);
  memcpy (wchs, lgl->wchs->stk.start, nwchs * sizeof *wchs);
  for (i = 0; i < MAXLDFW; i++) lgl->wchs->start[i] = INT_MAX;
  lgl->wchs->free = 0;
  start = lgl->wchs->stk.start;
  assert (nwchs >= 1);
  assert (start[0] == INT_MAX);
  offset = 1;
  for (line = lgl->queue.bottom; line; line = line->up)
  for (idx = line->last; idx; idx = lglqnd (lgl, idx)->prev)
  for (bit = 0; bit <= 1; bit++) {
    hts = lgl->dvars[idx].hts + bit;
    if (!hts->offset) { assert (!hts->count); continue; }
    ldsize = lglceilld (hts->count);
    size = (1 << ldsize);
    assert (size >= hts->count);
    w = wchs + hts->offset;
    hts->offset = offset;
    eow = w + hts->count;
    q = start + offset;
    for (p = w; p < eow; p++) *q++ = *p;
    offset += size;
    end = start + offset;
    while (q < end) *q++ = 0;
  }
  DEL (wchs, nwchs);
  q = start + offset;
  *q++ = INT_MAX;
  assert (q <= lgl->wchs->stk.top);
  lgl->wchs->stk.top = q;
  lglfitstk (lgl, &lgl->wchs->stk);
  lgl->limits->dfg.pshwchs = lgl->stats->pshwchs + lgl->opts->defragint.val;
  lgl->limits->dfg.prgss = lgl->stats->prgss;
  lglrep (lgl, 2, 'F');
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
	if (tag == OCCS) continue;
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

static Val lgliphase (LGL * lgl, int lit) {
  Val res = lglavar (lgl, lit)->phase;
  if (lit < 0) res = -res;
  return res;
}

static int lglcmphase (LGL * lgl, int a, int b) {
  return lgliphase (lgl, b) - lgliphase (lgl, a);
}

#define LGLCMPHASE(A,B) lglcmphase (lgl, *(A), *(B))

static void lglconnaux (LGL * lgl, int glue) {
  int lit, satisfied, lidx, size, red, act;
  const int * p, * c, * start, * top;
  int * q, * d;
  Stk * stk;
  Val val;
  if (glue >= 0) {
    assert (glue < MAXGLUE);
    red = REDCS;
    stk = lgl->red + glue;
  } else red = 0, stk = &lgl->irr;
  c = start = q = stk->start;
  top = stk->top;
  while (c < top) {
    act = *c;
    if (act == REMOVED) {
      for (p = c + 1; p < top && *p == REMOVED; p++)
	;
      assert (p >= top || *p < NOTALIT || lglisact (*p));
      c = p;
      continue;
    }
    if (lglisact (act)) *q++ = *c++; else act = -1;
    p = c;
    d = q;
    satisfied = 0;
    while (assert (p < top), (lit = *p++)) {
      assert (lit < NOTALIT);
      if (satisfied) continue;
      val = lglval (lgl, lit);
      if (lgliselim (lgl, lit))  {
	assert (lgl->eliminating || lgl->blocking);
	satisfied = 1;
      } else if (val > 0) satisfied = 1;
      else if (!val) *q++ = lit;
    }
    if (satisfied || p == c + 1) {
      q = d - (act >= 0);
    } else if (!(size = q - d)) {
      q = d - (act >= 0);
      if (!lgl->mt) {
	LOG (1, "empty clause during connection garbage collection phase");
	lgl->mt = 1;
      }
    } else if (size == 1) {
      q = d - (act >= 0);
      LOG (1, "unit during garbage collection");
      lglunit (lgl, d[0]);
    } else if (size == 2) {
      q = d - (act >= 0);
      lglwchbin (lgl, d[0], d[1], red);
      lglwchbin (lgl, d[1], d[0], red);
    } else if (size == 3) {
      q = d - (act >= 0);
      lglwchtrn (lgl, d[0], d[1], d[2], red);
      lglwchtrn (lgl, d[1], d[0], d[2], red);
      lglwchtrn (lgl, d[2], d[0], d[1], red);
    } else {
      assert (size > 3);
      if (lgl->opts->sortlits.val) SORT (int, d, size, LGLCMPHASE);
      *q++ = 0;
      lidx = d - start;
      if (red) {
	assert (lidx <= MAXREDLIDX);
	lidx <<= GLUESHFT;
	assert (0 <= lidx);
	lidx |= glue;
      }
      (void) lglwchlrg (lgl, d[0], d[1], red, lidx);
      (void) lglwchlrg (lgl, d[1], d[0], red, lidx);
    }
    c = p;
  }
  stk->top = q;
}

static void lglfullyconnected (LGL * lgl) {
  if (!lgl->notfullyconnected) return;
  LOG (1, "switching to fully connected mode");
  lgl->notfullyconnected  = 0;
}

static void lglcon (LGL * lgl) {
  int glue;
  for (glue = -1; glue < MAXGLUE; glue++) lglconnaux (lgl, glue);
  lglfullyconnected (lgl);
}

static void lglcount (LGL * lgl) {
  int idx, sign, lit, tag, blit, red, other, other2, glue, count;
  const int * p, * w, * c, * eow;
  HTS * hts;
  Stk * lir;
  lgl->stats->irr.clauses.cur = 0;
  lgl->stats->irr.lits.cur = 0;
  lgl->stats->red.bin = 0;
  lgl->stats->red.trn = 0;
  lgl->stats->red.lrg = 0;
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
	assert (abs (other) != abs (lit));
	if (abs (lit) >= abs (other)) continue;
	assert (2 == BINCS && 3 == TRNCS);
	if (tag == TRNCS) {
	  other2 = *p;
	  assert (abs (other2) != abs (lit));
	  assert (abs (other2) != abs (other));
	  if (abs (lit) >= abs (other2)) continue;
	}
	if (!red) {
	  lgl->stats->irr.clauses.cur++;
	  if (tag == BINCS) lgl->stats->irr.lits.cur += 2;
	  else assert (tag == TRNCS), lgl->stats->irr.lits.cur += 3;
	} else if (tag == BINCS) lgl->stats->red.bin++;
	else assert (tag == TRNCS), lgl->stats->red.trn++;
      }
    }
  assert (lgl->stats->red.bin >= 0 && lgl->stats->red.trn >= 0);
  for (c = lgl->irr.start; c < lgl->irr.top; c = p + 1) {
    if (*(p = c) >= REMOVED) continue;
    while (*p) p++;
    lgl->stats->irr.lits.cur += p - c;
    lgl->stats->irr.clauses.cur++;
  }
  LOG (1, "counted %d irredundant clauses with %d literals",
       lgl->stats->irr.clauses.cur, lgl->stats->irr.lits.cur);
  for (glue = 0; glue < MAXGLUE; glue++) {
    lir = lgl->red + glue;
    count = 0;
    for (c = lir->start; c < lir->top; c++)
      if (!*c) count++;
    if (count)
      LOG (1, "counted %d redundant clauses with glue %d", count, glue);
    lgl->stats->red.lrg += count;
    lgl->stats->lir[glue].clauses = count;
  }
  assert (lgl->stats->red.lrg >= 0);
  if (lgl->stats->red.bin)
    LOG (1, "counted %d binary redundant clauses altogether",
	 lgl->stats->red.bin);
  if (lgl->stats->red.trn)
    LOG (1, "counted %d ternary redundant clauses altogether",
	 lgl->stats->red.trn);
  if (lgl->stats->red.lrg)
    LOG (1, "counted %d large redundant clauses altogether",
	 lgl->stats->red.lrg);
}

static int lglilit (int ulit) {
  int res = ulit/2;
  assert (res >= 1);
  if (ulit & 1) res = -res;
  return res;
}

static void lglincjwh (LGL * lgl, int lit, Flt inc) {
  int ulit = lglulit (lit);
  Flt old = lgl->jwh[ulit];
  Flt new = lgladdflt (old, inc);
  lgl->jwh[ulit] = new;
}

static void lgljwh (LGL * lgl) {
  int idx, sign, lit, tag, blit, other, other2, red, size;
  const int *p, * w, * eow, * c;
  Val val, tmp, tmp2;
  HTS * hts;
  Stk * s;
  Flt inc;
  CLN (lgl->jwh, 2*lgl->nvars);
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      val = lglval (lgl, lit);
      if (val > 0) continue;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag == LRGCS) continue;
	red = blit & REDCS;
	if (red) continue;
	other = blit >> RMSHFT;
	if (abs (other) < abs (lit)) continue;
	tmp = lglval (lgl, other);
	if (tmp > 0) continue;
	if (tag == BINCS) {
	  assert (!tmp);
	  inc = lglflt (-2, 1);
	  lglincjwh (lgl, lit, inc);
	  lglincjwh (lgl, other, inc);
	} else {
	  assert (tag == TRNCS);
	  other2 = *p;
	  if (abs (other2) < abs (lit)) continue;
	  tmp2 = lglval (lgl, other2);
	  if (tmp2 > 0) continue;
	  assert ((val > 0) + (tmp > 0) + (tmp2 > 0) == 0);
	  assert ((val < 0) + (tmp < 0) + (tmp2 < 0) <= 1);
	  size = 3 + val + tmp + tmp2;
	  assert (2 <= size && size <= 3);
	  inc = lglflt (-size, 1);
	  lglincjwh (lgl, lit, inc);
	  lglincjwh (lgl, other, inc);
	  lglincjwh (lgl, other2, inc);
	}
      }
    }
  s = &lgl->irr;
  for (c = s->start; c < s->top; c = p + 1) {
    p = c;
    if (*p >= NOTALIT) continue;
    val = -1;
    size = 0;
    while ((other = *p)) {
      tmp = lglval (lgl, other);
      if (tmp > val) val = tmp;
      if (!tmp) size++;
      p++;
    }
    if (val > 0) continue;
    inc = lglflt (-size, 1);
    for (p = c; (other = *p); p++)
      if (!lglval (lgl, other))
	lglincjwh (lgl, other, inc);
  }
}

static int * lglis (LGL * lgl) {
  int idx, sign, lit, tag, blit, other, other2, red, * res;
  const int *p, * w, * eow, * c;
  Val val, tmp, tmp2;
  HTS * hts;
  Stk * s;
  NEW (res, 2*lgl->nvars);
  res += lgl->nvars;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      val = lglval (lgl, lit);
      if (val > 0) continue;
      hts = lglhts (lgl, lit);
      if (!hts->offset) continue;
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag == LRGCS) continue;
	red = blit & REDCS;
	if (red) continue;
	other = blit >> RMSHFT;
	if (abs (other) < abs (lit)) continue;
	tmp = lglval (lgl, other);
	if (tmp > 0) continue;
	if (tag == BINCS) {
	  assert (!tmp);
	  res[lit]++,
	  res[other]++;
	} else {
	  assert (tag == TRNCS);
	  other2 = *p;
	  if (abs (other2) < abs (lit)) continue;
	  tmp2 = lglval (lgl, other2);
	  if (tmp2 > 0) continue;
	  assert ((val > 0) + (tmp > 0) + (tmp2 > 0) == 0);
	  assert ((val < 0) + (tmp < 0) + (tmp2 < 0) <= 1);
	  if (!val) res[lit]++;
	  if (!tmp) res[other]++;
	  if (!tmp2) res[other2]++;
	}
      }
    }
  s = &lgl->irr;
  for (c = s->start; c < s->top; c = p + 1) {
    p = c;
    if (*p >= NOTALIT) continue;
    val = -1;
    while ((other = *p)) {
      tmp = lglval (lgl, other);
      if (tmp > val) val = tmp;
      p++;
    }
    if (val > 0) continue;
    for (p = c; (other = *p); p++)
      if (!lglval (lgl, other))
	res[other]++;
  }
  return res;
}

static int64_t lglnewirrlim (LGL * lgl) {
  int64_t add = lgl->stats->irr.clauses.add, res = add;
  res *= 100ll + lgl->opts->irrlim.val;
  res += 99;
  res /= 100;
  if (res <= add) res = add + 1;
  lglprt (lgl, 2, "[irrlim] %lld", (LGLL) res);
  return res;
}

static int lglsetjwhbias (LGL * lgl, int idx) {
  Flt pos = lgl->jwh[lglulit (idx)];
  Flt neg = lgl->jwh[lglulit (-idx)];
  AVar * av = lglavar (lgl, idx);
  int bias;
  if (av->phase) return av->phase;
  bias = (pos > neg) ? 1 : -1;
  if (av->bias == bias) return bias;
  av->bias = bias;
  lgl->stats->phase.set++;
  if (pos > neg) lgl->stats->phase.pos++; else lgl->stats->phase.neg++;
  return bias;
}

static void lglsetallphases (LGL * lgl) {
  int res = 1, idx;
  for (idx = 2; res && idx < lgl->nvars; idx++)
    res = !lglisfree (lgl, idx) || lglavar (lgl, idx)->phase;
  lgl->allphaseset = res;
}

static void lglflushphases (LGL * lgl) {
  int idx, flushed = 0;
  AVar * av;
  assert (lgl->flushphases);
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    av = lglavar (lgl, idx);
    if (!av->phase) continue;
    av->phase = 0;
    flushed++;
  }
  lglprt (lgl, 1, "[flushphases] %d saved phases reset", flushed);
  lgl->allphaseset = !flushed;
  lgl->flushphases = 0;
}

static void lglphase (LGL * lgl) {
  int64_t set = lgl->stats->phase.set;
  int64_t pos = lgl->stats->phase.pos;
  int64_t neg = lgl->stats->phase.neg;
  int idx;
  lglstart (lgl, &lgl->times->phs);
  if (lgl->flushphases) lglflushphases (lgl);
  if (lgl->opts->phase.val) goto DONE;
  lglsetallphases (lgl);
  if (lgl->allphaseset) goto DONE;
  lgl->stats->phase.count++;
  lgljwh (lgl);
  for (idx = 2; idx < lgl->nvars; idx++) lglsetjwhbias (lgl, idx);
  set = lgl->stats->phase.set - set;
  pos = lgl->stats->phase.pos - pos;
  neg = lgl->stats->phase.neg - neg;
  lglprt (lgl, 1,
     "[phase-%d] phase bias: %lld positive %.0f%%, %lld negative %.0f%%",
     lgl->stats->phase.count,
     (LGLL) pos, lglpcnt (pos, set),
     (LGLL) neg, lglpcnt (neg, set));
DONE:
  lglstop (lgl);
}

static void lglchkbcpclean (LGL * lgl, const char * where) {
  ASSERT (lgl->next2 == lgl->next && lgl->next == lglcntstk (&lgl->trail));
  ASSERT (!lgl->conf.lit);
  ASSERT (!lgl->mt);
}

static int lglcutwidth (LGL * lgl) {
  int lidx, res, l4, r4, b4, l10, r10, b10, m, oldbias;
  int idx, sign, lit, blit, tag, red, other, other2;
  const int * p, * w, * eow, * c, * q;
  int * widths, max, cut, min4, min10;
  int64_t sum, avg;
  HTS * hts;
  if (lgl->nvars <= 2) return 0;
  lglstart (lgl, &lgl->times->ctw);
  oldbias = lgl->bias;
  assert (abs (oldbias) <= 1);
  if (abs (lgl->opts->bias.val) <= 1) {
    lgl->bias = lgl->opts->bias.val;
    goto DONE;
  }
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
	other = abs (blit >> RMSHFT);
	if (tag == BINCS) {
	  if (red) continue;
	  if (other > idx) widths[other]++, cut++;
	} else if (tag == TRNCS) {
	  other2 = abs (*++p);
	  if (red) continue;
	  if (other > idx) widths[other]++, cut++;
	  if (other2 > idx) widths[other2]++, cut++;
	} else {
	  assert (tag == LRGCS);
	  lidx = *++p;
	  if (red) continue;
	  c = lglidx2lits (lgl, LRGCS, 0, lidx);
	  for (q = c; (other = abs (*q)); q++) {
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
  avg = sum / (LGLL) lgl->nvars;
  assert (avg <= INT_MAX);
  res = (int) avg;
  lglprt (lgl, 1,
    "[cut-width] %d, max %d, min4 %d at %.0f%%, min10 %d at %.0f%%",
    res, max,
    min4, lglpcnt ((b4 - 2), lgl->nvars - 2),
    min10, lglpcnt ((b10 - 2), lgl->nvars - 2));
  m = (lgl->nvars + 2)/2;
  if (b4 < m && b10 < m) lgl->bias = -1;
  if (b4 > m && b10 > m) lgl->bias = 1;
DONE:
  assert (abs (lgl->bias <= 1));
  lglprt (lgl, 1, "[decision-order] bias %d, old %d", lgl->bias, oldbias);
  lglstop (lgl);
  return res;
}

static int lglmaplit (int * map, int lit) {
  return map [ abs (lit) ] * lglsgn (lit);
}

static void lglmapstk (LGL * lgl, int * map, Stk * lits) {
  int * p, * eol;
  eol = lits->top;
  for (p = lits->start; p < eol; p++)
    *p = lglmaplit (map, *p);
}

static void lglmapglue (LGL * lgl, int * map, Stk * lits) {
  int * p, * eol;
  eol = lits->top;
  for (p = lits->start; p < eol; p++)
    if (!lglisact (*p)) *p = lglmaplit (map, *p);
}

static void lglmaplits (LGL * lgl, int * map) {
  int glue;
  lglmapstk (lgl, map, &lgl->irr);
  for (glue = 0; glue < MAXGLUE; glue++)
    lglmapglue (lgl, map, &lgl->red[glue]);
}

static void lglmapvars (LGL * lgl, int * map, int nvars) {
  int i, oldnvars = lgl->nvars, sign, udst, idst, usrc, isrc;
  DVar * dvars;
  AVar * avars;
  Qnd * nodes;
  Val * vals;
  int * i2e;
  Flt * jwh;

  if (nvars > 2) assert (nvars <= oldnvars);
  else nvars = 0;

  DEL (lgl->doms, 2*lgl->szvars);
  NEW (lgl->doms, 2*nvars);

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

  NEW (jwh, 2*nvars);
  for (i = 2; i < oldnvars; i++)
    if (lglisfree (lgl, i))
      for (sign = -1; sign <= 1; sign += 2) {
	  isrc = sign * i;
	  idst = sign * map[i];
	  usrc = lglulit (isrc);
	  udst = lglulit (idst);
	  jwh[udst] = lgladdflt (jwh[udst], lgl->jwh[usrc]);
	}
  DEL (lgl->jwh, 2*lgl->szvars);
  lgl->jwh = jwh;

  NEW (nodes, nvars);
  for (i = 2; i < oldnvars; i++)
    if (lglisfree (lgl, i))
      nodes[map[i]] = lgl->queue.nodes[i];
  DEL (lgl->queue.nodes, lgl->szvars);
  lgl->queue.nodes = nodes;

  NEW (avars, nvars);	  	
  for (i = 2; i < oldnvars; i++)
    if (lglisfree (lgl, i))
      avars[map[i]] = lgl->avars[i];
  DEL (lgl->avars, lgl->szvars);
  lgl->avars = avars;              // Last since 'lglisfree' depends on it !!!

  lgl->nvars = lgl->szvars = nvars;
  lgl->stats->fixed.current = 0;
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
  for (p = lgl->trail.start; p < lgl->trail.top; p++)
    if (lglevel (lgl, *p) > 0) break;
  for (q = lgl->trail.start; p < lgl->trail.top; p++) {
    src = *p;
    assert (lglevel (lgl, src) > 0);
    dst = lglmaplit (map, src);
    *q++ = dst;
  }
  lgl->trail.top = q;
  lgl->flushed = lgl->next2 = lgl->next = lglcntstk (&lgl->trail);
}

static void lglmapqln (LGL * lgl, Qln * line, int * map) {
  int prev, next, i;
  Qnd * n, * m;
  for (i = line->first; i && !lglisfree (lgl, i); i = next) {
    n = lglqnd (lgl, i);
    next = n->next;
    CLRPTR (n);
  }
  if (i) {
    line->first = abs (lglmaplit (map, i));
    for (prev = 0; i; i = next) {
      n = lglqnd (lgl, i);
      next = n->next;
      while (next && !lglisfree (lgl, next)) {
	m = lglqnd (lgl, next);
	next = m->next;
	CLRPTR (m);
      }
      n->prev = prev ? abs (lglmaplit (map, prev)) : 0;
      n->next = next ? abs (lglmaplit (map, next)) : 0;
      prev = i;
    }
    line->last = abs (lglmaplit (map, prev));
  } else line->first = line->last = 0;
  line->unassigned = line->first;
}

static void lglmapqueue (LGL * lgl, int * map) {
  Qln * p, * up, * down = 0;
  for (p = lgl->queue.bottom; p; p = up) {
    up = p->up;
    lglmapqln (lgl, p, map);
    if (p->first) {
      assert (p->last);
      if (down) down->up = p;
      else lgl->queue.bottom = p;
      p->down = down;
      down = p;
    } else {
      assert (!p->last);
      LOG (2, "delete queue[%d]", p->prior);
      assert (lgl->queue.nlines > 0);
      lgl->queue.nlines--;
      lgl->stats->queue.del++;
      p->up = lgl->queue.free;
      lgl->queue.free = p;
    }
  }
  if (down) down->up = 0;
  else lgl->queue.bottom = 0;
  lgl->queue.unassigned = lgl->queue.top = down;
}

static int lglptrjmp (int * repr, int max, int start) {
#ifndef NDEBUG
  int prev = 0, count = 0;
#endif
  int next, idx, res, sgn, tmp;
  assert (repr);
  next = start;
  do {
    res = next;
    idx = abs (res);
    assert (idx <= max);
    sgn = lglsgn (res);
    next = repr[idx];
    next *= sgn;
#ifndef NDEBUG
    if (prev || next) assert (prev != next);
    prev = res;
    assert (count <= max);
#endif
  } while (next);
  tmp = start;
  while (tmp != res) {
    idx = abs (tmp), sgn = lglsgn (tmp);
    next = repr[idx] * sgn;
    repr[idx] = sgn * res;
    tmp = next;
  }
  return res;
}

static int lglirepr (LGL * lgl, int lit) {
  assert (lgl->repr);
  return lglptrjmp (lgl->repr, lgl->nvars - 1, lit);
}

static void lglmapext (LGL * lgl, int * map) {
  int eidx, ilit, mlit;
  Ext * ext;
  for (eidx = 1; eidx <= lgl->maxext; eidx++) (void) lglerepr (lgl, eidx);
  for (eidx = 1; eidx <= lgl->maxext; eidx++) {
    ext = lgl->ext + eidx;
    if (!ext->imported) continue;
    if (ext->equiv) {
      LOG (3, "mapping external %d to equivalent external %d",
	   eidx, ext->repr);
      continue;
    }
    ilit = ext->repr;
    mlit = lglmaplit (map, ilit);
    LOG (3, "mapping external %d to internal %d", eidx, mlit);
    ext->repr = mlit;
  }
}

static void lglsignedmark (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << (lit < 0);
  if (av->mark & bit) return;
  av->mark |= bit;
}

static void lglsignedunmark (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << (lit < 0);
  if (!(av->mark & bit)) return;
  av->mark &= ~bit;
}

static void lglsignedmarknpushseen (LGL * lgl, int lit) {
  lglsignedmark (lgl, lit);
  lglpushstk (lgl, &lgl->seen, lit);
}

static int lglsignedmarked (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << (lit < 0);
  return av->mark & bit;
}

static void lglmapass (LGL * lgl, int * map) {
  int * p, * q, iass, mass, flushed;
  unsigned bit;
  AVar * av;
  if (abs (lgl->failed) != 1) lgl->failed = lglmaplit (map, lgl->failed);
  for (p = q = lgl->assume.start; p < lgl->assume.top; p++) {
    iass = *p;
    mass = lglmaplit (map, iass);
    if (mass == 1) continue;
    if (mass == -1) {
      if (lgl->failed != -1) {
#ifndef NDEBUG
	int * r;
	for (r = lgl->eassume.start; r < lgl->eassume.top; r++)
	  if (lglimport (lgl, *r) == -1) break;
	assert (r < lgl->eassume.top);
#endif
	LOG (2, "enforcing a failed assumption");
	lgl->failed = -1;
      }
      continue;
    }
    LOG (2, "mapping previous internal assumption %d to %d", iass, mass);
    av = lglavar (lgl, mass);
    bit = 1u << (mass < 0);
    if (!(av->assumed & bit)) {
      LOG (2, "assuming new representative %d instead of %d", mass, iass);
      av->assumed |= bit;
    }
    *q++ = mass;
  }
  lgl->assume.top = q;
  flushed = 0;
  for (p = q = lgl->assume.start; p < lgl->assume.top; p++) {
    iass = *p;
    if (lglsignedmarked (lgl, iass)) { flushed++; continue; }
    lglsignedmark (lgl, iass);
    *q++ = iass;
  }
  lgl->assume.top = q;
  for (p = lgl->assume.start; p < lgl->assume.top; p++) {
    iass = *p;
    assert (lglsignedmarked (lgl, iass));
    lglsignedunmark (lgl, iass);
  }
  if (flushed)
    LOG (2, "flushed %d duplicated internal assumptions", flushed);
}

#if !defined(NDEBUG) && !defined(NLGLPICOSAT)
static void lglpicosataddstk (LGL * lgl, const Stk * stk) {
  const int * p;
  int lit;
  for (p = stk->start; p < stk->top; p++) {
    lit = *p;
    if (lit >= NOTALIT) continue;
    picosat_add (PICOSAT, lit);
  }
}

static void lglpicosataddunits (LGL * lgl) {
  int idx, val;
  assert (!lgl->level);
  assert (lgl->picosat.solver);
  for (idx = 2; idx < lgl->nvars; idx++) {
    val = lglval (lgl, idx);
    assert (!val);
  }
}

static void lglpicosataddhts (LGL * lgl) {
  int idx, sign, lit, tag, blit, other, other2;
  const int * w, * p, * eow;
  HTS * hts;
  assert (lgl->picosat.solver);
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
	  if (abs (other) < idx) continue;
	  picosat_add (PICOSAT, lit);
	  picosat_add (PICOSAT, other);
	  picosat_add (PICOSAT, 0);
	} else if (tag == TRNCS) {
	  other = blit >> RMSHFT;
	  other2 = *++p;
	  if (abs (other) < idx) continue;
	  if (abs (other2) < idx) continue;
	  picosat_add (PICOSAT, lit);
	  picosat_add (PICOSAT, other);
	  picosat_add (PICOSAT, other2);
	  picosat_add (PICOSAT, 0);
	} else if (tag == LRGCS) p++;
	else assert (lgl->dense && tag == OCCS);
      }
    }
}
#endif

static void lglpicosatchkall (LGL * lgl) {
#if !defined(NDEBUG) && !defined(NLGLPICOSAT)
  int res, * p;
  lglpicosatinit (lgl);
  if (lgl->opts->check.val >= 2) {
    for (p = lgl->eassume.start; p < lgl->eassume.top; p++)
      picosat_assume (PICOSAT, lglimport (lgl, *p));
    res = picosat_sat (PICOSAT, -1);
    LOG (1, "PicoSAT returns %d", res);
    if (lgl->picosat.res) assert (res == lgl->picosat.res);
    lgl->picosat.res = res;
  }
  if (picosat_inconsistent (PICOSAT)) {
    assert (!lgl->picosat.res || lgl->picosat.res == 20);
    lgl->picosat.res = 20;
  }
#endif
}

static void lglpicosatrestart (LGL * lgl) {
#if !defined(NDEBUG) && !defined(NLGLPICOSAT)
  int glue;
  if (lgl->picosat.solver) {
    picosat_reset (lgl->picosat.solver);
    LOG (1, "PicoSAT reset");
    lgl->picosat.solver = 0;
  }
  lglpicosatinit (lgl);
  lglpicosataddunits (lgl);
  if (lgl->mt) picosat_add (PICOSAT, 0);
  lglpicosataddhts (lgl);
  lglpicosataddstk (lgl, &lgl->irr);
  for (glue = 0; glue < MAXGLUE; glue++)
    lglpicosataddstk (lgl, &lgl->red[glue]);
#endif
}

static int lglmapsize (LGL * lgl) {
  int size = 0, idx;
  for (idx = 2; idx < lgl->nvars; idx++) 
    if (lglisfree (lgl, idx)) size++;
  LOG (1, "mapping %d remaining variables", size);
  return size;
}

static void lglmapnonequiv (LGL * lgl, int * map, int size) {
  int count = 0, idx;
  AVar * av;
  Val val;
  map[0] = 0, map[1] = 1;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (map[idx]) continue;
    av = lglavar (lgl, idx);
    if (av->type == FREEVAR) {
      assert (idx > 0);
      if (map[idx]) { assert (map[idx] < idx); continue; }
      LOG (3, "mapping free %d to %d", idx, count + 2);
      map[idx] = count + 2;
      count++;
    } else if (av->type == EQUIVAR) {
      assert (lgl->repr);
      assert (!map[idx]);
    } else if (av->type == FIXEDVAR) {
      val = lgl->vals[idx];
      assert (val);
      LOG (3, "mapping assigned %d to %d", idx, (int) val);
      map[idx] = val;
    } else {
      assert (av->type == ELIMVAR);
      assert (!lglifrozen (lgl, idx));
      map[idx] = 0;
    }
  }
  assert (count == size);
}

static void lglmapequiv (LGL * lgl, int * map) {
  int idx, repr, dst;
  AVar * av;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (map[idx]) continue;
    av = lglavar (lgl, idx);
    if (av->type == ELIMVAR) continue;
    assert (av->type == EQUIVAR);
    assert (lgl->repr);
    assert (!map[idx]);
    repr = lglirepr (lgl, idx);
    assert (repr != idx);
    assert (map[abs (repr)]);
    dst = lglmaplit (map, repr);
    LOG (3, "mapping equivalent %d to %d", idx, dst);
    map[idx] = dst;
  }
}

typedef struct ForceData { int pos, count; double sum; } ForceData;

static void lglincfdat (ForceData * fdat, int lit, double cog) {
  int idx = abs (lit);
  fdat[idx].count++;
  fdat[idx].sum += cog;
}

static int lglcmpfdat (ForceData * fdat, int l, int k) {
  ForceData * d, * e;
  l = abs (l);
  k = abs (k);
  d = fdat + l;
  e = fdat + k;
  if (d->sum < e->sum) return -1;
  if (d->sum > e->sum) return 1;
  if (d->pos < e->pos) return -1;
  if (d->pos > e->pos) return 1;
  return l - k;
}

#define LGLCMPFDAT(A,B) (lglcmpfdat(fdat, *(A), *(B)))

static void lglforce (LGL * lgl, int * map) {
  int idx, lit, sign, blit, tag, red, other, other2, size, C, V, o, min, max;
  int round = 1, first = !lgl->stats->force.count;
  double cog, span, oldspan, mincut;
  const int * p, * w, * eow, * c;
  ForceData * fdat;
  HTS * hts;
  Stk order;
  if (!lgl->allowforce) return;
  if (!lgl->opts->force.val) return;
  lglstart (lgl, &lgl->times->force);
  span = INT_MAX;
RESTART:
  oldspan = span;
  lgl->stats->force.count++;
  NEW (fdat, lgl->nvars);
  CLR (order);
  V = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    fdat[idx].pos = map[idx];
    lglpushstk (lgl, &order, idx);
    V++;
  }
  if (V <= 1) goto DONE;
  C = 0;
  span = 0;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag == LRGCS) continue;
	red = blit & REDCS;
	if (red) continue;
	other = abs (blit >> RMSHFT);
	if (other < idx) continue;
	if (tag == BINCS) {
	  cog = fdat[idx].pos + fdat[other].pos;
	  cog /= 2;
	  lglincfdat (fdat, idx, cog);
	  lglincfdat (fdat, other, cog);
	  o = fdat[idx].pos, min = o, max = o;
	  o = fdat[other].pos, min = lglmin (min, o), max = lglmax (max, o);
	  span += max - min;
	  C++;
	} else if (tag == TRNCS) {
	  other2 = abs (*p);
	  if (other2 < idx) continue;
	  cog = fdat[idx].pos + fdat[other].pos + fdat[other2].pos;
	  cog /= 3;
	  lglincfdat (fdat, idx, cog);
	  lglincfdat (fdat, other, cog);
	  lglincfdat (fdat, other2, cog);
	  o = fdat[idx].pos, min = o, max = o;
	  o = fdat[other].pos, min = lglmin (min, o), max = lglmax (max, o);
	  o = fdat[other2].pos, min = lglmin (min, o), max = lglmax (max, o);
	  span += max - min;
	  C++;
	}
      }
    }
  for (c = lgl->irr.start; c < lgl->irr.top; c = p + 1) {
    if (*(p = c) >= NOTALIT) continue;
    cog = 0;
    while ((idx = abs (*p))) cog += fdat[idx].pos, p++;
    size = p - c;
    assert (size >= 4);
    assert (p >= c + 4);
    cog /= size;
    min = INT_MAX, max = INT_MIN;
    for (p = c; (idx = abs (*p)); p++) {
      o = fdat[idx].pos, min = lglmin (min, o), max = lglmax (max, o);
      lglincfdat (fdat, idx, cog);
    }
    span += max - min;
    C++;
  }
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (!(size = fdat[idx].count)) continue;
    fdat[idx].sum /= size;
  }
  assert (V > 1);
  mincut = span / (V - 1);
  if (C > 0) span /= C; else span = 0;
  if (lgl->stats->force.count > 1) {
    if (lgl->stats->force.mincut.min > mincut)
      lgl->stats->force.mincut.min = mincut;
    if (lgl->stats->force.mincut.max < mincut)
      lgl->stats->force.mincut.max = mincut;
  } else lgl->stats->force.mincut.min = lgl->stats->force.mincut.max = mincut;
  lglprt (lgl, 1, 
    "[force-%lld] mincut %.1f, span %.1f, %d variables, %d clauses",
    (LGLL) lgl->stats->force.count, mincut, span, V, C);
  SORT (int, order.start, lglcntstk (&order), LGLCMPFDAT);
  o = 2;
  for (p = order.start; p < order.top; p++) {
    idx = *p;
    assert (lglisfree (lgl, idx));
    LOG (3, "forced mapping free %d to %d", idx, o);
    map[idx] = o++;
  }
DONE:
  DEL (fdat, lgl->nvars);
  lglrelstk (lgl, &order);
  if (first) {
    if (round++ < lgl->opts->force.val) goto RESTART;
    if (round < lglceilld (lglrem (lgl))) goto RESTART;
    if (oldspan > span && (oldspan - span) > oldspan/100.0) goto RESTART;
  }
  lglstop (lgl);
}

static void lglmap (LGL * lgl) {
  int size, * map, oldnvars, mapsize;
#ifndef NLGLPICOSAT
  lglpicosatchkall (lgl);
#endif
  assert (!lgl->level);
  size = lglmapsize (lgl);
  oldnvars = lgl->nvars;
  mapsize = lglmax (oldnvars, 2);
  NEW (map, mapsize);
  lglmapnonequiv (lgl, map, size);
  lglmapequiv (lgl, map);
  lglforce (lgl, map);
  lglmaptrail (lgl, map);
  lglflushqmerged (lgl);
  lglmapqueue (lgl, map);
  lglmapvars (lgl, map, size + 2);
  lglmaplits (lgl, map);
  if (lgl->cgrclosing) lglmapstk (lgl, map, &lgl->cgr->units);
  lglmapext (lgl, map);
  lglmapass (lgl, map);
  assert (lglmtstk (&lgl->clause));
  lglmaphts (lgl, map);
  DEL (map, mapsize);
  if (lgl->repr) DEL (lgl->repr, oldnvars);
  lglpicosatrestart (lgl);
  lgl->unassigned = size;
}

static int lglgcnotnecessary (LGL * lgl) {
  if (lgl->forcegc) return 0;
  if (lgl->notfullyconnected) return 0;
  return lgl->stats->fixed.sum <= lgl->limits->gc.fixed;
}

static void lglcompact (LGL * lgl) {
  int glue;
  lglfitstk (lgl, &lgl->assume);
  lglfitstk (lgl, &lgl->clause);
  lglfitstk (lgl, &lgl->eassume);
  lglfitstk (lgl, &lgl->extend);
  lglfitstk (lgl, &lgl->fassume);
  lglfitstk (lgl, &lgl->cassume);
  lglfitstk (lgl, &lgl->frames);
#ifndef NCHKSOL
  lglfitstk (lgl, &lgl->orig);
#endif
  lglfitstk (lgl, &lgl->trail);
  lgldefrag (lgl);
  lglfitstk (lgl, &lgl->wchs->stk);

  lglfitstk (lgl, &lgl->irr);
  for (glue = 0; glue < MAXGLUE; glue++)
    lglfitlir (lgl, lgl->red + glue);
  lglrelstk (lgl, &lgl->lcaseen);
  lglrelstk (lgl, &lgl->poisoned);
  lglrelstk (lgl, &lgl->seen);
  lglrelstk (lgl, &lgl->esched);
}

static void lglgc (LGL * lgl) {
  if (lgl->mt) return;
  lglchkred (lgl);
  if (lglgcnotnecessary (lgl)) return;
  lglstart (lgl, &lgl->times->gc);
  lglchkbcpclean (lgl, "gc");
  lglrep (lgl, 2, 'g');
  lgl->stats->gcs++;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  for (;;) {
    lgldis (lgl);
    lglcon (lgl);
    if (lgl->mt) break;
    if (lgl->next2 == lgl->next &&
	lgl->next == lglcntstk (&lgl->trail)) break;
    if (!lglbcp (lgl)) {
      assert (!lgl->mt);
      LOG (1, "empty clause after propagating garbage collection unit");
      lgl->mt = 1;
      break;
    }
  }
  lglcount (lgl);
  lglmap (lgl);

  lglcompact (lgl);

  lgl->limits->gc.fixed = lgl->stats->fixed.sum;

  lglchkred (lgl);
  lglrep (lgl, 2, 'c');
  lglstop (lgl);
}

static int lgltopgc (LGL * lgl) {
  if (lgl->mt) return 0;
  assert (!lgl->forcegc && !lgl->allowforce);
  lgl->forcegc = lgl->allowforce = 1;
  lglgc (lgl);
  assert (lgl->forcegc && lgl->allowforce);
  lgl->forcegc = lgl->allowforce = 0;
  return !lgl->mt;
}

static void lgliassume (LGL * lgl, int lit) {
  lgl->level++;
  lglpushcontrol (lgl, lit);
  assert (lglcntctk (&lgl->control) == lgl->level + 1);
  LOG (2, "internally assuming %d", lit);
  lglassign (lgl, lit, DECISION, 0);
}

static int lglrandec (LGL * lgl) {
  unsigned size, pos, start, delta;
  int lit;
  lgl->limits->randec = lgl->stats->decisions;
  lgl->limits->randec += lgl->opts->randecint.val/2;
  lgl->limits->randec += lglrand (lgl) % lgl->opts->randecint.val;
  assert (lgl->nvars > 2);
  size = lgl->nvars - 2;
  if (!size) return 0;
  pos = start = lglrand (lgl) % size;
  lit = 2 + pos;
  assert (2 <= lit && lit < lgl->nvars);
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
      lit = pos + 2;
      assert (2 <= lit && lit < lgl->nvars);
    } while (lglval (lgl, lit));
  }
  lgl->stats->randecs++;
  return lit;
}

static int lgladecide (LGL * lgl) {
  int res, val;
  while (lgl->assumed < lglcntstk (&lgl->assume)) {
    res = lglpeek (&lgl->assume, lgl->assumed);
    val = lglcval (lgl, res);
    if (val > 0) LOG (3, "assumption %d already satisfied", res);
    lgl->assumed++;
    LOG (3, "new assumption queue level %d", lgl->assumed);
    assert (val >= 0);
    if (!val) return res;
  }
  return 0;
}

static void lgldassume (LGL * lgl, int lit) {
  LOG (2, "next decision %d", lit);
  lgl->stats->decisions++;
  lgl->stats->height += lgl->level;
  lgliassume (lgl, lit);
  assert (lgldecision (lgl, lit));
}

static int lgldefphase (LGL * lgl, int idx) {
  AVar * av;
  int bias;
  assert (idx > 0);
  av = lglavar (lgl, idx);
  if (!av->phase) {
    bias = lgl->opts->phase.val;
    if (!bias) bias = av->bias;
    if (!bias) bias = lglsetjwhbias (lgl, idx);
    av->phase = bias;
  }
  return av->phase;
}

static void lglupdflipint (LGL * lgl) {
  int64_t limit = lgl->opts->flipint.val;
  if (limit > (int64_t) INT_MAX) limit = INT_MAX;
  lgl->limits->flipint = limit;
}

static int lgldecidephase (LGL * lgl, int lit) {
  int res = abs (lit), flipped;
  AVar * av = lglavar (lgl, lit);
  if (av->fase) return av->fase * res;
  if (lgl->phaseneg || lgldefphase (lgl, res) <= 0) res = -res;
  if (!lgl->flipping &&
      !lgl->phaseneg &&
      lgl->opts->flipping.val &&
      lgl->level >= lgl->alevel &&
      lgl->assumed == lglcntstk (&lgl->assume) &&
      (!lgl->opts->fliptop.val || lgl->level == lgl->alevel)) {
    if (lgl->notflipped >= lgl->limits->flipint) {
      LOG (1, "switching on phase flipping");
      lgl->stats->fliphases++;
      lglupdflipint (lgl);
      lgl->flipping = lgl->opts->flipdur.val;
      lgl->notflipped = 0;
    } else lgl->notflipped++;
  }
  if (lgl->flipping) {
    assert (!lgl->phaseneg);
    flipped = -res;
    LOG (2, "flipping phase of %d to %d", res, flipped);
    lgl->stats->flipped++;
    res = flipped;
  }
  return res;
}

static int lglhasbins (LGL * lgl, int lit) {
  int blit, tag, other, other2, val, val2, implied;
  const int * p, * w, * eos, * q;
  HTS * hts;
  assert (!lgl->level);
  assert (lglisfree (lgl, lit));
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eos = w + hts->count;
  for (p = w; p < eos; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == BINCS) {
      other = blit >> RMSHFT;
      val = lglval (lgl, other);
      assert (val >= 0);
      if (!val) return 1;
    } else if (tag == TRNCS) {
      other = blit >> RMSHFT;
      other2= *++p;
      val = lglval (lgl, other);
      val2 = lglval (lgl, other2);
      assert (val >= 0 || val2 >= 0);
      if (val > 0 || val2 > 0) continue;
      if (!val && val2 < 0) return 1;
      if (val < 0 && !val2) return 1;
    } else {
      assert (tag == LRGCS);
      q = lglidx2lits (lgl, LRGCS, (blit & REDCS), *++p);
      implied = 0;
      while ((other = *q++)) {
	if (other == lit) continue;
	val = lglval (lgl, other);
	if (val > 0) break;
	if (val < 0) continue;
	if (implied) break;
	implied = other;
      }
      if (other) continue;
      if (implied) return 1;
    }
  }
  return 0;
}

static int lgldecide (LGL * lgl) {
  int lit;
  lglchkbcpclean (lgl, "decide");
  if (!lgl->unassigned) return 0;
  if ((lit = lgladecide (lgl))) {
    LOG (2, "using assumption %d as decision", lit);
    lgl->alevel = lgl->level + 1;
    LOG (2, "new assumption decision level %d", lgl->alevel);
  } else {
    if (lgl->opts->randec.val &&
	lgl->limits->randec <= lgl->stats->decisions) {
      lit = lglrandec (lgl);
      LOG (2, "random decision %d", lit);
    } else lit = lglnextdecision (lgl);
    lit = lgldecidephase (lgl, lit);
  }
  if (lit) lgldassume (lgl, lit);
  return 1;
}

static void lgldcpdis (LGL * lgl) {
  int idx, sign, lit, tag, blit, red, other, other2, i;
  const int * w, * p, * eow;
  Val val;
  HTS * hts;
  Stk * s;
  NEW (lgl->dis, 1);
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
	if (abs (other) < idx) continue;
	val = lglval (lgl, other);
	if (val > 0) continue;
	red = blit & REDCS;
	if (red && !lglisfree (lgl, other)) continue;
	if (tag == BINCS) {
	  s = red ? &lgl->dis->red.bin : &lgl->dis->irr.bin;
	} else {
	  assert (tag == TRNCS);
	  other2 = *p;
	  if (abs (other2) < idx) continue;
	  val = lglval (lgl, other2);
	  if (val > 0) continue;
	  if (red && !lglisfree (lgl, other2)) continue;
	  s = red ? &lgl->dis->red.trn : &lgl->dis->irr.trn;
	  lglpushstk (lgl, s, other2);
	}
	lglpushstk (lgl, s, other);
	lglpushstk (lgl, s, lit);
	lglpushstk (lgl, s, 0);
      }
    }
  lglrststk (&lgl->wchs->stk, 2);
  lgl->wchs->stk.top[-1] = INT_MAX;
  for (i = 0; i < MAXLDFW; i++) lgl->wchs->start[i] = INT_MAX;
  lgl->wchs->free = 0;
}

static void lgldcpclnstk (LGL * lgl, int red, Stk * s) {
  int oldsz, newsz, lit, mark, satisfied, repr, act;
  const int * p, * c, * eos = s->top;
  int * start, * q, * r, * d;
  Stk * t;
  Val val;
  q = start = s->start;
  for (c = q; c < eos; c = p + 1) {
    act = *c;
    if (act == REMOVED) {
      for (p = c + 1; p < eos && *p == REMOVED; p++)
	;
      assert (p >= eos || *p < NOTALIT || lglisact (*p));
      p--;
      continue;
    }
    if (lglisact (act)) *q++ = *c++; else act = -1;
    d = q;
    satisfied = 0;
#ifndef NDEBUG
    for (p = c; assert (p < eos), (lit = *p); p++) {
      assert (!lglavar (lgl, lit)->mark);
      repr = lglirepr (lgl, lit);
      assert (abs (repr) == 1 || !lglavar (lgl, lit)->mark);
    }
#endif
    for (p = c; assert (p < eos), (lit = *p); p++) {
      assert (lit < NOTALIT);
      if (satisfied) continue;
      repr = lglirepr (lgl, lit);
      val = lglcval (lgl, repr);
      if (val > 0) { satisfied = 1; continue; }
      if (val < 0) continue;
      mark = lglmarked (lgl, repr);
      if (mark < 0) { satisfied = 1; continue; }
      if (mark > 0) continue;
      lglmark (lgl, repr);
      *q++ = repr;
    }
    oldsz = p - c;
    for (r = d; r < q; r++) lglunmark (lgl, *r);
    if (satisfied || !oldsz) { q = d - (act >= 0); continue; }
    newsz = q - d;
    if (newsz >= 4) {
      assert (act < 0 || d[-1] == act);
      *q++ = 0;
      assert (d <= c);
    } else if (!newsz) {
      LOG (1, "found empty clause while cleaning decomposition");
      lgl->mt = 1;
      q = d - (act >= 0);
    } else if (newsz == 1) {
      LOG (1, "new unit %d while cleaning decomposition", d[0]);
      lglunit (lgl, d[0]);
      q = d - (act >= 0);
    } else if (newsz == 2) {
      t = red ? &lgl->dis->red.bin : &lgl->dis->irr.bin;
      if (s != t) {
	lglpushstk (lgl, t, d[0]);
	lglpushstk (lgl, t, d[1]);
	lglpushstk (lgl, t, 0);
	q = d - (act >= 0);
      } else *q++ = 0;
    } else {
      assert (newsz == 3);
      t = red ? &lgl->dis->red.trn : &lgl->dis->irr.trn;
      if (s != t) {
	lglpushstk (lgl, t, d[0]);
	lglpushstk (lgl, t, d[1]);
	lglpushstk (lgl, t, d[2]);
	lglpushstk (lgl, t, 0);
	q = d - (act >= 0);
      } else *q++ = 0;
    }
  }
  s->top = q;
}

static void lgldcpconnaux (LGL * lgl, int red, int glue, Stk * s) {
  int * start = s->start, * q, * d, lit, size, lidx, act;
  const int * p, * c, * eos = s->top;
  assert (red == 0 || red == REDCS);
  assert (!glue || red);
  q = start;
  for (c = q; c < eos; c = p + 1) {
    if (lglisact (act = *c)) *q++ = *c++; else act = -1;
    d = q;
    for (p = c; (lit = *p); p++) {
      assert (!lgl->repr[abs (lit)]);
      assert (!lgl->vals[abs (lit)]);
      *q++ = lit;
    }
    size = q - d;
    if (size == 2) {
      q = d - (act >= 0);
      lglwchbin (lgl, d[0], d[1], red);
      lglwchbin (lgl, d[1], d[0], red);
    } else if (size == 3) {
      q = d - (act >= 0);
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
      (void) lglwchlrg (lgl, d[0], d[1], red, lidx);
      (void) lglwchlrg (lgl, d[1], d[0], red, lidx);
    }
  }
  s->top = q;
}

static void lgldcpcon (LGL * lgl) {
  Stk * lir;
  int glue;
  lgldcpconnaux (lgl, 0, 0, &lgl->dis->irr.bin);
  lgldcpconnaux (lgl, REDCS, 0, &lgl->dis->red.bin);
  lgldcpconnaux (lgl, 0, 0, &lgl->dis->irr.trn);
  lgldcpconnaux (lgl, REDCS, 0, &lgl->dis->red.trn);
  lglrelstk (lgl, &lgl->dis->irr.bin);
  lglrelstk (lgl, &lgl->dis->irr.trn);
  lglrelstk (lgl, &lgl->dis->red.bin);
  lglrelstk (lgl, &lgl->dis->red.trn);
  DEL (lgl->dis, 1);
  lgldcpconnaux (lgl, 0, 0, &lgl->irr);
  for (glue = 0; glue < MAXGLUE; glue++) {
    lir = lgl->red + glue;
    lgldcpconnaux (lgl, REDCS, glue, lir);
  }
  lglfullyconnected (lgl);
}

static void lgldcpcln (LGL * lgl) {
  int glue, old, rounds = 0;
  Stk * lir;
  do {
    rounds++;
    old = lgl->stats->fixed.current;
    lgldcpclnstk (lgl, 0, &lgl->irr);
    lgldcpclnstk (lgl, 0, &lgl->dis->irr.bin);
    lgldcpclnstk (lgl, 0, &lgl->dis->irr.trn);
    lgldcpclnstk (lgl, REDCS, &lgl->dis->red.bin);
    lgldcpclnstk (lgl, REDCS, &lgl->dis->red.trn);
    for (glue = 0; glue < MAXGLUE; glue++) {
      lir = lgl->red + glue;
      lgldcpclnstk (lgl, REDCS, lir);
    }
  } while (old < lgl->stats->fixed.current);
  LOG (1, "iterated %d decomposition cleaning rounds", rounds);
}

static void lglepush (LGL * lgl, int ilit) {
  int elit = ilit ? lglexport (lgl, ilit) : 0;
  lglpushstk (lgl, &lgl->extend, elit);
  LOG (4, "pushing external %d internal %d", elit, ilit);
}

static void lglemerge (LGL * lgl, int ilit0, int ilit1) {
  int elit0 = lgl->i2e[abs (ilit0)] * lglsgn (ilit0);
  int elit1 = lgl->i2e[abs (ilit1)] * lglsgn (ilit1);
  int repr0 = lglerepr (lgl, elit0);
  int repr1 = lglerepr (lgl, elit1);
  Ext * ext0 = lglelit2ext (lgl, repr0);
#ifndef NDEBUG
  Ext * ext1 = lglelit2ext (lgl, repr1);
  int repr = repr1;
#endif
  assert (abs (repr0) != abs (repr1));
  if (repr0 < 0) {
#ifndef NLGLOG
    repr0 *= -1;
#endif
    repr1 *= -1;
  }
  ext0->equiv = 1;
  ext0->repr = repr1;
  LOG (2, "merging external literals %d and %d", repr0, repr1);
  assert (lglerepr (lgl, elit0) == repr);
  assert (lglerepr (lgl, elit1) == repr);
  assert (!(ext0->frozen || ext0->tmpfrozen) ||
	    ext1->frozen || ext1->tmpfrozen);
  lglepush (lgl, 0); lglepush (lgl, -ilit0); lglepush (lgl, ilit1);
  lglepush (lgl, 0); lglepush (lgl, ilit0); lglepush (lgl, -ilit1);
}

static void lglimerge (LGL * lgl, int lit, int repr) {
  int idx = abs (lit);
  AVar * av = lglavar (lgl, idx);
  assert (!lglifrozen (lgl, lit) || lglifrozen (lgl, repr));
  if (lit < 0) repr = -repr;
  assert (av->type == FREEVAR);
  assert (lgl->repr);
  av->type = EQUIVAR;
  lgl->repr[idx] = repr;
  lgl->stats->prgss++;
  lgl->stats->irrprgss++;
  lgl->stats->equiv.sum++;
  lgl->stats->equiv.current++;
  assert (lgl->stats->equiv.sum > 0);
  assert (lgl->stats->equiv.current > 0);
#ifndef NLGLPICOSAT
  lglpicosatchkclsarg (lgl, idx, -repr, 0);
  lglpicosatchkclsarg (lgl, -idx, repr, 0);
#endif
  lglemerge (lgl, idx, repr);
}

static void lglfreezer (LGL * lgl) {
  int frozen, melted, tmpfrozen, elit, erepr;
  Ext * ext, * rext;
  int * p, eass;
  if (lgl->frozen) return;
  for (elit = 1; elit <= lgl->maxext; elit++) lgl->ext[elit].tmpfrozen =0;
  tmpfrozen = frozen = 0;
  if (!lglmtstk (&lgl->eassume)) {
    for (p = lgl->eassume.start; p < lgl->eassume.top; p++) {
      eass = *p;
      ext = lglelit2ext (lgl, eass);
      assert (!ext->melted);
      assert (!ext->eliminated);
      assert (!ext->blocking);
      if (!ext->frozen && !ext->tmpfrozen) {
	ext->tmpfrozen = 1;
	tmpfrozen++;
	LOG (2, "temporarily freezing external assumption %d", eass);
	erepr = lglerepr (lgl, eass);
	rext = lglelit2ext (lgl, erepr);
	if (ext != rext && !rext->frozen && !rext->tmpfrozen) {
	  assert (!rext->equiv);
	  assert (!rext->eliminated);
	  // assert (!rext->blocking);
	  LOG (2,
	    "temporarily freezing external assumption literal %d", erepr);
	  rext->tmpfrozen = 1;
	  tmpfrozen++;
	}
      }
    }
  }
  for (elit = 1; elit <= lgl->maxext; elit++) {
    ext = lglelit2ext (lgl, elit);
    if (!ext->frozen) continue;
    frozen++;
    assert (!ext->melted);
    assert (!ext->eliminated);
    assert (!ext->blocking);
    erepr = lglerepr (lgl, elit);
    rext = lglelit2ext (lgl, erepr);
    if (ext == rext) continue;
    if (rext->frozen) continue;
    if (rext->tmpfrozen) continue;
    // assert (!rext->melted);
    assert (!rext->equiv);
    assert (!rext->eliminated);
    // assert (!rext->blocking);
    LOG (2, "temporarily freezing external literal %d", erepr);
    rext->tmpfrozen = 1;
    tmpfrozen++;
  }
  melted = 0;
  for (elit = 1; elit <= lgl->maxext; elit++) {
    ext = lglelit2ext (lgl, elit);
    if (ext->frozen) continue;
    if (ext->melted) continue;
    if (ext->tmpfrozen) continue;
    LOG (2, "permanently melted external %d", elit);
    ext->melted = 1;
    melted++;
  }
  LOG (2, "found %d frozen external variables", frozen);
  LOG (2, "temporarily frozen %d external variables", tmpfrozen);
  LOG (2, "permanently melted %d external variables", melted);
  lgl->frozen = 1;
  LOG (2, "frozen solver");
}

static int lglcmprepr (LGL * lgl, int a, int b) {
  int f = lglifrozen (lgl, a), g = lglifrozen (lgl, b), res;
  if ((res = g - f)) return res;
  if ((res = (abs (a) - abs (b)))) return res;
  return a - b;
}

static int lgltarjan (LGL * lgl) {
  int * dfsimap, * mindfsimap, idx, oidx, sign, lit, blit, tag, other;
  int dfsi, mindfsi, ulit, uother, tmp, repr, res, sgn, frozen;
  const int * p, * w, * eow;
  Stk stk, component;
  AVar * av;
  HTS * hts;
  if (!lgl->nvars) return 1;
  lglfreezer (lgl);
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
	    frozen = lglifrozen (lgl, repr);
	    for (p = component.top - 1; (other = *p) != lit; p--) {
	      if (lglcmprepr (lgl, other, repr) < 0) repr = other;
	      if (!frozen && lglifrozen (lgl, other)) frozen = 1;
	    }
	    while ((other = lglpopstk (&component)) != lit) {
	      mindfsimap[lglulit (other)] = INT_MAX;
	      if (other == repr) continue;
	      if (other == -repr) {
		LOG (1, "empty clause since repr[%d] = %d", repr, other);
		lgl->mt = 1; res = 0; goto DONE;
	      }
	      sgn = lglsgn (other);
	      oidx = abs (other);
	      tmp = lgl->repr[oidx];
	      if (tmp == sgn * repr) continue;
	      LOG (2, "repr[%d] = %d", oidx, sgn * repr);
	      if (tmp) {
		LOG (1, "empty clause since repr[%d] = %d and repr[%d] = %d",
		     oidx, tmp, oidx, sgn * repr);
		lgl->mt = 1; res = 0; goto DONE;
	      } else {
		av = lglavar (lgl, oidx);
		assert (sgn*oidx == other);
		if (av->type == FREEVAR) lglimerge (lgl, other, repr);
		else assert (av->type == FIXEDVAR);
	      }
	    }
	    mindfsimap[lglulit (lit)] = INT_MAX;
	    if (frozen) {
	      LOG (2, "equivalence class of %d is frozen", repr);
	      assert (lglifrozen (lgl, repr));
	    }
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
  return res;
}

static int64_t lglsteps (LGL * lgl) {
  int64_t steps = lgl->stats->props.simp;
  steps += lgl->stats->props.search;
  steps += lgl->stats->props.lkhd;
  steps += lgl->stats->trd.steps;
  steps += lgl->stats->unhd.steps;
  steps += lgl->stats->elm.steps;
  return steps;
}

static int lglterminate (LGL * lgl) {
  int64_t steps;
  int res;
  if (!lgl->cbs) return 0;
  if (!lgl->cbs->term.fun) return 0;
  if (lgl->cbs->term.done) return 1;
  steps = lglsteps (lgl);
  if (steps < lgl->limits->term.steps) return 0;
  res = lgl->cbs->term.fun (lgl->cbs->term.state);
  if (res) lgl->cbs->term.done = res;
  else lgl->limits->term.steps = steps + lgl->opts->termint.val;
  return  res;
}

static int lglsyncunits (LGL * lgl) {
  int * units, * eou, * p, elit, erepr, ilit, res, count = 0;
  void (*produce)(void*, int);
  int64_t steps;
  Ext * ext;
  Val val;
  if (lgl->mt) return 0;
  if (!lgl->cbs) return 1;
  if (!lgl->cbs->units.consume.fun) return 1;
  assert (!lgl->simp || !lgl->level);
  steps = lglsteps (lgl);
  if (steps < lgl->limits->sync.steps) return 1;
  lgl->limits->sync.steps = steps + lgl->opts->syncint.val;
  lgl->cbs->units.consume.fun (lgl->cbs->units.consume.state, &units, &eou);
  if (units == eou) return 1;
  produce = lgl->cbs->units.produce.fun;
  lgl->cbs->units.produce.fun = 0;
  for (p = units; !lgl->mt && p < eou; p++) {
    elit = *p;
    erepr = lglerepr (lgl, elit);
    ext = lglelit2ext (lgl, erepr);
    assert (!ext->equiv);
    ilit = ext->repr;
    if (!ilit) continue;
    if (erepr < 0) ilit = -ilit;
    if (ilit == 1) continue;
    if (ilit == -1) val = -1;
    else {
      assert (abs (ilit) > 1);
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
  lgl->cbs->units.produce.fun = produce;
  if (lgl->cbs->units.consumed.fun)
    lgl->cbs->units.consumed.fun (lgl->cbs->units.consumed.state, count);
  if (lgl->mt) return 0;
  if (count)
    LOG (1, "imported %d units", lgl->tid, count);
  if (!count) return 1;
  assert (!lgl->level);
  res = lglbcp (lgl);
  if(!res && !lgl->mt) lgl->mt = 1;
  return res;
}

static int lglprbpull (LGL * lgl, int lit, int probe) {
  AVar * av;
  assert (lgl->level == 1);
  av = lglavar (lgl, lit);
  if (av->mark) return 0;
  if (!lglevel (lgl, lit)) return 0;
  assert (lglevel (lgl, lit) == 1);
  av->mark = 1;
  lglpushstk (lgl, &lgl->seen, -lit);
  LOG (3, "pulled in literal %d during probing analysis", -lit);
  return 1;
}

static int lglprbana (LGL * lgl, int probe) {
  int open, lit, r0, r1, tag, red, other, res, * p, * rsn;
  assert (lgl->level == 1);
  assert (lgl->conf.lit);
  assert (lglmtstk (&lgl->seen));
  lit = lgl->conf.lit;
  r0 = lgl->conf.rsn[0], r1 = lgl->conf.rsn[1];
  open = lglprbpull (lgl, lit, probe);
  LOG (2, "starting probing analysis with reason of literal %d", lit);
  for (;;) {
    assert (lglevel (lgl, lit) == 1);
    tag = r0 & MASKCS;
    if (tag == BINCS || tag == TRNCS) {
      other = r0 >> RMSHFT;
      if (lglprbpull (lgl, other, probe)) open++;
      if (tag == TRNCS && lglprbpull (lgl, r1, probe)) open++;
    } else {
      assert (tag == LRGCS);
      red = r0 & REDCS;
      p = lglidx2lits (lgl, LRGCS, red, r1);
      while ((other = *p++)) open += lglprbpull (lgl, other, probe);
    }
    LOG (3, "open %d antecedents in probing analysis", open-1);
    assert (open > 0);
    while (!lglmarked (lgl, lit = lglpopstk (&lgl->trail)))
      lglunassign (lgl, lit);
    lglunassign (lgl, lit);
    if (!--open) { res = lit; break; }
    LOG (2, "analyzing reason of literal %d in probing analysis next", lit);
    rsn = lglrsn (lgl, lit);
    r0 = rsn[0], r1 = rsn[1];
  }
  assert (res);
  if (res == probe)
    LOG (2, "probing analysis returns the probe %d as 1st UIP", probe);
  else
    LOG (2, "probing analysis returns different 1st UIP %d and not probe %d",
	 res, probe);
  lglpopnunmarkstk (lgl, &lgl->seen);
  return res;
}

static int lglederef (LGL * lgl, int elit) {
  int ilit, res;
  Ext * ext;
  assert (elit);
  if (abs (elit) > lgl->maxext) return -1;
  ext = lglelit2ext (lgl, elit);
  if (!(res = ext->val)) {
    assert (!ext->equiv);
    ilit = ext->repr;
    res = ilit ? lglcval (lgl, ilit) : -1;
  }
  if (elit < 0) res = -res;
  return res;
}

static int lgldecomp (LGL *); // TODO move

static int lglhasbin (LGL * lgl, int a, int b) {
  const int * w, * eow, * p;
  int blit, tag, other;
  HTS * ha, * hb;
  ha = lglhts (lgl, a);
  hb = lglhts (lgl, b);
  if (hb->count < ha->count) {
    SWAP (int, a, b); SWAP (HTS *, ha, hb);
  }
  w = lglhts2wchs (lgl, ha);
  eow = w + ha->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == OCCS) continue;
    if (tag == TRNCS || tag == LRGCS) { p++; continue; }
    assert (tag == BINCS);
    other = blit >> RMSHFT;
    if (other == b) return 1;
  }
  return 0;
}

static void lglwrkinit (LGL * lgl, int posonly, int fifo) {
  int size, lit;
  NEW (lgl->wrk, 1);
  lgl->wrk->fifo = fifo;
  size = lgl->wrk->size = lgl->nvars;
  if (posonly) {
    NEW (lgl->wrk->pos, size);
    lgl->wrk->posonly = 1;
  } else {
    NEW (lgl->wrk->pos, 2*size);
    lgl->wrk->pos += size;
    for (lit = -size + 1; lit < -1; lit++) lgl->wrk->pos[lit] = -1;
  }
  for (lit = 2; lit < size; lit++) lgl->wrk->pos[lit] = -1;
}

static void lglwrkreset (LGL * lgl) {
  lglrelstk (lgl, &lgl->wrk->queue);
  if (lgl->wrk->posonly) DEL (lgl->wrk->pos, lgl->wrk->size);
  else {
    lgl->wrk->pos -= lgl->wrk->size;
    DEL (lgl->wrk->pos, 2*lgl->wrk->size);
  }
  DEL (lgl->wrk, 1);
}

static void lglwrkcompact (LGL * lgl) {
  int i, j = 0, lit, tail = lglcntstk (&lgl->wrk->queue);
  for (i = lgl->wrk->head; i < tail; i++) {
    lit = lgl->wrk->queue.start[i];
    if (!lit) continue;
    assert (!lgl->wrk->posonly || 1 < lit);
    assert (1 < abs (lit) && abs (lit) < lgl->wrk->size);
    assert (lgl->wrk->pos[lit] == i);
    if (!lglisfree (lgl, lit)) {
      lgl->wrk->pos[lit] = -1;
      assert (lgl->wrk->count > 0);
      lgl->wrk->count--;
    } else {
      lgl->wrk->queue.start[j] = lit;
      lgl->wrk->pos[lit] = j++;
    }
  }
  lglrststk (&lgl->wrk->queue, j);
  lgl->wrk->head = 0;
}

static int lglwrktouch (LGL * lgl, int lit) {
  int tail, pos;
  if (!lglisfree (lgl, lit)) return 1;
  if (lgl->donotsched) {
    if (lgl->cgrclosing && lglavar (lgl, lit)->donotcgrcls) return 1;
    if (lgl->ternresing && lglavar (lgl, lit)->donoternres) return 1;
  }
  if (lgl->wrk->posonly) lit = abs (lit);
  tail = lglcntstk (&lgl->wrk->queue);
  LOG (4, "work touch %d", lit);
  if ((pos = lgl->wrk->pos[lit]) >= 0) {
    assert (lgl->wrk->queue.start[pos] == lit);
    lgl->wrk->queue.start[pos] = 0;
  }
  lgl->wrk->count++;
  assert (lgl->wrk->count > 0);
  lgl->wrk->pos[lit] = tail;
  lglpushstk (lgl, &lgl->wrk->queue, lit);
  if (tail/2 > lgl->wrk->count) lglwrkcompact (lgl);
  return 1;
}

static int lglwrkdeq (LGL * lgl) {
  int res, pos;
  while ((pos = lgl->wrk->head) < lglcntstk (&lgl->wrk->queue)) {
    lgl->wrk->head++;
    res = lgl->wrk->queue.start[pos];
    if (!res) continue;
    lgl->wrk->queue.start[pos] = 0;
    assert (lgl->wrk->count > 0);
    lgl->wrk->count--;
    assert (lgl->wrk->pos[res] == pos);
    lgl->wrk->pos[res] = -1;
    if (lglisfree (lgl, res)) return res;
  }
  return 0;
}

static int lglwrkpop (LGL * lgl) {
  int res;
  while (lglcntstk (&lgl->wrk->queue) > lgl->wrk->head) {
    res = lglpopstk (&lgl->wrk->queue);
    if (!res) continue;
#ifndef NDEBUG
    {
      int pos = lglcntstk (&lgl->wrk->queue);
      assert (lgl->wrk->pos[res] == pos);
    }
#endif
    lgl->wrk->pos[res] = -1;
    if (lglisfree (lgl, res)) return res;
  }
  return 0;
}

static int lglwrknext (LGL * lgl) {
  return lgl->wrk->fifo ? lglwrkdeq (lgl) : lglwrkpop (lgl);
}

static int lglrandlitrav (LGL * lgl, int (*fun)(LGL*,int lit)) {
  int delta, mod, prev, first, ulit, count;
  if (lgl->nvars < 2) return 0;
  first = mod = 2*lgl->nvars;
  ulit = lglrand (lgl) % mod;
  delta = lglrand (lgl) % mod;
  if (!delta) delta++;
  while (lglgcd (delta, mod) > 1)
    if (++delta == mod) delta = 1;
  count = mod;
  for (;;) {
    count--;
    assert (count >= 0);
    if (ulit >= 4 && !fun (lgl, lglilit (ulit))) return 0;
    prev = ulit;
    ulit += delta;
    if (ulit >= mod) ulit -= mod;
    if (ulit == first) { assert (!count); break; }
    if (first == mod) first = prev;
  }
  return 1;
}

static int lglsimpleprobebinexists (LGL * lgl, int a, int b) {
  const int * p, * w, * eow;
  int blit, tag, red, other;
  HTS * hts;
  hts = lglhts (lgl, a);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag != BINCS) continue;
    red = blit & REDCS;
    if (red) continue;
    other = blit >> RMSHFT;
    if (other == b) return 1;
  }
  return 0;
}

static int lglsimpleprobetrnexists (LGL * lgl, int a, int b, int c) {
  int blit, tag, red, other, other2;
  const int * p, * w, * eow;
  HTS * hts;
  hts = lglhts (lgl, a);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    red = blit & REDCS;
    if (red) continue;
    other = blit >> RMSHFT;
    if (tag == BINCS) {
      if (other == b) return 1;
      if (other == c) return 1;
    } else if (tag == TRNCS) {
      other2 = *p;
      if (other == b && other2 == c) return 1;
      if (other == c && other2 == b) return 1;
    }
  }
  return 0;
}

static int lglsimpleprobelrgexists (LGL * lgl, int a) {
  int blit, tag, red, other, other2, lidx, res;
  const int * p, * w, * eow, * c, * q;
  HTS * hts;
  for (p = lgl->clause.start; p + 1 < lgl->clause.top; p++) {
    other = *p;
    assert (!lglsignedmarked (lgl, other));
    lglsignedmark (lgl, other);
  }
  assert (lglsignedmarked (lgl, a));
  hts = lglhts (lgl, a);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  res = 0;
  for (p = w; !res && p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    red = blit & REDCS;
    if (red || tag == LRGCS) continue;
    other = blit >> RMSHFT;
    if (tag == BINCS) {
      res = lglsignedmarked (lgl, other);
    } else if (tag == TRNCS) {
      other2 = *p;
      res = lglsignedmarked (lgl, other) && lglsignedmarked (lgl, other2);
    } else {
      assert (tag == OCCS);
      lidx = other;
      c = lglidx2lits (lgl, OCCS, 0, lidx);
      for (q = c; (other = *q); q++)
	if (!lglsignedmarked (lgl, other)) break;
      res = !other;
    }
  }
  for (p = lgl->clause.start; p + 1 < lgl->clause.top; p++) 
    lglunmark (lgl, *p);
  return res;
}

static int lglsimpleprobeclausexists (LGL * lgl) {
  int len = lglcntstk (&lgl->clause) - 1, a, b, c, * p, * s, res;
  assert (len >= 0);
  assert (!lgl->clause.top[-1]);
  s = lgl->clause.start;
  for (p = s + 1; p + 1 < lgl->clause.top; p++)
    if (lglhts (lgl, *s)->count > lglhts (lgl, *p)->count)
      SWAP (int, *s, *p);
  a = lgl->clause.start[0];
  if (len == 2) {
    b = lgl->clause.start[1];
    res = lglsimpleprobebinexists (lgl, a, b);
  } else if (len == 3) {
    b = lgl->clause.start[1];
    c = lgl->clause.start[2];
    res = lglsimpleprobetrnexists (lgl, a, b, c);
  } else if (len > 3)
    res = lglsimpleprobelrgexists (lgl, a);
  else res = 0;
  if (res) LOG (2, "will not add already existing clause");
  return res;
}

static int lglflushclauses (LGL * lgl, int lit);

static int lglszpen (LGL * lgl) {
  int res = lglceilld (lgl->stats->irr.lits.cur/lgl->opts->sizepen.val);
  if (res < 0) res = 0;
  if (res > lgl->opts->sizemaxpen.val) res = lgl->opts->sizemaxpen.val;
  return res;
}

static void lglbasicprobelit (LGL * lgl, int root) {
  int old, ok, dom, lit, val;
  Stk lift, saved;
  const int * p;
  CLR (lift); CLR (saved);
  assert (lgl->simp);
  assert (lgl->probing || lgl->cceing);
  assert (!lgl->level);
  LOG (2, "next probe %d positive phase", root);
  assert (!lgl->level);
  if (lgl->cceing) lgl->stats->cce.probed++;
  else assert (lgl->basicprobing), lgl->stats->prb.basic.probed++;
  lgliassume (lgl, root);
  old = lgl->next;
  ok = lglbcp (lgl);
  dom = 0;
  if (ok) {
    lglclnstk (&saved);
    for (p = lgl->trail.start + old; p < lgl->trail.top; p++) {
      if ((lit = *p) == root) continue;
      lglpushstk (lgl, &saved, lit);
    }
  } else dom = lglprbana (lgl, root);
  lglbacktrack (lgl, 0);
  if (!ok) {
    LOG (1, "failed literal %d on probing", dom, root);
    lglpushstk (lgl, &lift, -dom);
    goto MERGE;
  }
  LOG (2, "next probe %d negative phase", -root);
  assert (!lgl->level);
  if (lgl->cceing) lgl->stats->cce.probed++;
  else assert (lgl->basicprobing), lgl->stats->prb.basic.probed++;
  lgliassume (lgl, -root);
  ok = lglbcp (lgl);
  if (ok) {
    for (p = saved.start; p < saved.top; p++) {
      lit = *p;
      val = lglval (lgl, lit);
      if (val <= 0) continue;
      if (lgl->cceing) lgl->stats->cce.lifted++;
      else assert (lgl->basicprobing), lgl->stats->prb.basic.lifted++;
      lglpushstk (lgl, &lift, lit);
      LOG (2, "lifted %d", lit);
    }
  } else dom = lglprbana (lgl, -root);
  lglbacktrack (lgl, 0);
  if (!ok) {
    LOG (1, "failed literal %d on probing %d", dom, -root);
    lglpushstk (lgl, &lift, -dom);
  }
MERGE:
  while (!lglmtstk (&lift)) {
    lit = lglpopstk (&lift);
    val = lglval (lgl, lit);
    if (val > 0) continue;
    if (val < 0) goto EMPTY;
    lglunit (lgl, lit);
    if (lgl->cceing) lgl->stats->cce.failed++;
    else assert (lgl->basicprobing), lgl->stats->prb.basic.failed++;
    if (lglbcp (lgl)) continue;
EMPTY:
    LOG (1, "empty clause after propagating lifted and failed literals");
    lgl->mt = 1;
  }
  lglrelstk (lgl, &lift);
  lglrelstk (lgl, &saved);
}

static int lgljwhlook (LGL * lgl) {
  Flt best, pos, neg, score;
  int res, idx, elit;
  Ext * ext;
  lgljwh (lgl);
  best = FLTMIN;
  res = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    assert (!lglelit2ext (lgl, lglexport (lgl, idx))->eliminated);
    if (lglelit2ext (lgl, lglexport (lgl, idx))->blocking) continue;
    pos = lgl->jwh[lglulit (idx)];
    neg = lgl->jwh[lglulit (-idx)];
    score = lglmulflt (pos, neg);
    score = lgladdflt (score, lgladdflt (pos, neg));
    if (res && score <= best) continue;
    LOG (1, "jwh look-ahead score %s (pos %s, neg %s) of %d",
      lglflt2str (lgl, score), lglflt2str (lgl, pos), lglflt2str (lgl, neg),
      idx);
    res = (pos > neg) ? idx : -idx;
    best = score;
  }
  if (res) {
    elit = lglexport (lgl, res);
    ext = lglelit2ext (lgl, elit);
    assert (!ext->eliminated && !ext->blocking);
    lglprt (lgl, 1, "[jwhlook] best look-ahead %d score %s",
            res, lglflt2str (lgl, best));
    if (ext->melted) {
      ext->melted = 0;
      LOG (2, "jwh-look-ahead winner external %d not melted anymore", elit);
    } else
      LOG (2, "jwh-look-ahead winner external %d was not melted anyhow", elit);
  } else LOG (1, "no proper best jwh-look-ahead literal found");
  return res;
}

static int lglislook (LGL * lgl) {
  int64_t best, pos, neg, score;
  int res, idx, elit, * scores;
  Ext * ext;
  scores = lglis (lgl);
  best = res = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    assert (!lglelit2ext (lgl, lglexport (lgl, idx))->eliminated);
    if (lglelit2ext (lgl, lglexport (lgl, idx))->blocking) continue;
    pos = scores[idx], neg = scores[-idx];
    score = pos * neg + pos + neg;
    assert (0 <= score && pos <= score && neg <= score);
    if (res && score <= best) continue;
    LOG (1, "lis look-ahead score %lld (pos %lld, neg %lld) of %d",
         (LGLL) score, (LGLL) pos, (LGLL) neg, idx);
    res = (pos > neg) ? idx : -idx;
    best = score;
  }
  scores -= lgl->nvars;
  DEL (scores, 2*lgl->nvars);
  if (res) {
    elit = lglexport (lgl, res);
    ext = lglelit2ext (lgl, elit);
    assert (!ext->eliminated && !ext->blocking);
    lglprt (lgl, 1, "[lislook] best look-ahead %ld score %d", 
            res, (LGLL) best);
    if (ext->melted) {
      ext->melted = 0;
      LOG (2, "lis-look-ahead winner external %d not melted anymore", elit);
    } else
      LOG (2, "lis-look-ahead winner external %d was not melted anyhow", elit);
  } else LOG (1, "no proper best jwh-look-ahead literal found");
  return res;
}

static int lglschedbasicprobe (LGL * lgl, Stk * probes, int round) {
  int idx, res, i, j, donotbasicprobes, keepscheduled;
  assert (lglmtstk (probes));
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lgl->opts->prbasic.val <= 1 &&
        lglhasbins (lgl, idx) && lglhasbins (lgl, -idx)) continue;
    LOG (1, "new probe %d", idx);
    lglpushstk (lgl, probes, idx);
  }
  res = lglcntstk (probes);
  donotbasicprobes = keepscheduled = 0;
  for (i = 0; i < res; i++) {
    idx = lglpeek (probes, i);
    if (!idx) continue;
    assert (lglisfree (lgl, idx));
    if (lglavar (lgl, idx)->donotbasicprobe) donotbasicprobes++;
    else keepscheduled++;
  }
  if (!keepscheduled) {
    for (i = 0; i < res; i++) {
      idx = lglpeek (probes, i);
      if (!idx) continue;
      assert (lglisfree (lgl, idx));
      lglavar (lgl, idx)->donotbasicprobe = 0;
      keepscheduled++;
    }
    donotbasicprobes = 0;
  }
  for (i = 0; i < res; i++) {
    idx = lglpeek (probes, i);
    if (!idx) continue;
    assert (lglisfree (lgl, idx));
    if (lglavar (lgl, idx)->donotbasicprobe) donotbasicprobes++;
    else keepscheduled++;
  }
  j = 0;
  for (i = 0; i < res; i++) {
    idx = lglpeek (probes, i);
    if (!idx) continue;
    if (!lglavar (lgl, idx)->donotbasicprobe)
      lglpoke (probes, j++, idx);
  }
  lglrststk (probes, (res = j));
  if (!res)
    lglprt (lgl, 2, "[basicprobe-%d-%d] no potential probes found",
	    lgl->stats->prb.basic.count, round);
  else if (!donotbasicprobes)
    lglprt (lgl, 2, "[basicprobe-%d-%d] scheduled all %d potential probes",
	    lgl->stats->prb.basic.count, round, res);
  else
    lglprt (lgl, 2, "[basicprobe-%d-%d] scheduled %d probes %.0f%%",
	    lgl->stats->prb.basic.count, round,
	    res, lglpcnt (res, lglrem (lgl)));
  return res;
}

static void lglupdprbasicpen (LGL * lgl, int success) {
  if (success && lgl->limits->prb.pen.basic)
    lgl->limits->prb.pen.basic--;
  if (!success && lgl->limits->prb.pen.basic < MAXPEN)
    lgl->limits->prb.pen.basic++;
}

static void lglsetprbasiclim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->prbasicreleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->prbasicmineff.val)
    limit = lgl->opts->prbasicmineff.val;
  if (lgl->opts->prbasicmaxeff.val >= 0 &&
      limit > lgl->opts->prbasicmaxeff.val)
    limit = lgl->opts->prbasicmaxeff.val;
  limit >>= (pen = lgl->limits->prb.pen.basic + lglszpen (lgl));
  irrlim = lgl->stats->irr.lits.cur/2;
  irrlim >>= lgl->limits->simp.pen;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1,
      "[basicprobe-%d] limit %lld based on %d irredundant litearls",
      lgl->stats->prb.basic.count,
      (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[basicprobe-%d] limit %lld penalty %d = %d + %d",
      lgl->stats->prb.basic.count, (LGLL) limit,
      pen, lgl->limits->prb.pen.basic, lglszpen (lgl));
  lgl->limits->prb.steps = lgl->stats->prb.basic.steps + limit;
}

static int lglbasicprobe (LGL * lgl) {
#ifndef NLGLOG
  int origprobed = lgl->stats->prb.basic.probed;
#endif
  int origfailed = lgl->stats->prb.basic.failed;
  int origlifted = lgl->stats->prb.basic.lifted;
  int orighbr = lgl->stats->hbr.cnt;
  int root, failed, lifted, units, first, idx;
  int oldrem, deltarem, deltahbr, remprobes;
  int oldhbr, oldfailed, oldlifted;
  int nprobes, success, round;
  Stk probes, lift, saved;
  unsigned pos, delta;
#ifndef NLGLOG
  int probed;
#endif
  if (!lgl->nvars) return 1;
  if (!lgl->opts->probe.val) return 1;
  lglstart (lgl, &lgl->times->prb.basic);
  lgl->stats->prb.basic.count++;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  assert (!lgl->simp && !lgl->probing && !lgl->basicprobing);
  lgl->simp = lgl->probing = lgl->basicprobing = 1;
  CLR (lift); CLR (probes); CLR (saved);
  lglsetprbasiclim (lgl);
  oldfailed = origfailed;
  oldlifted = origlifted;
  oldhbr = lgl->stats->hbr.cnt;
  oldrem = lglrem (lgl);
  round = 0;
RESTART:
  nprobes = lglschedbasicprobe (lgl, &probes, round);
  remprobes = 0;
  if (!nprobes) goto DONE;
  pos = lglrand (lgl) % nprobes;
  delta = lglrand (lgl) % nprobes;
  if (!delta) delta++;
  while (lglgcd (delta, nprobes) > 1)
    if (++delta == nprobes) delta = 1;
  LOG (1, "probing start %u delta %u mod %u", pos, delta, nprobes);
  first = 0;
  while (!lgl->mt) {
    if (lgl->stats->prb.basic.steps >= lgl->limits->prb.steps) break;
    if (lglterminate (lgl)) break;
    if (!lglsyncunits (lgl)) break;
    assert (pos < (unsigned) nprobes);
    root = probes.start[pos];
    probes.start[pos] = 0;
    if (!root || root == first) {
      lglprt (lgl, 1,
        "[basicprobe-%d-%d] %d sched %.0f%%, %d failed, %d lifted, %d hbrs",
	lgl->stats->prb.basic.count, round,
	nprobes, lglpcnt (nprobes, lglrem (lgl)),
        lgl->stats->prb.basic.failed - oldfailed,
        lgl->stats->prb.basic.lifted - oldlifted,
        lgl->stats->hbr.cnt - oldhbr);
      for (idx = 2; idx < lgl->nvars; idx++)
	lglavar (lgl, idx)->donotbasicprobe = 0;
      break;
    }
    lglavar (lgl, root)->donotbasicprobe = 1;
    if (!first) first = root;
    pos += delta;
    if (pos >= nprobes) pos -= nprobes;
    if (!lglisfree (lgl, root)) continue;
    lglbasicprobelit (lgl, root);
  }
  if (!lgl->mt) {
    if (lgl->stats->prb.basic.steps >= lgl->limits->prb.steps) {
      while (!lglmtstk (&probes))
	if((idx = lglpopstk (&probes)) && lglisfree (lgl, idx)) remprobes++;
      lglprt (lgl, 1, 
        "[basicprobe-%d-%d] %d probes remain %.0f%% after last round",
	lgl->stats->prb.basic.count, round,
	remprobes, lglpcnt (remprobes, lglrem (lgl)));
    } else if (round >= lgl->opts->prbasicroundlim.val) {
      lglprt (lgl, 1,
	      "[basicprobe-%d-%d] round limit %d hit",
	      lgl->stats->prb.basic.count, round, 
	      lgl->opts->prbasicroundlim.val);
    } else if (lgl->stats->prb.basic.failed > oldfailed ||
               lgl->stats->prb.basic.lifted > oldlifted ||
	       lgl->stats->hbr.cnt > oldhbr) {
      oldfailed = lgl->stats->prb.basic.failed;
      oldlifted = lgl->stats->prb.basic.lifted;
      lglclnstk (&probes);
      if (oldhbr < lgl->stats->hbr.cnt && !lgldecomp (lgl)) goto DONE;
      oldhbr = lgl->stats->hbr.cnt;
      round++;
      goto RESTART;
    } else {
      assert (!remprobes);
      lglprt (lgl, 1,
	      "[basicprobe-%d-%d] fully completed probing",
	      lgl->stats->prb.basic.count, round);
      for (idx = 2; idx < lgl->nvars; idx++)
	lglavar (lgl, idx)->donotbasicprobe = 0;
    }
  }
DONE:
  lglrelstk (lgl, &lift);
  lglrelstk (lgl, &probes);
  lglrelstk (lgl, &saved);

  assert (lgl->stats->hbr.cnt >= orighbr);
  assert (lglrem (lgl) <= oldrem);
  deltarem = oldrem - lglrem (lgl);
  deltahbr = lgl->stats->hbr.cnt - orighbr;
  success = deltarem || deltahbr;
  lglupdprbasicpen (lgl, deltarem);
  assert (lgl->stats->prb.basic.failed >= origfailed);
  assert (lgl->stats->prb.basic.lifted >= origlifted);
  failed = lgl->stats->prb.basic.failed - origfailed;
  lifted = lgl->stats->prb.basic.lifted - origlifted;
#ifndef NLGLOG
  assert (lgl->stats->prb.basic.probed >= origprobed);
  probed = lgl->stats->prb.basic.probed - origprobed;
  LOG (1, "%ssuccessfully probed %d out of %d probes %.1f%%",
       success ? "" : "un", probed, nprobes, lglpcnt (probed, nprobes));
  LOG (1, "found %d failed %.1f%% lifted %d through probing",
       failed, lglpcnt (failed, probed), lifted);
#endif
  assert (lgl->probing && lgl->simp && lgl->basicprobing);
  lgl->simp = lgl->probing = lgl->basicprobing = 0;
  units = failed + lifted;
  lglprt (lgl, 1 + !units,
    "[basicprobe-%d-%d] %d units = %d failed (%.0f%%) + %d lifted (%.0f%%)",
    lgl->stats->prb.basic.count, round,
    units, failed, lglpcnt (failed, units), lifted, lglpcnt (lifted, units));
  lglprt (lgl, 1 + !success,
    "[basicprobe-%d-%d] removed %d variables, found %d hbrs",
    lgl->stats->prb.basic.count, round, deltarem, deltahbr);
  lglrep (lgl, 1 + !success, 'p');
  lglstop (lgl);
  return !lgl->mt;
}

static int lglsmallirr (LGL * lgl) {
  int maxirrlidx = lglcntstk (&lgl->irr), limit;
  int64_t tmp = MAXREDLIDX;
  tmp *= lgl->opts->smallirr.val;
  tmp /= 100;
  limit = (tmp < INT_MAX) ? tmp : INT_MAX;
  if (maxirrlidx >= limit) return 0;
  return  1;
}

static int lglprobe (LGL * lgl) {
  int res = 1;
  lglstart (lgl, &lgl->times->prb.all);
  res = lglbasicprobe (lgl);
  lglstop (lgl);
  return res;
}

static void lgldense (LGL * lgl, int occstoo) {
  int lit, lidx, count, idx, other, other2, blit, sign, tag, red;
  const int * start, * top, * c, * p, * eow;
  int * q, * w;
  EVar * ev;
  HTS * hts;
  LOG (1, "transition to dense mode");
  assert (!lgl->dense);
  assert (!lgl->evars);
  assert (lglsmallirr (lgl));
  assert (lglmtstk (&lgl->esched));
  assert (!lgl->eliminating || !lgl->elm->pivot);
  lgl->stats->dense++;
  count = 0;
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
	if (red && tag == LRGCS) continue;
	*q++ = blit;
	if (tag == LRGCS || tag == TRNCS) *q++ = *p;
	if (red) continue;
	if (tag == BINCS || tag == TRNCS) {
	  other = blit >> RMSHFT;
	  if (abs (other) < idx) continue;
	  if (tag == TRNCS) {
	    other2 = *p;
	    if (abs (other2) < idx) continue;
	    lglincocc (lgl, other2), count++;
	  }
	  lglincocc (lgl, lit), count++;
	  lglincocc (lgl, other), count++;
	} else {
	  assert (tag == LRGCS);
	}
      }
      lglshrinkhts (lgl, hts, q - w);
    }
  if (count)
    LOG (1, "counted %d occurrences in small irredundant clauses", count);
  if (occstoo) {
    count = 0;
    start = lgl->irr.start;
    top = lgl->irr.top;
    for (c = start; c < top; c = p + 1) {
      p = c;
      if (*c >= NOTALIT) continue;
      lidx = c - start;
      assert (lidx < MAXIRRLIDX);
      blit = (lidx << RMSHFT) | OCCS;
      for (; (lit = *p); p++) {
	hts = lglhts (lgl, lit);
	lglpushwch (lgl, hts, blit);
	lglincocc (lgl, lit), count++;
      }
    }
  }
  if (count)
    LOG (1, "counted %d occurrences in large irredundant clauses", count);
  count = 0;
  if (!lgl->cgrclosing && !lgl->probing && !lgl->gaussing && !lgl->cceing) {
    for (idx = 2; idx < lgl->nvars; idx++) {
      ev = lglevar (lgl, idx);
      if (ev->pos >= 0) continue;
      if (lglifrozen (lgl, idx)) continue;
      if (lgl->donotsched) {
	AVar * av = lglavar (lgl, idx);
	if (lgl->eliminating && av->donotelm) continue;
	if (lgl->blocking && av->donotblk) continue;
	if (lgl->cceing && av->donotcce) continue;
      }
      assert (!ev->occ[0] && !ev->occ[1]);
      lglesched (lgl, idx);
      count++;
    }
    if (count) LOG (1, "scheduled %d zombies", count);
  }
  LOG (1, "continuing in dense mode");
  lgl->dense = 1 + occstoo;
  lglfullyconnected (lgl);
  if (lgl->opts->verbose.val >= 1) {
    const char * str;
    int inst, vl;
    count = 0;
    if (lgl->eliminating) str = "elim", inst = lgl->stats->elm.count, vl = 1;
    else if (lgl->blocking) str = "block", inst = lgl->stats->blk.count, vl=1;
    else str = "dense", inst = lgl->stats->dense, vl = 2;
    for (idx = 2; idx < lgl->nvars; idx++)
      if (lglevar (lgl, idx)->pos >= 0) count++;
    lglprt (lgl, vl,
      "[%s-%d] scheduled %d variables %.0f%%",
      str, inst, count, lglpcnt (count, lgl->nvars-2));
  }
}

static void lglsparse (LGL * lgl) {
  int idx, sign, lit, count, blit, tag;
  int * w, *p, * eow, * q;
  HTS * hts;
  assert (!lgl->notfullyconnected);
  assert (lgl->dense);
  lgl->stats->sparse++;
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
	if (tag == OCCS) { count++; continue; }
	*q++ = blit;
	if (tag == BINCS) continue;
	assert (tag == LRGCS || tag == TRNCS);
	assert (p + 1 < eow);
	*q++ = *++p;
      }
      assert (hts->count - (p - q) == q - w);
      lglshrinkhts (lgl, hts, q - w);
    }
  DEL (lgl->evars, lgl->nvars);
  lglrelstk (lgl, &lgl->esched);
  LOG (1, "removed %d full irredundant occurrences", count);
  lgl->dense = 0;
  lgl->notfullyconnected = 1;
  LOG (1, "large clauses not fully connected yet");
}

static int lglm2i (LGL * lgl, int mlit) {
  int res, midx = abs (mlit);
  assert (0 < midx);
  res = lglpeek (&lgl->elm->m2i, midx);
  if (mlit < 0) res = -res;
  return res;
}

static int lgli2m (LGL * lgl, int ilit) {
  AVar * av = lglavar (lgl, ilit);
  int res = av->mark;
  if (!res) {
    res = lglcntstk (&lgl->seen) + 1;
    av->mark = res;
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm->lsigs) - 2);
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm->noccs) - 2);
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm->mark) - 2);
    assert (2*lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm->occs) - 2);
    assert (lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm->m2i) - 1);
    lglpushstk (lgl, &lgl->seen, abs (ilit));
    lglpushstk (lgl, &lgl->elm->lsigs, 0);
    lglpushstk (lgl, &lgl->elm->lsigs, 0);
    lglpushstk (lgl, &lgl->elm->noccs, 0);
    lglpushstk (lgl, &lgl->elm->noccs, 0);
    lglpushstk (lgl, &lgl->elm->mark, 0);
    lglpushstk (lgl, &lgl->elm->mark, 0);
    lglpushstk (lgl, &lgl->elm->occs, 0);
    lglpushstk (lgl, &lgl->elm->occs, 0);
    lglpushstk (lgl, &lgl->elm->m2i, abs (ilit));
    LOG (4, "mapped internal variable %d to marked variable %d",
	 abs (ilit), res);
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
  int ilit, mlit, umlit, size = 0, next, prev;
  unsigned csig = 0;
  const int * p;
  Val val;
#if !defined (NDEBUG) || !defined (NLGLOG)
  int lidx;
#endif
  LOGCLS (3, c, "copying irredundant clause");
  lgl->stats->elm.steps++;
  lgl->stats->elm.copies++;
  size = 0;
  for (p = c; (ilit = *p); p++) {
    val = lglval (lgl, ilit);
    assert (val <= 0);
    if (val < 0) continue;
    size++;
    if (abs (ilit) == lgl->elm->pivot) continue;
    mlit = lgli2m (lgl, ilit);
    assert (abs (mlit) != 1);
    csig |= lglsig (mlit);
  }
  assert (size >= 1);
  next = lglcntstk (&lgl->elm->lits);
#if !defined (NDEBUG) || !defined (NLGLOG)
  lidx = next;
#endif
  assert (next > 0);
  for (p = c; (ilit = *p); p++) {
    val = lglval (lgl, ilit);
    if (val < 0) continue;
    mlit = lgli2m (lgl, ilit);
    lglpushstk (lgl, &lgl->elm->lits, mlit);
    umlit = lglulit (mlit);
    prev = lglpeek (&lgl->elm->occs, umlit);
    lglpushstk (lgl, &lgl->elm->next, prev);
    lglpoke (&lgl->elm->occs, umlit, next++);
    lglpushstk (lgl, &lgl->elm->csigs, csig);
    lglpushstk (lgl, &lgl->elm->sizes, size);
    lgl->elm->noccs.start[umlit]++;
    lgl->elm->lsigs.start[umlit] |= csig;
  }
  lglpushstk (lgl, &lgl->elm->lits, 0);
  lglpushstk (lgl, &lgl->elm->next, 0);
  lglpushstk (lgl, &lgl->elm->csigs, 0);
  lglpushstk (lgl, &lgl->elm->sizes, 0);
  lgl->elm->necls++;
  LOGCLS (4, lgl->elm->lits.start + lidx, "copied and mapped clause");
#ifndef NDEBUG
  LOGMCLS (4, lgl->elm->lits.start + lidx, "copied and remapped clause");
  {
    int i, j = 0;
    for (i = 0; c[i]; i++) {
      Val val = lglval (lgl, c[i]);
      assert (val <= 0);
      if (val < 0) continue;
      assert (c[i] == lglm2i (lgl, lglpeek (&lgl->elm->lits, lidx + j++)));
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
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    red = blit & REDCS;
    if (red) continue;
    if (tag == BINCS || tag == TRNCS) {
      d[0] = lit;
      other = blit >> RMSHFT;
      d[1] = other;
      if (tag == TRNCS) d[2] = *p, d[3] = 0;
      else d[2] = 0;
      c = d;
    } else {
      assert (tag == OCCS);
      lidx = (tag == OCCS) ? (blit >> RMSHFT) : *p;
      c = lglidx2lits (lgl, OCCS, 0, lidx);
    }
    lgladdecl (lgl, c);
    count++;
  }
  return count;
}

static void lglrstecls (LGL * lgl)  {
  assert (lgl->elm->pivot);
  lglclnstk (&lgl->elm->lits);
  lglclnstk (&lgl->elm->next);
  lglclnstk (&lgl->elm->csigs);
  lglclnstk (&lgl->elm->lsigs);
  lglclnstk (&lgl->elm->sizes);
  lglclnstk (&lgl->elm->occs);
  lglclnstk (&lgl->elm->noccs);
  lglclnstk (&lgl->elm->mark);
  lglclnstk (&lgl->elm->m2i);
  lglpopnunmarkstk (lgl, &lgl->seen);
  lgl->elm->pivot = 0;
}

static void lglrelecls (LGL * lgl)  {
  lglrelstk (lgl, &lgl->elm->lits);
  lglrelstk (lgl, &lgl->elm->next);
  lglrelstk (lgl, &lgl->elm->csigs);
  lglrelstk (lgl, &lgl->elm->lsigs);
  lglrelstk (lgl, &lgl->elm->sizes);
  lglrelstk (lgl, &lgl->elm->occs);
  lglrelstk (lgl, &lgl->elm->noccs);
  lglrelstk (lgl, &lgl->elm->mark);
  lglrelstk (lgl, &lgl->elm->m2i);
  lglrelstk (lgl, &lgl->elm->clv);
}

static void lglinitecls (LGL * lgl, int idx) {
#ifndef NLGLOG
  int clauses;
#endif
  assert (!lgl->elm->pivot);
  assert (idx >= 2);
  assert (lglmtstk (&lgl->elm->lits));
  assert (lglmtstk (&lgl->elm->next));
  assert (lglmtstk (&lgl->elm->csigs));
  assert (lglmtstk (&lgl->elm->lsigs));
  assert (lglmtstk (&lgl->elm->sizes));
  assert (lglmtstk (&lgl->elm->occs));
  assert (lglmtstk (&lgl->elm->noccs));
  assert (lglmtstk (&lgl->elm->m2i));
  assert (lglmtstk (&lgl->seen));
  lgl->elm->pivot = idx;
  lglpushstk (lgl, &lgl->elm->mark, 0);
  lglpushstk (lgl, &lgl->elm->mark, 0);
  lglpushstk (lgl, &lgl->elm->occs, 0);
  lglpushstk (lgl, &lgl->elm->occs, 0);
  lglpushstk (lgl, &lgl->elm->noccs, 0);
  lglpushstk (lgl, &lgl->elm->noccs, 0);
  lglpushstk (lgl, &lgl->elm->lsigs, 0);
  lglpushstk (lgl, &lgl->elm->lsigs, 0);
  lglpushstk (lgl, &lgl->elm->m2i, 0);
  (void) lgli2m (lgl, idx);
  lglpushstk (lgl, &lgl->elm->lits, 0);
  lglpushstk (lgl, &lgl->elm->next, 0);
  lglpushstk (lgl, &lgl->elm->csigs, 0);
  lglpushstk (lgl, &lgl->elm->sizes, 0);
  lgl->elm->necls = 0;
#ifndef NLGLOG
  clauses =
#endif
  lglecls (lgl, idx);
  lgl->elm->negcls = lgl->elm->necls;
  lgl->elm->neglidx = lglcntstk (&lgl->elm->lits);
#ifndef NLGLOG
  clauses +=
#endif
  lglecls (lgl, -idx);
  LOG (2, "found %d variables in %d clauses with %d or %d",
       lglcntstk (&lgl->seen), clauses, idx, -idx);
  assert (lgl->elm->pivot);
}

static void lglelrmcls (LGL * lgl, int lit, int * c, int clidx) {
  int lidx, i, other, ulit, * lits, * csigs, blit, tag, red, other2;
  int * p, * eow, * w, count;
  HTS * hts;
#ifndef NDEBUG
  int size;
#endif
  lits = lgl->elm->lits.start;
  csigs = lgl->elm->csigs.start;
  assert (lits < c && c < lgl->elm->lits.top - 1);
  lidx = c - lits;
  LOGCLS (2, c, "removing clause");
  for (i = lidx; (other = lits[i]); i++) {
    assert (other < NOTALIT);
    lits[i] = REMOVED;
    csigs[i] = 0;
    ulit = lglulit (other);
    assert (ulit < lglcntstk (&lgl->elm->noccs));
    assert (lgl->elm->noccs.start[ulit] > 0);
    lgl->elm->noccs.start[ulit] -= 1;
  }
#ifndef NDEBUG
  size = lglpeek (&lgl->elm->sizes, lidx);
#endif
  hts = lglhts (lgl, lit);
  assert (hts->count > 0 && hts->count >= clidx);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  blit = tag = count = 0;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
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
    assert (tag == OCCS);
    lidx = (tag == OCCS) ? (blit >> RMSHFT) : *p;
#ifndef NDEBUG
    {
      int * q, * d = lglidx2lits (lgl, OCCS, 0, lidx);
      for (q = d; *q; q++)
	;
      assert (q - d >= size);
    }
#endif
    lglrmlcls (lgl, lidx, 0);
  }
}

static int lglbacksub (LGL * lgl, int * c, int str) {
  int * start = lgl->elm->lits.start, * p, * q, marked = 0, res, * d;
  int lit, ulit, occ, next, osize, other, uolit, size, plit, phase, clidx;
  unsigned ocsig, lsig, csig = 0;
#ifndef NLGLOG
  const char * mode = str ? "strengthening" : "subsumption";
#endif
  LOGMCLS (3, c, "backward %s check for clause", mode);
  LOGCLS (3, c, "backward %s check for mapped clause", mode);
  phase = (c - start) >= lgl->elm->neglidx;
  for (p = c; (lit = *p); p++)
    if (abs (lit) != 1)
      csig |= lglsig (lit);
  size = p - c;
  assert (csig == lglpeek (&lgl->elm->csigs, c - start));
  assert (size == lglpeek (&lgl->elm->sizes, c - start));
  res = 0;

  if (str) phase = !phase;
  lit = phase ? -1 : 1;

  ulit = lglulit (lit);
  occ = lglpeek (&lgl->elm->noccs, ulit);
  if (!str && occ <= 1) return 0;
  if (str && !occ) return 0;
  lsig = lglpeek (&lgl->elm->lsigs, ulit);
  if ((csig & ~lsig)) return 0;
  for (next = lglpeek (&lgl->elm->occs, ulit);
       !res && next;
       next = lglpeek (&lgl->elm->next, next)) {
      lgl->stats->elm.steps++;
      if (next == p - start) continue;
      if (phase != (next >= lgl->elm->neglidx)) continue;
      plit = lglpeek (&lgl->elm->lits, next);
      if (plit >= NOTALIT) continue;
      assert (plit == lit);
      osize = lglpeek (&lgl->elm->sizes, next);
      if (osize > size) continue;
      ocsig = lglpeek (&lgl->elm->csigs, next);
      assert (ocsig);
      if ((ocsig & ~csig)) continue;
      if (!marked) {
	for (q = c; (other = *q); q++) {
	  if (str && abs (other) == 1) other = -other;
	  uolit = lglulit (other);
	  assert (!lglpeek (&lgl->elm->mark, uolit));
	  lglpoke (&lgl->elm->mark, uolit, 1);
	}
	marked = 1;
      }
      d = lgl->elm->lits.start + next;
      if (c <= d && d < c + size) continue;
      if (str) lgl->stats->elm.strchks++; else lgl->stats->elm.subchks++;
      while (d[-1]) d--;
      assert (c != d);
      LOGMCLS (3, d, "backward %s check with clause", mode);
      res = 1;
      for (q = d; res && (other = *q); q++) {
	uolit = lglulit (other);
	res = lglpeek (&lgl->elm->mark, uolit);
      }
      if (!res || !str || osize < size) continue;
      LOGMCLS (2, d, "strengthening by double self-subsuming resolution");
      assert ((c - start) < lgl->elm->neglidx);
      assert ((d - start) >= lgl->elm->neglidx);
      assert (phase);
      clidx = 0;
      q = lgl->elm->lits.start + lgl->elm->neglidx;
      while (q < d) {
	other = *q++;
	if (other >= NOTALIT) { while (*q++) ; continue; }
	if (!other) clidx++;
      }
      LOGMCLS (2, d,
	"strengthened and subsumed original irredundant clause");
      LOGCLS (3, d, "strengthened and subsumed mapped irredundant clause");
      lglelrmcls (lgl, -lgl->elm->pivot, d, clidx);
  }
  if (marked) {
    for (p = c; (lit = *p); p++) {
      if (str && abs (lit) == 1) lit = -lit;
      ulit = lglulit (lit);
      assert (lglpeek (&lgl->elm->mark, ulit));
      lglpoke (&lgl->elm->mark, ulit, 0);
    }
  }
  return res;
}

static void lglelmsub (LGL * lgl) {
  int clidx, count, subsumed, pivot, * c;
  count = clidx = subsumed = 0;
  pivot = lgl->elm->pivot;
  for (c = lgl->elm->lits.start + 1;
       c < lgl->elm->lits.top &&
	 lgl->limits->elm.steps > lgl->stats->elm.steps;
       c++) {
    lgl->stats->elm.steps++;
    if (count++ == lgl->elm->negcls) clidx = 0, pivot = -pivot;
    if (lglbacksub (lgl, c, 0)) {
      subsumed++;
      lgl->stats->elm.sub++;
      LOGMCLS (2, c, "subsumed original irredundant clause");
      LOGCLS (3, c, "subsumed mapped irredundant clause");
      lglelrmcls (lgl, pivot, c, clidx);
    } else clidx++;
    while (*c) c++;
  }
  LOG (2, "subsumed %d clauses containing %d or %d",
       subsumed, lgl->elm->pivot, -lgl->elm->pivot);
}

static int lglelmstr (LGL * lgl) {
  int clidx, count, strengthened, pivot, * c, * p, mlit, ilit, res, found;
  int size;
  count = clidx = strengthened = 0;
  pivot = lgl->elm->pivot;
  res = 0;
  LOG (3, "strengthening with pivot %d", pivot);
  for (c = lgl->elm->lits.start + 1;
       c < lgl->elm->lits.top &&
	 lgl->limits->elm.steps > lgl->stats->elm.steps;
       c++) {
    lgl->stats->elm.steps++;
    if (count++ == lgl->elm->negcls) {
      clidx = 0, pivot = -pivot;
      LOG (3, "strengthening with pivot %d", pivot);
    }
    if (*c == REMOVED) {
      while (*c) { assert (*c == REMOVED); c++; }
      continue;
    }
    if (lglbacksub (lgl, c, 1)) {
      strengthened++;
      lgl->stats->elm.str++;
      LOGMCLS (2, c, "strengthening original irredundant clause");
      LOGCLS (3, c, "strengthening mapped irredundant clause");
      assert (lglmtstk (&lgl->clause));
      found = 0;
      size = 0;
      for (p = c; (mlit = *p); p++) {
	ilit = lglm2i (lgl, *p);
	if (ilit == pivot) { found++; continue; }
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
      lgladdcls (lgl, 0, 0, 1);
      lglclnstk (&lgl->clause);
      if (size == 1) { res = 1; break; }
    } else clidx++;
    while (*c) c++;
  }
  LOG (2, "strengthened %d clauses containing %d or %d",
       strengthened, lgl->elm->pivot, -lgl->elm->pivot);
  return res;
}

static int lglflushclauses (LGL * lgl, int lit) {
  int blit, tag, red, other, other2, count, glue, res;
  const int * p, * w, * eow;
  int lidx, glidx, slidx;
  int * c, * q;
  HTS * hts;
#ifndef NDEBUG
  int occs;
#endif
  lglchkirrstats (lgl);
  assert (lgl->probing || lgl->lkhd || lgl->dense);
  hts = lglhts (lgl, lit);
  if (!hts->count) return 0;
#ifndef NDEBUG
  occs = lglocc (lgl, lit);
#endif
  res = 0;
  LOG (2, "flushing clauses with literal %d", lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  count = 0;
  for (p = w; p < eow; p++) {
    if (lgl->blocking) lgl->stats->blk.steps++;
    if (lgl->eliminating) lgl->stats->elm.steps++;
    blit = *p;
    tag = blit & MASKCS;
    red = blit & REDCS;
    other = blit >> RMSHFT;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == BINCS) {
      lglrmbwch (lgl, other, lit, red);
      LOG (2, "flushed %s binary clause %d %d", lglred2str (red), lit, other);
      lgldeclscnt (lgl, 2, red, 0);
      if (!red) lgldecocc (lgl, lit), lgldecocc (lgl, other), res++;
      count++;
    } else if (tag == TRNCS) {
      other2 = *p;
      lglrmtwch (lgl, other2, lit, other, red);
      lglrmtwch (lgl, other, lit, other2, red);
      LOG (2, "flushed %s ternary clause %d %d %d",
	   lglred2str (red), lit, other, other2);
      lgldeclscnt (lgl, 3, red, 0);
      if (!red)  {
	lgldecocc (lgl, lit);
	lgldecocc (lgl, other);
	lgldecocc (lgl, other2);
	res++;
      }
      count++;
    } else {
      assert (tag == OCCS || tag == LRGCS);
      if (tag == LRGCS) {
	lidx = *p;
	c = lglidx2lits (lgl, LRGCS, red, lidx);
	glue = red ? (lidx & GLUEMASK) : 0;
      } else {
	lidx = blit >> RMSHFT;
	c = lglidx2lits (lgl, OCCS, red, lidx);
	glue = 0;
      }
      other = c[0];
      if (other >= NOTALIT) continue;
      LOGCLS (2, c, "flushed %s large clause", lglred2str (red));
      if (tag == LRGCS) {
	if (other == lit) other = c[1];
	assert (abs (other) != abs (lit));
	lglrmlwch (lgl, other, red, lidx);
      } else {
	glidx = lidx;
	if (red) glidx <<= GLUESHFT;
	if (c[1] != lit) lglrmlwch (lgl, c[1], red, glidx);
	if (other != lit) lglrmlwch (lgl, other, red, glidx);
      }
      if (red) {
	assert (!glue);
	LGLCHKACT (c[-1]);
	c[-1] = REMOVED;
      } else lgldecocc (lgl, lit);
      for (q = c; (other = *q); q++) {
	assert (other < NOTALIT);
	*q = REMOVED;
	if (other == lit) continue;
	if (red && glue) continue;
	slidx = lidx;
	if (red && tag == LRGCS) slidx >>= GLUESHFT;
	lglrmlocc (lgl, other, red, slidx);
	if (!red) lgldecocc (lgl, other);
      }
      *q = REMOVED;
      lgldeclscnt (lgl, q - c, red, glue);
      if (!red) res++;
      count++;
    }
  }
  if (!lgl->probing && !lgl->cgrclosing) assert (occs == res);
  lglshrinkhts (lgl, hts, 0);
  LOG (2, "flushed %d clauses with %d including %d irredundant",
       count, lit, res);
  lglchkirrstats (lgl);
  return res;
}

static int lglflushlits (LGL * lgl, int lit) {
  int blit, tag, red, other, other2, size, satisfied, d[3], glue;
  int * p, * w, * eow, * c, * l, * k;
  int lidx, slidx, glidx;
  int count, res;
  Val val, val2;
  long delta;

#define FIXPTRS() do { p += delta, w += delta, eow += delta; } while (0)

  HTS * hts;
  LOG (2, "flushing literal %d from clauses", lit);
  assert (!lgl->level);
  assert (lglifixed (lgl, lit) < 0);
  assert (lgl->dense);
  lglchkirrstats (lgl);
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  res = count = 0;
  for (p = w; p < eow; p++) {
    if (lgl->blocking) lgl->stats->blk.steps++;
    if (lgl->eliminating) lgl->stats->elm.steps++;
    count++;
    blit = *p;
    tag = blit & MASKCS;
    red = blit & REDCS;
    if (tag == BINCS) {
      other = blit >> RMSHFT;
      assert ((red && lgliselim (lgl, other)) || lglval (lgl, other) > 0);
      lglrmbwch (lgl, other, lit, red);
      LOG (2, "flushed %s binary clause %d %d", lglred2str (red), lit, other);
      lgldeclscnt (lgl, 2, red, 0);
      if (!red) {
	if (lgl->dense) lgldecocc (lgl, lit), lgldecocc (lgl, other);
	res++;
      }
    } else if (tag == TRNCS) {
      other = blit >> RMSHFT;
      other2 = *++p;
      lglrmtwch (lgl, other2, lit, other, red);
      lglrmtwch (lgl, other, lit, other2, red);
      LOG (2, "flushed %s ternary clause %d %d %d",
	   lglred2str (red), lit, other, other2);
      lgldeclscnt (lgl, 3, red, 0);
      if (!red)  {
	if (lgl->dense) {
	  lgldecocc (lgl, lit);
	  lgldecocc (lgl, other);
	  lgldecocc (lgl, other2);
	}
	res++;
      }
      val = lglval (lgl, other);
      val2 = lglval (lgl, other2);
      if (!val && !val2) {
	LOG (2,
  "reducing flushed %s ternary clause %d %d %d to binary %s clause %d %d",
	     lglred2str (red),
	     lit, other, other2,
	     lglred2str (red),
	     other, other2);
	delta = lglwchbin (lgl, other, other2, red);
	delta += lglwchbin (lgl, other2, other, red);
	if (delta) FIXPTRS ();
	if (red) {
	  lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
	} else {
	  lglincirr (lgl, 2);
	  if (lgl->dense) lglincocc (lgl, other), lglincocc (lgl, other2);
	}
      } else {
#ifndef NDEBUG
	if (!red || (!lgliselim (lgl, other) && !lgliselim (lgl, other2)))
	  assert (val > 0 || val2 > 0);
#endif
      }
    } else {
      assert (tag == OCCS || tag == LRGCS);
      lidx = (tag == LRGCS) ? *++p : (blit >> RMSHFT);
      c = lglidx2lits (lgl, tag, red, lidx);
      if (c[0] >= NOTALIT) continue;
      size = satisfied = 0;
      for (l = c; (other = *l); l++) {
	if (other == lit) continue;
	if ((val = lglval (lgl, other)) < 0) continue;
	if (val > 0) { satisfied = 1; break; }
	if (size < 3) d[size] = other;
	size++;
      }
      if (!satisfied && size == 2) {
	LOGCLS (2, c,
	  "reducing to binary %s clause %d %d flushed large %s clause",
	     lglred2str (red), d[0], d[1], lglred2str (red));
	delta = lglwchbin (lgl, d[0], d[1], red);
	delta += lglwchbin (lgl, d[1], d[0], red);
	if (delta) FIXPTRS ();
	if (red) {
	  lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
	} else {
	  lglincirr (lgl, 2);
	  if (lgl->dense) lglincocc (lgl, d[0]), lglincocc (lgl, d[1]);
	}
      }
      if (!satisfied && size == 3) {
	LOGCLS (2, c,
	  "reducing to ternary %s clause %d %d %d flushed large %s clause",
	     lglred2str (red), d[0], d[1], d[2], lglred2str (red));
	delta = lglwchtrn (lgl, d[0], d[1], d[2], red);
	delta += lglwchtrn (lgl, d[1], d[0], d[2], red);
	delta += lglwchtrn (lgl, d[2], d[0], d[1], red);
	if (delta) FIXPTRS ();
	if (red) {
	  lgl->stats->red.trn++, assert (lgl->stats->red.trn > 0);
	} else {
	  lglincirr (lgl, 3);
	  if (lgl->dense) {
	    lglincocc (lgl, d[0]);
	    lglincocc (lgl, d[1]);
	    lglincocc (lgl, d[2]);
	  }
	}
      }
      if (lgl->dense && !red) {
	for (l = c; (other = *l); l++) {
	  if (satisfied || size <= 3 || lglval (lgl, other) < 0) {
	    if (!red) lgldecocc (lgl, other);
	    if (other != lit) {
	      slidx = lidx;
	      if (red && tag == LRGCS) slidx >>= GLUESHFT;
	      lglrmlocc (lgl, other, red, slidx);
	    }
	  }
	}
      }
      glidx = lidx;
      if (red && tag == OCCS) glidx <<= GLUESHFT;
      if (c[0] != lit) lglrmlwch (lgl, c[0], red, glidx);
      if (c[1] != lit) lglrmlwch (lgl, c[1], red, glidx);
      if (satisfied || size <= 3) {
	if (red) { LGLCHKACT (c[-1]); c[-1] = REMOVED; }
	for (k = c; (other = *k); k++) *k = REMOVED;
	*k = REMOVED;
	if (red) {
	  glue = (tag == LRGCS) ? (lidx & GLUEMASK) : 0;
	  assert (lgl->stats->lir[glue].clauses > 0);
	  lgl->stats->lir[glue].clauses--;
	  assert (lgl->stats->red.lrg > 0);
	  lgl->stats->red.lrg--;
	} else lgldecirr (lgl, k - c);
      } else {
	for (l = k = c; (other = *l); l++) {
	  if ((val = lglval (lgl, other)) < 0) continue;
	  assert (abs (other) != abs (lit));
	  assert (!val);
	  *k++ = other;
	}
	assert (size == k - c);
	if (!red && k < l) {
	  assert (lgl->stats->irr.lits.cur >= l - k);
	  lgl->stats->irr.lits.cur -= l - k;
	}
	*k++ = 0;
	while (k <= l) *k++ = REMOVED;
	delta = lglwchlrg (lgl, c[0], c[1], red, glidx);
	delta += lglwchlrg (lgl, c[1], c[0], red, glidx);
	if (delta) FIXPTRS ();
      }
    }
  }
  hts = lglhts (lgl, lit);
  lglshrinkhts (lgl, hts, 0);
  LOG (2, "flushed %d occurrences of literal %d including %d irredundant",
       count, lit, res);
  lglchkirrstats (lgl);
  return res;
}

static int lglflush (LGL * lgl) {
  int lit, count;
  if (lgl->mt) return 0;
  lglchkirrstats (lgl);
  assert (!lgl->level);
  assert (lgl->probing || lgl->lkhd || lgl->dense);
  if (lgl->flushed == lglcntstk (&lgl->trail)) return 1;
  if (!lglbcp (lgl)) { lgl->mt = 1; return 0; }
  if (!lglsyncunits (lgl)) { assert (lgl->mt); return 0; }
  count = 0;
  while  (lgl->flushed < lglcntstk (&lgl->trail)) {
    lit = lglpeek (&lgl->trail, lgl->flushed++);
    lglflushclauses (lgl, lit);
    lglflushlits (lgl, -lit);
    count++;
  }
  LOG (2, "flushed %d literals", count);
  assert (!lgl->mt);
  return 1;
}

static void lglblockinglit (LGL * lgl, int ilit) {
  int elit = lglexport (lgl, ilit), sgnbit = (1 << (elit < 0));
  Ext * ext = lglelit2ext (lgl, elit);
  assert (!ext->equiv);
  assert (!ext->eliminated);
  assert (abs (ext->repr) == abs (ilit));
  if (ext->blocking & sgnbit) return;
  ext->blocking |= sgnbit;
  LOG (3, "marking external %d internal %d as blocking", elit, ilit);
  lgl->stats->blk.lits++;
}

static void lglelmfrelit (LGL * lgl, int mpivot,
			  int * sop, int * eop, int * son, int * eon) {
  int ipivot = mpivot * lgl->elm->pivot, clidx, ilit, tmp, cover, maxcover;
  int * c, * d, * p, * q, lit, nontrivial, idx, sgn, clen, reslen;
  assert (mpivot == 1 || mpivot == -1);
  assert (ipivot);
  LOG (3,
       "blocked clause elimination and forced resolution of clauses with %d",
	ipivot);
  clidx = 0;
  cover = lglpeek (&lgl->elm->noccs, lglulit (-mpivot));
  for (c = sop; c < eop; c = p + 1) {
    if (lgl->eliminating) lgl->stats->elm.steps++;
    if (*c == REMOVED) { for (p = c + 1; *p; p++) ; continue; }
    maxcover = 0;
    for (p = c; (lit = *p); p++) {
      if (lit == mpivot) continue;
      assert (lit != -mpivot);
      maxcover += lglpeek (&lgl->elm->noccs, lglulit (-lit));
    }
    if (maxcover < cover - 1) { clidx++; continue; }
    for (p = c; (lit = *p); p++) {
      if (lit == mpivot) continue;
      assert (lit != -mpivot);
      idx = abs (lit);
      assert (!lglpeek (&lgl->elm->mark, idx));
      sgn = lglsgn (lit);
      lglpoke (&lgl->elm->mark, idx, sgn);
    }
    nontrivial = 0;
    clen = p - c;
    for (d = son; !nontrivial && d < eon; d = q + 1) {
      lgl->stats->elm.steps++;
      if (*d == REMOVED) { for (q = d + 1; *q; q++) ; continue; }
      lgl->stats->elm.resolutions++;
      LOGMCLS (3, c, "trying forced resolution 1st antecedent");
      LOGMCLS (3, d, "trying forced resolution 2nd antecedent");
      assert (clen > 0);
      reslen = clen - 1;
      for (q = d; (lit = *q); q++) {
	if (lit == -mpivot) continue;
	assert (lit != mpivot);
	idx = abs (lit), sgn = lglsgn (lit);
	tmp = lglpeek (&lgl->elm->mark, idx);
	if (tmp == -sgn) break;
	if (tmp != sgn) reslen++;
      }
      if (lit) {
	while (*++q) ;
	LOG (3, "trying forced resolution ends with trivial resolvent");
      } else {
	LOG (3, "non trivial resolvent in blocked clause elimination");
	nontrivial = INT_MAX;
      }
    }
    for (p = c; (lit = *p); p++) {
      if (lit == mpivot) continue;
      assert (lit != -mpivot);
      idx = abs (lit);
      assert (lglpeek (&lgl->elm->mark, idx) == lglsgn (lit));
      lglpoke (&lgl->elm->mark, idx, 0);
    }
    assert (lgl->opts->elim.val);
    if (lgl->opts->block.val && lgl->opts->elmblk.val && !nontrivial) {
      assert (maxcover >= cover);
      lgl->stats->elm.blkd++;
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
      lglblockinglit (lgl, ipivot);
      continue;
    }
    clidx++;
    if (lgl->limits->elm.steps <= lgl->stats->elm.steps) {
      LOG (2, "maximum number of steps in elimination exhausted");
      return;
    }
  }
}

static void lglelmfre (LGL * lgl) {
  int * sop, * eop, * son, * eon;
  assert (lgl->elm->pivot);
  sop = lgl->elm->lits.start + 1;
  eop = son = lgl->elm->lits.start + lgl->elm->neglidx;
  eon = lgl->elm->lits.top;
  lglelmfrelit (lgl, 1, sop, eop, son, eon);
  lglelmfrelit (lgl, -1, son, eon, sop, eop);
}

static void lgleliminated (LGL * lgl, int pivot) {
  AVar * av;
  int elit;
  Ext * e;
  assert (!lglifrozen (lgl, pivot));
  av = lglavar (lgl, pivot);
  assert (av->type == FREEVAR);
  av->type = ELIMVAR;
  lgl->stats->elm.elmd++;
  assert (lgl->stats->elm.elmd > 0);
  lglflushclauses (lgl, pivot);
  lglflushclauses (lgl, -pivot);
  LOG (2, "eliminated %d", pivot);
  elit = lglexport (lgl, pivot);
  e = lglelit2ext (lgl, elit);
  assert (!e->eliminated);
  assert (!e->equiv);
  e->eliminated = 1;
}

static void lglepusheliminated (LGL * lgl, int idx) {
  const int * p, * w, * eow, * c, * l;
  int lit, blit, tag, red, other;
  HTS * hts;
  lit = (lglocc (lgl, idx) < lglocc (lgl, -idx)) ? idx : -idx;
  LOG (3, "keeping clauses with %d for extending assignment", lit);
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    red = blit & REDCS;
    if (red) continue;
    lglepush (lgl, 0);
    lglepush (lgl, lit);
    if (tag == BINCS || tag == TRNCS) {
      lglepush (lgl, blit >> RMSHFT);
      if (tag == TRNCS)
	lglepush (lgl, *p);
    } else {
      assert (tag == OCCS);
      c = lglidx2lits (lgl, OCCS, 0, blit >> RMSHFT);
      for (l = c; (other = *l); l++)
	if (other != lit)
	  lglepush (lgl, other);
    }
  }
  lglepush (lgl, 0);
  lglepush (lgl, -lit);
  lgleliminated (lgl, idx);
}

static int lglunhimpl (const DFPR * dfpr, int a, int b) {
  int u = lglulit (a), v = lglulit (b), c, d, f, g;
  c = dfpr[u].discovered; if (!c) return 0;
  d = dfpr[v].discovered; if (!d) return 0;
  f = dfpr[u].finished, g = dfpr[v].finished;
  assert (0 < c && c < f);
  assert (0 < d && d < g);
  return c < d && g < f;
}

static int lglunhimplies2 (const DFPR * dfpr, int a, int b) {
  return lglunhimpl (dfpr, a, b) || lglunhimpl (dfpr, -b, -a);
}

static int lglunhimplincl (const DFPR * dfpr, int a, int b) {
  int u = lglulit (a), v = lglulit (b), c, d, f, g;
  c = dfpr[u].discovered; if (!c) return 0;
  d = dfpr[v].discovered; if (!d) return 0;
  f = dfpr[u].finished, g = dfpr[v].finished;
  assert (0 < c && c < f);
  assert (0 < d && d < g);
  return c <= d && g <= f;
}

static int lglunhimplies2incl (const DFPR * dfpr, int a, int b) {
  return lglunhimplincl (dfpr, a, b) || lglunhimplincl (dfpr, -b, -a);
}

static int lglhastrn (LGL * lgl, int a, int b, int c) {
  int blit, tag, other, other2;
  const int * w, * eow, * p;
  HTS * ha, * hb, * hc;
  ha = lglhts (lgl, a);
  hb = lglhts (lgl, b);
  if (hb->count < ha->count) { SWAP (int, a, b); SWAP (HTS *, ha, hb); }
  hc = lglhts (lgl, c);
  if (hc->count < ha->count) { SWAP (int, a, c); SWAP (HTS *, ha, hc); }
  w = lglhts2wchs (lgl, ha);
  eow = w + ha->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == OCCS) continue;
    if (tag == BINCS) {
      other = blit >> RMSHFT;
      if (other == b || other == c) return 1;
      continue;
    }
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    assert (tag == TRNCS);
    other = blit >> RMSHFT;
    if (other != b && other != c) continue;
    other2 = *p;
    if (other2 == b || other2 == c) return 1;
  }
  if (hc->count < hb->count) { SWAP (int, b, c); SWAP (HTS *, hb, hc); }
  w = lglhts2wchs (lgl, hb);
  eow = w + hb->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == OCCS) continue;
    if (tag == TRNCS || tag == LRGCS) { p++; continue; }
    assert (tag == BINCS);
    other = blit >> RMSHFT;
    if (other == c) return 1;
  }
  return 0;
}

static int lgltrylargeve (LGL * lgl) {
  const int * c, * d, * sop, * eop, * son, * eon, * p, * q, * start, * end;
  int lit, idx, sgn, tmp, ip, mp, ilit, npocc, nnocc, limit, count, i;
  int clen, dlen, reslen, maxreslen;
  Val val;
  ip = lgl->elm->pivot;
  assert (ip);
  sop = lgl->elm->lits.start + 1;
  eop = son = lgl->elm->lits.start + lgl->elm->neglidx;
  eon = lgl->elm->lits.top;
  npocc = lglpeek (&lgl->elm->noccs, lglulit (1));
  nnocc = lglpeek (&lgl->elm->noccs, lglulit (-1));
  limit = npocc + nnocc;
  count = 0;
  for (i = 0; i <= 1; i++) {
    start = i ? son : sop;
    end = i ? eon : eop;
    for (c = start; c < end; c++) {
      lgl->stats->elm.steps++;
      if (*c == REMOVED) { while (*c) c++; continue; }
      while ((lit = *c)) {
	(void) lglm2i (lgl, lit);
	c++;
      }
      count++;
    }
  }
  assert (count == limit);
  LOG (3, "trying clause distribution for %d with limit %d", ip, limit);
  maxreslen = 0;
  for (c = sop; c < eop && limit >= 0; c = p + 1) {
    lgl->stats->elm.steps++;
    if (*c == REMOVED) { for (p = c + 1; *p; p++) ; continue; }
    assert (lglmtstk (&lgl->resolvent));
    clen = 0;
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = abs (lit);
      assert (!lglpeek (&lgl->elm->mark, idx));
      sgn = lglsgn (lit);
      lglpoke (&lgl->elm->mark, idx, sgn);
      ilit = lglm2i (lgl, lit);
      lglpushstk (lgl, &lgl->resolvent, ilit);
      clen++;
    }
    for (d = son; limit >= 0 && d < eon; d = q + 1) {
      lgl->stats->elm.steps++;
      if (*d == REMOVED) { for (q = d + 1; *q; q++) ; continue; }
      lgl->stats->elm.resolutions++;
      LOGMCLS (3, c, "trying resolution 1st antecedent");
      LOGMCLS (3, d, "trying resolution 2nd antecedent");
      dlen = 0;
      reslen = clen;
      for (q = d; (lit = *q); q++) {
	if (lit == -1) continue;
	dlen++;
	assert (lit != 1);
	idx = abs (lit), sgn = lglsgn (lit);
	tmp = lglpeek (&lgl->elm->mark, idx);
	if (tmp == -sgn) break;
	if (tmp == sgn) continue;
	ilit = lglm2i (lgl, lit);
	lglpushstk (lgl, &lgl->resolvent, ilit);
	reslen++;
      }
      assert (reslen == lglcntstk (&lgl->resolvent));
      if (!lit && reslen == 1) {
	LOG (3, "trying resolution ends with unit clause");
	lit = lglpeek (&lgl->resolvent, 0);
	limit += lglevar (lgl, lit)->occ[lit < 0];
      } else if (lit) {
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
      lglrststk (&lgl->resolvent, clen);
    }
    lglclnstk (&lgl->resolvent);
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = abs (lit);
      assert (lglpeek (&lgl->elm->mark, idx) == lglsgn (lit));
      lglpoke (&lgl->elm->mark, idx, 0);
    }
    if (lgl->limits->elm.steps <= lgl->stats->elm.steps) {
      LOG (2, "maximum number of steps in elmination exhausted");
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
  LOG (2, "variable elimination of %d", lgl->elm->pivot);
  lglflushclauses (lgl, ip);
  lglflushclauses (lgl, -ip);
  if (npocc < nnocc) start = sop, end = eop, mp = 1;
  else start = son, end = eon, ip = -ip, mp = -1;
  LOG (3, "will save clauses with %d for extending assignment", ip);
  for (c = start; c < end; c = p + 1) {
    lgl->stats->elm.steps++;
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
    lgl->stats->elm.steps++;
    if (*c == REMOVED) { for (p = c + 1; *p; p++) ; continue; }
    assert (lglmtstk (&lgl->resolvent));
    clen = 0;
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = abs (lit);
      assert (!lglpeek (&lgl->elm->mark, idx));
      sgn = lglsgn (lit);
      lglpoke (&lgl->elm->mark, idx, sgn);
      ilit = lglm2i (lgl, lit);
      lglpushstk (lgl, &lgl->resolvent, ilit);
      clen++;
    }
    for (d = son; limit >= 0 && d < eon; d = q + 1) {
      lgl->stats->elm.steps++;
      if (*d == REMOVED) { for (q = d + 1; *q; q++) ; continue; }
      lgl->stats->elm.resolutions++;
      assert (lglmtstk (&lgl->clause));
      dlen = 0;
      reslen = clen;
      for (q = d; (lit = *q); q++) {
	if (lit == -1) continue;
	dlen++;
	assert (lit != 1);
	idx = abs (lit), sgn = lglsgn (lit);
	tmp = lglpeek (&lgl->elm->mark, idx);
	if (tmp == sgn) continue;
	if (tmp == -sgn) break;
	ilit = lglm2i (lgl, lit);
	val = lglval (lgl, ilit);
	if (val < 0) continue;
	if (val > 0) break;
	lglpushstk (lgl, &lgl->clause, ilit);
	ilit = lglm2i (lgl, lit);
	lglpushstk (lgl, &lgl->resolvent, ilit);
	reslen++;
      }
      assert (reslen == lglcntstk (&lgl->resolvent));
      if (!lit && reslen == 1) {
	goto RESOLVE;
      } if (lit) {
	while (*++q) ;
      } else {
RESOLVE:
	LOGMCLS (3, c, "resolving variable elimination 1st antecedent");
	LOGMCLS (3, d, "resolving variable elimination 2nd antecedent");
	for (p = c; (lit = *p); p++) {
	  if (lit == 1) continue;
	  assert (lit != -1);
	  ilit = lglm2i (lgl, lit);
	  val = lglval (lgl, ilit);
	  if (val < 0) continue;
	  if (val > 0) break;
	  lglpushstk (lgl, &lgl->clause, ilit);
	}
	if (!lit) {
	  lglpushstk (lgl, &lgl->clause, 0);
	  LOGCLS (3, lgl->clause.start, "variable elimination resolvent");
	  lgladdcls (lgl, 0, 0, 1);
	}
      }
      lglclnstk (&lgl->clause);
      assert (!*q);
      lglrststk (&lgl->resolvent, clen);
    }
    lglclnstk (&lgl->resolvent);
    for (p = c; (lit = *p); p++) {
      if (lit == 1) continue;
      assert (lit != -1);
      idx = abs (lit);
      assert (lglpeek (&lgl->elm->mark, idx) == lglsgn (lit));
      lglpoke (&lgl->elm->mark, idx, 0);
    }
  }
  lgleliminated (lgl, lgl->elm->pivot);
  lgl->stats->elm.large++;
  return 1;
}

static void lglelimlitaux (LGL * lgl, int idx) {
  lglelmsub (lgl);
  if (lglelmstr (lgl)) return;
  lglelmfre (lgl);
  lgltrylargeve (lgl);
}

static int lgls2m (LGL * lgl, int ilit) {
  AVar * av = lglavar (lgl, ilit);
  int res = av->mark;
  if (!res) {
    res = lglcntstk (&lgl->seen) + 1;
    if (res > lgl->opts->smallvevars.val + 1) return 0;
    av->mark = res;
    assert (lglcntstk (&lgl->seen) == lglcntstk (&lgl->elm->m2i) - 1);
    lglpushstk (lgl, &lgl->seen, abs (ilit));
    lglpushstk (lgl, &lgl->elm->m2i, abs (ilit));
    LOG (4, "mapped internal variable %d to marked variable %d",
	 abs (ilit), res);
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
  i = q;
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
  int midx = abs (mlit), sidx = midx - 2;
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
#ifndef NDEBUG
  mlit =
#endif
  lgls2m (lgl, lit);
  assert (abs (mlit) == 1);
  hts = lglhts (lgl, lit);
  lgltruefun (res);
  if (!hts->count) goto DONE;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    lgl->stats->elm.steps++;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
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
      assert (tag == OCCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, OCCS, 0, lidx);
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
    lgl->stats->elm.steps++;
    lgl->stats->elm.copies++;
  }
DONE:
  return 1;
}

static void lglresetsmallve (LGL * lgl) {
  lglclnstk (&lgl->elm->m2i);
  lglclnstk (&lgl->elm->clv);
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
    cls = lglpeek (&lgl->elm->clv, p + i);
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

static int lglsmallfundeps0 (const Fun f) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (((f[i] & 0xaaaaaaaaaaaaaaaaull)>>1) !=
         (f[i] & 0x5555555555555555ull)) return 1;
  return 0;
}

static int lglsmallfundeps1 (const Fun f) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (((f[i] & 0xccccccccccccccccull)>>2) !=
         (f[i] & 0x3333333333333333ull)) return 1;
  return 0;
}

static int lglsmallfundeps2 (const Fun f) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (((f[i] & 0xf0f0f0f0f0f0f0f0ull)>>4) !=
         (f[i] & 0x0f0f0f0f0f0f0f0full)) return 1;
  return 0;
}

static int lglsmallfundeps3 (const Fun f) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (((f[i] & 0xff00ff00ff00ff00ull)>>8) !=
         (f[i] & 0x00ff00ff00ff00ffull)) return 1;
  return 0;
}

static int lglsmallfundeps4 (const Fun f) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (((f[i] & 0xffff0000ffff0000ull)>>16) !=
         (f[i] & 0x0000ffff0000ffffull)) return 1;
  return 0;
}

static int lglsmallfundeps5 (const Fun f) {
  int i;
  for (i = 0; i < FUNQUADS; i++)
    if (((f[i] & 0xffffffff00000000ull)>>32) !=
         (f[i] & 0x00000000ffffffffull)) return 1;
  return 0;
}

static int lglsmallfundepsgen (const Fun f, int min) {
  const int c = (1 << (min - 6));
  int i, j;
  assert (min >= 6);
  for (i = 0; i < FUNQUADS; i += (1 << (min - 5)))
    for (j = 0; j < c; j++)
      if (f[i + j] != f[i + c + j]) return 1;
  return 0;
}

static int lglsmalltopvar (const Fun f, int min) {
  int i;
  switch (min) {
    case 0: if (lglsmallfundeps0 (f)) return 0;
    case 1: if (lglsmallfundeps1 (f)) return 1;
    case 2: if (lglsmallfundeps2 (f)) return 2;
    case 3: if (lglsmallfundeps3 (f)) return 3;
    case 4: if (lglsmallfundeps4 (f)) return 4;
    case 5: if (lglsmallfundeps5 (f)) return 5;
  }
  for (i = lglmax (6, min); i <= FUNVAR - 2; i++)
    if (lglsmallfundepsgen (f, i)) return i;
  return i;
}

static Cnf lglsmalladdlit2cnf (LGL * lgl, Cnf cnf, int lit) {
  int p, m, q, n, i, cls;
  Cnf res;
  p = lglcnf2pos (cnf);
  m = lglcnf2size (cnf);
  q = lglcntstk (&lgl->elm->clv);
  for (i = 0; i < m; i++) {
    cls = lglpeek (&lgl->elm->clv, p + i);
    assert (!(cls & lit));
    cls |= lit;
    lglpushstk (lgl, &lgl->elm->clv, cls);
  }
  n = lglcntstk (&lgl->elm->clv) - q;
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

// The dual version of Minato's algorithm for computing an irredundant
// product of sums.   The original algorithm computes an irredundant sum of
// products. It uses BDDs for representing boolean functions and ZDDs for
// manipulating sum of product.  We work with function tables stored as bit
// maps and plain CNFs stored as lists of clauses instead.

static Cnf lglsmallipos (LGL * lgl, const Fun U, const Fun L, int min) {
  Fun U0, U1, L0, L1, Unew, ftmp;
  Cnf c0, c1, cstar, ctmp, res;
  int x, y, z;
  assert (lglefun (L, U));
  if (lglistruefun (U)) return TRUECNF;
  if (lglisfalsefun (L)) return FALSECNF;
  lgl->stats->elm.ipos++;
  assert (min < lglcntstk (&lgl->elm->m2i));
  y = lglsmalltopvar (U, min);
  z = lglsmalltopvar (L, min);
  lgl->stats->elm.steps += FUNVAR;
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
  res |= lglsize2cnf (lglcntstk (&lgl->elm->clv) - res);
  return res;
}

static void lglsmallve (LGL * lgl, Cnf cnf) {
  int * soc = lgl->elm->clv.start + lglcnf2pos (cnf);
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
      lgl->stats->elm.steps++;
      lgl->stats->elm.resolutions++;
      lglpushstk (lgl, &lgl->clause, 0);
      LOGCLS (3, lgl->clause.start, "small elimination resolvent");
#ifndef NLGLPICOSAT
      lglpicosatchkcls (lgl);
#endif
      lgladdcls (lgl, 0, 0, 1);
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
    cls = lglpeek (&lgl->elm->clv, p + i);
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
  assert (lglmtstk (&lgl->elm->m2i));
  assert (lglmtstk (&lgl->seen));
  assert (lglmtstk (&lgl->elm->clv));
  lglpushstk (lgl, &lgl->elm->m2i, 0);
  lglpushstk (lgl, &lgl->elm->clv, 0);
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
    lgl->stats->elm.small.tried++;
    if (new <= old) {
      LOG (2, "small elimination of %d removes %d clauses", idx, old - new);
      lglepusheliminated (lgl, idx);
      lglflushclauses (lgl, idx);
      lglflushclauses (lgl, -idx);
      lglsmallve (lgl, cnf);
      lgl->stats->elm.small.elm++;
      res = 1;
    } else {
      LOG (2, "small elimination of %d would add %d clauses", idx, new - old);
      if (units > 0) res = 1;
      else lgl->stats->elm.small.failed++;
    }
  } else LOG (2, "too many variables for small elimination");
  lglresetsmallve (lgl);
  return res;
}

static int lgl2manyoccs4elm (LGL * lgl, int lit) {
  return lglocc (lgl, lit) > lgl->opts->elmocclim.val;
}

static int lglchkoccs4elmlit (LGL * lgl, int lit) {
  int blit, tag, red, other, other2, lidx, size;
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
    if (red || tag == LRGCS) continue;
    if (tag == BINCS || tag == TRNCS) {
      other = blit >> RMSHFT;
      if (lgl2manyoccs4elm (lgl, other)) return 0;
      if (tag == TRNCS) {
        other2 = *p;
        if (lgl2manyoccs4elm (lgl, other2)) return 0;
      }
    } else {
      assert (tag == OCCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, OCCS, 0, lidx);
      size = 0;
      for (l = c; (other = *l); l++) {
        if (lgl2manyoccs4elm (lgl, other)) return 0;
        if (++size > lgl->opts->elmclslim.val) return 0;
      }
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

static void lglelimlit (LGL * lgl, int idx) {
  if (!lglisfree (lgl, idx)) return;
  if (!lglchkoccs4elm (lgl, idx)) return;
  LOG (2, "trying to eliminate %d", idx);
  if (lgl->opts->smallve.val && lgltrysmallve (lgl, idx)) return;
  lglinitecls (lgl, idx);
  lglelimlitaux (lgl, idx);
  if (lgl->elm->pivot) lglrstecls (lgl);
}

static int lglblockcls (LGL * lgl, int lit) {
  int blit, tag, red, other, other2, lidx, val, count, size;
  const int * p, * w, * eow, * c, *l;
  HTS * hts;
  lgl->stats->blk.steps++;
  hts = lglhts (lgl, lit);
  if (!hts->count) return 1;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  count = 0;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    red = blit & REDCS;
    if (red) continue;
    count++;
    lgl->stats->blk.res++;
    lgl->stats->blk.steps++;
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
      assert (tag == OCCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, OCCS, 0, lidx);
      size = 0;
      for (l = c; (other = *l); l++) {
	val = lglmarked (lgl, other);
	if (++size > lgl->opts->blkclslim.val) return 0;
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

static int lglpurelit (LGL * lgl, int lit) {
  int res;
  LOG(1, "pure literal %d", lit);
  lgl->stats->blk.pure++;
  assert (!lglocc (lgl, -lit));
  res = lglflushclauses (lgl, lit);
  lgl->stats->blk.clauses += res;
  if (lgl->blocking) lgl->stats->blk.steps += res;
  lglepusheliminated (lgl, lit);
  return res;
}

static int lgl2manyoccs4blk (LGL * lgl, int lit) {
  return lglhts (lgl, lit)->count > lgl->opts->blkocclim.val;
}

static void lglmvbcls (LGL * lgl, int a, int b) {
  assert (abs (a) != abs (b));
  assert (!lglval (lgl, a));
  assert (!lglval (lgl, b));
  assert (lglmtstk (&lgl->clause)); 
  lglpushstk (lgl, &lgl->clause, a);
  lglpushstk (lgl, &lgl->clause, b);
  lglpushstk (lgl, &lgl->clause, 0);
  if (!lglsimpleprobeclausexists (lgl)) {
    LOG (2, "moving redundant binary clause %d %d", a, b);
#ifndef NLGLPICOSAT
    lglpicosatchkcls (lgl);
#endif
    lgladdcls (lgl, REDCS, 0, 1);
  }
  lglclnstk (&lgl->clause);
  lgl->stats->moved.bin++;
}

static void lglmvtcls (LGL * lgl, int a, int b, int c) {
  assert (abs (a) != abs (b));
  assert (abs (a) != abs (c));
  assert (abs (b) != abs (c));
  assert (!lglval (lgl, a));
  assert (!lglval (lgl, b));
  assert (!lglval (lgl, c));
  assert (lglmtstk (&lgl->clause)); 
  lglpushstk (lgl, &lgl->clause, a);
  lglpushstk (lgl, &lgl->clause, b);
  lglpushstk (lgl, &lgl->clause, c);
  lglpushstk (lgl, &lgl->clause, 0);
  if (!lglsimpleprobeclausexists (lgl)) {
    LOG (2, "moving redundant ternary clause %d %d %d", a, b, c);
#ifndef NLGLPICOSAT
    lglpicosatchkcls (lgl);
#endif
    lgladdcls (lgl, REDCS, 0, 1);
  }
  lglclnstk (&lgl->clause);
  lgl->stats->moved.trn++;
}

static int lglblocklit (LGL * lgl, int lit, Stk * stk) {
  int blit, tag, red, blocked, other, other2, lidx, count, size;
  int * p, * w, * eow, * c, * l;
  HTS * hts;
  if (lglval (lgl, lit)) return 0;
  if (lgl2manyoccs4blk (lgl, lit)) return 0;
  hts = lglhts (lgl, lit);
  assert (hts->count > 0);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  count = 0;
  assert (lglmtstk (stk+2) && lglmtstk (stk+3) && lglmtstk (stk+4));
  for (p = w; p < eow; p++) {
    if (lgl->stats->blk.steps++ >= lgl->limits->blk.steps) break;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    red = blit & REDCS;
    if (red) continue;
    assert (lglmtstk (&lgl->seen));
    blocked = 0;
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
      assert (tag == OCCS);
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, OCCS, 0, lidx);
      size = 0;
      for (l = c; (other = *l); l++) {
	if (other == lit) continue;
        if (lgl2manyoccs4blk (lgl, other)) goto CONTINUE;
	if (++size > lgl->opts->blkclslim.val) goto CONTINUE;
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
      assert (tag == OCCS);
      lidx = blit >> RMSHFT;
      lglpushstk (lgl, stk+4, lidx);
    }
  }
  while (!lglmtstk (stk+2)) {
    if (lgl->stats->blk.steps++ >= lgl->limits->blk.steps) break;
    count++;
    other = lglpopstk (stk+2);
    LOG (2, "blocked binary clause %d %d on %d", lit, other, lit);
    lglrmbcls (lgl, lit, other, 0);
    lglepush (lgl, 0);
    lglepush (lgl, lit);
    lglepush (lgl, other);
    if (lgl->opts->move.val) lglmvbcls (lgl, lit, other);
  }
  while (!lglmtstk (stk+3)) {
    if (lgl->stats->blk.steps++ >= lgl->limits->blk.steps) break;
    count++;
    other2 = lglpopstk (stk+3);
    other = lglpopstk (stk+3);
    LOG (2, "blocked ternary clause %d %d %d on %d", lit, other, other2, lit);
    lglrmtcls (lgl, lit, other, other2, 0);
    lglepush (lgl, 0);
    lglepush (lgl, lit);
    lglepush (lgl, other);
    lglepush (lgl, other2);
    if (lgl->opts->move.val >= 2) lglmvtcls (lgl, lit, other, other2);
  }
  while (!lglmtstk (stk+4)) {
    if (lgl->stats->blk.steps++ >= lgl->limits->blk.steps) break;
    lidx = lglpopstk (stk+4);
    count++;
    c = lglidx2lits (lgl, LRGCS, 0, lidx);
    LOGCLS (2, c, "blocked on %d large clause", lit);
    lglepush (lgl, 0);
    lglepush (lgl, lit);
    for (l = c; (other = *l); l++)
      if (other != lit) lglepush (lgl, other);
    lglrmlcls (lgl, lidx, 0);
  }
  LOG (2, "found %d blocked clauses with %d", count, lit);
  lgl->stats->blk.clauses += count;
  if (count > 0) lglblockinglit (lgl, lit);
  lglclnstk (stk+2), lglclnstk (stk+3), lglclnstk (stk+4);
  return count;
}

static void lglsetblklim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  if (lgl->opts->blkrtc.val) {
    lgl->limits->blk.steps = LLMAX;
    lglprt (lgl, 1, "[block-%d] no limit", lgl->stats->elm.count);
  } else {
    limit = (lgl->opts->blkreleff.val*lgl->stats->visits.search)/1000;
    if (limit < lgl->opts->blkmineff.val) limit = lgl->opts->blkmineff.val;
    if (lgl->opts->blkmaxeff.val >= 0 && limit > lgl->opts->blkmaxeff.val)
      limit = lgl->opts->blkmaxeff.val;
    limit >>= (pen = lgl->limits->blk.pen + lglszpen (lgl));
    irrlim = lgl->stats->irr.lits.cur;
    irrlim >>= lgl->limits->simp.pen;
    if (limit < irrlim) {
      limit = irrlim;
      lglprt (lgl, 1, 
	"[block-%d] limit of %lld steps based on %d irredundant literals",
	lgl->stats->blk.count, (LGLL) limit, lgl->stats->irr.lits.cur);
    } else
      lglprt (lgl, 1, 
	"[block-%d] limit of %lld steps penalty %d = %d + %d",
	lgl->stats->blk.count, (LGLL) limit,
	pen, lgl->limits->blk.pen, lglszpen (lgl));
    lgl->limits->blk.steps = lgl->stats->blk.steps + limit;
  }
}

static void lglupdblkint (LGL * lgl, int success) {
  if (success && lgl->limits->blk.pen) lgl->limits->blk.pen--;
  if (!success && lgl->limits->blk.pen < MAXPEN) lgl->limits->blk.pen++;
  lgl->limits->blk.irrprgss = lgl->stats->irrprgss;
}

static int lgleschedrem (LGL * lgl, int this_time) {
  int idx, res = 0, count;
  const char * str;
  AVar * av;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (lglifrozen (lgl, idx)) continue;
    if (!lglisfree (lgl, idx)) continue;
    av = lglavar (lgl, idx);
    if (lgl->eliminating && av->donotelm) continue;
    if (lgl->blocking && av->donotblk) continue;
    if (lgl->cceing && av->donotcce) continue;
    res++;
  }
  assert (lgl->eliminating || lgl->blocking || lgl->cceing);
  if (lgl->eliminating) count = lgl->stats->elm.count, str = "elim";
  else if (lgl->blocking) count = lgl->stats->blk.count, str = "block";
  else assert (lgl->cceing), count = lgl->stats->cce.count, str = "cce";
  if (res)
    lglprt (lgl, 1,
      "[%s-%d] %d variables %.0f%% %s time",
      str, count,
      res, lglpcnt (res, lglrem (lgl)),
      this_time ? "will be scheduled this" :
		  "remain to be tried next");
  else {
    lglprt (lgl, 1,
      "[%s-%d] no untried remaining variables left",
      str, count);
    for (idx = 2; idx < lgl->nvars; idx++) {
      av = lglavar (lgl, idx);
      if (lgl->eliminating) av->donotelm = 0;
      if (lgl->blocking) av->donotblk = 0;
      if (lgl->cceing) av->donotcce = 0;
    }
  }
  return res;
}

static void lglsetdonotesched (LGL * lgl, int completed) {
  AVar * av;
  EVar * ev;
  int idx;
  for (idx = 2; idx < lgl->nvars; idx++) {
    av = lglavar (lgl, idx);
    ev = lglevar (lgl, idx);
    if (lgl->eliminating) {
      if (completed) av->donotelm = 0;
      else if (ev->pos < 0) av->donotelm = 1;
    }
    if (lgl->blocking) {
      if (completed) av->donotblk = 0;
      else if (ev->pos < 0) av->donotblk = 1;
    }
    if (lgl->cceing) {
      if (completed) av->donotcce = 0;
      else if (ev->pos < 0) av->donotcce = 1;
    }
  }
}

static void lglblock (LGL * lgl) {
  int oldrem = lgl->blkrem, oldall = lgl->blkall;
  int idx, count, frozen, all, rem;
  Stk blocked[5];
  assert (lglsmallirr (lgl));
  assert (!lgl->simp);
  assert (!lgl->dense);
  assert (!lgl->eliminating);
  assert (!lgl->blocking);
  lglstart (lgl, &lgl->times->blk);
  if (lgl->level) lglbacktrack (lgl, 0);
  lgl->simp = lgl->blocking = 1;
  lgl->stats->blk.count++;
  lglgc (lgl);
  lglfreezer (lgl);
  assert (!(oldall && !oldrem));
  all = !oldrem || !oldall;
  if (all) lglprt (lgl, 1, "[block-%d] scheduling all variables this time",
		   lgl->stats->blk.count);
  else if (!lgleschedrem (lgl, 1)) all = 1, oldrem = 0;
  if (!all) assert (!lgl->donotsched), lgl->donotsched = 1;
  lgldense (lgl, 1);
  if (!all) assert (lgl->donotsched), lgl->donotsched = 0;
  lglsetblklim (lgl);
  CLR (blocked);
  count = 0;
  while (!lglterminate (lgl) &&
	 lgl->stats->blk.steps < lgl->limits->blk.steps &&
	 !lglmtstk (&lgl->esched)) {
    idx = lglpopesched (lgl);
    lglavar (lgl, idx)->donotblk = 1;
    frozen = lglifrozen (lgl, idx);
    if (!lglisfree (lgl, idx)) continue;
    if (!frozen && !lglocc (lgl, idx)) count += lglpurelit (lgl, -idx);
    else if (!frozen && !lglocc (lgl, -idx)) count += lglpurelit (lgl, idx);
    else {
      count += lglblocklit (lgl, idx, blocked);
      count += lglblocklit (lgl, -idx, blocked);
    }
  }
  rem = lglcntstk (&lgl->esched);
  if (!rem)
    lglprt (lgl, 1, "[block-%d] fully completed blocked clause elimination",
	    lgl->stats->blk.count);
  else if (!oldrem)
    lglprt (lgl, 1,
      "[block-%d] incomplete blocked clause elimination %d not tried %.0f%%",
      lgl->stats->blk.count, rem, lglpcnt (rem, lgl->nvars - 2));
  else
    rem = lgleschedrem (lgl, 0);
  lglsetdonotesched (lgl, !rem);
  lglrelstk (lgl, &lgl->esched);
  lglsparse (lgl);
  lglgc (lgl);
  lglrelstk (lgl, blocked+2);
  lglrelstk (lgl, blocked+3);
  lglrelstk (lgl, blocked+4);
  lgl->blkrem = rem > 0;
  lgl->blkall = all && rem;
  lglprt (lgl, 1, "[block-%d] transition to [ all %d rem %d ] state",
	  lgl->stats->blk.count, lgl->blkall, lgl->blkrem);
  assert (lgl->simp && lgl->blocking);
  lgl->blocking = lgl->simp = 0;
  lgl->stats->irrprgss += count;
  lglupdblkint (lgl, count);
  lglprt (lgl, 1, "[block-%d] eliminated %d blocked clauses",
	  lgl->stats->blk.count, count);
  lglrep (lgl, 1 + !count, 'k');
  lglstop (lgl);
  assert (!lgl->mt);
}

static void lglsetccelim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->ccereleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->ccemineff.val) limit = lgl->opts->ccemineff.val;
  if (lgl->opts->ccemaxeff.val >= 0 && limit > lgl->opts->ccemaxeff.val)
    limit = lgl->opts->ccemaxeff.val;
  limit >>= (pen = lgl->limits->cce.pen + lglszpen (lgl));
  irrlim = lgl->stats->irr.lits.cur;
  irrlim >>= lgl->limits->simp.pen;
  if (lgl->opts->block.val && !lgl->blkrem) irrlim *= 2, limit *= 2;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1,
      "[cce-%d] limit of %lld steps based on %d irredundant literals",
      lgl->stats->cce.count, (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[cce-%d] limit of %lld steps penalty %d = %d + %d",
	    lgl->stats->cce.count, (LGLL) limit,
	    pen, lgl->limits->cce.pen, lglszpen (lgl));
  lgl->limits->cce.steps = lgl->stats->cce.steps + limit;
}

static void lglupdcceint (LGL * lgl, int success) {
  if (success && lgl->limits->cce.pen) lgl->limits->cce.pen--;
  if (!success && lgl->limits->cce.pen < MAXPEN) lgl->limits->cce.pen++;
}

static int lglcceing (LGL * lgl) {
  if (!lgl->opts->cce.val) return 0;
  if (!lglsmallirr (lgl)) return 0;
  return 1;
}

#define CCELOGLEVEL 2

static void lglsignedmark2 (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << (2 + (lit < 0));
  if (av->mark & bit) return;
  av->mark |= bit;
}

static void lglsignedunmark2 (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << (2 + (lit < 0));
  if (!(av->mark & bit)) return;
  av->mark &= ~bit;
}

static int lglsignedmarked2 (LGL * lgl, int lit) {
  AVar * av = lglavar (lgl, lit);
  int bit = 1 << (2 + (lit < 0));
  return av->mark & bit;
}

static int lglabcecls (LGL * lgl, int lit, const int * c) {
  int other, found = 0;
  const int * p;
  for (p = c; (other = *p); p++)
    if (other == -lit) found++;
    else if (lglsignedmarked (lgl, -other)) return 1;
  assert (found == 1);
  return 0;
}

static int lglabce (LGL * lgl, int lit) {
  const int * p, * w, * eow, * c;
  int blit, tag, other, cls[4];
  HTS * hts;
  assert (!lglifrozen (lgl, lit));
  hts = lglhts (lgl, -lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  cls[0] = -lit, cls[3] = 0;
  for (p = w; p < eow; p++) {
    if (lgl->limits->cce.steps <= ++lgl->stats->cce.steps) return 0;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (blit & REDCS) continue;
    if (tag == LRGCS) continue;
    other = blit >> RMSHFT;
    cls[1] = other;
    if (tag == BINCS) cls[2] = 0, c = cls;
    else if (tag == TRNCS) cls[2] = *p, c = cls;
    else assert (tag == OCCS), c = lglidx2lits (lgl, OCCS, 0, other);
    if (!lglabcecls (lgl, lit, c)) return 0;
  }
  return 1;
}

static int lglcceclause (LGL * lgl, 
                         const int * c, 
			 const int * ignwch,
			 int igntag) {
  int other, res, nextala, nextcla, lit, blit, tag, other2;
  const int * p, * eow, * w, * d, * q;
  int unit, first, old, prev;
  HTS * hts;
  int * r;
  LOGCLS (CCELOGLEVEL, c, "trying CCE on clause");
  assert (lglmtstk (&lgl->cce->extend));
  assert (lglmtstk (&lgl->cce->cla));
  assert (lglmtstk (&lgl->seen));
  for (p = c; (other = *p); p++) {
    assert (!lglmarked (lgl, other));
    lglpushstk (lgl, &lgl->seen, other);
    lglpushstk (lgl, &lgl->cce->cla, other);
    lglsignedmark (lgl, other);
  }
  nextcla = nextala = res = 0;
ALA:
  while (!res && nextala < lglcntstk (&lgl->seen)) {
    lit = lglpeek (&lgl->seen, nextala++);
    assert (lglsignedmarked (lgl, lit));
    assert (!lglsignedmarked (lgl, -lit));
    hts = lglhts (lgl, lit);
    w = lglhts2wchs (lgl, hts);
    eow = w + hts->count;
    for (p = w; !res && p < eow; p++) {
      if (lgl->limits->cce.steps <= ++lgl->stats->cce.steps) goto DONE;
      blit = *p;
      tag = blit & MASKCS;
      if (tag == TRNCS || tag == LRGCS) p++;
      if (p == ignwch) continue;
      if (tag == LRGCS) continue;
      if (blit & REDCS) continue;
      other = blit >> RMSHFT;
      if (tag == BINCS) {
	if (lglsignedmarked (lgl, -other)) continue;
	else if (lglsignedmarked (lgl, other)) {
	  if (igntag == BINCS) {
	    if (c[0] == lit && c[1] == other) continue;
	    if (c[1] == lit && c[0] == other) continue;
	  }
	  LOG (CCELOGLEVEL, 
	       "ALA on binary clause %d %d results in ATE", lit, other);
	  res = 1;
	} else {
	  assert (!lglmarked (lgl, other));
	  LOG (CCELOGLEVEL, "ALA %d through binary clause %d %d", 
	       -other, lit, other);
	  lglsignedmark (lgl, -other);
	  lglpushstk (lgl, &lgl->seen, -other);
	}
      } else if (tag == TRNCS) {
	if (lglsignedmarked (lgl, -other)) continue;
	other2 = *p;
	if (lglsignedmarked (lgl, -other2)) continue;
	if (lglsignedmarked (lgl, other)) {
	  if (lglsignedmarked (lgl, other2)) {
	    if (igntag == TRNCS) {
	      if (c[0] == lit && c[1] == other && c[2] == other2) continue;
	      if (c[0] == lit && c[2] == other && c[1] == other2) continue;
	      if (c[1] == lit && c[0] == other && c[2] == other2) continue;
	      if (c[1] == lit && c[2] == other && c[0] == other2) continue;
	      if (c[2] == lit && c[0] == other && c[1] == other2) continue;
	      if (c[2] == lit && c[1] == other && c[0] == other2) continue;
	    }
	    LOG (CCELOGLEVEL,
	      "ALA on ternary clause %d %d %d results in ATE",
	      lit, other, other2);
	    res = 1;
	  } else {
	    assert (!lglmarked (lgl, other2));
	    LOG (CCELOGLEVEL, 
	         "ALA %d through ternary clause %d %d %d (1st case)",
		 -other2, lit, other, other2);
	    lglsignedmark (lgl, -other2);
	    lglpushstk (lgl, &lgl->seen, -other2);
	  }
	} else if (lglsignedmarked (lgl, other2)) {
	  assert (!lglmarked (lgl, other));
	  LOG (CCELOGLEVEL, 
	      "ALA %d through ternary clause %d %d %d (2nd case)",
	      -other, lit, other, other2);
	  lglsignedmark (lgl, -other);
	  lglpushstk (lgl, &lgl->seen, -other);
	}
      } else {
	assert (tag == OCCS);
	d = lglidx2lits (lgl, OCCS, 0, other);
	if (d == c) continue;
	unit = 0;
	for (q = d; (other = *q); q++) {
	  if (other == lit) continue;
	  if (lglsignedmarked (lgl, -other)) break;
	  if (lglsignedmarked (lgl, other)) continue;
	  if (unit) break;
	  unit = -other;
	}
	if (other) continue;
	if (!unit) {
	  LOGCLS (CCELOGLEVEL, d, "ATE after ALA on large clause");
	  res = 1;
	} else {
	  assert (!lglmarked (lgl, unit));
	  LOGCLS (CCELOGLEVEL, d, "ALA %d through large clause", unit);
	  lglsignedmark (lgl, unit);
	  lglpushstk (lgl, &lgl->seen, unit);
	}
      }
    }
  }
  if (res || !lgl->opts->block.val || lgl->opts->cce.val < 3) goto SKIPCLA;
  while (!res && nextcla < lglcntstk (&lgl->cce->cla)) {
    lit = lglpeek (&lgl->cce->cla, nextcla++);
    if (lglifrozen (lgl, lit)) continue;
    assert (lglsignedmarked (lgl, lit));
    assert (!lglsignedmarked (lgl, -lit));
    hts = lglhts (lgl, -lit);
    w = lglhts2wchs (lgl, hts);
    eow = w + hts->count;
    old = lglcntstk (&lgl->cce->cla);
    first = 1;
    for (p = w; p < eow; p++) {
      if (lgl->limits->cce.steps <= ++lgl->stats->cce.steps) goto DONE;
      blit = *p;
      tag = blit & MASKCS;
      if (tag == TRNCS || tag == LRGCS) p++;
      assert (p != ignwch);
      if (tag == LRGCS) continue;
      if (blit & REDCS) continue;
      other = blit >> RMSHFT;
      if (first) {
	if (tag == BINCS) {
	  if (lglsignedmarked (lgl, -other)) continue;
	  if (!lglsignedmarked (lgl, other))
	    lglpushstk (lgl, &lgl->cce->cla, other);
	} else if (tag == TRNCS) {
	  if (lglsignedmarked (lgl, -other)) continue;
	  if (lglsignedmarked (lgl, -*p)) continue;
	  if (!lglsignedmarked (lgl, other))
	    lglpushstk (lgl, &lgl->cce->cla, other);
	  if (!lglsignedmarked (lgl, *p))
	    lglpushstk (lgl, &lgl->cce->cla, *p);
	} else { 
	  assert (tag == OCCS);
	  d = lglidx2lits (lgl, OCCS, 0, other);
	  assert (d != c);
	  for (q = d; (other = *q); q++)
	    if (other != -lit && lglsignedmarked (lgl, -other)) break;
	  if (other) continue;
	  for (q = d; (other = *q); q++)
	    if (other != -lit && !lglsignedmarked (lgl, other))
	      lglpushstk (lgl, &lgl->cce->cla, other);
	}
	first = 0;
      } else {
	r = lgl->cce->cla.start + old;
	if (tag == BINCS) {
	  if (lglsignedmarked (lgl, -other)) continue;
	  for (q = r; q < lgl->cce->cla.top; q++)
	    if (*q == other) *r++ = *q;
	} else if (tag == TRNCS) {
	  if (lglsignedmarked (lgl, -other)) continue;
	  if (lglsignedmarked (lgl, -(other2 = *p))) continue;
	  for (q = r; q < lgl->cce->cla.top; q++)
	    if (*q == other || *q == other2) *r++ = *q;
	} else {
	  assert (tag == OCCS);
	  d = lglidx2lits (lgl, OCCS, 0, other);
	  assert (d != c);
	  for (q = d; (other = *q); q++)
	    if (other != -lit && lglsignedmarked (lgl, -other)) break;
	  if (other) continue;
	  for (q = d; (other = *q); q++) {
	    if (other == -lit) continue;
	    assert (other != lit);
	    assert (!lglsignedmarked2 (lgl, other));
	    assert (!lglsignedmarked2 (lgl, -other));
	    lglsignedmark2 (lgl, other);
	  }
	  for (q = r; q < lgl->cce->cla.top; q++)
	    if (lglsignedmarked2 (lgl, (other = *q)))
	      *r++ = other;
	  for (q = d; (other = *q); q++) {
	    if (other == -lit) continue;
	    assert (other != lit);
	    assert (lglsignedmarked2 (lgl, other));
	    lglsignedunmark2 (lgl, other);
	  }
	}
	if ((lgl->cce->cla.top = r) == lgl->cce->cla.start + old) break;
      }
    } 
    if (lglcntstk (&lgl->cce->cla) > old) {
      nextcla = 0;
      lglpushstk (lgl, &lgl->cce->extend, 0);
      lglpushstk (lgl, &lgl->cce->extend, lit);
      for (q = lgl->cce->cla.start; q < lgl->cce->cla.start + old; q++)
	if (*q != lit) lglpushstk (lgl, &lgl->cce->extend, *q);
    }
    for (q = lgl->cce->cla.start + old; !res && q < lgl->cce->cla.top; q++) {
      if (lglsignedmarked (lgl, -*q)) {
	LOG (CCELOGLEVEL, "CLA on %d results in ATE", *q);
	res = 1;
      } else {
	LOG (CCELOGLEVEL, "CLA %d on %d", *q, lit);
	lglpushstk (lgl, &lgl->seen, *q);
	lglsignedmark (lgl, *q);
      }
    }
    if (!res && p == eow && nextala < lglcntstk (&lgl->seen)) goto ALA;
  }
SKIPCLA:
  if (res) {
    LOGCLS (CCELOGLEVEL, c, "ATE clause");
    lgl->stats->cce.ate++;
  } else if (lgl->opts->block.val && lgl->opts->cce.val >= 2) {
   for (p = lgl->cce->cla.start; p < lgl->cce->cla.top; p++)
     if (!lglifrozen (lgl, (other = *p)) && (res = lglabce (lgl, other))) 
       break;
    if (res) {
      LOGCLS (CCELOGLEVEL, c, "ABCE on %d clause", other);
      lglpushstk (lgl, &lgl->cce->extend, 0);
      lglpushstk (lgl, &lgl->cce->extend, other);
      for (p = lgl->cce->cla.start; p < lgl->cce->cla.top; p++)
        if (*p != other) lglpushstk (lgl, &lgl->cce->extend, *p);
      lgl->stats->cce.abce++;
    }
  }
  if (res) lgl->stats->cce.eliminated++;
DONE:
  lglpopnunmarkstk (lgl, &lgl->seen);
  lglclnstk (&lgl->cce->cla);
  if (res && !lglmtstk (&lgl->cce->extend)) {
    assert (lgl->opts->cce.val >= 2);
    assert (lgl->opts->block.val);
    prev = INT_MAX;
    for (p = lgl->cce->extend.start; p < lgl->cce->extend.top; p++) {
      lit = *p;
      lglepush (lgl, lit);
      if (!prev) assert (!lglifrozen (lgl, lit)), lglblockinglit (lgl, lit);
      prev = lit;
    }
  }
  lglclnstk (&lgl->cce->extend);
  return res;
}

static void lglrmvbcls (LGL * lgl, int a, int b) {
  lglrmbcls (lgl, a, b, 0);
  if (lgl->opts->move.val) lglmvbcls (lgl, a, b);
}

static void lglrmvtcls (LGL * lgl, int a, int b, int c) {
  assert (abs (a) != abs (b));
  assert (abs (a) != abs (c));
  assert (abs (b) != abs (c));
  assert (!lglval (lgl, a));
  assert (!lglval (lgl, b));
  assert (!lglval (lgl, c));
  lglrmtcls (lgl, a, b, c, 0);
  if (lgl->opts->move.val >= 2) lglmvtcls (lgl, a, b, c);
}

static void lglccelit (LGL * lgl, int lit) {
  int cls[4], blit, tag, * c, other, lidx;
  const int * p, * w, * eow, * l;
  HTS * hts;
  if (!lglisfree (lgl, lit)) return;
  if (lglrem (lgl) < lgl->cce->rem[abs (lit)]) {
    LOG (CCELOGLEVEL, "probing %d in covered clause elimination", lit);
    lglbasicprobelit (lgl, -lit);
    if (!lglflush (lgl)) return;
    lgl->cce->rem[abs (lit)] = lglrem (lgl);
  }
  LOG (CCELOGLEVEL, "trying to eliminate covered clauses with %d", lit);
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  cls[0] = lit;
  for (p = w; p < eow; p++) {
     if (lgl->limits->cce.steps <= ++lgl->stats->cce.steps) break;
     blit = *p;
     tag = blit & MASKCS;
     if (tag == TRNCS || tag == LRGCS) p++;
     if (tag == LRGCS) continue;
     if (blit & REDCS) continue;
     lidx = other = blit >> RMSHFT;
     c = cls;
     if (tag == BINCS) {
       if (abs (other) < abs (lit)) continue;
       cls[1] = other, cls[2] = 0;
     } else if (tag == TRNCS) {
       if (abs (other) < abs (lit)) continue;
       if (abs (*p) < abs (lit)) continue;
       cls[1] = other, cls[2] = *p, cls[3] = 0;
     } else {
       assert (tag == OCCS);
       c = lglidx2lits (lgl, OCCS, 0, lidx);
       for (l = c; (other = *l); l++)
	 if (abs (other) < abs (lit)) break;
       if (other) continue;
     }
     if (!lglcceclause (lgl, c, p, tag)) continue;
     if (tag == BINCS) lglrmvbcls (lgl, lit, other);
     else if (tag == TRNCS) lglrmvtcls (lgl, lit, other, *p);
     else assert (tag == OCCS), lglrmlcls (lgl, lidx, 0);
     return;
  }
  LOG (CCELOGLEVEL, "no covered clauses with %d eliminated", lit);
}

static int lglcce (LGL * lgl) {
  int oldrem = lgl->ccerem, oldall = lgl->cceall;
  int oldirr, eliminated, idx, all, rem;
  int oldvars = lgl->nvars;
  lglstart (lgl, &lgl->times->cce);
  lgl->stats->cce.count++;
  assert (!lgl->simp && !lgl->cceing);
  lgl->cceing = lgl->simp = 1;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  NEW (lgl->cce, 1);
  NEW (lgl->cce->rem, oldvars);
  for (idx = 2; idx < oldvars; idx++) lgl->cce->rem[idx] = INT_MAX;
  lglgc (lgl);
  lglfreezer (lgl);
  assert (!(oldall && !oldrem));
  all = !oldrem || !oldall;
  if (all) lglprt (lgl, 1, "[cce-%d] scheduling all variables this time",
                   lgl->stats->cce.count);
  else if (!lgleschedrem (lgl, 1)) all = 1, oldrem = 0;
  if (!all) assert (!lgl->donotsched), lgl->donotsched = 1;
  lgldense (lgl, 1);
  if (!all) assert (lgl->donotsched), lgl->donotsched = 0;
  lglsetccelim (lgl);
  oldirr = lgl->stats->irr.clauses.cur;
  while (!lgl->mt &&
         !lglmtstk (&lgl->esched) &&
         lgl->limits->cce.steps > lgl->stats->cce.steps) {
    idx = lglpopesched (lgl);
    lglavar (lgl, idx)->donotcce = 1;
    if (lglocc (lgl, -idx) < lglocc (lgl, idx)) idx = -idx;
    lglccelit (lgl, idx);
    if (lgl->mt) break;
    lglccelit (lgl, -idx);
    lgl->stats->cce.steps++;
  }
  rem = lglcntstk (&lgl->esched);
  if (!rem)
    lglprt (lgl, 1, "[cce-%d] fully completed covered clause elimination",
            lgl->stats->cce.count);
  else if (!oldrem)
    lglprt (lgl, 1, 
      "[cce-%d] incomplete covered clause elimination %d not tried %.0f%%",
      lgl->stats->cce.count, rem, lglpcnt (rem, lgl->nvars - 2));
  else
    rem = lgleschedrem (lgl, 0);
  lglsetdonotesched (lgl, !rem);
  lglsparse (lgl);
  lglgc (lgl);
  lglrelstk (lgl, &lgl->cce->extend);
  lglrelstk (lgl, &lgl->cce->cla);
  DEL (lgl->cce->rem, oldvars);
  DEL (lgl->cce, 1);
  lgl->ccerem = rem > 0;
  lgl->cceall = all && rem;
  lglprt (lgl, 1, "[cce-%d] transition to [ all %d rem %d ] state",
          lgl->stats->cce.count, lgl->cceall, lgl->ccerem);
  assert (oldirr >= lgl->stats->irr.clauses.cur);
  eliminated = oldirr - lgl->stats->irr.clauses.cur;
  lglprt (lgl, 1, "[cce-%d] eliminated %d covered clauses",
          lgl->stats->cce.count, eliminated);
  lglupdcceint (lgl, eliminated);
  assert (lgl->simp && lgl->cceing);
  lgl->cceing = lgl->simp = 0;
  lglrep (lgl, 1 + !eliminated, 'E');
  lglstop (lgl);
  return !lgl->mt;
}

static void lglcliffclause (LGL * lgl, const int * c) {
  int lit, start, i, first, dom, other, * r;
  const int * p, * q;
  for (p = c; (lit = *p);  p++) if (lglval (lgl, lit) > 0) return;
  LOGCLS (2, c, "cliffing clause");
  assert (lglmtstk (&lgl->cliff->lift));
  assert (!lgl->level);
  assert (!lgl->mt);
  start = lglcntstk (&lgl->trail);
  first = 1;
  for (p = c; (lit = *p); p++) {
    if (lglval (lgl, lit) < 0) continue;
    lgl->stats->cliff.decisions++;
    lgliassume (lgl, lit);
    if (!lglbcp (lgl)) {
      LOG (1, "cliffing failed literal %d", lit);
      dom = lglprbana (lgl, lit);
      lglbacktrack (lgl, 0);
      lgl->stats->cliff.failed++;
      lglunit (lgl, -dom);
      if (!lglbcp (lgl)) {
	LOG (1, "empty clause after propagating %d", -dom);
	lgl->mt = 1;
      }
      goto DONE;
    } 
    if (first) {
      for (i = start; i < lglcntstk (&lgl->trail); i++) {
	other = lglpeek (&lgl->trail, i);
	lglpushstk (lgl, &lgl->cliff->lift, other);
      }
      first = 0;
    } else {
      r = lgl->cliff->lift.start;
      for (q = r; q < lgl->cliff->lift.top; q++)
	if (lglval (lgl, (other = *q)) > 0)
	  *r++ = other;
      lgl->cliff->lift.top = r;
    }
    lglbacktrack (lgl, 0);
    if (lglmtstk (&lgl->cliff->lift)) return;
  }
  while (!lglmtstk (&lgl->cliff->lift)) {
    lit = lglpopstk (&lgl->cliff->lift);
    LOG (1, "cliffing lifted unit %d", lit);
    lgl->stats->cliff.lifted++;
    if (lglval (lgl, lit) > 0) continue;
    if (lglval (lgl, lit) < 0) {
      LOG (1, "inconsistent lifted unit %d", lit);
      lgl->mt = 1;
      goto DONE;
    }
    lglunit (lgl, lit);
    if (!lglbcp (lgl)) {
      LOG (1, "empty clause after propagating lifted unit %d", lit);
      lgl->mt = 1;
      goto DONE;
    }
  }
DONE:
  lglclnstk (&lgl->cliff->lift);
}

static int lglcliffclauses (LGL * lgl, Stk * stk) {
  const int * c, * p;
  for (c = stk->start; c < stk->top; c = p + 1) {
    if (*(p = c) >= REMOVED) continue;
    if (lgl->stats->cliff.steps++ >= lgl->limits->cliff.steps) return 0;
    lglcliffclause (lgl, c);
    if (lgl->mt) return 0;
    for (p = c; *p; p++)
      ;
  }
  return 1;
}

static int lglclifflit (LGL * lgl, int lit) {
  const int * w, * eow, * p, * c,  * l;
  int res, blit, tag, other, other2;
  HTS * hts;
  assert (lglmtstk (&lgl->cliff->lits));
  if (!lglisfree (lgl, lit)) return 1;
  if (lgl->stats->cliff.steps++ >= lgl->limits->cliff.steps) return 0;
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == BINCS) continue;
    if (tag == TRNCS) {
      other = blit >> RMSHFT;
      if (abs (other) < abs (lit)) continue;
      other2 = *p;
      if (abs (other2) < abs (lit)) continue;
      lglpushstk (lgl, &lgl->cliff->lits, lit);
      lglpushstk (lgl, &lgl->cliff->lits, other);
      lglpushstk (lgl, &lgl->cliff->lits, other2);
    } else {
      assert (tag == LRGCS);
      c = lglidx2lits (lgl, LRGCS, (blit & REDCS), *p);
      if (*c != lit) continue;
      for (l = c; (other = *l); l++)
	lglpushstk (lgl, &lgl->cliff->lits, other);
    }
    lglpushstk (lgl, &lgl->cliff->lits, 0);
  }
  res = lglcliffclauses (lgl, &lgl->cliff->lits);
  lglclnstk (&lgl->cliff->lits);
  return res && !lgl->mt;
}

static void lglsetclifflim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->cliffreleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->cliffmineff.val) limit = lgl->opts->cliffmineff.val;
  if (lgl->opts->cliffmaxeff.val >= 0 && limit > lgl->opts->cliffmaxeff.val)
    limit = lgl->opts->cliffmaxeff.val;
  limit >>= (pen = lgl->limits->cliff.pen + lglszpen (lgl));
  irrlim = 2*lgl->stats->irr.lits.cur;
  irrlim >>= lgl->limits->simp.pen;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1,
      "[cliff-%d] limit of %lld steps based on %d irredundant literals",
      lgl->stats->cliff.count, (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[cliff-%d] limit of %lld steps penalty %d = %d + %d",
	    lgl->stats->cliff.count, (LGLL) limit,
	    pen, lgl->limits->cliff.pen, lglszpen (lgl));
  lgl->limits->cliff.steps = lgl->stats->cliff.steps + limit;
}

static void lglupdcliffint (LGL * lgl, int success) {
  if (success && lgl->limits->cliff.pen) lgl->limits->cliff.pen--;
  if (!success && lgl->limits->cliff.pen < MAXPEN) lgl->limits->cliff.pen++;
  lgl->limits->cliff.irrprgss = lgl->stats->irrprgss;
}

static int lglcliff (LGL * lgl) {
  int lifted, failed, oldlifted, oldfailed, success;
  lglstart (lgl, &lgl->times->cliff);
  lgl->stats->cliff.count++;
  assert (!lgl->simp && !lgl->cliffing);
  lgl->simp = lgl->cliffing = 1;
  assert (!lgl->cliff);
  NEW (lgl->cliff, 1);
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  oldlifted = lgl->stats->cliff.lifted;
  oldfailed = lgl->stats->cliff.failed;
  lglsetclifflim (lgl);
  if (lglrandlitrav (lgl, lglclifflit))
    lglcliffclauses (lgl, &lgl->irr);
  lifted = lgl->stats->cliff.lifted - oldlifted;
  failed = lgl->stats->cliff.failed - oldfailed;
  lglprt (lgl, 1, "[cliff-%d] failed %d, lifted %d",
          lgl->stats->cliff.count, failed, lifted);
  assert (lgl->simp && lgl->cliffing);
  lgl->simp = lgl->cliffing = 0;
  lglrelstk (lgl, &lgl->cliff->lift);
  lglrelstk (lgl, &lgl->cliff->lits);
  DEL (lgl->cliff, 1);
  success = failed || lifted;
  lglupdcliffint (lgl, success);
  lglrep (lgl, 1 + !success, 'K');
  lglstop (lgl);
  return !lgl->mt;
}

static void lglsetelmlim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  if (lgl->opts->elmrtc.val) {
    lgl->limits->elm.steps = LLMAX;
    lglprt (lgl, 1, "[elim-%d] no limit", lgl->stats->elm.count);
  } else {
    limit = (lgl->opts->elmreleff.val*lgl->stats->visits.search)/1000;
    if (limit < lgl->opts->elmineff.val) limit = lgl->opts->elmineff.val;
    if (lgl->opts->elmaxeff.val >= 0 && limit > lgl->opts->elmaxeff.val)
      limit = lgl->opts->elmaxeff.val;
    limit >>= (pen = lgl->limits->elm.pen + lglszpen (lgl));
    irrlim = lgl->stats->irr.lits.cur;
    irrlim >>= lgl->limits->simp.pen;
    if (limit < irrlim) {
      limit = irrlim;
      lglprt (lgl, 1,
	"[elim-%d] limit of %lld steps based on %d irredundant literals",
	lgl->stats->elm.count, (LGLL) limit, lgl->stats->irr.lits.cur);
    } else
      lglprt (lgl, 1, "[elim-%d] limit of %lld steps penalty %d = %d + %d",
	      lgl->stats->elm.count, (LGLL) limit,
	      pen, lgl->limits->elm.pen, lglszpen (lgl));
    lgl->limits->elm.steps = lgl->stats->elm.steps + limit;
  }
}

static void lglupdelmint (LGL * lgl, int success) {
  if (success && lgl->limits->elm.pen) lgl->limits->elm.pen--;
  if (!success && lgl->limits->elm.pen < MAXPEN) lgl->limits->elm.pen++;
  lgl->limits->elm.irrprgss = lgl->stats->irrprgss;
}

static int lglelim (LGL * lgl) {
  int res = 1, idx, elmd, oldnvars, success, all, rem;
  int oldrem = lgl->elmrem, oldall = lgl->elmall;
  int64_t oldprgss;
  assert (lgl->opts->elim.val);
  assert (!lgl->mt);
  assert (lgl->nvars > 2);
  assert (!lgl->eliminating);
  assert (!lgl->simp);
  lglstart (lgl, &lgl->times->elm);
  lgl->stats->elm.count++;
  lgl->eliminating = lgl->simp = 1;
  NEW (lgl->elm, 1);
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  oldnvars = lglrem (lgl);
  lglgc (lgl);
  lglfreezer (lgl);
  assert (!(oldall && !oldrem));
  all = !oldrem || !oldall;
  if (all) lglprt (lgl, 1, "[elim-%d] scheduling all variables this time",
		   lgl->stats->elm.count);
  else if (!lgleschedrem (lgl, 1)) all = 1, oldrem = 0;
  if (!all) assert (!lgl->donotsched), lgl->donotsched = 1;
  lgldense (lgl, 1);
  if (!all) assert (lgl->donotsched), lgl->donotsched = 0;
  lglsetelmlim (lgl);
  oldprgss = lgl->stats->prgss;
  while (res &&
	 lglsmallirr (lgl) &&
	 !lglterminate (lgl) &&
	 !lglmtstk (&lgl->esched) &&
	 lgl->limits->elm.steps > lgl->stats->elm.steps) {
    idx = lglpopesched (lgl);
    lglavar (lgl, idx)->donotelm = 1;
    lglelimlit (lgl, idx);
    res = lglflush (lgl);
    assert (res || lgl->mt);
  }
  rem = lglcntstk (&lgl->esched);
  if (!rem)
    lglprt (lgl, 1, "[elim-%d] fully completed variable elimination",
	    lgl->stats->elm.count);
  else if (!oldrem)
    lglprt (lgl, 1,
      "[elim-%d] incomplete variable elimination %d not tried %.0f%%",
      lgl->stats->elm.count, rem, lglpcnt (rem, lgl->nvars - 2));
  else
    rem = lgleschedrem (lgl, 0);
  lglsetdonotesched (lgl, !rem);
  lglrelstk (lgl, &lgl->esched);
  lglrelecls (lgl);
  lglsparse (lgl);
  lglgc (lgl);
  lgl->elmrem = rem > 0;
  lgl->elmall = all && rem;
  lglprt (lgl, 1, "[elim-%d] transition to [ all %d rem %d ] state",
	  lgl->stats->elm.count, lgl->elmall, lgl->elmrem);
  elmd = oldnvars - lglrem (lgl);
  success = oldprgss < lgl->stats->prgss;
  lgl->stats->irrprgss += elmd;
  lglupdelmint (lgl, success);
  lglprt (lgl, 1, "[elim-%d] eliminated %d = %.0f%% variables out of %d",
	  lgl->stats->elm.count, elmd, lglpcnt (elmd, oldnvars), oldnvars);
  DEL (lgl->elm, 1);
  lglrep (lgl, 1 + !success, 'e');
  assert (lgl->eliminating && lgl->simp);
  lgl->eliminating = lgl->simp = 0;
  lglstop (lgl);
  return !lgl->mt;
}

static int lglelitblockingoreliminated (LGL * lgl, int elit) {
  Ext * ext = lglelit2ext (lgl, elit);
  return ext->blocking || ext->eliminated;
}

static int lglsynceqs (LGL * lgl) {
  int * ereprs, emax = lgl->maxext;
  int elit1, erepr1, elit2, erepr2;
  int ilit1, irepr1, ilit2, irepr2;
  int consumed = 0, produced = 0;
  assert (!lgl->mt);
  assert (!lgl->level);
  if (!lgl->nvars) return 1;
  if (!lgl->cbs) return 1;
  if (!lgl->cbs->eqs.lock.fun) return 1;
  assert (lgl->repr);
  ereprs = lgl->cbs->eqs.lock.fun (lgl->cbs->eqs.lock.state);
  produced = consumed = 0;
  for (elit1 = 1; elit1 <= emax; elit1++) {
    if (lglelitblockingoreliminated (lgl, elit1)) continue;
    elit2 = lglptrjmp (ereprs, emax, elit1);
    if (elit2 == elit1) continue;
    if (lglelitblockingoreliminated (lgl, elit2)) continue;
    assert (elit2 != -elit1);
    erepr1 = lglerepr (lgl, elit1);
    if (lglelitblockingoreliminated (lgl, erepr1)) continue;
    erepr2 = lglerepr (lgl, elit2);
    if (lglelitblockingoreliminated (lgl, erepr2)) continue;
    if (erepr1 == erepr2) continue;
    if (erepr1 == -erepr2) {
INCONSISTENT:
      LOG (1, "inconsistent external equivalence %d %d", elit1, elit2);
      assert (!lgl->level);
      lgl->mt = 1;
      goto DONE;
    }
    ilit1 = lglimport (lgl, elit1);
    ilit2 = lglimport (lgl, elit2);
    if (ilit1 == ilit2) continue;
    if (ilit1 == -ilit2) goto INCONSISTENT;
    if (abs (ilit1) <= 1) continue;
    if (abs (ilit2) <= 1) continue;
    irepr1 = lglirepr (lgl, ilit1);
    irepr2 = lglirepr (lgl, ilit2);
    if (irepr1 == irepr2) continue;
    if (irepr1 == -irepr2) goto INCONSISTENT;
    if (abs (irepr1) <= 1) continue;
    if (abs (irepr2) <= 1) continue;
    LOG (2, "importing external equivalence %d %d as internal %d %d",
	 elit1, elit2, irepr1, irepr2);
    if (!lglisfree (lgl, irepr1)) continue;
    if (!lglisfree (lgl, irepr2)) continue;
    consumed++;
    lglimerge (lgl, irepr1, irepr2);
  }
  LOG (1, "consumed %d equivalences", consumed);
  for (elit1 = 1; elit1 <= emax; elit1++) {
    elit2 = lglerepr (lgl, elit1);
    if (elit1 == elit2) continue;
    assert (elit1 != -elit2);
    erepr1 = lglptrjmp (ereprs, emax, elit1);
    erepr2 = lglptrjmp (ereprs, emax, elit2);
    if (erepr1 == erepr2) continue;
    assert (erepr1 != -erepr2);
    LOG (2, "exporting external equivalence %d %d", erepr1, erepr2);
    produced++;
    ereprs[abs (erepr1)] = (erepr1 < 0) ? -erepr2 : erepr2;
  }
  LOG (1, "produced %d equivalences", produced);
DONE:
  if (lgl->cbs->eqs.unlock.fun)
    lgl->cbs->eqs.unlock.fun (lgl->cbs->eqs.unlock.state, consumed, produced);
  return !lgl->mt;
}

static int lgldecomp (LGL * lgl) {
  int res = 1, oldnvars = lgl->nvars, success;
  assert (lgl->opts->decompose.val || lgl->probing || lgl->gaussing);
  assert (!lgl->decomposing);
  lglstart (lgl, &lgl->times->dcp);
  lgl->stats->decomps++;
  lgl->decomposing = 1;
  lgl->simp++;
  assert (lgl->simp > 0);
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  res = 0;
  lglgc (lgl);
  if (!lglsyncunits (lgl)) goto DONE;
  if (!lglbcp (lgl)) goto DONE;
  lglgc (lgl);
  if (lgl->mt) goto DONE;
  if (!lgltarjan (lgl)) goto DONE;
  if (!lglsynceqs (lgl)) goto DONE;
  lglchkred (lgl);
  lgldcpdis (lgl);
  lgldcpcln (lgl);
  lgldcpcon (lgl);
  lglcompact (lgl);
  lglmap (lgl);
  if (lgl->mt) goto DONE;
  if (!lglbcp (lgl)) { if (!lgl->mt) lgl->mt = 1; goto DONE; }
  lglcount (lgl);
  lglgc (lgl);
  if (lgl->mt) goto DONE;
  if (!lgl->mt) { lglpicosatchkall (lgl); lglpicosatrestart (lgl); }
  res = 1;
DONE:
  if (lgl->repr) DEL (lgl->repr, lgl->nvars);
  assert (lgl->decomposing);
  lgl->decomposing = 0;
  ASSERT (lgl->simp > 0);
  lgl->simp--;
  success = lgl->nvars < oldnvars;
  lglrep (lgl, 1 + (!success || lgl->probing), 'd');
  lglstop (lgl);
  return res;
}

int lglnvars (LGL * lgl) { return lglrem (lgl); }

static int lglcgrepr (LGL * lgl, int lit) {
  return lglptrjmp (lgl->repr, lgl->nvars - 1, lit);
}

static void lglpushgocc (LGL * lgl, int lit, int gidx) {
  int idx, repr;
  Stk * goccs;
  repr = lglcgrepr (lgl, lit);
  if (abs (repr) == 1) repr = lit;
  idx = abs (repr);
  assert (2 <= idx && idx < lgl->nvars);
  goccs = lgl->cgr->goccs + idx;
  lglpushstk (lgl, goccs, gidx);
}

static void lglenlargegates (LGL * lgl) {
  int oldsize = lgl->cgr->szgates;
  int newsize = oldsize ? 2*oldsize : 1;
  RSZ (lgl->cgr->gates, oldsize, newsize);
  lgl->cgr->szgates = newsize;
}

static int lglcgreprnotconst (LGL * lgl, int lit) {
  int res;
  assert (abs (lit) != 1);
  res = lglcgrepr (lgl, lit);
  if (abs (res) == 1) res = lit;
  return res;
}

static Gat * lglnewgate (LGL * lgl, GTag tag, int lhs, int size) {
  Gat * res;
  int gidx;
  assert (size >= 2);
  if (lgl->cgr->extracted.all >= lgl->cgr->szgates) lglenlargegates (lgl);
  assert (lgl->cgr->extracted.all < lgl->cgr->szgates);
  lgl->stats->cgr.extracted.all++;
  gidx = lgl->cgr->extracted.all++;
  res = lgl->cgr->gates + gidx;
  CLR (*res);
  lhs = lglcgreprnotconst (lgl, lhs);
  res->lhs = lhs;
  lglpushgocc (lgl, lhs, gidx);
  lglavar (lgl, lhs)->gate = 1;
  res->tag = tag;
  res->size = size;
  return res;
}

static Gat * lglgidx2gat (LGL * lgl, int gidx) {
  assert (0 <= gidx && gidx < lgl->cgr->extracted.all);
  return lgl->cgr->gates + gidx;
}

static int lglgat2idx (LGL * lgl, Gat * g) {
  assert (lgl->cgr->gates <= g && 
          g < lgl->cgr->gates + lgl->cgr->extracted.all);
  return g - lgl->cgr->gates;
}

static int lglcgeq (LGL * lgl, int a, int b) {
  return lglcgrepr (lgl, a) == lglcgrepr (lgl, b);
}

static int lglhasitegate (LGL * lgl, int lhs, int cond, int pos, int neg) {
  const int * p;
  int repr;
  Stk * s;
  repr = lglcgrepr (lgl, lhs);
  if (abs (repr) == 1) repr = lhs;
  s = lgl->cgr->goccs + abs (repr);
  Gat * g;
  for (p = s->start; p < s->top; p++) {
    g = lglgidx2gat (lgl, *p);
    if (g->tag != ITETAG) continue;
    assert (g->size == 2);
    if (g->lhs != lhs) continue;
    if (!lglcgeq (lgl, g->cond, cond)) continue;
    if (!lglcgeq (lgl, g->pos, pos)) continue;
    if (!lglcgeq (lgl, g->neg, neg)) continue;
    return 1;
  }
  return 0;
}

#define EL 1

static void lglnewitegate (LGL * lgl, int lhs, int cond, int pos, int neg) {
  int gidx;
  Gat * g;
  lhs = lglcgreprnotconst (lgl, lhs);
  if (lhs < 0) lhs = -lhs, pos = -pos, neg = -neg;
  if ((pos < 0 && neg > 0) || (cond < 0 && lglsgn (pos) == lglsgn (neg))) {
    SWAP (int, pos, neg);
    cond = -cond;
  }
  cond = lglcgreprnotconst (lgl, cond);
  pos = lglcgreprnotconst (lgl, pos);
  neg = lglcgreprnotconst (lgl, neg);
  assert (abs (pos) != -abs (neg));
  if (lglhasitegate (lgl, lhs, cond, pos, neg)) return;
  g = lglnewgate (lgl, ITETAG, lhs, 2);
  gidx = lglgat2idx (lgl, g);
  g->cond = cond, g->pos = pos, g->neg = neg;
  lglpushgocc (lgl, cond, gidx);
  lglpushgocc (lgl, pos, gidx);
  lglpushgocc (lgl, neg, gidx);
  LOG (EL, "extracted ite gate %d = %d ? %d : %d", lhs, cond, pos, neg);
  lgl->stats->cgr.extracted.ite++;
  lgl->cgr->extracted.ite++;
}

static int lglhasbingate (LGL * lgl, GTag tag, int lhs, int rhs0, int rhs1) {
  const int * p;
  Stk * s;
  Gat * g;
  lhs = lglcgreprnotconst (lgl, lhs);
  rhs0 = lglcgreprnotconst (lgl, rhs0);
  rhs1 = lglcgreprnotconst (lgl, rhs1);
  s = lgl->cgr->goccs + abs (lhs);
  for (p = s->start; p < s->top; p++) {
    g = lglgidx2gat (lgl, *p);
    if (g->tag != tag) continue;
    if (g->size != 2) continue;
    if (g->lhs != lhs) continue;
    if (!lglcgeq (lgl, g->lits[0], rhs0)) continue;
    if (!lglcgeq (lgl, g->lits[1], rhs1)) continue;
    return 1;
  }
  return 0;
}

static int lglnewbingate (LGL * lgl, GTag tag, int lhs, int rhs0, int rhs1) {
  int gidx;
  Gat * g;
  lhs = lglcgreprnotconst (lgl, lhs);
  rhs0 = lglcgreprnotconst (lgl, rhs0);
  rhs1 = lglcgreprnotconst (lgl, rhs1);
  if (abs (rhs0) > abs (rhs1)) SWAP (int, rhs0, rhs1);
  if (tag == XORTAG && lhs < 0) lhs = -lhs, rhs0 = -rhs0;
  if (tag == XORTAG && rhs0 < 0) rhs0 = -rhs0, rhs1 = -rhs1;
  if (lglhasbingate (lgl, tag, lhs, rhs0, rhs1)) return 0;
  g = lglnewgate (lgl, tag, lhs, 2);
  gidx = lglgat2idx (lgl, g);
  g->lits[0] = rhs0, g->lits[1] = rhs1;
  lglpushgocc (lgl, rhs0, gidx);
  lglpushgocc (lgl, rhs1, gidx);
  if (tag == ANDTAG) {
    lgl->cgr->extracted.and++;
    lgl->stats->cgr.extracted.and++;
    LOG (EL, "extracted binary and gate %d = %d & %d", lhs, -rhs0, -rhs1);
  } else if (tag == XORTAG) {
    lgl->cgr->extracted.xor++;
    lgl->stats->cgr.extracted.xor++;
    LOG (EL, "extracted binary xor gate %d = %d ^ %d", -lhs, rhs0, rhs1);
  }
  return 1;
}

static void lglnewlrgate (LGL * lgl, GTag tag, int lhs, int * cls, int size) {
  int gidx, other, lhsrepr;
  const int * p;
  Gat * g;
  assert (size >= 3);
  lhsrepr = lglcgreprnotconst (lgl, lhs);
  g = lglnewgate (lgl, tag, lhsrepr, size);
  g->origlhs = lhs;
  gidx = lglgat2idx (lgl, g);
  g->cls = cls;
  for (p = cls; (other = *p); p++) {
    if (abs (other) == lhs) continue;
    other = lglcgreprnotconst (lgl, other);
    lglpushgocc (lgl, other, gidx);
  }
  if (tag == ANDTAG) lgl->cgr->extracted.and++, lgl->stats->cgr.extracted.and++;
  if (tag == XORTAG) lgl->cgr->extracted.xor++, lgl->stats->cgr.extracted.xor++;
#ifndef NLGLOG
  if (lgl->opts->log.val >= EL) {
    if (tag == ANDTAG) {
      int count = 0;
      lglogstart (lgl, EL, "extracted %d-ary and gate %d = ", size, lhsrepr);
      for (p = cls; (other = *p); p++) {
	if (abs (other) == abs (lhs)) continue;
	if (count++) fputs (" & ",  lgl->out);
	fprintf (lgl->out, "%d", -lglcgrepr (lgl, other));
      }
      lglogend (lgl);
    } else if (tag == XORTAG) {
      int count = 0;
      lglogstart (lgl, EL, "extracted %d-ary xor gate %d = ", size, -lhs);
      for (p = cls; (other = *p); p++) {
	if (abs (other) == abs (lhs)) continue;
	if (count++) fputs (" ^ ",  lgl->out);
	fprintf (lgl->out, "%d", lglcgrepr (lgl, other));
      }
      lglogend (lgl);
    } else COVER (1);
  }
#endif
}

static void lglcgmerge (LGL * lgl, int other, int repr) {
  int * p, * q, gidx;
  Stk * from, * to;
  Gat * g;
  assert (lgl->cgrclosing);
  if (abs (other) == 1) SWAP (int, other, repr);
  if (repr == -1) other = -other, repr = 1;
  assert (lglcgrepr (lgl, other) == other);
  assert (abs (repr) != abs (other));
  if (repr == 1) {
    assert (abs (other) >= 2);
#ifndef NLGLPICOSAT
    if (lgl->picosat.chk) lglpicosatchkclsarg (lgl, other, 0);
#endif
    if (other < 0) other = -other, repr = -repr;
    lgl->repr[other] = repr;
    lglwrktouch (lgl, other);
  } else {
    assert (abs (repr) >= 2);
    assert (lglcgrepr (lgl, repr) == repr);
    if (lglcmprepr (lgl, other, repr) < 0) SWAP (int, repr, other);
    if (other < 0) other = -other, repr = -repr;
    lglimerge (lgl, other, repr);
    assert (0 < other);
    from = lgl->cgr->goccs + abs (other);
    to = lgl->cgr->goccs + abs (repr);
    q = to->start;
    for (p = q; p < to->top; p++) {
      g = lglgidx2gat (lgl, (gidx = *p));
      if (g->mark) continue;
      *q++ = gidx;
      g->mark = 1;
    }
    to->top = q;
    LOG (2, "merging %d gate occurrences of %d with %d occurrences of %d",
	 lglcntstk (from), other, lglcntstk (to), repr);
    for (p = from->start; p < from->top; p++) {
      g = lglgidx2gat (lgl, (gidx = *p));
      if (g->mark) continue;
      g->mark = 1;
      lglpushstk (lgl, to, gidx);
    }
    lglrelstk (lgl, from);
    for (p = to->start; p < to->top; p++) {
      g = lglgidx2gat (lgl, *p); assert (g->mark); g->mark = 0;
    }
    lglwrktouch (lgl, repr);
  }
}

static int lglcmpocc (LGL * lgl, int a, int b) {
  return lglocc (lgl, a) - lglocc (lgl, b);
}

#define LGLCMPOCC(A,B) lglcmpocc (lgl, *(A), *(B))

static int lglcgextractlimhit (LGL * lgl) {
  return lgl->stats->cgr.esteps >= lgl->limits->cgr.esteps;
}

static int lglincextractlimhit (LGL * lgl) {
  lgl->stats->cgr.esteps++;
  return lglcgextractlimhit (lgl);
}

static int lglcgunit (LGL * lgl, int lit) {
  int next, repr, other, ok;
  Val val = lglval (lgl, lit);
  if (val > 0) return 1;
  if (val < 0) {
    LOG (1, "extracted unit already %d falsified", lit);
    lgl->mt = 1;
    return 0;
  }
  next = lgl->next;
  lglunit (lgl, lit);
  if ((ok = lglbcp (lgl))) {
    while (next < lgl->next) {
      other = lglpeek (&lgl->trail, next++);
      repr = lglcgrepr (lgl, other);
      if (repr == 1) continue;
      if (repr == -1) { ok = 0; break; }
      assert (repr != -1);
      lglcgmerge (lgl, repr, 1);
    }
  }
  if (!ok) {
    LOG (1, "propagation of congruence closure unit %d failed", lit);
    lgl->mt = 1;
  }
  return ok;
}

static int lglcgextractands (LGL * lgl, int lit) {
  int blit, tag, other, other2, lidx, size, tmp, repr;
  const int * p, * w, * eow, * l;
  HTS * hts;
  Val val;
  int * c;
  if (!lgl->opts->cgrextand.val) return 1;
  repr = lglcgrepr (lgl, lit);
  if (lglval (lgl, lit)) { assert (abs (repr) == 1); return 1; }
  if (abs (repr) == 1) return 1;
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    if (lglincextractlimhit (lgl)) return 0;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == BINCS) {
      if (!lgl->opts->cgrexteq.val && !lgl->opts->cgrextunits.val) continue;
      tmp = blit >> RMSHFT;
      if ((val = lglval (lgl, tmp))) { assert (val > 0); continue; }
      repr = lglcgrepr (lgl, lit);
      other = lglcgrepr (lgl, -tmp);
      if (repr == other) continue;
      if (lglincextractlimhit (lgl)) return 0;
      if (lgl->opts->cgrextunits.val &&
	  repr != 1 &&
	  lglhasbin (lgl, lit, -tmp)) {
	LOG (EL, "extracted unit %d with %d %d and %d %d",
	     lit, lit, tmp, lit, -tmp);
	lgl->cgr->extracted.units++;
	lgl->stats->cgr.units++;
	if (repr == -1) {
	  LOG (1, "extracted unit %d in conflict with previous unit %d",
	       lit, -lit);
	  lgl->mt = 1;
	  return 0;
	}
	if (!lglcgunit (lgl, repr)) return 0;
	if (lglval (lgl, lit)) return 1;
      } else if (lgl->opts->cgrexteq.val && lglhasbin (lgl, -lit, -tmp)) {
	lgl->cgr->extracted.eq++;
	lgl->stats->cgr.eq++;
	LOG (EL, "extracted equivalence %d = %d with representatives %d = %d",
	     lit, -tmp, repr, other);
	if (repr == -other) {
	  assert (!lgl->mt);
	  LOG (1, "merging equivalence classes of opposite literals");
	  lgl->mt = 1;
	  return 0;
	}
	lglcgmerge (lgl, other, repr);
      }
      continue;
    }
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    if (tag == TRNCS) {
      other = blit >> RMSHFT;
      other2 = *p;
      if (lglocc (lgl, other) > lglocc (lgl, other2)) SWAP (int, other, other2);
      if (lglincextractlimhit (lgl)) return 0;
      if (!lglhasbin (lgl, -lit, -other)) continue;
      if (lglincextractlimhit (lgl)) return 0;
      if (!lglhasbin (lgl, -lit, -other2)) continue;
      lglnewbingate (lgl, ANDTAG, lit, other, other2);
    } else  {
      assert (tag == OCCS);
      if (lglincextractlimhit (lgl)) return 0;
      assert (lglmtstk (&lgl->clause));
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, OCCS, (blit & REDCS), lidx);
      for (l = c; (other = *l); l++)
	if (other != lit) lglpushstk (lgl, &lgl->clause, other);
      size = l - c - 1;
      assert (size >= 3);
      SORT (int, lgl->clause.start, size, LGLCMPOCC);
      for (l = lgl->clause.start; l < lgl->clause.top; l++) {
	if (lglincextractlimhit (lgl)) { lglclnstk (&lgl->clause); return 0; }
	if (!lglhasbin (lgl, -lit, -*l)) break;
      }
      if (l == lgl->clause.top) lglnewlrgate (lgl, ANDTAG, lit, c, size);
      lglclnstk (&lgl->clause);
    }
  }
  return 1;
}

static int lglparity (LGL * lgl) {
  const int * p;
  int res = 0;
  for (p = lgl->clause.start; p < lgl->clause.top; p++)
    if (*p < 0) res = !res;
  return res;
}

static void lglinclause (LGL * lgl, int parity) {
  int * p;
  assert (lglparity (lgl) == parity);
  do {
    for (p = lgl->clause.start; p < lgl->clause.top; p++)
      if ((*p = -*p) < 0) break;
  } while (lglparity (lgl) != parity);
}

static int lglxorhascls (LGL * lgl) {
  int lit, res, minlit = 0, minoccs = INT_MAX, litoccs, blit, tag, lidx;
  const int * p, * w, * eow, * l, * c;
  HTS * hts;
  for (p = lgl->clause.start; p < lgl->clause.top; p++) {
    lit = *p;
    litoccs = lglocc (lgl, lit);
    if (litoccs < minoccs) minlit = lit, minoccs = litoccs;
    lglsignedmark (lgl, lit);
  }
  res = 0;
  hts = lglhts (lgl, minlit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; !res && p < eow; p++) {
    blit = *p;
    if (lglincextractlimhit (lgl)) { assert (!res); break; }
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag != OCCS) continue;
    lidx = blit >> RMSHFT;
    if (lglincextractlimhit (lgl)) { assert (!res); break; }
    c = lglidx2lits (lgl, OCCS, (blit & REDCS), lidx);
    for (l = c; (lit = *l); l++) if (!lglsignedmarked (lgl, lit)) break;
    if (!lit) res = 1;
  }
  for (p = lgl->clause.start; p < lgl->clause.top; p++) lglunmark (lgl, *p);
  return res;
}

static int lglcgextractxors (LGL * lgl, int lit) {
  int blit, tag, other, other2, lidx, size, count, parity, * c;
  const int * p, * w, * eow, * l;
  HTS * hts;
  if (!lgl->opts->cgrextxor.val) return 1;
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    if (lglincextractlimhit (lgl)) return 0;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == BINCS) continue;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    if (tag == TRNCS) {
      if ((other = blit >> RMSHFT) < 0) continue;
      if ((other2 = *p) < 0) continue;
      lgl->stats->cgr.esteps++;
      if (!lglhastrn (lgl, lit, -other, -other2)) continue;
      lgl->stats->cgr.esteps++;
      if (!lglhastrn (lgl, -lit, other, -other2)) continue;
      lgl->stats->cgr.esteps++;
      if (!lglhastrn (lgl, -lit, -other, other2)) continue;
      lglnewbingate (lgl, XORTAG, lit, other, other2);
    } else {
      assert (tag == OCCS);
      if (lglincextractlimhit (lgl)) return 0;
      assert (lglmtstk (&lgl->clause));
      lidx = blit >> RMSHFT;
      c = lglidx2lits (lgl, OCCS, (blit & REDCS), lidx);
      for (l = c; (other = *l); l++) {
	if (other != lit && other < 0) break;
	lglpushstk (lgl, &lgl->clause, other);
      }
      if (!other && (size = l - c - 1) <= lgl->opts->cgrmaxority.val) {
	assert (3 <= size && size <= 30);
	count = (1 << size);
	assert (count > 0);
	parity = (lit < 0);
	while (--count) {
	  lglinclause (lgl, parity);
	  lgl->stats->cgr.esteps++;
	  if (!lglxorhascls (lgl)) break;
	}
	if (!count) {
#ifndef NDEBUG
	  int i;
	  lglinclause (lgl, parity);
	  for (i = 0; i <= size; i++) assert (lgl->clause.start[i] == c[i]);
#endif
	  lglnewlrgate (lgl, XORTAG, lit, c, size);
	}
      }
      lglclnstk (&lgl->clause);
    }
  }
  return !lglcgextractlimhit (lgl);
}

static int lglcmpitecands (const ITEC * c, const ITEC * d) {
  int a = c->other, b = d->other;
  int res = abs (a) - abs (b);
  if (res) return res;
  if ((res = a - b)) return res;
  return c->other2 - d->other2;
}

static int lglcgmergelhsrhs (LGL * lgl, int lhs, int rhs) {
  int conflict = 0;
  lhs = lglcgrepr (lgl, lhs);
  rhs = lglcgrepr (lgl, rhs);
  if (lhs == rhs) return 0;
  if (lhs == -rhs) conflict = 1;
  else if (lhs == 1) {
    if (rhs == -1) conflict = 1;
    else assert (rhs != 1), conflict = !lglcgunit (lgl, rhs);
  } else if (lhs == -1) {
    if (rhs == 1) conflict = 1;
    else assert (rhs != -1), conflict = !lglcgunit (lgl, -rhs);
  } else if (rhs == 1) conflict = !lglcgunit (lgl, lhs);
  else if (rhs == -1) conflict = !lglcgunit (lgl, -lhs);
  else lglcgmerge (lgl, lhs, rhs);
  return conflict;
}

static void lglcgextractitecands (LGL * lgl, int lhs, ITEC * cands, int ncands) {
  int cond, pos, neg;
  int l, m, r, i, j;
  for (l = 0; l < ncands; l = r) {
    for (r = l + 1; r < ncands; r++)
      if (abs (cands[l].other) != abs (cands[r].other)) break;
    if (cands[l].other == cands[r-1].other) continue;
    assert (cands[l].other < 0);
    assert (cands[r-1].other > 0);
    assert (cands[l].other == -cands[r-1].other);
    for (m = l + 1; cands[m].other < 0; m++) assert (m + 1 < r);
    for (i = l; i + 1 < m; i++) assert (cands[i].other == cands[i+1].other);
    for (i = m; i + 1 < r; i++) assert (cands[i].other == cands[i+1].other);
    for (i = l; i < m; i++) {
      for (j = m; j < r; j++) {
	lhs = lglcgreprnotconst (lgl, lhs);
	lgl->stats->cgr.esteps++;
	cond = -cands[l].other;
	pos = -cands[l].other2;
	neg = -cands[m].other2;
	pos = lglcgreprnotconst (lgl, pos);
	neg = lglcgreprnotconst (lgl, neg);
	if (pos == -neg) continue;		// skip XORTAGs
	if (pos == neg) {
	  if (lhs != pos && lglcgmergelhsrhs (lgl, lhs, pos)) return;
	} else lglnewitegate (lgl, lhs, cond, pos, neg);
      }
    }
  }
}

static int lglcgextractites (LGL * lgl, int lit) {
  int blit, tag, other, other2;
  ITEC * cands; int ncands;
  const int * p, * w, * eow;
  HTS * hts;
  if (!lgl->opts->cgrextite.val) return 1;
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    lgl->stats->cgr.esteps++;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == BINCS || tag == OCCS) continue;
    p++;
    if (tag == LRGCS) continue;
    assert (tag == TRNCS);
    other = blit >> RMSHFT;
    other2 = *p;
    lglsignedmark (lgl, other);
    lglsignedmark (lgl, other2);
  }
  assert (lglmtstk (&lgl->seen));
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == BINCS || tag == OCCS) continue;
    p++;
    if (tag == LRGCS) continue;
    lgl->stats->cgr.esteps++;
    assert (tag == TRNCS);
    other = blit >> RMSHFT;
    other2 = *p;
    if (lglsignedmarked (lgl, -other)) {
      lgl->stats->cgr.esteps++;
      if (lglhastrn (lgl, -lit, other, -other2)) {
	lglpushstk (lgl, &lgl->seen, other);
	lglpushstk (lgl, &lgl->seen, other2);
      }
    }
    if (lglsignedmarked (lgl, -other2)) {
      lgl->stats->cgr.esteps++;
      if (lglhastrn (lgl, -lit, -other, other2)) {
	lglpushstk (lgl, &lgl->seen, other2);
	lglpushstk (lgl, &lgl->seen, other);
      }
    }
  }
  for (p = w; p < eow; p++) {
    lgl->stats->cgr.esteps++;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == BINCS || tag == OCCS) continue;
    p++;
    if (tag == LRGCS) continue;
    assert (tag == TRNCS);
    other = blit >> RMSHFT;
    other2 = *p;
    lglunmark (lgl, other);
    lglunmark (lgl, other2);
  }
  if ((ncands = lglcntstk (&lgl->seen))) {
    cands = (ITEC *) lgl->seen.start;
    assert (!(ncands & 1));
    ncands /= 2;
    SORT (ITEC, cands, ncands, lglcmpitecands);
    lglcgextractitecands (lgl, lit, cands, ncands);
  }
  lglclnstk (&lgl->seen);
  return !lglcgextractlimhit (lgl);
}

static int lglcgextractidx (LGL * lgl, int idx) {
  if (!lglisfree (lgl, idx)) return 1;
  if (lglavar (lgl, idx)->donotcgrcls) return 1;
  if (lglcgextractlimhit (lgl)) return 0;
  if (lglterminate (lgl)) return 0;
  if (!lgl->mt && !lglcgextractands (lgl, idx)) return 0;
  if (!lgl->mt && !lglcgextractands (lgl, -idx)) return 0;
  if (!lgl->mt && !lglcgextractxors (lgl, idx)) return 0;
  if (!lgl->mt && !lglcgextractxors (lgl, -idx)) return 0;
  if (!lgl->mt && !lglcgextractites (lgl, idx)) return 0;
  if (!lgl->mt && !lglcgextractites (lgl, -idx)) return 0;
  return 1;
}

static void lglgateextract (LGL * lgl) {
  int idx, count;
  LOG (EL, "starting new extraction %d", lgl->stats->cgr.count);
  lglrandidxtrav (lgl, lglcgextractidx);
  count = 0;
  for (idx = 2; idx < lgl->nvars; idx++) count += lgl->avars[idx].gate;
  if (lgl->cgr->extracted.units)
    lglprt (lgl, 2, "[extract-%d] extracted %d units",
      lgl->stats->cgr.count, lgl->cgr->extracted.units);
  if (lgl->cgr->extracted.eq)
    lglprt (lgl, 2, "[extract-%d] extracted %d equivalences",
      lgl->stats->cgr.count, lgl->cgr->extracted.eq);
  if (lgl->cgr->extracted.all)
    lglprt (lgl, 2,
      "[extract-%d] extracted %d gates for %d variables %.0f%%",
      lgl->stats->cgr.count, lgl->cgr->extracted.all,
      count, lglpcnt (count, lgl->nvars));
  if (lgl->cgr->extracted.and)
    lglprt (lgl, 2,
      "[extract-%d] %d and gates %.0f%% of all extracted gates",
      lgl->stats->cgr.count,
      lgl->cgr->extracted.and, lglpcnt (lgl->cgr->extracted.and, lgl->cgr->extracted.all));
  if (lgl->cgr->extracted.xor)
    lglprt (lgl, 2,
      "[extract-%d] %d xor gates %.0f%% of all extracted gates",
      lgl->stats->cgr.count,
      lgl->cgr->extracted.xor, lglpcnt (lgl->cgr->extracted.xor, lgl->cgr->extracted.all));
  if (lgl->cgr->extracted.ite)
    lglprt (lgl, 2,
      "[extract-%d] %d ite gates %.0f%% of all extracted gates",
      lgl->stats->cgr.count,
      lgl->cgr->extracted.ite, lglpcnt (lgl->cgr->extracted.ite, lgl->cgr->extracted.all));
}

static void lglprtcgrem (LGL * lgl) {
  int idx, ret = 0, rem = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lglavar (lgl, idx)->donotcgrcls) ret++; else rem++;
  }
  if (rem)
    lglprt (lgl, 1, "[cgrclsr-%d] %d variables remain %.0f%% (%d retained)",
	    lgl->stats->cgr.count, rem, lglpcnt (rem, lglrem (lgl)), ret);
  else {
    lglprt (lgl, 1, "[cgrclsr-%d] fully completed congruence closure",
           lgl->stats->cgr.count);
    for (idx = 2; idx < lgl->nvars; idx++)
      lglavar (lgl, idx)->donotcgrcls = 0;
  }
}

static void lglcginit (LGL * lgl) {
  int idx, schedulable = 0, donotcgrcls = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lglavar (lgl, idx)->donotcgrcls) donotcgrcls++;
    else schedulable++;
  }
  if (!schedulable) {
    donotcgrcls = 0;
    for (idx = 2; idx < lgl->nvars; idx++) {
      if (!lglisfree (lgl, idx)) continue;
      lglavar (lgl, idx)->donotcgrcls = 0;
      schedulable++;
    }
  }
  if (!donotcgrcls)
    lglprt (lgl, 1, "[cgrclsr-%d] all %d free variables schedulable",
            lgl->stats->cgr.count, schedulable);
  else
    lglprt (lgl, 1,
      "[cgrclsr-%d] %d schedulable variables %.0f%%",
      lgl->stats->cgr.count, schedulable, lglpcnt (schedulable, lglrem (lgl)));
  lglwrkinit (lgl, 1, 1);
  assert (!lgl->donotsched), lgl->donotsched = 1;
  lglrandidxtrav (lgl, lglwrktouch);
  assert (lgl->donotsched), lgl->donotsched = 0;
  NEW (lgl->cgr->goccs, lgl->nvars);
}

static void lglcgreset (LGL * lgl) {
  const int * p;
  int idx;
  for (idx = 2; idx < lgl->nvars; idx++) lgl->avars[idx].donotcgrcls = 1;
  for (p = lgl->wrk->queue.start; p < lgl->wrk->queue.top; p++)
    lgl->avars[abs (*p)].donotcgrcls = 0;
  lglwrkreset (lgl);
  for (idx = 2; idx < lgl->nvars; idx++) lgl->avars[idx].gate = 0;
  for (idx = 2; idx < lgl->nvars; idx++) lglrelstk (lgl, lgl->cgr->goccs + idx);
  DEL (lgl->cgr->goccs, lgl->nvars);
  DEL (lgl->cgr->gates, lgl->cgr->szgates);
  lgl->cgr->szgates = 0;
}

static void lglsetbinminrhs (LGL * lgl, Gat * g) {
  int a = lglcgrepr (lgl, g->lits[0]);
  int b = lglcgrepr (lgl, g->lits[1]);
  assert (g->size == 2);
  assert (g->tag != ITETAG);
  if (abs (a) == 1 && abs (b) == 1) g->minrhs = INT_MAX;
  else if (abs (a) == 1) g->minrhs = b;
  else if (abs (b) == 1) g->minrhs = a;
  else g->minrhs = (abs (a) < abs (b)) ? a : b;
}

static void lglsetlrgminrhs (LGL * lgl, Gat * g) {
  int * p, other;
  assert (g->size >= 3);
  g->minrhs = INT_MAX;
  for (p = g->cls; (other = *p); p++) {
    if (abs (other) == abs (g->lhs)) continue;
    other = lglcgrepr (lgl, other);
    if (abs (other) == 1) continue;
    if (abs (g->minrhs) > abs (other)) g->minrhs = other;
  }
  assert (g->minrhs);
}

static void lglsetiteminrhs (LGL * lgl, Gat * g) {
  assert (g->tag == ITETAG);
  g->minrhs = lglcgrepr (lgl, g->cond);
}

static void lglsetminrhs (LGL * lgl, Gat * g) {
  if (g->tag == ITETAG) lglsetiteminrhs (lgl, g);
  else if (g->size == 2) lglsetbinminrhs (lgl, g);
  else lglsetlrgminrhs (lgl, g);
  if (g->tag != ANDTAG && g->minrhs < 0) g->minrhs = -g->minrhs;
}

static int lglcmpgoccs (LGL * lgl, int a, int b) {
  Gat * g = lglgidx2gat (lgl, a);
  Gat * h = lglgidx2gat (lgl, b);
  int res = (g->tag - h->tag); if (res) return res;
  if ((res = (g->size - h->size))) return res;
  if ((res = g->minrhs - h->minrhs)) return res;
  return a - b;
}

#define LGLCMPGOCCS(A,B) lglcmpgoccs (lgl, *(A), *(B))

static int lglgoccsmatchcand (LGL * lgl, int a, int b) {
  Gat * g = lglgidx2gat (lgl, a);
  Gat * h = lglgidx2gat (lgl, b);
  return g->tag == h->tag && g->size == h->size && g->minrhs == h->minrhs;
}

static int lglmatchitegate (LGL * lgl, Gat * g, Gat * h) {
  int gc = lglcgrepr (lgl, g->cond);
  int gp = lglcgrepr (lgl, g->pos);
  int gn = lglcgrepr (lgl, g->neg);
  int hc = lglcgrepr (lgl, h->cond);
  int hp = lglcgrepr (lgl, h->pos);
  int hn = lglcgrepr (lgl, h->neg);
  assert (g->tag == ITETAG);
  assert (g->size == 2);
  lgl->stats->cgr.csteps++;
  if (gc == 1) {
    if (hc == 1 && gp == hp) return 1;
    if (hc == -1 && gp == hn) return 1;
  } else if (gc == -1) {
    if (hc == 1 && gn == hp) return 1;
    if (hc == -1 && gn == hn) return 1;
  } else if (gc == hc) {
    if (gp == hp && gn == hn) return 1;
    if (gp == -hp && gn == -hn) return -1;
  } else if (gc == -hc) {
    if (gp == hn && gn == hp) return 1;
    if (gp == -hn && gn == -hp) return -1;
  }
  return 0;
}

static int lglmatchbingate (LGL * lgl, Gat * g, Gat * h) {
  int g0 = lglcgrepr (lgl, g->lits[0]);
  int g1 = lglcgrepr (lgl, g->lits[1]);
  int h0 = lglcgrepr (lgl, h->lits[0]);
  int h1 = lglcgrepr (lgl, h->lits[1]);
  int sign;
  lgl->stats->cgr.csteps++;
  assert (g->size == 2);
  if (g->tag == XORTAG) {
    sign = 1;
    if (g0 < 0) sign = -sign, g0 = -g0;
    if (g1 < 0) sign = -sign, g1 = -g1;
    if (h0 < 0) sign = -sign, h0 = -h0;
    if (h1 < 0) sign = -sign, h1 = -h1;
  } else sign = 1;
  if (g0 == h0 && g1 == h1) return sign;
  if (g0 == h1 && g1 == h0) return sign;
  return 0;
}

static int lglmatchlrgandaux (LGL * lgl, Gat * g, Gat * h) {
  int * p, other, repr, bit, res, jumped, gfalse, gtrue, htrue, hfalse;
  int found;
  AVar * u;
  assert (g->tag == ANDTAG);
  jumped = 1;
  gtrue = 1, gfalse = 0;
  found = 0;
  for (p = g->cls; (other = *p); p++) {
    lgl->stats->cgr.csteps++;
    if (abs (other) == abs (g->origlhs)) {
      assert (other == g->origlhs);
      found++;
      continue;
    }
    repr = lglcgrepr (lgl, -other);
    if (repr == -1) { gfalse = 1; break; }
    if (repr == 1) continue;
    gtrue = 0;
    if (repr != other) jumped = -1;
    u = lglavar (lgl, repr);
    bit = 1 << (repr < 0);
    if (u->mark & (bit^3)) gfalse = 1;
    if (u->mark & bit) continue;
    u->mark |= bit;
  }
  assert (other || found);
  res = 1;
  htrue  = 1, hfalse = 0;
  found = 0;
  for (p = h->cls; res && (other = *p); p++) {
    if (abs (other) == abs (h->origlhs)) {
      found++;
      assert (other == h->origlhs);
      continue;
    }
    repr = lglcgrepr (lgl, -other);
    if (repr == -1) { hfalse = 1; break; }
    if (repr == 1) continue;
    htrue = 0;
    if (repr != other) jumped = -1;
    u = lglavar (lgl, repr);
    bit = 1 << (repr < 0);
    if (!(u->mark & bit)) res = 0;
  }
  assert (!res || other || found);
  found = 0;
  for (p = g->cls; (other = *p); p++) {
    if (abs (other) == abs (g->origlhs)) { found++; continue; }
    repr = lglcgrepr (lgl, other);
    if (abs (repr) == 1) continue;
    u = lglavar (lgl, repr);
    u->mark = 0;
  }
  assert (found);
  if (gfalse) {
    lglcgmergelhsrhs (lgl, g->lhs, -1);
    res = 0;
  } else if (res && gtrue) {
    lglcgmergelhsrhs (lgl, g->lhs, 1);
    res = 0;
  }
  if (hfalse) {
    lglcgmergelhsrhs (lgl, h->lhs, -1);
    res = 0;
  } else if (htrue) {
    lglcgmergelhsrhs (lgl, h->lhs, 1);
    res = 0;
  }
  return jumped * res;
}

static int lglmatchlrgand (LGL * lgl, Gat * g, Gat * h) {
  int res = lglmatchlrgandaux (lgl, g, h);
  if (res >= 0) return res;
  res = lglmatchlrgandaux (lgl, h, g);
  return abs (res);
}

static int lglmatchlrgxor (LGL * lgl, Gat * g, Gat * h) {
  int * p, other, repr, res, gconst, hconst, hpar, gpar, sign, found;
  AVar * u;
  gconst = 1, gpar = -1;
  sign = 1;
  found = 0;
  for (p = g->cls; (other = *p); p++) {
    lgl->stats->cgr.csteps++;
    if (abs (other) == abs (g->origlhs)) {
      assert (other == g->origlhs);
      found++;
      continue;
    }
    repr = lglcgrepr (lgl, other);
    if (repr < 0) sign = -sign, gpar = -gpar;
    if (abs (repr) == 1) continue;
    else gconst = 0;
    u = lglavar (lgl, repr);
    u->mark = !u->mark;
  }
  assert (found);
  hconst = 1, hpar = -1;
  found = 0;
  for (p = h->cls; (other = *p); p++) {
    if (abs (other) == abs (h->origlhs)) {
      assert (other == h->origlhs);
      found++;
      continue;
    }
    repr = lglcgrepr (lgl, other);
    if (abs (repr) != 1) hconst = 0;
    if (repr < 0) sign = -sign, hpar = -hpar;
    if (abs (repr) == 1) continue;
    else gconst = 0;
    u = lglavar (lgl, repr);
    u->mark = !u->mark;
  }
  assert (found);
  res = 1;
  found = 0;
  for (p = g->cls; (other = *p); p++) {
    if (abs (other) == abs (g->origlhs)) { found++; continue; }
    repr = lglcgrepr (lgl, other);
    if (abs (repr) == 1) continue;
    u = lglavar (lgl, repr);
    if (!u->mark) continue;
    u->mark = 0;
    res = 0;
  }
  assert (found);
  found = 0;
  for (p = h->cls; (other = *p); p++) {
    if (abs (other) == abs (h->origlhs)) { found++; continue; }
    repr = lglcgrepr (lgl, other);
    if (abs (repr) == 1) continue;
    u = lglavar (lgl, repr);
    if (!u->mark) continue;
    u->mark = 0;
    res = 0;
  }
  assert (found);
  if (gconst) {
    lglcgmergelhsrhs (lgl, g->lhs, gpar);
    res = 0;
  }
  if (hconst) {
    lglcgmergelhsrhs (lgl, h->lhs, hpar);
    res = 0;
  }
  return sign*res;
}

static int lglmatchgate (LGL * lgl, int fixed, Gat * g, Gat * h) {
  int l = lglcgrepr (lgl, g->lhs);
  int k = lglcgrepr (lgl, h->lhs);
  int repr, other, s;
  assert (g != h);
  lgl->stats->cgr.csteps++;
  assert (g->tag == h->tag);
  assert (g->size == h->size);
  assert (g->minrhs == h->minrhs);
  assert (g->size >= 2);
  if (g->tag == ITETAG) { if (!(s = lglmatchitegate (lgl, g, h))) return 0; }
  else if (g->size == 2) { if (!(s = lglmatchbingate (lgl, g, h))) return 0; }
  else if (g->tag == ANDTAG) { if (!(s = lglmatchlrgand (lgl, g, h))) return 0; }
  else if (g->tag == XORTAG) { if (!(s = lglmatchlrgxor (lgl, g, h))) return 0; }
  else return 0;
  repr = s*l, other = k;
  if (repr == other) return 0;
  lgl->stats->cgr.matched.all++;
  lgl->cgr->matched.all++;
  if (g->tag == ANDTAG) lgl->cgr->matched.and++, lgl->stats->cgr.matched.and++;
  if (g->tag == XORTAG) lgl->cgr->matched.xor++, lgl->stats->cgr.matched.xor++;
  if (g->tag == ITETAG) lgl->cgr->matched.ite++, lgl->stats->cgr.matched.ite++;
  if (repr == -other) {
    assert (!lgl->mt);
    LOG (1, "gate match to same literal only differing in sign");
    lgl->mt = 1;
    return 0;
  }
  LOG (1, "gates with lhs %d and %d match", g->lhs, h->lhs);
  lglcgmerge (lgl, other, repr);
  if (abs (fixed) == abs (other)) return 1;
  if (abs (fixed) == abs (repr)) return 1;
  return 0;
}

static int lglcgrlimhit (LGL * lgl) {
  return lgl->stats->cgr.csteps >= lgl->limits->cgr.csteps;
}

static int lglsimpbinand (LGL * lgl, Gat * g) {
  int lhs, a, b, res, conflict;
  assert (g->tag == ANDTAG && g->size == 2);
  lhs = lglcgrepr (lgl, g->lhs);
  a = lglcgrepr (lgl, -g->lits[0]);
  b = lglcgrepr (lgl, -g->lits[1]);
  conflict = res = 0;
  if (a == 1 && b == 1) {
    if (lhs == 1) return 0;
    res = 1;
    LOG (2, "binary and gate with lhs %d simplified to true", g->lhs);
    if (lhs == -1) conflict = 1; else conflict = !lglcgunit (lgl, lhs);
  } else if (a == -1 || b == -1 || a == -b) {
    if (lhs == -1) return 0;
    res = 1;
    LOG (2, "binary and gate with lhs %d simplified to false", g->lhs);
    if (lhs == 1) conflict = 1; else conflict = !lglcgunit (lgl, -lhs);
  } else {
    if (b == 1) SWAP (int, a, b);
    if (a == 1) {
      if (lhs == b) return 0;
      res = 1;
      LOG (2, "binary and gate with lhs %d simplifies to %d", g->lhs, b);
      if (lhs == -b) conflict = 1; else lglcgmerge (lgl, lhs, b);
    }
  }
  if (res) {
    lgl->cgr->simplified.all++;
    lgl->stats->cgr.simplified.all++;
    lgl->cgr->simplified.and++;
    lgl->stats->cgr.simplified.and++;
  }
  if (conflict) {
    LOG (1,
      "simplifying binary and gate with lhs %d leads to conflict", g->lhs);
    lgl->mt = 1;
    assert (res);
  }
  return res;
}

static int lglsimplrgand (LGL * lgl, Gat * g) {
  int * p, lhs, other, repr, conflict, res, foundfalse, rhs, bit, found;
  AVar * u;
  assert (g->tag == ANDTAG && g->size > 2);
  lhs = lglcgrepr (lgl, g->lhs);
  rhs = foundfalse = 0;
  found = 0;
  for (p = g->cls; !foundfalse && (other = *p); p++) {
    if (other == g->origlhs) { found++; continue; }
    lgl->stats->cgr.csteps++;
    repr = lglcgrepr (lgl, -other);
    if (repr == 1) continue;
    if (repr == -1) { foundfalse = 1; break; }
    if (rhs) rhs = INT_MAX; else { assert (repr); rhs = repr; }
    u = lglavar (lgl, repr);
    bit = (1 << (repr < 0));
    if (u->mark & bit) continue;
    if (u->mark & (bit^3)) { foundfalse = 1; break; }
    u->mark |= bit;
  }
  assert (foundfalse || found);
  for (p = g->cls; (other = *p); p++) {
    if (other == g->origlhs) continue;
    repr = lglcgrepr (lgl, other);
    if (abs (repr) == 1) continue;
    lglavar (lgl, repr)->mark = 0;
  }
  conflict = res = 0;
  if (foundfalse) {
    if (lhs != -1) {
      res = 1;
      LOG (2, "gate with lhs %d simplifies to false", g->lhs);
      if (lhs == 1) conflict = 1; else conflict = !lglcgunit (lgl, -lhs);
    }
  } else if (!rhs) {
    if (lhs != 1) {
      res = 1;
      LOG (2, "large and gate with lhs %d simplifies to true", g->lhs);
      if (lhs == -1) conflict = 1; else conflict = !lglcgunit (lgl, lhs);
    }
  } else if (rhs != INT_MAX) {
    if (lhs != rhs) {
      res = 1;
      LOG (2, "large and gate with lhs %d simplifies to %d", g->lhs, rhs);
      if (lhs == -rhs) conflict = 1; else lglcgmerge (lgl, lhs, rhs);
    }
  }
  if (res) {
    lgl->cgr->simplified.all++;
    lgl->stats->cgr.simplified.all++;
    lgl->cgr->simplified.and++;
    lgl->stats->cgr.simplified.and++;
  }
  if (conflict) {
    LOG (1,
      "simplifying large and gate with lhs %d leads to conflict", g->lhs);
    lgl->mt = 1;
    assert (res);
  }
  return res;
}

static int lglsimpand (LGL * lgl, Gat * g) {
  assert (g->tag == ANDTAG);
  if (g->size == 2) return lglsimpbinand (lgl, g);
  else return lglsimplrgand (lgl, g);
}

static int lglsimpbinxor (LGL * lgl, Gat * g) {
  int res, conflict, lhs, rhs, a, b;
  assert (g->size == 2);
  assert (g->tag == XORTAG);
  lhs = -lglcgrepr (lgl, g->lhs);
  a = lglcgrepr (lgl, g->lits[0]);
  b = lglcgrepr (lgl, g->lits[1]);
  rhs = conflict = res = 0;
  if (a == b) rhs = -1;
  else if (a == -b) rhs = 1;
  else if (a == 1) rhs = -b;
  else if (a == -1) rhs = b;
  else if (b == 1) rhs = -a;
  else if (b == -1) rhs = a;
  else if (lhs == 1) lhs = a, rhs = -b;
  else if (lhs == -1) lhs = a, rhs = b;
  if (rhs && rhs != lhs) res = 1;
  if (res) {
    LOG (2, "simplified binary xor gate with lhs %d", g->lhs);
    assert (rhs);
    assert (lhs != rhs);
    lgl->cgr->simplified.all++;
    lgl->stats->cgr.simplified.all++;
    lgl->cgr->simplified.xor++;
    lgl->stats->cgr.simplified.xor++;
    conflict = lglcgmergelhsrhs (lgl, lhs, rhs);
  }
  if (conflict) {
    LOG (1,
      "simplifying binary xor gate with lhs %d leads to conflict", g->lhs);
    lgl->mt = 1;
    assert (res);
  }
  return res;
}

static int lglsimplrgxor (LGL * lgl, Gat * g) {
  int conflict, lhs, rhs, other, repr, * p, found;
  assert (g->size > 2);
  assert (g->tag == XORTAG);
  lhs = -lglcgrepr (lgl, g->lhs);
  rhs = -1;
  found = 0;
  for (p = g->cls; (other = *p); p++) {
    if (other == g->origlhs) { found++; continue; }
    repr = lglcgrepr (lgl, other);
    if (repr == -1) continue;
    else if (repr == 1) rhs = -rhs;
    else if (repr == rhs) rhs = -1;
    else if (repr == lhs) lhs = -1;
    else if (repr == -lhs) lhs = 1;
    else if (lhs == -1) lhs = repr;
    else if (lhs == 1) lhs = -repr;
    else if (abs (rhs) != 1) return 0;
    else if (rhs == -1) rhs = repr;
    else { assert (rhs == 1); rhs = -repr; }
  }
  assert (found);
  if (lhs == rhs) return 0;
  LOG (2, "simplified large xor gate with lhs %d", g->lhs);
  assert (lhs != rhs);
  lgl->cgr->simplified.all++;
  lgl->stats->cgr.simplified.all++;
  lgl->cgr->simplified.xor++;
  lgl->stats->cgr.simplified.xor++;
  conflict = lglcgmergelhsrhs (lgl, lhs, rhs);
  if (conflict) {
    LOG (1,
      "simplifying large xor gate with lhs %d leads to conflict", g->lhs);
    lgl->mt = 1;
  }
  return 1;
}

static int lglsimpxor (LGL * lgl, Gat * g) {
  assert (g->tag == XORTAG);
  if (g->size == 2) return lglsimpbinxor (lgl, g);
  else return lglsimplrgxor (lgl, g);
}

static int lglsimpite (LGL * lgl, Gat * g) {
  int glhs = g->lhs;
  int lhs = lglcgrepr (lgl, glhs);
  int gc = lglcgrepr (lgl, g->cond);
  int gp = lglcgrepr (lgl, g->pos);
  int gn = lglcgrepr (lgl, g->neg);
  int res, conflict, rhs, gate;
  assert (g->tag == ITETAG);
  assert (g->size == 2);
  gate = rhs = conflict = res = 0;
  if (gc == 1 || gp == gn) res = 1, rhs = gp;
  else if (gc == -1) res = 1, rhs = gn;
  else if (gp == 1 && gn == -1) res = 1, rhs = gc;
  else if (gp == -1 && gn == 1) res = 1, rhs = -gc;
  else if (gn == -1 || gc == gn) {
    // (lhs = gc ? gp : -1) => lhs = gc & gp
    // (lhs = gc ? gp : gc) => lhs = gc & gp
    // COVER ("simplified ite to 'gc & gp'");
    if (gp == 1) res = 1, rhs = gc;
    else if (gp == -1) {
      assert (gn != -1 && gc == gn);
      res = 1, rhs = -1;
    } else {
      res = lglnewbingate (lgl, ANDTAG, glhs, -gc, -gp);
      gate = 1;
    }
  } else if (gp == -1 || gc == -gp) {
    // (lhs = gc ? -1 : gn) => lhs = -gc & gn
    // (lhs = gc ? -gc : gn) => lhs = -gc & gn
    // COVER ("simplified ite to '-gc & gn'");
    assert (gn != -1);
    if (gn == 1) res = 1, rhs = -gc;
    else {
      assert (gn != -1);
      res = lglnewbingate (lgl, ANDTAG, glhs, gc, -gn);
      gate = 1;
    }
  } else if (gp == 1 || gc == gp) {
    // (lhs = gc ? 1 : gn) => lhs = gc | gn
    // (lhs = gc ? gp : gn) => lhs = gc | gn
    // COVER ("simplified ite to 'gc | gn'");
    if (gn == 1) res = 1, rhs = 1;
    else {
      assert (gn != -1);
      res = lglnewbingate (lgl, ANDTAG, -glhs, gc, gn);
      gate = 1;
    }
  } else if (gn == 1 || gc == -gn) {
    // (lhs = gc ? gp : 1) => lhs = gc gp | -gc = gp | -gc
    // (lhs = gc ? gp : -gc) => lhs = gc gp | -gc = gp | -gc
    // COVER ("simplified ite to 'gp | -gc'");
    if (gp == 1) res = 1, rhs = 1;
    else {
      assert (gp != -1);
      res = lglnewbingate (lgl, ANDTAG, -glhs, gp, -gc);
      gate = 1;
    }
  } else if (gp == -gn) {
    // (lhs = gc ? -gn : gn) => lhs = gc&-gn | -gc&gn = gc ^ gn
    // COVER ("simplified ite to 'gc ^ gn'");
    res = lglnewbingate (lgl, XORTAG, -glhs, gc, gn);
    gate = 1;
  }
  if (res && !gate && lhs == rhs) res = 0;
  if (res && (gate || lhs != rhs)) {
    LOG (2, "simplified ite gate with lhs %d", glhs);
    lgl->cgr->simplified.all++;
    lgl->stats->cgr.simplified.all++;
    lgl->cgr->simplified.ite++;
    lgl->stats->cgr.simplified.ite++;
    if (!gate) {
      assert (rhs);
      assert (lhs != rhs);
      if (lhs != rhs) conflict = lglcgmergelhsrhs (lgl, lhs, rhs);
      else res = 0;
    }
  }
  if (conflict) {
    LOG (1, "simplifying ite gate with lhs %d leads to conflict", glhs);
    lgl->mt = 1;
    assert (res);
  }
  return res;
}

static int lglsimpgate (LGL * lgl,  Gat * g) {
  if (g->tag == ANDTAG) return lglsimpand (lgl, g);
  else if (g->tag == ITETAG) return lglsimpite (lgl, g);
  else { assert (g->tag == XORTAG); return lglsimpxor (lgl, g); }
}

static void lglcgrlit (LGL * lgl, int lit) {
  int * p, * q, * l, * r, round;
  Gat * g, * h;
  Stk * goccs;
  round = 0;
RESTART:
  if (lgl->mt) return;
  goccs = lgl->cgr->goccs + abs (lit);
  if (lglmtstk (goccs)) return;
  round++;
  LOG (2, "simplifying gates with lhs %d", lit);
  for (l = goccs->start; !lgl->mt && l < goccs->top; l++) {
    lgl->stats->cgr.csteps++;
    if (lglcgrlimhit (lgl)) return;
    g = lglgidx2gat (lgl, *l);
    if (lglsimpgate (lgl, g)) goto RESTART;// goccs->start moves
  }
  LOG (2,
       "checking congruences of %d gates with rhs %d round %d",
       lglcntstk (goccs), lit, round);
  for (p = goccs->start; p < goccs->top; p++) {
    lgl->stats->cgr.csteps++;
    if (lglcgrlimhit (lgl)) return;
    g = lglgidx2gat (lgl, *p);
    lglsetminrhs (lgl, g);
  }
  SORT (int, goccs->start, lglcntstk (goccs), LGLCMPGOCCS);
  q = goccs->start + 1;
  for (p = q; p < goccs->top; p++) if (*p != q[-1]) *q++ = *p;
  goccs->top = q;
  for (l = goccs->start; !lgl->mt && l < goccs->top; l = r) {
    lgl->stats->cgr.csteps++;
    if (lglcgrlimhit (lgl)) return;
    for (r = l + 1; r < goccs->top && lglgoccsmatchcand (lgl, *l, *r); r++)
      ;
    for (p = l; !lgl->mt && p + 1 < r; p++) {
      g = lglgidx2gat (lgl, *p);
      if (abs (lglcgrepr (lgl, g->lhs)) == 1) continue;
      for (q = p + 1; !lgl->mt && q < r; q++) {
	// if (lglsimpgate (lgl, g)) goto RESTART;// goccs->start moves
	h = lglgidx2gat (lgl, *q);
	if (abs (lglcgrepr (lgl, h->lhs)) == 1) continue;
	if (lglsimpgate (lgl, h)) goto RESTART;// goccs->start moves
	if (lglmatchgate (lgl, lit, g, h)) goto RESTART;// goccs->start moves
      }
    }
  }
}

static void lglclsr (LGL * lgl) {
  int lit;
  while (!lgl->mt &&
	 !lglterminate (lgl) &&
	 (lit = lglwrknext (lgl)) &&
	 !lglcgrlimhit (lgl)) {
    lglcgrlit (lgl, lit);
    if (!lgl->mt && !lglcgrlimhit (lgl)) lglcgrlit (lgl, -lit);
  }

  if (lgl->cgr->simplified.all)
    lglprt (lgl, 2, "[closure-%d] simplified %d gates",
	   lgl->stats->cgr.count, lgl->cgr->simplified.all);
  if (lgl->cgr->simplified.and)
    lglprt (lgl, 2, "[closure-%d] simplified %d and gates",
	    lgl->stats->cgr.count, lgl->cgr->simplified.and);
  if (lgl->cgr->simplified.xor)
    lglprt (lgl, 2, "[closure-%d] simplified %d xor gates",
	    lgl->stats->cgr.count, lgl->cgr->simplified.xor);
  if (lgl->cgr->simplified.ite)
    lglprt (lgl, 2, "[closure-%d] simplified %d ite gates",
	    lgl->stats->cgr.count, lgl->cgr->simplified.ite);

  if (lgl->cgr->matched.all)
    lglprt (lgl, 2, "[closure-%d] matched %d gates",
	    lgl->stats->cgr.count, lgl->cgr->matched.all);
  if (lgl->cgr->matched.and)
    lglprt (lgl, 2, "[closure-%d] matched %d and gates %.0f%%",
	    lgl->stats->cgr.count,
	    lgl->cgr->matched.and, lglpcnt (lgl->cgr->matched.and, lgl->cgr->matched.all));
  if (lgl->cgr->matched.xor)
    lglprt (lgl, 2, "[closure-%d] matched %d xor gates %.0f%%",
	    lgl->stats->cgr.count,
	    lgl->cgr->matched.xor, lglpcnt (lgl->cgr->matched.xor, lgl->cgr->matched.all));
  if (lgl->cgr->matched.ite)
    lglprt (lgl, 2, "[closure-%d] matched %d ite gates %.0f%%",
	    lgl->stats->cgr.count,
	    lgl->cgr->matched.ite, lglpcnt (lgl->cgr->matched.ite, lgl->cgr->matched.all));
}

static int lgladdunits (LGL * lgl) {
  int idx, lit, repr;
  Val val;
  assert (!lgl->mt);
  assert (lglmtstk (&lgl->cgr->units));
  for (idx = 2; idx < lgl->nvars; idx++) {
    repr = lglcgrepr (lgl, idx);
    if (abs (repr) >= 2) continue;
    lit = (repr > 0) ? idx : -idx;
    val = lglval (lgl, lit);
    if (val > 0) continue;
    if (val < 0) {
      LOG (1, "inconsistent congruence closure unit %d", lit);
      lgl->mt = 1;
      return 0;
    }
    LOG (1, "adding congruence closure unit %d", lit);
    lglpushstk (lgl, &lgl->cgr->units, lit);
  }
  return 1;
}

static int lglpropunits (LGL * lgl) {
  int lit;
  Val val;
  assert (!lgl->mt);
  assert (!lgl->level);
  while (!lglmtstk (&lgl->cgr->units)) {
    lit = lglpopstk (&lgl->cgr->units);
    val = lglcval (lgl, lit);
    if (val > 0) continue;
    if (val < 0) {
      LOG (1, "inconsistent congruence closure unit %d", lit);
      lgl->mt = 1;
    } else {
      LOG (1, "assigning congruence closure unit %d", lit);
      lglunitnocheck (lgl, lit);
#if !defined(NLGLPICOSAT) && !defined(NDEBUG)
      {
	int cls[2]; cls[0] = lit; cls[1] = 0;
	lglpicosataddcls (lgl, cls);
      }
#endif
      if (lglbcp (lgl)) continue;
      LOG (1, "conflict after assigning congruence closure unit %d", lit);
      lgl->mt = 1;
    }
  }
  return !lgl->mt;
}

static void lglupdcgrpen (LGL * lgl, int success) {
  if (success && lgl->limits->cgr.pen) lgl->limits->cgr.pen--;
  if (!success && lgl->limits->cgr.pen < MAXPEN) lgl->limits->cgr.pen++;
}

static void lglsetcgrclsrlim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->cgreleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->cgrmineff.val) limit = lgl->opts->cgrmineff.val;
  if (lgl->opts->cgrmaxeff.val >= 0 && limit > lgl->opts->cgrmaxeff.val)
    limit = lgl->opts->cgrmaxeff.val;
  limit >>= (pen = lgl->limits->cgr.pen + lglszpen (lgl));
  irrlim = lgl->stats->irr.lits.cur/2;
  irrlim >>= lgl->limits->simp.pen;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1,
      "[cgrclsr-%d] limit %lld based on %d irredundant literals",
      lgl->stats->cgr.count, (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[cgrclsr-%d] limit %lld penalty %d = %d + %d",
      lgl->stats->cgr.count, (LGLL) limit,
      pen, lgl->limits->cgr.pen, lglszpen (lgl));
  lgl->limits->cgr.esteps = lgl->stats->cgr.esteps + limit;
  lgl->limits->cgr.csteps = lgl->stats->cgr.csteps + 2*limit;
}

static int lglcgrclsr (LGL * lgl) {
  int nvars, oldrem, removed;

  assert (lgl->opts->cgrclsr.val);
  assert (lglsmallirr (lgl));
  assert (!lgl->cgrclosing && !lgl->simp);

  lglstart (lgl, &lgl->times->cgr);

  oldrem = lglrem (lgl);

  lgl->stats->cgr.count++;
  lgl->cgrclosing = lgl->simp = 1;

  NEW (lgl->cgr, 1);

  if (lgl->level > 0) lglbacktrack (lgl, 0);
  lglgc (lgl);

  lglfreezer (lgl);
  lgldense (lgl, 1);

  nvars = lgl->nvars;
  NEW (lgl->repr, nvars);

  lglsetcgrclsrlim (lgl);

  lglcginit (lgl);
  lglgateextract (lgl);
  if (!lgl->mt) lglclsr (lgl);
  lglcgreset (lgl);

  lglsparse (lgl);
  if (lgl->mt) goto DONE;
  if (!lgladdunits (lgl)) { assert (lgl->mt); goto DONE; }
  lglchkred (lgl);
  lgldcpdis (lgl);
  lgldcpcln (lgl);
  lgldcpcon (lgl);
  lglcompact (lgl);
  lglmap (lgl);
  if (lgl->mt) goto DONE;
  if (!lglbcp (lgl)) goto DONE;
  if (!lglpropunits (lgl)) { assert (lgl->mt); goto DONE; }
  lglcount (lgl);
  lglgc (lgl);
  if (lgl->mt) goto DONE;
  if (!lgl->mt) { lglpicosatchkall (lgl); lglpicosatrestart (lgl); }

DONE:

  lglrelstk (lgl, &lgl->cgr->units);
  if (lgl->repr) { assert (lgl->mt); DEL (lgl->repr, nvars); }
  removed = oldrem - lglrem (lgl);
  lglupdcgrpen (lgl, removed);
  DEL (lgl->cgr, 1);
  assert (lgl->simp && lgl->cgrclosing);
  lgl->cgrclosing = lgl->simp = 0;
  lglprtcgrem (lgl);
  lglprt (lgl, 1 + !removed,
    "[cgrclsr-%d] removed %d variables", lgl->stats->cgr.count, removed);
  lglrep (lgl, 1 + !removed, 'C');
  lglstop (lgl);
  return !lgl->mt;
}

static int lglrandomprobe (LGL * lgl, Stk * outer) {
  unsigned pos, mod;
  int res;
  mod = lglcntstk (outer);
  if (!mod) return 0;
  pos = lglrand (lgl) % mod;
  res = lglpeek (outer, pos);
  if (lglval (lgl, res)) return 0;
  assert (res == abs (res));
  return res;
}

static int lglinnerprobe (LGL * lgl, int old,  Stk * outer, Stk * tmp) {
  int i, lit, blit, tag, other, other2, res;
  const int * w, * eow, * p;
  HTS * hts;
  assert (old < lglcntstk (&lgl->trail));
  assert (lglmtstk (tmp));
  for (i = old; i < lglcntstk (&lgl->trail); i++) {
    lit = lglpeek (&lgl->trail, i);
    hts = lglhts (lgl, -lit);
    w = lglhts2wchs (lgl, hts);
    eow = w + hts->count;
    for (p = w; p < eow; p++) {
      blit = *p;
      tag = blit & MASKCS;
      if (tag == TRNCS || tag == LRGCS) p++;
      if (tag == BINCS || tag == LRGCS) continue;
      assert (tag == TRNCS);
      other = blit >> RMSHFT;
      if (lglval (lgl, other) > 0) continue;
      other2 = *p;
      if (lglval (lgl, other2) > 0) continue;
      assert (!lglval (lgl, other));
      assert (!lglval (lgl, other2));
      other = abs (other);
      if (!lglmarked (lgl, other)) {
	lglmark (lgl, other);
	lglpushstk (lgl, tmp, other);
	LOG (4, "potential inner probe %d for %d", other, lit);
      }
      other2 = abs (other2);
      if (!lglmarked (lgl, other2)) {
	lglmark (lgl, other2);
	lglpushstk (lgl, tmp, other2);
	LOG (3, "potential inner probe %d for %d", other2, lit);
      }
    }
  }
  LOG (2, "found %d inner probes in ternary clauses", lglcntstk (tmp));
  res = lglrandomprobe (lgl, tmp);
  lglpopnunmarkstk (lgl, tmp);
  if (!res) res = lglrandomprobe (lgl, outer);
  return res;
}

static void lglcleanrepr (LGL * lgl, Stk * represented, int * repr) {
  int idx;
  while (!lglmtstk (represented)) {
    idx = lglpopstk (represented);
    assert (2 <= idx && idx < lgl->nvars);
    assert (repr[idx]);
    repr[idx] = 0;
  }
}

static void lgladdliftbincls (LGL * lgl, int a, int b) {
  assert (lgl->lifting);
  assert (lglmtstk (&lgl->clause));
  lglpushstk (lgl, &lgl->clause, a);
  lglpushstk (lgl, &lgl->clause, b);
  lglpushstk (lgl, &lgl->clause, 0);
  LOG (2, "lifted binary clause", a, b);
#ifndef NLGLPICOSAT
  lglpicosatchkcls (lgl);
#endif
  lgladdcls (lgl, REDCS, 0, 1);
  lglclnstk (&lgl->clause);
  lgl->stats->lift.impls++;
}

static int64_t lglobalftlim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->lftreleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->lftmineff.val) limit = lgl->opts->lftmineff.val;
  if (lgl->opts->lftmaxeff.val >= 0 && limit > lgl->opts->lftmaxeff.val)
    limit = lgl->opts->lftmaxeff.val;
  limit >>= (pen = lgl->limits->lft.pen + lglszpen (lgl));
  irrlim = lgl->stats->irr.lits.cur/4;
  irrlim >>= lgl->limits->simp.pen;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1,
      "[lift-%d] limit %lld based on %d irredundant literals",
      lgl->stats->lift.count, (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[lift-%d] limit %lld with penalty %d = %d + %d",
      lgl->stats->lift.count, (LGLL) limit,
      pen, lgl->limits->lft.pen, lglszpen (lgl));
  return limit;
}

static void lglupdlftpen (LGL * lgl, int success) {
  if (success && lgl->limits->lft.pen) lgl->limits->lft.pen--;
  if (!success && lgl->limits->lft.pen < MAXPEN) lgl->limits->lft.pen++;
}

static void lglprtlftrem (LGL * lgl) {
  int idx, ret = 0, rem = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lglavar (lgl, idx)->donotlft) ret++; else rem++;
  }
  if (rem)
    lglprt (lgl, 1, "[lift-%d] %d variables remain %.0f%% (%d retained)",
	    lgl->stats->lift.count, rem, lglpcnt (rem, lglrem (lgl)), ret);
  else {
    lglprt (lgl, 1, "[lift-%d] fully completed lifting",
            lgl->stats->lift.count);
    for (idx = 2; idx < lgl->nvars; idx++)
      lglavar (lgl, idx)->donotlft = 0;
  }
}

static int lgliftaux (LGL * lgl) {
  int lit1, lit2, repr1, repr2, orepr1, orepr2, tobeprobed, notobeprobed;
  int i, idx, lit, * reprs[3], first, outer, inner, changed, branch;
  Stk probes, represented[2], saved, tmp;
  int ok, oldouter, dom, repr, other;
  unsigned pos, delta, mod;
  Val val, val1, val2;
  int64_t global;
#ifndef NDEBUG
  int oldinner;
#endif
  assert (lgl->simp && lgl->lifting && !lgl->level);
  NEW (lgl->repr, lgl->nvars);
  CLR (probes); CLR (saved); CLR (tmp);
  CLR (represented[0]); CLR (represented[1]);
  NEW (reprs[0], lgl->nvars);
  NEW (reprs[1], lgl->nvars);
  NEW (reprs[2], lgl->nvars);
  global = lgl->stats->visits.simp + lglobalftlim (lgl);
  tobeprobed = notobeprobed = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lglavar (lgl, idx)->donotlft) notobeprobed++;
    else tobeprobed++;
  }
  if (!tobeprobed) {
    for (idx = 2; idx < lgl->nvars; idx++) {
      if (!lglisfree (lgl, idx)) continue;
      lglavar (lgl, idx)->donotlft = 0;
      tobeprobed++;
    }
    lglprt (lgl, 1, "[lift-%d] using all %d probes",
            lgl->stats->lift.count, tobeprobed);
  } else lglprt (lgl, 1, "[lift-%d] using %d probes %.0f%%",
                 lgl->stats->lift.count, tobeprobed,
		 lglpcnt (tobeprobed, notobeprobed + tobeprobed));
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lglavar (lgl, idx)->donotlft) continue;
    LOG (1, "new outer probe %d", idx);
    lglpushstk (lgl, &probes, idx);
  }
  mod = lglcntstk (&probes);
  assert (mod == tobeprobed);
  if (!(mod)) goto DONE;
  LOG (1, "found %u active outer probes out of %d variables %.1f%%",
       mod, lgl->nvars - 1, lglpcnt (mod, lgl->nvars-1));
  pos = lglrand (lgl)  % mod;
  delta = lglrand (lgl) % mod;
  if (!delta) delta++;
  while (lglgcd (delta, mod) > 1)
    if (++delta == mod) delta = 1;
  LOG (1, "lifting start %u delta %u mod %u", pos, delta, mod);
  changed = first = 0;
  assert (lgl->simp);
  while (!lgl->mt) {
    if (lgl->stats->visits.simp >= global) break;
    if (lglterminate (lgl)) break;
    if (!lglsyncunits (lgl)) break;
    assert (pos < (unsigned) mod);
    outer = probes.start[pos];
    lglavar (lgl, outer)->donotlft = 1;
    if (outer == first) { if (changed) changed = 0; else break; }
    if (!first) first = outer;
    pos += delta;
    if (pos >= mod) pos -= mod;
    if (lglval (lgl, outer)) continue;
    lgl->stats->lift.probed0++;
    LOG (2, "1st outer branch %d during lifting", outer);
    oldouter = lglcntstk (&lgl->trail);
    lgliassume (lgl, outer);
    ok = lglbcp (lgl);
    if (!ok) {
FIRST_OUTER_BRANCH_FAILED:
      dom = lglprbana (lgl, outer);
      LOG (1, "1st outer branch failed literal %d during lifting", outer);
      lgl->stats->lift.units++;
      lglbacktrack (lgl, 0);
      lglunit (lgl, -dom);
      if (lglbcp (lgl)) continue;
      LOG (1, "empty clause after propagating outer probe during lifting");
      assert (!lgl->mt);
      lgl->mt = 1;
      break;
    }
    inner = lglinnerprobe (lgl, oldouter, &probes, &tmp);
    assert (lglmtstk (&represented[0]));
    if (!inner) {
FIRST_OUTER_BRANCH_WIHOUT_INNER_PROBE:
      LOG (2, "no inner probe for 1st outer probe %d", outer);
      for (i = oldouter; i < lglcntstk (&lgl->trail); i++) {
	lit = lglpeek (&lgl->trail, i);
	idx = abs (lit);
	assert (!reprs[0][idx]);
	reprs[0][idx] = lglsgn (lit);
	lglpushstk (lgl, &represented[0], idx);
      }
      assert (lgl->level == 1);
      goto END_OF_FIRST_OUTER_BRANCH;
    }
#ifndef NDEBUG
    oldinner = lglcntstk (&lgl->trail);
#endif
    LOG (2, "1st inner branch %d in outer 1st branch %d", inner, outer);
    lgl->stats->lift.probed1++;
    lgliassume (lgl, inner);
    ok = lglbcp (lgl);
    if (!ok) {
      LOG (1, "1st inner branch failed literal %d on 1st outer branch %d",
	  inner, outer);
      lglbacktrack (lgl, 1);
      assert (lglcntstk (&lgl->trail) == oldinner);
      lgladdliftbincls (lgl, -inner, -outer);
      assert (lglcntstk (&lgl->trail) == oldinner + 1);
      ok = lglbcp (lgl);
      if (ok) goto FIRST_OUTER_BRANCH_WIHOUT_INNER_PROBE;
      LOG (1, "conflict after propagating negation of 1st inner branch");
      goto FIRST_OUTER_BRANCH_FAILED;
    }
    lglclnstk (&saved);
    for (i = oldouter; i < lglcntstk (&lgl->trail); i++)
      lglpushstk (lgl, &saved, lglpeek (&lgl->trail, i));
    LOG (3, "saved %d assignments of 1st inner branch %d in 1st outer branch",
	 lglcntstk (&saved), inner, outer);
    lglbacktrack (lgl, 1);
    assert (lglcntstk (&lgl->trail) == oldinner);
    LOG (2, "2nd inner branch %d in 1st outer branch %d", -inner, outer);
    lgl->stats->lift.probed1++;
    lgliassume (lgl, -inner);
    ok = lglbcp (lgl);
    if (!ok) {
      LOG (2, "2nd inner branch failed literal %d on 1st outer branch %d",
	   -inner, outer);
      lglbacktrack (lgl, 1);
      assert (lglcntstk (&lgl->trail) == oldinner);
      lgladdliftbincls (lgl, inner, -outer);
      assert (lglcntstk (&lgl->trail) == oldinner + 1);
      ok = lglbcp (lgl);
      if (ok) goto FIRST_OUTER_BRANCH_WIHOUT_INNER_PROBE;
      LOG (1, "conflict after propagating negation of 2nd inner branch");
      goto FIRST_OUTER_BRANCH_FAILED;
    }
    while (!lglmtstk (&saved)) {
      lit = lglpopstk (&saved);
      idx = abs (lit);
      val1 = lglsgn (lit);
      val2 = lglval (lgl, idx);
      if (val1 == val2) {
	assert (!reprs[0][idx]);
	reprs[0][idx] = val1;
	lglpushstk (lgl, &represented[0], idx);
      } else if (lit != inner && val1 == -val2) {
	assert (lit != -inner);
	repr = lglptrjmp (reprs[0], lgl->nvars-1, inner);
	other = lglptrjmp (reprs[0], lgl->nvars-1, lit);
	if (lglcmprepr (lgl, other, repr) < 0) SWAP (int, repr, other);
	if (other < 0) other = -other, repr = -repr;
	assert (!reprs[0][other]);
	reprs[0][other] = repr;
	lglpushstk (lgl, &represented[0], other);
      } else assert (lit == inner || !val2);
    }
    lglbacktrack (lgl, 1);
END_OF_FIRST_OUTER_BRANCH:
    assert (lgl->level == 1);
#ifndef NLGLOG
    {
      LOG (1, "start of 1st outer branch %d equivalences:", outer);
      for (i = 0; i < lglcntstk (&represented[0]); i++) {
	other = lglpeek (&represented[0], i);
	repr = reprs[0][other];
	LOG (1, "  1st branch equivalence %d : %d = %d", i + 1, other, repr);
      }
      LOG (1, "end of 1st outer branch %d equivalences.", outer);
    }
#endif
    lglbacktrack (lgl, 0);
    assert (lglcntstk (&lgl->trail) == oldouter);
    lgl->stats->lift.probed0++;
    LOG (2, "2nd outer branch %d during lifting", -outer);
    lgliassume (lgl, -outer);
    ok = lglbcp (lgl);
    if (!ok) {
SECOND_OUTER_BRANCH_FAILED:
      dom = lglprbana (lgl, -outer);
      LOG (1, "2nd branch outer failed literal %d during lifting", -outer);
      lgl->stats->lift.units++;
      lglbacktrack (lgl, 0);
      lglunit (lgl, -dom);
      if (lglbcp (lgl)) goto CONTINUE;
      assert (!lgl->mt);
      lgl->mt = 1;
      goto CONTINUE;
    }
    assert (lglmtstk (&represented[1]));
#ifndef NDEBUG
    oldinner = lglcntstk (&lgl->trail);
#endif
    if (!inner || lglval (lgl, inner))
      inner = lglinnerprobe (lgl, oldouter, &probes, &tmp);
    if (!inner) {
SECOND_OUTER_BRANCH_WIHOUT_INNER_PROBE:
      LOG (2, "no inner probe for 2nd outer branch %d", -outer);
      for (i = oldouter; i < lglcntstk (&lgl->trail); i++) {
	lit = lglpeek (&lgl->trail, i);
	idx = abs (lit);
	assert (!reprs[1][idx]);
	reprs[1][idx] = lglsgn (lit);
	lglpushstk (lgl, &represented[1], idx);
      }
      assert (lgl->level == 1);
      goto END_OF_SECOND_BRANCH;
    }
    LOG (2, "1st inner branch %d in outer 2nd branch %d", inner, -outer);
    lgl->stats->lift.probed1++;
    lgliassume (lgl, inner);
    ok = lglbcp (lgl);
    if (!ok) {
      LOG (1, "1st inner branch failed literal %d on 2nd outer branch %d",
	   inner, -outer);
      lglbacktrack (lgl, 1);
      assert (lglcntstk (&lgl->trail) == oldinner);
      lgladdliftbincls (lgl, -inner, outer);
      assert (lglcntstk (&lgl->trail) == oldinner + 1);
      ok = lglbcp (lgl);
      if (ok) goto SECOND_OUTER_BRANCH_WIHOUT_INNER_PROBE;
      LOG (1, "conflict after propagating negation of 1st inner branch");
      goto SECOND_OUTER_BRANCH_FAILED;
    }
    lglclnstk (&saved);
    for (i = oldouter; i < lglcntstk (&lgl->trail); i++)
      lglpushstk (lgl, &saved, lglpeek (&lgl->trail, i));
    LOG (3,
	 "saved %d assignments of 1st inner branch %d in 2nd outer branch %d",
	 lglcntstk (&saved), inner, -outer);
    lglbacktrack (lgl, 1);
    assert (lglcntstk (&lgl->trail) == oldinner);
    LOG (2, "2nd inner branch %d in 2nd outer branch %d", -inner, -outer);
    lgl->stats->lift.probed1++;
    lgliassume (lgl, -inner);
    ok = lglbcp (lgl);
    if (!ok) {
      LOG (1, "2nd inner branch failed literal %d on 2nd outer branch %d",
	   -inner, -outer);
      lglbacktrack (lgl, 1);
      assert (lglcntstk (&lgl->trail) == oldinner);
      lgladdliftbincls (lgl, inner, outer);
      assert (lglcntstk (&lgl->trail) == oldinner + 1);
      ok = lglbcp (lgl);
      if (ok) goto SECOND_OUTER_BRANCH_WIHOUT_INNER_PROBE;
      LOG (1, "conflict after propagating negation of 2nd inner branch");
      goto SECOND_OUTER_BRANCH_FAILED;
    }
    while (!lglmtstk (&saved)) {
      lit = lglpopstk (&saved);
      idx = abs (lit);
      val1 = lglsgn (lit);
      val2 = lglval (lgl, idx);
      if (val1 == val2) {
	assert (!reprs[1][idx]);
	reprs[1][idx] = val1;
	lglpushstk (lgl, &represented[1], idx);
      } else if (lit != inner && val1 == -val2) {
	assert (lit != -inner);
	repr = lglptrjmp (reprs[1], lgl->nvars-1, inner);
	other = lglptrjmp (reprs[1], lgl->nvars-1, lit);
	if (lglcmprepr (lgl, other, repr) < 0) SWAP (int, repr, other);
	if (other < 0) other = -other, repr = -repr;
	assert (!reprs[1][other]);
	reprs[1][other] = repr;
	lglpushstk (lgl, &represented[1], other);
      } else assert (lit == inner || !val2);
    }
    lglbacktrack (lgl, 1);
END_OF_SECOND_BRANCH:
    assert (lgl->level == 1);
#ifndef NLGLOG
    {
      LOG (1, "start of 2nd outer branch %d equivalences:", -outer);
      for (i = 0; i < lglcntstk (&represented[1]); i++) {
	other = lglpeek (&represented[1], i);
	repr = reprs[1][other];
	LOG (1, "  2nd branch equivalence %d : %d = %d", i + 1, other, repr);
      }
      LOG (1, "end of 2nd outer branch %d equivalences.", outer);
    }
#endif
    lglbacktrack (lgl, 0);
    for (branch = 0; branch <= 1; branch++) {
      assert (lglptrjmp (reprs[!branch], lgl->nvars-1, 1) == 1);
      assert (lglptrjmp (reprs[!branch], lgl->nvars-1, -1) == -1);
      for (i = 0; i < lglcntstk (&represented[branch]); i++) {
	lit1 = lglpeek (&represented[branch], i);
	assert (2 <= lit1 && lit1 < lgl->nvars);
	lit2 = reprs[branch][lit1];
	assert (lit2);
	if (abs (lit2) == 1) {
	  val = lglval (lgl, lit1);
	  assert (!val || val == lit2);
	  if (val) continue;
	  repr1 = lglptrjmp (reprs[!branch], lgl->nvars-1, lit1);
	  if (repr1 != lit2) continue;
	  LOG (1, "  common constant equivalence : %d = %d  (branch %d)",
	       lit1, lit2, branch);
	  lglunit (lgl, lit2*lit1);
	  lgl->stats->lift.units++;
	} else {
	  repr1 = lglptrjmp (reprs[2], lgl->nvars-1, lit1);
	  repr2 = lglptrjmp (reprs[2], lgl->nvars-1, lit2);
	  if (repr1 == repr2) continue;
	  orepr1 = lglptrjmp (reprs[!branch], lgl->nvars-1, lit1);
	  orepr2 = lglptrjmp (reprs[!branch], lgl->nvars-1, lit2);
	  if (orepr1 != orepr2) continue;
	  assert (abs (repr1) > 1 && abs (repr2) > 1);
	  if (lglcmprepr (lgl, repr2, repr1) < 0) SWAP (int, repr1, repr2);
	  if (repr2 < 0) repr2 = -repr2, repr1 = -repr1;
	  LOG (2, "  common equivalence candidate : %d = %d   (branch %d)",
	       repr2, repr1, branch);
	  reprs[2][repr2] = repr1;
	}
      }
    }
    if (!lglbcp (lgl)) lgl->mt = 1;
CONTINUE:
    assert (!lgl->level);
    lglcleanrepr (lgl, &represented[0], reprs[0]);
    lglcleanrepr (lgl, &represented[1], reprs[1]);
  }
  if (!lgl->mt) {
    for (idx = 2; idx < lgl->nvars; idx++)
      (void) lglptrjmp (reprs[2], lgl->nvars-1, idx);
    for (idx = 2; idx < lgl->nvars; idx++) {
      repr = lglptrjmp (reprs[2], lgl->nvars-1, idx);
      val = lglval (lgl, idx);
      if (!val) continue;
      if (repr == -val) {
	LOG (1, "inconsistent assigned members of equivalence classe");
	lgl->mt = 1;
	goto DONE;
      }
      if (repr < 0) repr = -repr, val = -val;
      if (repr == 1) { assert (val == 1); continue; }
      reprs[2][repr] = val;
    }
    for (idx = 2; idx < lgl->nvars; idx++) {
      repr = lglptrjmp (reprs[2], lgl->nvars-1, idx);
      assert (repr);
      assert (repr != -idx);
      if (repr == idx) continue;
      if (abs (repr) == 1) continue;
      lgl->stats->lift.eqs++;
      LOG (1, "  real common equivalence : %d = %d", idx, repr);
      lglimerge (lgl, idx, repr);
    }
  }
DONE:
  assert (!lgl->level);
  DEL (reprs[0], lgl->nvars);
  DEL (reprs[1], lgl->nvars);
  DEL (reprs[2], lgl->nvars);
  lglrelstk (lgl, &probes);
  lglrelstk (lgl, &represented[0]);
  lglrelstk (lgl, &represented[1]);
  lglrelstk (lgl, &saved);
  lglrelstk (lgl, &tmp);
  if (lgl->mt) DEL (lgl->repr, lgl->nvars);
  return !lgl->mt;
}

static int lglift (LGL * lgl) {
  int oldrem = lglrem (lgl), removed;
  assert (lgl->opts->lift.val);
  lglstart (lgl, &lgl->times->lft);
  assert (!lgl->lifting);
  lgl->lifting = 1;
  lgl->stats->lift.count++;
  assert (!lgl->simp);
  lgl->simp = 1;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  if (!lglbcp (lgl)) goto DONE;
  lglgc (lgl);
  if (lgl->mt) goto DONE;
  if (!lgliftaux (lgl)) { assert (lgl->mt); goto DONE; }
  if (!lglsynceqs (lgl)) { assert (lgl->mt); goto DONE; }
  lglchkred (lgl);
  lgldcpdis (lgl);
  lgldcpcln (lgl);
  lgldcpcon (lgl);
  lglcompact (lgl);
  lglmap (lgl);
  if (lgl->mt) goto DONE;
  if (!lglbcp (lgl)) goto DONE;
  lglcount (lgl);
  lglgc (lgl);
  if (lgl->mt) goto DONE;
  if (!lgl->mt) { lglpicosatchkall (lgl); lglpicosatrestart (lgl); }
DONE:
  removed = oldrem - lglrem (lgl);
  lglupdlftpen (lgl, removed);
  assert (lgl->lifting && lgl->simp);
  lgl->lifting = lgl->simp = 0;
  lglprtlftrem (lgl);
  lglprt (lgl, 1 + !removed,
    "[lift-%d] removed %d variables", lgl->stats->lift.count, removed);
  lglrep (lgl, 1 + !removed, '^');
  lglstop (lgl);
  return !lgl->mt;
}

static int lgldstpull (LGL * lgl, int lit) {
  AVar * av;
  av = lglavar (lgl, lit);
  assert ((lit > 0) == av->wasfalse);
  if (av->mark) return 0;
  if (!lglevel (lgl, lit)) return 0;
  av->mark = 1;
  if (lgldecision (lgl, lit)) {
    lglpushstk (lgl, &lgl->clause, lit);
    LOG (3, "added %d to learned clause", lit);
  } else {
    lglpushstk (lgl, &lgl->seen, -lit);
    LOG (3, "pulled in distillation literal %d", -lit);
  }
  return 1;
}

static int lglanalit (LGL * lgl, int lit) {
  int r0, r1, antecedents, other, next, tag, * p, * rsn;
  AVar * av;
  assert (lglmtstk (&lgl->seen));
  assert (lglmtstk (&lgl->clause));
  antecedents = 1;
  av = lglavar (lgl, lit);
  rsn = lglrsn (lgl, lit);
  r0 = rsn[0], r1 = rsn[1];
  LOGREASON (2, lit, r0, r1, "starting literal analysis for %d with", lit);
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
    } else if (tag == UNITCS) assert (!lglevel (lgl, lit));
    else if (tag == DECISION) assert (lglavar (lgl, lit)->assumed);
    else {
      assert (tag == LRGCS);
      for (p = lglidx2lits (lgl, LRGCS, (r0 & REDCS), r1); (other = *p); p++)
	if (other != lit) lgldstpull (lgl, *p);
    }
    if (next == lglcntstk (&lgl->seen)) break;
    lit = lglpeek (&lgl->seen, next++);
    assert ((lit < 0) == lglavar (lgl, lit)->wasfalse);
    rsn = lglrsn (lgl, lit);
    r0 = rsn[0], r1 = rsn[1];
    LOGREASON (2, lit, r0, r1, "literal analysis of");
    antecedents++;
  }
  lglpopnunmarkstk (lgl, &lgl->seen);
  LOG (2, "literal analysis used %d antecedents", antecedents);
  assert (lglcntstk (&lgl->clause) >= 1);
  return antecedents;
}

static int lglfailedass (LGL * lgl) {
  assert (lgl->level >= lgl->alevel);
  return lgl->level == lgl->alevel && lgl->failed;
}

static void lglanafailed (LGL * lgl) {
  int ilit, elit, erepr, failed, size;
  unsigned bit, rbit, ibit, count;
  Ext * ext, * rext;
  const int * p;
  AVar * av;
  assert (lgl->mt || lglfailedass (lgl));
  if (lgl->mt) {
    LOG (1, "no failed assumptions since CNF unconditionally inconsistent");
  } else if ((failed = lgl->failed) == -1) {
    assert (!lgl->level);
    elit = 0;
    for (p = lgl->eassume.start; !elit && p < lgl->eassume.top; p++) {
      erepr = lglerepr (lgl, *p);
      if (lglederef (lgl, erepr) < 0) elit = *p;
    }
    assert (elit);
    LOG (1, "found single external failed assumption %d", elit);
    ext = lglelit2ext (lgl, elit);
    assert (!ext->failed);
    bit = 1u << (elit < 0);
    assert (ext->assumed & bit);
    ext->failed |= bit;
  } else {
    assert (abs (failed) > 1);
    if ((av = lglavar (lgl, failed))->assumed == 3) {
      LOG (1, "inconsistent internal assumptions %d and %d", failed, -failed);
      assert (!av->failed);
      av->failed = 3;
    } else {
      lglanalit (lgl, -failed);
      for (p = lgl->clause.start; p < lgl->clause.top; p++) {
	ilit = *p;
	av = lglavar (lgl, ilit);
	bit = (1u << (ilit > 0));
	assert (av->assumed & bit);
	assert (!(av->failed & bit));
	av->failed |= bit;
      }
      size = lglcntstk (&lgl->clause);
      assert (size > 0);
      lglpushstk (lgl, &lgl->clause, 0);
      lglprt (lgl, 2,
	 "[analyze-final] learned clause with size %d out of %d",
	 size, lglcntstk (&lgl->eassume));
      LOGCLS (2, lgl->clause.start, "failed assumption clause");
      lgladdcls (lgl, REDCS, size, 0);
      lglpopstk (&lgl->clause);
      lglpopnunmarkstk (lgl, &lgl->clause);
    }
    count = 0;
    for (p = lgl->eassume.start; p < lgl->eassume.top; p++) {
      elit =  *p;
      bit = 1u << (elit < 0);
      ext = lglelit2ext (lgl, elit);
      assert (!ext->eliminated && !ext->blocking);
      assert (ext->assumed & bit);
      if (ext->failed & bit) continue;
      if (ext->equiv) {
	erepr = ext->repr;
	rbit = bit;
	if (erepr < 0) rbit ^= 3;
	if (elit < 0) erepr = -erepr;
	rext = lglelit2ext (lgl, erepr);
	assert (!rext->equiv);
	if (rext->failed & rbit) continue;
	ilit = rext->repr;
	ibit = rbit;
	if (ilit < 0) ilit = -ilit, ibit ^= 3;
	if (ilit == 1) continue;
	assert (ilit && ilit != -1);
	av = lglavar (lgl, ilit);
	if (!(av->failed & ibit)) continue;
	rext->failed |= rbit;
	count++;
	if (rext->assumed & rbit) {
	  LOG (2,
	       "found representative external failed assumption %d",
	       erepr);
	} else {
	  LOG (2,
	       "found non representative external failed assumption %d",
	       elit);
	  ext->failed |= bit;
	}
      } else {
	ilit = ext->repr;
	ibit = bit;
	if (ilit < 0) ilit = -ilit, ibit ^= 3;
	if (ilit == 1) continue;
	assert (ilit && ilit != -1);
	av = lglavar (lgl, ilit);
	if (!(av->failed & ibit)) continue;
	LOG (2, "found external failed assumption %d", elit);
	ext->failed |= bit;
	count++;
      }
    }
    LOG (1, "found %u external failed assumptions", count);
  }
  TRANS (FAILED);
}

static void lglternreslit (LGL * lgl, int lit) {
  int * pw, * peow, * nw, * neow, * p, * n;
  int pblit, ptag, pother, pother2, pdelta;
  int nblit, ntag, nother, nother2, ndelta;
  HTS * phts, * nhts;
  int a, b, c;

  phts = lglhts (lgl, lit);
  pw = lglhts2wchs (lgl, phts);
  peow = pw + phts->count;
  nhts = lglhts (lgl, -lit);
  nw = lglhts2wchs (lgl, nhts);
  neow = nw + nhts->count;
  for (n = nw; n < neow; n++) {
    if (++lgl->stats->trnr.steps >= lgl->limits->trnr.steps) return;
    nblit = *n;
    ntag = nblit & MASKCS;
    if (ntag == BINCS || ntag == OCCS) continue;
    if (ntag == TRNCS) break;
    assert (ntag == LRGCS);
    n++;
  }
  if (n >= neow) return;
  for (p = pw;
       p < peow && lgl->stats->trnr.steps < lgl->limits->trnr.steps;
       p++) {
    lgl->stats->trnr.steps++;
    pblit = *p;
    ptag = pblit & MASKCS;
    if (ptag == BINCS || ptag == OCCS) continue;
    if (ptag == TRNCS || ptag == LRGCS) p++;
    if (ptag == LRGCS) continue;
    assert (ptag == TRNCS);
    pother = pblit >> RMSHFT;
    if (lglval (lgl, pother)) continue;
    pother2 = *p;
    if (lglval (lgl, pother2)) continue;
    for (n = nw;
	 n < neow && lgl->stats->trnr.steps < lgl->limits->trnr.steps;
	 n++) {
      lgl->stats->trnr.steps++;
      nblit = *n;
      ntag = nblit & MASKCS;
      if (ntag == BINCS || ntag == OCCS) continue;
      if (ntag == TRNCS || ntag == LRGCS) n++;
      if (ntag == LRGCS) continue;
      assert (ntag == TRNCS);
      nother = nblit >> RMSHFT;
      if (lglval (lgl, nother)) continue;
      nother2 = *n;
      if (lglval (lgl, nother2)) continue;
      if ((nother == pother && nother2 == pother2) ||
	  (nother == pother2 && nother2 == pother)) {
	a = nother, b = nother2;
	if (lglhasbin (lgl, a, b)) continue;
	lgl->stats->trnr.bin++;
	LOG (2, "ternary resolvent %d %d", a, b);
#ifndef NLGLPICOSAT
	lglpicosatchkclsarg (lgl, a, b, 0);
#endif
	lglwchbin (lgl, a, b, REDCS);
	lglwchbin (lgl, b, a, REDCS);
	lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
	lglwrktouch (lgl, a);
	lglwrktouch (lgl, b);
      } else {
	a = nother, b = nother2;
	if (nother == pother || nother2 == pother) c = pother2;
	else if (nother == pother2 || nother2 == pother2) c = pother;
	else continue;
	assert (a != b && b != c && a != c);
	assert (a != -b);
	if (a == -c || b == -c) continue;
	if (lglhastrn (lgl, a, b, c)) continue;
	lgl->stats->trnr.trn++;
	LOG (2, "ternary resolvent %d %d %d", a, b, c);
#ifndef NLGLPICOSAT
	lglpicosatchkclsarg (lgl, a, b, c, 0);
#endif
	lglwchtrn (lgl, a, b, c, REDCS);
	lglwchtrn (lgl, b, a, c, REDCS);
	lglwchtrn (lgl, c, a, b, REDCS);
	lgl->stats->red.trn++, assert (lgl->stats->red.trn > 0);
	lglwrktouch (lgl, a);
	lglwrktouch (lgl, b);
	lglwrktouch (lgl, c);
      }
      pdelta = p - pw;
      phts = lglhts (lgl, lit);
      pw = lglhts2wchs (lgl, phts);
      peow = pw + phts->count;
      p = pw + pdelta;
      ndelta = n - nw;
      nhts = lglhts (lgl, -lit);
      nw = lglhts2wchs (lgl, nhts);
      neow = nw + nhts->count;
      n = nw + ndelta;
    }
  }
}

static void lglternresidx (LGL * lgl, int idx) {
  lglternreslit (lgl, idx);
  lglternreslit (lgl, -idx);
}

static void lglseternreslim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  if (lgl->opts->ternresrtc.val) {
    limit = LLMAX;
    lglprt (lgl, 1, "[ternres-%d] no limit (run to completion)",
	    lgl->stats->trnr.count);
  } else {
    limit = (lgl->opts->trnreleff.val*lgl->stats->visits.search)/1000;
    if (limit < lgl->opts->trnrmineff.val) limit = lgl->opts->trnrmineff.val;
    if (lgl->opts->trnrmaxeff.val >= 0 && limit > lgl->opts->trnrmaxeff.val)
      limit = lgl->opts->trnrmaxeff.val;
    limit >>= (pen = lgl->limits->trnr.pen + lglszpen (lgl));
    irrlim = 4*lgl->stats->irr.lits.cur;
    irrlim >>= lgl->limits->simp.pen;
    if (limit < irrlim) {
      limit = irrlim;
      lglprt (lgl, 1,
        "[ternres-%d] limit %lld based on %d irredundant literals",
	lgl->stats->trnr.count, (LGLL) limit, lgl->stats->irr.lits.cur);
    } else
      lglprt (lgl, 1, "[ternres-%d] limit %lld with penalty %d = %d + %d",
	lgl->stats->trnr.count, (LGLL) limit,
	pen, lgl->limits->trnr.pen, lglszpen (lgl));
  }
  lgl->limits->trnr.steps = lgl->stats->trnr.steps + limit;
}

static void lglupdternrespen (LGL * lgl, int success) {
  if (success && lgl->limits->trnr.pen) lgl->limits->trnr.pen--;
  if (!success && lgl->limits->trnr.pen < MAXPEN) lgl->limits->trnr.pen++;
}

static void lglprternresrem (LGL * lgl) {
  int idx, ret = 0, rem = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lglavar (lgl, idx)->donoternres) ret++; else rem++;
  }
  if (rem)
    lglprt (lgl, 1, "[ternres-%d] %d variables remain %.0f%% (%d retained)",
	    lgl->stats->trnr.count, rem, lglpcnt (rem, lglrem (lgl)), ret);
  else {
    lglprt (lgl, 1, "[ternres-%d] fully completed ternary resolution",
	    lgl->stats->trnr.count);
    for (idx = 2; idx < lgl->nvars; idx++)
      lglavar (lgl, idx)->donoternres = 0;
  }
}

static void lglternresinit (LGL * lgl) {
  int idx, schedulable = 0, donoternres = 0;
  lglwrkinit (lgl, 1, 1);
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (!lglisfree (lgl, idx)) continue;
    if (lglavar (lgl, idx)->donoternres) donoternres++;
    else schedulable++;
  }
  if (!schedulable) {
    donoternres = 0;
    for (idx = 2; idx < lgl->nvars; idx++) {
      if (!lglisfree (lgl, idx)) continue;
      lglavar (lgl, idx)->donoternres = 0;
      schedulable++;
    }
  }
  if (!donoternres)
    lglprt (lgl, 1, "[ternres-%d] all %d free variables schedulable",
            lgl->stats->trnr.count, schedulable);
  else
    lglprt (lgl, 1,
      "[ternres-%d] %d schedulable variables %.0f%%",
      lgl->stats->trnr.count, schedulable, lglpcnt (schedulable, lgl->nvars-2));
  assert (!lgl->donotsched), lgl->donotsched = 1;
  lglrandidxtrav (lgl, lglwrktouch);
  assert (lgl->donotsched), lgl->donotsched = 0;
}

static int lglternres (LGL * lgl) {
  int before, after, delta;
  int before2, after2, delta2;
  int before3, after3, delta3;
  int success, lit;
  if (lgl->nvars <= 2) return 1;
  lglstart (lgl, &lgl->times->trn);
  ASSERT (!lgl->simp && !lgl->ternresing);
  lgl->simp = lgl->ternresing = 1;
  lgl->stats->trnr.count++;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  lglseternreslim (lgl);

  lglternresinit (lgl);

  before2 = lgl->stats->trnr.bin;
  before3 = lgl->stats->trnr.trn;
  while (lgl->stats->trnr.steps < lgl->limits->trnr.steps) {
    if (lglterminate (lgl)) break;
    if (!lglsyncunits (lgl)) break;
    if (!(lit = lglwrknext (lgl))) {
      lglprt (lgl, 2,  "[ternres-%d] saturated", lgl->stats->trnr.count);
      break;
    }
    lgl->stats->trnr.steps++;
    assert (lit > 0);
    if (!lglisfree (lgl, lit)) continue;
    lglavar (lgl, lit)->donoternres = 1;
    lglternresidx (lgl, lit);
  }
  after2 = lgl->stats->trnr.bin;
  after3 = lgl->stats->trnr.trn;
  after = after2 + after3;
  before = before2 + before3;
  delta2 = after2 - before2;
  delta3 = after3 - before3;
  delta = after - before;
  success = before < after;
  lglprt (lgl, 1, "[ternres-%d] %d ternary resolvents (%d bin, %d trn)",
          lgl->stats->trnr.count, delta, delta2, delta3);
  lglupdternrespen (lgl, success);
  assert (lgl->simp && lgl->ternresing);
  lgl->simp = lgl->ternresing = 0;
  lglprternresrem (lgl);
  lglrep (lgl, 1 + !success, 'T');
  lglwrkreset (lgl);
  lglstop (lgl);
  return !lgl->mt;
}

static int lgltrdbin (LGL * lgl, int start, int target, int irr) {
  int lit, next, blit, tag, red, other, * p, * w, * eow, res, ign, val;
  HTS * hts;
  assert (lglmtstk (&lgl->seen));
  assert (abs (start) < abs (target));
  LOG (2, "trying transitive reduction of %s binary clause %d %d",
       lglred2str (irr^REDCS), start, target);
  lgl->stats->trd.bins++;
  lglpushnmarkseen (lgl, -start);
  next = 0;
  res = 0;
  ign = 1;
  while (next < lglcntstk (&lgl->seen)) {
    lit = lglpeek (&lgl->seen, next++);
    lgl->stats->trd.steps++;
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
	lgl->stats->trd.failed++;
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
  lgl->stats->trd.lits++;
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w;
       p < eow && (lgl->stats->trd.steps < lgl->limits->trd.steps);
       p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag != BINCS) continue;
    target = blit >> RMSHFT;
    if (abs (start) > abs (target)) continue;
    red = blit & REDCS;
    val = lgltrdbin (lgl, start, target, red^REDCS);
    if (!val) continue;
    if (val < 0) { assert (lgl->mt || lgl->unassigned < unassigned); break; }
    LOG (2, "removing transitive redundant %s binary clause %d %d",
	 lglred2str (red), start, target);
    lgl->stats->trd.red++;
    lgl->stats->prgss++;
    lglrmbwch (lgl, start, target, red);
    lglrmbwch (lgl, target, start, red);
    assert (!lgl->dense);
    if (red) assert (lgl->stats->red.bin > 0), lgl->stats->red.bin--;
    else lgldecirr (lgl, 2);
    break;
  }
}

static void lglsetrdlim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->trdreleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->trdmineff.val) limit = lgl->opts->trdmineff.val;
  if (lgl->opts->trdmaxeff.val >= 0 && limit > lgl->opts->trdmaxeff.val)
    limit = lgl->opts->trdmaxeff.val;
  limit >>= (pen = lgl->limits->trd.pen + lglszpen (lgl));
  irrlim = lgl->stats->irr.lits.cur;
  irrlim >>= lgl->limits->simp.pen;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1,
      "[transred-%d] limit %lld based on %d irredundant literals",
      lgl->stats->trd.count++, (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[transred-%d] limit %lld with penalty %d = %d + %d",
      lgl->stats->trd.count++, (LGLL) limit,
      pen, lgl->limits->trd.pen, lglszpen (lgl));
  lgl->limits->trd.steps = lgl->stats->trd.steps + limit;
}

static int lgltrd (LGL * lgl) {
  unsigned pos, delta, mod, ulit, first, last;
  int64_t oldprgss = lgl->stats->prgss;
  int lit, count, success;
  if (lgl->nvars <= 2) return 1;
  lgl->stats->trd.count++;
  lglstart (lgl, &lgl->times->trd);
  assert (!lgl->simp);
  lgl->simp = 1;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  lglsetrdlim (lgl);
  mod = 2*(lgl->nvars - 2);
  assert (mod > 0);
  pos = lglrand (lgl) % mod;
  delta = lglrand (lgl) % mod;
  if (!delta) delta++;
  while (lglgcd (delta, mod) > 1)
    if (++delta == mod) delta = 1;
  LOG (1, "transitive reduction start %u delta %u mod %u", pos, delta, mod);
  first = mod;
  count = 0;
  while (lgl->stats->trd.steps < lgl->limits->trd.steps) {
    if (lglterminate (lgl)) break;
    if (!lglsyncunits (lgl)) break;
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
  success = oldprgss < lgl->stats->prgss;
  if (success && lgl->limits->trd.pen) lgl->limits->trd.pen--;
  if (!success && lgl->limits->trd.pen < MAXPEN) lgl->limits->trd.pen++;
  assert (lgl->simp);
  lgl->simp = 0;
  lglrep (lgl, 1 + !success, 't');
  lglstop (lgl);
  return !lgl->mt;
}

static int lglunhdhasbins (LGL * lgl, const DFPR * dfpr,
			   int lit, int irronly) {
  int blit, tag, other, val, red, ulit;
  const int * p, * w, * eos;
  HTS * hts;
  assert (!lglval (lgl, lit));
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eos = w + hts->count;
  for (p = w; p < eos; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == OCCS) continue;
    if (tag == TRNCS || tag == LRGCS) { p++; continue; }
    red = blit & REDCS;
    if (irronly && red) continue;
    other = blit >> RMSHFT;
    val = lglval (lgl, other);
    assert (val >= 0);
    if (val > 0) continue;
    ulit = lglulit (other);
    if (!dfpr[ulit].discovered) return 1;
  }
  return 0;
}

static int lglunhdisroot (LGL * lgl, int lit, DFPR * dfpr, int irronly) {
  int res = !lglunhdhasbins (lgl, dfpr, lit, irronly);
  assert (!res || !dfpr[lglulit (lit)].discovered);
  return res;
}

static int lglmtwtk (Wtk * wtk) { return wtk->top == wtk->start; }

static int lglfullwtk (Wtk * wtk) { return wtk->top == wtk->end; }

static int lglsizewtk (Wtk * wtk) { return wtk->end - wtk->start; }

static int lglcntwtk (Wtk * wtk) { return wtk->top - wtk->start; }

static void lglrelwtk (LGL * lgl, Wtk * wtk) {
  DEL (wtk->start, lglsizewtk (wtk));
  memset (wtk, 0, sizeof *wtk);
}

static void lglenlwtk (LGL * lgl, Wtk * wtk) {
  int oldsize = lglsizewtk (wtk);
  int newsize = oldsize ? 2*oldsize : 1;
  int count = lglcntwtk (wtk);
  RSZ (wtk->start, oldsize, newsize);
  wtk->top = wtk->start + count;
  wtk->end = wtk->start + newsize;
}

static void lglpushwtk (LGL * lgl, Wtk * wtk,
			Wrag wrag, int lit, int other, int red) {
  Work w;
  if (lglfullwtk (wtk)) lglenlwtk (lgl, wtk);
  w.wrag = wrag;
  w.other = other;
  w.red = red ? 1 : 0;
  w.removed = 0;
  w.lit = lit;
  *wtk->top++ = w;
}

static int lglstamp (LGL * lgl, int root,
		     DFPR * dfpr, DFOPF * dfopf,
		     Wtk * work, Stk * units, Stk * sccs, Stk * trds,
		     int * visitedptr, int stamp, int irronly) {
  int uroot, lit, ulit, blit, tag, red, other, failed, uother, unotother;
  int observed, discovered, pos, undiscovered;
  unsigned start, end, mod, i, j, sccsize;
  const int * p, * w, * eos;
  int startstamp;
  const Work * r;
  int removed;
  HTS * hts;
  Wrag wrag;
  if (lglval (lgl, root)) return stamp;
  uroot =  lglulit (root);
  if (dfpr[uroot].discovered) return stamp;
  assert (!dfpr[uroot].finished);
  assert (lglmtwtk (work));
  assert (lglmtstk (units));
  assert (lglmtstk (sccs));
  assert (lglmtstk (trds));
  LOG (3, "stamping dfs %s %d %s",
       (lglunhdisroot (lgl, root, dfpr, irronly) ? "root" : "start"), root,
       irronly ? "only over irredundant clauses" :
		 "also over redundant clauses");
  startstamp = 0;
  lglpushwtk (lgl, work, PREFIX, root, 0, 0);
  while (!lglmtwtk (work)) {
    lgl->stats->unhd.steps++;
    LGLPOPWTK (work, wrag, lit, other, red, removed);
    if (removed) continue;
    if (wrag == PREFIX) {
      ulit = lglulit (lit);
      if (dfpr[ulit].discovered) {
	dfopf[ulit].observed = stamp;
	LOG (3, "stamping %d observed %d", lit, stamp);
	continue;
      }
      assert (!dfpr[ulit].finished);
      dfpr[ulit].discovered = ++stamp;
      dfopf[ulit].observed = stamp;
      LOG (3, "stamping %d observed %d", lit, stamp);
      *visitedptr += 1;
      if (!startstamp) {
	startstamp = stamp;
	LOG (3, "root %d with stamp %d", lit, startstamp);
	dfpr[ulit].root = lit;
	LOG (4, "stamping %d root %d", lit, lit);
	assert (!dfpr[ulit].parent);
	LOG (4, "stamping %d parent %d", lit, 0);
      }
      LOG (4, "stamping %d discovered %d", lit, stamp);
      lglpushwtk (lgl, work, POSTFIX, lit, 0, 0);
      assert (dfopf[ulit].pushed < 0);
      dfopf[ulit].pushed = lglcntwtk (work);
      assert (!dfopf[ulit].flag);
      dfopf[ulit].flag = 1;
      lglpushstk (lgl, sccs, lit);
      hts = lglhts (lgl, -lit);
      w = lglhts2wchs (lgl, hts);
      eos = w + hts->count;
      for (undiscovered = 0; undiscovered <= 1 ; undiscovered++) {
	start = lglcntwtk (work);
	for (p = w; p < eos; p++) {
	  blit = *p;
	  tag = blit & MASKCS;
	  if (tag == OCCS) continue;
	  if (tag == TRNCS || tag == LRGCS) { p++; continue; }
	  assert (tag == BINCS);
	  red = blit & REDCS;
	  if (irronly && red) continue;
	  other = blit >> RMSHFT;
	  if (lglval (lgl, other)) continue;
	  uother = lglulit (other);
	  if (undiscovered != !dfpr[uother].discovered) continue;
	  // kind of defensive, since 'lglrmbindup' should avoid it
	  // and this fix may not really work anyhow since it does
	  // not distinguish between irredundant and redundant clauses
	  COVER (lglsignedmarked (lgl, other) > 0);
	  if (lglsignedmarked (lgl, other) > 0) {
	    LOG (2, "stamping skips duplicated edge %d %d", lit, other);
	    continue;
	  }
	  lglsignedmark (lgl, other);
	  lglpushwtk (lgl, work, BEFORE, lit, other, red);
	}
	end = lglcntwtk (work);
	for (r = work->start + start; r < work->top; r++)
	  lglunmark (lgl, r->other);
	mod = (end - start);
	if (mod <= 1) continue;
	for (i = start; i < end - 1;  i++) {
	  assert (1 < mod && mod == (end - i));
	  j = lglrand (lgl) % mod--;
	  if (!j) continue;
	  j = i + j;
	  SWAP (Work, work->start[i], work->start[j]);
	}
      }
    } else if (wrag == BEFORE) {	// before recursive call
      LOG (2, "stamping edge %d %d before recursion", lit, other);
      lglpushwtk (lgl, work, AFTER, lit, other, red);
      ulit = lglulit (lit);
      uother = lglulit (other);
      unotother = lglulit (-other);
      if (lgl->opts->unhdextstamp.val && (irronly || red) &&
	  dfopf[uother].observed > dfpr[ulit].discovered) {
	LOG (2, "transitive edge %d %d during stamping", lit, other);
	lgl->stats->unhd.stamp.trds++;
	lgl->stats->prgss++;
	if (red) lgl->stats->unhd.tauts.red++;
	lglrmbcls (lgl, -lit, other, red);
	if ((pos = dfopf[unotother].pushed) >= 0) {
	  while (pos  < lglcntwtk (work)) {
	    if (work->start[pos].lit != -other) break;
	    if (work->start[pos].other == -lit) {
	      LOG (3, "removing edge %d %d from DFS stack", -other, -lit);
	      work->start[pos].removed = 1;
	    }
	    pos++;
	  }
	}
	work->top--;
	assert (dfpr[uother].discovered); // and thus 'parent' + 'root' set
	continue;
      }
      observed = dfopf[unotother].observed;
      if (lgl->opts->unhdextstamp.val && startstamp <= observed) {
	LOG (1, "stamping failing edge %d %d", lit, other);
	for (failed = lit;
	     dfpr[lglulit (failed)].discovered > observed;
	     failed = dfpr[lglulit (failed)].parent)
	  assert (failed);
	LOG (1, "stamping failed literal %d", failed);
	lglpushstk (lgl, units, -failed);
	lgl->stats->unhd.stamp.failed++;
	if (dfpr[unotother].discovered && !dfpr[unotother].finished) {
	  LOG (2, "stamping skips edge %d %d after failed literal %d",
	       lit, other, failed);
	  work->top--;
	  continue;
	}
      }
      if (!dfpr[uother].discovered) {
	dfpr[uother].parent = lit;
	LOG (4, "stamping %d parent %d", other, lit);
	dfpr[uother].root = root;
	LOG (4, "stamping %d root %d", other, root);
	lglpushwtk (lgl, work, PREFIX, other, 0, 0);
      }
    } else if (wrag == AFTER) {		// after recursive call
      LOG (2, "stamping edge %d %d after recursion", lit, other);
      uother = lglulit (other);
      ulit = lglulit (lit);
      if (lgl->opts->unhdextstamp.val && !dfpr[uother].finished &&
	  dfpr[uother].discovered < dfpr[ulit].discovered) {
	LOG (2, "stamping back edge %d %d", lit, other);
	dfpr[ulit].discovered = dfpr[uother].discovered;
	LOG (3, "stamping %d reduced discovered to %d",
	     lit, dfpr[ulit].discovered);
	if (dfopf[ulit].flag) {
	  LOG (2, "stamping %d as being part of a non-trivial SCC", lit);
	  dfopf[ulit].flag = 0;
	}
      }
      dfopf[uother].observed = stamp;
      LOG (3, "stamping %d observed %d", other, stamp);
    } else {
      assert (wrag == POSTFIX);
      LOG (2, "stamping postfix %d", lit);
      ulit = lglulit (lit);
      if (dfopf[ulit].flag) {
	stamp++;
	sccsize = 0;
	discovered = dfpr[ulit].discovered;
	do {
	  other = lglpopstk (sccs);
	  uother = lglulit (other);
	  dfopf[uother].pushed = -1;
	  dfopf[uother].flag = 0;
	  dfpr[uother].discovered = discovered;
	  dfpr[uother].finished = stamp;
	  LOG (3, "stamping %d interval %d %d parent %d root %d",
	       other, dfpr[uother].discovered, dfpr[uother].finished,
	       dfpr[uother].parent, dfpr[uother].root);
	  sccsize++;
	} while (other != lit);
	assert (lgl->opts->unhdextstamp.val || sccsize == 1);
	if (sccsize > 1) {
	  LOG (2, "stamping non trivial SCC of size %d", sccsize);
	  lgl->stats->unhd.stamp.sumsccsizes += sccsize;
	  lgl->stats->unhd.stamp.sccs++;
	}
      } else assert (lgl->opts->unhdextstamp.val);
    }
  }
  assert (lglmtwtk (work));
  assert (lglmtstk (sccs));
  return stamp;
}

static int lglunhlca (LGL * lgl, const DFPR * dfpr, int a, int b) {
  const DFPR * c, * d;
  int u, v, p;
  if (a == b) return a;
  u = lglulit (a), v = lglulit (b);
  c = dfpr + u, d = dfpr + v;
  if (c->discovered <= d->discovered) {
    p = a;
  } else {
    assert (c->discovered > d->discovered);
    p = b;
    SWAP (const DFPR *, c, d);
  }
  for (;;) {
    assert (c->discovered <= d->discovered);
    if (d->finished <= c->finished) break;
    p = c->parent;
    if (!p) break;
    u = lglulit (p);
    c = dfpr + u;
  }
  LOG (3, "unhiding least common ancestor of %d and %d is %d", a, b, p);
  return p;
}

static int lglunhidefailed (LGL * lgl, const DFPR * dfpr) {
  int idx, sign, lit, unit, nfailed = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    for (sign = -1; sign <= 1; sign += 2) {
      if (lglterminate (lgl)) return 0;
      if (!lglsyncunits (lgl)) return 0;
      lgl->stats->unhd.steps++;
      lit = sign * idx;
      if (lglval (lgl, lit)) continue;
      if (!dfpr[lglulit (lit)].discovered) continue;
      if (lglunhimplincl (dfpr, lit, -lit)) {
	unit = -lit;
	LOG (2, "unhiding %d implies %d", lit, -lit);
      } else if (lglunhimplincl (dfpr, -lit, lit)) {
	unit = lit;
	LOG (2, "unhiding %d implies %d", -lit, lit);
      } else continue;
      LOG (1, "unhiding failed literal %d", -unit);
      lglunit (lgl, unit);
      lgl->stats->unhd.failed.lits++;
      nfailed++;
      if (lglbcp (lgl)) continue;
      LOG (1, "empty clause after propagating unhidden failed literal");
      assert (!lgl->mt);
      lgl->mt = 1;
      return 0;
    }
  }
  LOG (1, "unhiding %d failed literals in this round", nfailed);
  return 1;
}

static int lglunhroot (const DFPR * dfpr, int lit) {
  return dfpr[lglulit (lit)].root;
}

static int lglunhidebintrn (LGL * lgl, const DFPR * dfpr, int irronly) {
  int idx, sign, lit, blit, tag, red, other, other2, unit, root, lca;
  int nbinred, ntrnred, nbinunits, ntrnunits, ntrnstr, ntrnhbrs;
  const int * p, * eow;
  int ulit, uother;
  int * w , * q;
  long delta;
  HTS * hts;
  nbinred = ntrnred = nbinunits = ntrnunits = ntrnstr = ntrnhbrs = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    for (sign = -1; sign <= 1; sign += 2) {
      if (lglterminate (lgl)) return 0;
      if (!lglsyncunits (lgl)) return 0;
      lgl->stats->unhd.steps++;
      lit = sign * idx;
      if (lglval (lgl, lit)) continue;
      ulit = lglulit (lit);
      if (!dfpr[ulit].discovered) continue;
      hts = lglhts (lgl, lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      q = w;
      for (p = w; p < eow; p++) {
	blit = *p;
	*q++ = blit;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) *q++ = *++p;
	if (tag == LRGCS) continue;
	red = blit & REDCS;
	other = blit >> RMSHFT;
	if (lglval (lgl, other)) continue;
	uother = lglulit (other);
	if (tag == BINCS) {
	  if (lglunhimplies2 (dfpr, other, lit)) {
	    LOG (2, "unhiding removal of literal %d "
		    "with implication %d %d from binary clause %d %d",
		 other, other, lit, lit, other);
	    lgl->stats->unhd.units.bin++;
	    nbinunits++;
	    unit = lit;
UNIT:
	    lglunit (lgl, unit);
	    p++;
	    while (p < eow) *q++ = *p++;
	    lglshrinkhts (lgl, hts, hts->count - (p - q));
	    if (lglbcp (lgl)) goto NEXTIDX;
	    LOG (1, "empty clause after propagating unhidden lifted unit");
	    assert (!lgl->mt);
	    lgl->mt = 1;
	    return 0;
	  } else if ((root = lglunhroot (dfpr, -lit)) &&
		     !lglval (lgl, root) &&
		     root == lglunhroot (dfpr, -other)) {
	    LOG (2, "negated literals in binary clause %d %d implied by %d",
		 lit, other, root);
	    lgl->stats->unhd.failed.bin++;
	    lca = lglunhlca (lgl, dfpr, -lit, -other);
	    unit = -lca;
	    assert (unit);
	    goto UNIT;
	  } else if (!irronly && !red) continue;
	  else {
	    if (dfpr[uother].parent == -lit) continue;
	    if (dfpr[ulit].parent == -other) continue;
	    if (!lglunhimplies2 (dfpr, -lit, other)) continue;
	    LOG (2, "unhiding tautological binary clause %d %d", lit, other);
	    lgl->stats->unhd.tauts.bin++;
	    lgl->stats->prgss++;
	    if (red) lgl->stats->unhd.tauts.red++;
	    nbinred++;
	    lglrmbwch (lgl, other, lit, red);
	    LOG (2, "removed %s binary clause %d %d",
		 lglred2str (red), lit, other);
	    lgldeclscnt (lgl, 2, red, 0);
	    assert (!lgl->dense);
	    q--;
	  }
	} else {
	  assert (tag == TRNCS);
	  other2 = *p;
	  if (lglval (lgl, other2)) continue;
	  if (lglunhimplies2incl (dfpr, other, lit) &&
	      lglunhimplies2incl (dfpr, other2, lit)) {
	    LOG (2,
		 "unhiding removal of literals %d and %d with implications "
		 "%d %d and %d %d from ternary clause %d %d %d",
		 other, other2,
		 other, lit,
		 other2, lit,
		 lit, other, other2);
	    lgl->stats->unhd.str.trn += 2;
	    if (red) lgl->stats->unhd.str.red += 2;
	    lgl->stats->unhd.units.trn++;
	    ntrnunits++;
	    unit = lit;
	    goto UNIT;
	  } else if ((root = lglunhroot (dfpr, -lit)) &&
		     !lglval (lgl, root) &&
		     root == lglunhroot (dfpr, -other) &&
		     root == lglunhroot (dfpr, -other2)) {
	    LOG (2,
	      "negation of literals in ternary clauses %d %d %d "
	      "implied by %d", lit, other, other2, root);
	    lgl->stats->unhd.failed.trn++;
	    lca = lglunhlca (lgl, dfpr, -lit, -other);
	    assert (lca);
	    lca = lglunhlca (lgl, dfpr, lca, -other2);
	    assert (lca);
	    unit = -lca;
	    goto UNIT;
	  } else if ((red || irronly) &&
		     (lglunhimplies2incl (dfpr, -lit, other) ||
		      lglunhimplies2incl (dfpr, -lit, other2))) {
	    LOG (2, "unhiding tautological ternary clause %d %d %d",
		 lit, other, other2);
	    lgl->stats->unhd.tauts.trn++;
	    lgl->stats->prgss++;
	    if (red) lgl->stats->unhd.tauts.red++;
	    ntrnred++;
	    lglrmtwch (lgl, other, lit, other2, red);
	    lglrmtwch (lgl, other2, lit, other, red);
	    LOG (2, "removed %s ternary clause %d %d %d",
		 lglred2str (red), lit, other, other2);
	    lgldeclscnt (lgl, 3, red, 0);
	    assert (!lgl->dense);
	    q -= 2;
	  } else if (lglunhimplies2incl (dfpr, other2, lit)) {
TRNSTR:
	    LOG (2,
		 "unhiding removal of literal %d with implication "
		 "%d %d from ternary clause %d %d %d",
		 other2, other2, lit, lit, other, other2);
	    lgl->stats->unhd.str.trn++;
	    lgl->stats->prgss++;
	    if (red) lgl->stats->unhd.str.red++;
	    ntrnstr++;
	    lglrmtwch (lgl, other, lit, other2, red);
	    lglrmtwch (lgl, other2, lit, other, red);
	    LOG (2, "removed %s ternary clause %d %d %d",
		 lglred2str (red), lit, other, other2);
	    lgldeclscnt (lgl, 3, red, 0);
	    if (!red) lglincirr (lgl, 2);
	    else lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
	    delta = lglwchbin (lgl, other, lit, red);
	    if (delta) { p += delta, q += delta, eow += delta, w += delta; }
	    (--q)[-1] = red | BINCS | (other << RMSHFT);
#ifndef NLGLPICOSAT
	    lglpicosatchkclsarg (lgl, lit, other, 0);
#endif
	    continue;
	  } else if (lglunhimplies2incl (dfpr, other, lit)) {
	    SWAP (int, other, other2);
	    goto TRNSTR;
	  } else if (lgl->opts->unhdhbr.val &&
		     (root = lglunhroot (dfpr, -lit)) &&
		     !lglval (lgl, root)) {
	    if (root == lglunhroot (dfpr, -other2)) {
	      lca = lglunhlca (lgl, dfpr, -lit, -other2);
	    } else if (root == lglunhroot (dfpr, -other)) {
	      lca = lglunhlca (lgl, dfpr, -lit, -other);
	      SWAP (int, other, other2);
	    } else if (lglunhimplies2incl (dfpr, root, -other2)) lca = root;
	    else if (lglunhimplies2incl (dfpr, root, -other)) {
	      lca = root;
	      SWAP (int, other, other2);
	    } else continue;
	    assert (lca);
	    if (abs (lca) == abs (lit)) continue;
	    if (abs (lca) == abs (other)) continue;
	    if (abs (lca) == abs (other2)) continue;
	    if (lglunhimplies2incl (dfpr, lca, other)) continue;
	    LOG (2,
	      "negations of literals %d %d in ternary clause %d %d %d "
	      "implied by %d", lit, other2, lit, other, other2, lca);
	    lgl->stats->unhd.hbrs.trn++;
	    if (red) lgl->stats->unhd.hbrs.red++;
	    lgl->stats->prgss++;
	    ntrnhbrs++;
	    LOG (2, "unhidden hyper binary resolved clause %d %d",-lca,other);
#ifndef NLGLPICOSAT
	    lglpicosatchkclsarg (lgl, -lca, other, 0);
#endif
	    assert (lca != -lit);
	    lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
	    delta = lglwchbin (lgl, -lca, other, REDCS);
	    if (delta) { p += delta, q += delta, eow += delta, w += delta; }
	    delta = lglwchbin (lgl, other, -lca, REDCS);
	    if (delta) { p += delta, q += delta, eow += delta, w += delta; }
	  }
	}
      }
      lglshrinkhts (lgl, hts, hts->count - (p - q));
    }
NEXTIDX:
    ;
  }
  if (nbinred) LOG (2, "unhiding %d tautological binary clauses", nbinred);
  if (nbinunits) LOG (2, "unhiding %d units in binary clauses", nbinunits);
  if (ntrnred) LOG (2, "unhiding %d tautological ternary clauses", ntrnred);
  if (ntrnstr) LOG (2, "unhiding %d strengthened ternary clauses", ntrnstr);
  if (ntrnunits) LOG (2, "unhiding %d units in ternary clauses", ntrnunits);
  if (ntrnstr) LOG (2, 
    "unhidden %d hyper binary resolution from ternary clauses", ntrnhbrs);
  return 1;
}

static int lglcmpdfl (const DFL * a, const DFL * b) {
  return a->discovered - b->discovered;
}

static int lglunhideglue (LGL * lgl, const DFPR * dfpr, int glue, int irronly) {
  DFL * dfl, * eodfl, * d, * e; int szdfl, posdfl, negdfl, ndfl, res;
  int oldsize, newsize, hastobesatisfied, satisfied, tautological;
  int watched, lit, ulit, val, sign, nonfalse, root, lca, unit;
  int ntaut = 0, nstr = 0, nunits = 0, nhbrs = 0, lidx;
  int * p, * q, * c, * eoc, red;
  int lca1, lca2, root1, root2;
  Stk * lits;
#ifndef NLGLOG
  char type[20];
  if (glue < 0) sprintf (type, "irredundant");
  else sprintf (type, "redundant glue %d", glue);
#endif
  assert (!lgl->mt);
  assert (-1 <= glue && glue < MAXGLUE);
  if (glue < 0) {
    lits = &lgl->irr;
    red = 0;
  } else {
    lits = lgl->red + glue;
    red = REDCS;
  }
  res = 1;
  dfl = 0; szdfl = 0;
  // go through all clauses of this glue and for each do:
  //
  //   SHRINK  simplify clause according to current assignment
  //   FAILED  check if it is a hidden failed literal
  //   HTE     check if it is a hidden tautology
  //   STRNEG  remove hidden literals using complement literals
  //   STRPOS  remove hidden literals using positive literals
  //   HBR     perform unhiding hyper binary resolution
  //   NEXT    clean up, unwatch if necessary, reconnect bin/trn, bcp unit
  //
  for (c = lits->start; !lgl->mt && c < lits->top; c = eoc + 1) {
    if (lglterminate (lgl) || !lglsyncunits (lgl)) { res = 0; break; }
    if ((lit = *(eoc = c)) >= NOTALIT) continue;
    lgl->stats->unhd.steps++;
    lidx = c - lits->start;
    if (red) { lidx <<= GLUESHFT; lidx |= glue; }
    watched = 1;
    while (*eoc) eoc++;
    oldsize = eoc - c;

    unit = hastobesatisfied = satisfied = tautological = ndfl = 0;
//SHRINK: check satisfied + remove false literals + count visited
    q = c;
    nonfalse = posdfl = negdfl = 0;
    for (p = c; p < eoc; p++) {
      lit = *p;
      val = lglval (lgl, lit);
      if (val > 0) {
	satisfied = 1;
	q = c + 2;
	break;
      }
      if (val < 0) {
	if (p < c + 2) {
	  *q++ = lit;		// watched, so have to keep it
	  hastobesatisfied = 1;	// for assertion checking only
	}
	continue;
      }
      *q++ = lit;
      nonfalse++;
      if (dfpr[lglulit (lit)].discovered) posdfl++;	// count pos in BIG
      if (dfpr[lglulit (-lit)].discovered) negdfl++;	// count neg in BIG
    }
    *(eoc = q) = 0;
    assert (eoc - c >= 2);	// we assume bcp done before
    ndfl = posdfl + negdfl;	// number of literals in BIG
    if (hastobesatisfied) assert (satisfied);
    if (satisfied || ndfl < 2) goto NEXT;
    assert (nonfalse = eoc - c);
    assert (nonfalse >= negdfl);
//FAILED: find root implying all negated literals
    if (nonfalse != negdfl) goto HTE;	// not enough complement lits in BIG
    assert (c < eoc);
    root = lglunhroot (dfpr, -*c);
    if (lglval (lgl, root)) goto HTE;
    for (p = c + 1; p < eoc && lglunhroot (dfpr, -*p) == root; p++)
      ;
    if (p < eoc) goto HTE;		// at least two roots
    LOGCLS (2, c, "unhiding failed literal through large %s clause",type);
    LOG (2, "unhiding that all negations are implied by root %d", root);
    lca = -*c;
    for (p = c + 1; p < eoc; p++)
      lca = lglunhlca (lgl, dfpr, -*p, lca);
    assert (!lglval (lgl, lca));
    LOG (2, "unhiding failed large %s clause implied by LCA %d", type, lca);
    lgl->stats->unhd.failed.lrg++;
    unit = -lca;
    goto NEXT;  // otherwise need to properly unwatch and restart etc.
		// which is too difficult to implement so leave further
		// simplification of this clause to next unhiding round
HTE:
    if (glue < 0 && !irronly) goto STRNEG; // skip tautology checking if
					   // redundant clauses used and
					   // we work on irredundant clauses
    if (posdfl < 2 || negdfl < 2) goto STRNEG;
    if (ndfl > szdfl) { RSZ (dfl, szdfl, ndfl); szdfl = ndfl; }
    ndfl = 0;
    // copy all literals and their complements to 'dfl'
    for (p = c; p < eoc; p++) {
      for (sign = -1; sign <= 1; sign += 2) {
	lit = *p;
	ulit = lglulit (sign * lit);
	if (!dfpr[ulit].discovered) continue;	// not in BIG so skip
	assert (ndfl < szdfl);
	dfl[ndfl].discovered = dfpr[ulit].discovered;
	dfl[ndfl].finished = dfpr[ulit].finished;
	dfl[ndfl].sign = sign;
#ifndef NLGLOG
	dfl[ndfl].lit4logging = lit;
#endif
	ndfl++;
      }
    }
    lgl->stats->unhd.steps += 6;			// rough guess
    SORT (DFL, dfl, ndfl, lglcmpdfl);
    eodfl = dfl + ndfl;
    // Now check if there are two consecutive literals, the first
    // a complement of a literal in the clause, which implies another
    // literal actually occurring positive in the clause, where
    // 'd' ranges over complement literals
    // 'e' ranges over original literals
    for (d = dfl; d < eodfl - 1; d++)
      if (d->sign < 0) break;			// get first negative pos
    while (d < eodfl - 1) {
      assert (d->sign < 0);
      for (e = d + 1; e < eodfl && e->finished < d->finished; e++) {
	if (e->sign < 0) continue;		// get next positive pos
	assert (d->sign < 0 && e->sign > 0);
	assert (d->discovered <= e->discovered);
	assert (e ->finished <= d->finished);
	LOGCLS (2, c,
		"unhiding with implication %d %d tautological %s clause",
		-d->lit4logging, e->lit4logging, type);
	ntaut++;
	lgl->stats->unhd.tauts.lrg++;
	if (red) lgl->stats->unhd.tauts.red++;
	lgl->stats->prgss++;
	tautological = 1;
	goto NEXT;
      }
      for (d = e; d < eodfl && d->sign > 0; d++)
	;
    }
STRNEG:
    if (negdfl < 2) goto STRPOS;
    if (negdfl > szdfl) { RSZ (dfl, szdfl, negdfl); szdfl = negdfl; }
    lgl->stats->unhd.steps++;
    ndfl = 0;
    // copy complement literals to 'dfl'
    for (p = c; p < eoc; p++) {
      lit = *p;
      ulit = lglulit (-lit);			// NOTE: '-lit' not 'lit'
      if (!dfpr[ulit].discovered) continue;
      assert (ndfl < szdfl);
      dfl[ndfl].discovered = dfpr[ulit].discovered;	// of '-lit'
      dfl[ndfl].finished = dfpr[ulit].finished;		// of '-lit'
      dfl[ndfl].lit = lit;			// NOTE: but 'lit' here
      ndfl++;
    }
    if (ndfl < 2) goto STRPOS;
    lgl->stats->unhd.steps += 3;			// rough guess
    SORT (DFL, dfl, ndfl, lglcmpdfl);
    eodfl = dfl + ndfl;
    for (d = dfl; d < eodfl - 1; d = e) {
      assert (d->discovered);
      for (e = d + 1; e < eodfl && d->finished >= e->finished; e++) {
	assert (d->discovered <= e->discovered);
	lit = e->lit;
	LOGCLS (2, c,
		"unhiding removal of literal %d "
		"with implication %d %d from large %s clause",
		lit, -d->lit, -e->lit, type);
	e->lit = 0;
	nstr++;
	lgl->stats->unhd.str.lrg++;
	if (red) lgl->stats->unhd.str.red++;
	lgl->stats->prgss++;
	if (!watched) continue;
	if (lit != c[0] && lit != c[1]) continue;
	lglrmlwch (lgl, c[0], red, lidx);
	lglrmlwch (lgl, c[1], red, lidx);
	watched = 0;
      }
    }
    assert (eoc - c >= 1 );
    q = c;
    if (watched) q += 2;			// keep watched literals
						// if we still watch
    // move non BIG literals
    for (p = q; p < eoc; p++) {
      lit = *p;
      ulit = lglulit (-lit);			// NOTE: '-lit' not 'lit'
      if (dfpr[ulit].discovered) continue;
      *q++ = lit;
    }
    // copy from 'dfl' unremoved BIG literals back to clause
    for (d = dfl; d < eodfl; d++) {
      lit = d->lit;
      if (!lit) continue;
      if (watched && lit == c[0]) continue;
      if (watched && lit == c[1]) continue;
      *q++ = lit;
    }
    *(eoc = q) = 0;
STRPOS:
    if (posdfl < 2) goto HBR;
    if (posdfl > szdfl) { RSZ (dfl, szdfl, posdfl); szdfl = posdfl; }
    ndfl = 0;
    // copy original literals to 'dfl' but sort reverse wrt 'discovered'
    for (p = c; p < eoc; p++) {
      lit = *p;
      ulit = lglulit (lit);			// NOTE: now 'lit'
      if (!dfpr[ulit].discovered) continue;
      assert (ndfl < szdfl);
      dfl[ndfl].discovered = -dfpr[ulit].discovered;	// of 'lit'
      dfl[ndfl].finished = -dfpr[ulit].finished;		// of 'lit'
      dfl[ndfl].lit = lit;
      ndfl++;
    }
    if (ndfl < 2) goto NEXT;
    lgl->stats->unhd.steps += 3;			// rough guess
    SORT (DFL, dfl, ndfl, lglcmpdfl);
    eodfl = dfl + ndfl;
    for (d = dfl; d < eodfl - 1; d = e) {
      assert (d->discovered);
      for (e = d + 1; e < eodfl && d->finished >= e->finished; e++) {
	assert (d->discovered <= e->discovered);
	lit = e->lit;
	LOGCLS (2, c,
		"unhiding removal of literal %d "
		"with implication %d %d from large %s clause",
		lit, e->lit, d->lit, type);
	e->lit = 0;
	nstr++;
	lgl->stats->unhd.str.lrg++;
	if (red) lgl->stats->unhd.str.red++;
	lgl->stats->prgss++;
	if (!watched) continue;
	if (lit != c[0] && lit != c[1]) continue;
	lglrmlwch (lgl, c[0], red, lidx);
	lglrmlwch (lgl, c[1], red, lidx);
	watched = 0;
      }
    }
    assert (eoc - c >= 1 );
    q = c;
    if (watched) q += 2;
    for (p = q; p < eoc; p++) {
      lit = *p;
      ulit = lglulit (lit);			// NOTE: now 'lit'
      if (dfpr[ulit].discovered) continue;
      *q++ = lit;
    }
    for (d = dfl; d < eodfl; d++) {
      lit = d->lit;
      if (!lit) continue;
      if (watched && lit == c[0]) continue;
      if (watched && lit == c[1]) continue;
      *q++ = lit;
    }
    *(eoc = q) = 0;
HBR:
    if (!lgl->opts->unhdhbr.val) goto NEXT;
    if (eoc - c < 3) goto NEXT;
    root1 = root2 = lca1 = lca2 = 0;
    for (p = c; (lit = *p); p++) {
      root = lglunhroot (dfpr, -lit);
      if (!root) root = -lit;
      if (!root1) { root1 = root; continue; }
      if (root1 == root) continue;
      if (!root2) { root2 = root; continue; }
      if (root2 == root) continue;
      if (lglunhimplies2incl (dfpr, root1, -lit)) { lca1 = root1; continue; }
      if (lglunhimplies2incl (dfpr, root2, -lit)) { lca2 = root2; continue; }
      goto NEXT;			// else more than two roots ...
    }
    assert (root1);
    if (!root2) goto NEXT;
    if (root1 == -root2) goto NEXT;
    if (lglunhimplies2incl (dfpr, root1, -root2)) goto NEXT;
    LOGCLS (2, c, "root hyper binary resolution %d %d of %s clause",
	    -root1, -root2, type);
    if (!lca1 && !lca2) {
      for (p = c; (lit = *p); p++) {
	root = lglunhroot (dfpr, -lit);
	if (root) {
	  if (root == root1)
	    lca1 = lca1 ? lglunhlca (lgl, dfpr, lca1, -lit) : -lit;
	  if (root == root2)
	    lca2 = lca2 ? lglunhlca (lgl, dfpr, lca2, -lit) : -lit;
	} else {
	  assert (!lca2);
	  if (lca1) lca2 = -lit; else lca1 = -lit;
	}
      }
    } else {
      if (!lca1) lca1 = root1;
      if (!lca2) lca2 = root2;
    }
    if (lca1 == -lca2) goto NEXT;
    if (lglunhimplies2incl (dfpr, lca1, -lca2)) goto NEXT;
    LOGCLS (2, c, "lca hyper binary resolution %d %d of %s clause",
	    -lca1, -lca2, type);
    lgl->stats->unhd.hbrs.lrg++;
    if (red) lgl->stats->unhd.hbrs.red++;
#ifndef NLGLPICOSAT
    lglpicosatchkclsarg (lgl, -lca1, -lca2, 0);
#endif
    lglwchbin (lgl, -lca1, -lca2, REDCS);
    lglwchbin (lgl, -lca2, -lca1, REDCS);
    lgl->stats->red.bin++;
    assert (lgl->stats->red.bin > 0);
NEXT:
    newsize = eoc - c;
    assert (satisfied || tautological || newsize >= 1);
    if (newsize <= 3 || satisfied || tautological) {
      lgldeclscnt (lgl, oldsize, red, glue);
      if (watched) {
	lglrmlwch (lgl, c[0], red, lidx);
	lglrmlwch (lgl, c[1], red, lidx);
      }
    } else if (!red) {
      assert (lgl->stats->irr.lits.cur >= c + oldsize - eoc);
      lgl->stats->irr.lits.cur -= c + oldsize - eoc;
    }
    assert (!*eoc);
    for (p = c + oldsize; p > eoc; p--) *p = REMOVED;
    if (satisfied || tautological) {
      while (p >= c) *p-- = REMOVED;
      if (red) { LGLCHKACT (c[-1]); c[-1] = REMOVED; }
      eoc = c + oldsize;
      continue;
    }
#ifndef NLGLPICOSAT
    if (newsize < oldsize) lglpicosatchkclsaux (lgl, c);
#endif
    if (red && newsize <= 3) { LGLCHKACT (c[-1]); c[-1] = REMOVED; }
    if (newsize > 3 && !watched) {
      (void) lglwchlrg (lgl, c[0], c[1], red, lidx);
      (void) lglwchlrg (lgl, c[1], c[0], red, lidx);
    } else if (newsize == 3) {
      LOGCLS (2, c, "large %s clause became ternary", type);
      lglwchtrn (lgl, c[0], c[1], c[2], red);
      lglwchtrn (lgl, c[1], c[0], c[2], red);
      lglwchtrn (lgl, c[2], c[0], c[1], red);
      if (!red) lglincirr (lgl, 3);
      else lgl->stats->red.trn++, assert (lgl->stats->red.trn > 0);
      c[0] = c[1] = c[2] = *eoc = REMOVED;
    } else if (newsize == 2) {
      LOGCLS (2, c, "large %s clause became binary", type);
      lglwchbin (lgl, c[0], c[1], red);
      lglwchbin (lgl, c[1], c[0], red);
      if (!red) lglincirr (lgl, 2);
      else lgl->stats->red.bin++, assert (lgl->stats->red.bin > 0);
      c[0] = c[1] = *eoc = REMOVED;
    } else if (newsize == 1) {
      LOGCLS (2, c, "large %s clause became unit", type);
      unit = c[0];		// even works if unit already set
      c[0] = *eoc = REMOVED;
      lgl->stats->unhd.units.lrg++;
      nunits++;
    }
    if (!unit) continue;
    lglunit (lgl, unit);
    if (lglbcp (lgl)) continue;
    assert (!lgl->mt);
    lgl->mt = 1;
    LOG (1, "unhiding large clause produces empty clause");
    res = 0;
  }
  if (nunits)
    LOG (1, "unhiding %d units from large %s clauses", nunits, type);
  if (ntaut)
    LOG (1, "unhiding %d large tautological %s clauses", ntaut, type);
  if (nstr)
    LOG (1, "unhiding removal of %d literals in %s clauses", nstr, type);
  if (nhbrs)
    LOG (1, "unhiding %d hyper binary resolutions in %s clauses", nhbrs, type);
  if (dfl) DEL (dfl, szdfl);
  return res;
}

static void lglfixlrgwchs (LGL * lgl) {
  int idx, sign, lit, blit, tag, red, lidx, fixed;
  const int * p, * eow, * c;
  int * q, * w;
  HTS * hts;
  fixed = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      hts = lglhts (lgl, lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      q = w;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == BINCS) { *q++ = blit; continue; }
	lidx = *++p;
	if (tag == TRNCS) { *q++ = blit; *q++ = lidx; continue; }
	assert (tag == LRGCS);
	red = blit & REDCS;
	c = lglidx2lits (lgl, LRGCS, red, lidx);
	if (*c >= NOTALIT) { fixed++; continue; }
	*q++ = blit;
	*q++ = lidx;
      }
      lglshrinkhts (lgl, hts, hts->count - (p - q));
    }
  }
  assert (!(fixed & 1));
  if (fixed) LOG (1, "fixed %d large watches", fixed);
}

static int lglunhidelrg (LGL * lgl, const DFPR * dfpr, int irronly) {
  int glue, res = 1;
  for (glue = -1; res && glue < MAXGLUE; glue++)
    res = lglunhideglue (lgl, dfpr, glue, irronly);
  lglfixlrgwchs (lgl);
  return res;
}

static int lglunhdunits (LGL * lgl) {
  int res = lgl->stats->unhd.units.bin;
  res += lgl->stats->unhd.units.trn;
  res += lgl->stats->unhd.units.lrg;
  return res;
}

static int lglunhdfailed (LGL * lgl) {
  int res = lgl->stats->unhd.stamp.failed;
  res += lgl->stats->unhd.failed.lits;
  res += lgl->stats->unhd.failed.bin;
  res += lgl->stats->unhd.failed.trn;
  res += lgl->stats->unhd.failed.lrg;
  return res;
}

static int lglunhdhbrs (LGL * lgl) {
  int res = lgl->stats->unhd.hbrs.trn;
  res += lgl->stats->unhd.hbrs.lrg;
  return res;
}

static int lglunhdtauts (LGL * lgl) {
  int res = lgl->stats->unhd.stamp.trds;
  res += lgl->stats->unhd.tauts.bin;
  res += lgl->stats->unhd.tauts.trn;
  res += lgl->stats->unhd.tauts.lrg;
  return res;
}

static int lglunhdstrd (LGL * lgl) {
  int res = lgl->stats->unhd.units.bin;	// shared!
  res += lgl->stats->unhd.str.trn;
  res += lgl->stats->unhd.str.lrg;
  return res;
}

static void lglrmbindup (LGL * lgl) {
  int idx, sign, lit, blit, tag, red, other, round, redrem, irrem;
  int * w, * eow, * p, * q;
  HTS * hts;
  redrem = irrem = 0;
  for (idx = 2; idx < lgl->nvars; idx++) {
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      assert (lglmtstk (&lgl->seen));
      for (round = 0; round < 2; round++) {
	hts = lglhts (lgl, lit);
	w = lglhts2wchs (lgl, hts);
	eow = w + hts->count;
	q = w;
	for (p = w; p < eow; p++) {
	  blit = *p;
	  tag = blit & MASKCS;
	  if (tag != BINCS) *q++ = blit;
	  if (tag== LRGCS || tag == TRNCS) *q++ = *++p;
	  if (tag != BINCS) continue;
	  red = blit & REDCS;
	  other = blit >> RMSHFT;
	  if (lglsignedmarked (lgl, other)) {
	    if (round && !red) goto ONLYCOPY;
	    if (red) redrem++; else irrem++;
	    if (abs (lit) > abs (other)) {
	      LOG (2,
		"removing duplicated %s binary clause %d %d and 2nd watch %d",
		lglred2str (red), lit, other, other);
	      lgldeclscnt (lgl, 2, red, 0);
	      if (!red && lgl->dense)
		lgldecocc (lgl, lit), lgldecocc (lgl, other);
	      lgl->stats->bindup.removed++;
	      if (red) lgl->stats->bindup.red++;
	    } else
	      LOG (2,
		"removing 1st watch %d of duplicated %s binary clause %d %d",
		other, lglred2str (red), other, lit);
	  } else {
	    if ((!round && !red) || (round && red))
	      lglsignedmarknpushseen (lgl, other);
ONLYCOPY:
	    *q++ = blit;
	  }
	}
	lglshrinkhts (lgl, hts, hts->count - (p - q));
      }
      lglpopnunmarkstk (lgl, &lgl->seen);
    }
  }
  assert (!(irrem & 1));
  assert (!(redrem & 1));
}

static DFPR * lglstampall (LGL * lgl, int irronly) {
  int roots, searches, noimpls, unassigned, visited;
  unsigned pos, delta, mod, ulit, first, last, count;
  int root, stamp, rootsonly, lit;
  Stk units, sccs, trds;
  DFOPF * dfopf, * q;
  DFPR * dfpr;
  Wtk work;
  Val val;
  if (lgl->nvars <= 2) return 0;
  lglrmbindup (lgl);
  NEW (dfpr, 2*lgl->nvars);
  NEW (dfopf, 2*lgl->nvars);
  CLR (work); CLR (sccs); CLR (trds); CLR (units);
  for (q = dfopf; q < dfopf + 2*lgl->nvars; q++) q->pushed = -1;
  searches = roots = noimpls = unassigned = stamp = visited = 0;
  for (rootsonly = 1; rootsonly >= 0; rootsonly--) {
    count = 0;
    first = mod = 2*(lgl->nvars - 2);
    assert (mod > 0);
    pos = lglrand (lgl) % mod;
    delta = lglrand (lgl) % mod;
    if (!delta) delta++;
    while (lglgcd (delta, mod) > 1)
      if (++delta == mod) delta = 1;
    LOG (2, "unhiding %s round start %u delta %u mod %u",
	 (rootsonly ? "roots-only": "non-root"), pos, delta, mod);
    for (;;) {
      if (lglterminate (lgl)) { searches = 0; goto DONE; }
      if (!lglsyncunits (lgl)) { assert (lgl->mt); goto DONE; }
      ulit = pos + 4;
      root = lglilit (ulit);
      lgl->stats->unhd.steps++;
      count++;
      if (lglval (lgl, root)) goto CONTINUE;
      if (rootsonly) unassigned++;
      if (dfpr[lglulit (root)].discovered) goto CONTINUE;
      if (rootsonly &&
	  !lglunhdisroot (lgl, root, dfpr, irronly)) goto CONTINUE;
      if (!lglunhdhasbins (lgl, dfpr, -root, irronly)) {
	if (rootsonly) noimpls++; goto CONTINUE;
      }
      if (rootsonly) roots++;
      searches++;
      assert (lglmtstk (&units));
      stamp = lglstamp (lgl, root, dfpr, dfopf,
			&work, &units, &sccs, &trds, &visited,
			stamp, irronly);
      while (!lglmtstk (&units)) {
	lit = lglpopstk (&units);
	val = lglval (lgl, lit);
	if (val > 0) continue;
	if (val < 0) {
	  assert (!lgl->mt);
	  LOG (1, "unhidding stamp unit %d already false", lit);
	  lgl->mt = 1;
	  goto DONE;
	}
	lglunit (lgl, lit);
	if (!lglbcp (lgl)) {
	  assert (!lgl->mt);
	  LOG (1, "propagating unhidden stamp unit %d failed", lit);
	  lgl->mt = 1;
	  goto DONE;
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
  }
  assert (searches >= roots);
  lglprt (lgl, 2,
	  "[unhd-%d-%d] %d unassigned variables out of %d (%.0f%%)",
	  lgl->stats->unhd.count, lgl->stats->unhd.rounds,
	  lgl->unassigned, lgl->nvars - 2,
	    lglpcnt (lgl->unassigned, lgl->nvars - 2));
  lglprt (lgl, 2,
	  "[unhd-%d-%d] %d root literals out of %d (%.0f%%)",
	  lgl->stats->unhd.count, lgl->stats->unhd.rounds,
	  roots, unassigned, lglpcnt (roots, unassigned));
  lglprt (lgl, 2,
    "[unhd-%d-%d] %d additional non-root searches out of %d (%.0f%%)",
	  lgl->stats->unhd.count, lgl->stats->unhd.rounds,
	  searches - roots, unassigned,
	    lglpcnt (searches - roots, unassigned));
  lglprt (lgl, 2,
	  "[unhd-%d-%d] %d literals not in F2 out of %d (%.0f%%)",
	  lgl->stats->unhd.count, lgl->stats->unhd.rounds,
	  noimpls, unassigned, lglpcnt (noimpls, unassigned));
  lglprt (lgl, 2,
	  "[unhd-%d-%d] %d visited literals out of %d (%.0f%%)",
	  lgl->stats->unhd.count, lgl->stats->unhd.rounds,
	  visited, unassigned, lglpcnt (visited, unassigned));
  lglprt (lgl, 2,
	  "[unhd-%d-%d] %.2f average number visited literals per search",
	  lgl->stats->unhd.count, lgl->stats->unhd.rounds,
	  lglavg (visited, searches));
DONE:
  if (!searches || lgl->mt) { DEL (dfpr, 2*lgl->nvars); dfpr = 0; }
  lglrelwtk (lgl, &work);
  lglrelstk (lgl, &units);
  lglrelstk (lgl, &sccs);
  lglrelstk (lgl, &trds);
  DEL (dfopf, 2*lgl->nvars);
  return dfpr;
}

static void lglsetunhdlim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->unhdreleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->unhdmineff.val) limit = lgl->opts->unhdmineff.val;
  if (lgl->opts->unhdmaxeff.val >= 0 && limit > lgl->opts->unhdmaxeff.val)
    limit = lgl->opts->unhdmaxeff.val;
  limit >>= (pen = lgl->limits->unhd.pen + lglszpen (lgl));
  irrlim = lgl->stats->irr.lits.cur;
  irrlim >>= lgl->limits->simp.pen;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1,
      "[unhide-%d] limit %lld based on %d irredundant literals",
      lgl->stats->unhd.count, (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[unhide-%d] limit %lld with penalty %d = %d + %d",
      lgl->stats->unhd.count, (LGLL) limit,
      pen, lgl->limits->unhd.pen, lglszpen (lgl));
  lgl->limits->unhd.steps = lgl->stats->unhd.steps + limit;
}

static void lglupdunhdpen (LGL * lgl, int success) {
  if (success && lgl->limits->unhd.pen) lgl->limits->unhd.pen--;
  if (!success && lgl->limits->unhd.pen < MAXPEN) lgl->limits->unhd.pen++;
}

static int lglunhide (LGL * lgl) {
  int64_t oldprgss = lgl->stats->prgss, roundprgss = 0;
  int irronly, round, maxrounds, noprgssrounds, success;
  int oldunits, oldfailed, oldtauts, oldhbrs, oldstrd;
  DFPR * dfpr = 0;
  assert (lgl->opts->unhide.val);
  if (lgl->nvars <= 2) return 1;
  lgl->stats->unhd.count++;
  assert (!lgl->unhiding);
  lgl->unhiding = 1;
  assert (!lgl->simp);
  lgl->simp = 1;
  lglstart (lgl, &lgl->times->unhd);
  irronly = !lgl->stats->red.bin || (lgl->stats->unhd.count & 1);
  if (lgl->level > 0) lglbacktrack (lgl, 0);

  maxrounds = lgl->opts->unhdroundlim.val;
  lglsetunhdlim (lgl);

  oldunits = lglunhdunits (lgl);
  oldfailed = lglunhdfailed (lgl);
  oldtauts = lglunhdtauts (lgl);
  oldhbrs = lglunhdhbrs (lgl);
  oldstrd = lglunhdstrd (lgl);
  noprgssrounds = round = 0;
  while (!lgl->mt) {
    if (round >= maxrounds) break;
    if (round > 0) {
      if (roundprgss == lgl->stats->prgss) {
	if (noprgssrounds++ == lgl->opts->unhdlnpr.val) {
	  LOG (1, "too many non progress unhiding rounds");
	  break;
	}
      }
    }
    round++;
    roundprgss = lgl->stats->prgss;
    lgl->stats->unhd.rounds++;
    lglgc (lgl);
    if (!lgl->nvars || lgl->mt) break;
    assert (!dfpr);
    dfpr = lglstampall (lgl, irronly);
    if (!dfpr) break;
    if (!lglunhidefailed (lgl, dfpr)) break;
    if (!lglunhidebintrn (lgl, dfpr, irronly)) break;
    if (!lglunhidelrg (lgl, dfpr, irronly)) break;
    if (lgl->stats->unhd.steps >= lgl->limits->unhd.steps) break;
    irronly = !lgl->stats->red.bin || !irronly;
    DEL (dfpr, 2*lgl->nvars);
    assert (!dfpr);
  }
  if (dfpr) DEL (dfpr, 2*lgl->nvars);
  lglprt (lgl, 1,
    "[unhide-%d-%d] %d units, %d failed, %d tauts, %d hbrs, %d literals",
    lgl->stats->unhd.count, lgl->stats->unhd.rounds,
    lglunhdunits (lgl) - oldunits,
    lglunhdfailed (lgl) - oldfailed,
    lglunhdtauts (lgl) - oldtauts,
    lglunhdhbrs (lgl) - oldhbrs,
    lglunhdstrd (lgl) - oldstrd);
  success = (oldprgss < lgl->stats->prgss);
  lglupdunhdpen (lgl, success);
  assert (lgl->simp);
  lgl->simp = 0;
  assert (lgl->unhiding);
  lgl->unhiding = 0;
  lglrep (lgl, 1 + !success, 'u');
  lglstop (lgl);
  return !lgl->mt;
}

static int lglpar64 (uint64_t i) {
  unsigned x;
  int res = 0;
  for (x = i; x; x = x & (x - 1))
    res = !res;
  return res;
}

static uint64_t lgldec64 (uint64_t i) {
  uint64_t res;
  for (res = i - 1; lglpar64 (res); res--)
    ;
  return res;
}

static void lglgdump (LGL * lgl) {
#ifndef NLGLOG
  const int * p, * start, * top;
  start = lgl->gauss->xors.start;
  top = lgl->gauss->xors.top;
  for (p = start; p < top; p++) {
    if (*p >= NOTALIT) continue;
    fprintf (lgl->out, "c eqn[%ld]", p - start);
    while (*p > 1) fprintf (lgl->out, " %d", *p++);
    fprintf (lgl->out, " = %d\n", *p);
  }
#endif
}

static int lglgaussubclsaux (LGL * lgl, uint64_t signs,  const int * c) {
  int lit, i, min, minocc, tmpocc, blit, tag, other, other2, red, lidx;
  const int * p, * w, * eow, * d, * q;
  HTS * hts;
  assert (lgl->dense);
  minocc = INT_MAX;
  min = i = 0;
  lgl->stats->gauss.steps.extr++;
  for (p = c; (lit = *p); p++) {
    if (lglmarked (lgl, lit)) return 0;
    assert (i < 8 * sizeof signs);
    if (signs & (1ull << i++)) lit = -lit;
    lglsignedmark (lgl, lit);
    tmpocc = lglocc (lgl, lit) + lglhts (lgl, lit)->count;
    assert (tmpocc < INT_MAX);
    if (tmpocc >= minocc) continue;
    minocc = tmpocc;
    min = lit;
  }
  assert (min);
  hts = lglhts (lgl, min);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    lgl->stats->gauss.steps.extr++;
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == LRGCS) continue;
    if (tag == BINCS) {
      other = blit >> RMSHFT;
      if (lglsignedmarked (lgl, other)) return 1;
    } else if (tag == TRNCS) {
      other = blit >> RMSHFT;
      if (!lglsignedmarked (lgl, other)) continue;
      other2 = *p;
      if (lglsignedmarked (lgl, other2)) return 1;
    } else {
      assert (tag == OCCS);
      red = blit & REDCS;
      lidx = blit >> RMSHFT;
      d = lglidx2lits (lgl, OCCS, red, lidx);
      for (q = d; (other = *q); q++)
	if (!lglsignedmarked (lgl, other)) break;
      if (!other) return 1;
    }
  }
  return 0;
}

static int lglgaussubcls (LGL * lgl, uint64_t signs,  const int * c) {
  int res = lglgaussubclsaux (lgl, signs, c), lit;
  const int * p;
  for (p = c; (lit = *p); p++) lglunmark (lgl, lit);
  return res;
}

static int lglgaussextractxoraux (LGL * lgl, const int * c) {
  int lit, val, size, maxsize, negs, start, max, *d, * q;
  int allxors = lgl->opts->gaussextrall.val;
  uint64_t signs;
  const int * p;
  assert (!lgl->level);
  maxsize = lgl->opts->gaussmaxor.val;
  max = negs = size = 0;
  start = lglcntstk (&lgl->gauss->xors);
  for (p = c; (lit = *p); p++) {
    if ((val = lglval (lgl, lit)) > 0) return 0;
    if (val < 0) continue;
    if (lit < 0) { 
      if (!allxors && negs) return 0;
      negs = !negs; 
    }
    if (!max || abs (max) < abs (lit)) max = lit;
    lglpushstk (lgl, &lgl->gauss->xors, lit);
    if (++size > maxsize) return 0;
  }
  assert (negs == 0 || negs == 1);
  if (size <= 1) return 0;
  if (!allxors && negs && max > 0) return 0;
  lglpushstk (lgl, &lgl->gauss->xors, 0);
  d = lgl->gauss->xors.start + start;
  assert (size <= 8 * sizeof signs);
  signs = lgldec64 (1ull << size);
  do {
    if (!lglgaussubcls (lgl, signs, d)) break;
    signs = lgldec64 (signs);
  } while (signs &&
           lgl->stats->gauss.steps.extr < lgl->limits->gauss.steps.extr);
  if (signs) return 0;
  for (q = d; (lit = *q); q++) *q = abs (lit);
  *q = !negs;
  LOGEQN (2, start, "extracted %d-ary XOR constraint",  size);
  lgl->stats->gauss.arity.sum += size;
  if (lgl->stats->gauss.arity.max < size) lgl->stats->gauss.arity.max = size;
  lgl->stats->gauss.extracted++;
  return 1;
}

static int lglgaussextractxor (LGL * lgl, const int * c) {
  int old = lglcntstk (&lgl->gauss->xors), res;
  if (!(res = lglgaussextractxoraux (lgl, c)))
    lglrststk (&lgl->gauss->xors, old);
  return res;
}

static int lglgaussextractsmallit (LGL * lgl, int lit) {
  int allxors = lgl->opts->gaussextrall.val;
  int cls[4], blit, tag, other, other2;
  const int * w, * eow, * p;
  HTS * hts;
  if (lgl->stats->gauss.steps.extr >= lgl->limits->gauss.steps.extr) 
    return 0;
  if (lglval (lgl, lit) > 0) return 1;
  hts = lglhts (lgl, lit);
  w = lglhts2wchs (lgl, hts);
  eow = w + hts->count;
  for (p = w; p < eow; p++) {
    blit = *p;
    tag = blit & MASKCS;
    if (tag == TRNCS || tag == LRGCS) p++;
    if (tag == OCCS || tag == LRGCS) continue;
    other = blit >> RMSHFT;
    if (!allxors && abs (other) < lit) continue;
    cls[0] = lit;
    cls[1] = other;
    if (tag == TRNCS) {
      other2 = *p;
      if (!allxors && abs (other2) < lit) continue;
      cls[2] = other2;
      cls[3] = 0;
    } else {
      assert (tag == BINCS);
      cls[2] = 0;
    }
    lglgaussextractxor (lgl, cls);
  }
  return 1;
}

static int lglgaussextractsmall (LGL * lgl) {
  int64_t before = lgl->stats->gauss.extracted, after, delta;
  int res;
  lglrandlitrav (lgl, lglgaussextractsmallit);
  after = lgl->stats->gauss.extracted;
  delta = after - before;
  res = (delta > INT_MAX) ? INT_MAX : delta;
  return res;
}

static int lglgaussextractlarge (LGL * lgl) {
  const int * p, * c;
  int res = 0;
  for (c = lgl->irr.start; c < lgl->irr.top; c = p + 1) {
    if (lgl->stats->gauss.steps.extr >= lgl->limits->gauss.steps.extr) break;
    if (*(p = c) >= NOTALIT) continue;
    res += lglgaussextractxor (lgl, c);
    while (*p) p++;
  }
  return res;
}

static void lglgaussconeqn (LGL * lgl, int eqn) {
  const int * xors = lgl->gauss->xors.start;
  int i, var;
  lgl->stats->gauss.steps.elim++;
  for (i = eqn; (var = xors[i]) > 1; i++)
    lglpushstk (lgl, lgl->gauss->occs + var, eqn);
}

static void lglgaussdiseqn (LGL * lgl, int eqn) {
  int * xors = lgl->gauss->xors.start, i, var;
  for (i = eqn; (var = xors[i]) > 1; i++) {
    xors[i] = NOTALIT;
    lgl->gauss->garbage++;
    lgl->stats->gauss.steps.elim++;
    lglrmstk (lgl->gauss->occs + var, eqn);
  }
  xors[i] = NOTALIT;
  lgl->gauss->garbage++;
}

static void lglgaussconnect (LGL * lgl) {
  int c, i, eox = lglcntstk (&lgl->gauss->xors), connected, var, vars;
  const int * xors = lgl->gauss->xors.start;
  Stk * occs;
  LOG (2,  "connecting equations");
  assert (!lgl->gauss->occs);
  NEW (lgl->gauss->occs, lgl->nvars);
  vars = connected = 0;
  for (c = 0; c < eox; c = i + 1) {
    lgl->stats->gauss.steps.elim++;
    for (i = c; (var = xors[i]) > 1; i++) {
      occs = lgl->gauss->occs + var;
      if (lglmtstk (occs)) vars++;
      lglpushstk (lgl, lgl->gauss->occs + var, c);
      connected++;
    }
  }
  lglprt (lgl, 1, 
     "[gauss-%d] connected %d occurrences of %d variables (average %.1f)",
      lgl->stats->gauss.count, connected, vars, lglavg (connected, vars));
}

static int lglgaussorderidx (LGL * lgl,  int var) {
  assert (2 <= var && var < lgl->nvars);
  if (!lglmtstk (lgl->gauss->occs + var))
    lglpushstk (lgl, &lgl->gauss->order, var);
  return 1;
}

static void lglgaussorder (LGL * lgl) {
  lglrandidxtrav (lgl, lglgaussorderidx);
  NEW (lgl->gauss->eliminated, lgl->nvars);
}

static void lglgaussdisconnect (LGL * lgl) {
  int idx;
  assert (lgl->gauss->occs);
  LOG (2, "disconnecting equations");
  for (idx = 2; idx < lgl->nvars; idx++)
    lglrelstk (lgl, lgl->gauss->occs + idx);
  DEL (lgl->gauss->occs, lgl->nvars);
  assert (!lgl->gauss->occs);
}

static void lglgaussextract (LGL * lgl) {
  int extracted, lits;
  assert (lglsmallirr (lgl));
  if (lgl->level) lglbacktrack (lgl, 0);
  lglgc (lgl);
  if (lgl->mt) return;
  lgldense (lgl, 1);
  extracted = lglgaussextractsmall (lgl);
  extracted += lglgaussextractlarge (lgl);
  lits = lglcntstk (&lgl->gauss->xors) - extracted;
  lglprt (lgl, 1, "[gauss-%d] extracted %d xors of average arity %.1f",
          lgl->stats->gauss.count, extracted, lglavg (lits, extracted));
  lglsparse (lgl);
  lglgc (lgl);
  if (lgl->mt) return;
  lglfitstk (lgl, &lgl->gauss->xors);
}

static int lglgaussoccs (LGL * lgl, int a) {
  return lglcntstk (lgl->gauss->occs + a);
}

static int lglcmpgauss (LGL * lgl, int a, int b) {
  int res = lglgaussoccs (lgl, a) - lglgaussoccs (lgl, b);
  if (!res) res = a - b;
  return res;
}

#define LGLCMPGAUSS(A,B) lglcmpgauss (lgl, *(A), *(B))

static void lglgaussort (LGL * lgl) {
  int max = lglcntstk (&lgl->gauss->order), rest, * start;
  assert (lgl->gauss->next <= max);
  rest = max - lgl->gauss->next;
  start = lgl->gauss->order.start + lgl->gauss->next;
  lgl->stats->gauss.steps.elim += rest;
  SORT (int, start, rest, LGLCMPGAUSS);
  lglprt (lgl, 3, 
     "[gauss-%d] sorted %d remaining variables",
      lgl->stats->gauss.count, rest);
}

static int lglgausspickeqn (LGL * lgl, int pivot) {
  int res, cand, weight, size, tmp, other, found;
  const int * p, * e, * q;
  Stk * occs;
  assert (lglgaussoccs (lgl, pivot));
  res = -1;
  weight = INT_MAX; size = INT_MAX;
  occs = lgl->gauss->occs + pivot;
  for (p = occs->start; p < occs->top; p++) {
    e = lgl->gauss->xors.start + (cand = *p);
    found = tmp = 0;
    lgl->stats->gauss.steps.elim++;
    for (q = e; (other = *q) > 1; q++) {
      if (lgl->gauss->eliminated[other]) break;
      if (other == pivot) { assert (!found); found++; continue; }
      tmp += lglgaussoccs (lgl, other) - 1;
    }
    if (other > 1) continue;
    assert (found);
    if (res >= 0 && q - e >= size) continue;
    if (res >= 0 && q - e == size && tmp >= weight) continue;
    weight = tmp;
    size = q - e;
    res = cand;
  }
  if (res >= 0)
    LOGEQN (2, res, "picking size %d weight %d equation", size, weight);
  else
    LOG (2, "no uneliminated equation for pivot %d left", pivot);
  return res;
}

static void lglcpystk (LGL * lgl, Stk * dst, Stk * src) {
  const int * p;
  for (p = src->start; p < src->top; p++)
    lglpushstk (lgl, dst, *p);
}

static int lglgaussaddeqn (LGL * lgl, int eqn) {
  const int * p;
  AVar * av;
  int var;
  for (p = lgl->gauss->xors.start + eqn; (var = *p) > 1; p++) {
    av = lglavar (lgl, var);
    if (!av->mark) lglpushstk (lgl, &lgl->clause, var);
    av->mark = !av->mark;
  }
  return var;
}

static void lglgaussubst (LGL * lgl, int pivot, int subst) {
  Stk * occs = lgl->gauss->occs + pivot;
  int eqn, rhs, res;
  const int * p;
  int * q;
  assert (lglcntstk (occs) > 1);
  while (lglcntstk (occs) > 1) {
    if (lglterminate (lgl)) return;
    eqn = occs->start[0];
    if (eqn == subst) eqn = occs->start[1];
    assert (lglmtstk (&lgl->clause));
    LOGEQN (2, subst, "  1st row (kept)     ");
    rhs = lglgaussaddeqn (lgl, eqn);
    LOGEQN (2, eqn, "  2nd row (replaced) ");
    if (lglgaussaddeqn (lgl, subst)) rhs = !rhs;
    lglgaussdiseqn (lgl, eqn);
    q = lgl->clause.start;
    for (p = q; p < lgl->clause.top; p++)
      if (lglmarked (lgl, *p)) *q++ = *p;
    lgl->clause.top = q;
    if (!lglmtstk (&lgl->clause)) {
      res = lglcntstk (&lgl->gauss->xors);
      lglcpystk (lgl, &lgl->gauss->xors, &lgl->clause);
      lglpushstk (lgl, &lgl->gauss->xors, rhs);
      LOGEQN (2, res, "  result row         ");
      lglgaussconeqn (lgl, res);
    } else if (rhs == 0) {
      LOG (2, "trivial result row 0 = 0");
    } else {
      assert (rhs == 1);
      LOG (1, "inconsistent result row 0 = 1 from gaussian elimination");
      lgl->mt= 1;
    }
    lglpopnunmarkstk (lgl, &lgl->clause);
  }
}

static void lglgausschkeliminated (LGL * lgl) {
#ifndef NDEBUG
  int idx, eliminated, occs;
  if (!lgl->opts->check.val) return;
  for (idx = 2; idx < lgl->nvars; idx++) {
    eliminated = lgl->gauss->eliminated[idx];
    occs = lglgaussoccs (lgl, idx);
    if (eliminated == 1) assert (occs == 1);
    if (eliminated == 2) assert (occs == 0);
  }
#endif
}

static void lglgaussgc (LGL * lgl) {
  int count = lglcntstk (&lgl->gauss->xors), * q;
  const int * p;
  if (lgl->gauss->garbage < count/2 + 10000) return;
  lgl->stats->gauss.gcs++;
  lglprt (lgl, 2, 
     "[gauss-%d] collecting %d garbage out of %d",
     lgl->stats->gauss.count, lgl->gauss->garbage, count);
  lglgaussdisconnect (lgl);
  q = lgl->gauss->xors.start;
  for (p = q; p < lgl->gauss->xors.top; p++)
    if (*p != NOTALIT) *q++ = *p;
  lgl->gauss->xors.top = q;
  lglfitstk (lgl, &lgl->gauss->xors);
  lglgaussconnect (lgl);
  lgl->gauss->garbage = 0;
}

static int lglgausselimvar (LGL * lgl, int pivot) {
  int subst, changed, occs, eliminated;
  LOG (2, "eliminating variable %d occurring %d times",
       pivot, lglgaussoccs (lgl, pivot));
  assert (!lgl->gauss->eliminated[pivot]);
  if (!(occs = lglgaussoccs (lgl, pivot))) {
    LOG (2, "variable %d disappeared ", pivot);
    eliminated = 2;
    changed = 0;
  } else if (occs == 1) {
    LOG (2, "eliminated variable %d occurrs exactly once", pivot);
    eliminated = 1;
    changed = 0;
  } else {
    lglgaussgc (lgl);
    subst = lglgausspickeqn (lgl, pivot);
    if (subst >= 0) {
      lglgaussubst (lgl, pivot, subst);
      eliminated = 1;
      changed = 1;
    } else {
      eliminated = 3;
      changed = 0;
    }
  }
  lgl->gauss->eliminated[pivot] = eliminated;
  lglgausschkeliminated (lgl);
  return changed;
}

static void lglgausselim (LGL * lgl) {
  int pivot, changed = 1;
  while (!lgl->mt && lgl->gauss->next < lglcntstk (&lgl->gauss->order)) {
    if (lgl->stats->gauss.steps.elim >= lgl->limits->gauss.steps.elim) break;
    if (lglterminate (lgl)) break;
    if (changed) lglgaussort (lgl);
    pivot = lglpeek (&lgl->gauss->order, lgl->gauss->next++);
    changed = lglgausselimvar (lgl, pivot);
  }
}

static void lglgaussinit (LGL * lgl) {
  assert (!lgl->gauss);
  NEW (lgl->gauss, 1);
}

static void lglgaussreset (LGL * lgl) {
  assert (lgl->gauss);
  if (lgl->gauss->occs) lglgaussdisconnect (lgl);
  if (lgl->gauss->eliminated) DEL (lgl->gauss->eliminated, lgl->nvars);
  lglrelstk (lgl, &lgl->gauss->xors);
  lglrelstk (lgl, &lgl->gauss->order);
  DEL (lgl->gauss, 1);
}

static int lglgaussexp2 (LGL * lgl, int a, int b) {
  assert (lgl->gaussing);
  assert (lglmtstk (&lgl->clause));
  if (lglhasbin (lgl, a, b)) return 0;
  lglpushstk (lgl, &lgl->clause, a);
  lglpushstk (lgl, &lgl->clause, b);
  lglpushstk (lgl, &lgl->clause, 0);
  LOGCLS (2, lgl->clause.start, "gauss exported binary clause");
#ifndef NLGLPICOSAT
  lglpicosatchkcls (lgl);
#endif
  lgladdcls (lgl, REDCS, 0, 0);
  lglclnstk (&lgl->clause);
  return 1;
}

static int lglgaussexp3 (LGL * lgl, int a, int b, int c) {
  assert (lgl->gaussing);
  assert (lglmtstk (&lgl->clause));
  if (lglhastrn (lgl, a, b, c)) return 0;
  lglpushstk (lgl, &lgl->clause, a);
  lglpushstk (lgl, &lgl->clause, b);
  lglpushstk (lgl, &lgl->clause, c);
  lglpushstk (lgl, &lgl->clause, 0);
  LOGCLS (2, lgl->clause.start, "gauss exported ternary clause");
#ifndef NLGLPICOSAT
  lglpicosatchkcls (lgl);
#endif
  lgladdcls (lgl, REDCS, 0, 0);
  lglclnstk (&lgl->clause);
  return 1;
}

static int lglgaussexport (LGL * lgl) {
  int var, size, val, rhs, unit, a, b, c, exported;
  const int * e, * p, * q;
  for (e = lgl->gauss->xors.start; e < lgl->gauss->xors.top; e = p + 1) {
    if (*(p = e) >= NOTALIT) continue;
    while ((var = *p) > 1) p++;
    rhs = *p;
    assert (lglmtstk (&lgl->clause));
    for (q = e; q < p; q++) {
      var = *q;
      val = lglval (lgl, var);
      if (val < 0) continue;
      if (val > 0) { rhs = !rhs; continue; }
      lglpushstk (lgl, &lgl->clause, var);
    }
    size = lglcntstk (&lgl->clause);
    if (!size && !rhs) continue;
    if (!size && rhs) {
      LOGEQN (1, e - lgl->gauss->xors.start, 
              "gauss exporting inconsistent equation");
      return 0;
    }
    a = (size > 0) ? lgl->clause.start[0] : 0;
    b = (size > 1) ? lgl->clause.start[1] : 0;
    c = (size > 2) ? lgl->clause.start[2] : 0;
    lglclnstk (&lgl->clause);
    if (size == 1) {
      unit = a;
      if (!rhs) unit = -unit;
      LOG (1, "gauss exporting unit %d", unit);
      lgl->stats->gauss.units++;
      lglunit (lgl, unit);
    } else if (size == 2) {
      if (rhs) b = -b;
      exported = lglgaussexp2 (lgl, -a, b);
      exported |= lglgaussexp2 (lgl, a, -b);
      if (exported) {
	LOG (1, "gauss exporting equivalence %d = %d", a, b);
	lgl->stats->gauss.equivs++;
      }
    } else if (size == 3 && lgl->opts->gaussexptrn.val) {
      if (!rhs) c = -c;
      exported = lglgaussexp3 (lgl, a, b, c);
      exported |= lglgaussexp3 (lgl, a, -b, -c);
      exported |= lglgaussexp3 (lgl, -a, b, -c);
      exported |= lglgaussexp3 (lgl, -a, -b, c);
      if (exported) {
	LOG (1, "gauss exporting ternary equation %d + %d + %d = 1", 
	     a, b, c);
	lgl->stats->gauss.trneqs++;
      }
    }
  }
  return 1;
}

static void lglsetgausslim (LGL * lgl) {
  int64_t limit, irrlim;
  int pen;
  limit = (lgl->opts->gaussreleff.val*lgl->stats->visits.search)/1000;
  if (limit < lgl->opts->gaussmineff.val) limit = lgl->opts->gaussmineff.val;
  if (lgl->opts->gaussmaxeff.val >= 0 && limit > lgl->opts->gaussmaxeff.val)
    limit = lgl->opts->gaussmaxeff.val;
  limit >>= (pen = lgl->limits->gauss.pen + lglszpen (lgl));
  irrlim = lgl->stats->irr.lits.cur/2;
  irrlim >>= lgl->limits->simp.pen;
  if (limit < irrlim) {
    limit = irrlim;
    lglprt (lgl, 1, "[gauss-%d] limit %lld based on %d irredundant literals",
      lgl->stats->gauss.count, (LGLL) limit, lgl->stats->irr.lits.cur);
  } else
    lglprt (lgl, 1, "[gauss-%d] limit %lld penalty %d = %d + %d",
      lgl->stats->gauss.count, (LGLL) limit,
      pen, lgl->limits->gauss.pen, lglszpen (lgl));
  lgl->limits->gauss.steps.extr = lgl->stats->gauss.steps.extr + limit;
  lgl->limits->gauss.steps.elim = lgl->stats->gauss.steps.elim + limit;
}

static void lglupdgausspen (LGL * lgl, int success) {
  if (success && lgl->limits->gauss.pen)
    lgl->limits->gauss.pen--;
  if (!success && lgl->limits->gauss.pen < MAXPEN)
    lgl->limits->gauss.pen++;
}

static int lglgauss (LGL * lgl) {
  int oldunits, oldequivs, oldtrneqs;
  int units, equivs, trneqs;
  int success;
  assert (lgl->opts->gauss.val);
  if (lgl->mt) return 0;
  if (lgl->nvars <= 2) return 1;
  lglstart (lgl, &lgl->times->gauss);
  assert (!lgl->simp && !lgl->gaussing);
  lgl->simp = lgl->gaussing = 1;
  lgl->stats->gauss.count++;
  lglsetgausslim (lgl);
  lglgaussinit (lgl);
  lglgaussextract (lgl);
  oldunits = lgl->stats->gauss.units;
  oldequivs = lgl->stats->gauss.equivs;
  oldtrneqs = lgl->stats->gauss.trneqs;
  if (!lglmtstk (&lgl->gauss->xors)) {
    lglgaussconnect (lgl);
    lglgaussorder (lgl);
    lglsetgausslim (lgl);
    lglgausselim (lgl);
    if (!lgl->mt && !lglterminate (lgl)) {
      if (lgl->opts->verbose.val >= 3) lglgdump (lgl);
      lglgaussdisconnect (lgl);
      if (!lglgaussexport (lgl) || !lglbcp (lgl)) lgl->mt = 1;
      else if (lgl->limits->gauss.steps.extr > lgl->stats->gauss.steps.extr &&
               lgl->limits->gauss.steps.elim > lgl->stats->gauss.steps.elim)
	lglprt (lgl, 1, "[gauss-%d] fully completed", lgl->stats->gauss.count);
    }
  }
  lglgaussreset (lgl);
  units = lgl->stats->gauss.units - oldunits;
  equivs = lgl->stats->gauss.equivs - oldequivs;
  trneqs = lgl->stats->gauss.trneqs - oldtrneqs;
  success = units || equivs;
  if (!lgl->mt && success && !lglterminate (lgl)) lgldecomp (lgl);
  if (trneqs) success = 1;
  if (lgl->mt)
    lglprt (lgl, 1, "[gauss-%d] proved unsatisfiability",
      lgl->stats->gauss.count);
  else {
    lglprt (lgl, 1, 
      "[gauss-%d] exported %d unary, %d binary and %d ternary equations",
      lgl->stats->gauss.count, units, equivs, trneqs);
  }
  lglupdgausspen (lgl, success);
  lglrep (lgl, 1, 'G');
  assert (lgl->simp && lgl->gaussing);
  lgl->simp = lgl->gaussing = 0;
  lglstop (lgl);
  return !lgl->mt;
}

static int lglprobing (LGL * lgl) {
  if (!lgl->opts->probe.val) return 0;
  if (lgl->opts->prbasic.val) return 1;
  if (!lglsmallirr (lgl)) return 0;
  return 0;
}

#define CARDLOGLEVEL 0

static void lglcard (LGL * lgl) {
  int * card, idx, sign, lit, start, size, count, blit, tag, other, other2, i;
  const int * p, * w, * eow;
  Stk atmostone;
  int64_t sum;
  HTS * hts;
  lglstart (lgl, &lgl->times->card);
  CLR (atmostone);
  lglpushstk (lgl, &atmostone, 0);
  NEW (card, 2*lgl->nvars);
  card += lgl->nvars;
  count = 0;
  sum = 0;
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = idx * sign;
      if (card[lit]) continue;
      start = lglcntstk (&atmostone);
      LOG (CARDLOGLEVEL + 1, "starting clique[%d] for %d", start, lit);
      lglpushstk (lgl, &atmostone, lit);
      card[lit] = start;
      hts = lglhts (lgl, -lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (tag != BINCS) continue;
	other = -(blit >> RMSHFT);
	if (card[other]) continue;
	for (i = start + 1; i < lglcntstk (&atmostone); i++) {
	  other2 = lglpeek (&atmostone, i);
	  if (!lglhasbin (lgl, -other, -other2)) break;
	}
	if (i < lglcntstk (&atmostone)) continue;
	card[other] = start;
	lglpushstk (lgl, &atmostone, other);
	LOG (CARDLOGLEVEL + 1, "adding %d to clique[%d] for %d", other, start, lit);
      }
      size = lglcntstk (&atmostone) - start;
      if (size <= 2) {
	LOG (CARDLOGLEVEL + 1, "resetting clique[%d] for %d of trivial size %d", 
	     start, lit, size);
	while (lglcntstk (&atmostone) > start) {
	  other = lglpopstk (&atmostone);
	  assert (card[other] == start);
	  card[other] = 0;
	}
      } else {
#ifndef NLGLOG
	if (lgl->opts->log.val >= CARDLOGLEVEL) {
	  lglogstart (lgl, CARDLOGLEVEL,
	    "non trivial size %d at-most-one constraint ", size);
	  for (i = start; i < start + size; i++) {
	    if (i > start) fputs (" + ", lgl->out);
	    fprintf (lgl->out, "%d", lglpeek (&atmostone, i));
	  }
	  fputs (" <= 1", lgl->out);
	  lglogend (lgl);
	}
#endif
	lglpushstk (lgl, &atmostone, 0);
	sum += size;
	count++;
      }
    }
  card -= lgl->nvars;
  DEL (card, 2*lgl->nvars);
  lglrelstk (lgl, &atmostone);
  lglprt (lgl, 1, 
    "[card] found %d at-most-one constraints of average size %.1f",
    count, lglavg (sum, count));
  lglstop (lgl);
}

static int lgltreducing (LGL * lgl) { return lgl->opts->transred.val; }

static int lglunhiding (LGL * lgl) { return lgl->opts->unhide.val; }

static int lgldecomposing (LGL * lgl) { return lgl->opts->decompose.val; }

static int lglcgrclosing (LGL * lgl) {
  return lgl->opts->cgrclsr.val && lglsmallirr (lgl);
}

static int lglifting (LGL * lgl) { return lgl->opts->lift.val; }

static int lglblocking (LGL * lgl) {
  if (!lgl->opts->block.val) return 0;
  if (!lglsmallirr (lgl)) return 0;
  if (lgl->nvars <= 2) return 0;
  if (lgl->mt) return 0;
  if (lgl->blkrem) return 1;
  return lgl->stats->irrprgss > lgl->limits->blk.irrprgss;
}

static int lgleliminating (LGL * lgl) {
  if (!lgl->opts->elim.val) return 0;
  if (!lglsmallirr (lgl)) return 0;
  if (lgl->nvars <= 2) return 0;
  if (lgl->mt) return 0;
  if (lgl->elmrem) return 1;
  return lgl->stats->irrprgss > lgl->limits->elm.irrprgss;
}

static int lglreducing (LGL * lgl) {
  int lrg, lrglue;
  if (!lgl->opts->reduce.val) return 0;
  lrg = lgl->stats->red.lrg;
  lrglue = lgl->lrgluereasons;
  assert (lrg >= lrglue);
  return lrg - lrglue >= lgl->limits->reduce.inner;
}

static int lgldefragmenting (LGL * lgl) {
  int relfree;
  if (lgl->stats->pshwchs < lgl->limits->dfg.pshwchs) return 0;
  if (!lgl->nvars) return 0;
  relfree = (100 * lgl->wchs->free + 99) / lgl->nvars;
  return relfree >= lgl->opts->defragfree.val;
}

static int lglrestarting (LGL * lgl) {
  int assumptions;
  if (!lgl->opts->restart.val) return 0;
  if (!lgl->level) return 0;
  if ((assumptions = lglcntstk (&lgl->assume))) {
    if (lgl->assumed < assumptions) return 0;
    assert (lgl->alevel <= lgl->level);
    if (lgl->alevel == lgl->level) return 0;
  }
  return lgl->stats->confs >= lgl->limits->restart.confs;
}

static int lglternresolving (LGL * lgl) { return lgl->opts->ternres.val; }

static int lglgaussing (LGL * lgl) { 
  if (!lglsmallirr (lgl)) return 0;
  return lgl->opts->gauss.val;
}

static int lglcliffing (LGL * lgl) {
  return lgl->opts->cliff.val;
}

static int lglisimp (LGL * lgl) {
  if (!lgl->opts->simplify.val) return 1;

  if (lglternresolving (lgl) && !lglternres (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lglgaussing (lgl) && !lglgauss (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lgldecomposing (lgl) && !lgldecomp (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lglprobing (lgl) && !lglprobe (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lglcgrclosing (lgl) && !lglcgrclsr (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lglifting (lgl) && !lglift (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lglcliffing (lgl) && !lglcliff (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lgl->opts->card.val) lglcard (lgl);

  if (lglunhiding (lgl) && !lglunhide (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lgltreducing (lgl) && !lgltrd (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lglblocking (lgl)) lglblock (lgl);
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lglcceing (lgl) && !lglcce (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lgleliminating (lgl) && !lglelim (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (lgl->opts->card.val) lglcard (lgl);

  if (!lgltopgc (lgl)) return 0;
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  if (!lgl->allphaseset) lglphase (lgl);
  if (lglterminate (lgl)) return 1;
  assert (!lgl->mt);

  lgldefrag (lgl);

  return 1;
}

static void lglregularly (LGL * lgl) {
  if (lglreducing (lgl)) lglreduce (lgl, 0);
  if (lgldefragmenting (lgl)) lgldefrag (lgl);
}

static void lglupdprepint (LGL * lgl, int red) {
  int div = abs (red)/2 + 1;
  int64_t inc;
  assert (red >= 0);
  if (red > 100)
    lglprt (lgl, 1, "[simplification-%d] no increase of limit increments",
	    lgl->stats->simp.count);
  else {
    if (div == 1)
      lglprt (lgl, 1, "[simplification-%d] no reduction of limit increments",
	      lgl->stats->simp.count, div);
    else
      lglprt (lgl, 1, 
        "[simplification-%d] limit increments divided by %d",
	lgl->stats->simp.count, div);

    if (lgl->opts->simplify.val) {
      inc = lgl->opts->cintinc.val / div;
      lgl->limits->simp.cinc += inc;
      lglprt (lgl, 1, 
"[simplification-%d] arithmetic increase of conflict limit interval by %lld",
	lgl->stats->simp.count, (LGLL) inc);
    } else
      lglprt (lgl, 1,
        "[simplification-%d] limit does not change",
	lgl->stats->simp.count);

  }
  lgl->limits->simp.confs = lgl->stats->confs + lgl->limits->simp.cinc;
  lgl->limits->simp.irr = lglnewirrlim (lgl);
  lglprt (lgl, 1,
    "[simplification-%d] new irredundant limit %lld",
    lgl->stats->simp.count, (LGLL) lgl->limits->simp.irr);
  lglprt (lgl, 1,
    "[simplification-%d] new conflict limit %lld",
    lgl->stats->simp.count, (LGLL) lgl->limits->simp.confs);
  lgl->limits->simp.prgss = lgl->stats->prgss;
  if (lgl->limits->simp.pen > 0) {
    lgl->limits->simp.pen--;
    lglprt (lgl, 1,
      "[simplification-%d] simplification penalty reduced to %d",
      lgl->stats->simp.count, lgl->limits->simp.pen);
  }
}

static int lglsimpcntrem (LGL * lgl, int oldrem) {
  int rem = lglrem (lgl), removed = oldrem - rem, pcnt;
  int64_t pcnt64;
  assert (removed >= 0);
  if (removed <= 0) pcnt = 0;
  else {
    pcnt64 = (100*removed) / oldrem;
    pcnt = pcnt64;
  }
  lglprt (lgl, 1, "[simplification-%d] removed %d variables %d%%",
          lgl->stats->simp.count, removed, pcnt);
  return pcnt;
}

static int lglpreprocessing (LGL * lgl, int forced) {
  int res = !lgl->mt, oldrem, pcntremoved;
  assert (!lgl->searching);
  if (forced || !lgl->stats->simp.count) {
    lgl->stats->simp.count++;
    oldrem = lglrem (lgl);
    res = lglisimp (lgl);
    pcntremoved = lglsimpcntrem (lgl, oldrem);
    lglupdprepint (lgl, forced ? INT_MAX : pcntremoved);
  }
  assert (res == !lgl->mt);
  return res;
}

static int lglsimplimhit (LGL * lgl) {

  if (!lgl->opts->inprocessing.val && lgl->stats->simp.count) return 0;

  if (lgl->stats->confs < lgl->limits->simp.confs) return 0;

  lgl->stats->simp.climhit++;
  lglprt (lgl, 1,
    "[simplification-%d] limit hit at %lld conflicts",
    lgl->stats->simp.count + 1, (LGLL) lgl->stats->confs);
  if (lgl->limits->simp.pen)
    lglprt (lgl, 1,
      "[simplification-%d] simplification penalty of %d",
      lgl->stats->simp.count + 1, lgl->limits->simp.pen);
  else
    lglprt (lgl, 1,
      "[simplification-%d] no simplification penalty",
      lgl->stats->simp.count + 1);
  return 1;
}

static int lglinprocessing (LGL * lgl) {
  int res, oldrem;
  assert (lgl->searching);
  if (!lglsimplimhit (lgl)) return !lgl->mt;
  lgl->stats->simp.count++;
  lglstart (lgl, &lgl->times->inpr);
  oldrem = lglrem (lgl);
  res = lglisimp (lgl);
  lglupdprepint (lgl, lglsimpcntrem (lgl, oldrem));
  lglstop (lgl);
  assert (res == !lgl->mt);
  return res;
}

static int lglbcptop (LGL * lgl) {
  int res;
  if (lglbcp (lgl)) res = 1;
  else {
#ifndef NDEBUG
    int tmp =
#endif
    lglana (lgl);
    assert (!tmp);
    if (lgl->conf.lit) {
      LOG (1, "top level propagation produces inconsistency");
      if (!lgl->mt) lgl->mt = 1;
    } else assert (lgl->failed);
    res = 0;
  }
  return res;
}

static int lglimhit (LGL * lgl, Lim * lim) {
  if (!lim) return 0;
  if (lim->decs >= lgl->stats->decisions) {
    lglprt (lgl, 1, "[limits] decision limit %lld hit at %lld decisions",
      (LGLL) lim->decs, (LGLL) lgl->stats->decisions);
    return 1;
  }
  if (lim->confs >= 0 && lgl->stats->confs >= lim->confs) {
    lglprt (lgl, 1, "[limits] conflict limit %lld hit at %lld conflicts",
      (LGLL) lim->confs, (LGLL) lgl->stats->confs);
    return 1;
  }
  return 0;
}

static int lgloop (LGL * lgl, Lim * lim) {
  for (;;) {
    if (lglbcpsearch (lgl) && lglinprocessing (lgl)) {
      if (lglterminate (lgl)) return 0;
      if (!lglsyncunits (lgl)) return 20;
      if (lglfailedass (lgl)) return 20;
      lglregularly (lgl);
      if (lglimhit (lgl, lim)) return 0;
      if (lglrestarting (lgl)) { lglrestart (lgl); continue; }
      if (!lgldecide (lgl)) return 10;
    } else if (!lglana (lgl)) return 20;
  }
}

static int lglsearch (LGL * lgl, Lim * lim) {
  int res;
  assert (!lgl->searching);
  lgl->searching = 1;
  lglstart (lgl, &lgl->times->srch);
  res = lgloop (lgl, lim);
  assert (lgl->searching);
  lgl->searching = 0;
  lglstop (lgl);
  return res;
}

static void lglqschedall (LGL * lgl) {
  int idx;
  assert (!lgl->qscheduling);
  lgl->qscheduling = 1;
  for (idx = 2; idx < lgl->nvars; idx++) lglqsched (lgl, idx);
  lglprt (lgl, 1, "[queue-schedule-all-variables]");
}

static int lgltopsimp (LGL * lgl, int forcesimp) {
  assert (lgl->state == READY);
  if (lgl->mt) return 20;
  if (lglfailedass (lgl)) return 20;
  if (!lglbcptop (lgl)) return 20;
  if (lgl->mt || lglfailedass (lgl)) return 20;
  if (lglterminate (lgl)) return 0;
  if ((forcesimp || lglsimplimhit (lgl)) && !lglpreprocessing (lgl, forcesimp)) return 20;
  if (lglfailedass (lgl)) return 20;
  if (!lgl->qscheduling) {
    if (!lgl->stats->confs) lglcutwidth (lgl);
    lglqschedall (lgl);
  }
  lglrep (lgl, 1, 's');
  return 0;
}

static int lglsolve (LGL * lgl, Lim * lim, int forcesimp) {
  int res;
  lgl->limits->simp.pen = lgl->opts->simpen.val;
  lglstart (lgl, &lgl->times->prep);
  res = lgltopsimp (lgl, forcesimp);
  lglstop (lgl);
  if (res) { assert (res == 20); return res; }
  return lglsearch (lgl, lim);
}

static void lglsetup (LGL * lgl) {
  if (lgl->state == RESET) goto DONE;
  assert (lgl->state & (UNUSED | OPTSET | USED));

  lgl->limits->dfg.pshwchs = lgl->stats->pshwchs + lgl->opts->defragint.val;
  lgl->limits->reduce.inner = lgl->opts->redlinit.val;
  lglboundredl (lgl);
  lgl->limits->reduce.outer = 2*lgl->limits->reduce.inner;

  lgl->limits->blk.irrprgss = -1;
  lgl->limits->elm.irrprgss = -1;
  // lgl->limits->simp.confs = -1;
  lgl->limits->simp.prgss = -1;
  lgl->limits->term.steps = -1;

  lgl->limits->flipint = lgl->opts->flipint.val;
  lgl->phaseneg = lgl->opts->phaseneginit.val;

  lglincrestartl (lgl, 0);

  lgl->rng.w = (unsigned) lgl->opts->seed.val;
  lgl->rng.z = ~lgl->rng.w;
  lgl->rng.w <<= 1;
  lgl->rng.z <<= 1;
  lgl->rng.w += 1;
  lgl->rng.z += 1;
  lgl->rng.w *= 2019164533u, lgl->rng.z *= 1000632769u;

  assert (!lgl->stats->decisions);
  lgl->limits->randec += lgl->opts->randecint.val/2;
  lgl->limits->randec += lglrand (lgl) % lgl->opts->randecint.val;

  lglchkenv (lgl);
DONE:
  TRANS (READY);
}

static void lglinitsolve (LGL * lgl) {
  if (lgl->state != READY) lglsetup (lgl);
  lglredvars (lgl);
  lglfitstk (lgl, &lgl->irr);
#ifndef NCHKSOL
  lglfitstk (lgl, &lgl->orig);
#endif
  lglrep (lgl, 1, '*');
}

#ifndef NLGLPICOSAT
static void lglpicosatchksol (LGL * lgl) {
#ifndef NDEBUG
  int idx, lit, res, * p;
  Val val;
  if (lgl->picosat.res) assert (lgl->picosat.res == 10);
  lglpicosatinit (lgl);
  assert (!picosat_inconsistent (PICOSAT));
  for (idx = 2; idx < lgl->nvars; idx++) {
    val = lglval (lgl, idx);
    assert (val);
    lit = lglsgn (val) * idx;
    picosat_assume (PICOSAT, lit);
  }
  for (p = lgl->eassume.start; p < lgl->eassume.top; p++)
    picosat_assume (PICOSAT, lglimport (lgl, *p));
  res = picosat_sat (PICOSAT, -1);
  assert (res == 10);
  LOG (1, "PicoSAT checked solution");
#endif
}
#endif

static void lgleassign (LGL * lgl, int lit) {
  Ext * ext;
  ext = lglelit2ext (lgl, lit);
  LOG (3, "external assign %d ", lit);
  ext->val = lglsgn (lit);
}

static void lglcomputechanged (LGL * lgl) {
  Ext * ext;
  int eidx;
  lgl->changed = 0;
  for (eidx = 1; eidx <= lgl->maxext; eidx++) {
    ext = lglelit2ext (lgl, eidx);
    if (ext->oldval && ext->oldval != ext->val) lgl->changed++;
    ext->oldval = ext->val;
  }
  LOG (1, "changed %d assignments in extension", lgl->changed);
}

static void lglextend (LGL * lgl) {
  int * p, lit, eidx, ilit, next, satisfied, val, * start, erepr, equiv;
  Ext * ext, * extrepr;
  assert (lgl->state & SATISFIED);
  assert (!(lgl->state & EXTENDED));
  lgleunassignall (lgl);
  for (equiv = 0; equiv <= 1; equiv++) {
    if (equiv)
      LOG (1, "initializing assignment of non-representative externals");
    else
      LOG (1, "initializing assignment of external representatives");
    for (eidx = 1; eidx <= lgl->maxext; eidx++) {
      ext = lglelit2ext (lgl, eidx);
      if (!ext->imported) continue;
      if (equiv != ext->equiv) continue;
      assert (!ext->val);
      if (ext->equiv) {
	erepr = lglerepr (lgl, eidx);
LOG (3, "initializing external %d assignment from external representative %d",
	 eidx, erepr);
	assert (erepr != eidx);
	extrepr = lglelit2ext (lgl, erepr);
	if (!(val = extrepr->val)) {
	  ilit = extrepr->repr;
	  if (ilit) {
	    LOG (3, "using external %d to internal %d mapping",
		 abs (erepr), ilit);
	    val = lglcval (lgl, ilit);
	  } else
LOG (3, "external %d without internal representative", abs (erepr));
	}
	if (erepr < 0) val = -val;
      } else
      if ((ilit = ext->repr)) {
	LOG (3, "using external %d to internal %d mapping", eidx, ilit);
	val = lglcval (lgl, ilit);
      } else {
	LOG (3, "external %d without internal representative", eidx);
	val = 0;
      }
      lit = (val > 0) ? eidx : -eidx;
      lgleassign (lgl, lit);
    }
  }
#ifndef NLGLOG
  lglpushstk (lgl, &lgl->extend, 0);
  (void) lglpopstk (&lgl->extend);
#endif
  start = lgl->extend.start;
  p = lgl->extend.top;
  while (p > start) {
#ifndef NLGLOG
    if (lgl->opts->log.val >= 4) {
      int * q;
      for (q = p; q[-1]; q--)
	;
      LOGCLS (4, q, "next sigma clause to consider");
    }
#endif
    satisfied = 0;
    next = 0;
    do {
      lit = next;
      next = *--p;
      if (!lit || satisfied) continue;
      val = lglederef (lgl, lit);
      assert (!next || val);
      if (val > 0) {
	LOG (4, "sigma clause satisfied by %d", lit);
	satisfied = 1;
      }
    } while (next);
    assert (lit);
    if (satisfied) continue;
    lgleassign (lgl, lit);
  }
  lglcomputechanged (lgl);
  TRANS (EXTENDED);
}

void lglsetphases (LGL * lgl) {
  int elit, phase;
  REQINIT ();
  TRAPI ("setphases");
  REQUIRE (SATISFIED | EXTENDED);
  if (!(lgl->state & EXTENDED)) lglextend (lgl);
  for (elit = 1; elit <= lgl->maxext; elit++) {
    phase = lglederef (lgl, elit);
    assert (abs (phase) <= 1);
    lglesetphase (lgl, elit, phase);
  }
  if (lgl->clone) lglsetphases (lgl->clone);
}

#ifndef NCHKSOL
#include <signal.h>
#include <unistd.h>
static void lglchksol (LGL * lgl) {
  int * p, * c, * eoo = lgl->orig.top, lit, satisfied, sign, idx;
  unsigned bit;
  Ext * ext;
  assert (lglmtstk (&lgl->orig) || !eoo[-1]);
  assert (lgl->state == EXTENDED);
  for (c = lgl->orig.start; c < eoo; c = p + 1) {
    satisfied = 0;
    for (p = c; (lit = *p); p++)
      if (!satisfied && lglederef (lgl, lit) > 0)
	satisfied = 1;
    if (satisfied) continue;
    fflush (stderr);
    lglmsgstart (lgl, 0);
    fprintf (lgl->out, "unsatisfied original external clause");
    for (p = c; (lit = *p); p++) fprintf (lgl->out, " %d", lit);
    lglmsgend (lgl);
    assert (satisfied);
    usleep (1000);
    abort ();	// NOTE: not 'lglabort' on purpose !!
  }
  for (idx = 1; idx <= lgl->maxext; idx++) {
    ext = lglelit2ext (lgl, idx);
    if (!ext->assumed) continue;
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      bit = 1u << (lit < 0);
      if (!(ext->assumed & bit)) continue;
      if (lglederef (lgl, lit) > 0) continue;
      lglprt (lgl, 0, "unsatisfied assumption %d", lit);
      assert (lglederef (lgl, lit) > 0);
      usleep (1000);
      abort ();	// DITO: not 'lglabort' on purpose !!
    }
  }
}
#endif

static void lglclass (LGL * lgl, LGL * from) {
  Ext * extfrom, * extlgl;
  int eidx, cloned;
  REQINIT ();
  ABORTIF (lgl->mt, "can not clone assignment into inconsistent manager");
  ABORTIF (!from, "uninitialized 'from' solver");
  ABORTIF (!(from->state & (SATISFIED | EXTENDED)),
    "require 'from' state to be (SATISFIED | EXTENDED)"); 
  ABORTIF (from->maxext != lgl->maxext,
    "can not clone assignments for different sets of variables");
  if (!(from->state & EXTENDED)) lglextend (from);
  lglreset (lgl);
  lgleunassignall (lgl);
  cloned = lgl->changed = 0;
  for (eidx = 1; eidx <= lgl->maxext; eidx++) {
    extlgl = lglelit2ext (lgl, eidx);
    if (!extlgl->imported) continue;
    extfrom = lglelit2ext (from, eidx);
    ABORTIF (!extfrom->imported, 
      "can not clone assignment of literal imported only by 'to'");
    assert (extfrom->val == 1 || extfrom->val == -1);
    lgleassign (lgl, extfrom->val*eidx);
    cloned++;
  }
  lglcomputechanged (lgl);
  lglprt (lgl, 1, "[class] cloned %d assignments (%d changed)",
          cloned, lgl->changed);
  TRANS (EXTENDED);
#ifndef NCHKSOL
  lglchksol (lgl);
#endif
#ifndef NLGLPICOSAT
  lglpicosatchksol (lgl);
#endif
}

static void lglnegass (LGL * lgl) {
  const int  * p;
  Stk eassume;
  REQINIT ();
  TRAPI ("negass");
  if (lgl->mt) return;
  CLR (eassume);
  for (p = lgl->eassume.start; p < lgl->eassume.top; p++)
    lglpushstk (lgl, &eassume, *p);
  for (p = eassume.start; p < eassume.top; p++)
    lgleadd (lgl, -*p);
  lgleadd (lgl, 0);
  for (p = eassume.start; p < eassume.top; p++)
    lglassume (lgl, *p);
  lglrelstk (lgl, &eassume);
  lgluse (lgl);
  if (lgl->clone) lglnegass (lgl->clone);
}

static int lglcompactify (LGL * lgl, int res) {
  if (!lgl->opts->compact.val) return 0;
  if (!res) return 1;
  if (res == 20) return 1;
  assert (res == 10);
  return lgl->opts->compact.val >= 2;
}

static int lglisat (LGL * lgl, Lim * lim, int simpits) {
  int res, count;
  lglreset (lgl);
  lglinitsolve (lgl);
  res = lglsolve (lgl, lim, 0);
  for (count = 0; !res && count < simpits; count++) {
    lglprt (lgl, 1, "next forced simplification iteration %d", count);
    res = lglsolve (lgl, lim, 1);
  }
  if (lglcompactify (lgl, res)) lglcompact (lgl);
  if (!res) {
    TRANS (UNKNOWN);
    lglrep (lgl, 1, '?');
  }
  if (res == 10) {
    TRANS (SATISFIED);
    lglrep (lgl, 1, '1');
  }
  if (res == 20) {
    TRANS (UNSATISFIED);
    if (!lgl->level && !lgl->mt) assert (lgl->failed);
    lglrep (lgl, 1, '0');
  }
  lglflshrep (lgl);
  if (res == 10) lglextend (lgl);
#ifndef NCHKSOL
  if (res == 10) lglchksol (lgl);
#endif
#ifndef NLGLPICOSAT
  if (res == 10) lglpicosatchksol (lgl);
  if (res == 20) lglpicosatchkunsat (lgl);
#endif
  return res;
}

int lglunclone (LGL * lgl, LGL * from) {
  int res;
  REQINIT ();
  if (lgl->mt) return 20;
  ABORTIF (!from, "uninitialized 'from' solver");
  if (from->mt || (from->state & UNSATISFIED)) {
    lglnegass (lgl);
    res = lglisat (lgl, 0, 0);
  } else if (from->state & (SATISFIED | EXTENDED)) {
    lglclass (lgl, from);
    res = 10;
  } else {
    lglreset (lgl);
    TRANS (UNKNOWN);
    res = 0;
  }
  return res;
}

#define CHKCLONESTATS(STATS) \
do { \
  assert (clone->stats->STATS == orig->stats->STATS); \
} while (0)

static void lglchkclonesamestats (LGL * orig) {
#ifndef NDEBUG
  LGL * clone = orig->clone;
  assert (clone);
  assert (clone->state == orig->state);
  CHKCLONESTATS (confs);
  CHKCLONESTATS (decisions);
  CHKCLONESTATS (bytes.current);
  CHKCLONESTATS (bytes.max);
  CHKCLONESTATS (props.search);
  CHKCLONESTATS (props.simp);
  CHKCLONESTATS (props.lkhd);
  CHKCLONESTATS (visits.search);
  CHKCLONESTATS (visits.simp);
  CHKCLONESTATS (visits.lkhd);
#endif
}

#define CHKCLONE() \
do { \
  if (!lgl->clone) break; \
  lglchkclonesamestats (lgl); \
} while (0)

#define CHKCLONENORES(FUN) \
do { \
  if (!lgl->clone) break; \
  FUN (lgl->clone); \
  CHKCLONE (); \
} while (0)

#define CHKCLONERES(FUN,RES) \
do { \
  int CLONERES; \
  if (!lgl->clone) break; \
  CLONERES = FUN (lgl->clone); \
  ABORTIF (CLONERES != (RES), \
           "%s (lgl->clone) = %d differs from %s (lgl) = %d", \
	   __FUNCTION__, CLONERES, __FUNCTION__, (RES)); \
  CHKCLONE (); \
} while (0)

#define RETURN(FUN,RES) \
do { \
  TRAPI ("return %d", (RES)); \
  CHKCLONERES (FUN, (RES)); \
} while (0)

#define CHKCLONEARGRES(FUN,ARG,RES) \
do { \
  int CLONERES; \
  if (!lgl->clone) break; \
  CLONERES = FUN (lgl->clone, (ARG)); \
  ABORTIF (CLONERES != (RES), \
           "%s (lgl->clone, %d) = %d differs from %s (lgl, %d) = %d", \
	   __FUNCTION__, (ARG), CLONERES, __FUNCTION__, (ARG), (RES)); \
  CHKCLONE (); \
} while (0)

#define RETURNARG(FUN,ARG,RES) \
do { \
  TRAPI ("return %d", (RES)); \
  CHKCLONEARGRES (FUN, (ARG), (RES)); \
} while (0)

static void lglsetlim (LGL * lgl, Lim * lim) {
  int64_t clim, confs, delay, delayed;
  lim->decs = -1;
  if ((clim = lgl->opts->clim.val) < 0) {
    lim->confs = -1;
    lglprt (lgl, 1, "[limits] no conflict limit");
  } else {
    confs = lgl->stats->confs;
    lim->confs = (confs >= LLMAX - clim) ? LLMAX : confs + clim;
    lglprt (lgl, 1, "[limits] conflict limit %lld after %lld conflicts", 
            (LGLL) lim->confs, (LGLL) confs);
  }
  if ((delay = lgl->opts->simpdelay.val) > 0) {
    delayed = lgl->stats->confs + delay;
    if (delayed > lgl->limits->simp.confs) {
      lgl->limits->simp.confs = delayed;
      lglprt (lgl, 1,
	"[limits] simplification delayed by %lld to %lld conflicts",
	(LGLL) delay, (LGLL) lgl->limits->simp.confs);
    } else lglprt (lgl, 1, 
             "[limits] simplification conflict limit already exceeds delay");
  } else lglprt (lgl, 1, 
           "[limits] simplification not delayed since 'simpdelay == 0'");
}

int lglsat (LGL * lgl) {
  int res;
  Lim lim;
  REQINIT ();
  TRAPI ("sat");
  lglstart (lgl, &lgl->times->all);
  lgl->stats->calls.sat++;
  ABORTIF (!lglmtstk (&lgl->clause), "clause terminating zero missing");
  lglsetlim (lgl, &lim);
  res = lglisat (lgl, &lim, 0);
  lglstop (lgl);
  RETURN (lglsat, res);
  return res;
}

int lglmosat (LGL * lgl, void * state, lglnotify f, int * targets) {
  int cint, clim, target, ntargets, rtargets, next, round, done;
  signed char * reported, * r;
  int64_t confs, totalclim;
  const int * p;
  Val val;
  Lim lim;

  REQINIT ();
  ABORTIF (!lglmtstk (&lgl->clause), "clause terminating zero missing");

  TRAPI ("mosat");
  lglstart (lgl, &lgl->times->all);
  lgl->stats->calls.mosat++;

  ntargets = 0;
  for (p = targets; *p; p++) ntargets++;
  NEW (reported, ntargets);
  rtargets = ntargets;

  cint = lgl->opts->mocint.val;
  totalclim = (lgl->opts->clim.val < 0) ? 
                LLMAX : lgl->stats->confs + lgl->opts->clim.val;
  round = 1;
  done = next = 0;

  lglprt (lgl, 1,
    "[mosat-%lld] given %d targets",
    (LGLL) lgl->stats->calls.mosat, ntargets);

  while (!lgl->mt) {

    for (p = targets, r = reported; !done && (target = *p); p++, r++) {
      if (*r) continue;
      val = lglfixed (lgl, target);
      if (val >= 0) {
         if (lgl->state == SATISFIED && lglval (lgl, target) > 0) val = 1;
	 else val = 0;
      }
      if (!val) continue;
      LOG (1, "notify %d %d", target, val);
      done = !f (state, target, val);
      assert (rtargets > 0), rtargets--;
      *r = val;
    }

    round++;
    lglprt (lgl, 1,
      "[mosat-%lld-%d] %d targets remain out of %d (%.0f%%)",
      (LGLL) lgl->stats->calls.mosat, round, rtargets, ntargets,
      lglpcnt (rtargets, ntargets));

    if (!rtargets) break;
    if (lgl->stats->confs > totalclim) break;

    for (;;) {
      assert (next < ntargets);
      if (!lglfixed (lgl, (target = targets[next]))) break;
      if (++next == ntargets) next = 0;
    }

    LOG (1, "focusing on target %d", target);
    lglassume (lgl, target);

    confs = lgl->stats->confs;
    clim = lglmin (cint, lgl->opts->clim.val);
    lim.confs = (confs >= LLMAX - clim) ? LLMAX : confs + clim;
    lim.decs = -1;
    lglprt (lgl, 1, "[limits] conflict limit %lld after %lld conflicts", 
            (LGLL) lim.confs, (LGLL) confs);

    (void) lglisat (lgl, &lim, 0);
    cint += lgl->opts->mocint.val;
  }

  DEL (reported, ntargets);

  lglprt (lgl, 1,
    "[mosat-%lld] solved %d targets out of %d, %d remain",
    (LGLL) lgl->stats->calls.mosat,
    ntargets - rtargets, ntargets, rtargets);

  lglstop (lgl);

  return !rtargets;
}

int lglookahead (LGL * lgl) {
  int ilit, res;
  REQINIT ();
  TRAPI ("lkhd");
  ABORTIF (!lglmtstk (&lgl->eassume), "imcompatible with 'lglassume'");
  ABORTIF (!lglmtstk (&lgl->clause), "clause terminating zero missing");
  lglstart (lgl, &lgl->times->all);
  lglstart (lgl, &lgl->times->lkhd);
  lglreset (lgl);
  assert (!lgl->lkhd);
  lgl->lkhd = 1;
  if (lgl->level) lglbacktrack (lgl, 0);
  if (!lgl->mt && lglbcp (lgl)) {
    ilit = 0;
    if (lgl->opts->lkhd.val == 2 && !lglsmallirr (lgl))
      ilit = lgljwhlook (lgl);
    else switch (lgl->opts->lkhd.val) {
      case 0: ilit = lglislook (lgl); break;
      default:
      case 1: ilit = lgljwhlook (lgl); break;
    }
    res = (!lgl->mt && ilit) ? lglexport (lgl, ilit) : 0;
    assert (!res || !lglelit2ext (lgl, res)->melted);
  } else lgl->mt = 1, res = 0;
  assert (lgl->lkhd);
  lgl->lkhd = 0;
  lglstop (lgl);
  lglstop (lgl);
  TRANS (LOOKED);
  RETURN (lglookahead, res);
  return res;
}

int lglchanged (LGL * lgl) {
  int res;
  REQINIT ();
  TRAPI ("changed");
  REQUIRE (EXTENDED);
  res = lgl->changed;
  RETURN (lglchanged, res);
  return res;
}

int lglsimp (LGL * lgl, int iterations) {
  Lim lim;
  int res;
  REQINIT ();
  TRAPI ("simp %d", iterations);
  ABORTIF (iterations < 0, "negative number of simplification iterations");
  ABORTIF (!lglmtstk (&lgl->clause), "clause terminating zero missing");
  lglstart (lgl, &lgl->times->all);
  lgl->stats->calls.simp++;
  CLR (lim);
  lim.decs = lgl->stats->decisions;
  res = lglisat (lgl, &lim, iterations);
  assert (!lgl->level);
  lglstop (lgl);
  RETURNARG (lglsimp, iterations, res);
  return res;
}

int lglmaxvar (LGL * lgl) {
  int res;
  REQINIT ();
  TRAPI ("maxvar");
  res = lgl->maxext;
  RETURN (lglmaxvar, res);
  return res;
}

int lglincvar (LGL  *lgl) {
  int res;
  REQINIT ();
  TRAPI ("incvar");
  res = lgl->maxext + 1;
  (void) lglimport (lgl, res);
  RETURN (lglincvar, res);
  return res;
}

int lglderef (LGL * lgl, int elit) {
  int res;
  REQINIT ();
  TRAPI ("deref %d", elit);
  lgl->stats->calls.deref++;
  ABORTIF (!elit, "can not deref zero literal");
  REQUIRE (SATISFIED | EXTENDED);
  if (!(lgl->state & EXTENDED)) lglextend (lgl);
  res = lglederef (lgl, elit);
  RETURNARG (lglderef, elit, res);
  return res;
}

int lglfailed (LGL * lgl, int elit) {
  unsigned bit;
  Ext * ext;
  int res;
  REQINIT ();
  TRAPI ("failed %d", elit);
  lgl->stats->calls.failed++;
  ABORTIF (!elit, "can not check zero failed literal");
  REQUIRE (UNSATISFIED | FAILED);
  ABORTIF (abs (elit) > lgl->maxext,
	   "can not check unimported failed literal");
  ext = lglelit2ext (lgl, elit);
  bit = 1u << (elit < 0);
  ABORTIF (!(ext->assumed & bit),
	   "can not check unassumed failed literal");
  if (!(lgl->state & FAILED)) {
    lglstart (lgl, &lgl->times->all);
    lglanafailed (lgl);
    lglstop (lgl);
  }
  res = (ext->failed & bit) != 0;
  RETURNARG (lglfailed, elit, res);
  return res;
}

int lglinconsistent (LGL * lgl) {
  int res;
  TRAPI ("inconsistent");
  res = (lgl->mt != 0);
  RETURN (lglinconsistent, res);
  return res;
}

static int lglefixed (LGL * lgl, int elit) {
  int res, ilit;
  assert (elit);
  if (abs (elit) > lgl->maxext) return 0;
  ilit = lglimport (lgl, elit);
  if (!ilit) res = 0;
  else if (abs (ilit) == 1) res = ilit;
  else res = lglifixed (lgl , ilit);
  return res;
}

int lglfixed (LGL * lgl, int elit) {
  int res;
  REQINIT ();
  TRAPI ("fixed %d", elit);
  lgl->stats->calls.fixed++;
  ABORTIF (!elit, "can not deref zero literal");
  res = lglefixed (lgl, elit);
  RETURNARG (lglfixed, elit, res);
  return res;
}

int lglrepr (LGL * lgl, int elit) {
  int res, eidx = abs (elit);
  REQINIT ();
  TRAPI ("repr %d", elit);
  lgl->stats->calls.repr++;
  if (eidx > lgl->maxext) res = elit;
  else {
    res = lglerepr (lgl, elit);
    if (abs (res) <= 1) res = elit;
  }
  RETURNARG (lglrepr, elit, res);
  return res;
}

void lglfreeze (LGL * lgl, int elit) {
  Ext * ext;
  REQINIT ();
  TRAPI ("freeze %d", elit);
  lgl->stats->calls.freeze++;
  ABORTIF (!elit, "can not freeze zero literal");
  REQUIRE (UNUSED|OPTSET|USED|RESET|SATISFIED|UNSATISFIED|FAILED|LOOKED|
	   UNKNOWN|EXTENDED);
  LOG (2, "freezing external literal %d", elit);
  (void) lglimport (lgl, elit);
  ext = lglelit2ext (lgl, elit);
  ABORTIF (ext->melted, "freezing melted literal %d", elit);
  assert (!ext->blocking && !ext->eliminated);
  ABORTIF (ext->frozen == INT_MAX, "literal %d frozen too often", elit);
  ext->frozen++;
  if (!ext->frozen) lgl->stats->irrprgss++;
  lglmelter (lgl);
  if (lgl->clone) lglfreeze (lgl->clone, elit);
}

int lglitgone (LGL * lgl, int elit) {
  int res;
  REQINIT ();
  TRAPI ("litgone %d", elit);
  ABORTIF (!elit, "can not check zero literal for being gone");
  if (abs (elit) > lgl->maxext) res = 1;
  else if (lglfixed (lgl, elit)) res = 1;
  else if (lglerepr (lgl, elit) != elit) res = 1;
  else res = lglelit2ext (lgl, elit)->eliminated;
  RETURNARG (lglitgone, elit, res);
  return res;
}

int lglitcanbemelted (LGL * lgl, int elit) {
  int res;
  Ext * ext;
  REQINIT ();
  TRAPI ("litcanbemelted %d", elit);
  ABORTIF (!elit, "can not check zero literal for being frozen");
  if (abs (elit) > lgl->maxext) res = 0;
  else if (lglfixed (lgl, elit)) res = 0;
  else if (lglerepr (lgl, elit) != elit) res = 0;
  else if (!(ext = lglelit2ext (lgl, elit))->frozen) res = 0;
  else if (ext->melted) res = 0;
  else res = 1;
  RETURNARG (lglitcanbemelted, elit, res);
  return res;
}

void lglmeltall (LGL * lgl) {
  int idx, melted;
  Ext * ext;
  REQINIT ();
  melted = 0;
  for (idx = 1; idx <= lgl->maxext; idx++) {
    ext = lglelit2ext (lgl, idx);
    ext->melted = 0;
    if (!ext->frozen) continue;
    lgl->stats->irrprgss++;
    ext->frozen = 0;
    melted++;
  }
  lglprt (lgl, 1, "[meltall] melted %d frozen literals", melted);
}

void lglmelt (LGL * lgl, int elit) {
  Ext * ext;
  REQINIT ();
  TRAPI ("melt %d", elit);
  lgl->stats->calls.melt++;
  ABORTIF (!elit, "can not melt zero literal");
  REQUIRE (UNUSED|OPTSET|USED|RESET|
	   SATISFIED|UNSATISFIED|FAILED|UNKNOWN|LOOKED|
	   EXTENDED);
  LOG (2, "melting external literal %d", elit);
  (void) lglimport (lgl, elit);
  ext = lglelit2ext (lgl, elit);
  ABORTIF (!ext->frozen, "can not melt fully unfrozen literal %d", elit);
  assert (!ext->blocking && !ext->eliminated);
  ext->frozen--;
  lglmelter (lgl);
  if (lgl->clone) lglmelt (lgl->clone, elit);
}

static void lglprstart (LGL * lgl) {
  assert (lgl->prefix);
  fputs (lgl->prefix, lgl->out);
  if (lgl->tid >= 0) fprintf (lgl->out, "%d ", lgl->tid);
}

static void lglprs (LGL * lgl, const char * fmt, ...) {
  va_list ap;
  lglprstart (lgl);
  va_start (ap, fmt);
  vfprintf (lgl->out, fmt, ap);
  va_end (ap);
  fputc ('\n', lgl->out);
}

static double lglsqr (double a) { return a*a; }

static void lglgluestats (LGL * lgl) {
  int64_t added, reduced, forcing, resolved, conflicts;
  int64_t wadded, wreduced, wforcing, wresolved, wconflicts;
  int64_t avgadded, avgreduced, avgforcing, avgresolved, avgconflicts;
  double madded, mreduced, mforcing, mresolved, mconflicts;
  double vadded, vreduced, vforcing, vresolved, vconflicts;
  double sadded, sreduced, sforcing, sresolved, sconflicts;
  Stats * s = lgl->stats;
  int glue;
  lglprs (lgl, "");
  lglprs (lgl, "scaledglue%7s %3s %9s %3s %9s %3s %9s %3s %9s",
	  "added","",
	  "reduced","",
	  "forcing","",
	  "resolved","",
	  "conflicts");
  added = reduced = forcing = resolved = conflicts = 0;
  wadded = wreduced = wforcing = wresolved = wconflicts = 0;
  for (glue = 0; glue <= MAXGLUE; glue++) {
    added += s->lir[glue].added;
    reduced += s->lir[glue].reduced;
    forcing += s->lir[glue].forcing;
    resolved += s->lir[glue].resolved;
    conflicts += s->lir[glue].conflicts;
    wadded += glue*s->lir[glue].added;
    wreduced += glue*s->lir[glue].reduced;
    wforcing += glue*s->lir[glue].forcing;
    wresolved += glue*s->lir[glue].resolved;
    wconflicts += glue*s->lir[glue].conflicts;
  }
  avgadded = added ? (((10*wadded)/added+5)/10) : 0;
  avgreduced = reduced ? (((10*wreduced)/reduced+5)/10) : 0;
  avgforcing = forcing ? (((10*wforcing)/forcing+5)/10) : 0;
  avgresolved = resolved ? (((10*wresolved)/resolved+5)/10) : 0;
  avgconflicts = conflicts ? (((10*wconflicts)/conflicts+5)/10) : 0;
  lglprs (lgl, "");
  lglprs (lgl,
    "all %9lld %3.0f %9lld %3.0f %9lld %3.0f %9lld %3.0f %9lld %3.0f",
     (LGLL) added, 100.0,
     (LGLL) reduced, 100.0,
     (LGLL) forcing, 100.0,
     (LGLL) resolved, 100.0,
     (LGLL) conflicts, 100.0);
  lglprs (lgl, "");
  for (glue = 0; glue <= MAXGLUE; glue++) {
    lglprs (lgl,
"%2d  %9lld %3.0f%c%9lld %3.0f%c%9lld %3.0f%c%9lld %3.0f%c%9lld %3.0f%c",
      glue,
      (LGLL) s->lir[glue].added,
	lglpcnt (s->lir[glue].added, added),
	(glue == avgadded) ? '<' : ' ',
      (LGLL) s->lir[glue].reduced,
	lglpcnt (s->lir[glue].reduced, reduced),
	(glue == avgreduced) ? '<' : ' ',
      (LGLL) s->lir[glue].forcing,
	lglpcnt (s->lir[glue].forcing, forcing),
	(glue == avgforcing) ? '<' : ' ',
      (LGLL) s->lir[glue].resolved,
	lglpcnt (s->lir[glue].resolved, resolved),
	(glue == avgresolved) ? '<' : ' ',
      (LGLL) s->lir[glue].conflicts,
	lglpcnt (s->lir[glue].conflicts, conflicts),
	(glue == avgconflicts) ? '<' : ' ');
  }
  lglprs (lgl, "");

  madded = lglavg (wadded, added),
  mreduced = lglavg (wreduced, reduced),
  mforcing = lglavg (wforcing, forcing),
  mresolved = lglavg (wresolved, resolved),
  mconflicts = lglavg (wconflicts, conflicts);

  lglprs (lgl,
    "avg  %14.1f%14.1f%14.1f%14.1f%14.1f",
     madded, mreduced, mforcing, mresolved, mconflicts);

  vadded = vreduced = vforcing = vresolved = vconflicts = 0;
  for (glue = 0; glue <= MAXGLUE; glue++) {
    vadded += s->lir[glue].added * lglsqr (glue - madded);
    vreduced += s->lir[glue].reduced * lglsqr (glue - mreduced);
    vforcing += s->lir[glue].forcing * lglsqr (glue - mforcing);
    vresolved += s->lir[glue].resolved * lglsqr (glue - mresolved);
    vconflicts += s->lir[glue].conflicts * lglsqr (glue - mconflicts);
  }
  sadded = sqrt (lglavg (vadded, added));
  sreduced = sqrt (lglavg (vreduced, reduced));
  sforcing = sqrt (lglavg (vforcing, forcing));
  sresolved = sqrt (lglavg (vresolved, resolved));
  sconflicts = sqrt (lglavg (vconflicts, conflicts));

  lglprs (lgl,
    "std  %14.1f%14.1f%14.1f%14.1f%14.1f",
     sadded, sreduced, sforcing, sresolved, sconflicts);
}

void lglstats (LGL * lgl) {
  Times * ts = lgl->times;
  Stats * s = lgl->stats;
  double t = ts->all, simp, search;
  long long p = s->props.search + s->props.simp + s->props.lkhd;
  int remaining, removed, sum;
  long long v, min;
  int64_t steps;
  REQINIT ();
  lglprs (lgl, "blkd: %d bces, %d removed, %lld resolutions, %lld steps",
	  s->blk.count, s->blk.clauses,
	  (LGLL) s->blk.res, (LGLL) s->blk.steps);
  lglprs (lgl, "blkd: %d blocking literals %.0f%%, %d pure",
	  s->blk.lits, lglpcnt (s->blk.lits, 2*lgl->maxext), s->blk.pure);
  lglprs (lgl, "cces: %d cces, %d eliminated, %d ate %.0f%%, %d abce %.0f%%",
     s->cce.count, s->cce.eliminated,
     s->cce.ate, lglpcnt (s->cce.ate, s->cce.eliminated),
     s->cce.abce, lglpcnt (s->cce.abce, s->cce.eliminated));
  lglprs (lgl, "cces: %lld probed, %d lifted, %d failed",
     (LGLL) s->cce.probed, s->cce.lifted, s->cce.failed);
  lglprs (lgl, "clff: %d cliffs, %d lifted, %d failed",
     s->cliff.count, s->cliff.lifted, s->cliff.failed);
  lglprs (lgl, "clff: %lld decisions, %lld steps",
     (LGLL) s->cliff.decisions, (LGLL) s->cliff.steps);
  lglprs (lgl, "clls: %lld sat, %lld simp, %lld freeze, %lld melt",
    (LGLL) s->calls.sat, (LGLL) s->calls.simp,
    (LGLL) s->calls.freeze, (LGLL) s->calls.melt);
  lglprs (lgl, "clls: %lld add, %lld assume,, %lld deref, %lld failed",
    (LGLL) s->calls.add, (LGLL) s->calls.assume,
    (LGLL) s->calls.deref, (LGLL) s->calls.failed);
  lglprs (lgl, "clls: %lld cassume, %lld mosat",
    (LGLL) s->calls.cassume, (LGLL) s->calls.mosat);
  lglprs (lgl, "coll: %d gcs, %d rescored clauses",
    s->gcs, s->rescored.clauses);
  lglprs (lgl, "cgrs: %d count, %lld esteps, %lld csteps",
    s->cgr.count, (LGLL) s->cgr.esteps, (LGLL) s->cgr.csteps);
  lglprs (lgl, "cgrs: %d eqs, %d units", s->cgr.eq, s->cgr.units);
  lglprs (lgl,
    "cgrs: %d matched (%d ands %.0f%%, %d xors %.0f%%, %d ites %.0f%%)",
    s->cgr.matched.all,
    s->cgr.matched.and, lglpcnt (s->cgr.matched.and, s->cgr.matched.all),
    s->cgr.matched.xor, lglpcnt (s->cgr.matched.xor, s->cgr.matched.all),
    s->cgr.matched.ite, lglpcnt (s->cgr.matched.ite, s->cgr.matched.all));
  lglprs (lgl,
    "cgrs: %d simplified (%d ands %.0f%%, %d xors %.0f%%, %d ites %.0f%%)",
    s->cgr.simplified.all,
    s->cgr.simplified.and,
      lglpcnt (s->cgr.simplified.and, s->cgr.simplified.all),
    s->cgr.simplified.xor,
     lglpcnt (s->cgr.simplified.xor, s->cgr.simplified.all),
    s->cgr.simplified.ite,
     lglpcnt (s->cgr.simplified.ite, s->cgr.simplified.all));
  lglprs (lgl,
    "cgrs: %lld extracted "
    "(%lld ands %.0f%%, %lld xors %.0f%%, %lld ites %.0f%%)",
    (LGLL)s->cgr.extracted.all,
    (LGLL)s->cgr.extracted.and,
      lglpcnt (s->cgr.extracted.and, s->cgr.extracted.all),
    (LGLL)s->cgr.extracted.xor,
      lglpcnt (s->cgr.extracted.xor, s->cgr.extracted.all),
    (LGLL)s->cgr.extracted.ite,
       lglpcnt (s->cgr.extracted.ite, s->cgr.extracted.all));
  lglprs (lgl, "dcps: %d decompositions, %d equivalent %.0f%%",
	  s->decomps, s->equiv.sum, lglpcnt (s->equiv.sum, lgl->maxext));
  lglprs (lgl,
    "decs: %lld decision, %lld random %.3f%%",
    (LGLL) s->decisions,
    (LGLL) s->randecs, lglpcnt (s->randecs, s->decisions));
  lglprs (lgl,
    "decs: %lld flipped %.3f%% (in %lld phases)",
    (LGLL) s->flipped, lglpcnt (s->flipped, s->decisions),
    (LGLL) s->fliphases);
  lglprs (lgl, "elms: %d elims, %d eliminated %.0f%%",
	  s->elm.count, s->elm.elmd, lglpcnt (s->elm.elmd, lgl->maxext));
  lglprs (lgl, "elms: %d small %.0f%%, %d large %.0f%%",
	  s->elm.small.elm, lglpcnt (s->elm.small.elm, s->elm.elmd),
	  s->elm.large, lglpcnt (s->elm.large, s->elm.elmd));
  lglprs (lgl, "elms: %d tried small, %d succeeded %.0f%%, %d failed %.0f%%",
    s->elm.small.tried, s->elm.small.tried - s->elm.small.failed,
      lglpcnt (s->elm.small.tried - s->elm.small.failed, s->elm.small.tried),
    s->elm.small.failed,
      lglpcnt (s->elm.small.failed, s->elm.small.tried));
  lglprs (lgl, "elms: %d subsumed, %d strengthened, %d blocked",
	  s->elm.sub, s->elm.str, s->elm.blkd);
  lglprs (lgl, "elms: %lld copies, %lld resolutions, %lld ipos",
	  (LGLL) s->elm.copies,
	  (LGLL) s->elm.resolutions,
	  (LGLL) s->elm.ipos);
  lglprs (lgl, "elms: %lld subchks, %lld strchks",
	  (LGLL) s->elm.subchks, (LGLL) s->elm.strchks);
  lglprs (lgl, "frcs: %lld computed, %.1f - %.1f average min-cut range",
          (LGLL) s->force.count, s->force.mincut.min, s->force.mincut.max);
  lglprs (lgl, "gaus: %lld extractions, %lld extracted, %.1f size, %d max",
          s->gauss.count, s->gauss.extracted,
	  lglavg (s->gauss.arity.sum, s->gauss.extracted),
	  s->gauss.arity.max);
  lglprs (lgl, "gaus: exported %d units, %d binary and %d ternary equations",
	  s->gauss.units, s->gauss.equivs, s->gauss.trneqs);
  steps = s->gauss.steps.extr + s->gauss.steps.extr;
  lglprs (lgl, "gaus: %d gc, %lld steps, %lld extr %.0f%%, %lld elim %.0f%%",
          s->gauss.gcs, steps,
	  s->gauss.steps.extr, lglpcnt (s->gauss.steps.extr, steps),
	  s->gauss.steps.elim, lglpcnt (s->gauss.steps.elim, steps));
  lglprs (lgl, "glue: %.1f avg, %.1f scaled avg",
	  lglavg (s->clauses.glue, s->clauses.learned),
	  lglavg (s->clauses.scglue, s->clauses.learned));
  lglprs (lgl, "glue: %lld maxredglue=%d (%.0f%%)",
	  (LGLL) s->clauses.maxglue, MAXGLUE,
	  lglpcnt (s->clauses.maxglue, s->clauses.learned));
  sum = s->lift.probed0 + s->lift.probed1;
  lglprs (lgl, "lift: %d phases, "
	  "%d probed (%lld level1 %.0f%%, %lld level2 %.0f%%)",
	  s->lift.count, sum,
	  (LGLL) s->lift.probed0, lglpcnt (s->lift.probed0, sum),
	  (LGLL) s->lift.probed1, lglpcnt (s->lift.probed1, sum));
  lglprs (lgl,
	  "lift: %d units, %d equivalences, %d implications",
	  s->lift.units, s->lift.eqs, s->lift.impls);
  lglprs (lgl,
    "hbrs: %d hbrs = %d trn %.0f%% + %d lrg %.0f%%, %d sub %.0f%%",
    s->hbr.cnt,
    s->hbr.trn, lglpcnt (s->hbr.trn, s->hbr.cnt),
    s->hbr.lrg, lglpcnt (s->hbr.lrg, s->hbr.cnt),
    s->hbr.sub, lglpcnt (s->hbr.sub, s->hbr.cnt));
  lglprs (lgl, "lrnd: %lld clauses, %lld uips %.0f%%, %.1f length, %.1f glue",
	  (LGLL) s->clauses.learned,
	  (LGLL) s->uips, lglpcnt (s->uips, s->clauses.learned),
	  lglavg (s->lits.learned, s->clauses.learned),
	  lglavg (s->clauses.glue, s->clauses.learned));
  lglprs (lgl, "ints: %lld luby (%lld steps), %lld inout (%lld steps)",
          (LGLL) s->luby.count, (LGLL) s->luby.steps,
          (LGLL) s->inout.count, (LGLL) s->inout.steps);
  assert (s->lits.nonmin >= s->lits.learned);
  min = s->lits.nonmin - s->lits.learned;
  sum = s->moved.bin + s->moved.trn;
  lglprs (lgl, "mins: %lld learned lits, %.0f%% minimized",
	  (LGLL) s->lits.learned, lglpcnt (min, s->lits.nonmin));
  lglprs (lgl, "move: moved %lld, %lld binary %.0f%%, %lld ternary %.0f%%",
	  sum,
	  (LGLL) s->moved.bin, lglpcnt (s->moved.bin, sum),
	  (LGLL) s->moved.trn, lglpcnt (s->moved.trn, sum));
  sum = s->otfs.str.dyn.red + s->otfs.str.dyn.irr;
  lglprs (lgl,
	  "otfs: str %d dyn (%d red, %d irr) "
	  "%d drv %.0f%%, %d rst %.0f%%",
	  sum,
	  s->otfs.str.dyn.red, s->otfs.str.dyn.irr,
	  s->otfs.driving, lglpcnt (s->otfs.driving, sum),
	  s->otfs.restarting, lglpcnt (s->otfs.restarting, sum));
  lglprs (lgl, "otfs: sub %d dyn (%d red, %d irr)",
	  s->otfs.sub.dyn.red + s->otfs.sub.dyn.irr,
	  s->otfs.sub.dyn.red, s->otfs.sub.dyn.irr);
  lglprs (lgl,
    "phas: %lld computed, %lld set, %lld pos (%.0f%%), %lld neg (%.0f%%)",
    (LGLL) s->phase.count, (LGLL) s->phase.set,
    (LGLL) s->phase.pos, lglpcnt (s->phase.pos, s->phase.set),
    (LGLL) s->phase.neg, lglpcnt (s->phase.neg, s->phase.set));
  lglprs (lgl, "prbs: %d basic, %lld probed, %d failed, %d lifted",
	  s->prb.basic.count, (LGLL) s->prb.basic.probed,
	  s->prb.basic.failed, s->prb.basic.lifted);
  lglprs (lgl, "prps: %lld props, %.0f props/dec",
	  (LGLL) p, lglavg (s->props.search, s->decisions));
  lglprs (lgl, "prps: %.0f%% srch, %.0f%% simp, %.0f%% lkhd",
	  lglpcnt (s->props.search, p), lglpcnt (s->props.simp, p),
	  lglpcnt (s->props.lkhd, p));
  lglprs (lgl, "psns: %lld searches, %lld hits, %.0f%% hit rate",
	  (LGLL) s->poison.search, (LGLL) s->poison.hits,
	  lglpcnt (s->poison.hits, s->poison.search));
  lglprs (lgl, "queu: %lld new, %lld del, %d maximum priority",
          (LGLL) s->queue.new, (LGLL) s->queue.del, s->queue.max);
  lglprs (lgl, "queu: %lld merged, %lld collected, %lld gcs",
          (LGLL) s->queue.merged, (LGLL) s->queue.col,
	  (LGLL) s->queue.gcs);
  lglprs (lgl, "queu: %lld deprioritized, %.1f lines on average",
          (LGLL) s->queue.deprior.count,
	  lglavg (s->queue.deprior.sum, s->queue.deprior.count));
  lglprs (lgl, "reds: %d count, %d reset, %d acts %.0f%%, %d exp %.0f%%",
	  s->reduced.count, s->reduced.reset,
	  s->acts, lglpcnt (s->acts, s->reduced.count),
	  s->reduced.geom, lglpcnt (s->reduced.geom, s->reduced.count));
  lglprs (lgl, "reds: %d arithmetic %.0f%%, %d double %.0f%%",
	  s->reduced.arith, lglpcnt (s->reduced.arith, s->reduced.count),
	  s->reduced.arith2, lglpcnt (s->reduced.arith2, s->reduced.arith));
  lglprs (lgl, "rmbd: %d removed, %d red %.0f%%",
	  s->bindup.removed, s->bindup.red,
	  lglpcnt (s->bindup.red, s->bindup.removed));
  sum = s->restarts.count + s->restarts.skipped;
  lglprs (lgl, "rsts: %d restarts %.0f%%, %d skipped %.0f%%",
	  s->restarts.count, lglpcnt (s->restarts.count, sum),
	  s->restarts.skipped, lglpcnt (s->restarts.skipped, sum));
  lglprs (lgl, "rsts: %d kept %.1f%% average %.1f%%",
	  s->restarts.kept.count,
	  lglpcnt (s->restarts.kept.count, s->restarts.count),
	  lglavg (s->restarts.kept.sum, s->restarts.kept.count));
  lglprs (lgl,
    "simp: %d count (%d ilim %.0f%%, %d plim %.0f%%, %d clim %0.f%%)",
    s->simp.count,
    s->simp.ilimhit, lglpcnt (s->simp.ilimhit, s->simp.count),
    s->simp.plimhit, lglpcnt (s->simp.plimhit, s->simp.count),
    s->simp.climhit, lglpcnt (s->simp.climhit, s->simp.count));
  lglprs (lgl, "trnr: %d count, %d bin, %d trn, %lld steps",
	  s->trnr.count, s->trnr.bin, s->trnr.trn, (LGLL) s->trnr.steps);
  lglprs (lgl, "tops: %d fixed %.0f%%, %d iterations",
	  s->fixed.sum, lglpcnt (s->fixed.sum, lgl->maxext), s->iterations);
  lglprs (lgl, "trds: %d transitive reductions, %d removed, %d failed",
	  s->trd.count, s->trd.red, s->trd.failed);
  lglprs (lgl, "trds: %lld nodes, %lld edges, %lld steps",
	  (LGLL) s->trd.lits, (LGLL) s->trd.bins,
	  (LGLL) s->trd.steps);
  lglprs (lgl, "unhd: %d count, %d rounds, %lld steps",
	  s->unhd.count, s->unhd.rounds, (LGLL) s->unhd.steps);
  lglprs (lgl, "unhd: %d non-trivial sccs of average size %.1f",
	  s->unhd.stamp.sccs,
	  lglavg (s->unhd.stamp.sumsccsizes, s->unhd.stamp.sccs));
  sum = lglunhdunits (lgl);
  lglprs (lgl, "unhd: %d units, %d bin, %d trn, %d lrg",
	  sum, s->unhd.units.bin, s->unhd.units.trn, s->unhd.units.lrg);
  sum = lglunhdfailed (lgl);
  lglprs (lgl, "unhd: %d failed, %d stamp, %d lits, %d bin, %d trn, %d lrg",
	  sum, s->unhd.stamp.failed, s->unhd.failed.lits,
	  s->unhd.failed.bin, s->unhd.failed.trn, s->unhd.units.lrg);
  sum = lglunhdtauts (lgl);
  lglprs (lgl,
	  "unhd: %d tauts, %d bin %.0f%%, %d trn %.0f%%, %d lrg %.0f%%",
	  sum,
	  s->unhd.tauts.bin, lglpcnt (s->unhd.tauts.bin, sum),
	  s->unhd.tauts.trn, lglpcnt (s->unhd.tauts.trn, sum),
	  s->unhd.tauts.lrg, lglpcnt (s->unhd.tauts.lrg, sum));
  lglprs (lgl,
	  "unhd: %d tauts, %d stamp %.0f%%, %d red %.0f%%",
	  sum,
	  s->unhd.stamp.trds, lglpcnt (s->unhd.stamp.trds, sum),
	  s->unhd.tauts.red, lglpcnt (s->unhd.tauts.red, sum));
  sum = lglunhdhbrs (lgl);
  lglprs (lgl,
	  "unhd: %d hbrs, %d trn %.0f%%, %d lrg %.0f%%, %d red %.0f%%",
	  sum,
	  s->unhd.hbrs.trn, lglpcnt (s->unhd.hbrs.trn, sum),
	  s->unhd.hbrs.lrg, lglpcnt (s->unhd.hbrs.lrg, sum),
	  s->unhd.hbrs.red, lglpcnt (s->unhd.hbrs.red, sum));
  sum = lglunhdstrd (lgl);
  lglprs (lgl,
	  "unhd: %d strd, %d bin %.0f%%, %d trn %.0f%%, "
	  "%d lrg %.0f%%, %d red %.0f%%",
	  sum,
	  s->unhd.units.bin, lglpcnt (s->unhd.units.bin, sum),
	  s->unhd.str.trn, lglpcnt (s->unhd.str.trn, sum),
	  s->unhd.str.lrg, lglpcnt (s->unhd.str.lrg, sum),
	  s->unhd.str.red, lglpcnt (s->unhd.str.red, sum));
  removed = s->fixed.sum + s->elm.elmd + s->equiv.sum;
  remaining = lgl->maxext - removed;
  assert (remaining >= 0);
  lglprs (lgl, "vars: %d remaining %.0f%% and %d removed %.0f%% out of %d",
	  remaining, lglpcnt (remaining, lgl->maxext),
	  removed, lglpcnt (removed, lgl->maxext),
	  lgl->maxext);
  lglprs (lgl, "vars: %d fixed %.0f%%, "
		     "%d eliminated %.0f%%, "
		     "%d equivalent %.0f%%",
	  s->fixed.sum, lglpcnt (s->fixed.sum, lgl->maxext),
	  s->elm.elmd, lglpcnt (s->elm.elmd, lgl->maxext),
	  s->equiv.sum, lglpcnt (s->equiv.sum, lgl->maxext));
  v = s->visits.search + s->visits.simp + s->visits.lkhd;
  lglprs (lgl, "vsts: %lld visits, %.0f%% srch, %.0f%% simp, %.0f%% lkhd",
	  (LGLL) v, lglpcnt (s->visits.search, v),
	  lglpcnt (s->visits.simp, v), lglpcnt (s->visits.lkhd, v));
  lglprs (lgl, "vsts: %.1f search visits per propagation %.1f per conflict",
          lglavg (s->visits.search, s->props.search),
          lglavg (s->visits.search, s->confs));
  lglprs (lgl, "wchs: %lld pushed, %lld enlarged, %d defrags",
	   (LGLL) s->pshwchs, (LGLL) s->enlwchs, s->defrags);
  lglgluestats (lgl);
  lglprs (lgl, "");
  lglprs (lgl, "%lld decisions, %lld conflicts, %.1f conflicts/sec",
	  (LGLL) s->decisions, (LGLL) s->confs,
	  lglavg (s->confs, t));
  lglprs (lgl, "%lld propagations, %.1f megaprops/sec",
	  (LGLL) p, lglavg (p/1e6, t));
  lglprs (lgl, "%.1f seconds, %.1f MB", t, lglmaxmb (lgl));
  lglprs (lgl, "");
  lglprs (lgl, "%8.3f %3.0f%% analysis", ts->ana, lglpcnt (ts->ana, t));
  lglprs (lgl, "%8.3f %3.0f%% block", ts->blk, lglpcnt (ts->blk, t));
  lglprs (lgl, "%8.3f %3.0f%% bump", ts->bump, lglpcnt (ts->bump, t));
  lglprs (lgl, "%8.3f %3.0f%% card", ts->card, lglpcnt (ts->card, t));
  lglprs (lgl, "%8.3f %3.0f%% cce", ts->cce, lglpcnt (ts->cce, t));
  lglprs (lgl, "%8.3f %3.0f%% cliff", ts->cliff, lglpcnt (ts->cliff, t));
  lglprs (lgl, "%8.3f %3.0f%% cutwidth", ts->ctw, lglpcnt (ts->ctw, t));
  lglprs (lgl, "%8.3f %3.0f%% decide", ts->dec, lglpcnt (ts->dec, t));
  lglprs (lgl, "%8.3f %3.0f%% force", ts->force, lglpcnt (ts->force, t));
  lglprs (lgl, "%8.3f %3.0f%% gc", ts->gc, lglpcnt (ts->gc, t));
  lglprs (lgl, "%8.3f %3.0f%% cgrclsr", ts->cgr, lglpcnt (ts->cgr, t));
  lglprs (lgl, "%8.3f %3.0f%% decomp", ts->dcp, lglpcnt (ts->dcp, t));
  lglprs (lgl, "%8.3f %3.0f%% defrag", ts->dfg , lglpcnt (ts->dfg , t));
  lglprs (lgl, "%8.3f %3.0f%% elim", ts->elm, lglpcnt (ts->elm, t));
  lglprs (lgl, "%8.3f %3.0f%% gauss", ts->gauss, lglpcnt (ts->gauss, t));
  lglprs (lgl, "%8.3f %3.0f%% lift", ts->lft, lglpcnt (ts->lft, t));
  lglprs (lgl, "%8.3f %3.0f%% mincls", ts->mcls, lglpcnt (ts->mcls, t));
  lglprs (lgl, "%8.3f %3.0f%% phase", ts->phs, lglpcnt (ts->phs, t));
  lglprs (lgl, "%8.3f %3.0f%% probe", ts->prb.all, lglpcnt (ts->prb.all, t));
  lglprs (lgl, "%8.3f %3.0f%% reduce", ts->red , lglpcnt (ts->red , t));
  lglprs (lgl, "%8.3f %3.0f%% restart", ts->rsts, lglpcnt (ts->rsts, t));
  lglprs (lgl, "%8.3f %3.0f%% ternres", ts->trn, lglpcnt (ts->trn, t));
  lglprs (lgl, "%8.3f %3.0f%% transred", ts->trd, lglpcnt (ts->trd, t));
  lglprs (lgl, "%8.3f %3.0f%% unhide", ts->unhd, lglpcnt (ts->unhd, t));
  lglprs (lgl, "==================================");
  simp = ts->prep + ts->inpr;
  lglprs (lgl, "%8.3f %3.0f%% preprocessing   %3.0f%%",
	  ts->prep,
	  lglpcnt (ts->prep, t),
	  lglpcnt (ts->prep, simp));
  lglprs (lgl, "%8.3f %3.0f%% inprocessing    %3.0f%%",
	  ts->inpr,
	  lglpcnt (ts->inpr, t),
	  lglpcnt (ts->inpr, simp));
  lglprs (lgl, "==================================");
  lglprs (lgl, "%8.3f %3.0f%% simplifying", simp, lglpcnt (simp, t));
  lglprs (lgl, "%8.3f %3.0f%% lookahead", 
          ts->lkhd, lglpcnt (ts->lkhd, t));
  search = ts->srch - ts->inpr;
  lglprs (lgl, "%8.3f %3.0f%% search", search, lglpcnt (search, t));
  lglprs (lgl, "==================================");
  lglprs (lgl, "%8.3f %3.0f%% all", t, 100.0);
  fflush (lgl->out);
}

#define PRF(F) \
do { \
  assert (lgl->prefix); \
  fprintf (lgl->out, "%sfeature %s %d\n", \
    lgl->prefix, # F, lgl->stats->features.F); \
} while (0)

static void lglcomputecog (LGL * lgl) {
  int eidx, minval, maxval, minvar, maxvar, count;
  int64_t tmp, avgval, avgvar;
  Ext * ext;
  count = minvar = maxvar = 0;
  avgval = avgvar = 0;
  minval = INT_MAX;
  maxval = -1;
  for (eidx = 1; eidx <= lgl->maxext; eidx++) {
    ext = lglelit2ext (lgl, eidx);
    if (!ext->cog.count) continue;
    tmp = 10*ext->cog.sum;
    tmp /= ext->cog.count;
    tmp += 5;
    tmp /= 10;
    assert (0 < tmp && tmp < INT_MAX);
    if (tmp < minval) minval = tmp, minvar = eidx;
    if (tmp > maxval) maxval = tmp, maxvar = eidx;
    avgval += tmp;
    avgvar += eidx;
    count++;
  }
  if (count) {
    avgval *= 10; avgval /= count; avgval += 5; avgval /= 10;
    avgvar *= 10; avgvar /= count; avgvar += 5; avgvar /= 10;
  }
  lgl->stats->features.cog.val.min = minval;
  lgl->stats->features.cog.val.avg = avgval;
  lgl->stats->features.cog.val.max = maxval;
  lgl->stats->features.cog.var.min = minvar;
  lgl->stats->features.cog.var.avg = avgvar;
  lgl->stats->features.cog.var.max = maxvar;
}

void lglprintfeatures (LGL * lgl) {
  PRF (vars);
  PRF (lits.total);
  PRF (lits.pos);
  PRF (lits.neg);
  PRF (clauses.total);
  PRF (clauses.unit);
  PRF (clauses.bin);
  PRF (clauses.trn);
  PRF (clauses.lrg);
  lglcomputecog (lgl);
  PRF (cog.val.min);
  PRF (cog.val.avg);
  PRF (cog.val.max);
  PRF (cog.var.min);
  PRF (cog.var.avg);
  PRF (cog.var.max);
}

int64_t lglgetprops (LGL * lgl) {
  REQINIT ();
  return lgl->stats->props.search + lgl->stats->props.simp;
}

int64_t lglgetconfs (LGL * lgl) {
  REQINIT ();
  return lgl->stats->confs;
}

int64_t lglgetdecs (LGL * lgl) {
  REQINIT ();
  return lgl->stats->decisions;
}

void lglsizes (LGL * lgl) {
  lglprt (lgl, 0, "sizeof (int) == %ld", (long) sizeof (int));
  lglprt (lgl, 0, "sizeof (unsigned) == %ld", (long) sizeof (unsigned));
  lglprt (lgl, 0, "sizeof (void*) == %ld", (long) sizeof (void*));
  lglprt (lgl, 0, "sizeof (Stk) == %ld", (long) sizeof (Stk));
  lglprt (lgl, 0, "sizeof (Fun) == %ld", (long) sizeof (Fun));
  lglprt (lgl, 0, "sizeof (AVar) == %ld", (long) sizeof (AVar));
  lglprt (lgl, 0, "sizeof (DVar) == %ld", (long) sizeof (DVar));
  lglprt (lgl, 0, "sizeof (EVar) == %ld", (long) sizeof (EVar));
  lglprt (lgl, 0, "sizeof (Gat) == %ld", (long) sizeof (Gat));
  lglprt (lgl, 0, "sizeof (Stats.lir) == %ld", (long) sizeof ((Stats*)0)->lir);
  lglprt (lgl, 0, "sizeof (Stats) == %ld", (long) sizeof (Stats));
  lglprt (lgl, 0, "sizeof (LGL) == %ld", (long) sizeof (LGL));
  lglprt (lgl, 0, "MAXVAR == %ld", (long) MAXVAR);
  lglprt (lgl, 0, "MAXREDLIDX == %ld", (long) MAXREDLIDX);
  lglprt (lgl, 0, "MAXIRRLIDX == %ld", (long) MAXIRRLIDX);
}

#define LGLRELSTK(MGR,STKPTR) \
do { \
  assert (lglmtstk (STKPTR)); \
  lglrelstk ((MGR), (STKPTR)); \
} while (0)

static void lglrelqueue (LGL * lgl) {
  Qln * p, * up;
  for (p = lgl->queue.bottom; p; p = up) { up = p->up; DEL (p, 1); }
  for (p = lgl->queue.merged; p; p = up) { up = p->up; DEL (p, 1); }
  for (p = lgl->queue.free; p; p = up) { up = p->up; DEL (p, 1); }
  CLR (lgl->queue);
}

void lglrelease (LGL * lgl) {
  lgldealloc dealloc;
  int i;

  REQINIT ();
  if (lgl->clone) lglrelease (lgl->clone), lgl->clone = 0;
  TRAPI ("release");

  // Heap state starts here:

  DEL (lgl->avars, lgl->szvars);
  DEL (lgl->drail, lgl->szdrail);
  DEL (lgl->dvars, lgl->szvars);
  DEL (lgl->ext, lgl->szext);
  DEL (lgl->i2e, lgl->szvars);
  DEL (lgl->doms, 2*lgl->szvars);
  DEL (lgl->jwh, 2*lgl->szvars);
  DEL (lgl->queue.nodes, lgl->szvars);
  DEL (lgl->vals, lgl->szvars);

  lglrelqueue (lgl);
  lglrelctk (lgl, &lgl->control);

  lglrelstk (lgl, &lgl->assume);
  lglrelstk (lgl, &lgl->clause);
  lglrelstk (lgl, &lgl->eclause);
  lglrelstk (lgl, &lgl->eassume);
  lglrelstk (lgl, &lgl->extend);
  lglrelstk (lgl, &lgl->fassume);
  lglrelstk (lgl, &lgl->cassume);
  lglrelstk (lgl, &lgl->frames);
#ifndef NCHKSOL
  lglrelstk (lgl, &lgl->orig);
#endif
  lglrelstk (lgl, &lgl->trail);
  lglrelstk (lgl, &lgl->wchs->stk);

  lglrelstk (lgl, &lgl->irr);
  for (i = 0; i <= MAXGLUE; i++) lglrelstk (lgl, &lgl->red[i]);

  // The following heap allocated memory has no state:

  LGLRELSTK (lgl, &lgl->seen);
  LGLRELSTK (lgl, &lgl->lcaseen);
  LGLRELSTK (lgl, &lgl->poisoned);

  DEL (lgl->limits, 1);
  DEL (lgl->times, 1);
  DEL (lgl->timers, 1);
  DEL (lgl->red, MAXGLUE+1);
  DEL (lgl->wchs, 1);

  if (lgl->fltstr) DEL (lgl->fltstr, 1);
  if (lgl->cbs) DEL (lgl->cbs, 1);		// next to last
  lgldelstr (lgl, lgl->prefix);			// last

  // adjust upfront the mem counters ...

  lgldec (lgl, sizeof *lgl->stats);
  lgldec (lgl, sizeof *lgl->opts);
  lgldec (lgl, sizeof *lgl->mem);
  lgldec (lgl, sizeof *lgl);

  assert (getenv ("LGLEAK") || !lgl->stats->bytes.current);

  if (lgl->closeapitrace == 1) fclose (lgl->apitrace);
  if (lgl->closeapitrace == 2) pclose (lgl->apitrace);

#ifndef NDEBUG
#ifndef NLGLPICOSAT
  if (lgl->tid < 0 && lgl->picosat.solver) {
    if (lgl->opts->verbose.val > 0 && lgl->opts->check.val > 0)
      picosat_stats (PICOSAT);
    picosat_reset (PICOSAT);
    lgl->picosat.solver = 0;
  }
#endif
#endif

  if ((dealloc = lgl->mem->dealloc)) {
     void * memstate = lgl->mem->state;
     if (lgl->stats) dealloc (memstate, lgl->stats, sizeof *lgl->stats);
     if (lgl->times) dealloc (memstate, lgl->times, sizeof *lgl->times);
     if (lgl->opts) dealloc (memstate, lgl->opts, sizeof *lgl->opts);
     assert (lgl->mem);
     dealloc (memstate, lgl->mem, sizeof *lgl->mem);
     dealloc (memstate, lgl, sizeof *lgl);
  } else {
    free (lgl->stats);
    free (lgl->times);
    free (lgl->opts);
    free (lgl->mem);
    free (lgl);
  }
}

void lglutrav (LGL * lgl, void * state, void (*trav)(void *, int)) {
  int elit, val;
  REQINIT ();
  if (!lgl->mt && !lglbcp (lgl)) lgl->mt = 1;
  if (!lgl->mt) lglgc (lgl);
  if (lgl->mt) return;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  for (elit = 1; elit <= lgl->maxext; elit++) {
    if (!(val = lglefixed (lgl, elit))) continue;
    trav (state, (val < 0) ? -elit : elit);
  }
}

void lgletrav (LGL * lgl, void * state, void (*trav)(void *, int, int)) {
  int elit, erepr;
  REQINIT ();
  if (!lgl->mt && !lglbcp (lgl)) lgl->mt = 1;
  if (!lgl->mt) lglgc (lgl);
  if (lgl->mt) return;
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  for (elit = 1; elit <= lgl->maxext; elit++) {
    if (lglefixed (lgl, elit)) continue;
    erepr = lglerepr (lgl, elit);
    if (erepr == elit) continue;
    trav (state, elit, erepr);
  }
}

void lglctrav (LGL * lgl, void * state, void (*trav)(void *, int)) {
  int idx, sign, lit, blit, tag, red, other, other2;
  const int * p, * w, * eow, * c;
  HTS * hts;
  REQINIT ();
  if (!lgl->mt && !lglbcp (lgl)) lgl->mt = 1;
  if (!lgl->mt) lglgc (lgl);
  if (lgl->mt) { trav (state, 0); return; }
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  for (idx = 2; idx < lgl->nvars; idx++)
    for (sign = -1; sign <= 1; sign += 2) {
      lit = sign * idx;
      assert (!lglval (lgl, lit));
      hts = lglhts (lgl, lit);
      w = lglhts2wchs (lgl, hts);
      eow = w + hts->count;
      for (p = w; p < eow; p++) {
	blit = *p;
	tag = blit & MASKCS;
	red = blit & REDCS;
	if (tag == TRNCS || tag == LRGCS) p++;
	if (red) continue;
	if (tag != BINCS && tag != TRNCS) continue;
	other = blit >> RMSHFT;
	if (abs (other) < idx) continue;
	assert (!lglval (lgl, other));
	if (tag == TRNCS) {
	  other2 = *p;
	  if (abs (other2) < idx) continue;
	  assert (!lglval (lgl, other2));
	} else other2 = 0;
	trav (state, lglexport (lgl, lit));
	trav (state, lglexport (lgl, other));
	if (other2) trav (state, lglexport (lgl, other2));
	trav (state, 0);
      }
    }
  for (c = lgl->irr.start; c < lgl->irr.top; c = p + 1) {
    p = c;
    if (*p >= NOTALIT) continue;
    while ((other = *p)) {
      assert (!lglval (lgl, other));
      trav (state, lglexport (lgl, other));
      p++;
    }
    trav (state, 0);
  }
}

static void lgltravcounter (void * voidptr, int lit) {
  int * cntptr = voidptr;
  if (!lit) *cntptr += 1;
}

static void lgltravprinter (void * voidptr, int lit) {
  FILE * out = voidptr;
  if (lit) fprintf (out, "%d ", lit);
  else fprintf (out, "0\n");
}

void lglprint (LGL * lgl, FILE * file) {
  int count = 0;
  lglctrav (lgl, &count, lgltravcounter);
  fprintf (file, "p cnf %d %d\n", lglmaxvar (lgl), count);
  lglctrav (lgl, file, lgltravprinter);
}

void lglrtrav (LGL * lgl, void * state, void (*trav)(void *, int, int)) {
  int idx, sign, lit, blit, tag, red, other, other2, glue;
  const int * p, * c, * w, * eow;
  Stk * lir;
  HTS * hts;
  REQINIT ();
  if (lgl->mt) return;
  lglgc (lgl);
  if (lgl->level > 0) lglbacktrack (lgl, 0);
  for (idx = 2; idx < lgl->nvars; idx++) {
    if (lglval (lgl, idx)) continue;
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
	if (!red) continue;
	if (tag != BINCS && tag != TRNCS) continue;
	other = blit >> RMSHFT;
	if (abs (other) < idx) continue;
	assert (!lglval (lgl, other));
	if (tag == TRNCS) {
	  other2 = *p;
	  if (abs (other2) < idx) continue;
	  assert (!lglval (lgl, other2));
	} else other2 = 0;
	trav (state, lglexport (lgl, lit), 0);
	trav (state, lglexport (lgl, other), 0);
	if (other2) trav (state, lglexport (lgl, other2), 0);
	trav (state, 0, 0);
      }
    }
  }
  for (glue = 0; glue < MAXGLUE; glue++) {
    lir = lgl->red + glue;
    for (c = lir->start; c < lir->top; c = p + 1) {
      p = c;
      if (*p >= NOTALIT) continue;
      while ((other = *p)) {
	assert (!lglval (lgl, other));
	trav (state, lglexport (lgl, other), 0);
	p++;
      }
      trav (state, 0, 0);
    }
  }
}

static void lgltravallu (void * voidptr, int unit) {
  Trv * state = voidptr;
  state->trav (state->state, unit);
  state->trav (state->state, 0);
}

static void lgltravalle (void * voidptr, int lit, int repr) {
  Trv * state = voidptr;
  state->trav (state->state, -lit);
  state->trav (state->state, repr);
  state->trav (state->state, 0);
  state->trav (state->state, lit);
  state->trav (state->state, -repr);
  state->trav (state->state, 0);
}

void lgltravall (LGL * lgl, void * state, void (*trav)(void *, int)) {
  Trv travstate;
  travstate.state = state;
  travstate.trav = trav;
  lglutrav (lgl, &travstate, lgltravallu);
  lgletrav (lgl, &travstate, lgltravalle);
  lglctrav (lgl, state, trav);
}

static void lglfjadd (LGL * lgl, int elit) {
  int ilit = elit ? lglimport (lgl, elit) : 0;
  lglpushstk (lgl, &lgl->clause, ilit);
  if (ilit) return;
  if (!lglsimpcls (lgl)) lgladdcls (lgl, REDCS, 0, 1);
  lglclnstk (&lgl->clause);
}

static void lglfjradd (LGL * lgl, int elit, int glue) {
  int ilit = elit ? lglimport (lgl, elit) : 0;
  lglpushstk (lgl, &lgl->clause, ilit);
  if (ilit) return;
  if (!lglsimpcls (lgl)) lgladdcls (lgl, REDCS, glue, 1);
  lglclnstk (&lgl->clause);
}

static void lglcpyopts (LGL * dst, const LGL * src) {
  const Opt * s;
  Opt * d;
  for (s = FIRSTOPT (src), d = FIRSTOPT (dst); s <= LASTOPT (src); s++, d++) {
    assert (d <= LASTOPT (dst));
    assert (!strcmp (d->lng, s->lng));
    if (!strcmp (s->lng, "clim")) continue;
    if (!strcmp (s->lng, "dlim")) continue;
    if (d->val == s->val) continue;
    lglsetopt (dst, s->lng, s->val);
  }
}

void lglresetforked (LGL * lgl) {
  REQINIT ();
  ABORTIF (!lgl->forked, "not forked");
  lgl->forked = lgl->bruteforked = 0;
}

static void lglcompletefork (LGL * dst, LGL * src) {
  // NOTE sync with 'lglimport'
  int eidx, erepr, val;
  Ext * dext, * rext;
  for (eidx = 1; eidx <= src->maxext; eidx++) {
    erepr = lglerepr (src, eidx);
    if (eidx >= dst->szext) lgladjext (dst, eidx);
    if (eidx > dst->maxext) {
      dst->maxext = eidx;
      lglmelter (dst);
    }
    rext = lglelit2ext (src, erepr);
    dext = lglelit2ext (dst, eidx);
    if (erepr != eidx) {
      assert (!dext->imported);
      dext->repr = erepr;
      dext->equiv = 1;
      dext->imported = 1;
    } else if (abs (rext->repr) > 1) continue;
    else {
      val = rext->repr;
      assert (val == lglefixed (src, eidx));
      assert (!dext->imported);
      dext->repr = val;
      dext->imported = 1;
    }
  }
}

static LGL * lglforkaux (LGL * lgl, int brutefork, int complete) {
  int nass, elit;
  const int * p;
  LGL * res;
  REQINIT ();
  ABORTIF (lgl->forked, "can not fork twice yet");
  lglstart (lgl, &lgl->times->all);
  lglrep (lgl, 1, 'f');
  if (!lgl->mt && !lglbcp (lgl)) lgl->mt = 1;
  if (!lgl->mt) lglgc (lgl);
  res = lglminit (lgl->mem->state,
		  lgl->mem->alloc, lgl->mem->realloc, lgl->mem->dealloc);
  lglcpyopts (res, lgl);
  lglctrav (lgl, res, (void(*)(void*,int)) lgleadd);
  if (!res->mt) lglrtrav (lgl, res, (void (*)(void *, int, int)) lglfjradd);
  if (complete) lglcompletefork (res, lgl);
  nass = lglcntstk (&lgl->assume);
  if ((res->bruteforked = (nass && brutefork)))
    LOG (1, "brute forking with %d assumptions", nass);
  else LOG (1, "non-brute forking with %d assumptions", nass);
  assert (lglmtstk (&res->fassume));
  for (p = lgl->eassume.start; p < lgl->eassume.top; p++) {
    lglpushstk (res, &res->fassume, (elit = * p));
    if (!res->bruteforked) lglassume (res, elit);
    else lglfjadd (res, elit), lglfjadd (res, 0);
  }
  lgl->forked = 1;
  lglstop (lgl);
  return res;
}

LGL * lglfork (LGL * lgl, int complete) {
  return lglforkaux (lgl, 0, complete);
}

LGL * lglbrutefork (LGL * lgl, int complete) {
  return lglforkaux (lgl, 1, complete);
}

static void lglreqinit (LGL *lgl) { REQINIT (); }

static void lglforkmerge (LGL * to, LGL * from) {
  const int * p;
  Trv travstate;
  if (!from->bruteforked) {
    travstate.state = to;
    travstate.trav = (void(*)(void*,int)) lgleadd;
    lglutrav (from, &travstate, lgltravallu);
    lgletrav (from, &travstate, lgltravalle);
  }
  for (p = from->fassume.start; p < from->fassume.top; p++)
    lgleassume (to, *p);
}

int lgljoin (LGL * to, LGL * from) {
  int elit, ok, ilit, val, res, expected;
  LGL * lgl = to;
  const int * p;
  REQINIT ();
  assert (lgl == to);
  ABORTIF (!to->forked, "expected forked state");
  lglreqinit (from);
  lglstart (to, &to->times->all);
  if (to->level > 0) lglbacktrack (to, 0);
  if ((from->state & (UNSATISFIED|FAILED))) {
    expected = 20;
    if (lglmtstk (&from->fassume)) {
      assert (!from->bruteforked);
      assert ((from->state & UNSATISFIED));
      assert (from->mt);
      LOG (1, "importing empty clause");
      to->mt = 1;
    } else {
      for (p = from->fassume.start; p < from->fassume.top; p++) {
	elit = *p;
	if (from->bruteforked || lglfailed (from, elit))
	  lglfjadd (to, -elit);
      }
      lglfjadd (to, 0);
      lglforkmerge (to, from);
    }
  } else if ((from->state & (SATISFIED|EXTENDED))) {
    expected = 10;
    lglforkmerge (to, from);
    ok = lglbcp (to);
    if (ok) {
      for (elit = 1; ok && elit <= to->maxext; elit++) {
	if (elit > from->maxext) continue;
	if (!lglelit2ext (from, elit)->imported) continue;
	val = lglderef (from, elit);
	if (!val) continue;
	ilit = lglimport (to, elit);
	if (abs (ilit) == 1) continue;
	if (lglval (to, ilit)) continue;
	lgldassume (to, (val < 0) ? -ilit : ilit);
	ok = lglbcp (to);
      }
    }
  } else {
    expected = 0;
    lglforkmerge (to, from);
  }
  lglrelease (from);
  if (to->mt) assert (!expected || expected == 20), res = 20;
  else if (expected) {
    res = lglisat (to, 0, 0);
    assert (!res || res == expected);
  } else if (to->conf.lit ||
	   to->next2 < to->next ||
	   to->next < lglcntstk (&to->trail)) {
    if (to->level > 0) lglbacktrack (to, 0), res = 0;
    else {
      if (!to->conf.lit) {
	assert (to->next2 < to->next || to->next < lglcntstk (&to->trail));
	(void) lglbcp (to);
      }
      if (to->conf.lit) {
	  if (!to->mt) {
	    LOG (1, "empty clause after fork merging");
	    to->mt = 1;
	  }
	  res = 20;
      } else res = 0;
    }
  } else res = 0;
  to->forked = 0;
  lglrep (to, 1, 'j');
  lglstop (to);
  return res;
}

#ifndef NDEBUG

void lgldump (LGL * lgl) {
  int idx, sign, lit, blit, tag, red, other, other2, glue;
  const int * p, * w, * eow, * c, * top;
  Stk * lir;
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
	  if (abs (other) < idx) continue;
	  other2 = 0;
	} else if (tag == TRNCS) {
	  other = blit >> RMSHFT;
	  if (abs (other) < idx) continue;
	  other2 = *p;
	  if (abs (other2) < idx) continue;
	} else continue;
	fprintf (lgl->out, "%s %d %d", red ? "red" : "irr", lit, other);
	if (tag == TRNCS) fprintf (lgl->out, " %d", other2);
	fprintf (lgl->out, "\n");
      }
    }
  top = lgl->irr.top;
  for (c = lgl->irr.start; c < top; c = p + 1) {
    p = c;
    if (*p >= NOTALIT) continue;
    fprintf (lgl->out, "irr");
    while (*p) fprintf (lgl->out, " %d", *p++);
    fprintf (lgl->out, "\n");
  }
  for (glue = 0; glue < MAXGLUE; glue++) {
    lir = lgl->red + glue;
    top = lir->top;
    for (c = lir->start; c < top; c = p + 1) {
      p = c;
      if (*p >= NOTALIT) continue;
      fprintf (lgl->out, "glue%d", glue);
      while (*p) fprintf (lgl->out, " %d", *p++);
      fprintf (lgl->out, "\n");
    }
  }
}

#endif
