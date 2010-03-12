#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "snc_main.h"
#include "analysis.h"

const int impossible = 0;

static Options *analyse_options(Options *options, Expr *opt_list);
static Scope *analyse_declarations(Expr *defn_list);
static ChanList *analyse_assignments(Scope *scope, Expr *defn_list);
static void analyse_monitors(Scope *scope, Expr *defn_list);
static void analyse_syncs_and_syncqs(Scope *scope, Expr *defn_list);

static int option_stmt(
	struct options	*options,
	char	*name,		/* "a", "r", ... */
	char	*value		/* "+", "-" */
);
static void assign_subscr(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*subscript,	/* subscript value or 0 */
	char		*db_name
);
static void assign_single(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*db_name
);
static void assign_list(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	Expr		*db_name_list
);
static void monitor_stmt(
	Var		*vp,
	Expr		*dp,
	char		*subscript	/* element number or 0 */
);
static void sync_stmt(
	Scope		*scope,
	Expr		*dp,
	char		*subscript,
	char		*ef_name
);
static void syncq_stmt(
	Scope		*scope,
	Expr		*dp,
	char		*subscript,
	char		*ef_name,
	char		*maxQueueSize
);

typedef struct connect_var_args {
	Scope *scope;
	int opt_warn;
} connect_var_args;

static Chan *build_channel(ChanList *chan_list, Var *vp);
static void alloc_channel_lists(Chan *cp, int length);
static void add_chan(ChanList *chan_list, Chan *cp);
static void reconcile_variables(Scope *scope, int opt_warn, Expr *expr_list);
static void reconcile_states(Expr *ss_list);
static void connect_variable(Expr *ep, connect_var_args *args);
static int db_queue_count(Scope *scope);
static int db_chan_count(ChanList *chan_list);

void analyse_program(Program *p, Options *os)
{
	Expr	*defn_list = p->global_defn_list;

#ifdef	DEBUG
	report("---- Analysis ----\n");
#endif	/*DEBUG*/

	p->options = analyse_options(os, defn_list);
	p->global_scope = analyse_declarations(defn_list);
	p->chan_list = analyse_assignments(p->global_scope, defn_list);
	analyse_monitors(p->global_scope, defn_list);
	analyse_syncs_and_syncqs(p->global_scope, defn_list);

	/* Count number of db channels and state sets defined */
	p->num_queues = db_queue_count(p->global_scope);
	p->num_channels = db_chan_count(p->chan_list);
	p->num_ss = expr_count(p->ss_list);

	/* Reconcile all variable and tie each to the appropriate var struct */
	reconcile_variables(p->global_scope, p->options->warn, p->ss_list);
	reconcile_variables(p->global_scope, p->options->warn, p->entry_code_list);
	reconcile_variables(p->global_scope, p->options->warn, p->exit_code_list);

	/* reconcile all state names, including next state in transitions */
	reconcile_states(p->ss_list);
}

static Options *analyse_options(Options *options, Expr *defn_list)
{
	Expr *def;

	for (def = defn_list; def != 0; def = def->next)
	{
		if (def->type == E_OPTION)
		{
			char *name = def->value;
			char *value = def->left->value;
			assert(def->left->type == E_X);
			if (!option_stmt(options, name, value))
				report_at_expr(def, "warning: unknown option %s\n", name);
		}
	}
	return options;
}

static Scope *analyse_declarations(Expr *defn_list)
{
	Expr *dp;
	Var *vp, *vp2;
	Scope *scope;

#ifdef	DEBUG
	report("-- Declarations --\n");
#endif	/*DEBUG*/
	scope = allocScope();
	scope->var_list = allocVarList();
	for (dp = defn_list; dp != 0; dp = dp->next)
	{
		if (dp->type != E_DECL)
			continue;
		vp = (Var *)dp->value;
		assert(vp != 0);
		vp->line_num = dp->line_num;
		vp2 = find_var(scope, vp->name);
		if (vp2 != 0)
		{
			report("%s:%d: ", dp->src_file, dp->line_num);
			report("assign: variable %s already declared in line %d\n",
			 vp->name, vp2->line_num);
			continue;
		}
		add_var(scope, vp);
	}
	return scope;
}

static ChanList *analyse_assignments(Scope *scope, Expr *defn_list)
{
	Expr		*dp;
	ChanList	*chan_list;

#ifdef	DEBUG
	report("-- Assignments --\n");
#endif	/*DEBUG*/
	chan_list = allocChanList();
	for (dp = defn_list; dp != 0; dp = dp->next)
	{
		char *name;
		Expr *pv_names;
		Var *vp;

		if (dp->type != E_ASSIGN)
			continue;
		name = dp->value;
		pv_names = dp->right;

		vp = find_var(scope, name);
		if (vp == 0)
		{
			report("%s:%d: ", dp->src_file, dp->line_num);
			report("assign: variable %s not declared\n", name);
			continue;
		}
		if (dp->left != 0)
		{
			assign_subscr(chan_list, dp, vp, dp->left->value,
				pv_names->value);
		}
		else if (dp->right->next == 0) {
			assign_single(chan_list, dp, vp, pv_names->value);
		}
		else
		{
			assign_list(chan_list, dp, vp, pv_names);
		}
	}
	return chan_list;
}

