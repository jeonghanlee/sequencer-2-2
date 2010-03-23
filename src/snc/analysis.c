#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "sym_table.h"
#include "snc_main.h"
#include "analysis.h"

/* #define DEBUG */

static const int impossible = 0;

static void analyse_definitions(Program *p);
static void analyse_option(Options *options, Expr *defn);
static void analyse_state_option(StateOptions *options, Expr *defn);
static void analyse_declaration(SymTable st, Expr *scope, Expr *defn);
static void analyse_assignment(SymTable st, ChanList *chan_list, Expr *scope, Expr *defn);
static void analyse_monitor(SymTable st, Expr *scope, Expr *defn);
static void analyse_sync(SymTable st, Expr *scope, Expr *defn, int *num_queues);
static void analyse_syncq(int *num_queues, SymTable st, Expr *scope, Expr *defn);
static void assign_subscript(ChanList *chan_list, Expr *dp, Var *vp, char *subscr, char *pv_name);
static void assign_single(ChanList *chan_list, Expr *dp, Var *vp, char *pv_name);
static void assign_list(ChanList *chan_list, Expr *dp, Var *vp, Expr *pv_name_list);
static Chan *new_channel(ChanList *chan_list, Var *vp);
static void alloc_channel_lists(Chan *cp, int length);
static void connect_variables(SymTable st, Expr *scope);
static void connect_state_change_stmts(SymTable st, Expr *scope);
static int connect_states(SymTable st, Expr *ss_list);
static int pv_chan_count(ChanList *chan_list);
static void add_var(SymTable st, Var *vp, Expr *scope);
static Var *find_var(SymTable st, char *name, Expr *scope);
static int assign_ef_bits(Expr *scope, ChanList *chan_list);

Program *analyse_program(Expr *prog, Options options)
{
	assert(prog != 0);
#ifdef	DEBUG
	report("-------------------- Analysis --------------------\n");
#endif	/*DEBUG*/

	Program *p = new(Program);

	assert(p != 0);

	p->options = options;
	p->prog = prog;

	p->name			= prog->value;
	if (prog->prog_param)
		p->param	= prog->prog_param->value;
	else
		p->param	= "";

	p->sym_table = sym_table_create();

#ifdef	DEBUG
	report("created symbol table\n");
#endif	/*DEBUG*/

	p->chan_list = new(ChanList);

#ifdef	DEBUG
	report("created channel list\n");
#endif	/*DEBUG*/

	analyse_definitions(p);

	p->num_channels = pv_chan_count(p->chan_list);
	p->num_ss = connect_states(p->sym_table, prog);

	connect_variables(p->sym_table, prog);

	connect_state_change_stmts(p->sym_table, prog);

	/* Assign bits for event flags */
	p->num_event_flags = assign_ef_bits(p->prog, p->chan_list);

	return p;
}

int analyse_defn(Expr *scope, Expr *parent_scope, void *parg)
{
	Program	*p = (Program *)parg;

	assert(scope != 0);

#ifdef	DEBUG
	report("analyse_defn: scope=(%s:%s)\n",
		expr_type_name(scope), scope->value);
#endif	/*DEBUG*/

	assert(is_scope(scope));
	assert(parent_scope == 0 || is_scope(parent_scope));

	Expr *defn_list = defn_list_from_scope(scope);
	VarList **pvar_list = pvar_list_from_scope(scope);

	/* NOTE: We always need to allocate a var_list, even if there are no
	   definitions on this level, so later on (see connect_variables below)
	   we can traverse in the other direction to find the nearest enclosing
	   scope. See connect_variables below. */
	if (*pvar_list == 0)
	{
		*pvar_list = new(VarList);
		(*pvar_list)->parent_scope = parent_scope;
	}

	Expr *defn;

	foreach (defn, defn_list)
	{
		switch (defn->type)
		{
		case D_OPTION:
			if (scope->type == D_PROG)
				analyse_option(&p->options, defn);
			else if (scope->type == D_STATE)
			{
				analyse_state_option(&scope->extra.e_state->options, defn);
			}
			break;
		case D_DECL:
			analyse_declaration(p->sym_table, scope, defn);
			break;
		case D_ASSIGN:
			analyse_assignment(p->sym_table, p->chan_list, scope, defn);
			break;
		case D_MONITOR:
			analyse_monitor(p->sym_table, scope, defn);
			break;
		case D_SYNC:
			analyse_sync(p->sym_table, scope, defn, &p->num_queues);
			break;
		case D_SYNCQ:
			analyse_syncq(&p->num_queues, p->sym_table, scope, defn);
			break;
		case T_TEXT:
			break;
		default:
			assert(impossible);
		}
	}
	return TRUE; /* always descend into children */
}

