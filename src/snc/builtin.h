/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INCLbuiltinh
#define INCLbuiltinh

#include "sym_table.h"

struct const_symbol
{
    const char  *name;
    int         type;
};

enum const_type
{
    CT_NONE,
    CT_BOOL,
    CT_SYNCFLAG,
    CT_EVFLAG,
    CT_PVSTAT,
    CT_PVSEVR
};

enum func_type
{
    FT_EVENT,
    FT_PV,
    FT_OTHER
};

struct func_symbol
{
    const char  *name;
    uint        type            :2; /* see enum func_type */
    uint        multi_pv        :1; /* whether multi-pv args are supported */
    uint        add_length      :1; /* need to pass array size */
    uint        default_args    :2; /* number of optional parameters */
    uint        ef_action_only  :1; /* not allowed in when-conditions */
    uint        ef_args         :1; /* extra parameter must be an event flag */
    uint        cond_only       :1; /* only allowed in when-conditions */
    const char  **default_values;   /* defaults for optional parameters */
};

/* Insert builtin constants into symbol table */
void register_builtin_consts(SymTable sym_table);

/* Insert builtin functions into symbol table */
void register_builtin_funcs(SymTable sym_table);

/* Look up a builtin function from the symbol table */
struct func_symbol *lookup_builtin_func(SymTable sym_table, const char *func_name);

/* Look up a builtin constant from the symbol table */
struct const_symbol *lookup_builtin_const(SymTable sym_table, const char *const_name);

#endif /*INCLbuiltinh */
