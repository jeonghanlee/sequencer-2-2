/*#define	DEBUG	1*/
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

< 	parse.c,v 1.3 1995/10/19 16:30:16 wright Exp
	DESCRIPTION: Parsing support routines for state notation compiler.
	 The 'yacc' parser calls these routines to build the tables
	 and linked lists, which are then passed on to the phase 2 routines.

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
#include	"parse.h"
#undef expr_type_GLOBAL
#include	"phase2.h"
#include	"snc_main.h"

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif	/*TRUE*/

static Chan *build_channel(ChanList *chan_list, Var *vp);
static void alloc_channel_lists(Chan *cp, int length);
static void add_chan(ChanList *chan_list, Chan *cp);
Scope *analyze_declarations(Expr *defn_list);
ChanList *analyze_assignments(Scope *scope, Expr *defn_list);
void analyze_monitors(Scope *scope, Expr *defn_list);
void analyze_syncs_and_syncqs(Scope *scope, Expr *defn_list);

static void assign_subscr(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*subscript,	/* subscript value or NULL */
	char		*db_name
);
static void assign_single(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*db_name
);
void assign_list(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	Expr		*db_name_list
);
void monitor_stmt(
	Var		*vp,
	Expr		*dp,
	char		*subscript	/* element number or NULL */
);
void sync_stmt(
	Scope		*scope,
	Expr		*dp,
	char		*subscript,
	char		*ef_name
);
void syncq_stmt(
	Scope		*scope,
	Expr		*dp,
	char		*subscript,
	char		*ef_name,
	char		*maxQueueSize
);

/* Parsing whole program */
void program(
	char *pname,
	char *pparam,
	Expr *defn_list,
	Expr *entry_list,
	Expr *prog_list,
	Expr *exit_list,
	Expr *c_list
)
{
	Parse *parse = allocParse();

	parse->prog_name = pname;
	parse->prog_param = pparam;
	parse->global_defn_list = defn_list;
	parse->entry_code_list = entry_list;
	parse->ss_list = prog_list;
	parse->exit_code_list = exit_list;
	parse->global_c_list = c_list;

#ifdef	DEBUG
	fprintf(stderr, "---- Analysis ----\n");
#endif	/*DEBUG*/
	parse->global_scope = analyze_declarations(defn_list);
	parse->chan_list = analyze_assignments(parse->global_scope, defn_list);
	analyze_monitors(parse->global_scope, defn_list);
	analyze_syncs_and_syncqs(parse->global_scope, defn_list);

#ifdef	DEBUG
	fprintf(stderr, "---- Phase2 ----\n");
#endif	/*DEBUG*/
	phase2(parse);

	exit(0);
}

Scope *analyze_declarations(Expr *defn_list)
{
	Expr *dp;
	Var *vp, *vp2;
	Scope *scope;

#ifdef	DEBUG
	fprintf(stderr, "-- Declarations --\n");
#endif	/*DEBUG*/
	scope = allocScope();
	scope->var_list = allocVarList();
	for (dp = defn_list; dp != NULL; dp = dp->next)
	{
		if (dp->type != E_DECL)
			continue;
		vp = (Var *)dp->value;
		assert(vp != NULL);
		vp->line_num = dp->line_num;
		vp2 = find_var(scope, vp->name);
		if (vp2 != NULL)
		{
			fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
			fprintf(stderr, "assign: variable %s already declared in line %d\n",
			 vp->name, vp2->line_num);
			continue;
		}
		add_var(scope, vp);
	}
	return scope;
}

