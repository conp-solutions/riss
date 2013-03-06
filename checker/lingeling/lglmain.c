/* Copyright 2010-2012 Armin Biere, Johannes Kepler University, Linz, Austria */

#include "lglib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>

static LGL * lgl4sigh;
static int catchedsig, verbose, ignmissingheader, ignaddcls;

static int * targets, sztargets, ntargets, multiple;

static void (*sig_int_handler)(int);
static void (*sig_segv_handler)(int);
static void (*sig_abrt_handler)(int);
static void (*sig_term_handler)(int);
static void (*sig_bus_handler)(int);
static void (*sig_alrm_handler)(int);

static void resetsighandlers (void) {
  (void) signal (SIGINT, sig_int_handler);
  (void) signal (SIGSEGV, sig_segv_handler);
  (void) signal (SIGABRT, sig_abrt_handler);
  (void) signal (SIGTERM, sig_term_handler);
  (void) signal (SIGBUS, sig_bus_handler);
}

static void caughtsigmsg (int sig) {
  if (!verbose) return;
  printf ("c\nc CAUGHT SIGNAL %d", sig);
  switch (sig) {
    case SIGINT: printf (" SIGINT"); break;
    case SIGSEGV: printf (" SIGSEGV"); break;
    case SIGABRT: printf (" SIGABRT"); break;
    case SIGTERM: printf (" SIGTERM"); break;
    case SIGBUS: printf (" SIGBUS"); break;
    case SIGALRM: printf (" SIGALRM"); break;
    default: break;
  }
  printf ("\nc\n");
  fflush (stdout);
}

static void catchsig (int sig) {
  if (!catchedsig) {
    catchedsig = 1;
    caughtsigmsg (sig);
    fputs ("s UNKNOWN\n", stdout);
    fflush (stdout);
    if (verbose) {
      lglflushtimers (lgl4sigh);
      lglstats (lgl4sigh);
      caughtsigmsg (sig);
    }
  }
  resetsighandlers ();
  if (!getenv ("LGLNABORT")) raise (sig); else exit (1);
}

static void setsighandlers (void) {
  sig_int_handler = signal (SIGINT, catchsig);
  sig_segv_handler = signal (SIGSEGV, catchsig);
  sig_abrt_handler = signal (SIGABRT, catchsig);
  sig_term_handler = signal (SIGTERM, catchsig);
  sig_bus_handler = signal (SIGBUS, catchsig);
}

static int timelimit = -1, caughtalarm = 0;

static void catchalrm (int sig) {
  assert (sig == SIGALRM);
  if (!caughtalarm) {
    caughtalarm = 1;
    caughtsigmsg (sig);
    if (timelimit >= 0) {
      printf ("c time limit of %d reached after %.1f seconds\nc\n",
              timelimit, lglsec (lgl4sigh));
      fflush (stdout);
    }
  }
}

static int checkalarm (void * ptr) {
  assert (ptr == (void*) &caughtalarm);
  return caughtalarm;
}

static int next (FILE * in, int *linenoptr) {
  int res = getc (in);
  if (res == '\n') *linenoptr += 1;
  return res;
}

static char isoptchartab[256];
static int isoptchartabinitialized;

static int isoptchar (unsigned char uch) { 
  int i;
  if (!isoptchartabinitialized) {
    for (i = 'a'; i <= 'z'; i++) isoptchartab[i] = 1;
    for (i = 'A'; i <= 'Z'; i++) isoptchartab[i] = 1;
    for (i = '0'; i <= '9'; i++) isoptchartab[i] = 1;
    isoptchartab['-'] = isoptchartab['_'] = 1;
    isoptchartabinitialized = 1;
  }
  return isoptchartab[uch];
}

typedef struct Opt { char * name; int size, count; } Opt;

static void pushoptch (Opt * opt, int ch) {
  if (opt->count + 1 >= opt->size)
   opt->name = realloc (opt->name, opt->size = opt->size ? 2*opt->size : 2);

  opt->name[opt->count++] = ch;
  opt->name[opt->count] = 0;
}

