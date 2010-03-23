/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	DESCRIPTION: Parsing support routines for state notation compiler.

	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Replaced lstLib calls with built-in links.
20nov91,ajk	Removed snc_init() - no longer did anything useful.
20nov91,ajk	Added option_stmt() routine.
28apr92,ajk	Implemented new event flag mode.
29opc93,ajk	Implemented assignment of pv's to array elements.
29oct93,ajk	Implemented variable class (VC_SIMPLE, VC_ARRAY, & VC_POINTER).
29oct93,ajk	Added 'v' (vxWorks include) option.
17may94,ajk	Removed old event flag (-e) option.
08aug96,wfl	Added new syncq_stmt() routine.
29apr99,wfl	Avoided compilation warnings; removed unused vx_opt option.
17may99,wfl	Fixed crash in debug output.
06jul99,wfl	Added "+m" (main) option.
07sep99,wfl	Added E_DECL expression type (for local declarations).
22sep99,grw     Supported entry and exit actions; supported state options.
06mar00,wfl	Avoided NULL pointer crash when DEBUG is enabled.
31mar00,wfl     Supported entry handler.
***************************************************************************/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>
#include        <stdarg.h>

#define expr_type_GLOBAL
#include	"types.h"
#undef expr_type_GLOBAL
#include	"parse.h"
#include	"snc_main.h"

/* #define DEBUG */

/* Parsing a variable declaration */
Expr *decl(
	int	type,		/* variable type (e.g. V_FLOAT) */
	int	class,		/* variable class (e.g. VC_ARRAY) */
	Token	var,		/* variable name token */
	char	*s_length1,	/* array lth (1st dim, arrays only) */
	char	*s_length2,	/* array lth (2nd dim, [n]x[m] arrays only) */
	Expr	*init		/* initial value or NULL */
)
{
	Expr	*ep;
	Var	*vp;
	int	length1 = 1, length2 = 1;

	if (s_length1 != NULL)
	{
		length1 = atoi(s_length1);
		if (length1 <= 0) {
			error_at(var.file, var.line,
				"invalid array size (must be >= 1)\n");
			length1 = 1;
		}
	}
	if (s_length2 != NULL)
	{
		length2 = atoi(s_length2);
		if (length2 <= 0) {
			error_at(var.file, var.line,
				"invalid array size (must be >= 1)\n");
			length2 = 1;
		}
	}
	vp = new(Var);
	vp->name = var.str;
	vp->class = class;
	vp->type = type;
	vp->length1 = length1;
	vp->length2 = length2;
	vp->value = init;

        ep = expr(D_DECL, var, init);
	ep->extra.e_decl = vp;
#ifdef	DEBUG
	report_at_expr(ep, "decl: name=%s, type=%d, class=%d, "
		"length1=%d, length2=%d, value=%s\n",
		vp->name, vp->type, vp->class,
		vp->length1, vp->length2, vp->value);
#endif	/*DEBUG*/
	vp->decl = ep;
	return ep;
}

/* Expr is the generic syntax tree node */
Expr *expr(
	int	type,
	Token	tok,
	...			/* variable number of child arguments */
)
{
        va_list	argp;
	int	i, num_children;
	Expr	*ep;

	num_children = expr_type_info[type].num_children;

	ep = new(Expr);
	ep->next = 0;
	ep->last = ep;
	ep->type = type;
	ep->value = tok.str;
	ep->line_num = tok.line;
	ep->src_file = tok.file;
	ep->children = calloc(num_children, sizeof(Expr*));
	/* allocate extra data */
	switch (type)
	{
	case D_SS:
		ep->extra.e_ss = new(StateSet);
		break;
	case D_STATE:
		ep->extra.e_state = new(State);
		ep->extra.e_state->options = DEFAULT_STATE_OPTIONS;
		break;
	case D_WHEN:
		ep->extra.e_when = new(When);
		break;
	}

#ifdef	DEBUG
	report_at_expr(ep, "expr: ep=%p, type=%s, value=\"%s\", file=%s, line=%d",
		ep, expr_type_name(ep), tok.str, tok.file, tok.line);
#endif	/*DEBUG*/
        va_start(argp, tok);
	for (i = 0; i < num_children; i++)
	{
		ep->children[i] = va_arg(argp, Expr*);
#ifdef	DEBUG
		report(", child[%d]=%p", i, ep->children[i]);
#endif	/*DEBUG*/
	}
        va_end(argp);
#ifdef	DEBUG
	report(")\n");
#endif	/*DEBUG*/

	return ep;
}

Expr *opt_defn(Token name, Token value)
{
	Expr *opt = expr(D_OPTION, name);
	opt->extra.e_option = (value.str[0] == '+');
	return opt;
}

/* Link two expression structures and/or lists.  Returns ptr to combined list.
   Note: last ptrs are correct only for 1-st element of the resulting list */
Expr *link_expr(
	Expr	*ep1,	/* 1-st structure or list */
	Expr	*ep2	/* 2-nd (append it to 1-st) */
)
{
	if (ep1 == 0)
		return ep2;
	if (ep2 == 0)
		return 0;
	ep1->last->next = ep2;
	ep1->last = ep2->last;
	ep2->last = 0;
	return ep1;
}
