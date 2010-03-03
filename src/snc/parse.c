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

/*====================== Includes, globals, & defines ====================*/
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"parse.h"
#include	"phase2.h"
#include	"snc_main.h"

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif	/*TRUE*/

static Chan *build_db_struct(Var *vp);
static void alloc_db_lists(Chan *cp, int length);
static void add_chan(Chan *cp);

static Parse global_parse = {0,0,0,0,0,0,0,0,0,0,0};

static Parse *parse = &global_parse;

/* Parsing "program" statement */
void program(
	char *pname,
	char *pparam,
	Expr *entry_list,
	Expr *prog_list,
	Expr *exit_list,
	Expr *c_list
)
{
	parse->prog_name = pname;
	parse->prog_param = pparam;
	parse->entry_code_list = entry_list;
	parse->ss_list = prog_list;
	parse->exit_code_list = exit_list;
	parse->global_c_list = c_list;
#ifdef	DEBUG
	fprintf(stderr, "----Phase2---\n");
#endif	/*DEBUG*/
	phase2(parse);

	exit(0);
}

/* Parsing a declaration statement */
void decl_stmt(
	int	type,		/* variable type (e.g. V_FLOAT) */
	int	class,		/* variable class (e.g. VC_ARRAY) */
	char	*name,		/* ptr to variable name */
	char	*s_length1,	/* array lth (1st dim, arrays only) */
	char	*s_length2,	/* array lth (2nd dim, [n]x[m] arrays only) */
	char	*value		/* initial value or NULL */
)
{
	Var		*vp;
	int		length1, length2;

#ifdef	DEBUG
	fprintf(stderr, 
	 "variable decl: type=%d, class=%d, name=%s, ", type, class, name);
#endif	/*DEBUG*/
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

#ifdef	DEBUG
	fprintf(stderr, "length1=%d, length2=%d\n", length1, length2);
#endif	/*DEBUG*/
	/* See if variable already declared */
	vp = (Var *)find_var(name);
	if (vp != 0)
	{
		fprintf(stderr, "variable %s already declared, line %d\n",
		 name, globals->line_num);
		return;
	}
	/* Build a struct for this variable */
	vp = allocVar();
	add_var(vp); /* add to var list */
	vp->name = name;
	vp->class = class;
	vp->type = type;
	vp->length1 = length1;
	vp->length2 = length2;
	vp->value = value; /* initial value or NULL */
	vp->chan = NULL;
	return;
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
void assign_single(
	char	*name,		/* ptr to variable name */
	char	*db_name	/* ptr to db name */
)
{
	Chan		*cp;
	Var		*vp;

#ifdef	DEBUG
	fprintf(stderr, "assign %s to \"%s\";\n", name, db_name);
#endif	/*DEBUG*/
	/* Find the variable */
	vp = (Var *)find_var(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, globals->line_num);
		return;
	}

	cp = vp->chan;
	if (cp != NULL)
	{
		fprintf(stderr, "assign: %s already assigned, line %d\n",
		 name, globals->line_num);
		return;
	}

	/* Build structure for this channel */
	cp = build_db_struct(vp);

	cp->db_name = db_name;	/* DB name */

	/* The entire variable is assigned */
	cp->count = vp->length1 * vp->length2;

	return;
}

/* "Assign" statement: assign an array element to a DB channel.
 * Format: assign <variable>[<subscr>] to <string>; */