static void analyse_monitors(Scope *scope, Expr *defn_list)
{
	Expr *dp = defn_list;

#ifdef	DEBUG
	report("-- Monitors --\n");
#endif	/*DEBUG*/
	for (dp = defn_list; dp; dp = dp->next)
	{
		if (dp->type == E_MONITOR)
		{
			char *name = dp->value;
			char *subscript = dp->left ? dp->left->value : 0;
			Var *vp = find_var(scope, name);

			if (vp == 0)
			{
				report("%s:%d: ", dp->src_file, dp->line_num);
				report("assign: variable %s not declared\n", name);
				continue;
			}
			monitor_stmt(vp, dp, subscript);
		}
	}
}

static void analyse_syncs_and_syncqs(Scope *scope, Expr *defn_list)
{
	Expr *dp = defn_list;

#ifdef	DEBUG
	report("-- Sync and SyncQ --\n");
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
			assert(maxQueueSize==0 || dp->right->left->type==E_CONST);
			syncq_stmt(scope, dp, subscript, ef_name, maxQueueSize);
		}
	}
}

/* Option statement */
static int option_stmt(
	Options	*options,
	char	*name,		/* "a", "r", ... */
	char	*value		/* "+", "-" */
)
{
	int optval = *value == '+';

	assert(*value == '+' || *value == '-');
	switch(*name)
	{
	    case 'a':
		options->async = optval;
		break;
	    case 'c':
		options->conn = optval;
		break;
	    case 'd':
		options->debug = optval;
		break;
	    case 'e':
		options->newef = optval;
		break;
	    case 'i':
		options->init_reg = optval;
		break;
	    case 'l':
		options->line = optval;
		break;
	    case 'm':
		options->main = optval;
		break;
	    case 'r':
		options->reent = optval;
		break;
	    case 'w':
		options->warn = optval;
		break;
	    default:
	        return FALSE;
	}
	return TRUE;
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
	report("assign %s to \"%s\";\n", name, db_name);
#endif	/*DEBUG*/

	cp = vp->chan;
	if (cp != 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("assign: %s already assigned\n", name);
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
	char		*subscript,	/* subscript value or 0 */
	char		*db_name
)
{
	Chan		*cp;
	char		*name = vp->name;
	int		subNum;

#ifdef	DEBUG
	report("assign %s[%s] to \"%s\";\n", name, subscript, db_name);
#endif	/*DEBUG*/

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("assign: variable %s not an array\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		/* Build structure for this channel */
		cp = build_channel(chan_list, vp);
	}
	else if (cp->db_name != 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("assign: array %s already assigned\n", name);
		return;
	}

	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= vp->length1)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("assign: subscript %s[%d] is out of range\n",
		 name, subNum);
		return;
	}

	if (cp->db_name_list == 0)
		alloc_channel_lists(cp, vp->length1); /* allocate lists */
	else if (cp->db_name_list[subNum] != 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("assign: %s[%d] already assigned\n",
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
 * the remaining elements receive 0 assignments.
 */
static void assign_list(
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
	report("assign %s to {", name);
#endif	/*DEBUG*/

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("assign: variable %s is not an array\n", name);
		return;
	}

	cp = vp->chan;
	if (cp != 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("assign: variable %s already assigned\n", name);
		return;
	}

	/* Build a db structure for this variable */
	cp = build_channel(chan_list, vp);

	/* Allocate lists */
	alloc_channel_lists(cp, vp->length1); /* allocate lists */

	/* fill in the array of pv names */
	for (elem_num = 0; elem_num < vp->length1; elem_num++)
	{
		if (db_name_list == 0)
			break; /* end of list */

#ifdef	DEBUG
		report("\"%s\", ", db_name_list->value);
#endif	/*DEBUG*/
		cp->db_name_list[elem_num] = db_name_list->value; /* DB name */
		cp->count = vp->length2;
 
		db_name_list = db_name_list->next;
	}
#ifdef	DEBUG
		report("};\n");
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

static void monitor_stmt(
	Var		*vp,
	Expr		*dp,
	char		*subscript	/* element number or 0 */
)
{
	Chan		*cp;
	int		subNum;
	char		*name = vp->name;

#ifdef	DEBUG
	report("monitor %s", name);
	if (subscript != 0) report("[%s]", subscript);
	report("\n");
#endif	/*DEBUG*/

	/* Find a channel assigned to this variable */
	cp = vp->chan;
	if (cp == 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("monitor: variable %s not assigned\n", name);
		return;
	}

	if (subscript == 0)
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

	/* subscript != 0 */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("monitor: subscript of %s out of range\n", name);
		return;
	}

	if (cp->num_elem == 0 || cp->db_name_list[subNum] == 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("monitor: %s[%d] not assigned\n",
		 name, subNum);
		return;
	}

	cp->mon_flag_list[subNum] = TRUE;
	return;
}

static void sync_stmt(
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
	report("sync %s%s%s%s to %s\n", name,
	 subscript?"[":"", subscript?subscript:"", subscript?"]":"", ef_name);
#endif	/*DEBUG*/

	vp = find_var(scope, name);
	if (vp == 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("sync: variable %s not declared\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("sync: variable %s not assigned\n", name);
		return;
	}

	/* Find the event flag varible */
	vp = find_var(scope, ef_name);
	if (vp == 0 || vp->type != V_EVFLAG)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("sync: e-f variable %s not declared\n",
		 ef_name);
		return;
	}

	if (subscript == 0)
	{	/* no subscript */
		if (cp->db_name != 0)
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

	/* subscript != 0 */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("sync: subscript %s[%d] out of range\n",
		 name, subNum);
		return;
	}
	cp->ef_var_list[subNum] = vp; /* sync to a specific element of the array */

	return;
}

static void syncq_stmt(
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
	report("syncq_stmt: name=%s, subNum=%s, ef_name=%s, "
	    "maxQueueSize=%s\n", name, subscript, ef_name, maxQueueSize);
#endif	/*DEBUG*/

	/* Find the variable and check it's assigned */
	vp = find_var(scope, name);
	if (vp == 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("syncQ: variable %s not declared\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("syncQ: variable %s not assigned\n", name);
		return;
	}

	/* Check that the variable has not already been syncQ'd */
	if (vp->queued)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("syncQ: variable %s already syncQ'd\n", name);
		return;
	}

	/* Find the event flag variable */
	efp = find_var(scope, ef_name);
	if (efp == 0 || efp->type != V_EVFLAG)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("syncQ: e-f variable %s not declared\n", ef_name);
		return;
	}

	/* Check that the event flag has not already been syncQ'd */
	if (efp->queued)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("syncQ: e-f variable %s already syncQ'd\n", ef_name);
		return;
	}

	/* Note queued (for both variable and event flag) and set the
	   maximum queue size (0 means default) */
	vp->queued = efp->queued = TRUE;
	vp->maxQueueSize = (maxQueueSize == 0) ? 0 : atoi(maxQueueSize);

	if (subscript == 0)
	{	/* no subscript */
		if (cp->db_name != 0)
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

	/* subscript != 0 */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		report("%s:%d: ", dp->src_file, dp->line_num);
		report("syncQ: subscript %s[%d] out of range\n",
		 name, subNum);
		return;
	}
	cp->ef_var_list[subNum] = efp; /* sync to a specific element of the array */
}

