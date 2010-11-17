/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1989-93, The Regents of the University of California.
		         Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)
***************************************************************************
		Parser support routines
***************************************************************************/
#ifndef INCLparseh
#define INCLparseh

#include "types.h"

Expr *expr(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	Token	tok,		/* "==", "+=", var name, constant, etc. */
	...
);

Expr *opt_defn(
	Token	name,
	Token	value
);

Expr *link_expr(
	Expr	*ep1,		/* beginning of 1-st structure or list */
	Expr	*ep2		/* beginning 2-nd (append it to 1-st) */
);

boolean strtoui(
	char *str,		/* string representing a number */
	uint limit,		/* result should be < limit */
	uint *pnumber		/* location for result if successful */
);

Expr *decl_add_base_type(Expr *ds, uint tag);
Expr *decl_add_init(Expr *d, Expr *init);
Expr *decl_create(Token name);
Expr *decl_postfix_array(Expr *d, char *s);
Expr *decl_prefix_pointer(Expr *d);

#endif	/*INCLparseh*/