VarList **pvar_list_from_scope(Expr *scope)
{
	assert(is_scope(scope));
	switch(scope->type)
	{
	case D_PROG:
		return &scope->extra.e_prog;
	case D_SS:
		assert(scope->extra.e_ss);
		return &scope->extra.e_ss->var_list;
	case D_STATE:
		assert(scope->extra.e_state);
		return &scope->extra.e_state->var_list;
	case D_WHEN:
		assert(scope->extra.e_when);
		return &scope->extra.e_when->var_list;
	case D_ENTRY:
		return &scope->extra.e_entry;
	case D_EXIT:
		return &scope->extra.e_exit;
	case S_CMPND:
		return &scope->extra.e_cmpnd;
	default:
		assert(impossible);
	}
}

Expr *defn_list_from_scope(Expr *scope)
{
	assert(is_scope(scope));
	switch(scope->type)
	{
	case D_PROG:
		return scope->prog_defns;
	case D_SS:
		return scope->ss_defns;
	case D_STATE:
		return scope->state_defns;
	case D_WHEN:
		return scope->when_defns;
	case D_ENTRY:
		return scope->entry_defns;
	case D_EXIT:
		return scope->exit_defns;
	case S_CMPND:
		return scope->cmpnd_defns;
	default:
		assert(impossible);
	}
}

static void analyse_definitions(Program *p)
{
#ifdef	DEBUG
	report("**begin** analyse definitions\n");
#endif	/*DEBUG*/

	traverse_expr_tree(p->prog, scope_mask, ~has_sub_scope_mask, 0, analyse_defn, p);

#ifdef	DEBUG
	report("**end** analyse definitions\n");
#endif	/*DEBUG*/
}

/* Options at the top-level. Note: latest given value for option wins. */
static void analyse_option(Options *options, Expr *defn)
{
	char	*optname = defn->value;
	int	optval = defn->extra.e_option;

	for (; *optname; optname++)
	{
		switch(*optname)
		{
		case 'a': options->async = optval; break;
		case 'c': options->conn = optval; break;
		case 'd': options->debug = optval; break;
		case 'e': options->newef = optval; break;
		case 'i': options->init_reg = optval; break;
		case 'l': options->line = optval; break;
		case 'm': options->main = optval; break;
		case 'r': options->reent = optval; break;
		case 'w': options->warn = optval; break;
		default: report_at_expr(defn,
		  "warning: unknown option '%s'\n", optname);
		}
	}
}

/* Options in state declarations. Note: latest given value for option wins. */
static void analyse_state_option(StateOptions *options, Expr *defn)
{
	char	*optname = defn->value;
	int	optval = defn->extra.e_option;

	for (; *optname; optname++)
	{
		switch(*optname)
		{
		case 't': options->do_reset_timers = optval; break;
		case 'e': options->no_entry_from_self = optval; break;
		case 'x': options->no_exit_to_self = optval; break;
		default: report_at_expr(defn,
		  "warning: unknown state option '%s'\n", optname);
		}
	}
}

static void analyse_declaration(SymTable st, Expr *scope, Expr *defn)
{
	Var *vp;

	vp = defn->extra.e_decl;

	assert(vp != 0);
#ifdef	DEBUG
	report("declaration: %s\n", vp->name);
#endif	/*DEBUG*/

	VarList *var_list = *pvar_list_from_scope(scope);

	if (!sym_table_insert(st, vp->name, var_list, vp))
	{
		Var *vp2 = sym_table_lookup(st, vp->name, var_list);
		error_at_expr(defn,
			"variable '%s' already declared at %s:%d\n",
			vp->name, vp2->decl->src_file, vp2->decl->line_num);
	}
	else
	{
		add_var(st, vp, scope);
	}
}