/* Add a channel to the channel linked list */
static void add_chan(ChanList *chan_list, Chan *cp)
{
	if (chan_list->first == 0)
		chan_list->first = cp;
	else
		chan_list->last->next = cp;
	chan_list->last = cp;
	cp->next = 0;
}

/* Add a variable to a scope; (append to the end of the list */
void add_var(Scope *scope, Var *vp)
{
	VarList	*var_list = scope->var_list;

	if (var_list->first == 0)
		var_list->first = vp;
	else
		var_list->last->next = vp;
	var_list->last = vp;
	vp->next = 0;
}

/* Find a variable by name, given a scope; first searches the given
   scope, then the parent scope, and so on. Returns a pointer to the
   var struct or 0 if the variable is not found. */
Var *find_var(Scope *scope, char *name)
{
	Var	*vp;

	for ( ; scope != 0; scope = scope->next)
	{
		for (vp = scope->var_list->first; vp != 0; vp = vp->next)
		{
			if (strcmp(vp->name, name) == 0)
				return vp;
		}
	}
	return 0;
}

/* Count the number of linked expressions */
int expr_count(Expr *ep)
{
	int		count;

	for (count = 0; ep != 0; ep = ep->next)
		count++;
	return count;
}

/* Sets vp->queueIndex for each syncQ'd variable, & returns number of
 * syncQ queues defined. 
 */
static int db_queue_count(Scope *scope)
{
	int		nqueue;
	Var		*vp;

	nqueue = 0;
	for (vp = scope->var_list->first; vp != NULL; vp = vp->next)
	{
		if (vp->type != V_EVFLAG && vp->queued)
		{
			vp->queueIndex = nqueue;
			nqueue++;
		}
	}

	return nqueue;
}

/* Sets cp->index for each variable, & returns number of db channels defined. 
 */
