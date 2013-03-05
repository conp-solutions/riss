/*-------------------------------------------------------------------------*/
/* Copyright 2010-2012 Armin Biere Johannes Kepler University Linz Austria */
/*-------------------------------------------------------------------------*/

#include "lglib.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>

#define NWORKERS 8
#define MAXGB 12
#define NUNITS (1<<9)
#define NOEQ 0

#define NEW(PTR,NUM) \
do { \
  size_t BYTES = (NUM) * sizeof *(PTR); \
  if (!((PTR) = malloc (BYTES))) { die ("out of memory"); exit (1); } \
  memset ((PTR), 0, BYTES); \
  incmem (BYTES); \
} while (0)

#define DEL(PTR,NUM) \
do { \
  size_t BYTES = (NUM) * sizeof *(PTR); \
  decmem (BYTES); \
  free (PTR); \
} while (0)

typedef struct Worker {
  LGL * lgl;
  pthread_t thread;
  int res, fixed;
  int units[NUNITS], nunits;
  struct {
    struct { int calls, produced, consumed; } units;
    struct { int produced, consumed; } eqs;
    int produced, consumed;
  } stats;
} Worker;

static int nworkers;
static int64_t memlimit, softmemlimit;
static Worker * workers;
static int nvars, nclauses;
static int * vals, * fixed, * repr, * maxscore;
static int nfixed, globalres;
static int verbose, notime, plain, ignaddcls, noeq = NOEQ;
#ifndef NLGLOG
static int loglevel;
#endif
static const char * name;
struct { size_t max, current;} mem;
static int catchedsig;
static double start;
static FILE * file;

struct { int units, eqs; } syncs;
static int done, termchks, units, eqs, flushed;
static pthread_mutex_t donemutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t msgmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fixedmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t reprmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memutex = PTHREAD_MUTEX_INITIALIZER;

static double currentime (void) {
  double res = 0;
  struct timeval tv;
  if (!gettimeofday (&tv, 0)) res = 1e-6 * tv.tv_usec, res += tv.tv_sec;
  return res;
}

static double getime () { 
  if (notime) return 0;
  return currentime () - start; 
}

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

static void incmem (size_t bytes) {
  if (pthread_mutex_lock (&memutex))
    warn ("failed to lock 'mem' mutex in 'incmem'");
  mem.current += bytes;
  if (mem.current > mem.max) mem.max = mem.current;
  if (pthread_mutex_unlock (&memutex))
    warn ("failed to unlock 'mem' mutex in 'incmem'");
}

static void decmem (size_t bytes) {
  if (pthread_mutex_lock (&memutex))
    warn ("failed to lock 'mem' mutex in 'decmem'");
  assert (mem.current >= bytes);
  mem.current -= bytes;
  if (pthread_mutex_unlock (&memutex))
    warn ("failed to unlock 'mem' mutex in 'decmem'");
}

static void * alloc (void * dummy, size_t bytes) {
  char * res;
  NEW (res, bytes);
  return res;
}

static void dealloc (void * dummy, void * void_ptr, size_t bytes) {
  char * char_ptr = void_ptr;
  DEL (char_ptr, bytes);
}

static void * resize (void * dummy, void * ptr, 
                      size_t old_bytes, size_t new_bytes) {
  if (pthread_mutex_lock (&memutex))
    warn ("failed to lock 'mem' mutex in 'resize'");
  assert (mem.current >= old_bytes);
  mem.current -= old_bytes;
  mem.current += new_bytes;
  if (mem.current > mem.max) mem.max = mem.current;
  if (pthread_mutex_unlock (&memutex))
    warn ("failed to unlock 'mem' mutex in 'resize'");
  return realloc (ptr, new_bytes);
}

