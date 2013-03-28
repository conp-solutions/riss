/*-------------------------------------------------------------------------*/
/* Copyright 2010-2012 Armin Biere Johannes Kepler University Linz Austria */
/*-------------------------------------------------------------------------*/

#ifndef lglib_h_INCLUDED
#define lglib_h_INCLUDED

#include <stdio.h>				// for 'FILE'
#include <stdlib.h>				// for 'int64_t'

//--------------------------------------------------------------------------

#define LGL_UNKNOWN 0
#define LGL_SATISFIABLE 10
#define LGL_UNSATISFIABLE 20

//--------------------------------------------------------------------------

typedef struct LGL LGL;

//--------------------------------------------------------------------------

LGL * lglinit (void);				// constructor
void lglrelease (LGL *);			// destructor

// externally provided memory manager ...

typedef void * (*lglalloc) (void*mem, size_t);
typedef void (*lgldealloc) (void*mem, void*, size_t);
typedef void * (*lglrealloc) (void*mem, void *ptr, size_t old, size_t);

LGL * lglminit (void *mem, lglalloc, lglrealloc, lgldealloc);

LGL * lglclone (LGL *);				// identical copy

int lglunclone (LGL * dst, LGL * src);

//--------------------------------------------------------------------------
/* functions used for forking (cloning) and joining (merging):

  so far there is only a 'one parent to one forked child' relation:

   LGL * base, * forked;

   // initialize and work with 'base'
   // add assumptions to 'base'

   forked = lglforked (base);

   // assumptions of 'base' have been copied as well
   // work with 'forked' but not with 'base'

   lglsat (forked);

   res = lgljoin (base, forked);	// merge and release

   // this copies back units and equivalences and
   // also failed assumptions from 'forked' to 'base'

   if (res == 20) {
     ... = lglfailed (base, lit);
   }

   // continue working with 'base'
*/

// They handle assumptions properly both in cloning and merging and copy
// options except for decision and conflict limits. The result of 'lglfork'
// has a copy of the clauses and assumptions.  Units and equivalent literals
// are not copied in 'lglfork', unless 'complete' is non zero.
//
LGL * lglfork (LGL *, int complete);		// copy constructor

// Almost same semantics as 'lglfork', but this time assumptions are added
// as unit clauses.  So the only thing that can be merged back in a
// following 'lgljoin', is a clause that consists of the negated assumptions
// respectively the empty clause if there were no assumptions.
//
LGL * lglbrutefork (LGL *, int complete);	// copy constructor

// During 'lgljoin' units are copied back as well as detected equivalences.
//
int lgljoin (LGL * to, LGL * from);		// join 'from' into 'to'
						// AND release 'from'

// The forked copy remembers the old assumptions and whether 'lglfork' or
// 'lglbrutefork' was used.  So 'lgljoin' is from the user perspective
// independent of how the clone was forked.  However, in certain situations
// you might want to continue with the original (forked) solver without
// joining it.  In this case you can reset the internal 'forked' flag
// with the following function.  In this case of course the originally
// 'forked' solver can not be 'joined' again latter.
//
void lglresetforked (LGL *);

//--------------------------------------------------------------------------

const char * lglversion ();

void lglbnr (const char * name,
             const char * prefix,
	     FILE * file);			// ... banner

void lglusage (LGL *);				// print usage "-h"
void lglopts (LGL *, const char * prefix, int);	// ... defaults "-d" | "-e"
void lglrgopts (LGL *);				// ... option ranges "-r"
void lglsizes (LGL *);				// ... data structure sizes

//--------------------------------------------------------------------------
// setters and getters for options

void lglsetout (LGL *, FILE*);			// output file for report
void lglsetprefix (LGL *, const char*);		// prefix for messages

FILE * lglgetout (LGL *);
const char * lglgetprefix (LGL *);

void lglsetopt (LGL *, const char *, int);	// set option value
int lglreadopts (LGL *, FILE *);		// read and set options
int lglgetopt (LGL *, const char *);		// get option value
int lglhasopt (LGL *, const char *);		// exists option?

