/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INCLvar_typesh
#define INCLvar_typesh

#include "seq_prim_types.h"

enum type_tag {
    T_NONE,     /* undeclared (or declared as foreign) variable */
    T_EVFLAG,   /* event flags */
    T_PRIM,     /* primitive types: numbers, char, string */
    T_FOREIGN,  /* foreign types (declared in C code) */
    T_POINTER,
    T_ARRAY,
    T_PV,
};

enum foreign_type_tag {
    F_ENUM,
    F_STRUCT,
    F_UNION,
    F_TYPENAME,
};

struct array_type {
    unsigned    num_elems;
    struct type *elem_type;
};

struct pointer_type {
    struct type *value_type;
};

struct foreign_type {
    enum foreign_type_tag tag;
    char *name;
};

typedef struct type Type;

struct type {
    enum type_tag tag;
    union {
        enum prim_type_tag  prim;
        struct foreign_type foreign;
        struct pointer_type pointer;
        struct array_type   array;
    } val;
    struct type *parent;
};

/* base type for any combination of pointers and arrays */
#define base_type(t) (t->parent)

/* array length in 1st and 2nd dimension */
unsigned type_array_length1(Type *t);
unsigned type_array_length2(Type *t);

/* whether type can be assign'ed to a PV */
unsigned type_assignable(Type *t);

/* generate code for a type, name is an optional variable name  */
void gen_type(Type *t, char *name);

/* creating types */
Type mk_prim_type(enum prim_type_tag tag);
Type mk_foreign_type(enum foreign_type_tag tag, char *name);
Type mk_ef_type();
Type mk_no_type();


#ifndef var_types_GLOBAL
extern
#endif
const char *foreign_type_prefix[]
#ifdef var_types_GLOBAL
= {
    "enum ",
    "struct ",
    "union ",
    "",
}
#endif
;

#endif /*INCLvar_typesh */
