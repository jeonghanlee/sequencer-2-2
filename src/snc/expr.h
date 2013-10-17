/*************************************************************************\
Copyright (c) 1989-1993 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                Parser support routines
\*************************************************************************/
#ifndef INCLparseh
#define INCLparseh

#include "types.h"

/* defined in expr.c */
Expr *expr(
	uint	type,
	Token	tok,
	...
);

Expr *opt_defn(
	Token	name,
	Token	value
);

Expr *link_expr(
	Expr	*ep1,
	Expr	*ep2
);

uint strtoui(
	char *str,		/* string representing a number */
	uint limit,		/* result should be < limit */
	uint *pnumber		/* location for result if successful */
);

Token token_from_expr(Expr *e);

#endif	/*INCLparseh*/
