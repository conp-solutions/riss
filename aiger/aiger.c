/***************************************************************************
Copyright (c) 2011, Siert Wieringa, Aalto University, Finl_and.
Copyright (c) 2006-2011, Armin Biere, Johannes Kepler University.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software _and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, _and/or
sell copies of the Software, _and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice _and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
***************************************************************************/

#include "aiger/aiger.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

/*------------------------------------------------------------------------*/

// TODO move this to seperate file _and sync it with git hash

const char *
aiger_id (void)
{
    return "invalid id";
}

/*------------------------------------------------------------------------*/

const char *
aiger_version (void)
{
    return AIGER_VERSION;
}

/*------------------------------------------------------------------------*/

#define GZIP "gzip -c > %s 2>/dev/null"
#define GUNZIP "gunzip -c %s 2>/dev/null"

#define NEWN(p,n) \
    do { \
        size_t bytes = (n) * sizeof (*(p)); \
        (p) = _private->malloc_callback (_private->memory_mgr, bytes); \
        memset ((p), 0, bytes); \
    } while (0)

#define REALLOCN(p,m,n) \
    do { \
        size_t mbytes = (m) * sizeof (*(p)); \
        size_t nbytes = (n) * sizeof (*(p)); \
        size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; \
        void * res = _private->malloc_callback (_private->memory_mgr, nbytes); \
        memcpy (res, (p), minbytes); \
        if (nbytes > mbytes) \
            memset (((char*)res) + mbytes, 0, nbytes - mbytes); \
        _private->free_callback (_private->memory_mgr, (p), mbytes); \
        (p) = res; \
    } while (0)

#define FIT(p,m,n) \
    do { \
        size_t old_size = (m); \
        size_t _new_size = (n); \
        if (old_size < _new_size) \
        { \
            REALLOCN (p,old_size,_new_size); \
            (m) = _new_size; \
        } \
    } while (0)

#define ENLARGE(p,s) \
    do { \
        size_t old_size = (s); \
        size_t _new_size = old_size ? 2 * old_size : 1; \
        REALLOCN (p,old_size,_new_size); \
        (s) = _new_size; \
    } while (0)

#define PUSH(p,t,s,l) \
    do { \
        if ((t) == (s)) \
            ENLARGE (p, s); \
        (p)[(t)++] = (l); \
    } while (0)

#define DELETEN(p,n) \
    do { \
        size_t bytes = (n) * sizeof (*(p)); \
        _private->free_callback (_private->memory_mgr, (p), bytes); \
        (p) = 0; \
    } while (0)

#define CLR(p) do { memset (&(p), 0, sizeof (p)); } while (0)

#define NEW(p) NEWN (p,1)
#define DELETE(p) DELETEN (p,1)

#define IMPORT_private_FROM(p) \
    aiger_private * _private = (aiger_private*) (p)

#define EXPORT_public_FROM(p) \
    aiger * _public = &(p)->_public

typedef struct aiger_private aiger_private;
typedef struct aiger_buffer aiger_buffer;
typedef struct aiger_reader aiger_reader;
typedef struct aiger_type aiger_type;

struct aiger_type {
    unsigned input: 1;
    unsigned latch: 1;
    unsigned _and: 1;

    unsigned mark: 1;
    unsigned onstack: 1;

    /* Index int to '_public->{inputs,latches,_ands}'.
     */
    unsigned idx;
};

struct aiger_private {
    aiger _public;

    aiger_type *types;        /* [0..maxvar] */
    unsigned size_types;

    unsigned char * coi;
    unsigned size_coi;

    unsigned size_inputs;
    unsigned size_latches;
    unsigned size_outputs;
    unsigned size_ands;
    unsigned size_bad;
    unsigned size_constraints;
    unsigned size_justice;
    unsigned size_fairness;

    unsigned num_comments;
    unsigned size_comments;

    void *memory_mgr;
    aiger_malloc malloc_callback;
    aiger_free free_callback;

    char *error;
};

struct aiger_buffer {
    char *start;
    char *cursor;
    char *end;
};

struct aiger_reader {
    void *state;
    aiger_get get;

    int ch;

    unsigned lineno;
    unsigned charno;

    unsigned lineno_at_last_token_start;

    int done_with_reading_header;
    int looks_like_aag;

    aiger_mode mode;
    unsigned maxvar;
    unsigned inputs;
    unsigned latches;
    unsigned outputs;
    unsigned _ands;
    unsigned bad;
    unsigned constraints;
    unsigned justice;
    unsigned fairness;

    char *buffer;
    unsigned top_buffer;
    unsigned size_buffer;
};

aiger *
aiger_init_mem (void *memory_mgr,
                aiger_malloc external_malloc, aiger_free external_free)
{
    aiger_private *_private;
    aiger *_public;

    assert (external_malloc);
    assert (external_free);
    _private = (aiger_private *)external_malloc (memory_mgr, sizeof (*_private));
    CLR (*_private);
    _private->memory_mgr = memory_mgr;
    _private->malloc_callback = external_malloc;
    _private->free_callback = external_free;
    _public = &_private->_public;
    PUSH (_public->comments, _private->num_comments, _private->size_comments, 0);

    return _public;
}

static void *
aiger_default_malloc (void *state, size_t bytes)
{
    return malloc (bytes);
}

static void
aiger_default_free (void *state, void *ptr, size_t bytes)
{
    free (ptr);
}

aiger *
aiger_init (void)
{
    return aiger_init_mem (0, aiger_default_malloc, aiger_default_free);
}

static void
aiger_delete_str (aiger_private * _private, char *str)
{
    if (str)
    { DELETEN (str, strlen (str) + 1); }
}

static char *
aiger_copy_str (aiger_private * _private, const char *str)
{
    char *res;

    if (!str || !str[0])
    { return 0; }

    NEWN (res, strlen (str) + 1);
    strcpy (res, str);

    return res;
}

static unsigned
aiger_delete_symbols_aux (aiger_private * _private,
                          aiger_symbol * symbols, unsigned size)
{
    unsigned i, res;

    res = 0;
    for (i = 0; i < size; i++) {
        aiger_symbol *s = symbols + i;
        if (!s->name)
        { continue; }

        aiger_delete_str (_private, s->name);
        s->name = 0;
        res++;
    }

    return res;
}

static void
aiger_delete_symbols (aiger_private * _private,
                      aiger_symbol * symbols, unsigned size)
{
    aiger_delete_symbols_aux (_private, symbols, size);
    DELETEN (symbols, size);
}

static unsigned
aiger_delete_comments (aiger * _public)
{
    char **start = (char **) _public->comments, ** end, ** p;
    IMPORT_private_FROM (_public);

    end = start + _private->num_comments;
    for (p = start; p < end; p++)
    { aiger_delete_str (_private, *p); }

    _private->num_comments = 0;
    _public->comments[0] = 0;

    return _private->num_comments;
}

void
aiger_reset (aiger * _public)
{
    unsigned i;

    IMPORT_private_FROM (_public);

    aiger_delete_symbols (_private, _public->inputs, _private->size_inputs);
    aiger_delete_symbols (_private, _public->latches, _private->size_latches);
    aiger_delete_symbols (_private, _public->outputs, _private->size_outputs);
    aiger_delete_symbols (_private, _public->bad, _private->size_bad);
    aiger_delete_symbols (_private, _public->constraints,
                          _private->size_constraints);
    for (i = 0; i < _public->num_justice; i++)
    { DELETEN (_public->justice[i].lits, _public->justice[i].size); }
    aiger_delete_symbols (_private, _public->justice, _private->size_justice);
    aiger_delete_symbols (_private, _public->fairness, _private->size_fairness);
    DELETEN (_public->ands, _private->size_ands);

    aiger_delete_comments (_public);
    DELETEN (_public->comments, _private->size_comments);

    DELETEN (_private->coi, _private->size_coi);

    DELETEN (_private->types, _private->size_types);
    aiger_delete_str (_private, _private->error);
    DELETE (_private);
}

static aiger_type *
aiger_import_literal (aiger_private * _private, unsigned lit)
{
    unsigned var = aiger_lit2var (lit);
    EXPORT_public_FROM (_private);

    if (var > _public->maxvar)
    { _public->maxvar = var; }

    while (var >= _private->size_types)
    { ENLARGE (_private->types, _private->size_types); }

    return _private->types + var;
}

void
aiger_add_input (aiger * _public, unsigned lit, const char *name)
{
    IMPORT_private_FROM (_public);
    aiger_symbol symbol;
    aiger_type *type;

    assert (!aiger_error (_public));

    assert (lit);
    assert (!aiger_sign (lit));

    type = aiger_import_literal (_private, lit);

    assert (!type->input);
    assert (!type->latch);
    assert (!type->_and);

    type->input = 1;
    type->idx = _public->num_inputs;

    CLR (symbol);
    symbol.lit = lit;
    symbol.name = aiger_copy_str (_private, name);

    PUSH (_public->inputs, _public->num_inputs, _private->size_inputs, symbol);
}

