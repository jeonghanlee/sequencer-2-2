/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include "builtin.h"

static struct const_symbol const_symbols[] =
{
    {"TRUE",                CT_BOOL},
    {"FALSE",               CT_BOOL},
    {"SYNC",                CT_SYNCFLAG},
    {"ASYNC",               CT_SYNCFLAG},
    {"NOEVFLAG",            CT_EVFLAG},
    {"pvStatOK",            CT_PVSTAT},
    {"pvStatERROR",         CT_PVSTAT},
    {"pvStatDISCONN",       CT_PVSTAT},
    {"pvStatREAD",          CT_PVSTAT},
    {"pvStatWRITE",         CT_PVSTAT},
    {"pvStatHIHI",          CT_PVSTAT},
    {"pvStatHIGH",          CT_PVSTAT},
    {"pvStatLOLO",          CT_PVSTAT},
    {"pvStatLOW",           CT_PVSTAT},
    {"pvStatSTATE",         CT_PVSTAT},
    {"pvStatCOS",           CT_PVSTAT},
    {"pvStatCOMM",          CT_PVSTAT},
    {"pvStatTIMEOUT",       CT_PVSTAT},
    {"pvStatHW_LIMIT",      CT_PVSTAT},
    {"pvStatCALC",          CT_PVSTAT},
    {"pvStatSCAN",          CT_PVSTAT},
    {"pvStatLINK",          CT_PVSTAT},
    {"pvStatSOFT",          CT_PVSTAT},
    {"pvStatBAD_SUB",       CT_PVSTAT},
    {"pvStatUDF",           CT_PVSTAT},
    {"pvStatDISABLE",       CT_PVSTAT},
    {"pvStatSIMM",          CT_PVSTAT},
    {"pvStatREAD_ACCESS",   CT_PVSTAT},
    {"pvStatWRITE_ACCESS",  CT_PVSTAT},
    {"pvSevrOK",            CT_PVSEVR},
    {"pvSevrERROR",         CT_PVSEVR},
    {"pvSevrNONE",          CT_PVSEVR},
    {"pvSevrMINOR",         CT_PVSEVR},
    {"pvSevrMAJOR",         CT_PVSEVR},
    {"pvSevrINVALID",       CT_PVSEVR},
    {0,                     CT_NONE}
};

const char *pvGetPutArgs[] = {
    "DEFAULT",
    "DEFAULT_TIMEOUT"
};

const char *pvGetPutCompleteArgs[] = {
    "FALSE",
    "NULL"
};

static struct func_symbol func_symbols[] =
{
    /* name                             type           add_length    ef_action_only   cond_only          */
    /*  |                                |        multi_pv  | default_args |  ef_args   | default_values */
    /*  |                                |            |     |       |      |    |       |       |        */
    {"delay",           0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  TRUE ,  0},
    {"efClear",         0,              FT_EVENT,   FALSE,  FALSE,  0,  TRUE,   FALSE,  FALSE,  0},
    {"efSet",           0,              FT_EVENT,   FALSE,  FALSE,  0,  TRUE,   FALSE,  FALSE,  0},
    {"efTest",          0,              FT_EVENT,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"efTestAndClear",  0,              FT_EVENT,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"macValueGet",     0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"optGet",          0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvAssign",        0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvAssignCount",   0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvAssigned",      0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvChannelCount",  0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvConnectCount",  0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvConnected",     0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvCount",         0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvFlush",         0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvFlushQ",        0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvFreeQ",         0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvGet",           "pvGetTmo",     FT_PV,      FALSE,  FALSE,  2,  FALSE,  FALSE,  FALSE,  pvGetPutArgs},
    {"pvGetCancel",     0,              FT_PV,      TRUE,   TRUE,   0,  FALSE,  FALSE,  FALSE,  0},
    {"pvGetComplete",   0,              FT_PV,      FALSE,  TRUE,   2,  FALSE,  FALSE,  FALSE,  pvGetPutCompleteArgs},
    {"pvGetQ",          0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvIndex",         0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvMessage",       0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvMonitor",       0,              FT_PV,      FALSE,  TRUE,   0,  FALSE,  FALSE,  FALSE,  0},
    {"pvName",          0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvPut",           "pvPutTmo",     FT_PV,      FALSE,  FALSE,  2,  FALSE,  FALSE,  FALSE,  pvGetPutArgs},
    {"pvPutCancel",     0,              FT_PV,      TRUE,   TRUE,   0,  FALSE,  FALSE,  FALSE,  0},
    {"pvPutComplete",   0,              FT_PV,      TRUE,   TRUE,   2,  FALSE,  FALSE,  FALSE,  pvGetPutCompleteArgs},
    {"pvSeverity",      0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvStatus",        0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pvStopMonitor",   0,              FT_PV,      FALSE,  TRUE,   0,  FALSE,  FALSE,  FALSE,  0},
    {"pvSync",          0,              FT_PV,      FALSE,  TRUE,   0,  FALSE,  TRUE ,  FALSE,  0},
    {"pvTimeStamp",     0,              FT_PV,      FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"seqLog",          0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"pVar",            0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {"ssId",            0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0},
    {0,                 0,              FT_OTHER,   FALSE,  FALSE,  0,  FALSE,  FALSE,  FALSE,  0}
};

/* Insert builtin constants into symbol table */
void register_builtin_consts(SymTable sym_table)
{
    struct const_symbol *sym;

    for (sym = const_symbols; sym->name; sym++) {
        /* use address of const_symbols array as the symbol type */
        sym_table_insert(sym_table, sym->name, const_symbols, sym);
    }
}

/* Insert builtin functions into symbol table */
void register_builtin_funcs(SymTable sym_table)
{
    struct func_symbol *sym;

    for (sym = func_symbols; sym->name; sym++) {
        /* use address of func_symbols array as the symbol type */
        sym_table_insert(sym_table, sym->name, func_symbols, sym);
    }
}

/* Look up a builtin function from the symbol table */
struct func_symbol *lookup_builtin_func(SymTable sym_table, const char *func_name)
{
    /* use address of func_symbols array as the symbol type */
    return (struct func_symbol *)sym_table_lookup(sym_table, func_name, func_symbols);
}

/* Look up a builtin constant from the symbol table */
struct const_symbol *lookup_builtin_const(SymTable sym_table, const char *const_name)
{
    /* use address of const_symbols array as the symbol type */
    return (struct const_symbol *)sym_table_lookup(sym_table, const_name, const_symbols);
}