void assign_subscr(
	char	*name,		/* ptr to variable name */
	char	*subscript,	/* subscript value or NULL */
	char	*db_name	/* ptr to db name */
)
{
	Chan		*cp;
	Var		*vp;
	int		subNum;

#ifdef	DEBUG
	fprintf(stderr, "assign %s[%s] to \"%s\";\n", name, subscript, db_name);
#endif	/*DEBUG*/
	/* Find the variable */
	vp = (Var *)find_var(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, globals->line_num);
		return;
	}

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		fprintf(stderr, "assign: variable %s not an array, line %d\n",
		 name, globals->line_num);
		return;
	}

	cp = vp->chan;
	if (cp == NULL)
	{
		/* Build structure for this channel */
		cp = build_db_struct(vp);
	}
	else if (cp->db_name != NULL)
	{
		fprintf(stderr, "assign: array %s already assigned, line %d\n",
		 name, globals->line_num);
		return;
	}

	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= vp->length1)
	{
		fprintf(stderr, 
		 "assign: subscript %s[%d] is out of range, line %d\n",
		 name, subNum, globals->line_num);
		return;
	}

	if (cp->db_name_list == NULL)
		alloc_db_lists(cp, vp->length1); /* allocate lists */
	else if (cp->db_name_list[subNum] != NULL)
	{
		fprintf(stderr, "assign: %s[%d] already assigned, line %d\n",
		 name, subNum, globals->line_num);
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
	char	*name,		/* ptr to variable name */
	Expr	*db_name_list	/* ptr to db name list */
)
{
	Chan		*cp;
	Var		*vp;
	int		elem_num;

#ifdef	DEBUG
	fprintf(stderr, "assign %s to {", name);
#endif	/*DEBUG*/
	/* Find the variable */
	vp = (Var *)find_var(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, globals->line_num);
		return;
	}

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		fprintf(stderr, "assign: variable %s is not an array, line %d\n",
		 name, globals->line_num);
		return;
	}

	cp = vp->chan;
	if (cp != NULL)
	{
		fprintf(stderr, "assign: variable %s already assigned, line %d\n",
		 name, globals->line_num);
		return;
	}

	/* Build a db structure for this variable */
	cp = build_db_struct(vp);

	/* Allocate lists */
	alloc_db_lists(cp, vp->length1); /* allocate lists */

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

/* Build a db structure for this variable */
static Chan *build_db_struct(Var *vp)
{
	Chan		*cp;

	cp = allocChan();
	add_chan(cp);		/* add to Chan list */

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
static void alloc_db_lists(Chan *cp, int length)
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

/* Parsing a "monitor" statement.
 * Format:
 * 	monitor <var>; - monitor a single variable or all elements in an array.
 * 	monitor <var>[<m>]; - monitor m-th element of an array.
 */
void monitor_stmt(
	char	*name,		/* variable name (should be assigned) */
	char	*subscript	/* element number or NULL */
)
{
	Var		*vp;
	Chan		*cp;
	int		subNum;

#ifdef	DEBUG
	fprintf(stderr, "monitor_stmt: name=%s", name);
	if (subscript != NULL) fprintf(stderr, "[%s]", subscript);
	fprintf(stderr, "\n");
#endif	/*DEBUG*/

	/* Find the variable */
	vp = (Var *)find_var(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, globals->line_num);
		return;
	}

	/* Find a channel assigned to this variable */
	cp = vp->chan;
	if (cp == 0)
	{
		fprintf(stderr, "monitor: variable %s not assigned, line %d\n",
		 name, globals->line_num);
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
		fprintf(stderr, "monitor: subscript of %s out of range, line %d\n",
		 name, globals->line_num);
		return;
	}

	if (cp->num_elem == 0 || cp->db_name_list[subNum] == NULL)
	{
		fprintf(stderr, "monitor: %s[%d] not assigned, line %d\n",
		 name, subNum, globals->line_num);
		return;
	}

	cp->mon_flag_list[subNum] = TRUE;
	return;
}
	
/* Parsing "sync" statement */
void sync_stmt(char *name, char *subscript, char *ef_name)
{
	Chan		*cp;
	Var		*vp;
	int		subNum;

#ifdef	DEBUG
	fprintf(stderr, "sync_stmt: name=%s, subNum=%s, ef_name=%s\n",
	 name, subscript?subscript:"(no subscript)", ef_name);
#endif	/*DEBUG*/

	vp = (Var *)find_var(name);
	if (vp == 0)
	{
		fprintf(stderr, "sync: variable %s not declared, line %d\n",
		 name, globals->line_num);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		fprintf(stderr, "sync: variable %s not assigned, line %d\n",
		 name, globals->line_num);
		return;
	}

	/* Find the event flag varible */
	vp = (Var *)find_var(ef_name);
	if (vp == 0 || vp->type != V_EVFLAG)
	{
		fprintf(stderr, "sync: e-f variable %s not declared, line %d\n",
		 ef_name, globals->line_num);
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
		fprintf(stderr,
		 "sync: subscript %s[%d] out of range, line %d\n",
		 name, subNum, globals->line_num);
		return;
	}
	cp->ef_var_list[subNum] = vp; /* sync to a specific element of the array */

	return;
}