void
aiger_add_latch (aiger * _public,
                 unsigned lit, unsigned next, const char *name)
{
    IMPORT_private_FROM (_public);
    aiger_symbol symbol;
    aiger_type *type;

    assert (!aiger_error (_public));

    assert (lit);
    assert (!aiger_sign (lit));

    type = aiger_import_literal (_private, lit);

    assert (!type->input);
    assert (!type->latch);
    assert (!type->_and);

    /* Warning: importing 'next' makes 'type' invalid.
     */
    type->latch = 1;
    type->idx = _public->num_latches;

    aiger_import_literal (_private, next);

    CLR (symbol);
    symbol.lit = lit;
    symbol.next = next;
    symbol.name = aiger_copy_str (_private, name);

    PUSH (_public->latches, _public->num_latches, _private->size_latches, symbol);
}

void
aiger_add_reset (aiger * _public, unsigned lit, unsigned reset)
{
    IMPORT_private_FROM (_public);
    aiger_type * type;
    assert (reset <= 1 || reset == lit);
    assert (!aiger_error (_public));
    assert (lit);
    assert (!aiger_sign (lit));
    type = aiger_import_literal (_private, lit);
    assert (type->latch);
    assert (type->idx < _public->num_latches);
    _public->latches[type->idx].reset = reset;
}

void
aiger_add_output (aiger * _public, unsigned lit, const char *name)
{
    IMPORT_private_FROM (_public);
    aiger_symbol symbol;
    aiger_import_literal (_private, lit);
    CLR (symbol);
    symbol.lit = lit;
    symbol.name = aiger_copy_str (_private, name);
    PUSH (_public->outputs, _public->num_outputs, _private->size_outputs, symbol);
}

void
aiger_add_bad (aiger * _public, unsigned lit, const char *name)
{
    IMPORT_private_FROM (_public);
    aiger_symbol symbol;
    aiger_import_literal (_private, lit);
    CLR (symbol);
    symbol.lit = lit;
    symbol.name = aiger_copy_str (_private, name);
    PUSH (_public->bad, _public->num_bad, _private->size_bad, symbol);
}

void
aiger_add_constraint (aiger * _public, unsigned lit, const char *name)
{
    IMPORT_private_FROM (_public);
    aiger_symbol symbol;
    aiger_import_literal (_private, lit);
    CLR (symbol);
    symbol.lit = lit;
    symbol.name = aiger_copy_str (_private, name);
    PUSH (_public->constraints,
          _public->num_constraints, _private->size_constraints, symbol);
}

void
aiger_add_justice (aiger * _public,
                   unsigned size, unsigned * lits,
                   const char * name)
{
    IMPORT_private_FROM (_public);
    aiger_symbol symbol;
    unsigned i, lit;
    CLR (symbol);
    symbol.size = size;
    NEWN (symbol.lits, size);
    for (i = 0; i < size; i++) {
        lit = lits[i];
        aiger_import_literal (_private, lit);
        symbol.lits[i] = lit;
    }
    symbol.name = aiger_copy_str (_private, name);
    PUSH (_public->justice,
          _public->num_justice, _private->size_justice, symbol);
}

void
aiger_add_fairness (aiger * _public, unsigned lit, const char *name)
{
    IMPORT_private_FROM (_public);
    aiger_symbol symbol;
    aiger_import_literal (_private, lit);
    CLR (symbol);
    symbol.lit = lit;
    symbol.name = aiger_copy_str (_private, name);
    PUSH (_public->fairness,
          _public->num_fairness, _private->size_fairness, symbol);
}

void
aiger_add_and (aiger * _public, unsigned lhs, unsigned rhs0, unsigned rhs1)
{
    IMPORT_private_FROM (_public);
    aiger_type *type;
    aiger_and *_and;

    assert (!aiger_error (_public));

    assert (lhs > 1);
    assert (!aiger_sign (lhs));

    type = aiger_import_literal (_private, lhs);

    assert (!type->input);
    assert (!type->latch);
    assert (!type->_and);

    type->_and = 1;
    type->idx = _public->num_ands;

    aiger_import_literal (_private, rhs0);
    aiger_import_literal (_private, rhs1);

    if (_public->num_ands == _private->size_ands)
    { ENLARGE (_public->ands, _private->size_ands); }

    _and = _public->ands + _public->num_ands;

    _and->lhs = lhs;
    _and->rhs0 = rhs0;
    _and->rhs1 = rhs1;

    _public->num_ands++;
}

void
aiger_add_comment (aiger * _public, const char *comment)
{
    IMPORT_private_FROM (_public);
    char **p;

    assert (!aiger_error (_public));

    assert (!strchr (comment, '\n'));
    assert (_private->num_comments);
    p = _public->comments + _private->num_comments - 1;
    assert (!*p);
    *p = aiger_copy_str (_private, comment);
    PUSH (_public->comments, _private->num_comments, _private->size_comments, 0);
}

static const char *
aiger_error_s (aiger_private * _private, const char *s, const char *a)
{
    unsigned tmp_len, error_len;
    char *tmp;
    assert (!_private->error);
    tmp_len = strlen (s) + strlen (a) + 1;
    NEWN (tmp, tmp_len);
    sprintf (tmp, s, a);
    error_len = strlen (tmp) + 1;
    NEWN (_private->error, error_len);
    memcpy (_private->error, tmp, error_len);
    DELETEN (tmp, tmp_len);
    return _private->error;
}

static const char *
aiger_error_u (aiger_private * _private, const char *s, unsigned u)
{
    unsigned tmp_len, error_len;
    char *tmp;
    assert (!_private->error);
    tmp_len = strlen (s) + sizeof (u) * 4 + 1;
    NEWN (tmp, tmp_len);
    sprintf (tmp, s, u);
    error_len = strlen (tmp) + 1;
    NEWN (_private->error, error_len);
    memcpy (_private->error, tmp, error_len);
    DELETEN (tmp, tmp_len);
    return _private->error;
}

static const char *
aiger_error_uu (aiger_private * _private, const char *s, unsigned a,
                unsigned b)
{
    unsigned tmp_len, error_len;
    char *tmp;
    assert (!_private->error);
    tmp_len = strlen (s) + sizeof (a) * 4 + sizeof (b) * 4 + 1;
    NEWN (tmp, tmp_len);
    sprintf (tmp, s, a, b);
    error_len = strlen (tmp) + 1;
    NEWN (_private->error, error_len);
    memcpy (_private->error, tmp, error_len);
    DELETEN (tmp, tmp_len);
    return _private->error;
}

static const char *
aiger_error_usu (aiger_private * _private,
                 const char *s, unsigned a, const char *t, unsigned b)
{
    unsigned tmp_len, error_len;
    char *tmp;
    assert (!_private->error);
    tmp_len = strlen (s) + strlen (t) + sizeof (a) * 4 + sizeof (b) * 4 + 1;
    NEWN (tmp, tmp_len);
    sprintf (tmp, s, a, t, b);
    error_len = strlen (tmp) + 1;
    NEWN (_private->error, error_len);
    memcpy (_private->error, tmp, error_len);
    DELETEN (tmp, tmp_len);
    return _private->error;
}

const char *
aiger_error (aiger * _public)
{
    IMPORT_private_FROM (_public);
    return _private->error;
}

static int
aiger_literal_defined (aiger_private * _private, unsigned lit)
{
    unsigned var = aiger_lit2var (lit);
    #ifndef NDEBUG
    EXPORT_public_FROM (_private);
    #endif
    aiger_type *type;

    assert (var <= _public->maxvar);
    if (!var)
    { return 1; }

    type = _private->types + var;

    return type->_and || type->input || type->latch;
}

static void
aiger_check_next_defined (aiger_private * _private)
{
    EXPORT_public_FROM (_private);
    unsigned i, next, latch;
    aiger_symbol *symbol;

    if (_private->error)
    { return; }

    for (i = 0; !_private->error && i < _public->num_latches; i++) {
        symbol = _public->latches + i;
        latch = symbol->lit;
        next = symbol->next;

        assert (!aiger_sign (latch));
        assert (_private->types[aiger_lit2var (latch)].latch);

        if (!aiger_literal_defined (_private, next))
            aiger_error_uu (_private,
                            "next state function %u of latch %u undefined",
                            next, latch);
    }
}