static void analyse_assignment(SymTable st, ChanList *chan_list, Expr *scope, Expr *defn)
{
	char *name = defn->value;
	Var *vp = find_var(st, name, scope);

	if (vp == 0)
	{
		error_at_expr(defn, "assign: variable '%s' not declared\n", name);
		return;
	}
	if (defn->assign_subscr != 0)
	{
		assign_subscript(chan_list, defn, vp, defn->assign_subscr->value,
			defn->assign_pvs->value);
	}
	else if (defn->assign_pvs->next == 0) {
		assign_single(chan_list, defn, vp, defn->assign_pvs->value);
	}
	else
	{
		assign_list(chan_list, defn, vp, defn->assign_pvs);
	}
}

/* "Assign" statement: Assign a variable to a channel.
 * Format: assign <variable> to <string>;
 * Note: Variable may be subscripted.
 */
static void assign_single(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*pv_name
)
{
	Chan		*cp;
	char		*name = vp->name;

#ifdef	DEBUG
	report("assign %s to %s;\n", name, pv_name);
#endif	/*DEBUG*/

	cp = vp->chan;
	if (cp != 0)
	{
		error_at_expr(dp, "variable '%s' already assigned\n", name);
		return;
	}

	/* Build structure for this channel */
	cp = new_channel(chan_list, vp);

	cp->pv_names[0] = pv_name;

	/* The entire variable is assigned */
	cp->count = vp->length1 * vp->length2;
}

/* "Assign" statement: assign an array element to a channel.
 * Format: assign <variable>[<subscr>] to <string>; */
static void assign_subscript(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	char		*subscript,	/* subscript value or 0 */
	char		*pv_name
)
{
	Chan		*cp;
	char		*name = vp->name;
	int		subNum;

#ifdef	DEBUG
	report("assign %s[%s] to '%s';\n", name, subscript, pv_name);
#endif	/*DEBUG*/

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		error_at_expr(dp, "assign: variable '%s' not an array\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		cp = new_channel(chan_list, vp);
	}
	else if (cp->pv_names[0] != 0)
	{
		error_at_expr(dp, "assign: array '%s' already assigned\n", name);
		return;
	}

	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= vp->length1)
	{
		error_at_expr(dp, "assign: subscript '%s[%d]' is out of range\n",
			name, subNum);
		return;
	}

	if (cp->pv_names[subNum] != 0)
	{
		error_at_expr(dp, "assign: '%s[%d]' already assigned\n",
			name, subNum);
		return;
	}

	cp->pv_names[subNum] = pv_name;
	cp->count = vp->length2; /* could be a 2-dimensioned array */
}

/* Assign statement: assign an array to multiple channels.
 * Format: assign <variable> to { <string>, <string>, ... };
 * Assignments for double dimensioned arrays:
 * <var>[0][0] assigned to 1st pv name,
 * <var>[1][0] assigned to 2nd pv name, etc.
 * If pv name list contains fewer names than the array dimension,
 * the remaining elements receive 0 assignments.
 */
static void assign_list(
	ChanList	*chan_list,
	Expr		*dp,
	Var		*vp,
	Expr		*pv_name_list
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
		error_at_expr(dp, "assign: variable '%s' is not an array\n", name);
		return;
	}

	cp = vp->chan;
	if (cp != 0)
	{
		error_at_expr(dp, "assign: variable '%s' already assigned\n", name);
		return;
	}

	cp = new_channel(chan_list, vp);

	/* fill in the array of pv names */
	for (elem_num = 0; elem_num < vp->length1; elem_num++)
	{
		if (pv_name_list == 0)
			break; /* end of list */

#ifdef	DEBUG
		report("'%s', ", pv_name_list->value);
#endif	/*DEBUG*/
		cp->pv_names[elem_num] = pv_name_list->value;
		cp->count = vp->length2;
 
		pv_name_list = pv_name_list->next;
	}
#ifdef	DEBUG
	report("};\n");
#endif	/*DEBUG*/
}

