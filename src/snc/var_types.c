/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#define var_types_GLOBAL
#include "expr.h"
#undef var_types_GLOBAL

/* #define DEBUG */

static const int impossible = FALSE;

/* Add a common base type to all types in a multi-variable declaration */
Expr *decl_add_base_type(Expr *ds, Type basetype)
{
    Expr *d;

    foreach(d, ds) {
        Var *var;
        Type *t = new(Type), *bt = t;

        assert(d->type == D_DECL);      /* pre-condition */
        var = d->extra.e_decl;
        assert(var);
        /* structure copy */
        *t = basetype;
        t->parent = var->type;
        /* now roll back the stack of type expressions */
        while(t->parent) {
            switch (t->parent->tag) {
            case T_ARRAY:
                if (basetype.tag == T_NONE) {
                    error_at_expr(d, "cannot declare array of foreign variables\n");
                }
                if (basetype.tag == T_EVFLAG) {
                    error_at_expr(d, "cannot declare array of event flags\n");
                }
                t->parent->val.array.elem_type = t;
                break;
            case T_POINTER:
                if (basetype.tag == T_NONE) {
                    error_at_expr(d, "cannot declare pointer to foreign variable\n");
                }
                if (basetype.tag == T_EVFLAG) {
                    error_at_expr(d, "cannot declare pointer to event flag\n");
                }
                t->parent->val.pointer.value_type = t;
                break;
            default:
                assert(impossible);
            }
            t = t->parent;
        }
        assert(!t->parent);
        t->parent = bt;
        var->type = t;
        if (basetype.tag == T_EVFLAG)
            var->chan.evflag = new(EvFlag);
    }
    return ds;
}

Expr *decl_add_init(Expr *d, Expr *init)
{
    assert(d->type == D_DECL);          /* pre-condition */
#ifdef DEBUG
    report("decl_add_init: var=%s, init=%p\n", d->extra.e_decl->name, init);
#endif
    d->extra.e_decl->init = init;
    d->decl_init = init;
    return d;
}

Expr *decl_create(Token id)
{
    Expr *d = expr(D_DECL, id, 0);
    Var *var = new(Var);

#ifdef DEBUG
    report("decl_create: name(%s)\n", id.str);
#endif
    assert(d->type == D_DECL);          /* expr() post-condition */
    var->name = id.str;
    d->extra.e_decl = var;
    var->decl = d;
    return d;
}

Expr *abstract_decl_create(void)
{
    Token t = {0,0,0};
    return decl_create(t);
}

Expr *decl_postfix_array(Expr *d, char *s)
{
    Type *t = new(Type);
    uint num_elems;

    assert(d->type == D_DECL);          /* pre-condition */
    if (!strtoui(s, UINT_MAX, &num_elems) || num_elems == 0) {
        error_at_expr(d, "invalid array size (must be >= 1)\n");
        num_elems = 1;
    }

#ifdef DEBUG
    report("decl_postfix_array %u\n", num_elems);
#endif

    t->tag = T_ARRAY;
    t->val.array.num_elems = num_elems;
    t->parent = d->extra.e_decl->type;
    d->extra.e_decl->type = t;
    return d;
}

Expr *decl_prefix_pointer(Expr *d)
{
    Type *t = new(Type);

#ifdef DEBUG
    report("decl_prefix_pointer\n");
#endif
    assert(d->type == D_DECL);          /* pre-condition */
    t->tag = T_POINTER;
    t->parent = d->extra.e_decl->type;
    d->extra.e_decl->type = t;
    return d;
}


Type mk_prim_type(enum prim_type_tag tag)
{
    Type t;

    memset(&t, 0, sizeof(Type));

    t.tag = T_PRIM;
    t.val.prim = tag;
    return t;
}

Type mk_foreign_type(enum foreign_type_tag tag, char *name)
{
    Type t;

    memset(&t, 0, sizeof(Type));

    t.tag = T_FOREIGN;
    t.val.foreign.tag = tag;
    t.val.foreign.name = name;
    return t;
}

Type mk_ef_type()
{
    Type t;

    memset(&t, 0, sizeof(Type));

    t.tag = T_EVFLAG;
    return t;
}

Type mk_void_type()
{
    Type t;

    memset(&t, 0, sizeof(Type));

    t.tag = T_VOID;
    return t;
}

Type mk_no_type()
{
    Type t;

    memset(&t, 0, sizeof(Type));

    t.tag = T_NONE;
    return t;
}

unsigned type_array_length1(Type *t)
{
    switch (t->tag) {
    case T_ARRAY:
        return t->val.array.num_elems;
    default:
        return 1;
    }
}

unsigned type_array_length2(Type *t)
{
    switch (t->tag) {
    case T_ARRAY:
        return type_array_length1(t->val.array.elem_type);
    default:
        return 1;
    }
}

static unsigned type_assignable_array(Type *t, int depth)
{
    if (depth > 2)
        return FALSE;
    switch (t->tag) {
    case T_NONE:
    case T_FOREIGN:
    case T_POINTER:
    case T_EVFLAG:
        return FALSE;
    case T_ARRAY:
        return type_assignable_array(t->val.array.elem_type, depth + 1);
    case T_PRIM:
    /* make the compiler happy: */
    default:
        return TRUE;
    }
}

unsigned type_assignable(Type *t)
{
    return type_assignable_array(t, 0);
}

static void gen_array_pointer(Type *t, enum type_tag last_tag, const char *prefix, const char *name)
{
    int paren = last_tag == T_ARRAY;
    switch (t->tag) {
    case T_POINTER:
        if (paren)
            gen_code("(");
        gen_code("*");
        gen_array_pointer(t->parent, t->tag, prefix, name);
        if (paren)
            gen_code(")");
        break;
    case T_ARRAY:
        gen_array_pointer(t->parent, t->tag, prefix, name);
        gen_code("[%d]", t->val.array.num_elems);
        break;
    default:
        if (name)
            gen_code(" %s%s", prefix, name);
        break;
    }
}

void gen_type(Type *t, const char *prefix, const char *name)
{
    Type *bt = base_type(t);

    switch (bt->tag) {
    case T_EVFLAG:
        gen_code("evflag");
        break;
    case T_VOID:
        gen_code("void");
        break;
    case T_PRIM:
        gen_code("%s", prim_type_name[bt->val.prim]);
        break;
    case T_FOREIGN:
        gen_code("%s%s", foreign_type_prefix[bt->val.foreign.tag], bt->val.foreign.name);
        break;
    default:
        assert(impossible);
    }
    gen_array_pointer(bt->parent, T_NONE, prefix, name);
}
