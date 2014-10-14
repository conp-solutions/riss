/* Copyright 2010-2012 Armin Biere, Johannes Kepler University, Linz, Austria */
#include "lglib.h"

#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

static double startime;
static size_t allocated, maxallocated;
static FILE * statsfile, * histfile;

#define FORK_LIMIT		20000
#define BRUTE_FORK_LIMIT	100000

#define INC(BYTES) \
do { \
  allocated += (BYTES); \
  if (allocated > maxallocated) maxallocated = allocated; \
} while (0)

#define DEC(BYTES) \
do { \
  assert (allocated >= (BYTES)); \
  allocated -= (BYTES); \
} while (0)

#define NEW(PTR,N) \
do { \
  size_t BYTES = (N) * sizeof *(PTR); \
  (PTR) = malloc (BYTES); \
  if(!(PTR)) { die ("out of memory"); exit (1); } \
  memset ((PTR), 0, BYTES); \
  INC (BYTES); \
} while (0)

#define DEL(PTR,N) \
do { \
  size_t BYTES = (N) * sizeof *(PTR); \
  DEC (BYTES); \
  free (PTR); \
  (PTR) = 0; \
} while (0);

#define RSZ(PTR,O,N) \
do { \
  size_t OBYTES = (O) * sizeof *(PTR); \
  size_t NBYTES = (N) * sizeof *(PTR); \
  DEC (OBYTES); \
  (PTR) = realloc ((PTR), NBYTES); \
  if (!(PTR)) die ("out of memory"); \
  INC (NBYTES); \
} while (0)

#define ENLARGE(STACK) \
do { \
  size_t oldbytes, newbytes; \
  oldbytes = sz ## STACK * sizeof *STACK; \
  DEC (oldbytes); \
  sz ## STACK = sz ## STACK ? 2*sz ## STACK : 1; \
  newbytes = sz ## STACK * sizeof *STACK; \
  STACK = realloc (STACK, newbytes); \
  if (!(STACK)) die ("out of memory"); \
  INC (newbytes); \
} while (0)