int lglgetoptminmax (LGL *, const char *, int * minptr, int * maxptr);

void * lglfirstopt (LGL *);			// option iterator: first

void * lglnextopt (LGL *, 			// option iterator: next
                   void * iterator, 
                   const char ** nameptr,
		   int *valptr, int *minptr, int *maxptr);

// individual ids for logging and statistics:

void lglsetid (LGL *, int tid, int tids);

// Set default phase of a literal.  Any decision on this literal will always
// try this phase.  Note, that this function does not have any effect on
// eliminated variables.  Further equivalent variables share the same forced 
// phase and thus if they are set to different default phases, only the last
// set operation will be kept.

void lglsetphase (LGL *, int lit);
void lglresetphase (LGL *, int lit);	// Stop forcing phase in decisions.

// Assume the solver is in the SATISFIABLE state (after 'lglsat' or
// 'lglsimp'), then calling 'lglsetphases' will copy the current assignment
// as default phases.

void lglsetphases (LGL *);

//--------------------------------------------------------------------------
// call back for abort

void lglonabort (LGL *, void * state, void (*callback)(void* state));

//--------------------------------------------------------------------------
// write and read API trace

void lglwtrapi (LGL *, FILE *);
void lglrtrapi (LGL *, FILE *);

//--------------------------------------------------------------------------
// traverse units, equivalences, remaining clauses, or all clauses:

void lglutrav (LGL *, void * state, void (*trav)(void *, int unit));
void lgletrav (LGL *, void * state, void (*trav)(void *, int lit, int repr));
void lglctrav (LGL *, void * state, void (*trav)(void *, int lit));
void lgltravall (LGL *, void * state, void (*trav)(void *state, int lit));

//--------------------------------------------------------------------------

void lglprint (LGL *, FILE *);			// in DIMACS format

//--------------------------------------------------------------------------
// main interface as in PicoSAT (see 'picosat.h' for more information)

int lglmaxvar (LGL *);
int lglincvar (LGL *);

void lgladd (LGL *, int lit);
void lglassume (LGL *, int lit);		// assume single units

void lglcassume (LGL *, int lit);		// assume clause
						// (at most one)

void lglfixate (LGL *);				// add assumptions as units
void lglmeltall (LGL *);			// melt everything

int lglsat (LGL *);
int lglsimp (LGL *, int iterations);

int lglderef (LGL *, int lit);			// neg=false, pos=true
int lglfixed (LGL *, int lit);			// dito but toplevel

int lglfailed (LGL *, int lit);			// dito for assumptions
int lglinconsistent (LGL *);			// contains empty clause?
int lglchanged (LGL *);				// model changed
void lglflushcache (LGL *);			// reset cache size

/* Return representative from equivalence class if literal is not top-level
 * assigned nor eliminated.
 */
int lglrepr (LGL *, int lit);

//--------------------------------------------------------------------------
// Multiple-Objective SAT tries to solve multiple objectives at the same
// time.  If a target can be satisfied or falsified the user is notified via
// a callback function of type 'lglnotify'. The argument is an abstract
// state (of the user), the target literal and as last argument the value
// (-1 = unsatisfiable, 1 = satisfiable).  If the notification callback
// returns 1 the 'lglmosat' procedure continues, otherwise it stops.  It
// returns a non zero value iff only all targets have been either proved to
// be satisfiable or unsatisfiable.  Even if 'val' given to the notification
// function is '1', it can not assume that the current assignment is
// complete, e.g. satisfies the formula.
//
typedef int (*lglnotify)(void *state, int target, int val);

int lglmosat (LGL *, void * state, lglnotify, int * targets);

