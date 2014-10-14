/* Copyright 2010 Armin Biere, Johannes Kepler University, Linz, Austria */

#include "lglib.h"

#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>

#define NWORKERS 8
#define NUNITS (1<<9)

#define NEW(PTR,N) \
do { \
  size_t BYTES = (N) * sizeof *(PTR); \
  if (!((PTR) = malloc (BYTES))) die ("out of memory"); \
  allocated += BYTES; \
  assert (allocated >= 0); \
  memset ((PTR), 0, BYTES); \
} while (0)

typedef struct Worker {
  LGL * lgl;
  pthread_t thread;
  int res, fixed, produced, consumed;
  int units[NUNITS], nunits, unitcalls;
} Worker;

static int nworkers;
static Worker * workers;
static int nvars, nclauses;
static int * vals, * fixed, nfixed, globalres;
static int verbose, plain, ignaddcls;
#ifndef NLGLOG
static int loglevel;
#endif
static const char * name;
static size_t allocated;
static int catchedsig;
static double start;
static FILE * file;

static int done, termchks, units, syncs, flushed;
static pthread_mutex_t donemutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t msgmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fixedmutex = PTHREAD_MUTEX_INITIALIZER;

static double currentime (void) {
  double res = 0;
  struct timeval tv;
  if (!gettimeofday (&tv, 0)) res = 1e-6 * tv.tv_usec, res += tv.tv_sec;
  return res;
}

static double getime () { return currentime () - start; }

static void msg (int wid, int level, const char * fmt, ...) {
  va_list ap;
  if (verbose < level) return;
  pthread_mutex_lock (&msgmutex);
  if (wid < 0) printf ("c - "); else printf ("c %d ", wid);
  printf ("W %6.1f ", getime ());
  va_start (ap, fmt);
  vfprintf (stdout, fmt, ap);
  fputc ('\n', stdout);
  fflush (stdout);
  pthread_mutex_unlock (&msgmutex);
}