static void stats (void) {
  double real, process, mpps, cps, mb;
  int64_t decs, confs, props;
  int i, unitcalls;
  Worker * w;
  unitcalls = decs = confs = 0;
  props = 0;
  mb = mem.max / (double)(1<<20);
  for (i = 0; i < nworkers; i++) {
    w = workers + i;
    if (!w->lgl) continue;
    decs += lglgetdecs (w->lgl);
    confs += lglgetconfs (w->lgl);
    props += lglgetprops (w->lgl);
    mb += lglmaxmb (w->lgl);
    unitcalls += w->stats.units.calls;
  }
  real = getime ();
  process = lglprocesstime ();
  cps = real > 0 ? confs / real : 0;
  mpps = real > 0 ? (props/1e6) / real : 0;
  printf ("c equiv: %d found, %d syncs\n", eqs, syncs.eqs);
  printf ("c terms: %d termination checks\n", termchks);
  printf ("c units: %d found, %d publications, %d syncs, %d flushed\n", 
          units, unitcalls, syncs.units, flushed);
  printf ("c\n");
  printf ("c %lld decisions, %lld conflicts, %.1f conflicts/sec\n", 
          (long long)decs, (long long)confs, cps);
  printf ("c %lld0 propagations, %.1f megaprops/sec\n",
          (long long)props, mpps);
  printf ("c %.1f process time, %.0f%% utilization\n",
          process, real > 0 ? (100.0 * process) / real / nworkers : 0.0);
  printf ("c\n");
  printf ("c %.1f seconds, %.1f MB\n", real, mb);
  fflush (stdout);
}

static void catchsig (int sig) {
  if (!catchedsig) {
    fputs ("s UNKNOWN\n", stdout);
    fflush (stdout);
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
  int ch, lit, sign;
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
  if (!noeq) NEW (repr, nvars + 1);
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
  lgladd (workers[0].lgl, lit);
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
    worker->stats.units.calls++;
    val = (lit < 0) ? -1 : 1;
    tmp = vals[idx];
    if (!tmp) {
      assert (nfixed < nvars);
      fixed[nfixed++] = lit;
      vals[idx] = val;
      assert (!fixed[nfixed]);
      worker->stats.units.produced++;
      worker->stats.produced++;
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
  syncs.units++;
  *fromptr = fixed + worker->fixed;
  *toptr = fixed + nfixed;
  if (pthread_mutex_unlock (&fixedmutex))
    warn ("failed to unlock 'fixed' in consume");
}

static int * lockrepr (void * voidptr) {
  Worker * worker = voidptr;
  int wid = worker - workers;
  if (pthread_mutex_lock (&reprmutex))
    warn ("failed to lock 'repr' mutex");
  msg (wid, 3, "starting equivalences synchronization");
  syncs.eqs++;
  return repr;
}

static void unlockrepr (void * voidptr, int consumed, int produced) {
  Worker * worker = voidptr;
  int wid = worker - workers;
  msg (wid, 3, 
       "finished equivalences synchronization: %d consumed, %d produced",
       consumed, produced);
  worker->stats.eqs.consumed += consumed;
  worker->stats.eqs.produced += produced;
  worker->stats.consumed += consumed;
  worker->stats.produced += produced;
  eqs += produced;
  assert (eqs < nvars);
  if (pthread_mutex_unlock (&reprmutex))
    warn ("failed to unlock 'repr' mutex");
}

static void consumed (void * voidptr, int consumed) {
  Worker * worker = voidptr;
  int wid = worker - workers;
  worker->stats.units.consumed += consumed;
  worker->stats.consumed += consumed;
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
	 worker->stats.units.consumed, 
	 percent (worker->stats.units.consumed, nfixed),
	 worker->stats.units.produced, 
	 percent (worker->stats.units.produced, nfixed));
    if (pthread_mutex_unlock (&fixedmutex))
      warn ("failed to unlock 'fixed' in work");
  }
  return worker->res ? worker : 0;
}

static int64_t getsystemtotalmem (int explain) {
  long long res;
  FILE * p = popen ("grep MemTotal /proc/meminfo", "r");
  if (p && fscanf (p, "MemTotal: %lld kB", &res) == 1) {
    if (explain)
      msg (-1, 0, "%lld KB total memory according to /proc/meminfo", res);
    res <<= 10;
  } else {
    res = MAXGB << 30;;
    if (explain) 
      msg (-1, 0, "assuming compiled in memory size of %d GB", MAXGB);
  }
  if (p) pclose (p);
  return (int64_t) res;
}