static void
aiger_check_right_h_and_side_defined (aiger_private * _private, aiger_and * _and,
                                      unsigned rhs)
{
    if (_private->error)
    { return; }

    assert (_and);
    if (!aiger_literal_defined (_private, rhs))
    { aiger_error_uu (_private, "literal %u in AND %u undefined", rhs, _and->lhs); }
}

static void
aiger_check_right_h_and_sides_defined (aiger_private * _private)
{
    EXPORT_public_FROM (_private);
    aiger_and *_and;
    unsigned i;

    if (_private->error)
    { return; }

    for (i = 0; !_private->error && i < _public->num_ands; i++) {
        _and = _public->ands + i;
        aiger_check_right_h_and_side_defined (_private, _and, _and->rhs0);
        aiger_check_right_h_and_side_defined (_private, _and, _and->rhs1);
    }
}

static void
aiger_check_outputs_defined (aiger_private * _private)
{
    EXPORT_public_FROM (_private);
    unsigned i, output;

    if (_private->error)
    { return; }

    for (i = 0; !_private->error && i < _public->num_outputs; i++) {
        output = _public->outputs[i].lit;
        output = aiger_strip (output);
        if (output <= 1)
        { continue; }

        if (!aiger_literal_defined (_private, output))
        { aiger_error_u (_private, "output %u undefined", output); }
    }
}

static void
aiger_check_bad_defined (aiger_private * _private)
{
    EXPORT_public_FROM (_private);
    unsigned i, bad;

    if (_private->error)
    { return; }

    for (i = 0; !_private->error && i < _public->num_bad; i++) {
        bad = _public->bad[i].lit;
        bad = aiger_strip (bad);
        if (bad <= 1)
        { continue; }

        if (!aiger_literal_defined (_private, bad))
        { aiger_error_u (_private, "bad %u undefined", bad); }
    }
}

static void
aiger_check_constraints_defined (aiger_private * _private)
{
    EXPORT_public_FROM (_private);
    unsigned i, constraint;

    if (_private->error)
    { return; }

    for (i = 0; !_private->error && i < _public->num_constraints; i++) {
        constraint = _public->constraints[i].lit;
        constraint = aiger_strip (constraint);
        if (constraint <= 1)
        { continue; }

        if (!aiger_literal_defined (_private, constraint))
        { aiger_error_u (_private, "constraint %u undefined", constraint); }
    }
}

static void
aiger_check_fairness_defined (aiger_private * _private)
{
    EXPORT_public_FROM (_private);
    unsigned i, fairness;

    if (_private->error)
    { return; }

    for (i = 0; !_private->error && i < _public->num_fairness; i++) {
        fairness = _public->fairness[i].lit;
        fairness = aiger_strip (fairness);
        if (fairness <= 1)
        { continue; }

        if (!aiger_literal_defined (_private, fairness))
        { aiger_error_u (_private, "fairness %u undefined", fairness); }
    }
}

static void
aiger_check_justice_defined (aiger_private * _private)
{
    EXPORT_public_FROM (_private);
    unsigned i, j, justice;

    if (_private->error)
    { return; }

    for (i = 0; !_private->error && i < _public->num_justice; i++) {
        for (j = 0; !_private->error && j < _public->justice[i].size; j++) {
            justice = _public->justice[i].lits[j];
            justice = aiger_strip (justice);
            if (justice <= 1)
            { continue; }

            if (!aiger_literal_defined (_private, justice))
            { aiger_error_u (_private, "justice %u undefined", justice); }
        }
    }
}

static void
aiger_check_for_cycles (aiger_private * _private)
{
    unsigned i, j, *stack, size_stack, top_stack, tmp;
    EXPORT_public_FROM (_private);
    aiger_type *type;
    aiger_and *_and;

    if (_private->error)
    { return; }

    stack = 0;
    size_stack = top_stack = 0;

    for (i = 1; !_private->error && i <= _public->maxvar; i++) {
        type = _private->types + i;

        if (!type->_and || type->mark)
        { continue; }

        PUSH (stack, top_stack, size_stack, i);
        while (top_stack) {
            j = stack[top_stack - 1];

            if (j) {
                type = _private->types + j;
                if (type->mark && type->onstack) {
                    aiger_error_u (_private,
                                   "cyclic definition for _and gate %u", j);
                    break;
                }

                if (!type->_and || type->mark) {
                    top_stack--;
                    continue;
                }

                /* Prefix code.
                 */
                type->mark = 1;
                type->onstack = 1;
                PUSH (stack, top_stack, size_stack, 0);

                assert (type->idx < _public->num_ands);
                _and = _public->ands + type->idx;

                tmp = aiger_lit2var (_and->rhs0);
                if (tmp)
                { PUSH (stack, top_stack, size_stack, tmp); }

                tmp = aiger_lit2var (_and->rhs1);
                if (tmp)
                { PUSH (stack, top_stack, size_stack, tmp); }
            } else {
                /* All descendends traversed.  This is the postfix code.
                 */
                assert (top_stack >= 2);
                top_stack -= 2;
                j = stack[top_stack];
                assert (j);
                type = _private->types + j;
                assert (type->mark);
                assert (type->onstack);
                type->onstack = 0;
            }
        }
    }

    DELETEN (stack, size_stack);
}

const char *
aiger_check (aiger * _public)
{
    IMPORT_private_FROM (_public);

    assert (!aiger_error (_public));

    aiger_check_next_defined (_private);
    aiger_check_outputs_defined (_private);
    aiger_check_bad_defined (_private);
    aiger_check_constraints_defined (_private);
    aiger_check_justice_defined (_private);
    aiger_check_fairness_defined (_private);
    aiger_check_right_h_and_sides_defined (_private);
    aiger_check_for_cycles (_private);

    return _private->error;
}

static int
aiger_default_get (FILE * file)
{
    return getc (file);
}

static int
aiger_default_put (char ch, FILE * file)
{
    return putc ((unsigned char) ch, file);
}

static int
aiger_string_put (char ch, aiger_buffer * buffer)
{
    if (buffer->cursor == buffer->end)
    { return EOF; }
    *buffer->cursor++ = ch;
    return ch;
}

static int
aiger_put_s (void *state, aiger_put put, const char *str)
{
    const char *p;
    char ch;

    for (p = str; (ch = *p); p++)
        if (put (ch, state) == EOF)
        { return EOF; }

    return p - str;       /* 'fputs' semantics, >= 0 is OK */
}

static int
aiger_put_u (void *state, aiger_put put, unsigned u)
{
    char buffer[sizeof (u) * 4];
    sprintf (buffer, "%u", u);
    return aiger_put_s (state, put, buffer);
}

static int
aiger_write_delta (void *state, aiger_put put, unsigned delta)
{
    unsigned char ch;
    unsigned tmp = delta;

    while (tmp & ~0x7f) {
        ch = tmp & 0x7f;
        ch |= 0x80;

        if (put (ch, state) == EOF)
        { return 0; }

        tmp >>= 7;
    }

    ch = tmp;
    return put (ch, state) != EOF;
}

