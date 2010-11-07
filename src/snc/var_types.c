/* Copyright 2010 Helmholtz-Zentrum Berlin f. Materialien und Energie GmbH
   (see file Copyright.HZB included in this distribution)
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "snc_main.h"
#include "parse.h"

/* #define DEBUG */

static void retrofit_base_type(unsigned tag, Type *t, Expr *d)
{
    assert(tag < V_POINTER);        /* pre-condition */
    assert(t != NULL);
    assert(t->tag >= V_POINTER);    /* pre-condition */
    switch (t->tag) {
    case V_ARRAY:
        if (tag == V_NONE) {
            error_at_expr(d, "cannot declare array of foreign variables\n");
            break;
        }
        if (tag == V_EVFLAG) {
            error_at_expr(d, "cannot declare array of event flags\n");
            break;
        }
        if (t->val.array.elem_type == NULL) {
            t->val.array.elem_type  = new(Type);
            t->val.array.elem_type->tag = tag;
        } else {
            retrofit_base_type(tag, t->val.array.elem_type, d);
        }
        break;
    case V_POINTER:
        if (tag == V_NONE) {
            error_at_expr(d, "cannot declare pointer to foreign variable\n");
            break;
        }
        if (tag == V_EVFLAG) {
            error_at_expr(d, "cannot declare pointer to event flag\n");
            break;
        }
        if (t->val.pointer.value_type == NULL) {
            t->val.pointer.value_type  = new(Type);
            t->val.pointer.value_type->tag = tag;
        } else {
            retrofit_base_type(tag, t->val.pointer.value_type, d);
        }
        break;
    default:
        break;                      /* dummy to pacify compiler */
    }
}

Expr *decl_add_base_type(Expr *ds, int tag)
{
    Expr *d;

    foreach(d, ds) {
        Var *var = d->extra.e_decl;

        assert(d->type == D_DECL);      /* pre-condition */
        if (var->type == NULL) {
            var->type = new(Type);
            var->type->tag = tag;
        } else {
            retrofit_base_type(tag, var->type, d);
        }
        if (tag == V_EVFLAG)
            var->chan.evflag = new(EvFlag);
#ifdef DEBUG
        fprintf(stderr, "base_type(%d) for name = %s\n", tag, var->name);
#endif
    }
    return ds;
}

Expr *decl_add_init(Expr *d, Expr *init)
{
    assert(d->type == D_DECL);          /* pre-condition */
    d->extra.e_decl->value = init;
    return d;
}

Expr *decl_create(Token name)
{
    Expr *d = expr(D_DECL, name, 0);
    Var *var = new(Var);

#ifdef DEBUG
    fprintf(stderr, "name(%s)\n", name.str);
#endif
    assert(d->type == D_DECL);          /* expr() post-condition */
    var->name = name.str;
    d->extra.e_decl = var;
    var->decl = d;
    return d;
}

Expr *decl_postfix_array(Expr *d, char *s)
{
    int l = atoi(s);
    Type *t = new(Type);

#ifdef DEBUG
    fprintf(stderr, "array\n");
#endif
    assert(d->type == D_DECL);          /* pre-condition */
    if (l <= 0) {
        error_at_expr(d, "invalid array size (must be >= 1)\n");
        l = 1;
    }
    t->tag = V_ARRAY;
    t->val.array.num_elems = l;
    t->val.array.elem_type = d->extra.e_decl->type;
    d->extra.e_decl->type = t;
    return d;
}

Expr *decl_prefix_pointer(Expr *d)
{
    Type *t = new(Type);

#ifdef DEBUG
    fprintf(stderr, "pointer\n");
#endif
    assert(d->type == D_DECL);          /* pre-condition */
    t->tag = V_POINTER;
    t->val.pointer.value_type = d->extra.e_decl->type;
    d->extra.e_decl->type = t;
    return d;
}

unsigned type_base_type(Type *t)
{
    switch (t->tag) {
    case V_ARRAY:
        return type_base_type(t->val.array.elem_type);
    case V_POINTER:
        return type_base_type(t->val.pointer.value_type);
    default:
        return t->tag;
    }
}

unsigned type_array_length1(Type *t)
{
    switch (t->tag) {
    case V_ARRAY:
        return t->val.array.num_elems;
    default:
        return 1;
    }
}

unsigned type_array_length2(Type *t)
{
    switch (t->tag) {
    case V_ARRAY:
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
    case V_NONE:
    case V_EVFLAG:
    case V_POINTER:
        return FALSE;
    case V_ARRAY:
        return type_assignable_array(t->val.array.elem_type, depth + 1);
    default:
        return TRUE;
    }
}

unsigned type_assignable(Type *t)
{
    return type_assignable_array(t, 0);
}