static const char * parse (LGL * lgl, FILE * in, int * lp) {
  int ch, prev, m, n, v, c, l, lit, sign, val, embedded = 0, header;
  Opt opt;
  memset (&opt, 0, sizeof opt);
SKIP:
  ch = next (in, lp);
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') goto SKIP;
  if (ch == 'c') {
    ch  = next (in, lp);
    while (ch != '\n') {
      if (ch == EOF) return "end of file in comment";
      prev = ch;
      ch = next (in, lp);
      if (prev != '-') continue;
      if (ch != '-') continue;
      assert (!opt.count);
      ch = next (in, lp);
      while (isoptchar (ch)) {
	assert (ch != '\n');
	pushoptch (&opt, ch);
	ch = next (in, lp);
      }
      opt.count = 0;
      if (ch != '=') continue;
      ch = next (in, lp);
      if (ch == '-') sign = -1, ch = next (in, lp); else sign = 1;
      if (!isdigit (ch)) continue;
      val = ch - '0';
      while (isdigit (ch = next (in, lp)))
	val = 10 * val + (ch - '0');

      if (!lglhasopt (lgl, opt.name)) {
	fprintf (stderr, 
	         "*** lingeling warning: "
		 "parsed invalid embedded option '--%s'\n", 
		 opt.name);
	continue;
      }

      val *= sign;

      if (!embedded++ && verbose) printf ("c\nc embedded options:\nc\n");
      if (!strcmp (opt.name, "verbose")) verbose = val;
      if (verbose) printf ("c --%s=%d\n", opt.name, val);
      lglsetopt (lgl, opt.name, val);
    }
    goto SKIP;
  }
  free (opt.name);
  if (verbose) {
     if (embedded) printf ("c\n"); else printf ("c no embedded options\n");
     fflush (stdout);
  }
  header = m = n = v = l = c = 0;
  if (ignmissingheader) {
    if (ch == 'p')  {
      if (verbose) printf ("will not read header");
      while ((ch = next (in, lp)) != '\n' && ch != EOF)
	;
    } else if (verbose) printf ("c skipping missing header\n");
    goto BODY2;
  }
  if (ch != 'p') return "missing 'p ...' header";
  if (next (in, lp) != ' ') return "invalid header: expected ' ' after 'p'";
  while ((ch = next (in, lp)) == ' ')
    ;
  if (ch != 'c') return "invalid header: expected 'c' after ' '";
  if (next (in, lp) != 'n') return "invalid header: expected 'n' after 'c'";
  if (next (in, lp) != 'f') return "invalid header: expected 'f' after 'n'";
  if (next (in, lp) != ' ') return "invalid header: expected ' ' after 'f'";
  while ((ch = next (in, lp)) == ' ')
    ;
  if (!isdigit (ch)) return "invalid header: expected digit after 'p cnf '";
  m = ch - '0';
  while (isdigit (ch = next (in, lp)))
    m = 10 * m + (ch - '0');
  if (ch != ' ') return "invalid header: expected ' ' after 'p cnf <m>'"; 
  while ((ch = next (in, lp)) == ' ')
    ;
  if (!isdigit (ch)) 
    return "invalid header: expected digit after 'p cnf <m> '";
  n = ch - '0';
  while (isdigit (ch = next (in, lp)))
    n = 10 * n + (ch - '0');
  while (ch == ' ')
    ch = next (in, lp);
  if (ch != '\n') return "invalid header: expected new line after header";
  if (verbose) printf ("c found 'p cnf %d %d' header\n", m, n);
  header = 1;
BODY:
  ch = next (in, lp);
BODY2:
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') goto BODY;
  if (ch == 'c') {
    while ((ch = next (in, lp)) != '\n')
      if (ch == EOF) return "end of file in comment";
    goto BODY;
  }
  if (ch == EOF) {
    if (header && c + 1 == n) return "clause missing";
    if (header && c < n) return "clauses missing";
DONE:
    if (verbose) {
      printf ("c read %d variables, %d clauses, %d literals in %.1f seconds\n", 
	      v, c, l, lglsec (lgl));
      printf ("c\n");
      lglprintfeatures (lgl);
      printf ("c\n");
      fflush (stdout);
    }
    return 0;
  }
  if (ch == '-') {
    ch = next (in, lp);
    if (ch == '0') return "expected positive digit after '-'";
    sign = -1;
  } else sign = 1;
  if (!isdigit (ch)) return "expected digit";
  if (header && c == n) return "too many clauses";
  lit = ch - '0';
  while (isdigit (ch = next (in, lp)))
    lit = 10 * lit + (ch - '0');
  if (header && lit > m) return "maxium variable index exceeded";
  if (lit > v) v = lit;
  if (lit) l++;
  else c++;
  lit *= sign;
  lgladd (lgl, lit);
  if (!lit && ignaddcls && c == n) goto DONE;
  goto BODY;
}