static int getsystemcores (int explain) {
  int syscores, coreids, physids, procpuinfocores;
  int usesyscores, useprocpuinfo, amd, intel, res;
  FILE * p;

  syscores = sysconf (_SC_NPROCESSORS_ONLN);
  if (explain) {
    if (syscores > 0)
      msg (-1, 1, "'sysconf' reports %d processors online", syscores);
    else
      msg (-1, 1, "'sysconf' fails to determine number of online processors");
  }

  p = popen ("grep '^core id' /proc/cpuinfo 2>/dev/null|sort|uniq|wc -l", "r");
  if (p) {
    if (fscanf (p, "%d", &coreids) != 1) coreids = 0;
    if (explain) {
      if (coreids > 0) 
	msg (-1, 1, "found %d unique core ids in '/proc/cpuinfo'", coreids);
      else
	msg (-1, 1, "failed to extract core ids from '/proc/cpuinfo'");
    }
    pclose (p);
  } else coreids = 0;

  p = popen (
        "grep '^physical id' /proc/cpuinfo 2>/dev/null|sort|uniq|wc -l", "r");
  if (p) {
    if (fscanf (p, "%d", &physids) != 1) physids = 0;
    if (explain) {
      if (physids > 0) 
	msg (-1, 1, "found %d unique physical ids in '/proc/cpuinfo'", 
            physids);
      else
	msg (-1, 1, "failed to extract physical ids from '/proc/cpuinfo'");
    }
    pclose (p);
  } else physids = 0;

  if (coreids > 0 && physids > 0 && 
      (procpuinfocores = coreids * physids) > 0) {
    if (explain)
      msg (-1, 1, 
           "%d cores = %d core times %d physical ids in '/proc/cpuinfo'",
           procpuinfocores, coreids, physids);
  } else procpuinfocores = 0;

  usesyscores = useprocpuinfo = 0;

  if (procpuinfocores > 0 && procpuinfocores == syscores) {
    if (explain) msg (-1, 1, "'sysconf' and '/proc/cpuinfo' results match");
    usesyscores = 1;
  } else if (procpuinfocores > 0 && syscores <= 0) {
    if (explain) msg (-1, 1, "only '/proc/cpuinfo' result valid");
    useprocpuinfo = 1;
  } else if (procpuinfocores <= 0 && syscores > 0) {
    if (explain) msg (-1, 1, "only 'sysconf' result valid");
    usesyscores = 1;
  } else {
    intel = !system ("grep vendor /proc/cpuinfo 2>/dev/null|grep -q Intel");
    if (intel && explain) 
      msg (-1, 1, "found Intel as vendor in '/proc/cpuinfo'");
    amd = !system ("grep vendor /proc/cpuinfo 2>/dev/null|grep -q AMD");
    if (amd && explain) 
      msg (-1, 1, "found AMD as vendor in '/proc/cpuinfo'");
    assert (syscores > 0);
    assert (procpuinfocores > 0);
    assert (syscores != procpuinfocores);
    if (amd) {
      if (explain) msg (-1, 1, "trusting 'sysconf' on AMD");
      usesyscores = 1;
    } else if (intel) {
      if (explain) {
	msg (-1, 1, 
	     "'sysconf' result off by a factor of %f on Intel", 
	     syscores / (double) procpuinfocores);
	msg (-1, 1, "trusting '/proc/cpuinfo' on Intel");
      }
      useprocpuinfo = 1;
    }  else {
      if (explain)
	msg (-1, 1, "trusting 'sysconf' on unknown vendor machine");
      usesyscores = 1;
    }
  } 
  
  if (useprocpuinfo) {
    if (explain) 
      msg (-1, 0, 
        "assuming cores = core * physical ids in '/proc/cpuinfo' = %d",
        procpuinfocores);
    res = procpuinfocores;
  } else if (usesyscores) {
    if (explain) 
      msg (-1, 0,
           "assuming cores = number of processors reported by 'sysconf' = %d",
           syscores);
    res = syscores;
  } else {
    if (explain) 
      msg (-1, 0, "using compiled in default value of %d workers", NWORKERS);
    res = NWORKERS;
  }

  return res;
}

static int cmproduced (const void * p, const void * q) {
  Worker * u = *(Worker**) p;
  Worker * v = *(Worker**) q;
  int res = v->stats.produced - u->stats.produced;
  if (res) return res;
  return u - v;
}

static int cmpconsumed (const void * p, const void * q) {
  Worker * u = *(Worker**) p;
  Worker * v = *(Worker**) q;
  int res = v->stats.consumed - u->stats.consumed;
  if (res) return res;
  return u - v;
}

static int parsenbcoreenv (void) {
  const char * str = getenv ("NBCORE");
  if (!str) return 0;
  if (!isposnum (str)) 
    die ("invalid value '%s' for environment variable NBCORE", str);
  return atoi (str);
}