static void analyse_monitor(SymTable st, Expr *scope, Expr *defn)
{
	Expr	*e_subscr = defn->monitor_subscr;
	char	*name = defn->value;
	Var	*vp = find_var(st, name, scope);
	int	subNum = e_subscr ? atoi(e_subscr->value) : 0;
	Chan	*cp;

#ifdef	DEBUG
	report("monitor %s", name);
	if (e_subscr != 0) report("[%d]", subNum);
	report("\n");
#endif	/*DEBUG*/

	if (vp == 0)
	{
		error_at_expr(defn,
			"monitor: variable '%s' not declared\n", name);
		return;
	}

	/* Find a channel assigned to this variable */
	cp = vp->chan;
	if (cp == 0)
	{
		error_at_expr(defn, "monitor: variable '%s' not assigned\n", name);
		return;
	}
	if (e_subscr == 0)
	{
		/* else monitor all channels in pv list */
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{	/* 1 pv per element of the array */
			cp->mon_flags[subNum] = TRUE;
		}
		return;
	}
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		error_at_expr(defn, "monitor: subscript of '%s' out of range\n", name);
		return;
	}
	if (cp->pv_names[subNum] == 0)
	{
		error_at_expr(defn, "monitor: '%s[%d]' not assigned\n",
			name, subNum);
		return;
	}
	cp->mon_flags[subNum] = TRUE;
}

static void analyse_sync(SymTable st, Expr *scope, Expr *defn, int *num_queues)
{
	Expr	*e_subscr = defn->sync_subscr;
	Expr	*e_evflag = defn->sync_evflag;
	char	*name = defn->value;
	char	*ef_name = e_evflag->value;
	int	subNum = e_subscr ? atoi(e_subscr->value) : 0;
	Var	*vp;
	Chan	*cp;

	vp = find_var(st, name, scope);
	if (vp == 0)
	{
		error_at_expr(defn, "sync: variable '%s' not declared\n", name);
		return;
	}
	cp = vp->chan;
	if (cp == 0)
	{
		error_at_expr(defn, "sync: variable '%s' not assigned\n", name);
		return;
	}
	/* Find the event flag varible */
	vp = find_var(st, ef_name, scope);
	if (vp == 0 || vp->type != V_EVFLAG)
	{
		error_at_expr(defn, "sync: event flag '%s' not declared\n",
			ef_name);
		return;
	}
	if (e_subscr == 0)
	{
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{
			cp->ef_vars[subNum] = vp;
		}
	}
	else if (subNum < 0 || subNum >= cp->num_elem)
	{
		error_at_expr(defn, "sync: subscript '%s[%d]' out of range\n",
			name, subNum);
	}
	else
	{
		cp->ef_vars[subNum] = vp; /* sync to a specific element of the array */
	}
}

static void analyse_syncq(int *num_queues, SymTable st, Expr *scope, Expr *defn)
{
	Expr	*e_subscr = defn->syncq_subscr;
	Expr	*e_evflag = defn->syncq_evflag;
	Expr	*e_maxqsize = defn->syncq_maxqsize;
	char	*name = defn->value;
	char	*ef_name = e_evflag->value;
	char	*s_subscr = e_subscr ? e_subscr->value : 0;
	int	subNum = e_subscr ? atoi(e_subscr->value) : 0;
	Var	*vp, *efp;
	Chan	*cp;

	/* Find the variable and check it's assigned */
	vp = find_var(st, name, scope);
	if (vp == 0)
	{
		error_at_expr(defn, "syncQ: variable '%s' not declared\n", name);
		return;
	}

	cp = vp->chan;
	if (cp == 0)
	{
		error_at_expr(defn, "syncQ: variable '%s' not assigned\n", name);
		return;
	}

	/* Check that the variable has not already been syncQ'd */
	if (vp->queued)
	{
		error_at_expr(defn, "syncQ: variable '%s' already syncQ'd\n", name);
		return;
	}

	/* Find the event flag variable */
	efp = find_var(st, ef_name, scope);
	if (efp == 0 || efp->type != V_EVFLAG)
	{
		error_at_expr(defn, "syncQ: event flag %s not declared\n", ef_name);
		return;
	}

	/* Check that the event flag has not already been syncQ'd */
	if (efp->queued)
	{
		error_at_expr(defn, "syncQ: event flag %s already syncQ'd\n", ef_name);
		return;
	}

	/* Note queued (for both variable and event flag) and set the
	   maximum queue size (0 means default) */
	vp->queued = efp->queued = TRUE;
	vp->maxqsize = e_maxqsize ? atoi(e_maxqsize->value) : 0;

	vp->qindex = *num_queues;
	*num_queues += 1;

	if (e_subscr == 0)
	{
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{
			cp->ef_vars[subNum] = efp;
		}
	}
	else
	{
		subNum = atoi(s_subscr);
		if (subNum < 0 || subNum >= cp->num_elem)
		{
			error_at_expr(defn,
				"syncQ: subscript '%s[%d]' out of range\n", name, subNum);
			return;
		}
		cp->ef_vars[subNum] = efp; /* sync to a specific element of the array */
	}
}