static int db_chan_count(ChanList *chan_list)
{
	int	nchan;
	Chan	*cp;

	nchan = 0;
	for (cp = chan_list->first; cp != NULL; cp = cp->next)
	{
		cp->index = nchan;
		if (cp->num_elem == 0)
			nchan += 1;
		else
			nchan += cp->num_elem; /* array with multiple channels */
	}

	return nchan;
}

static void reconcile_variables(Scope *scope, int opt_warn, Expr *expr_list)
{
	Expr *ep;
	connect_var_args cv_args = { scope, opt_warn };

	for (ep = expr_list; ep != 0; ep = ep->next)
	{
		traverse_expr_tree(ep, E_VAR, 0, (expr_fun*)connect_variable, &cv_args);
	}
}

/* Traverse the expression tree, and call the supplied
 * function whenever type = ep->type AND value matches ep->value.
 * The condition value = 0 matches all.
 * The function is called with the current ep and a supplied argument (argp) */
void traverse_expr_tree(
	Expr	    *ep,	/* ptr to start of expression */
	int	    type,	/* to search for */
	char	    *value,	/* with optional matching value */
	expr_fun    *funcp,	/* function to call */
	void	    *argp	/* ptr to argument to pass on to function */
)
{
	Expr		*ep1;

	if (ep == 0)
		return;

#if 0
	if (printTree)
		report("traverse_expr_tree: type=%s, value=%s\n",
		 expr_type_names[ep->type], ep->value);
#endif

	/* Call the function? */
	if ((ep->type == type) && (value == 0 || strcmp(ep->value, value) == 0) )
	{
		funcp(ep, argp);
	}

	/* Continue traversing the expression tree */
	switch(ep->type)
	{
	case E_VAR:
	case E_CONST:
	case E_STRING:
	case E_TEXT:
	case E_BREAK:
		break;

	case E_PAREN:
	case E_SS:
	case E_STATE:
	case E_FUNC:
	case E_CMPND:
	case E_STMT:
	case E_ELSE:
	case E_PRE:
	case E_POST:
		for (ep1 = ep->left; ep1 != 0;	ep1 = ep1->next)
		{
			traverse_expr_tree(ep1, type, value, funcp, argp);
		}
		break;

	case E_DECL:
	case E_WHEN:
        case E_ENTRY:  /* E_ENTRY and E_EXIT only have expressions on RHS (->right) */
        case E_EXIT:   /* but add them here incase ->left is used in future. */
	case E_BINOP:
	case E_TERNOP:
	case E_SUBSCR:
	case E_IF:
	case E_WHILE:
	case E_FOR:
	case E_X:
		for (ep1 = ep->left; ep1 != 0;	ep1 = ep1->next)
		{
			traverse_expr_tree(ep1, type, value, funcp, argp);
		}
		for (ep1 = ep->right; ep1 != 0;	ep1 = ep1->next)
		{
			traverse_expr_tree(ep1, type, value, funcp, argp);
		}
		break;

	default:
		report("traverse_expr_tree: type=%s ???\n", expr_type_names[ep->type]);
	}
}

/* Connect a variable in an expression to the the Var structure */
static void connect_variable(Expr *ep, connect_var_args *args)
{
	Scope *scope = args->scope;
	int opt_warn = args->opt_warn;
	Var *vp;

	if (ep->type != E_VAR)
		return;
#ifdef	DEBUG
	report("connect_variable: \"%s\", line %d\n", ep->value, ep->line_num);
#endif	/*DEBUG*/
	vp = find_var(scope, ep->value);
#ifdef	DEBUG
	report("\t \"%s\" was %s\n", ep->value, vp ? "found" : "not found" );
#endif	/*DEBUG*/
	if (vp == 0)
	{	/* variable not declared; add it to the variable list */
		if (opt_warn)
			report(
			 "Warning:  variable \"%s\" is used but not declared.\n",
			 ep->value);
		vp = allocVar();
		add_var(scope, vp);
		vp->name = ep->value;
		vp->type = V_NONE; /* undeclared type */
		vp->length1 = 1;
		vp->length2 = 1;
		vp->value = 0;
	}
	ep->left = (Expr *)vp; /* make connection */

	return;
} 

/* Reconcile state names */
static void reconcile_states(Expr *ss_list)
{
	Expr		*ssp, *sp, *sp1;

	for (ssp = ss_list; ssp != 0; ssp = ssp->next)
	{
	    for (sp = ssp->left; sp != 0; sp = sp->next)
	    {
		/* Check for duplicate state names in this state set */
		for (sp1 = sp->next; sp1 != 0; sp1 = sp1->next)
		{
			if (strcmp(sp->value, sp1->value) == 0)
			{
			    report(
			       "State \"%s\" is duplicated in state set \"%s\"\n",
			       sp->value, ssp->value);
			}
		}		
	    }
	}
}
