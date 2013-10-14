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

#define var_types_GLOBAL
#include "main.h"
#include "snl.h"
#include "gen_code.h"   /* implicit parameter names */
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
            case T_FUNCTION:
                if (basetype.tag == T_NONE) {
                    error_at_expr(d, "cannot declare function returning foreign variable\n");
                }
                if (basetype.tag == T_EVFLAG) {
                    error_at_expr(d, "cannot declare function returning event flag\n");
                }
                t->parent->val.function.return_type = t;
                break;
            case T_CONST:
                switch (t->tag) {
                case T_NONE:
                    error_at_expr(d, "cannot declare constant foreign entity\n");
                    break;
                case T_EVFLAG:
                    error_at_expr(d, "cannot declare constant event flag\n");
                    break;
                case T_VOID:
                    error_at_expr(d, "cannot declare constant void\n");
                    break;
                case T_ARRAY:
                    error_at_expr(d, "cannot declare constant array\n");
                    break;
                case T_FUNCTION:
                    warning_at_expr(d, "declaring constant function is redundant\n");
                    break;
                case T_CONST:
                    warning_at_expr(d, "declaring constant constant is redundant\n");
                    break;
                case T_PRIM:
                case T_FOREIGN:
                case T_POINTER:
                case T_STRUCT:
                    break;
                }
                t->parent->val.constant.value_type = t;
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

