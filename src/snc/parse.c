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

#define expr_type_GLOBAL
#include	"types.h"
#undef expr_type_GLOBAL
#include	"parse.h"
#include	"gen_code.h"
#include	"snc_main.h"

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif	/*TRUE*/

/* Parsing a program */
Program *program(
	char	*name,		/* program name */
	char	*param,		/* program parameters */
	Expr	*defn_list,	/* list of top-level definitions */
	Expr	*entry_code,	/* global entry actions */
	Expr	*ss_list,	/* state sets */
	Expr	*exit_code,	/* global exit actions */
	Expr	*c_code		/* global c code */
)
{
	Program *p = allocProgram();

	p->name = name;
	p->param = param;
	p->global_defn_list = defn_list;
	p->entry_code_list = entry_code;
	p->ss_list = ss_list;
	p->exit_code_list = exit_code;
	p->global_c_list = c_code;
	return p;
}

/* Parsing a variable declaration */
Expr *decl(
	int	type,		/* variable type (e.g. V_FLOAT) */
	int	class,		/* variable class (e.g. VC_ARRAY) */
	Token	var,		/* variable name token */
	char	*s_length1,	/* array lth (1st dim, arrays only) */
	char	*s_length2,	/* array lth (2nd dim, [n]x[m] arrays only) */
	char	*value		/* initial value or NULL */
)
{
	Var	*vp;
	int	length1, length2;

	length1 = length2 = 1;
	if (s_length1 != NULL)
	{
		length1 = atoi(s_length1);
		if (length1 <= 0)
			length1 = 1;
	}
	if (s_length2 != NULL)
	{
		length2 = atoi(s_length2);
		if (length2 <= 0)
			length2 = 1;
	}
	vp = allocVar();
	vp->name = var.str;
	vp->class = class;
	vp->type = type;
	vp->length1 = length1;
	vp->length2 = length2;
	vp->value = value;
	vp->chan = NULL;
	return expr(E_DECL, tok((char*)vp), 0, 0);
}

/* Expr is the generic syntax tree node. It is formed by a type  */
Expr *expr(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	Token	value,		/* "==", "+=", var name, constant, etc. */	
	Expr	*left,		/* LH side */
	Expr	*right		/* RH side */
)
{
	Expr		*ep;

	/* Allocate a structure for this item or expression */
	ep = allocExpr();
#ifdef	DEBUG
	if (type == E_DECL)
	{
		Var	*vp = (Var*)value.str;

		report(
		 "expr: ep=%p, type=%s, value="
		 "(var: name=%s, type=%d, class=%d, length1=%d, length2=%d, value=%s), "
		 "left=%p, right=%p\n",
		 ep, expr_type_names[type],
		 vp->name, vp->type, vp->class, vp->length1, vp->length2, vp->value,
		 left, right);
	}
        else
		report(
		 "expr: ep=%p, type=%s, value=\"%s\", left=%p, right=%p\n",
		 ep, expr_type_names[type], value.str, left, right);
#endif	/*DEBUG*/
	/* Fill in the structure */
	ep->next = 0;
	ep->last = ep;
	ep->type = type;
	ep->value = value.str;
	ep->left = left;
	ep->right = right;
	ep->line_num = value.line;
	ep->src_file = value.file;

	return ep;
}

/* Link two expression structures and/or lists.  Returns ptr to combined list.
   Note:  ->last ptrs are correct only for 1-st and last structures in the list */
Expr *link_expr(
	Expr	*ep1,	/* beginning of 1-st structure or list */
	Expr	*ep2	/* beginning 2-nd (append it to 1-st) */
)
{
#ifdef	DEBUG
	Expr	*ep;
#endif

	if (ep1 == 0 && ep2 == 0)
		return NULL;
	else if (ep1 == 0)
		return ep2;
	else if (ep2 == 0)
		return ep1;

	ep1->last->next = ep2;
	ep1->last = ep2->last;
#ifdef	DEBUG
	report("link_expr(");
	for (ep = ep1; ; ep = ep->next)
	{
		report("%p, ", ep);
		if (ep == ep1->last)
			break;
	}
	report(")\n");
#endif	/*DEBUG*/
	return ep1;
}