static void setopts (LGL * lgl, int i) {
  Worker * w = workers + i;
  w->lgl = lgl;
  lglsetid (lgl, i, nworkers);
  lglsetime (lgl, getime);
  lglsetopt (lgl, "verbose", verbose);
#ifndef NLGLOG
  lglsetopt (lgl, "log", loglevel);
#endif
  lglsetopt (lgl, "seed", i);

  switch (i % 3) {
    case 0: lglsetopt (lgl, "phase", 0); break;
    case 1: lglsetopt (lgl, "phase", -1); break;
    case 2: lglsetopt (lgl, "phase", 1); break;
  }

  switch (i % 5) {
    default: lglsetopt (lgl, "phase", 2); break;
    case 1: lglsetopt (lgl, "phase", -1); break;
    case 2: lglsetopt (lgl, "phase", 1); break;
    case 3: lglsetopt (lgl, "phase", 0); break;
  }

  switch (i % 2) {
    case 0: lglsetopt (lgl, "reduce", 1); break;
    case 1: lglsetopt (lgl, "reduce", 2); break;
  }

  lglseterm (lgl, term, w);
  if (!plain) {
    lglsetproduceunit (lgl, produce, w);
    lglsetconsumeunits (lgl, consume, w);
    if (!noeq) {
      lglsetlockeq (lgl, lockrepr, w);
      lglsetunlockeq (lgl, unlockrepr, w);
    }
    lglsetconsumedunits (lgl, consumed, w);
    lglsetmsglock (lgl, msglock, msgunlock, w);
  }
  msg (i, 2, "initialized");
}

static long long bytes2mbll (int64_t bytes) {
  return (bytes + (1ll<<20) - 1) >> 20;
}

static long long bytes2gbll (int64_t bytes) {
  return (bytes + (1ll<<30) - 1) >> 30;
}

int main (int argc, char ** argv) {
  Worker * w, * winner, *maxconsumer, * maxproducer, ** sorted;
  int i, res, clin, lit, val, id, nbcore, witness = 1;
  int sumconsumed, sumconsumedunits, sumconsumedeqs;
  const char * errstr, * arg;
  char * cmd;
  LGL * lgl;
  start = currentime ();
  clin = 0;
  for (i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "-h")) {
      printf (
"usage: plingeling [-t <threads>][-h][-n][-p][-v][--no-time]"
#ifndef NLGLOG
"[-l]"
#endif
"[<dimacs>[.gz]]\n"
"\n"
"  -t <num>   number of worker threads (default %d on this machine)\n"
"  -g <num>   maximal memory in GB (default %lld GB on this machine)\n"
"  -h         print this command line option summary\n"
"  -n         do not print solution / witness\n"
"  -p         plain portfolio, no sharing\n"
"  -d         do not share equivalences%s\n"
"  -e         share equivalences%s\n"
"  -v         increase verbose level\n"
"  -i         ignore additional clauses\n"
#ifndef NLGLOG
"  -l           increase log level\n"
#endif
"  --no-time  do not measure time (default on verbose level 0)\n",
getsystemcores (0),
bytes2gbll (getsystemtotalmem (0)),
NOEQ ? " (default)" : "",
NOEQ ? "" : " (default)");
      exit (0);
    }
    if (!strcmp (argv[i], "-v")) verbose++;
#ifndef NLGLOG
    else if (!strcmp (argv[i], "-l")) loglevel++;
