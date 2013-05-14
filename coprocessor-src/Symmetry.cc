/*************************************************************************************[Symmetry.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Symmetry.h"

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - SYMMETRY";

static IntOption pr_uip            (_cat, "sym-uips",   "perform learning if a conflict occurs up to x-th UIP (-1 = all )", -1, IntRange(-1, INT32_MAX));
static BoolOption pr_double        (_cat, "sym-double", "perform double look-ahead",true);

#if defined CP3VERSION  
static const int debug_out = 0;
#else
static IntOption debug_out        (_cat, "sym-debug", "debug output for probing",0, IntRange(0,4) );
#endif