typedef struct OBuf { char line[81]; int pos; } OBuf;

static void flushobuf (OBuf * obuf, FILE * file) {
  assert (0 < obuf->pos);
  assert (obuf->pos < 81);
  obuf->line[obuf->pos++] = '\n';
  assert (obuf->pos < 81);
  obuf->line[obuf->pos++] = 0;
  fputc ('v', file);
  fputs (obuf->line, file);
  obuf->pos = 0;
}

static void print2obuf (OBuf * obuf, int i, FILE * file) {
  char str[20];
  int len;
  sprintf (str, " %d", i);
  len = strlen (str);
  assert (len > 1);
  if (obuf->pos + len > 79) flushobuf (obuf, file);
  strcpy (obuf->line + obuf->pos, str);
  obuf->pos += len;
  assert (obuf->pos <= 79);
}

static FILE * writefile (const char * name, int * clptr) {
  int len = strlen (name);
  char * tmp;
  FILE * res;
  if (len >= 3 && !strcmp (name + len - 3, ".gz")) {
    tmp = malloc (len + 20);
    unlink (name);
    sprintf (tmp, "gzip -c > %s", name);
    res = popen (tmp, "w");
    if (res) *clptr = 2;
    free (tmp);
  } else {
    res = fopen (name, "w");
    if (res) *clptr = 1;
  }
  if (!res) fprintf (stderr, "*** lingeling error: can not write %s\n", name);
  return res;
}

static void closefile (FILE * file, int type) {
  if (type == 1) fclose (file);
  if (type == 2) pclose (file);
}

static void lgltravcounter (void * voidptr, int lit) {
  int * cntptr = voidptr;
  if (!lit) *cntptr += 1;
}

static void lglpushtarget (int target) {
  if (ntargets == sztargets)
    targets = realloc (targets, sizeof *targets *
               (sztargets = sztargets ? 2*sztargets : 1));
  targets[ntargets++] = target;
}

static int lglmonotifiy (void * lgl, int target, int val) {
  printf ("t %d %d after %.3f seconds\n", target, val, lglsec (lgl));
  fflush (stdout);
  return 1;
}