#endif
    else if (!strcmp (argv[i], "--no-time")) notime = 1;
    else if (!strcmp (argv[i], "-i")) ignaddcls = 1;
    else if (!strcmp (argv[i], "-p")) plain = 1;
    else if (!strcmp (argv[i], "-d")) noeq = 1;
    else if (!strcmp (argv[i], "-n")) witness = 0;
    else if (!strcmp (argv[i], "-t")) {
      if (nworkers) die ("multiple '-t' options");
      if (i + 1 == argc) die ("argument to '-t' missing");
      if (!isposnum (arg = argv[++i]) || (nworkers = atoi (arg)) <= 0)
	die ("invalid argument '%s' to '-t'", arg);
    } else if (!strcmp (argv[i], "-g")) {
      if (memlimit) die ("multiple '-g' options");
      if (i + 1 == argc) die ("argument to '-g' missing");
      if (!isposnum (arg = argv[++i]) || (memlimit = (atoll (arg)<<30)) <= 0)
	die ("invalid argument '%s' to '-g'", arg);
    } else if (argv[i][0] == '-') 
      die ("invalid option '%s' (try '-h')", argv[i]);
    else if (name) die ("multiple input files '%s' and '%s'", name, argv[i]);
    else name = argv[i];
  }
  lglbnr ("Plingeling Parallel Lingeling", "c ", stdout);
  fflush (stdout);
  if (verbose) printf ("c\n");
  nbcore = parsenbcoreenv ();
  if (nworkers) {
    msg (-1, 1, 
	 "command line option '-t %d' overwrites system default %d",
	 nworkers, getsystemcores (0));
    if (nbcore)
      msg (-1, 1, 
           "and also overwrites environment variable NBCORE=%d",
	   nbcore);
  } else if (nbcore) {
    msg (-1, 1, 
	 "environment variable NBCORE=%d overwrites system default %d",
	 nbcore, getsystemcores (0));
    nworkers = nbcore;
  } else {
    msg (-1, 1,
      "no explicit specification of number of workers");
      nworkers = getsystemcores (1);
  }
  msg (-1, 0, "USING %d WORKER THREADS", nworkers);
  if (memlimit) {
    msg (-1, 1,
      "memory limit %lld MB ('-g %lld') overwrites system default %lld MB",
      bytes2mbll (memlimit), bytes2gbll (memlimit),
      bytes2mbll (getsystemtotalmem (0)));
  } else {
    memlimit = getsystemtotalmem (1);
    msg (-1, 1, "memory limit set to system default of %lld MB total memory",
         bytes2mbll (memlimit));
  }
  softmemlimit = memlimit/2;
  msg (-1, 0, "soft memory limit set to %lld MB", bytes2mbll (softmemlimit));
  NEW (workers, nworkers);
  NEW (maxscore, nvars + 1);
  workers[0].lgl = lglminit (0, alloc, resize, dealloc);
  setopts (workers[0].lgl, 0);
  setsighandlers ();
  if (name) { 
    if (strlen (name) >= 3 && !strcmp (name + strlen(name) - 3, ".gz")) {
      cmd = malloc (strlen (name) + 30);
      sprintf (cmd, "gunzip -c %s", name);
      file = popen (cmd, "r");
      free (cmd);
      clin = 4;
    } else if (strlen (name) >= 4 &&
               !strcmp (name + strlen(name) - 4, ".bz2")) {
      cmd = malloc (strlen (name) + 30);
      sprintf (cmd, "bzcat %s", name);
      file = popen (cmd, "r");
      free (cmd);
      clin = 4;
    } else if (strlen (name) >= 3 && 
               !strcmp (name + strlen (name) - 3, ".7z")) {
      cmd = malloc (strlen (name) + 40);
      sprintf (cmd, "7z x -so %s 2>/dev/null", name);
      file = popen (cmd, "r");
      free (cmd);
      clin = 4;
    } else {
      file = fopen (name, "r");
      clin = 1;
    }
    if (!file) die ("can not read %s", name);
  } else file = stdin, name = "<stdin>";
  msg (-1, 0, "parsing %s", name);
  errstr = parse ();
  if (errstr) die ("parse error: %s", errstr);
  if (clin == 1) fclose (file);
  if (clin == 2) pclose (file);
  msg (-1, 0, "simplifying original formula with worker 0");
  res = lglsimp (workers[0].lgl, 0);
  if (res) {
    msg (-1, 1,
      "simplification of worker 0 produced %d", res);
    maxproducer = maxconsumer = winner = workers;
  } else {
    lglsetopt (workers[0].lgl, "clim", -1);
    msg (-1, 0, "starting to clone %d workers", nworkers-1);
    for (i = 1; i < nworkers; i++) {
      msg (-1, 0, 
        "cloning %lld MB + allocated %lld MB = %lld MB",
	bytes2mbll (lglbytes (workers[0].lgl)),
	bytes2mbll (mem.current),
	bytes2mbll (lglbytes (workers[0].lgl) + mem.current));

      if (lglbytes (workers[0].lgl) + mem.current >= softmemlimit) {
	msg (-1, 0, "soft memory limit %lld MB would be hit",
	     bytes2mbll (softmemlimit));
	break;
      }
      workers[i].lgl = lgl = lglclone (workers[0].lgl);
      setopts (lgl, i);
    }
    msg (-1, 2, "starting %d workers", nworkers);
    for (i = 0; i < nworkers; i++) {
      w = workers + i;
      if (!w->lgl) continue;
      if (pthread_create (&w->thread, 0, work, w))
	die ("failed to create worker thread %d", i);
      msg (-1, 2, "started worker %d", i);
    }
    maxproducer = maxconsumer = winner = 0;
    msg (-1, 2, "joining %d workers", nworkers);
    for (i = 0; i < nworkers; i++) {
      w = workers + i;
      if (!w->lgl) continue;
      if (pthread_join (w->thread, 0))
	die ("failed to join worker thread %d", i);
      msg (-1, 2, "joined worker %d", i);
      if (w->res) {
	if (!res) {
	  res = w->res;
	  winner = w;
	  msg (-1, 0, "worker %d is the winner with result %d", i, res);
	} else if (res != w->res) die ("result discrepancy");
      }
      if (!maxconsumer || w->stats.consumed > maxconsumer->stats.consumed)
	maxconsumer = w;
      if (!maxproducer || w->stats.produced > maxproducer->stats.produced)
	maxproducer = w;
    }
  }
  NEW (sorted, nworkers);
  for (i = 0; i < nworkers; i++) sorted[i] = workers + i;
  printf ("c\n");
  assert (maxproducer);
  qsort (sorted, nworkers, sizeof *sorted, cmproduced);
  for (i = 0; i < nworkers; i++) {
    w = sorted[i];
    if (!w->lgl) continue;
    id = w - workers;
    printf (
      "c worker %2d %s %7d %3.0f%% = %7d units %3.0f%% + %7d eqs %3.0f%%\n",
       id, (w == maxproducer ? "PRODUCED" : "produced"),
       w->stats.produced, percent (w->stats.produced, units + eqs),
       w->stats.units.produced, percent (w->stats.units.produced, units),
       w->stats.eqs.produced, percent (w->stats.eqs.produced, eqs));
  }
  fputs ("c ", stdout);
  for (i = 0; i < 71; i++) fputc ('-', stdout);
  fputc ('\n', stdout);
  printf (
    "c           produced %7d 100%% = %7d units 100%% + %7d eqs 100%%\n",
    units + eqs, units, eqs);
  printf ("c\n");
  assert (maxconsumer);
  qsort (sorted, nworkers, sizeof *sorted, cmpconsumed);
  sumconsumed = sumconsumedunits = sumconsumedeqs =0;
  for (i = 0; i < nworkers; i++) {
    w = sorted[i];
    if (!w->lgl) continue;
    id = w - workers;
    sumconsumed += w->stats.consumed;
    sumconsumedeqs += w->stats.eqs.consumed;
    sumconsumedunits += w->stats.units.consumed;
    printf (
      "c worker %2d %s %7d %3.0f%% = %7d units %3.0f%% + %7d eqs %3.0f%%\n",
      id, (w == maxconsumer ? "CONSUMED" : "consumed"),
      w->stats.consumed, percent (w->stats.consumed, units + eqs),
      w->stats.units.consumed, percent (w->stats.units.consumed, units),
      w->stats.eqs.consumed, percent (w->stats.eqs.consumed, eqs));
  fputs ("c ", stdout);
  for (i = 0; i < 71; i++) fputc ('-', stdout);
  fputc ('\n', stdout);
  printf (
    "c           consumed %7d 100%% = %7d units 100%% + %7d eqs 100%%\n",
    sumconsumed, sumconsumedunits, sumconsumedeqs);
  free (sorted);
  fflush (stdout);
}
  if (!res) res = globalres;
  if (!res) die ("no result by any worker");
  assert (res);
  msg (-1, 2, "copying assignment");
  if (winner && res == 10) {
    for (i = 1; i <= nvars; i++) {
      val = lglderef (winner->lgl, i);
      if (vals[i]) assert (val == vals[i]);
      vals[i] = val;
    }
  }
  resetsighandlers ();
  if (verbose) {
    for (i = 0; i < nworkers; i++) {
      if (!workers[i].lgl) continue;
      printf ("c\nc ------------[worker %d statistics]------------ \nc\n", i);
      lglstats (workers[i].lgl);
    }
    printf ("c\nc -------------[overall statistics]------------- \nc\n");
  }
  stats ();
  if (verbose) printf ("c\n");
  msg (-1, 2, "releasing %d workers", nworkers);
  for (i = 0; i < nworkers; i++) {
    w = workers + i;
    if (!w->lgl) continue;
    if (!w->lgl) continue;
    lglrelease (w->lgl);
    msg (-1, 2, "released worker %d", i);
  }
  free (workers);
  free (maxscore);
  free (fixed);
  if (!noeq) free (repr);
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