Expr *abs_decl_create(void)
{
    Token t = {0,0,0,0};
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

static Expr *add_implicit_parameters(Expr *fun_decl, Expr *param_decls)
{
    Expr *p1, *p2;
    Token t;

    /* "SS_ID " NM_SS */
    t.type = TOK_NAME;
    t.str = NM_SS;
    t.line = fun_decl->line_num;
    t.file = fun_decl->src_file;
    p1 = decl_add_base_type(
        decl_create(t),
        mk_foreign_type(F_TYPENAME, "SS_ID")
    );
    /* "SEQ_VARS *const " NM_VAR */
    t.type = TOK_NAME;
    t.str = NM_VAR;
    t.line = fun_decl->line_num;
    t.file = fun_decl->src_file;
    p2 = decl_add_base_type(
        decl_prefix_pointer(decl_prefix_const(decl_create(t))),
        mk_foreign_type(F_TYPENAME, "SEQ_VARS")
    );
    return link_expr(p1, link_expr(p2, param_decls));
}

static Expr *remove_void_parameter(Expr *param_decls)
{
    if (param_decls && param_decls->extra.e_decl->type->tag == T_VOID) {
        /* no other params should be there */
        if (param_decls->next) {
            error_at_expr(param_decls->next, "void must be the only parameter\n");
        }
        if (param_decls->extra.e_decl->name) {
            error_at_expr(param_decls, "void parameter should not have a name\n");
        }
        /* void means empty parameter list */
        return 0;
    } else {
        return param_decls;
    }
}

Expr *decl_postfix_function(Expr *decl, Expr *param_decls)
{
    Type *t = new(Type);

    assert(decl->type == D_DECL);          /* pre-condition */

#ifdef DEBUG
    report("decl_postfix_function\n");
#endif

    t->tag = T_FUNCTION;
    t->val.function.param_decls = add_implicit_parameters(decl,
        remove_void_parameter(param_decls)
    );
    t->parent = decl->extra.e_decl->type;
    decl->extra.e_decl->type = t;
    return decl;
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

Expr *decl_prefix_const(Expr *d)
{
    Type *t = new(Type);

#ifdef DEBUG
    report("decl_prefix_const\n");
#endif
    assert(d->type == D_DECL);          /* pre-condition */
    t->tag = T_CONST;
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
    case T_FUNCTION:
    case T_EVFLAG:
    case T_VOID:
    case T_CONST:
    case T_STRUCT:  /* for now, at least */
        return FALSE;
    case T_ARRAY:
        return type_assignable_array(t->val.array.elem_type, depth + 1);
    case T_PRIM:
        return TRUE;
    }
    /* avoid bogus compiler warning: */
    assert(impossible);
    return FALSE;
}

unsigned type_assignable(Type *t)
{
    return type_assignable_array(t, 0);
}

enum assoc {
    L,
    R,
};

static void gen_pre(Type *t, enum assoc prev_assoc, int letter)
{
    const char *sep = letter ? " " : "";
    switch (t->tag) {
    case T_NONE:
        assert(impossible);
        break;
    case T_POINTER:
        gen_pre(t->val.pointer.value_type, L, TRUE);
        gen_code("*");
        break;
    case T_CONST:
        gen_pre(t->val.constant.value_type, L, TRUE);
        gen_code("const%s", sep);
        break;
    case T_ARRAY:
        gen_pre(t->val.array.elem_type, R, letter || prev_assoc == L);
        if (prev_assoc == L)
            gen_code("(");
        break;
    case T_FUNCTION:
        gen_pre(t->val.function.return_type, R, letter || prev_assoc == L);
        if (prev_assoc == L)
            gen_code("(");
        break;
    case T_EVFLAG:
        gen_code("evflag%s", sep);
        break;
    case T_VOID:
        gen_code("void%s", sep);
        break;
    case T_PRIM:
        gen_code("%s%s", prim_type_name[t->val.prim], sep);
        break;
    case T_FOREIGN:
        gen_code("%s%s%s", foreign_type_prefix[t->val.foreign.tag], t->val.foreign.name, sep);
        break;
    case T_STRUCT:
        gen_code("struct %s%s", t->val.structure.name, sep);
        break;
    }
}

static void gen_post(Type *t, enum assoc prev_assoc)
{
    Expr *pd;

    switch (t->tag) {
    case T_POINTER:
        gen_post(t->val.pointer.value_type, L);
        break;
    case T_CONST:
        gen_post(t->val.constant.value_type, L);
        break;
    case T_ARRAY:
        if (prev_assoc == L)
            gen_code(")");
        gen_code("[%d]", t->val.array.num_elems);
        gen_post(t->val.array.elem_type, R);
        break;
    case T_FUNCTION:
        if (prev_assoc == L)
            gen_code(")");
        gen_code("(");
        foreach (pd, t->val.function.param_decls) {
            Var *var = pd->extra.e_decl;
            gen_type(var->type, "", var->name);
            if (pd->next)
                gen_code (", ");
        }
        gen_code(")");
        gen_post(t->val.function.return_type, R);
        break;
    default:
        break;
    }
}

void gen_type(Type *t, const char *prefix, const char *name)
{
    gen_pre(t, R, name != NULL);
    if (name)
        gen_code("%s%s", prefix, name);
    gen_post(t, R);
}

static void ind(int level)
{
    while (level--)
        report("  ");
}

void dump_type(Type *t, int l)
{
    ind(l); report("dump_type(): tag=%s\n", type_tag_names[t->tag]);
    switch (t->tag) {
    case T_NONE:
    case T_EVFLAG:
    case T_VOID:
    case T_PRIM:
        break;
    case T_FOREIGN:
        ind(l+1); report("foreign.tag=%s, name=%s\n",
            foreign_type_prefix[t->val.foreign.tag], t->val.foreign.name);
        break;
    case T_POINTER:
        dump_type(t->val.pointer.value_type, l+1);
        break;
    case T_ARRAY:
        ind(l+1); report("array.num_elems=%d", t->val.array.num_elems);
        dump_type(t->val.array.elem_type, l+1);
        break;
    case T_FUNCTION:
        dump_type(t->val.function.return_type, l+1);
        break;
    case T_CONST:
        dump_type(t->val.constant.value_type, l+1);
        break;
    case T_STRUCT:
        ind(l+1); report("struct.name=%s\n", t->val.structure.name);
        break;
    }
}