static int
aiger_write_header (aiger * _public,
                    const char *format_string,
                    int compact_inputs_and_latches,
                    void *state, aiger_put put)
{
    unsigned i, j;

    if (aiger_put_s (state, put, format_string) == EOF) { return 0; }
    if (put (' ', state) == EOF) { return 0; }
    if (aiger_put_u (state, put, _public->maxvar) == EOF) { return 0; }
    if (put (' ', state) == EOF) { return 0; }
    if (aiger_put_u (state, put, _public->num_inputs) == EOF) { return 0; }
    if (put (' ', state) == EOF) { return 0; }
    if (aiger_put_u (state, put, _public->num_latches) == EOF) { return 0; }
    if (put (' ', state) == EOF) { return 0; }
    if (aiger_put_u (state, put, _public->num_outputs) == EOF) { return 0; }
    if (put (' ', state) == EOF) { return 0; }
    if (aiger_put_u (state, put, _public->num_ands) == EOF) { return 0; }

    if (_public->num_bad ||
            _public->num_constraints ||
            _public->num_justice ||
            _public->num_fairness) {
        if (put (' ', state) == EOF) { return 0; }
        if (aiger_put_u (state, put, _public->num_bad) == EOF) { return 0; }
    }

    if (_public->num_constraints ||
            _public->num_justice ||
            _public->num_fairness) {
        if (put (' ', state) == EOF) { return 0; }
        if (aiger_put_u (state, put, _public->num_constraints) == EOF) { return 0; }
    }

    if (_public->num_justice ||
            _public->num_fairness) {
        if (put (' ', state) == EOF) { return 0; }
        if (aiger_put_u (state, put, _public->num_justice) == EOF) { return 0; }
    }

    if (_public->num_fairness) {
        if (put (' ', state) == EOF) { return 0; }
        if (aiger_put_u (state, put, _public->num_fairness) == EOF) { return 0; }
    }

    if (put ('\n', state) == EOF) { return 0; }

    if (!compact_inputs_and_latches && _public->num_inputs) {
        for (i = 0; i < _public->num_inputs; i++)
            if (aiger_put_u (state, put, _public->inputs[i].lit) == EOF ||
                    put ('\n', state) == EOF)
            { return 0; }
    }

    if (_public->num_latches) {
        for (i = 0; i < _public->num_latches; i++) {
            if (!compact_inputs_and_latches) {
                if (aiger_put_u (state, put, _public->latches[i].lit) == EOF)
                { return 0; }
                if (put (' ', state) == EOF) { return 0; }
            }

            if (aiger_put_u (state, put, _public->latches[i].next) == EOF)
            { return 0; }

            if (_public->latches[i].reset) {
                if (put (' ', state) == EOF) { return 0; }
                if (aiger_put_u (state, put, _public->latches[i].reset) == EOF)
                { return 0; }
            }
            if (put ('\n', state) == EOF) { return 0; }
        }
    }

    if (_public->num_outputs) {
        for (i = 0; i < _public->num_outputs; i++)
            if (aiger_put_u (state, put, _public->outputs[i].lit) == EOF ||
                    put ('\n', state) == EOF)
            { return 0; }
    }

    if (_public->num_bad) {
        for (i = 0; i < _public->num_bad; i++)
            if (aiger_put_u (state, put, _public->bad[i].lit) == EOF ||
                    put ('\n', state) == EOF)
            { return 0; }
    }

    if (_public->num_constraints) {
        for (i = 0; i < _public->num_constraints; i++)
            if (aiger_put_u (state, put, _public->constraints[i].lit) == EOF ||
                    put ('\n', state) == EOF)
            { return 0; }
    }

    if (_public->num_justice) {
        for (i = 0; i < _public->num_justice; i++) {
            if (aiger_put_u (state, put, _public->justice[i].size) == EOF)
            { return 0; }
            if (put ('\n', state) == EOF) { return 0; }
        }

        for (i = 0; i < _public->num_justice; i++) {
            for (j = 0; j < _public->justice[i].size; j++) {
                if (aiger_put_u (state, put, _public->justice[i].lits[j]) == EOF)
                { return 0; }
                if (put ('\n', state) == EOF) { return 0; }
            }
        }
    }

    if (_public->num_fairness) {
        for (i = 0; i < _public->num_fairness; i++)
            if (aiger_put_u (state, put, _public->fairness[i].lit) == EOF ||
                    put ('\n', state) == EOF)
            { return 0; }
    }

    return 1;
}

static int
aiger_have_at_least_one_symbol_aux (aiger * _public,
                                    aiger_symbol * symbols, unsigned size)
{
    unsigned i;

    for (i = 0; i < size; i++)
        if (symbols[i].name)
        { return 1; }

    return 0;
}

static int
aiger_have_at_least_one_symbol (aiger * _public)
{
    if (aiger_have_at_least_one_symbol_aux (_public,
                                            _public->inputs, _public->num_inputs))
    { return 1; }

    if (aiger_have_at_least_one_symbol_aux (_public,
                                            _public->outputs,
                                            _public->num_outputs))
    { return 1; }

    if (aiger_have_at_least_one_symbol_aux (_public,
                                            _public->latches,
                                            _public->num_latches))
    { return 1; }

    if (aiger_have_at_least_one_symbol_aux (_public,
                                            _public->bad,
                                            _public->num_bad))
    { return 1; }

    if (aiger_have_at_least_one_symbol_aux (_public,
                                            _public->constraints,
                                            _public->num_constraints))
    { return 1; }

    if (aiger_have_at_least_one_symbol_aux (_public,
                                            _public->justice,
                                            _public->num_justice))
    { return 1; }

    if (aiger_have_at_least_one_symbol_aux (_public,
                                            _public->fairness,
                                            _public->num_fairness))
    { return 1; }

    return 0;
}

static int
aiger_write_symbols_aux (aiger * _public,
                         void *state, aiger_put put,
                         const char *type,
                         aiger_symbol * symbols, unsigned size)
{
    unsigned i;

    for (i = 0; i < size; i++) {
        if (!symbols[i].name)
        { continue; }

        assert (symbols[i].name[0]);

        if (aiger_put_s (state, put, type) == EOF ||
                aiger_put_u (state, put, i) == EOF ||
                put (' ', state) == EOF ||
                aiger_put_s (state, put, symbols[i].name) == EOF ||
                put ('\n', state) == EOF)
        { return 0; }
    }

    return 1;
}

static int
aiger_write_symbols (aiger * _public, void *state, aiger_put put)
{
    if (!aiger_write_symbols_aux (_public, state, put,
                                  "i", _public->inputs, _public->num_inputs))
    { return 0; }

    if (!aiger_write_symbols_aux (_public, state, put,
                                  "l", _public->latches, _public->num_latches))
    { return 0; }

    if (!aiger_write_symbols_aux (_public, state, put,
                                  "o", _public->outputs, _public->num_outputs))
    { return 0; }

    if (!aiger_write_symbols_aux (_public, state, put,
                                  "b", _public->bad, _public->num_bad))
    { return 0; }

    if (!aiger_write_symbols_aux (_public, state, put,
                                  "c", _public->constraints,
                                  _public->num_constraints))
    { return 0; }

    if (!aiger_write_symbols_aux (_public, state, put,
                                  "j", _public->justice, _public->num_justice))
    { return 0; }

    if (!aiger_write_symbols_aux (_public, state, put,
                                  "f", _public->fairness, _public->num_fairness))
    { return 0; }

    return 1;
}

int
aiger_write_symbols_to_file (aiger * _public, FILE * file)
{
    assert (!aiger_error (_public));
    return aiger_write_symbols (_public, file, (aiger_put) aiger_default_put);
}

static int
aiger_write_comments (aiger * _public, void *state, aiger_put put)
{
    char **p, *str;

    for (p = _public->comments; (str = *p); p++) {
        if (aiger_put_s (state, put, str) == EOF)
        { return 0; }

        if (put ('\n', state) == EOF)
        { return 0; }
    }

    return 1;
}

int
aiger_write_comments_to_file (aiger * _public, FILE * file)
{
    assert (!aiger_error (_public));
    return aiger_write_comments (_public, file, (aiger_put) aiger_default_put);
}

static int
aiger_write_ascii (aiger * _public, void *state, aiger_put put)
{
    aiger_and *_and;
    unsigned i;

    assert (!aiger_check (_public));

    if (!aiger_write_header (_public, "aag", 0, state, put))
    { return 0; }

    for (i = 0; i < _public->num_ands; i++) {
        _and = _public->ands + i;
        if (aiger_put_u (state, put, _and->lhs) == EOF ||
                put (' ', state) == EOF ||
                aiger_put_u (state, put, _and->rhs0) == EOF ||
                put (' ', state) == EOF ||
                aiger_put_u (state, put, _and->rhs1) == EOF ||
                put ('\n', state) == EOF)
        { return 0; }
    }

    return 1;
}

static unsigned
aiger_max_input_or_latch (aiger * _public)
{
    unsigned i, tmp, res;

    res = 0;

    for (i = 0; i < _public->num_inputs; i++) {
        tmp = _public->inputs[i].lit;
        assert (!aiger_sign (tmp));
        if (tmp > res)
        { res = tmp; }
    }

    for (i = 0; i < _public->num_latches; i++) {
        tmp = _public->latches[i].lit;
        assert (!aiger_sign (tmp));
        if (tmp > res)
        { res = tmp; }
    }

    return res;
}

int
aiger_is_reencoded (aiger * _public)
{
    unsigned i, tmp, max, lhs;
    aiger_and *_and;

    max = 0;
    for (i = 0; i < _public->num_inputs; i++) {
        max += 2;
        tmp = _public->inputs[i].lit;
        if (max != tmp)
        { return 0; }
    }

    for (i = 0; i < _public->num_latches; i++) {
        max += 2;
        tmp = _public->latches[i].lit;
        if (max != tmp)
        { return 0; }
    }

    lhs = aiger_max_input_or_latch (_public) + 2;
    for (i = 0; i < _public->num_ands; i++) {
        _and = _public->ands + i;

        if (_and->lhs <= max)
        { return 0; }

        if (_and->lhs != lhs)
        { return 0; }

        if (_and->lhs < _and->rhs0)
        { return 0; }

        if (_and->rhs0 < _and->rhs1)
        { return 0; }

        lhs += 2;
    }

    return 1;
}