int main (int argc, char ** argv) {
  int res, i, j, clin, clout, val, len, lineno, simponly, count, target;
  const char * iname, * oname, * pname, * match, * p, * err;
  FILE * in, * out, * pfile;
  int maxvar, lit, nopts;
  char * tmp;
  LGL * lgl;
  OBuf obuf;
  lineno = 1;
  in = out = 0;
  res = clin = clout = simponly = 0;
  iname = oname = pname = 0;
  lgl4sigh = lgl = lglinit ();
  setsighandlers ();
  for (i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help")) {
      printf ("usage: lingeling [<option> ...][<file>[.gz]]\n");
      printf ("\n");
      printf ("where <option> is one of the following:\n");
      printf ("\n");
      printf ("-s               only simplify and print to output file\n");
      printf ("-o <output>      set output file (default 'stdout')\n");
      printf ("-p <options>     read options from file\n");
      printf ("\n");
      printf ("-t <seconds>     set time limit\n");
      printf ("\n");
      printf ("-a <assumption>  use multiple assumptions\n");
      printf ("-m <objective>   use multiple objectives\n");
      printf ("\n");
      printf ("-h|--help        print command line option summary\n");
      printf ("-f|--force       force reading even without header\n");
      printf ("-i|--ignore      ignore additional clauses\n");
      printf ("-r|--ranges      print value ranges of options\n");
      printf ("-d|--defaults    print default values of options\n");
      printf ("-e|--embedded    dito but in an embedded format print\n");
      printf ("-n|--nowitness   no solution (see '--witness')\n");
      printf ("\n");
      printf (
"The following options can also be used in the form '--<name>=<int>',\n"
"just '--<name>' for increment and '--no-<name>' for zero.  They\n"
"can be embedded into the CNF file, set through the API or capitalized\n"
"with prefix 'LGL' instead of '--' through environment variables.\n"
"Their default values are displayed in square brackets, explicitly\n"
"at higher verbose levels or with the command line options '-d' and\n"
"'--defaults'.\n");
      printf ("\n");
      lglusage (lgl);
      goto DONE;
    } else if (!strcmp (argv[i], "-s")) simponly = 1;
    else if (!strcmp (argv[i], "-o")) {
      if (++i == argc) {
	fprintf (stderr, "*** lingeling error: argument to '-o' missing");
	res = 1;
	goto DONE;
      } 
      if (oname) {
	fprintf (stderr, 
	         "*** lingeling error: "
		 "multiple output files '%s' and '%s'\n",
		 oname, argv[i]);
	res = 1;
	goto DONE;
      }
      oname = argv[i];
    } else if (!strcmp (argv[i], "-p")) {
      if (++i == argc) {
	fprintf (stderr, "*** lingeling error: argument to '-p' missing");
	res = 1;
	goto DONE;
      } 
      if (pname) {
	fprintf (stderr, 
	         "*** lingeling error: "
		 "multiple option files '%s' and '%s'\n",
		 pname, argv[i]);
	res = 1;
	goto DONE;
      }
      pname = argv[i];
    } else if (!strcmp (argv[i], "-t")) {
      if (++i == argc) {
	fprintf (stderr, "*** lingeling error: argument to '-t' missing");
	res = 1;
	goto DONE;
      }
      if (timelimit >= 0) {
	fprintf (stderr, "*** lingeling error: timit limit set twice");
	res = 1;
	goto DONE;
      }
      for (p = argv[i]; *p && isdigit (*p); p++) 
	;
      if (p == argv[i] || *p || (timelimit = atoi (argv[i])) < 0) {
	fprintf (stderr, 
	  "*** lingeling error: invalid time limit '-t %s'", argv[i]);
	res = 1;
	goto DONE;
      }
    } else if (!strcmp (argv[i], "-a")) {
      if (++i == argc) {
	fprintf (stderr, "*** lingeling error: argument to '-a' missing");
	res = 1;
	goto DONE;
      }
      if (multiple) {
	assert (ntargets > 0);
	fprintf (stderr, "*** lingeling error: unexpectd '-a' after '-m'");
	res = 1;
	goto DONE;
      }
      target = atoi (argv[i]);
      if (!target) {
	fprintf (stderr,
	  "*** lingeling error: invalid literal in '-a %d'", target);
	res = 1;
	goto DONE;
      }
      lglpushtarget (target);
    } else if (!strcmp (argv[i], "-m")) {
      if (++i == argc) {
	fprintf (stderr, "*** lingeling error: argument to '-m' missing");
	res = 1;
	goto DONE;
      }
      if (!multiple && ntargets > 0) {
	fprintf (stderr, "*** lingeling error: unexpectd '-m' after '-a'");
	res = 1;
	goto DONE;
      }
      target = atoi (argv[i]);
      if (!target) {
	fprintf (stderr,
	  "*** lingeling error: invalid literal in '-m %d'", target);
	res = 1;
	goto DONE;
      }
      lglpushtarget (target);
      multiple = 1;
    } else if (!strcmp (argv[i], "-d") || !strcmp (argv[i], "--defaults")) {
      lglopts (lgl, "", 0);
      goto DONE;
    } else if (!strcmp (argv[i], "-e") || !strcmp (argv[i], "--embedded")) {
      lglopts (lgl, "c ", 1);
      goto DONE;
    } else if (!strcmp (argv[i], "-r") || !strcmp (argv[i], "--ranges")) {
      lglrgopts (lgl);
      goto DONE;
    } else if (!strcmp (argv[i], "-f") || !strcmp (argv[i], "--force")) {
      ignmissingheader = 1;
    } else if (!strcmp (argv[i], "-i") || !strcmp (argv[i], "--ignore")) {
      ignaddcls = 1;
    } else if (!strcmp (argv[i], "-n") || !strcmp (argv[i], "nowitness")) {
      lglsetopt (lgl, "witness", 0);
    } else if (argv[i][0] == '-') {
      if (argv[i][1] == '-') {
	match = strchr (argv[i] + 2, '=');
	if (match) {
	  p = match + 1;
	  if (*p == '-') p++;
	  len = p - argv[i];
	  if (!strncmp (argv[i], "--write-api-trace=", len)) continue;
	  else if (!isdigit (*p)) {
ERR:
            fprintf (stderr,
	      "*** lingeling error: invalid command line option '%s'\n",
	      argv[i]);
	    res = 1;
	    goto DONE;
	  }
	  while (*++p) if (!isdigit (*p)) goto ERR;
	  len = match - argv[i] - 2;
	  tmp = malloc (len + 1);
	  j = 0;
	  for (p = argv[i] + 2; *p != '='; p++) tmp[j++] = *p;
	  tmp[j] = 0;
	  val = atoi (match + 1);
	} else if (!strncmp (argv[i], "--no-", 5)) {
	  tmp = strdup (argv[i] + 5);
	  val = 0;
	} else {
	  tmp = strdup (argv[i] + 2);
	  val = lglgetopt (lgl, tmp) + 1;
	}
	if (!lglhasopt (lgl, tmp)) { free (tmp); goto ERR; }
	lglsetopt (lgl, tmp, val);
	free (tmp);
      } else {
	if (argv[i][2]) goto ERR;
	if (!lglhasopt (lgl, argv[i] + 1)) goto ERR;
	val = lglgetopt (lgl, argv[i] + 1) + 1;
	lglsetopt (lgl, argv[i] + 1, val);
      }
    } else if (iname) {
      fprintf (stderr, "*** lingeling error: can not read '%s' and '%s'\n",
               iname, argv[i]);
      res = 1;
      goto DONE;
    } else iname = argv[i];
  }
  verbose = lglgetopt (lgl, "verbose");
  if (verbose) {
    lglbnr ("Lingeling SAT Solver", "c ", stdout);
    if (simponly) printf ("c simplifying only\n");
    if (oname) printf ("c output file %s\n", oname);
    if (simponly || oname) fflush (stdout);
  }
  if (verbose >= 2) {
   printf ("c\nc options after command line parsing:\nc\n");
   lglopts (lgl, "c ", 0);
   printf ("c\n");
   lglsizes (lgl);
   printf ("c\n");
  }
  if (iname) {
    len = strlen (iname);
    if (len >= 3 && !strcmp (iname + len - 3, ".gz")) {
      if (verbose > 1) printf ("c openening %s with 'gunzip'\n", iname);
      tmp = malloc (len + 20);
      sprintf (tmp, "gunzip -c %s", iname);
      in = popen (tmp, "r");
      if (in) clin = 2;
      free (tmp);
    } else if (len >= 5 && !strcmp (iname + len - 5, ".lzma")) {
      tmp = malloc (len + 20);
      sprintf (tmp, "lzcat %s", iname);
      in = popen (tmp, "r");
      if (in) clin = 2;
      free (tmp);
    } else if (len >= 4 && !strcmp (iname + len - 4, ".bz2")) {
      tmp = malloc (len + 20);
      sprintf (tmp, "bzcat %s", iname);
      in = popen (tmp, "r");
      if (in) clin = 2;
      free (tmp);
    } else if (len >= 3 && !strcmp (iname + len - 3, ".7z")) {
      tmp = malloc (len + 40);
      sprintf (tmp, "7z x -so %s 2>/dev/null", iname);
      in = popen (tmp, "r");
      if (in) clin = 2;
      free (tmp);
    } else {
      in = fopen (iname, "r");
      if (in) clin = 1;
    }
    if (!in) {
      fprintf (stderr,
         "*** lingeling error: can not read input file %s\n", iname);
      res = 1;
      goto DONE;
    }
  } else iname = "<stdin>", in = stdin;
  if (pname) {
    pfile = fopen (pname, "r");
    if (!pfile) {
      fprintf (stderr,
        "*** lingeling error: can not read option file %s\n", pname);
      res = 1;
      goto DONE;
    }
    if (verbose) {
      printf ("c reading options file %s\n", pname);
      fflush (stdout);
    }
    nopts = lglreadopts (lgl, pfile);
    if (verbose) 
      printf ("c read and set %d options\nc\n", nopts), fflush (stdout);
    fclose (pfile);
  }
  if (verbose) printf ("c reading input file %s\n", iname);
  fflush (stdout);
  if ((err = parse (lgl, in, &lineno))) {
    fprintf (stderr, "%s:%d: %s\n", iname, lineno, err);
    res = 1;
    goto DONE;
  }
  closefile (in, clin);
  clin = 0;
  if (verbose) {
    printf ("c\n");
    if (verbose >= 2) printf ("c final options:\nc\n");
    lglopts (lgl, "c ", 0);
  }
  if (timelimit >= 0) {
    if (verbose) {
      printf ("c\nc setting time limit of %d seconds\n", timelimit);
      fflush (stdout);
    }
    lglseterm (lgl, checkalarm, &caughtalarm);
    sig_alrm_handler = signal (SIGALRM, catchalrm);
    alarm (timelimit);
  }
  if (multiple) lglpushtarget (0), ntargets--;
  else for (i = 0; i < ntargets; i++) lglassume (lgl, targets[i]);
  if (multiple) res = lglmosat (lgl, lgl, lglmonotifiy, targets);
  else if (simponly) res = lglsimp (lgl, 0);
  else res = lglsat (lgl);
  if (timelimit >= 0) {
    caughtalarm = 0;
    (void) signal (SIGALRM, sig_alrm_handler);
  }
  if (oname) {
    double start = lglsec (lgl), delta;
    if (!strcmp (oname, "-")) out = stdout, oname = "<stdout>", clout = 0;
    else if (!(out = writefile (oname, &clout))) { res = 1; goto DONE; }
    if (verbose) {
      count = 0;
      lglctrav (lgl, &count, lgltravcounter);
      printf ("c\nc writing 'p cnf %d %d' to '%s'\n",
	      lglmaxvar (lgl), count, oname);
      fflush (stdout);
    }
    lglprint (lgl, out);
    closefile (out, clout);
    if (verbose) {
      delta = lglsec (lgl) - start; if (delta < 0) delta = 0;
      printf ("c collected garbage and wrote '%s' in %.1f seconds\n", 
              oname, delta);
      printf ("c\n"), fflush (stdout);
    }
  }
  if (verbose) lglstats (lgl), fputs ("c\n", stdout);
  if (res == 10) fputs ("s SATISFIABLE\n", stdout);
  else if (res == 20) fputs ("s UNSATISFIABLE\n", stdout);
  else fputs ("s UNKNOWN\n", stdout);
  fflush (stdout);
  if (res == 10 && lglgetopt (lgl, "witness")) {
    obuf.pos = 0;
    maxvar = lglmaxvar (lgl);
    for (i = 1; i <= maxvar; i++) {
      lit = (lglderef (lgl, i) > 0) ? i : -i;
      print2obuf (&obuf, lit, stdout);
    }
    print2obuf (&obuf, 0, stdout);
    if (obuf.pos > 0) flushobuf (&obuf, stdout);
    fflush (stdout);
  }
DONE:
  closefile (in, clin);
  resetsighandlers ();
  lgl4sigh = 0;
  lglrelease (lgl);
  free (targets);
  return res;
}
