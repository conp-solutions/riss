// Modified 9.4.2013 to read from stdin instead of file and give simple solution answer

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

/* This flag does not make sense yet, since we do not need the resolution
 * proof but only check correctness.
 *
 * #define ANALYZE 
 */
static size_t allocated;
static int maxvar;
static int maxclause;
static int count_trace;
static int size_trace;
static int * trace;
static int count_clauses;
static int ** clauses;
static int ** antecedents;
static char * connected;
static char * nonroots;
static int lineno;
static int count_literals;
static int count_antecedents;
static int * first;
static int * second;
static int * pos;
static int * neg;
static int * trail;
static int * top;
static int * next;
static signed char * assignment;
static int count_chains;
static int count_resolved;
static int count_empty_clauses;
static int verbose;

#ifdef ANALYZE
static int * reason;
static char * marked;
static int count_used_antecedents;
#endif

#define OUTOFMEMORY() \
  do { \
    die ("out of memory at %llu bytes", (unsigned long long) allocated); \
  } while (0)

#define NEWN(p,n) \
  do { \
    size_t bytes = sizeof (*(p)) * (n); \
    (p) = malloc (bytes); \
    if (!(p)) OUTOFMEMORY (); \
    memset ((p), 0, bytes); \
    allocated += bytes; \
  } while (0)