ChanList *analyze_assignments(Scope *scope, Expr *defn_list)
{
	Expr		*dp;
	ChanList	*chan_list;

#ifdef	DEBUG
	fprintf(stderr, "-- Assignments --\n");
#endif	/*DEBUG*/
	chan_list = allocChanList();
	for (dp = defn_list; dp != NULL; dp = dp->next)
	{
		char *name;
		Expr *pv_names;
		Var *vp;

		if (dp->type != E_ASSIGN)
			continue;
		name = dp->value;
		pv_names = dp->right;

		vp = find_var(scope, name);
		if (vp == NULL)
		{
			fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
			fprintf(stderr, "assign: variable %s not declared\n", name);
			continue;
		}
		if (dp->left != NULL)
		{
			assign_subscr(chan_list, dp, vp, dp->left->value,
				pv_names->value);
		}
		else if (dp->right->next == NULL) {
			assign_single(chan_list, dp, vp, pv_names->value);
		}
		else
		{
			assign_list(chan_list, dp, vp, pv_names);
		}
	}
	return chan_list;
}

void analyze_monitors(Scope *scope, Expr *defn_list)
{
	Expr *dp = defn_list;

#ifdef	DEBUG
	fprintf(stderr, "-- Monitors --\n");
#endif	/*DEBUG*/
	for (dp = defn_list; dp; dp = dp->next)
	{
		if (dp->type == E_MONITOR)
		{
			char *name = dp->value;
			char *subscript = dp->left ? dp->left->value : 0;
			Var *vp = find_var(scope, name);

			if (vp == NULL)
			{
				fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
				fprintf(stderr, "assign: variable %s not declared\n", name);
				continue;
			}
			monitor_stmt(vp, dp, subscript);
		}
	}
}

void analyze_syncs_and_syncqs(Scope *scope, Expr *defn_list)
{
	Expr *dp = defn_list;

#ifdef	DEBUG
	fprintf(stderr, "-- Sync and SyncQ --\n");
#endif	/*DEBUG*/
	for (dp = defn_list; dp; dp = dp->next)
	{
		char *subscript = dp->left ? dp->left->value : 0;
		char *ef_name;

		if (dp->type == E_SYNC)
		{
			assert(dp->right);
			assert(dp->right->type == E_X);
			ef_name = dp->right->value;
			sync_stmt(scope, dp, subscript, ef_name);
		}
		if (dp->type == E_SYNCQ)
		{
			char *maxQueueSize;
			assert(dp->right);
			assert(dp->right->type == E_X);
			ef_name = dp->right->value;
			maxQueueSize = dp->right->left ? dp->right->left->value : 0;
			assert(maxQueueSize==NULL || dp->right->left->type==E_CONST);
			syncq_stmt(scope, dp, subscript, ef_name, maxQueueSize);
		}
	}
}

/* Parsing a declaration */
Expr *declaration(
	int	type,		/* variable type (e.g. V_FLOAT) */
	int	class,		/* variable class (e.g. VC_ARRAY) */
	char	*name,		/* ptr to variable name */
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
	vp->name = name;
	vp->class = class;
	vp->type = type;
	vp->length1 = length1;
	vp->length2 = length2;
	vp->value = value;
	vp->chan = NULL;
	return expression(E_DECL, (char *)vp, 0, 0);
}

/* Option statement */
void option_stmt(
	char	*option,	/* "a", "r", ... */
	int	value		/* TRUE means +, FALSE means - */
)
{
	Options *options = globals->options;
	switch(*option)
	{
	    case 'a':
		options->async = value;
		break;
	    case 'c':
		options->conn = value;
		break;
	    case 'd':
		options->debug = value;
		break;
	    case 'e':
		options->newef = value;
		break;
	    case 'i':
		options->init_reg = value;
		break;
	    case 'l':
		options->line = value;
		break;
	    case 'm':
		options->main = value;
		break;
	    case 'r':
		options->reent = value;
		break;
	    case 'w':
		options->warn = value;
		break;
	}
	return;
}

/* "Assign" statement: Assign a variable to a DB channel.
 * Format: assign <variable> to <string;
 * Note: Variable may be subscripted.
 */