static void
aiger_new_code (unsigned var, unsigned *_new, unsigned *code)
{
    unsigned lit = aiger_var2lit (var), res;
    assert (!code[lit]);
    res = *_new;
    code[lit] = res;
    code[lit + 1] = res + 1;
    *_new += 2;
}

static unsigned
aiger_reencode_lit (aiger * _public, unsigned lit,
                    unsigned *_new, unsigned *code,
                    unsigned **stack_ptr, unsigned * size_stack_ptr)
{
    unsigned res, old, top, child0, child1, tmp, var, size_stack, * stack;
    IMPORT_private_FROM (_public);
    aiger_type *type;
    aiger_and *_and;

    if (lit < 2)
    { return lit; }

    res = code[lit];
    if (res)
    { return res; }

    var = aiger_lit2var (lit);
    assert (var <= _public->maxvar);
    type = _private->types + var;

    if (type->_and) {
        top = 0;
        stack = *stack_ptr;
        size_stack = *size_stack_ptr;
        PUSH (stack, top, size_stack, var);

        while (top > 0) {
            old = stack[--top];
            if (old) {
                if (code[aiger_var2lit (old)])
                { continue; }

                assert (old <= _public->maxvar);
                type = _private->types + old;
                if (type->onstack)
                { continue; }

                type->onstack = 1;

                PUSH (stack, top, size_stack, old);
                PUSH (stack, top, size_stack, 0);

                assert (type->_and);
                assert (type->idx < _public->num_ands);

                _and = _public->ands + type->idx;
                assert (_and);

                child0 = aiger_lit2var (_and->rhs0);
                child1 = aiger_lit2var (_and->rhs1);

                if (child0 < child1) {
                    tmp = child0;
                    child0 = child1;
                    child1 = tmp;
                }

                assert (child0 >= child1);    /* smaller child first */

                if (child0) {
                    type = _private->types + child0;
                    if (!type->input && !type->latch && !type->onstack)
                    { PUSH (stack, top, size_stack, child0); }
                }

                if (child1) {
                    type = _private->types + child1;
                    if (!type->input && !type->latch && !type->onstack)
                    { PUSH (stack, top, size_stack, child1); }
                }
            } else {
                assert (top > 0);
                old = stack[--top];
                assert (!code[aiger_var2lit (old)]);
                type = _private->types + old;
                assert (type->onstack);
                type->onstack = 0;
                aiger_new_code (old, _new, code);
            }
        }
        *size_stack_ptr = size_stack;
        *stack_ptr = stack;
    } else {
        assert (type->input || type->latch);
        assert (lit < *_new);

        code[lit] = lit;
        code[aiger_not (lit)] = aiger_not (lit);
    }

    assert (code[lit]);

    return code[lit];
}

static int
cmp_lhs (const void *a, const void *b)
{
    const aiger_and *c = a;
    const aiger_and *d = b;
    return ((int) c->lhs) - (int) d->lhs;
}

void
aiger_reencode (aiger * _public)
{
    unsigned *code, i, j, size_code, old, _new, lhs, rhs0, rhs1, tmp;
    unsigned *stack, size_stack;
    IMPORT_private_FROM (_public);

    assert (!aiger_error (_public));

    aiger_symbol *symbol;
    aiger_type *type;
    aiger_and *_and;

    if (aiger_is_reencoded (_public))
    { return; }

    size_code = 2 * (_public->maxvar + 1);
    if (size_code < 2)
    { size_code = 2; }

    NEWN (code, size_code);

    code[1] = 1;          /* not used actually */

    _new = 2;

    for (i = 0; i < _public->num_inputs; i++) {
        old = _public->inputs[i].lit;

        code[old] = _new;
        code[old + 1] = _new + 1;

        _new += 2;
    }

    for (i = 0; i < _public->num_latches; i++) {
        old = _public->latches[i].lit;

        code[old] = _new;
        code[old + 1] = _new + 1;

        _new += 2;
    }

    stack = 0;
    size_stack = 0;

    for (i = 0; i < _public->num_latches; i++) {
        old = _public->latches[i].next;
        _public->latches[i].next =
            aiger_reencode_lit (_public, old, &_new, code, &stack, &size_stack);

        old = _public->latches[i].reset;
        _public->latches[i].reset =
            aiger_reencode_lit (_public, old, &_new, code, &stack, &size_stack);
    }

    for (i = 0; i < _public->num_outputs; i++) {
        old = _public->outputs[i].lit;
        _public->outputs[i].lit =
            aiger_reencode_lit (_public, old, &_new, code, &stack, &size_stack);
    }

    for (i = 0; i < _public->num_bad; i++) {
        old = _public->bad[i].lit;
        _public->bad[i].lit =
            aiger_reencode_lit (_public, old, &_new, code, &stack, &size_stack);
    }

    for (i = 0; i < _public->num_constraints; i++) {
        old = _public->constraints[i].lit;
        _public->constraints[i].lit =
            aiger_reencode_lit (_public, old, &_new, code, &stack, &size_stack);
    }

    for (i = 0; i < _public->num_justice; i++) {
        for (j = 0; j < _public->justice[i].size; j++) {
            old = _public->justice[i].lits[j];
            _public->justice[i].lits[j] =
                aiger_reencode_lit (_public, old, &_new, code, &stack, &size_stack);
        }
    }

    for (i = 0; i < _public->num_fairness; i++) {
        old = _public->fairness[i].lit;
        _public->fairness[i].lit =
            aiger_reencode_lit (_public, old, &_new, code, &stack, &size_stack);
    }

    DELETEN (stack, size_stack);

    j = 0;
    for (i = 0; i < _public->num_ands; i++) {
        _and = _public->ands + i;
        lhs = code[_and->lhs];
        if (!lhs)
        { continue; }

        rhs0 = code[_and->rhs0];
        rhs1 = code[_and->rhs1];

        _and = _public->ands + j++;

        if (rhs0 < rhs1) {
            tmp = rhs1;
            rhs1 = rhs0;
            rhs0 = tmp;
        }

        assert (lhs > rhs0);
        assert (rhs0 >= rhs1);

        _and->lhs = lhs;
        _and->rhs0 = rhs0;
        _and->rhs1 = rhs1;
    }
    _public->num_ands = j;

    qsort (_public->ands, j, sizeof (*_and), cmp_lhs);

    assert (_new);
    assert (_public->maxvar >= aiger_lit2var (_new - 1));
    _public->maxvar = aiger_lit2var (_new - 1);

    /* Reset types.
     */
    for (i = 1; i <= _public->maxvar; i++) {
        type = _private->types + i;
        type->input = 0;
        type->latch = 0;
        type->_and = 0;
        type->idx = 0;
    }

    /* Fix types for ANDs.
     */
    for (i = 0; i < _public->num_ands; i++) {
        _and = _public->ands + i;
        type = _private->types + aiger_lit2var (_and->lhs);
        type->_and = 1;
        type->idx = i;
    }

    /* Fix types for inputs.
     */
    for (i = 0; i < _public->num_inputs; i++) {
        symbol = _public->inputs + i;
        assert (symbol->lit < size_code);
        symbol->lit = code[symbol->lit];
        type = _private->types + aiger_lit2var (symbol->lit);
        type->input = 1;
        type->idx = i;
    }

    /* Fix types for latches.
     */
    for (i = 0; i < _public->num_latches; i++) {
        symbol = _public->latches + i;
        symbol->lit = code[symbol->lit];
        type = _private->types + aiger_lit2var (symbol->lit);
        type->latch = 1;
        type->idx = i;
    }

    DELETEN (code, size_code);

    #ifndef NDEBUG
    for (i = 0; i <= _public->maxvar; i++) {
        type = _private->types + i;
        assert (!(type->input && type->latch));
        assert (!(type->input && type->_and));
        assert (!(type->latch && type->_and));
    }
    #endif
    assert (aiger_is_reencoded (_public));
    assert (!aiger_check (_public));
}

const unsigned char *
aiger_coi (aiger * _public)
{
    IMPORT_private_FROM (_public);
    _private->size_coi = _public->maxvar + 1;
    NEWN (_private->coi, _private->size_coi);
    memset (_private->coi, 1, _private->size_coi);
    return _private->coi;
}

static int
aiger_write_binary (aiger * _public, void *state, aiger_put put)
{
    aiger_and *_and;
    unsigned lhs, i;

    assert (!aiger_check (_public));

    aiger_reencode (_public);

    if (!aiger_write_header (_public, "aig", 1, state, put))
    { return 0; }

    lhs = aiger_max_input_or_latch (_public) + 2;

    for (i = 0; i < _public->num_ands; i++) {
        _and = _public->ands + i;

        assert (lhs == _and->lhs);
        assert (lhs > _and->rhs0);
        assert (_and->rhs0 >= _and->rhs1);

        if (!aiger_write_delta (state, put, lhs - _and->rhs0))
        { return 0; }

        if (!aiger_write_delta (state, put, _and->rhs0 - _and->rhs1))
        { return 0; }

        lhs += 2;
    }

    return 1;
}