//--------------------------------------------------------------------------
// Incremental interface provides reference counting for indices, i.e.
// unfrozen indices become invalid after next 'lglsat' or 'lglsimp'.
// This is actually a reference counter for variable indices still in use
// after the next 'lglsat' or 'lglsimp' call.  It is actually variable based
// and only applies to literals in new clauses or used as assumptions. 
/*
  LGL * lgl = lglinit ();
  int res;
  lgladd (lgl, -14); lgladd (lgl, 2); lgladd (lgl, 0);  // binary clause
  lgladd (lgl, 14); lgladd (lgl, -1); lgladd (lgl, 0);  // binary clause
  lglassume (lgl, 1);                                   // assume '1'
  lglfreeze (lgl, 1);                                   // will use '1' below
  lglfreeze (lgl, 14);                                  // will use '14 too
  // frozen (1) && frozen (14)
  res = lglsat ();
  (void) lglderef (lgl, 1);                             // fine
  (void) lglderef (lgl, 2);                             // fine
  (void) lglderef (lgl, 3);                             // fine
  (void) lglderef (lgl, 14);                            // fine
  // lgladd (lgl, 2);                                   // ILLEGAL
  // lglassume (lgl, 2);                                // ILLEGAL
  lgladd (lgl, -14); lgladd (lgl, 1); lgladd (lgl, 0);  // binary clause
  lgladd (lgl, 15); lgladd (lgl, 0);                    // fine!
  lglmelt (14);                                         // '1' discarded
  res = lglsat ();
  // frozen (1)
  (void) lglderef (lgl, 1);                             // fine
  (void) lglderef (lgl, 2);                             // fine
  (void) lglderef (lgl, 3);                             // fine
  (void) lglderef (lgl, 14);                            // fine
  (void) lglderef (lgl, 15);                            // fine
  // lglassume (lgl, 2);                                // ILLEGAL
  // lglassume (lgl, 14);                               // ILLEGAL
  lgladd (lgl, 1);                                      // still frozen
  lglmelt (lgl, 1);
  res = lglsat (lgl);
  // none frozen
  // lgladd(lgl, 1);                                    // ILLEGAL
*/

void lglfreeze (LGL *, int lit);
void lglmelt (LGL *, int lit);

//--------------------------------------------------------------------------
// Returns a good look ahead literal or zero if all potential literals have
// been eliminated or have been used as blocking literals in blocked clause
// elimination.  Zero is also returned if the formula is already
// inconsistent.  The lookahead literal is made usable again, for instance
// if it was not frozen during a previous SAT call and thus implicitly
// became melted.  Therefore it can be added in a unit clause.

int lglookahead (LGL *);

//--------------------------------------------------------------------------
// stats:

void lglflushtimers (LGL *lgl);			// after interrupt etc.

void lglstats (LGL *);
void lglprintfeatures (LGL *);
int64_t lglgetconfs (LGL *);
int64_t lglgetdecs (LGL *);
int64_t lglgetprops (LGL *);
size_t lglbytes (LGL *);
int lglnvars (LGL *);
double lglmb (LGL *);
double lglmaxmb (LGL *);
double lglsec (LGL *);
double lglprocesstime (void);

//--------------------------------------------------------------------------
// low-level parallel support through call backs

void lglseterm (LGL *, int (*term)(void*), void*);

void lglsetproduceunit (LGL *, void (*produce)(void*, int), void*);
void lglsetconsumeunits (LGL *, void (*consume)(void*,int**,int**), void*);

void lglsetlockeq (LGL *, int * (*lock)(void*), void *);
void lglsetunlockeq (LGL *, void (*unlock)(void*,int cons,int prod), void *);

void lglsetconsumedunits (LGL *, void (*consumed)(void*,int), void*);

void lglsetmsglock (LGL *, void (*lock)(void*), void (*unlock)(void*), void*);
void lglsetime (LGL *, double (*time)(void));

// term: called regularly if set: terminate if non zero return value
// produce: called for each derived unit if set
// consume: called regularly if set: import derived units
// lock: return zero terminated vector of externally derived units
// unlock: unlock using the locked list and return number of consumed units

#endif