/* Allocate a channel structure for this variable, add it to the channel list,
   and mutually connect variable and channel. */
static Chan *new_channel(ChanList *chan_list, Var *vp)
{
	Chan *cp = new(Chan);

	alloc_channel_lists(cp, vp->length1);

	/* mutually connect variable and channel */
	cp->var = vp;
	vp->chan = cp;

	/* add new channel to chan_list */
	if (chan_list->first == 0)
		chan_list->first = cp;
	else
		chan_list->last->next = cp;
	chan_list->last = cp;
	cp->next = 0;

	return cp;
}

/* Allocate lists for assigning multiple pv's to a variable */
static void alloc_channel_lists(Chan *cp, int length)
{
	/* allocate an array of pv names */
	cp->pv_names = (char **)calloc(sizeof(char **), length);

	/* allocate an array for monitor flags */
	cp->mon_flags = (int *)calloc(sizeof(int **), length);

	/* allocate an array for event flag var ptrs */
	cp->ef_vars = (Var **)calloc(sizeof(Var **), length);

	/* allocate an array for event flag numbers */
	cp->ef_nums = (int *)calloc(sizeof(int **), length);

	cp->num_elem = length;
}

/* Add a variable to a scope (append to the end of the var_list) */
void add_var(SymTable st, Var *vp, Expr *scope)
{
	VarList	*var_list = *pvar_list_from_scope(scope);

	if (var_list->first == 0)
		var_list->first = vp;
	else
		var_list->last->next = vp;
	var_list->last = vp;
	vp->next = 0;

	vp->scope = scope;
}

/* Find a variable by name, given a scope; first searches the given
   scope, then the parent scope, and so on. Returns a pointer to the
   var struct or 0 if the variable is not found. */
Var *find_var(SymTable st, char *name, Expr *scope)
{
	VarList *var_list = *pvar_list_from_scope(scope);
	Var	*vp;

#ifdef DEBUG
	report("searching %s in %s:%s, ", name, scope->value,
		expr_type_name(scope));
#endif
	vp = sym_table_lookup(st, name, var_list);
	if (vp)
	{
#ifdef DEBUG
		report("found\n");
#endif
		return vp;
	}
	else if (var_list->parent_scope == 0)
	{
#ifdef DEBUG
		report("not found\n");
#endif
		return 0;
	}
	else
		return find_var(st, name, var_list->parent_scope);
}

/* Sets cp->index for each variable, & returns number of channels defined. 
 */
static int pv_chan_count(ChanList *chan_list)
{
	int	nchan;
	Chan	*cp;

	nchan = 0;
	foreach (cp, chan_list->first)
	{
		cp->index = nchan;
		nchan += cp->num_elem; /* array with multiple channels */
	}
	return nchan;
}

/* Connect a variable in an expression (E_VAR) to the Var structure.
   If there is no such structure, e.g. because the variable has not been
   declared, then allocate one, assign type V_NONE, and assign the most
   local scope for the variable. */