unsigned
aiger_strip_symbols_and_comments (aiger * _public)
{
    IMPORT_private_FROM (_public);
    unsigned res;

    assert (!aiger_error (_public));

    res = aiger_delete_comments (_public);

    res += aiger_delete_symbols_aux (_private,
                                     _public->inputs,
                                     _private->size_inputs);
    res += aiger_delete_symbols_aux (_private,
                                     _public->latches,
                                     _private->size_latches);
    res += aiger_delete_symbols_aux (_private,
                                     _public->outputs,
                                     _private->size_outputs);
    res += aiger_delete_symbols_aux (_private,
                                     _public->bad,
                                     _private->size_bad);
    res += aiger_delete_symbols_aux (_private,
                                     _public->constraints,
                                     _private->size_constraints);
    res += aiger_delete_symbols_aux (_private,
                                     _public->justice,
                                     _private->size_justice);
    res += aiger_delete_symbols_aux (_private,
                                     _public->fairness,
                                     _private->size_fairness);
    return res;
}

int
aiger_write_generic (aiger * _public,
                     aiger_mode mode, void *state, aiger_put put)
{
    assert (!aiger_error (_public));

    if ((mode & aiger_ascii_mode)) {
        if (!aiger_write_ascii (_public, state, put))
        { return 0; }
    } else {
        if (!aiger_write_binary (_public, state, put))
        { return 0; }
    }

    if (!(mode & aiger_stripped_mode)) {
        if (aiger_have_at_least_one_symbol (_public)) {
            if (!aiger_write_symbols (_public, state, put))
            { return 0; }
        }

        if (_public->comments[0]) {
            if (aiger_put_s (state, put, "c\n") == EOF)
            { return 0; }

            if (!aiger_write_comments (_public, state, put))
            { return 0; }
        }
    }

    return 1;
}

int
aiger_write_to_file (aiger * _public, aiger_mode mode, FILE * file)
{
    assert (!aiger_error (_public));
    return aiger_write_generic (_public,
                                mode, file, (aiger_put) aiger_default_put);
}

int
aiger_write_to_string (aiger * _public, aiger_mode mode, char *str, size_t len)
{
    aiger_buffer buffer;
    int res;

    assert (!aiger_error (_public));

    buffer.start = str;
    buffer.cursor = str;
    buffer.end = str + len;
    res = aiger_write_generic (_public,
                               mode, &buffer, (aiger_put) aiger_string_put);

    if (!res)
    { return 0; }

    if (aiger_string_put (0, &buffer) == EOF)
    { return 0; }

    return 1;
}

static int
aiger_has_suffix (const char *str, const char *suffix)
{
    if (strlen (str) < strlen (suffix))
    { return 0; }

    return !strcmp (str + strlen (str) - strlen (suffix), suffix);
}

int
aiger_open_and_write_to_file (aiger * _public, const char *file_name)
{
    IMPORT_private_FROM (_public);
    int res, pclose_file;
    char *cmd, size_cmd;
    aiger_mode mode;
    FILE *file;

    assert (!aiger_error (_public));

    assert (file_name);

    if (aiger_has_suffix (file_name, ".gz")) {
        size_cmd = strlen (file_name) + strlen (GZIP);
        NEWN (cmd, size_cmd);
        sprintf (cmd, GZIP, file_name);
        file = popen (cmd, "w");
        DELETEN (cmd, size_cmd);
        pclose_file = 1;
    } else {
        file = fopen (file_name, "w");
        pclose_file = 0;
    }

    if (!file)
    { return 0; }

    if (aiger_has_suffix (file_name, ".aag") ||
            aiger_has_suffix (file_name, ".aag.gz"))
    { mode = aiger_ascii_mode; }
    else
    { mode = aiger_binary_mode; }

    res = aiger_write_to_file (_public, mode, file);

    if (pclose_file)
    { pclose (file); }
    else
    { fclose (file); }

    if (!res)
    { unlink (file_name); }

    return res;
}

static int
aiger_next_ch (aiger_reader * reader)
{
    int res;

    res = reader->get (reader->state);

    if (isspace (reader->ch) && !isspace (res))
    { reader->lineno_at_last_token_start = reader->lineno; }

    reader->ch = res;

    if (reader->done_with_reading_header && reader->looks_like_aag) {
        if (!isspace (res) && !isdigit (res) && res != EOF)
        { reader->looks_like_aag = 0; }
    }

    if (res == '\n')
    { reader->lineno++; }

    if (res != EOF)
    { reader->charno++; }

    return res;
}

/* Read a number assuming that the current character has already been
 * checked to be a digit, e.g. the start of the number to be read.
 */
static unsigned
aiger_read_number (aiger_reader * reader)
{
    unsigned res;

    assert (isdigit (reader->ch));
    res = reader->ch - '0';

    while (isdigit (aiger_next_ch (reader)))
    { res = 10 * res + (reader->ch - '0'); }

    return res;
}

/* Expect _and read an unsigned number followed by at least one white space
 * character.  The white space should either the space character or a _new
 * line as specified by the 'followed_by' parameter.  If a number can not be
 * found or there is no white space after the number, an apropriate error
 * message is returned.
 */
static const char *
aiger_read_literal (aiger_private * _private,
                    aiger_reader * reader,
                    unsigned *res_ptr,
                    char expected_followed_by,
                    char * followed_by_ptr)
{
    unsigned res;

    assert (expected_followed_by == ' ' ||
            expected_followed_by == '\n' ||
            !expected_followed_by);

    if (!isdigit (reader->ch))
        return aiger_error_u (_private,
                              "line %u: expected literal", reader->lineno);

    res = aiger_read_number (reader);

    if (expected_followed_by == ' ') {
        if (reader->ch != ' ')
            return aiger_error_uu (_private,
                                   "line %u: expected space after literal %u",
                                   reader->lineno_at_last_token_start, res);
    }
    if (expected_followed_by == '\n') {
        if (reader->ch != '\n')
            return aiger_error_uu (_private,
                                   "line %u: expected _new line after literal %u",
                                   reader->lineno_at_last_token_start, res);
    }
    if (!expected_followed_by) {
        if (reader->ch != '\n' && reader->ch != ' ')
            return aiger_error_uu (_private,
                                   "line %u: expected space or _new line after literal %u",
                                   reader->lineno_at_last_token_start, res);
    }

    if (followed_by_ptr)
    { *followed_by_ptr = reader->ch; }

    aiger_next_ch (reader);   /* skip white space */

    *res_ptr = res;

    return 0;
}

static const char *
aiger_already_defined (aiger * _public, aiger_reader * reader, unsigned lit)
{
    IMPORT_private_FROM (_public);
    aiger_type *type;
    unsigned var;

    assert (lit);
    assert (!aiger_sign (lit));

    var = aiger_lit2var (lit);

    if (_public->maxvar < var)
    { return 0; }

    type = _private->types + var;

    if (type->input)
        return aiger_error_uu (_private,
                               "line %u: literal %u already defined as input",
                               reader->lineno_at_last_token_start, lit);

    if (type->latch)
        return aiger_error_uu (_private,
                               "line %u: literal %u already defined as latch",
                               reader->lineno_at_last_token_start, lit);

    if (type->_and)
        return aiger_error_uu (_private,
                               "line %u: literal %u already defined as AND",
                               reader->lineno_at_last_token_start, lit);
    return 0;
}