static void die (const char * fmt, ...) {
  va_list ap;
  fputs ("*** plingeling error: ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fputc ('\n', stderr);
  fflush (stderr);
  exit (1);
}

static void warn (const char * fmt, ...) {
  va_list ap;
  fputs ("*** plingeling warning: ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

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

static void stats (void) {
  double real, process, props, mpps, mb;
  int decs, confs, i, unitcalls;
  unitcalls = decs = confs = 0;
  props = 0;
  mb = allocated / (double)(1<<20);
  for (i = 0; i < nworkers; i++) {
    decs += lglgetdecs (workers[i].lgl);
    confs += lglgetconfs (workers[i].lgl);
    props += lglgetprops (workers[i].lgl);
    mb += lglmaxmb (workers[i].lgl);
    unitcalls += workers[i].unitcalls;
  }
  real = getime ();
  process = lglprocesstime ();
  mpps = real > 0 ? (props/1e6) / real : 0;
  printf ("c syncs: %d synchronizations\n", syncs);
  printf ("c terms: %d termination checks\n", termchks);
  printf ("c units: %d publications, %d units, %d flushed\n", 
          unitcalls, units, flushed);
  printf ("c\n");
  printf ("c %d decisions, %d conflicts\n", decs, confs);
  printf ("c %.0f propagations, %.1f megaprops/sec\n", props, mpps);
  printf ("c %.1f process time, %.0f%% utilization\n",
          process, real > 0 ? (100.0 * process) / real / nworkers : 0.0);
  printf ("c\n");
  printf ("c %.1f seconds, %.1f MB\n", real, mb);
  fflush (stdout);
}

static void catchsig (int sig) {
  if (!catchedsig) {
    catchedsig = 1;
    caughtsigmsg (sig);
    if (verbose) stats (), caughtsigmsg (sig);
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

static const char * parse (void) {
  int ch, lit, sign, i;
HEADER:
  ch = getc (file);
  if (ch == 'c') {
    while ((ch = getc (file)) != '\n')
      if (ch == EOF) return "EOF in comment";
    goto HEADER;
  }
  if (ch != 'p') return "expected header or comment";
  ungetc (ch, file);
  if (fscanf (file, "p cnf %d %d", &nvars, &nclauses) != 2)
    return "can not parse header";
  msg (-1, 1, "p cnf %d %d", nvars, nclauses);
  NEW (fixed, nvars + 1);
  NEW (vals, nvars + 1);
LIT:
  ch = getc (file);
  if (ch == 'c') {
    while ((ch = getc (file)) != '\n')
      if (ch == EOF) return "EOF in comment";
    goto LIT;
  }
  if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') goto LIT;
  if (ch == EOF) {
    if (nclauses > 0) return "not enough clauses";
DONE:
    msg (-1, 1, "finished parsing in %.1f seconds", getime ());
    return 0;
  }
  if (ch == '-') {
    ch = getc (file);
    sign = -1;
  } else sign = 1;
  if (!isdigit (ch)) return "expected digit";
  if (!nclauses) return "too many clauses";
  lit = ch - '0';
  while (isdigit (ch = getc (file)))
    lit = 10 * lit + (ch - '0');
  if (lit < 0 || lit > nvars) return "invalid variable index";
  lit *= sign;
  for (i = 0; i < nworkers; i++)
    lgladd (workers[i].lgl, lit);
  if (!lit) {
    nclauses--;
    if (ignaddcls && !nclauses) goto DONE;
  }
  goto LIT;
}

static int isposnum (const char * str) {
  int ch;
  if (!(ch = *str++) || !isdigit (ch)) return 0;
  while (isdigit (ch = *str++))
    ;
  return !ch;
}

static int term (void * voidptr) {
  Worker * worker = voidptr;
  int wid = worker - workers, res;
  assert (0 <= wid && wid < nworkers);
  msg (wid, 3, "checking early termination");
  if (pthread_mutex_lock (&donemutex))
    warn ("failed to lock 'done' mutex in termination check");
  res = done;
  termchks++;
  if (pthread_mutex_unlock (&donemutex)) 
    warn ("failed to unlock 'done' mutex in termination check");
  msg (wid, 3, "early termination check %s", res ? "succeeded" : "failed");
  return res;
}

static void flush (Worker * worker, int keep_locked) {
  int wid = worker - workers;
  int lit, idx, val, tmp, i;
  assert (worker->nunits);
  msg (wid, 2, "flushing %d units", worker->nunits);
  if (pthread_mutex_lock (&fixedmutex))
    warn ("failed to lock 'fixed' mutex in flush");
  flushed++;
  for (i = 0; i < worker->nunits; i++) {
    lit = worker->units[i];
    idx = abs (lit);
    assert (1 <= idx && idx <= nvars);
    assert (0 <= wid && wid < nworkers);
    worker->unitcalls++;
    val = (lit < 0) ? -1 : 1;
    tmp = vals[idx];
    if (!tmp) {
      assert (nfixed < nvars);
      fixed[nfixed++] = lit;
      vals[idx] = val;
      assert (!fixed[nfixed]);
      worker->produced++;
      units++;
    } else if (tmp == -val) {
      if (pthread_mutex_lock (&donemutex))
	warn ("failed to lock 'done' mutex flushing unit");
      if (!globalres) msg (wid, 1, "mismatched unit");
      globalres = 20;
      done = 1;
      if (pthread_mutex_unlock (&donemutex)) 
	warn ("failed to unlock 'done' mutex flushing unit");
      break;
    } else assert (tmp == val);
  }
  worker->nunits = 0;
  if (keep_locked) return;
  if (pthread_mutex_unlock (&fixedmutex)) 
    warn ("failed to unlock 'fixed' mutex in flush");
}

static void produce (void * voidptr, int lit) {
  Worker * worker = voidptr;
  int wid = worker - workers;
  assert (worker->nunits < NUNITS);
  worker->units[worker->nunits++] = lit;
  msg (wid, 3, "producing unit %d", lit);
  if (worker->nunits == NUNITS) flush (worker, 0);
}

static void consume (void * voidptr, int ** fromptr, int ** toptr) {
  Worker * worker = voidptr;
  int wid = worker - workers;
  if (worker->nunits) flush (worker, 1);
  else if (pthread_mutex_lock (&fixedmutex))
    warn ("failed to lock 'fixed' mutex in consume");
  msg (wid, 3, "starting unit synchronization");
  syncs++;
  *fromptr = fixed + worker->fixed;
  *toptr = fixed + nfixed;
  if (pthread_mutex_unlock (&fixedmutex))
    warn ("failed to unlock 'fixed' in consume");
}

static void consumed (void * voidptr, int consumed) {
  Worker * worker = voidptr;
  int wid = worker - workers;
  worker->consumed += consumed;
  msg (wid, 3, "consuming %d units", consumed);
}

static void msglock (void * voidptr) {
  (void) voidptr;
  pthread_mutex_lock (&msgmutex);
}

static void msgunlock (void * voidptr) {
  (void) voidptr;
  pthread_mutex_unlock (&msgmutex);
}

static double percent (double a, double b) { return b ? (100 * a) / b : 0; }

static void * work (void * voidptr) {
  Worker * worker = voidptr;
  int wid = worker - workers;
  LGL * lgl = worker->lgl;
  assert (0 <= wid && wid < nworkers);
  msg (wid, 1, "running");
  assert (workers <= worker && worker < workers + nworkers);
  worker->res = lglsat (lgl);
  msg (wid, 1, "result %d", worker->res);
  if (pthread_mutex_lock (&donemutex))
    warn ("failed to lock 'done' mutex in worker");
  done = 1;
  if (pthread_mutex_unlock (&donemutex)) 
    warn ("failed to unlock 'done' mutex in worker");
  msg (wid, 2, "%d decisions, %d conflicts, %.0f props, %.1f MB",
       lglgetdecs (lgl), lglgetconfs (lgl), lglgetprops (lgl), lglmb (lgl));
  if (verbose >= 2) {
    if (pthread_mutex_lock (&fixedmutex))
      warn ("failed to lock 'fixed' in work");
    msg (wid, 2, "consumed %d units %.0f%%, produced %d units %.0f%%",
	 worker->consumed, percent (worker->consumed, nfixed),
	 worker->produced, percent (worker->produced, nfixed));
    if (pthread_mutex_unlock (&fixedmutex))
      warn ("failed to unlock 'fixed' in work");
  }
  return worker->res ? worker : 0;
}

static void setopt (int wid, const char * opt, int val) {
  Worker * w;
  LGL * lgl; 
  int old;
  assert (0 <= wid && wid < nworkers);
  w = workers + wid;
  lgl = w->lgl;
  assert (lglhasopt (lgl, opt));
  old = lglgetopt (lgl, opt);
  if (old == val) return;
  msg (wid, 1, "--%s=%d (instead of %d)", opt, val, old);
  lglsetopt (lgl, opt, val);
}

static void set10x (int wid, const char * opt) {
  int old, val;
  Worker * w;
  LGL * lgl; 
  assert (0 <= wid && wid < nworkers);
  w = workers + wid;
  lgl = w->lgl;
  assert (lglhasopt (lgl, opt));
  old = lglgetopt (lgl, opt);
  val = 10*old;
  msg (wid, 1, "--%s=%d (instead of %d)", opt, val, old);
  lglsetopt (lgl, opt, val);
}

static int numcores (void) {
  int syscores, coreids = 0, res;
  FILE * p;
  syscores = sysconf (_SC_NPROCESSORS_ONLN);
  p = popen ("grep '^core id' /proc/cpuinfo 2>/dev/null|sort|uniq|wc -l", "r");
  if (p) {
    if (fscanf (p, "%d", &coreids) != 1) coreids = 0;
    pclose (p);
  }
  if (coreids > 0 && syscores > 0 && coreids == syscores / 2) res = coreids;
  else if (coreids > 0 && coreids == syscores) res = coreids;
  else if (coreids <= 0 && syscores > 0) res = syscores;
  else if (coreids > 0 && syscores <= 0) res = coreids;
  else res = NWORKERS;
  return res;
}

int main (int argc, char ** argv) {
  Worker * w, * winner, *maxconsumer, * maxproducer;
  int i, res, clin, lit, val, witness = 1;
  const char * errstr, * arg;
  long cores;
  char * cmd;
  LGL * lgl;
  start = currentime ();
  cores = numcores ();
  clin = 0;
  for (i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "-h")) {
      printf (
"usage: plingeling [-t <threads>][-h][-n][-p][-v]"
#ifndef NLGLOG
"[-l]"
#endif
"[<dimacs>[.gz]]\n"
"\n"
"  -t <num>   number of worker threads (default %ld on this machine)\n"
"  -h         print this command line option summary\n"
"  -n         do NOT print solution / witness\n"
"  -p         plain portfolio, no sharing\n"
"  -v         increase verbose level\n"
"  -i         ignore additional clauses\n"
#ifndef NLGLOG
"  -l           increase log level\n"
#endif
, cores);
      exit (0);
    }
    if (!strcmp (argv[i], "-v")) verbose++;
#ifndef NLGLOG
    else if (!strcmp (argv[i], "-l")) loglevel++;
#endif
    else if (!strcmp (argv[i], "-i")) ignaddcls = 1;
    else if (!strcmp (argv[i], "-p")) plain = 1;
    else if (!strcmp (argv[i], "-n")) witness = 0;
    else if (!strcmp (argv[i], "-t")) {
      if (nworkers) die ("multiple '-t' arguments");
      if (i + 1 == argc) die ("argument to '-t' missing");
      else if (!isposnum (arg = argv[++i]) || (nworkers = atoi (arg)) <= 0)
	die ("invalid argument '%s' to '-t'", arg);
    } else if (argv[i][0] == '-') 
      die ("invalid option '%s' (try '-h')", argv[i]);
    else if (name) die ("multiple input files '%s' and '%s'", name, argv[i]);
    else name = argv[i];
  }
  if (verbose) lglbnr ("Plingeling"), printf ("c\n");
  if (!nworkers) nworkers = cores;
  msg (-1, 1, "using %d workers", nworkers);
  NEW (workers, nworkers);
  for (i = 0; i < nworkers; i++) {
    w = workers + i;
    lgl = lglinit ();
    w->lgl = lgl;
    lglsetid (lgl, i);
    lglsetime (lgl, getime);
    lglsetopt (lgl, "verbose", verbose);
#ifndef NLGLOG
    lglsetopt (lgl, "log", loglevel);
#endif
    setopt (i, "seed", i);
    setopt (i, "phase", -((i+1)%3 - 1));
    setopt (i, "bias", - (((i/2)+1)%3 - 1));
     	 if ((i & 7) == 1) set10x (i, "prbreleff");
    else if ((i & 7) == 2) set10x (i, "dstreleff");
    else if ((i & 7) == 3) setopt (i, "elim", 0);
    else if ((i & 7) == 4) set10x (i, "elmreleff");
    else if ((i & 7) == 5) set10x (i, "dstreleff");
    else if ((i & 7) == 6) set10x (i, "prbreleff");
    else if ((i & 7) == 7) setopt (i, "redlinit", 10000),
			   setopt (i, "redlinc", 10000);
    lglseterm (lgl, term, w);
    if (!plain) {
      lglsetproduce (lgl, produce, w);
      lglsetconsume (lgl, consume, w);
      lglsetconsumed (lgl, consumed, w);
      lglsetmsglock (lgl, msglock, msgunlock, w);
    }
    msg (i, 2, "initialized");
  }
  setsighandlers ();
  if (name) { 
    if (strlen (name) >= 3 && !strcmp (name + strlen(name) - 3, ".gz")) {
      cmd = malloc (strlen (name) + 30);
      sprintf (cmd, "gunzip -c %s 2>/dev/null", name);
      file = popen (cmd, "r");
      free (cmd);
      clin = 4;
    } else {
      file = fopen (name, "r");
      clin = 1;
    }
    if (!file) die ("can not read %s", name);
  } else file = stdin, name = "<stdin>";
  msg (-1, 1, "parsing %s", name);
  errstr = parse ();
  if (errstr) die ("parse error: %s", errstr);
  if (clin == 1) fclose (file);
  if (clin == 2) pclose (file);
  msg (-1, 2, "starting %d workers", nworkers);
  for (i = 0; i < nworkers; i++) {
    if (pthread_create (&workers[i].thread, 0, work, workers + i))
      die ("failed to create worker thread %d", i);
    msg (-1, 2, "started worker %d", i);
  }
  maxproducer = maxconsumer = winner = 0;
  res = 0;
  msg (-1, 2, "joining %d workers", nworkers);
  for (i = 0; i < nworkers; i++) {
    w = workers + i;
    if (pthread_join (w->thread, 0))
      die ("failed to join worker thread %d", i);
    msg (-1, 2, "joined worker %d", i);
    if (w->res) {
      if (!res) {
	res = w->res;
	winner = w;
	msg (-1, 1, "worker %d is the winner with result %d", i, res);
      } else if (res != w->res) die ("result discrepancy");
    }
    if (!maxconsumer || w->consumed > maxconsumer->consumed)
      maxconsumer = w;
    if (!maxproducer || w->produced > maxproducer->produced)
      maxproducer = w;
  }
  assert (maxconsumer);
  msg (-1, 1, "worker %d is the maximal consumer with %d units %.0f%%",
       maxconsumer - workers, maxconsumer->consumed,
       percent (maxconsumer->consumed, nfixed));
  assert (maxproducer);
  msg (-1, 1, "worker %d is the maximal producer with %d units %.0f%%",
       maxproducer - workers, maxproducer->produced,
       percent (maxproducer->produced, nfixed));
  if (!res) res = globalres;
  if (!res) die ("no result by any worker");
  assert (res);
  msg (-1, 2, "copying assignment");
  if (res == 10) {
    assert (winner);
    for (i = 1; i <= nvars; i++) {
      val = lglderef (winner->lgl, i);
      if (vals[i]) assert (val == vals[i]);
      vals[i] = val;
    }
  }
  resetsighandlers ();
  if (verbose) {
    printf ("c\n");
    for (i = 0; i < nworkers; i++)
      lglstats (workers[i].lgl);
    printf ("c\n");
    stats ();
    printf ("c\n");
  }
  msg (-1, 2, "releasing %d workers", nworkers);
  for (i = 0; i < nworkers; i++) {
    lglrelease (workers[i].lgl);
    msg (-1, 2, "released worker %d", i);
  }
  free (workers);
  free (fixed);
  if (verbose >= 2) printf ("c\n");
  if (res == 10) {
    printf ("s SATISFIABLE\n");
    if (witness) {
      fflush (stdout);
      if (nvars) printf ("v");
      for (i = 1; i <= nvars; i++) {
	if (!(i & 7)) fputs ("\nv", stdout);
	lit = vals[i] < 0 ? -i : i;
	printf (" %d", lit);
      }
      printf ("\nv 0\n");
    }
  } else if (res == 20) {
    printf ("s UNSATISFIABLE\n");
  } else printf ("s UNKNOWN\n");
  fflush (stdout);
  free (vals);
  return res;
}