static void assign_single(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*db_name
)
{
	Chan		*cp;
	char		*name = vp->name;

#ifdef	DEBUG
	fprintf(stderr, "assign %s to \"%s\";\n", name, db_name);
#endif	/*DEBUG*/

	cp = vp->chan;
	if (cp != NULL)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "assign: %s already assigned\n", name);
		return;
	}

	/* Build structure for this channel */
	cp = build_channel(chan_list, vp);

	cp->db_name = db_name;	/* DB name */

	/* The entire variable is assigned */
	cp->count = vp->length1 * vp->length2;

	return;
}

/* "Assign" statement: assign an array element to a DB channel.
 * Format: assign <variable>[<subscr>] to <string>; */
static void assign_subscr(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*subscript,	/* subscript value or NULL */
	char		*db_name
)
{
	Chan		*cp;
	char		*name = vp->name;
	int		subNum;

#ifdef	DEBUG
	fprintf(stderr, "assign %s[%s] to \"%s\";\n", name, subscript, db_name);
#endif	/*DEBUG*/

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "assign: variable %s not an array\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == NULL)
	{
		/* Build structure for this channel */
		cp = build_channel(chan_list, vp);
	}
	else if (cp->db_name != NULL)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "assign: array %s already assigned\n", name);
		return;
	}

	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= vp->length1)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "assign: subscript %s[%d] is out of range\n",
		 name, subNum);
		return;
	}

	if (cp->db_name_list == NULL)
		alloc_channel_lists(cp, vp->length1); /* allocate lists */
	else if (cp->db_name_list[subNum] != NULL)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "assign: %s[%d] already assigned\n",
		 name, subNum);
		return;
	}

	cp->db_name_list[subNum] = db_name;
	cp->count = vp->length2; /* could be a 2-dimensioned array */

	return;
}

/* Assign statement: assign an array to multiple DB channels.
 * Format: assign <variable> to { <string>, <string>, ... };
 * Assignments for double dimensioned arrays:
 * <var>[0][0] assigned to 1st db name,
 * <var>[1][0] assigned to 2nd db name, etc.
 * If db name list contains fewer names than the array dimension,
 * the remaining elements receive NULL assignments.
 */
void assign_list(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	Expr		*db_name_list
)
{
	Chan		*cp;
	int		elem_num;
	char		*name = vp->name;

#ifdef	DEBUG
	fprintf(stderr, "assign %s to {", name);
#endif	/*DEBUG*/

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "assign: variable %s is not an array\n", name);
		return;
	}

	cp = vp->chan;
	if (cp != NULL)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "assign: variable %s already assigned\n", name);
		return;
	}

	/* Build a db structure for this variable */
	cp = build_channel(chan_list, vp);

	/* Allocate lists */
	alloc_channel_lists(cp, vp->length1); /* allocate lists */

	/* fill in the array of pv names */
	for (elem_num = 0; elem_num < vp->length1; elem_num++)
	{
		if (db_name_list == NULL)
			break; /* end of list */

#ifdef	DEBUG
		fprintf(stderr, "\"%s\", ", db_name_list->value);
#endif	/*DEBUG*/
		cp->db_name_list[elem_num] = db_name_list->value; /* DB name */
		cp->count = vp->length2;
 
		db_name_list = db_name_list->next;
	}
#ifdef	DEBUG
		fprintf(stderr, "};\n");
#endif	/*DEBUG*/

	return;
}

/* Build a channel structure for this variable */
static Chan *build_channel(ChanList *chan_list, Var *vp)
{
	Chan		*cp;

	cp = allocChan();
	add_chan(chan_list, cp);		/* add to Chan list */

	/* make connections between Var & Chan structures */
	cp->var = vp;
	vp->chan = cp;

	/* Initialize the structure */
	cp->db_name_list = 0;
	cp->mon_flag_list = 0;
	cp->ef_var_list = 0;
	cp->ef_num_list = 0;
	cp->num_elem = 0;
	cp->mon_flag = 0;
	cp->ef_var = 0;
	cp->ef_num = 0;

	return cp;
}

