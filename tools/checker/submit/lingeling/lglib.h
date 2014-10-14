/* Copyright 2010 Armin Biere, Johannes Kepler University, Linz, Austria */

#ifndef lglib_h_INCLUDED
#define lglib_h_INCLUDED

typedef struct LGL LGL;

LGL * lglinit (void);				// constructor
void lglrelease (LGL *);			// destructor

void lglsetopt (LGL *, const char *, int);	// set option value
int lglgetopt (LGL *, const char *);		// get option value
int lglhasopt (LGL *, const char *);		// exists option?
void lglusage (LGL *);				// print usage "-h"
void lglopts (LGL *, const char * prefix);	// ... current options "-d"
void lglrgopts (LGL *);				// ... option ranges "-r"
void lglbnr (const char * name);		// ... banner
void lglsizes (void);				// ... data structure sizes

// main interface as in PicoSAT:
int lglmaxvar (LGL *);
void lgladd (LGL *, int lit);
int lglsat (LGL *);
int lglderef (LGL *, int lit);			// neg=false, pos=true

// stats:
void lglstats (LGL *);
int lglgetconfs (LGL *);
int lglgetdecs (LGL *);
double lglgetprops (LGL *);
double lglmb (LGL *);
double lglmaxmb (LGL *);
double lglsec (LGL *);
double lglprocesstime (void);

// threaded support:

void lglseterm (LGL *, int (*term)(void*), void*);
void lglsetproduce (LGL *, void (*produce)(void*, int), void*);
void lglsetconsume (LGL *, void (*consume)(void*,int**,int**), void*);

// threaded support for logging and statistics:
void lglsetid (LGL *, int);
void lglsetconsumed (LGL *, void (*consumed)(void*,int), void*);
void lglsetmsglock (LGL *, void (*lock)(void*), void (*unlock)(void*), void*);
void lglsetime (LGL *, double (*time)(void));

// term: called regularly if set, terminate if non zero return value
// unit: called for each derived unit in this instances of the library
// lock: return zero terminated vector of externally derived units
// unlock: unlock using the locked list and return number of consumed units

#endif