static int connect_variable(Expr *ep, Expr *scope, void *parg)
{
	SymTable	st = *(SymTable *)parg;
	Var		*vp;

	assert(ep);
	assert(ep->type == E_VAR);
	assert(scope);

#ifdef	DEBUG
	report("connect_variable: %s, line %d\n", ep->value, ep->line_num);
#endif	/*DEBUG*/

	vp = find_var(st, ep->value, scope);

#ifdef	DEBUG
	if (vp)
	{
		report_at_expr(ep, "var %s found in scope (%s:%s)\n", ep->value,
			expr_type_name(scope),
			scope->value);
	}
	else
		report_at_expr(ep, "var %s not found\n", ep->value);
#endif	/*DEBUG*/
	if (vp == 0)
	{
		VarList *var_list = *pvar_list_from_scope(scope);

		error_at_expr(ep, "variable '%s' used but not declared\n",
			ep->value);
		/* create a pseudo declaration so we can finish the analysis phase */
		vp = new(Var);
		vp->name = ep->value;
		vp->type = V_NONE;	/* undeclared type */
		vp->length1 = 1;
		vp->length2 = 1;
		vp->value = 0;
		sym_table_insert(st, vp->name, var_list, vp);
		add_var(st, vp, scope);
	}
	ep->extra.e_var = vp;		/* make connection */
	return FALSE;			/* there are no children anyway */
}

static void connect_variables(SymTable st, Expr *scope)
{
#ifdef	DEBUG
	report("**begin** connect_variables\n");
#endif	/*DEBUG*/
	traverse_expr_tree(scope, 1<<E_VAR, ~has_sub_expr_mask,
		0, connect_variable, &st);
#ifdef	DEBUG
	report("**end** connect_variables\n");
#endif	/*DEBUG*/
}

void traverse_expr_tree(
	Expr		*ep,		/* start expression */
	int		call_mask,	/* when to call iteratee */
	int		stop_mask,	/* when to stop descending */
	Expr		*scope,		/* current scope, 0 at top-level */
	expr_iter	*iteratee,	/* function to call */
	void		*parg		/* argument to pass to function */
)
{
	Expr	*cep;
	int	i;
	int	descend = TRUE;

	if (ep == 0)
		return;

#ifdef DEBUG
	report("traverse_expr_tree(type=%s,value=%s)\n",
		expr_type_name(ep), ep->value);
#endif

	/* Call the function? */
	if (call_mask & (1<<ep->type))
	{
		descend = iteratee(ep, scope, parg);
	}

	if (!descend)
		return;

	/* Are we just entering a new scope? */
	if (is_scope(ep))
	{
#ifdef DEBUG
	report("traverse_expr_tree: new scope=(%s,%s)\n",
		expr_type_name(ep), ep->value);
#endif
		scope = ep;
	}

	/* Descend into children */
	for (i = 0; i < expr_type_info[ep->type].num_children; i++)
	{
		foreach (cep, ep->children[i])
		{
			if (cep && !((1<<cep->type) & stop_mask))
			{
				traverse_expr_tree(cep, call_mask, stop_mask,
					scope, iteratee, parg);
			}
		}
	}
}

static int assign_next_delay_id(Expr *ep, Expr *scope, void *parg)
{
	int *delay_id = (int *)parg;

	assert(ep->type == E_DELAY);
	ep->extra.e_delay = *delay_id;
	*delay_id += 1;
	return FALSE;	/* delays cannot be nested as they do not return a value */
}