static const char *
aiger_read_header (aiger * _public, aiger_reader * reader)
{
    IMPORT_private_FROM (_public);
    unsigned i, j, lit, next, reset;
    unsigned * sizes, * lits;
    const char *error;
    char ch;

    aiger_next_ch (reader);
    if (reader->ch != 'a')
        return aiger_error_u (_private,
                              "line %u: expected 'a' as first character",
                              reader->lineno);

    if (aiger_next_ch (reader) != 'i' && reader->ch != 'a')
        return aiger_error_u (_private,
                              "line %u: expected 'i' or 'a' after 'a'",
                              reader->lineno);

    if (reader->ch == 'a')
    { reader->mode = aiger_ascii_mode; }
    else
    { reader->mode = aiger_binary_mode; }

    if (aiger_next_ch (reader) != 'g')
        return aiger_error_u (_private,
                              "line %u: expected 'g' after 'a[ai]'",
                              reader->lineno);

    if (aiger_next_ch (reader) != ' ')
        return aiger_error_u (_private,
                              "line %u: expected ' ' after 'a[ai]g'",
                              reader->lineno);

    aiger_next_ch (reader);

    if (aiger_read_literal (_private, reader, &reader->maxvar, ' ', 0) ||
            aiger_read_literal (_private, reader, &reader->inputs, ' ', 0) ||
            aiger_read_literal (_private, reader, &reader->latches, ' ', 0) ||
            aiger_read_literal (_private, reader, &reader->outputs, ' ', 0) ||
            aiger_read_literal (_private, reader, &reader->_ands, 0, &ch) ||
            (ch == ' ' &&
             aiger_read_literal (_private, reader, &reader->bad, 0, &ch)) ||
            (ch == ' ' &&
             aiger_read_literal (_private, reader, &reader->constraints, 0, &ch)) ||
            (ch == ' ' &&
             aiger_read_literal (_private, reader, &reader->justice, 0, &ch)) ||
            (ch == ' ' &&
             aiger_read_literal (_private, reader, &reader->fairness, '\n', 0))) {
        assert (_private->error);
        return _private->error;
    }

    if (reader->mode == aiger_binary_mode) {
        i = reader->inputs;
        i += reader->latches;
        i += reader->_ands;

        if (i != reader->maxvar)
            return aiger_error_u (_private,
                                  "line %u: invalid maximal variable index",
                                  reader->lineno);
    }

    _public->maxvar = reader->maxvar;

    FIT (_private->types, _private->size_types, _public->maxvar + 1);
    FIT (_public->inputs, _private->size_inputs, reader->inputs);
    FIT (_public->latches, _private->size_latches, reader->latches);
    FIT (_public->outputs, _private->size_outputs, reader->outputs);
    FIT (_public->ands, _private->size_ands, reader->_ands);
    FIT (_public->bad, _private->size_bad, reader->bad);
    FIT (_public->constraints, _private->size_constraints, reader->constraints);
    FIT (_public->justice, _private->size_justice, reader->justice);
    FIT (_public->fairness, _private->size_fairness, reader->fairness);

    for (i = 0; i < reader->inputs; i++) {
        if (reader->mode == aiger_ascii_mode) {
            error = aiger_read_literal (_private, reader, &lit, '\n', 0);
            if (error)
            { return error; }

            if (!lit || aiger_sign (lit)
                    || aiger_lit2var (lit) > _public->maxvar)
                return aiger_error_uu (_private,
                                       "line %u: literal %u is not a valid input",
                                       reader->lineno_at_last_token_start, lit);

            error = aiger_already_defined (_public, reader, lit);
            if (error)
            { return error; }
        } else
        { lit = 2 * (i + 1); }

        aiger_add_input (_public, lit, 0);
    }

    for (i = 0; i < reader->latches; i++) {
        if (reader->mode == aiger_ascii_mode) {
            error = aiger_read_literal (_private, reader, &lit, ' ', 0);
            if (error)
            { return error; }

            if (!lit || aiger_sign (lit)
                    || aiger_lit2var (lit) > _public->maxvar)
                return aiger_error_uu (_private,
                                       "line %u: literal %u is not a valid latch",
                                       reader->lineno_at_last_token_start, lit);

            error = aiger_already_defined (_public, reader, lit);
            if (error)
            { return error; }
        } else
        { lit = 2 * (i + reader->inputs + 1); }

        error = aiger_read_literal (_private, reader, &next, 0, &ch);
        if (error)
        { return error; }

        if (aiger_lit2var (next) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: literal %u is not a valid literal",
                                   reader->lineno_at_last_token_start, next);

        aiger_add_latch (_public, lit, next, 0);

        if (ch == ' ') {
            error = aiger_read_literal (_private, reader, &reset, '\n', 0);
            if (error)
            { return error; }

            aiger_add_reset (_public, lit, reset);
        }
    }

    for (i = 0; i < reader->outputs; i++) {
        error = aiger_read_literal (_private, reader, &lit, '\n', 0);
        if (error)
        { return error; }

        if (aiger_lit2var (lit) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: literal %u is not a valid output",
                                   reader->lineno_at_last_token_start, lit);

        aiger_add_output (_public, lit, 0);
    }

    for (i = 0; i < reader->bad; i++) {
        error = aiger_read_literal (_private, reader, &lit, '\n', 0);
        if (error)
        { return error; }

        if (aiger_lit2var (lit) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: literal %u is not valid bad",
                                   reader->lineno_at_last_token_start, lit);

        aiger_add_bad (_public, lit, 0);
    }

    for (i = 0; i < reader->constraints; i++) {
        error = aiger_read_literal (_private, reader, &lit, '\n', 0);
        if (error)
        { return error; }

        if (aiger_lit2var (lit) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: literal %u is not a valid constraint",
                                   reader->lineno_at_last_token_start, lit);

        aiger_add_constraint (_public, lit, 0);
    }

    if (reader->justice) {
        NEWN (sizes, reader->justice);
        error =  0;
        for (i = 0; !error && i < reader->justice; i++)
        { error = aiger_read_literal (_private, reader, sizes + i, '\n', 0); }
        for (i = 0; !error && i < reader->justice; i++) {
            NEWN (lits, sizes[i]);
            for (j = 0; !error && j < sizes[i]; j++)
            { error = aiger_read_literal (_private, reader, lits + j, '\n', 0); }
            if (!error)
            { aiger_add_justice (_public, sizes[i], lits, 0); }
            DELETEN (lits, sizes[i]);
        }
        DELETEN (sizes, reader->justice);
        if (error)
        { return error; }
    }

    for (i = 0; i < reader->fairness; i++) {
        error = aiger_read_literal (_private, reader, &lit, '\n', 0);
        if (error)
        { return error; }

        if (aiger_lit2var (lit) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: literal %u is not valid fairness",
                                   reader->lineno_at_last_token_start, lit);

        aiger_add_fairness (_public, lit, 0);
    }

    reader->done_with_reading_header = 1;
    reader->looks_like_aag = 1;

    return 0;
}

static const char *
aiger_read_ascii (aiger * _public, aiger_reader * reader)
{
    IMPORT_private_FROM (_public);
    unsigned i, lhs, rhs0, rhs1;
    const char *error;

    for (i = 0; i < reader->_ands; i++) {
        error = aiger_read_literal (_private, reader, &lhs, ' ', 0);
        if (error)
        { return error; }

        if (!lhs || aiger_sign (lhs) || aiger_lit2var (lhs) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: "
                                   "literal %u is not a valid LHS of AND",
                                   reader->lineno_at_last_token_start, lhs);

        error = aiger_already_defined (_public, reader, lhs);
        if (error)
        { return error; }

        error = aiger_read_literal (_private, reader, &rhs0, ' ', 0);
        if (error)
        { return error; }

        if (aiger_lit2var (rhs0) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: literal %u is not a valid literal",
                                   reader->lineno_at_last_token_start, rhs0);

        error = aiger_read_literal (_private, reader, &rhs1, '\n', 0);
        if (error)
        { return error; }

        if (aiger_lit2var (rhs1) > _public->maxvar)
            return aiger_error_uu (_private,
                                   "line %u: literal %u is not a valid literal",
                                   reader->lineno_at_last_token_start, rhs1);

        aiger_add_and (_public, lhs, rhs0, rhs1);
    }

    return 0;
}

static const char *
aiger_read_delta (aiger_private * _private, aiger_reader * reader,
                  unsigned *res_ptr)
{
    unsigned res, i, charno;
    unsigned char ch;

    if (reader->ch == EOF)
    UNEXPECTED_EOF:
        return aiger_error_u (_private,
                              "character %u: unexpected end of file",
                              reader->charno);
    i = 0;
    res = 0;
    ch = reader->ch;

    charno = reader->charno;

    while ((ch & 0x80)) {
        assert (sizeof (unsigned) == 4);

        if (i == 5)
        INVALID_CODE:
            return aiger_error_u (_private, "character %u: invalid code", charno);

        res |= (ch & 0x7f) << (7 * i++);
        aiger_next_ch (reader);
        if (reader->ch == EOF)
        { goto UNEXPECTED_EOF; }

        ch = reader->ch;
    }

    if (i == 5 && ch >= 8)
    { goto INVALID_CODE; }

    res |= ch << (7 * i);
    *res_ptr = res;

    aiger_next_ch (reader);

    return 0;
}