/* Allocate lists for assigning multiple pv's to a variable */
static void alloc_channel_lists(Chan *cp, int length)
{
	/* allocate an array of pv names */
	cp->db_name_list = (char **)calloc(sizeof(char **), length);

	/* allocate an array for monitor flags */
	cp->mon_flag_list = (int *)calloc(sizeof(int **), length);

	/* allocate an array for event flag var ptrs */
	cp->ef_var_list = (Var **)calloc(sizeof(Var **), length);

	/* allocate an array for event flag numbers */
	cp->ef_num_list = (int *)calloc(sizeof(int **), length);

	cp->num_elem = length;
}

void monitor_stmt(
	Var		*vp,
	Expr		*dp,
	char		*subscript	/* element number or NULL */
)
{
	Chan		*cp;
	int		subNum;
	char		*name = vp->name;

#ifdef	DEBUG
	fprintf(stderr, "monitor %s", name);
	if (subscript != NULL) fprintf(stderr, "[%s]", subscript);
	fprintf(stderr, "\n");
#endif	/*DEBUG*/

	/* Find a channel assigned to this variable */
	cp = vp->chan;
	if (cp == 0)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "monitor: variable %s not assigned\n", name);
		return;
	}

	if (subscript == NULL)
	{
		if (cp->num_elem == 0)
		{	/* monitor one channel for this variable */
			cp->mon_flag = TRUE;
			return;
		}

		/* else monitor all channels in db list */
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{	/* 1 pv per element of the array */
			cp->mon_flag_list[subNum] = TRUE;
		}
		return;
	}

	/* subscript != NULL */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "monitor: subscript of %s out of range\n", name);
		return;
	}

	if (cp->num_elem == 0 || cp->db_name_list[subNum] == NULL)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "monitor: %s[%d] not assigned\n",
		 name, subNum);
		return;
	}

	cp->mon_flag_list[subNum] = TRUE;
	return;
}

void sync_stmt(
	Scope		*scope,
	Expr		*dp,
	char		*subscript,
	char		*ef_name
)
{
	Chan		*cp;
	Var		*vp;
	int		subNum;
	char		*name = dp->value;

#ifdef	DEBUG
	fprintf(stderr, "sync %s%s%s%s to %s\n", name,
	 subscript?"[":"", subscript?subscript:"", subscript?"]":"", ef_name);
#endif	/*DEBUG*/

	vp = find_var(scope, name);
	if (vp == 0)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "sync: variable %s not declared\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "sync: variable %s not assigned\n", name);
		return;
	}

	/* Find the event flag varible */
	vp = find_var(scope, ef_name);
	if (vp == 0 || vp->type != V_EVFLAG)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "sync: e-f variable %s not declared\n",
		 ef_name);
		return;
	}

	if (subscript == NULL)
	{	/* no subscript */
		if (cp->db_name != NULL)
		{	/* 1 pv assigned to this variable */
			cp->ef_var = vp;
			return;
		}

		/* 1 pv per element in the array */
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{
			cp->ef_var_list[subNum] = vp;
		}
		return;
	}

	/* subscript != NULL */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "sync: subscript %s[%d] out of range\n",
		 name, subNum);
		return;
	}
	cp->ef_var_list[subNum] = vp; /* sync to a specific element of the array */

	return;
}

