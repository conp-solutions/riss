/* Copyright 2010 Armin Biere, Johannes Kepler University, Linz, Austria */

#include "lglib.h"
#include "lglcfg.h"
#include "lglcflags.h"

#include <stdio.h>
#include <assert.h>

void lglbnr (const char * name) {
  const char * p = LGL_CFLAGS, * q, * n;
  printf ("c %s Version %s %s\n", name, LGL_VERSION, LGL_ID);
  printf ("c Copyright (C) 2010, Armin Biere, JKU, Linz, Austria."
          "  All rights reserved.\n");
  printf ("c released %s\n", LGL_RELEASED);
  printf ("c compiled %s\n", LGL_COMPILED);
  printf ("c %s\n", LGL_CC);
  assert (*p);
  for (;;) {
    fputs ("c ", stdout);
    for (q = p; *q && *q != ' '; q++)
      ;
    if (*q && q - p < 76) {
      for (;;) {
	for (n = q + 1; *n && *n != ' '; n++)
	  ;
	if (n - p >= 76) break;
	q = n;
	if (!*n) break;
      }
    }
    while (p < q) fputc (*p++, stdout);
    fputc ('\n', stdout);
    if (!*p) break;
    assert(*p == ' ');
    p++;
  }
  printf ("c %s\n", LGL_OS);
  fflush (stdout);
}