/* Check for duplicate state set and state names and resolve transitions between states */
static int connect_states(SymTable st, Expr *prog)
{
	Expr	*ssp;
	int	num_ss = 0;

	foreach (ssp, prog->prog_statesets)
	{
		Expr *sp;
		int num_states = 0;

#ifdef	DEBUG
		report("connect_states: ss = %s\n", ssp->value);
#endif	/*DEBUG*/
		if (!sym_table_insert(st, ssp->value, prog, ssp))
		{
			Expr *ssp2 = sym_table_lookup(st, ssp->value, prog);
			error_at_expr(ssp,
				"a state set with name '%s' was already "
				"declared at line %d\n", ssp->value, ssp2->line_num);
		}
		foreach (sp, ssp->ss_states)
		{
			if (!sym_table_insert(st, sp->value, ssp, sp))
			{
				Expr *sp2 = sym_table_lookup(st, sp->value, ssp);
				error_at_expr(sp,
					"a state with name '%s' in state set '%s' "
					"was already declared at line %d\n",
					sp->value, ssp->value, sp2->line_num);
			}
			assert(sp->extra.e_state);
#ifdef	DEBUG
			report("connect_states: ss = %s, state = %s, index = %d\n",
				ssp->value, sp->value, num_states);
#endif	/*DEBUG*/
			sp->extra.e_state->index = num_states++;
		}
		ssp->extra.e_ss->num_states = num_states;
#ifdef	DEBUG
		report("connect_states: ss = %s, num_states = %d\n", ssp->value, num_states);
#endif	/*DEBUG*/
		foreach (sp, ssp->ss_states)
		{
			Expr *tp;
			/* Each state has its own delay ids */
			int delay_id = 0;

			foreach (tp, sp->state_whens)
			{
				Expr *next_sp = sym_table_lookup(st, tp->value, ssp);

				if (next_sp == 0)
				{
					error_at_expr(tp,
						"a state with name '%s' does not "
						"exist in state set '%s'\n",
					 	tp->value, ssp->value);
				}
				tp->extra.e_when->next_state = next_sp;
				assert(!next_sp || strcmp(tp->value,next_sp->value) == 0);
#ifdef	DEBUG
				report("connect_states: ss = %s, state = %s, when(...){...} state (%s,%d)\n",
					ssp->value, sp->value, tp->value, next_sp->extra.e_state->index);
#endif	/*DEBUG*/
				/* assign delay ids */
				traverse_expr_tree(tp->when_cond, 1<<E_DELAY, 0, 0,
					assign_next_delay_id, &delay_id);
			}
		}
		num_ss++;
	}
	return num_ss;
}

typedef struct {
	SymTable	st;
	Expr		*ssp;
	int		in_when;
} connect_state_change_arg;

static int iter_connect_state_change_stmts(Expr *ep, Expr *scope, void *parg)
{
	connect_state_change_arg *pcsc_arg = (connect_state_change_arg *)parg;

	assert(pcsc_arg != 0);
	assert(ep != 0);
	if (ep->type == D_SS)
	{
		pcsc_arg->ssp = ep;
		return TRUE;
	}
	else if (ep->type == D_ENTRY || ep->type == D_EXIT)
	{
		/* to flag erroneous state change statements, see below */
		pcsc_arg->in_when = FALSE;
		return TRUE;
	}
	else if (ep->type == D_WHEN)
	{
		pcsc_arg->in_when = TRUE;
		return TRUE;
	}
	else
	{
		assert(ep->type == S_CHANGE);
		if (!pcsc_arg->ssp || !pcsc_arg->in_when)
		{
			error_at_expr(ep, "state change statement not allowed here\n");
		}
		else
		{
			Expr *sp = sym_table_lookup(pcsc_arg->st, ep->value, pcsc_arg->ssp);
			if (sp == 0)
			{
				error_at_expr(ep,
					"a state with name '%s' does not "
					"exist in state set '%s'\n",
				 	ep->value, pcsc_arg->ssp->value);
				return FALSE;
			}
			ep->extra.e_change = sp;
		}
		return FALSE;
	}
}

static void connect_state_change_stmts(SymTable st, Expr *scope)
{
	connect_state_change_arg csc_arg = {st, 0, FALSE};
	traverse_expr_tree(scope,
		(1<<S_CHANGE)|(1<<D_SS)|(1<<D_ENTRY)|(1<<D_EXIT)|(1<<D_WHEN),
		expr_mask, 0, iter_connect_state_change_stmts, &csc_arg);
}

/* Assign event bits to event flags and associate pv channels with
 * event flags. Return number of event flags found.
 */
static int assign_ef_bits(Expr *scope, ChanList *chan_list)
{
	Var	*vp;
	Chan	*cp;
	int	n;
	int	num_event_flags;
	VarList	*var_list;

	var_list = *pvar_list_from_scope(scope);

	/* Assign event flag numbers (starting at 1) */
	num_event_flags = 0;
	foreach (vp, var_list->first)
	{
		if (vp->type == V_EVFLAG)
		{
			num_event_flags++;
			vp->ef_num = num_event_flags;
		}
	}
	/* Associate event flags with channels */
	foreach (cp, chan_list->first)
	{
		for (n = 0; n < cp->num_elem; n++)
		{
			vp = cp->ef_vars[n];
			if (vp != NULL)
				cp->ef_nums[n] = vp->ef_num;
		}
	}
	return num_event_flags;
}