static const char *
aiger_read_binary (aiger * _public, aiger_reader * reader)
{
    unsigned i, lhs, rhs0, rhs1, delta, charno;
    IMPORT_private_FROM (_public);
    const char *error;

    delta = 0;            /* avoid warning with -O3 */

    lhs = aiger_max_input_or_latch (_public);

    for (i = 0; i < reader->_ands; i++) {
        lhs += 2;
        charno = reader->charno;
        error = aiger_read_delta (_private, reader, &delta);
        if (error)
        { return error; }

        if (delta > lhs)      /* can at most be the same */
        INVALID_DELTA:
            return aiger_error_u (_private, "character %u: invalid delta", charno);

        rhs0 = lhs - delta;

        charno = reader->charno;
        error = aiger_read_delta (_private, reader, &delta);
        if (error)
        { return error; }

        if (delta > rhs0)     /* can well be the same ! */
        { goto INVALID_DELTA; }

        rhs1 = rhs0 - delta;

        aiger_add_and (_public, lhs, rhs0, rhs1);
    }

    return 0;
}

static void
aiger_reader_push_ch (aiger_private * _private, aiger_reader * reader, char ch)
{
    PUSH (reader->buffer, reader->top_buffer, reader->size_buffer, ch);
}

static const char *
aiger_read_comments (aiger * _public, aiger_reader * reader)
{
    IMPORT_private_FROM (_public);
    assert( reader->ch == '\n' );

    aiger_next_ch (reader);

    while (reader->ch != EOF) {
        while (reader->ch != '\n') {
            aiger_reader_push_ch (_private, reader, reader->ch);
            aiger_next_ch (reader);

            if (reader->ch == EOF)
                return aiger_error_u (_private,
                                      "line %u: _new line after comment missing",
                                      reader->lineno);
        }

        aiger_next_ch (reader);
        aiger_reader_push_ch (_private, reader, 0);
        aiger_add_comment (_public, reader->buffer);
        reader->top_buffer = 0;
    }

    return 0;
}

static const char *
aiger_read_symbols_and_comments (aiger * _public, aiger_reader * reader)
{
    IMPORT_private_FROM (_public);
    const char *error, *type;
    unsigned pos, num, count;
    aiger_symbol *symbol;

    assert (!reader->buffer);

    for (count = 0;; count++) {
        if (reader->ch == EOF)
        { return 0; }

        if (reader->ch != 'i' &&
                reader->ch != 'l' &&
                reader->ch != 'o' &&
                reader->ch != 'b' &&
                reader->ch != 'c' &&
                reader->ch != 'j' &&
                reader->ch != 'f') {
            if (reader->looks_like_aag)
                return aiger_error_u (_private,
                                      "line %u: corrupted symbol table "
                                      "('aig' instead of 'aag' header?)",
                                      reader->lineno);
            return aiger_error_u (_private,
                                  "line %u: expected '[cilobcjf]' or EOF",
                                  reader->lineno);
        }

        /* 'c' is a special case as it may be either the start of a comment,
        or the start of a constraint symbol */
        if (reader->ch == 'c') {
            if ( aiger_next_ch (reader) == '\n' )
            { return aiger_read_comments(_public, reader); }

            type = "constraint";
            num = _public->num_constraints;
            symbol = _public->constraints;
        } else {
            if (reader->ch == 'i') {
                type = "input";
                num = _public->num_inputs;
                symbol = _public->inputs;
            } else if (reader->ch == 'l') {
                type = "latch";
                num = _public->num_latches;
                symbol = _public->latches;
            } else if (reader->ch == 'o') {
                type = "output";
                num = _public->num_outputs;
                symbol = _public->outputs;
            } else if (reader->ch == 'b') {
                type = "bad";
                num = _public->num_bad;
                symbol = _public->bad;
            } else if (reader->ch == 'j') {
                type = "justice";
                num = _public->num_justice;
                symbol = _public->justice;
            } else {
                assert (reader->ch == 'f');
                type = "fairness";
                num = _public->num_fairness;
                symbol = _public->fairness;
            }

            aiger_next_ch (reader);
        }

        error = aiger_read_literal (_private, reader, &pos, ' ', 0);
        if (error)
        { return error; }

        if (pos >= num)
            return aiger_error_usu (_private,
                                    "line %u: "
                                    "%s symbol table entry position %u too large",
                                    reader->lineno_at_last_token_start, type,
                                    pos);

        symbol += pos;

        if (symbol->name)
            return aiger_error_usu (_private,
                                    "line %u: %s %u has multiple symbols",
                                    reader->lineno_at_last_token_start, type,
                                    symbol->lit);

        while (reader->ch != '\n' && reader->ch != EOF) {
            aiger_reader_push_ch (_private, reader, reader->ch);
            aiger_next_ch (reader);
        }

        if (reader->ch == EOF)
            return aiger_error_u (_private,
                                  "line %u: _new line missing", reader->lineno);

        assert (reader->ch == '\n');
        aiger_next_ch (reader);

        aiger_reader_push_ch (_private, reader, 0);
        symbol->name = aiger_copy_str (_private, reader->buffer);
        reader->top_buffer = 0;
    }
}

const char *
aiger_read_generic (aiger * _public, void *state, aiger_get get)
{
    IMPORT_private_FROM (_public);
    aiger_reader reader;
    const char *error;

    assert (!aiger_error (_public));

    CLR (reader);

    reader.lineno = 1;
    reader.state = state;
    reader.get = get;
    reader.ch = ' ';

    error = aiger_read_header (_public, &reader);
    if (error)
    { return error; }

    if (reader.mode == aiger_ascii_mode)
    { error = aiger_read_ascii (_public, &reader); }
    else
    { error = aiger_read_binary (_public, &reader); }

    if (error)
    { return error; }

    error = aiger_read_symbols_and_comments (_public, &reader);

    DELETEN (reader.buffer, reader.size_buffer);

    if (error)
    { return error; }

    return aiger_check (_public);
}

const char *
aiger_read_from_file (aiger * _public, FILE * file)
{
    assert (!aiger_error (_public));
    return aiger_read_generic (_public, file, (aiger_get) aiger_default_get);
}

const char *
aiger_open_and_read_from_file (aiger * _public, const char *file_name)
{
    IMPORT_private_FROM (_public);
    char *cmd, size_cmd;
    const char *res;
    int pclose_file;
    FILE *file;

    assert (!aiger_error (_public));

    if (aiger_has_suffix (file_name, ".gz")) {
        size_cmd = strlen (file_name) + strlen (GUNZIP);
        NEWN (cmd, size_cmd);
        sprintf (cmd, GUNZIP, file_name);
        file = popen (cmd, "r");
        DELETEN (cmd, size_cmd);
        pclose_file = 1;
    } else {
        file = fopen (file_name, "rb");
        pclose_file = 0;
    }

    if (!file)
    { return aiger_error_s (_private, "can not read '%s'", file_name); }

    res = aiger_read_from_file (_public, file);

    if (pclose_file)
    { pclose (file); }
    else
    { fclose (file); }

    return res;
}

const char *
aiger_get_symbol (aiger * _public, unsigned lit)
{
    IMPORT_private_FROM (_public);
    aiger_symbol *symbol;
    aiger_type *type;
    unsigned var;

    assert (!aiger_error (_public));

    assert (lit);
    assert (!aiger_sign (lit));

    var = aiger_lit2var (lit);
    assert (var <= _public->maxvar);
    type = _private->types + var;

    if (type->input)
    { symbol = _public->inputs; }
    else if (type->latch)
    { symbol = _public->latches; }
    else
    { return 0; }

    return symbol[type->idx].name;
}

static aiger_type *
aiger_lit2type (aiger * _public, unsigned lit)
{
    IMPORT_private_FROM (_public);
    aiger_type *type;
    unsigned var;

    assert (!aiger_sign (lit));
    var = aiger_lit2var (lit);
    assert (var <= _public->maxvar);
    type = _private->types + var;

    return type;
}

int
aiger_lit2tag (aiger * _public, unsigned lit)
{
    aiger_type * type;
    lit = aiger_strip (lit);
    if (!lit) { return 0; }
    type = aiger_lit2type (_public, lit);
    if (type->input) { return 1; }
    if (type->latch) { return 2; }
    return 3;
}

aiger_symbol *
aiger_is_input (aiger * _public, unsigned lit)
{
    aiger_type *type;
    aiger_symbol *res;

    assert (!aiger_error (_public));
    type = aiger_lit2type (_public, lit);
    if (!type->input)
    { return 0; }

    res = _public->inputs + type->idx;

    return res;
}

aiger_symbol *
aiger_is_latch (aiger * _public, unsigned lit)
{
    aiger_symbol *res;
    aiger_type *type;

    assert (!aiger_error (_public));
    type = aiger_lit2type (_public, lit);
    if (!type->latch)
    { return 0; }

    res = _public->latches + type->idx;

    return res;
}

aiger_and *
aiger_is_and (aiger * _public, unsigned lit)
{
    aiger_type *type;
    aiger_and *res;

    assert (!aiger_error (_public));
    type = aiger_lit2type (_public, lit);
    if (!type->_and)
    { return 0; }

    res = _public->ands + type->idx;

    return res;
}