#define PUSH(STACK,ELEM) \
do { \
  if (n ## STACK >= sz ## STACK) ENLARGE (STACK); \
  STACK[n ## STACK++] = ELEM; \
} while (0)

typedef struct Worker { 
  LGL * lgl;
  struct { 
    LGL * lgl;
    int count, bcount;
    int64_t decs, confs, props;
    pthread_mutex_t lock;
  } forked;
  int last, res;
  pthread_t thread;
} Worker;

static Worker * workers;
static int nworkers;

#define WID(W) \
(assert (workers <= (W) && (W) < workers + nworkers), (int)((W) - workers))

static int nassumptions, queue, szassumptions;
static int sumassumptions, redassumptions;
static double * times, sumtimes;
static int ** assumptions;

static int nvars, szvars, nclauses, nused, * used;
static signed char * vals;
static int nlits, szlits;
static int * lits;

static int verbose, bar, nowitness, addassumptions, plain, dofork = 2;

static int lineno = 1;
static char * inputname;
static FILE * inputfile;

static int done;

static pthread_mutex_t msgmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t donemutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t queuemutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t finishedmutex = PTHREAD_MUTEX_INITIALIZER;

static int finished;

static void msglock (void * voidptr) {
  (void) voidptr;
  pthread_mutex_lock (&msgmutex);
}

static void msgunlock (void * voidptr) {
  (void) voidptr;
  pthread_mutex_unlock (&msgmutex);
}

static void msg (Worker * w, int level, const char * fmt, ...) {
  va_list ap;
  if (verbose < level) return;
  msglock (0);
  if (w) printf ("c %d ", WID (w)); else printf ("c - ");
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
  msgunlock (0);
}

static void die (const char * fmt, ...) {
  va_list ap;
  fputs ("*** [ilingeling] ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
  exit (1);
}

static void warn (const char * fmt, ...) {
  va_list ap;
  fputs ("*** [ilingeling] warning: ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

static double currentime (void) {
  double res = 0;
  struct timeval tv;
  if (!gettimeofday (&tv, 0)) res = 1e-6 * tv.tv_usec, res += tv.tv_sec;
  return res;
}

static double getime () { return currentime () - startime; }

static int isnum (const char * str) {
  const char * p = str;
  if (!isdigit (*p++)) return 0;
  while (*p && isdigit (*p)) p++;
  return !*p;
}

static int term (void * voidptr) {
  Worker * w = voidptr;
  int res;
  msg (w, 3, "checking early termination");
  if (pthread_mutex_lock (&donemutex))
    warn ("failed to lock 'done' mutex in termination check");
  res = done;
  if (pthread_mutex_unlock (&donemutex)) 
    warn ("failed to unlock 'done' mutex in termination check");
  msg (w, 3, "early termination check %s", res ? "succeeded" : "failed");
  return res;
}

static void progress (int pmille, int total, int max, double avg, int nl) {
  int ch, i, lim, eta;
  char fmt[10];
  double rem;
  msglock (0);
  if (isatty (1)) fputc ('\r', stdout);

  lim = 10;
  for (i = 1; lim < max && i < 11; i++) lim *= 10;
  sprintf (fmt, "c %%0%dd", i);
  printf (fmt, total);
  printf (" / %d |", max);

  for (i = 0; i < pmille/50; i++) fputc ('=', stdout);
  if (total == max) ch = '=';
  else switch (pmille % 4) {
    case 1 : ch = '\\'; break;
    case 2 : ch = '|'; break;
    case 3 : ch = '/'; break;
    default: ch = '-'; break;
  }
  if (i++ < 20) fputc (ch, stdout);
  while (i++ < 20) fputc ('-', stdout);

  printf ("| %3d%%", pmille/10);

  printf (" %.4f sec/cube", avg);

  if (max <= total) eta = 0;
  else {
    rem = (max - total) * avg;
    if (rem >= 100*3600) eta = INT_MAX;
    else { eta = rem; assert (eta < INT_MAX); }
  }
  if (eta < INT_MAX) {
    if (eta > 3600) printf ("%02d:", eta / 3600), eta %= 3600;
    else printf ("   ");
    printf ("%02d:%02d ETS", eta/60, eta%60);
  } else printf ("   --:-- ETS");
  if (nl || !isatty (1)) fputc ('\n', stdout);
  fflush (stdout);
  msgunlock (0);
}

static void initlgl (LGL * lgl, Worker * w, int opts) {
  lglsetid (lgl, WID (w), nworkers);
  lglsetime (lgl, getime);
  lglseterm (lgl, term, w);
  lglsetmsglock (lgl, msglock, msgunlock, w);
  if (!opts) return;
  if (lglhasopt (lgl, "boost")) lglsetopt (lgl, "boost", 0);
  if (lglhasopt (lgl, "cgrboost")) lglsetopt (lgl, "cgrboost", 0);
  if (lglhasopt (lgl, "restartint")) lglsetopt (lgl, "restartint", 100);
  if (verbose) lglsetopt (lgl, "verbose", verbose-1);
  if (plain) lglsetopt (lgl, "plain", 1);
}

static int justreturn (Worker * w) {
  int res;
  if (pthread_mutex_lock (&donemutex))
    warn ("worker %d failed to lock 'done' mutex", WID (w));
  res = done;
  if (pthread_mutex_unlock (&donemutex))
    warn ("worker %d failed to unlock 'done' mutex", WID (w));
  return res;
}

static int sat (Worker * w) {
#ifndef NDEBUG
  int oldres;
#endif
  int res;
  char name[100];
  LGL * forked;
  if (dofork >= 1) lglsetopt (w->lgl, "clim", FORK_LIMIT);
  res = lglsat (w->lgl);
  if (!res && !justreturn (w)) {
    msg (w, 1, "forking after %d conflicts", FORK_LIMIT);
    forked = lglfork (w->lgl, 0);
    initlgl (forked, w, 0);
    if (dofork >= 2) lglsetopt (forked, "clim", BRUTE_FORK_LIMIT);
    sprintf (name, "c F%d ", w->forked.count++);
    lglsetprefix (forked, name);
    if (pthread_mutex_lock (&w->forked.lock))
      warn ("worker %d failed to lock 'forked' mutex", WID (w));
    assert (!w->forked.lgl);
    w->forked.lgl = forked;
    if (pthread_mutex_unlock (&w->forked.lock))
      warn ("worker %d failed to unlock 'forked' mutex", WID (w));
#ifndef NDEBUG
    oldres = 
#else
    (void)
#endif
    lglsat (forked);
    if (pthread_mutex_lock (&w->forked.lock))
      warn ("worker %d failed to lock 'forked' mutex", WID (w));
    assert (forked == w->forked.lgl);
    w->forked.lgl = 0;
    w->forked.decs += lglgetdecs (forked);
    w->forked.confs += lglgetconfs (forked);
    w->forked.props += lglgetprops (forked);
    if (pthread_mutex_unlock (&w->forked.lock))
      warn ("worker %d failed to unlock 'forked' mutex", WID (w));
    if (statsfile) {
      lglsetout (forked, statsfile);
      lglstats (forked);
      lglsetout (forked, stdout);
    }
    msg (w, 1, "joining forked solver");
    res = lgljoin (w->lgl, forked);
    assert (!oldres || res == oldres);
    if (!res && !justreturn (w)) {
      msg (w, 1, "brute forking after %d conflicts", BRUTE_FORK_LIMIT);
      forked = lglbrutefork (w->lgl, 0);
      lglsetid (forked, WID (w), nworkers);
      initlgl (forked, w, 0);
      sprintf (name, "c B%d ", w->forked.bcount++);
      lglsetprefix (forked, name);
      if (pthread_mutex_lock (&w->forked.lock))
	warn ("worker %d failed to lock '(brute) forked' mutex", WID (w));
      assert (!w->forked.lgl);
      w->forked.lgl = forked;
      if (pthread_mutex_unlock (&w->forked.lock))
	warn ("worker %d failed to unlock '(brute) forked' mutex", WID (w));
#ifndef NDEBUG
      oldres = 
#else
      (void)
#endif
      lglsat (forked);
      assert (res || justreturn (w));
      if (pthread_mutex_lock (&w->forked.lock))
	warn ("worker %d failed to lock '(brute) forked' mutex", WID (w));
      assert (forked == w->forked.lgl);
      w->forked.lgl = 0;
      w->forked.decs += lglgetdecs (forked);
      w->forked.confs += lglgetconfs (forked);
      w->forked.props += lglgetprops (forked);
      if (pthread_mutex_unlock (&w->forked.lock))
	warn ("worker %d failed to unlock '(brute) forked' mutex", WID (w));
      if (statsfile) {
	lglsetout (forked, statsfile);
	lglstats (forked);
	lglsetout (forked, stdout);
      }
      msg (w, 1, "joining brute forked solver");
      res = lgljoin (w->lgl, forked);
      assert (!res || res == oldres);
    }
  }
  return res;
}

static void * work (void * voidptr) {
  int i, last, pm, lm, count, lit, idx, *a, * p, size, red, fin;
  double start, end, delta, avg;
  Worker * w = voidptr;
  msg (w, 1, "running");
  for (;;) {
    if (pthread_mutex_lock (&queuemutex))
      die ("worker %d failed to lock 'queue' mutex", WID (w));
    assert (queue <= nassumptions);
    if ((last = queue) < nassumptions) queue++;
    if (pthread_mutex_unlock (&queuemutex))
      die ("worker %d failed to unlock 'queue' mutex", WID (w));
    if (last == nassumptions) {
DONE:
      if (!bar) msg (w, 1, "done");
      return 0;
    }
    msg (w, 2, "got job %d", last);
    count = 0;
    for(i = w->last + 1; i <= last; i++) {
      a = assumptions[i];
      if (addassumptions && i < last) {
	for (p = a; (lit = *p); p++) lgladd (w->lgl, -lit);
	lgladd (w->lgl, 0);
      }
      for (p = a; (lit = *p); p++)
	if (used [idx = abs (lit)] == i)
	  lglmelt (w->lgl, idx), count++;
    }
    msg (w, 2, "melted %d variables", count);
    a = assumptions[w->last = last];
    for (p = a; (lit = *p); p++) lglassume (w->lgl, lit);
    start = getime ();
    w->res = sat (w);
    end = getime ();
    delta = end - start;
    delta = (delta <= 0) ? 0 : delta;
    times[last] = delta;
    if (bar) {
      (void) pthread_mutex_lock (&finishedmutex);
      fin = ++finished;
      sumtimes += delta;
      avg = sumtimes / fin;
      (void) pthread_mutex_unlock (&finishedmutex);
      pm = (1000*(fin-1))/nassumptions;
      lm = (1000*fin)/nassumptions;
      if (pm < lm) progress (lm, fin, nassumptions, avg, 0);
    }
    if (w->res == 10) {
      if (!bar) msg (w, 1, "job %d SATISFIABLE", last);
      if (pthread_mutex_lock (&donemutex))
	warn ("worker %d failed to lock 'done' mutex", WID (w));
      done = 1;
      if (pthread_mutex_unlock (&donemutex))
	warn ("worker %d failed to unlock 'done' mutex", WID (w));
      goto DONE;
    } else if (w->res == 20) {
      red = 0;
      for (p = a; (lit = *p); p++)
	if (lglfailed (w->lgl, lit)) red++;
      sumassumptions += (size = p - a);
      redassumptions += red;
      if (!bar)
        msg (w, 1, "job %d UNSATISFIABLE (%d failed / %d) in %.3f seconds",
	     last, red, size, delta);
      lglflushcache (w->lgl);
    } else {
      if (!bar) msg (w, 1, "job %d UNKNOWN", last);
      goto DONE;
    }
  }
}

static void init (void) {
  Worker * w;
  assert (nworkers > 0);
  NEW (workers, nworkers);
  for (w = workers; w < workers + nworkers; w++) {
    w->last = -1;
    w->lgl = lglinit ();
    pthread_mutex_init (&w->forked.lock, 0);
    initlgl (w->lgl, w, 1);
  }
  msg (0, 1, "allocated %d workers", nworkers);
}

static void reset (void) {
  int i, * p, * a;
  if (vals) DEL (vals, nvars);
  for (i = 0; i < nworkers; i++) lglrelease (workers[i].lgl);
  DEL (workers, nworkers);
  for (i = 0; i < nassumptions; i++) {
    for (p = (a = assumptions[i]); *p; p++)
      ;
    DEL (a, (p - a) + 1);
  }
  DEL (assumptions, szassumptions);
  DEL (times, nassumptions);
  DEL (lits, szlits);
  DEL (used, szvars);
  if (allocated) 
    warn ("internal memory leak of %lld bytes", (long long) allocated);
}

static void perr (const char * fmt, ...) {
  va_list ap;
  fprintf (stderr, "%s:%d: ", inputname, lineno);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fputc ('\n', stderr);
  fflush (stderr);
  exit (1);
}

static int next (void) {
  int res;
  res = getc (inputfile);
  if (res == '\n') lineno++;
  return res;
}

static void add (int lit) {
  int i;
  for (i = 0; i < nworkers; i++) lgladd (workers[i].lgl, lit);
}

static void parse (void) {
  int ch, lit, sign, * assumption, i;
HEADER:
  ch = next ();
  if (ch == EOF) perr ("unexpected end-of-file in header");
  if (ch == 'c') {
    while ((ch = next ()) != '\n')
      if (ch == EOF) perr ("unexpected end-of-file in comment");
    goto HEADER;
  }
  if (ch != 'p' ||
      next () != ' ' ||
      next () != 'i' ||
      next () != 'n' ||
      next () != 'c' ||
      next () != 'c' ||
      next () != 'n' ||
      next () != 'f') perr ("invalid header (expected 'p inccnf')");
  ch = next ();
CLAUSES:
  if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
    ch = next (); goto CLAUSES;
  }
  if (ch == EOF && nlits) perr ("unexpected end-of-file in clause");
  if (ch == 'a' && nlits) perr ("unexpected 'a' in clause");
  if (ch == 'a') goto ASSUMPTIONS;
  if (ch == EOF) goto DONE;
  if (ch == 'a') goto ASSUMPTIONS;
  if (ch == '-') {
    sign = -1;
    ch = next ();
    if (!isdigit (ch)) perr ("expected digit after '-'");
  } else sign = 1;
  if (!isdigit (ch)) perr ("expected literal");
  lit = ch - '0';
  while (isdigit (ch = next ())) lit = 10*lit + ch - '0';
  if (lit > nvars) {
    if (lit >= szvars) {
      int oldszvars = szvars;
      szvars = szvars ? 2*szvars : 1;
      while (szvars <= lit) szvars *= 2;
      RSZ (used, oldszvars, szvars);
      for (i = oldszvars; i < szvars; i++) used[i] = -1;
    }
    assert (lit < szvars);
    nvars = lit;
  }
  lit *= sign;
  if (lit) nlits++; else nlits = 0, nclauses++;
  add (lit);
  goto CLAUSES;
ASSUMPTIONS:
  assert (!nlits);
  assert (ch == 'a');
  ch = next ();
  if (ch != ' ') perr ("expected space after 'a'");
LITS:
  ch = next ();
  if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') goto LITS;
  if (ch == EOF && nlits) perr ("unexpected end-of-file in assumptions");
  if (ch == '-') {
    sign = -1;
    ch = next ();
    if (!isdigit (ch)) perr ("expected digit after '-'");
  } else sign = 1;
  if (!isdigit (ch)) perr ("expected literal");
  lit = ch - '0';
  while (isdigit (ch = next ())) lit = 10*lit + ch - '0';
  if (lit > nvars)
    perr ("assumption %d exceeds maximum variables %d", lit, nvars);
  assert (0 <= lit && lit < szvars);
  if (used[lit] < 0) nused++;
  used[lit] = nassumptions;
  lit *= sign;
  if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
    perr ("expected white space after '%l'", lit);
  if (lit) { PUSH (lits, lit); goto LITS; }
  NEW (assumption, nlits + 1);
  for (i = 0; i < nlits; i++) assumption[i] = lits[i];
  assert (!assumption[nlits]);
  PUSH (assumptions, assumption);
  nlits = 0;
NEXT:
  ch = next ();
  if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') goto NEXT;
  if (ch == EOF) goto DONE;
  if (ch == '-' || isdigit (ch))
#if 1
    goto CLAUSES;
  if (ch != 'a') perr ("expected literal, 'a' or end-of-file");
#else
    perr ("can not handle clauses after assumptions yet");
  if (ch != 'a') perr ("expected 'a' or end-of-file");
#endif
  goto ASSUMPTIONS;
DONE:
  msg (0, 1, "maximum variable %d in %d clauses", nvars, nclauses);
  msg (0, 1, "parsed %d assumptions", nassumptions);
  nvars++;
  NEW (times, nassumptions);
  for (i = 0; i < nassumptions; i++) times[i] = -1;
}

static void freeze (void) {
  int idx, i;
  for (idx = 1; idx < nvars; idx++)
    if (used[idx] >= 0)
      for (i = 0; i < nworkers; i++)
	lglfreeze (workers[i].lgl, idx);
}

static void start (void) {
  Worker * w;
  for (w = workers; w < workers + nworkers; w++) {
    if (pthread_create (&w->thread, 0, work, w))
      die ("failed to create worker thread %d", WID (w));
  }
}

static void stop (void) {
  Worker * w;
  double avg;
  for (w = workers; w < workers + nworkers; w++)
    if (pthread_join (w->thread, 0))
      die ("failed to join worker %d", WID (w));
  if (bar) {
    avg = finished ? sumtimes / finished : 0.0;
    progress ((1000*finished)/nassumptions, finished, nassumptions, avg, 1);
  }
  msg (0, 1, "joined all %d workers", nworkers);
}

static void statsps (FILE * file,
                     const char * name, long long stats, double time) {
  const char * scale;
  if (stats > 10000000) scale = " million", stats /= 1000000;
  else if (stats > 10000) scale = " thousand", stats /= 1000;
  else scale = "";
  fprintf (file, 
           "c %lld%s %s, %.1f%s per second\n",
           stats, scale, name, (time > 0 ? stats/time : 0.0), scale);
}

static int cmpdblptr (const void * p, const void * q) {
  double a = *(double*)p;
  double b = *(double*)q;
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

static void stats (void) {
  int64_t decs = 0, confs = 0, props = 0;;
  double mb  = maxallocated / (double)(1<<20);
  double wct = getime (), prt = lglprocesstime ();
  FILE * file = statsfile ? statsfile : stdout;
  double sum, t, min, max, avg, std;
  int i, n;
  for (i = 0; i < nworkers; i++) lglflushtimers (workers[i].lgl);
  for (i = 0; i < nworkers; i++) {
    lglflushtimers (workers[i].lgl);
    fprintf (file, "c\n");
    fprintf (file, "c ---------[worker %d stats]------------------\n", i);
    fprintf (file, "c\n");
    lglsetout (workers[i].lgl, file);
    lglstats (workers[i].lgl);
    lglsetout (workers[i].lgl, stdout);
    mb += lglmb (workers[i].lgl);
    decs += lglgetdecs (workers[i].lgl);
    confs += lglgetconfs (workers[i].lgl);
    props += lglgetprops (workers[i].lgl);
    if (pthread_mutex_lock (&workers[i].forked.lock))
      warn ("worker failed to lock 'forked' mutex");
    if (workers[i].forked.lgl) {
      fprintf (file, "c ---------[forked worker %d dstats]----------\n", i);
      fprintf (file, "c\n");
      lglsetout (workers[i].forked.lgl, file);
      lglstats (workers[i].forked.lgl);
      lglsetout (workers[i].forked.lgl, stdout);
      mb += lglmb (workers[i].forked.lgl);
      decs += lglgetdecs (workers[i].forked.lgl);
      confs += lglgetconfs (workers[i].forked.lgl);
      props += lglgetprops (workers[i].forked.lgl);
    }
    decs += workers[i].forked.decs;
    confs += workers[i].forked.confs;
    props += workers[i].forked.props;
    if (pthread_mutex_unlock (&workers[i].forked.lock))
      warn ("worker failed to lock 'forked' mutex");
  }
  fprintf (file, "c\n");
  fprintf (file, "c ---------[global-stats]-------------------------\n");
  fprintf (file, "c\n");
  statsps (file, "scheduled jobs", queue, wct);
  fprintf (file, "c %d failed assumptions %.0f%% out of %d\n",
	   redassumptions,
	   sumassumptions ? (100.0*redassumptions)/sumassumptions : 0,
           sumassumptions);
  fprintf (file, "c\n");
  statsps (file, "conflicts", confs, wct);
  statsps (file, "decisions", decs, wct);
  statsps (file, "propagations", props, wct);
  fprintf (file, "c wall clock time %.1f seconds\n", wct);
  fprintf (file, "c process time %.1f seconds\n", prt);
  fprintf (file, "c utilization %.0f%%\n", 
           wct > 0 ? 100.0 * (prt / wct / nworkers) : 0.0);
  fflush (file);
  n = 0;
  sum = 0;
  max = min = -1;
  for (i = 0; i < nassumptions; i++) {
    if ((t = times[i]) < 0) continue;
    sum += t;
    times[n++] = t;
    if (min < 0 || (t < min)) min = t;
    if (max < 0 || (t > max)) max = t;
  }
  if (n) {
    fprintf (file, "c\n");
    avg = sum/n;
    fprintf (file, "c %d finished jobs in average time %.3f\n", n, avg);
    fprintf (file, "c time: sum %.3f, min %.3f, max %.3f\n", sum, min, max);
    std = 0;
    for (i = 0; i < n; i++) {
      t = times[i] - avg;
      std += t*t;
    }
    std = sqrt (std);
    qsort (times, n, sizeof times, cmpdblptr);
    fprintf (file, "c time: median %.3f, std dve %.3f\n", times[n/2], std);
  }
  fflush (file);
}

static void hist (void) {
  FILE * file = histfile ? histfile : stdout;
  int i;
  for (i = 0; i < nassumptions; i++)
    fprintf (file, "%.3f\n", times[i]);
  fflush (file);
}

static int catchedsig;
static void (*sig_int_handler)(int);
static void (*sig_segv_handler)(int);
static void (*sig_abrt_handler)(int);
static void (*sig_term_handler)(int);

static void resetsighandlers (void) {
  (void) signal (SIGINT, sig_int_handler);
  (void) signal (SIGSEGV, sig_segv_handler);
  (void) signal (SIGABRT, sig_abrt_handler);
  (void) signal (SIGTERM, sig_term_handler);
}

static void caughtsigmsg (int sig) {
  if (!verbose) return;
  printf ("c\nc CAUGHT SIGNAL %d\nc\n", sig);
  fflush (stdout);
}

static void catchsig (int sig) {
  if (!catchedsig) {
    fputs ("s UNKNOWN\n", stdout);
    fflush (stdout);
    catchedsig = 1;
    caughtsigmsg (sig);
    if (statsfile) stats ();
    if (histfile) hist ();
    if (statsfile || histfile) caughtsigmsg (sig);
  }
  resetsighandlers ();
  if (!getenv ("LGLNABORT")) raise (sig); else exit (1);
}

static void setsighandlers (void) {
  sig_int_handler = signal (SIGINT, catchsig);
  sig_segv_handler = signal (SIGSEGV, catchsig);
  sig_abrt_handler = signal (SIGABRT, catchsig);
  sig_term_handler = signal (SIGTERM, catchsig);
}

int main (int argc, char ** argv) {
  const char * statsfilename = 0, * histfilename = 0;
  int i, closeinputfile, res = 0;
  Worker * winner, * w;
  startime = currentime ();
  for (i = 1; i < argc; i++) { if (!strcmp (argv[i], "-h")) {
      printf (
"usage: ilingeling [<option> ...][<inccnf>][<nworkers>]\n"
"\n"
"where <option> is one of the following:\n"
"\n"
"  -h  print this command line option summary\n"
"\n"
"  -v  increase verbose level\n"
"  -q  do not print 'c job ...' lines (requires verbosity < 2)\n"
"  -b  progress bar (implies '-q')\n"
"\n"
"  -s  <stats> output statistics to separate file\n"
"  -t  <hist> output job run time histogram to separate file\n"
"\n"
"  --no-fork         disabling forking after %d conflicts\n"
"  --no-brute-fork   do not brute-fork after additional %d conflicts\n"
"\n"
"  -a  add previous assumptions as don't care\n"
"\n"
"  <inccnf>    'p inccnf' + '<lit*> 0' clauses + 'a <lit>* 0' assumptions\n"
"  <nworkers>  number of workers defaults to 1\n",
      FORK_LIMIT, BRUTE_FORK_LIMIT);
      exit (0);
    } else if (!strcmp (argv[i], "-v")) verbose++;
    else if (!strcmp (argv[i], "-b")) bar = 1;
    else if (!strcmp (argv[i], "-n")) nowitness = 1;
    else if (!strcmp (argv[i], "-a")) addassumptions = 1;
    else if (!strcmp (argv[i], "-p")) plain = 1;
    else if (!strcmp (argv[i], "-s")) {
      if (statsfilename) die ("two '-s' options");
      if (++i == argc) die ("argument to '-s' missing");
      statsfilename = argv[i];
    } else if (!strcmp (argv[i], "-t")) {
      if (histfilename) die ("two '-t' options");
      if (++i == argc) die ("argument to '-t' missing");
      histfilename = argv[i];
    } else if (!strcmp (argv[i], "--no-fork")) dofork = 0;
    else if (!strcmp (argv[i], "--no-brute-fork")) dofork = dofork ? 1 : 0;
    else if (argv[i][0] == '-') die ("invalid option '%s'", argv[i]);
    else if (isnum (argv[i])) {
      if (nworkers) die ("number of workers specified twice: '%d' and '%s'",
	                 nworkers, argv[i]);
      if ((nworkers = atoi (argv[i])) <= 0)
	die ("invalid number of workers argument: '%s'", argv[i]);
    } else if (inputname)
      die ("two files given: '%s' and '%s'", inputname, argv[i]);
    else inputname = argv[i];
  }
  if (bar && !isatty (1)) 
    die ("progress bar requested but <stdout> not connected to terminal");
  if (verbose >= 2 && bar) die ("verbosity %d > 1 with '-b'", verbose);
  if (statsfilename && !(statsfile = fopen (statsfilename, "w")))
    die ("can not write to stats file '%s'", statsfilename);
  if (histfilename && !(histfile = fopen (histfilename, "w")))
    die ("can not write to job run time histogram file '%s'", histfilename);
  if (verbose && !statsfile) statsfile = stdout;
  if (verbose) 
    lglbnr ("iLingeling Incremental Parallel Lingeling", "c ", stdout),
    printf ("c\n"), fflush (stdout);
  if (!nworkers) nworkers = 1;
  msg (0, 1, "using %d workers", nworkers);
  if (inputname) {
    if (!(inputfile = fopen (inputname, "r"))) 
      die ("can not read '%s'", inputname);
    closeinputfile = 1;
  } else inputname = "<stdin>", inputfile = stdin, closeinputfile = 0;
  init ();
  setsighandlers ();
  msg (0, 1, "parsing %s", inputname);
  parse ();
  if (closeinputfile) fclose (inputfile);
  msg (0, 1, "%d variables out of %d used in assumptions which is %.0f%%",
       nused, nvars, nvars ? 100.0 * (nused /(double)nvars) : 0.0);
  freeze ();
  start ();
  stop ();
  winner = 0;
  for (w = workers; w < workers + nworkers; w++)
    if (w->res) { winner = w;  if (w->res == 10) break; }
  if (winner && (res = winner->res) == 10 && !nowitness) {
      NEW (vals, nvars);
      for (i = 1; i < nvars; i++)
	vals[i] = lglderef (winner->lgl, i);
  }
  assert (winner || !nassumptions);
  resetsighandlers ();
  if (statsfile) stats ();
  if (statsfilename) fclose (statsfile);
  if (histfile) hist ();
  if (histfile) fclose (histfile);
  if (res == 10) printf ("s SATISFIABLE\n");
  else if (res == 20) printf ("s UNSATISFIABLE\n");
  else printf ("s UNKNOWN\n");
  fflush (stdout);
  if (vals) {
    for (i = 1; i < nvars; i++) {
      fputs ("v ", stdout);
      if (vals[i] < 0) fputc ('-', stdout);
      printf ("%d\n", i);
    }
    fputs ("v 0\n", stdout);
    fflush (stdout);
  }
  reset ();
  return res;
}