/* Parsing "syncq" statement */
void syncq_stmt(char *name, char *subscript, char *ef_name, char *maxQueueSize)
{
	Chan		*cp;
	Var		*vp;
	Var		*efp;
	int		subNum;

#ifdef	DEBUG
	fprintf(stderr, "syncq_stmt: name=%s, subNum=%s, ef_name=%s, "
	    "maxQueueSize=%s\n", name, subscript, ef_name, maxQueueSize);
#endif	/*DEBUG*/

	/* Find the variable and check it's assigned */
	vp = (Var *)find_var(name);
	if (vp == 0)
	{
		fprintf(stderr, "syncQ: variable %s not declared, line %d\n",
		 name, globals->line_num);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		fprintf(stderr, "syncQ: variable %s not assigned, line %d\n",
		 name, globals->line_num);
		return;
	}

	/* Check that the variable has not already been syncQ'd */
	if (vp->queued)
	{
		fprintf(stderr, "syncQ: variable %s already syncQ'd, "
		 "line %d\n", name, globals->line_num);
		return;
	}

	/* Find the event flag variable */
	efp = (Var *)find_var(ef_name);
	if (efp == 0 || efp->type != V_EVFLAG)
	{
		fprintf(stderr, "syncQ: e-f variable %s not declared, "
		 "line %d\n", ef_name, globals->line_num);
		return;
	}

	/* Check that the event flag has not already been syncQ'd */
	if (efp->queued)
	{
		fprintf(stderr, "syncQ: e-f variable %s already syncQ'd, "
		 "line %d\n", ef_name, globals->line_num);
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
		fprintf(stderr,
		 "syncQ: subscript %s[%d] out of range, line %d\n",
		 name, subNum, globals->line_num);
		return;
	}
	cp->ef_var_list[subNum] = efp; /* sync to a specific element of the array */

	return;
}
	

/* Definition C code */
void defn_c_stmt(
	Expr *c_list	/* ptr to C code */
)
{
#ifdef	DEBUG
	fprintf(stderr, "defn_c_stmt\n");
#endif	/*DEBUG*/
	if (parse->defn_c_list == 0)
		parse->defn_c_list = c_list;
	else
		link_expr(parse->defn_c_list, c_list);	
	
	return;
}

/* Add a variable to the variable linked list */
void add_var(Var *vp)
{
	if (parse->global_var_list == NULL)
		parse->global_var_list = vp;
	else
		parse->global_var_tail->next = vp;
	parse->global_var_tail = vp;
	vp->next = NULL;
}

/* Find a variable by name;  returns a pointer to the Var struct;
	returns 0 if the variable is not found. */
Var *find_var(char *name)
{
	Var		*vp;

	for (vp = parse->global_var_list; vp != NULL; vp = vp->next)
	{
		if (strcmp(vp->name, name) == 0)
		{
			return vp;
		}
	}
	return 0;
}

/* Add a channel to the channel linked list */
static void add_chan(Chan *cp)
{
	if (parse->chan_list == NULL)
		parse->chan_list = cp;
	else
		parse->chan_tail->next = cp;
	parse->chan_tail = cp;
	cp->next = NULL;
}

/* Build an expression list (hierarchical):
	Builds a node on a binary tree for each expression primitive.
 */
Expr *expression(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	char	*value,		/* "==", "+=", var name, constant, etc. */	
	Expr	*left,		/* LH side */
	Expr	*right		/* RH side */
)
{
	Expr		*ep;
#ifdef	DEBUG
#endif	/*DEBUG*/
	/* Allocate a structure for this item or expression */
	ep = allocExpr();
#ifdef	DEBUG
	fprintf(stderr,
		"expression: ep=%p, type=%s, value=\"%s\", left=%p, right=%p\n",
		ep, expr_type_names[type], value, left, right);
#endif	/*DEBUG*/
	/* Fill in the structure */
	ep->next = (Expr *)0;
	ep->last = ep;
	ep->type = type;
	ep->value = value;
	ep->left = left;
	ep->right = right;
	if (type == E_TEXT)
		ep->line_num = globals->c_line_num;
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

	(ep1->last)->next = ep2;
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

/* The ordering of this list must correspond with the ordering in parse.h */
char *expr_type_names[E_NUM_TYPES] = {
	"E_EMPTY", "E_CONST", "E_VAR", "E_FUNC", "E_STRING", "E_UNOP", "E_BINOP",
	"E_ASGNOP", "E_PAREN", "E_SUBSCR", "E_TEXT", "E_STMT", "E_CMPND",
	"E_IF", "E_ELSE", "E_WHILE", "E_SS", "E_STATE", "E_WHEN",
	"E_FOR", "E_X", "E_PRE", "E_POST", "E_BREAK", "E_COMMA", "E_DECL",
	"E_ENTRY", "E_EXIT", "E_OPTION"
};
