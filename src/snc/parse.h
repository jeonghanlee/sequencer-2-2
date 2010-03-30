/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1989-93, The Regents of the University of California.
		         Los Alamos National Laboratory

	DESCRIPTION: Structures for parsing the state notation language.

	ENVIRONMENT: UNIX
	HISTORY:
18nov91,ajk	Replaced lstLib stuff with in-line links.
28oct93,ajk	Added support for assigning array elements to pv's.
28oct93,ajk	Added support for pointer declarations (see VC_*)
05nov93,ajk	Changed structures var & db_chan to handle array assignments.
05nov93,ajk	changed malloc() to calloc() 3 places.
20jul95,ajk	Added unsigned types (V_U...).
08aug96,wfl	Added syncQ variables to var struct.
01sep99,grw     Added E_OPTION, E_ENTRY, E_EXIT.
07sep99,wfl	Added E_DECL (for local variable declarations).
***************************************************************************/

#ifndef INCLparseh
#define INCLparseh

#include "types.h"

Expr *expr(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	Token	tok,		/* "==", "+=", var name, constant, etc. */
	...
);

Expr *decl(
	int	type,		/* variable type (e.g. V_FLOAT) */
	int	class,		/* variable class (e.g. VC_ARRAY) */
	Token	var,		/* variable name token */
	char	*s_length1,	/* array lth (1st dim, arrays only) */
	char	*s_length2,	/* array lth (2nd dim, [n]x[m] arrays only) */
	Expr	*value		/* initial value or NULL */
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

#endif	/*INCLparseh*/