static void
msg (const char * fmt, ...)
{
  va_list ap;
  if (!verbose)
    return;
  fputs ("c [stc] ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

static void
die (const char * fmt, ...)
{
  va_list ap;
  fputs ("*** stc: ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
  exit (1);
}

static int
nextch (FILE * file)
{
  int res = getc (file);
  if (res == '\n')
    lineno++;
  return res;
}

static void
scan (FILE * file)
{
  int ch, num, sign;

  lineno = 1;
RESTART:
  ch = nextch (file);
  if (isspace (ch))
    goto RESTART;

  if (ch == 'c')
    {
      while ((ch = nextch (file)) != '\n' && ch != EOF)
	;

      if (ch != EOF)
	goto RESTART;
    }

  if (ch == EOF)
    return;

  if (ch == '*')
    die ("invalid character '*' (compact trace format not supported)");

  if (ch == '-')
    {
      sign = -1;
      ch = nextch (file);
    }
  else
    sign = 1;

  if (!isdigit (ch))
    die ("expected digit in line %d", lineno);

  num = ch - '0';
  while (isdigit (ch = nextch (file)))
    num = 10 * num + (ch - '0');

  num *= sign;

  if (!isspace (ch))
    die ("expected white space in line %d", lineno);

  if (count_trace == size_trace)
    {
      if (size_trace)
	{
	  allocated += size_trace * sizeof (trace[0]);
	  size_trace *= 2;
	  trace = realloc (trace, size_trace * sizeof (trace[0]));
	  if (!trace)
	    OUTOFMEMORY ();
	}
      else
	{
	  size_trace = 1;
	  NEWN (trace, size_trace);
	}
    }

  trace[count_trace++] = num;

  goto RESTART;
}

static void
parse (FILE * file)
{
  int i, clause_idx, ant, idx, lit, start;

  scan (file);

  msg ("scanned trace and read %d numbers", count_trace);

  i = 0;
  count_clauses = 0;
  while (i < count_trace)
    {
      clause_idx = trace[i++];
      if (clause_idx <= 0)
	die ("found non positive clause index %d", clause_idx);

      while (i < count_trace && (lit = trace[i]))
	{
	  idx = abs (lit);
	  if (idx > maxvar)
	    maxvar = idx;

	  i++;
	}

      if (i == count_trace)
	die ("zero sentinel of clause %d missing", clause_idx);

      i++;

      while (i < count_trace && (ant = trace[i]))
	{
	  if (trace[i] < 0)
	    die ("negative antecedent %d in clause %d", ant, clause_idx);

	  if (trace[i] > clause_idx)
	    die ("larger antecedent %d in clause %d", ant, clause_idx);

	  if (trace[i] == clause_idx)
	    die ("antecedent %d in same clause %d", ant, clause_idx);

	  i++;
	}

      if (i == count_trace)
	die ("zero terminating antecedents of clause %d missing", clause_idx);

      count_clauses++;

      if (clause_idx > maxclause)
	maxclause = clause_idx;

      i++;
    }

  msg ("found %d clauses", count_clauses);
  msg ("max clause %d", maxclause);
  msg ("max variable %d", maxvar);

  NEWN (clauses, maxclause + 1);
  NEWN (antecedents, maxclause + 1);
  NEWN (nonroots, maxclause + 1);
  NEWN (connected, maxclause + 1);

  i = 0;
  while (i < count_trace)
    {
      clause_idx = trace[i++] ;

      clauses[clause_idx] = trace + (start = i);

      if (trace[i++])
	{
	  while (trace[i++])
	    ;
	}
      else
	count_empty_clauses++;

      count_literals += i - start - 1;

      antecedents[clause_idx] = trace + (start = i);

      if (trace[i++])
	{
	  count_chains++;
	  while (trace[i++])
	    ;
	}

      count_antecedents += i - start - 1;
    }

  msg ("found %d literals", count_literals);
  msg ("found %d antecedents in %d chains", count_antecedents, count_chains);

#if 0
  for (i = 1; i <= maxclause; i++)
    {
      p = clauses[i];
      if (!p)
	continue;

      printf ("%d ", i);
      while (*p)
	printf ("%d ", *p++);

      printf ("0 ");
      p = antecedents[i];
      assert (p);
      while (*p)
	printf ("%d ", *p++);

      printf ("0\n");
    }
#endif
}

static void
untrail (void)
{
  int lit, idx;

  next = trail;
  while (top > trail)
    {
      lit = *--top;
      idx = abs (lit);
      assignment[idx] = 0;
#ifdef ANALYZE
      if (marked[idx])
	count_used_antecedents++;
      reason[idx] = 0;
      marked[idx] = 0;
#endif
    }
}

static int
assign (int lit, int r)
{
  signed char * p, val;
  int idx = abs (lit);

  val = (lit < 0) ? -1 : 1;
  p = assignment + idx;

  if (*p == val)
    return 1;

  if (*p == -val)
    return 0;

  assert (!*p);

  *p = val;
  *top++ = lit;
#ifdef ANALYZE
  reason[idx] = r;
#endif

  return 1;
}

static int
connect (int clause_idx)
{
  const int * p, * q;
  int lit, idx, ant;

  p = antecedents[clause_idx];
  assert (p);
  while ((ant = *p++))
    {
      q = clauses[ant];
      if (!q)
	die ("antecedent %d in clause %d undefined", ant, clause_idx);

#ifndef ANALYZE
      nonroots[ant] = 1;
#endif
      if (connected[ant])
	continue;

      connected[ant] = 1;

      lit = *q++;
      if (lit)
	{
	  idx = abs (lit);
	  if (lit > 0)
	    {
	      first[ant] = pos[idx];
	      pos[idx] = ant;
	    }
	  else
	    {
	      first[ant] = neg[idx];
	      neg[idx] = ant;
	    }

	  lit = *q;
	  if (lit)
	    {
	      idx = abs (lit);
	      if (lit > 0)
		{
		  second[ant] = pos[idx];
		  pos[idx] = ant;
		}
	      else
		{
		  second[ant] = neg[idx];
		  neg[idx] = ant;
		}
	    }
	  else if (!assign (q[-1], clause_idx))
	    return 0;
	}
    }

  return 1;
}

static void
disconnect (int clause_idx)
{
  const int * p, * q;
  int lit, idx, ant;

  p = antecedents[clause_idx];
  assert (p);
  while ((ant = *p++))
    {
      if (!connected[ant])
	continue;

      connected[ant] = 0;

      q = clauses[ant];
      lit = *q++;
      if (lit)
	{
	  idx = abs (lit);
	  first[ant] = 0;
	  if (lit > 0)
	    pos[idx] = 0;
	  else
	    neg[idx] = 0;

	  lit = *q++;
	  if (lit)
	    {
	      idx = abs (lit);
	      second[ant] = 0;
	      if (lit > 0)
		pos[idx] = 0;
	      else
		neg[idx] = 0;
	    }
	}
    }
}

static int
deref (int lit)
{
  return assignment[abs (lit)] * (lit < 0 ? -1 : 1);
}

static int
assume (int clause_idx)
{
  const int * p = clauses[clause_idx];
  int lit;
  while ((lit = *p++) && assign (-lit, 0))
    ;
  return !lit;
}

static int
visit (int lit)
{
  int * literals, * p, * q, idx, clause_idx, tmp, other, other_idx, next;
  
  idx = abs (lit);
  p = (lit < 0) ? neg + idx : pos + idx;

  assert (deref (lit) < 0);
  while ((clause_idx = *p))
    {
      literals = clauses[clause_idx];

      if (!literals[1])		/* falsified unit clause */
	return 0;

      tmp = literals[0];
      if (tmp != lit)
	{
	  assert (literals[1] == lit);
	  literals[1] = tmp;
	  literals[0] = lit;

	  tmp = first[clause_idx];
	  first[clause_idx] = second[clause_idx];
	  second[clause_idx] = tmp;
	}

      if (deref (literals[1]) > 0)
	{
	  p = first + clause_idx;
	  continue;
	}

      q = literals + 2;
      while ((other = *q))
	{
	  if (deref (other) < 0)
	    {
	      q++;
	      continue;
	    }

	  *q = lit;
	  literals[0] = other;
	  other_idx = abs (other);
	  next = first[clause_idx];
	  if (other > 0)
	    {
	      first[clause_idx] = pos[other_idx];
	      pos[other_idx] = clause_idx;
	    }
	  else
	    {
	      first[clause_idx] = neg[other_idx];
	      neg[other_idx] = clause_idx;
	    }

	  *p = next;
	  assert (other);
	  break;			/* and continue outer loop */
	}

      if (other)
	continue;

      if (!assign (literals[1], clause_idx))
	return 0;
    }

  return 1;
}

static int
bcp (void)
{
  while (next < top)
    if (!visit (-*next++))
      return 0;

  return 1;
}

static int
cmp (const void * p, const void * q)
{
  return *(const int*)p - *(const int*)q;
}

static void
unique (int * start)
{
  int * end, * p, * q;

  for (end = start; *end; end++)
    ;

  if (start == end)
    return;

  qsort (start, end - start, sizeof (start[0]), cmp);

  for (p = q = start + 1; p < end; p++)
    if (*p != q[-1])
      *q++ = *p;

  assert (q <= end);
  *q++ = 0;
}

static void
resolve (int clause_idx)
{
  unique (clauses[clause_idx]);

  if (!*antecedents[clause_idx])
    return;

  if (connect (clause_idx) && assume (clause_idx) && bcp ())
    die ("clause %d can not be resolved from antecedents", clause_idx);

  untrail ();
  disconnect (clause_idx);
  count_resolved++;
}

static void
check (void)
{
  int i;

  NEWN (first, maxclause + 1);
  NEWN (second, maxclause + 1);

  NEWN (trail, maxvar + 1);
  NEWN (pos, maxvar + 1);
  NEWN (neg, maxvar + 1);
  NEWN (assignment, maxvar + 1);
#ifdef ANALYZE
  NEWN (marked, maxvar + 1);
  NEWN (reason, maxvar + 1);
#endif

  top = next = trail;
  for (i = 1; i <= maxclause; i++)
    if (clauses[i])
      resolve (i);

  msg ("resolved %d clauses", count_resolved);
#ifdef ANALYZE
  msg ("used %d antecedents", count_used_antecedents);
#endif
}

int
main (int argc, char ** argv)
{
  int i, count_roots;
  const char * name;
  FILE * file;

  name = 0;
  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-h"))
	{
	  fprintf (stderr, "usage: stc [-h][-v] <trace>\n");
	  exit (0);
	}

      if (!strcmp (argv[i], "-v"))
	verbose = 1;
      else if (argv[i][0] == '-')
	die ("invalid command line option '%s'", argv[i]);
      else if (name)
	die ("multiple input files");
      else
	name = argv[i];
    }

  //if (!name)
  //  die ("no trace given");

  //if (!(file = fopen (name, "r")))
  //  die ("can not read '%s'", name);
  
  // Modified 9.4.2013 to read from stdin instead of file
  file = stdin;

  parse (file);

  if (name)
    fclose (file);

  check ();

  count_roots = 0;
  for (i = 1; i <= maxclause; i++)
    count_roots += antecedents[i] && antecedents[i][0] && !nonroots[i];

  //printf ("resolved %d root%s and %d empty clause%s\n",
  //        count_roots, count_roots == 1 ? "" : "s", 
  //	  count_empty_clauses, count_empty_clauses == 1 ? "" : "s");
  // Modified 9.4.2013 to print simple verification line
  if (count_empty_clauses > 0) printf("s VERIFIED\n"); else printf("s No empty clauses were resolved\n");
	  

#ifndef NDEBUG
  free (trace);
  free (clauses);
  free (antecedents);
  free (nonroots);
  free (connected);
  free (first);
  free (second);
  free (trail);
  free (pos);
  free (neg);
  free (assignment);
#ifdef ANALYZE
  free (marked);
  free (reason);
#endif
#endif
  msg ("allocated %llu bytes", (unsigned long long) allocated);

  return 0;
}