void syncq_stmt(
	Scope		*scope,
	Expr		*dp,
	char		*subscript,
	char		*ef_name,
	char		*maxQueueSize
)
{
	Chan		*cp;
	Var		*vp;
	Var		*efp;
	int		subNum;
	char		*name = dp->value;

#ifdef	DEBUG
	fprintf(stderr, "syncq_stmt: name=%s, subNum=%s, ef_name=%s, "
	    "maxQueueSize=%s\n", name, subscript, ef_name, maxQueueSize);
#endif	/*DEBUG*/

	/* Find the variable and check it's assigned */
	vp = find_var(scope, name);
	if (vp == 0)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "syncQ: variable %s not declared\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "syncQ: variable %s not assigned\n", name);
		return;
	}

	/* Check that the variable has not already been syncQ'd */
	if (vp->queued)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "syncQ: variable %s already syncQ'd\n", name);
		return;
	}

	/* Find the event flag variable */
	efp = find_var(scope, ef_name);
	if (efp == 0 || efp->type != V_EVFLAG)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "syncQ: e-f variable %s not declared\n", ef_name);
		return;
	}

	/* Check that the event flag has not already been syncQ'd */
	if (efp->queued)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "syncQ: e-f variable %s already syncQ'd\n", ef_name);
		return;
	}

	/* Note queued (for both variable and event flag) and set the
	   maximum queue size (0 means default) */
	vp->queued = efp->queued = TRUE;
	vp->maxQueueSize = (maxQueueSize == NULL) ? 0 : atoi(maxQueueSize);

	if (subscript == NULL)
	{	/* no subscript */
		if (cp->db_name != NULL)
		{	/* 1 pv assigned to this variable */
			cp->ef_var = efp;
			return;
		}

		/* 1 pv per element in the array */
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{
			cp->ef_var_list[subNum] = efp;
		}
		return;
	}

	/* subscript != NULL */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		fprintf(stderr, "%s:%d: ", dp->src_file, dp->line_num);
		fprintf(stderr, "syncQ: subscript %s[%d] out of range\n",
		 name, subNum);
		return;
	}
	cp->ef_var_list[subNum] = efp; /* sync to a specific element of the array */
}

/* Add a variable to a scope; (append to the end of the list */
void add_var(Scope *scope, Var *vp)
{
	VarList	*var_list = scope->var_list;

	if (var_list->first == NULL)
		var_list->first = vp;
	else
		var_list->last->next = vp;
	var_list->last = vp;
	vp->next = NULL;
}

/* Find a variable by name, given a scope; first searches the given
   scope, then the parent scope, and so on. Returns a pointer to the
   var struct or NULL if the variable is not found. */
Var *find_var(Scope *scope, char *name)
{
	Var	*vp;

	for ( ; scope != NULL; scope = scope->next)
	{
		for (vp = scope->var_list->first; vp != NULL; vp = vp->next)
		{
			if (strcmp(vp->name, name) == 0)
				return vp;
		}
	}
	return NULL;
}

/* Add a channel to the channel linked list */
static void add_chan(ChanList *chan_list, Chan *cp)
{
	if (chan_list->first == NULL)
		chan_list->first = cp;
	else
		chan_list->last->next = cp;
	chan_list->last = cp;
	cp->next = NULL;
}

/* Build an expression list (hierarchical):
   Builds a node on a binary tree for each expression primitive. */
Expr *expression(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	char	*value,		/* "==", "+=", var name, constant, etc. */	
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
		Var	*vp = (Var*)value;

		fprintf(stderr,
		 "expression: ep=%p, type=%s, value="
		 "(var: name=%s, type=%d, class=%d, length1=%d, length2=%d, value=%s), "
		 "left=%p, right=%p\n",
		 ep, expr_type_names[type],
		 vp->name, vp->type, vp->class, vp->length1, vp->length2, vp->value,
		 left, right);
	}
        else
		fprintf(stderr,
		 "expression: ep=%p, type=%s, value=\"%s\", left=%p, right=%p\n",
		 ep, expr_type_names[type], value, left, right);
#endif	/*DEBUG*/
	/* Fill in the structure */
	ep->next = 0;
	ep->last = ep;
	ep->type = type;
	ep->value = value;
	ep->left = left;
	ep->right = right;
	if (type == E_TEXT)
		ep->line_num = globals->line_num-1;
	else
		ep->line_num = globals->line_num;
	ep->src_file = globals->src_file;

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
	fprintf(stderr, "link_expr(");
	for (ep = ep1; ; ep = ep->next)
	{
		fprintf(stderr, "%p, ", ep);
		if (ep == ep1->last)
			break;
	}
	fprintf(stderr, ")\n");	
#endif	/*DEBUG*/
	return ep1;
}
