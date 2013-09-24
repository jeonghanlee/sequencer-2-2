/*************************************************************************\
Copyright (c) 1990      The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                State set code generation
\*************************************************************************/
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "expr.h"
#include "analysis.h"
#include "gen_code.h"
#include "main.h"
#include "builtin.h"
#include "gen_ss_code.h"

static const int impossible = 0;

static void gen_local_var_decls(Expr *scope, int level);
static void gen_state_func(
	const char *ss_name,
	uint ss_num,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *xp),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
);
static void gen_entex_body(Expr *xp);
static void gen_delay_body(Expr *xp);
static void gen_event_body(Expr *xp);
static void gen_action_body(Expr *xp);
static int gen_delay(Expr *ep, Expr *scope, void *parg);
static void gen_expr(int context, Expr *ep, int level);
static void gen_ef_func(int context, Expr *ep, const char *func_name, uint ef_action_only);
static void gen_pv_func(int context, Expr *ep,
	const char *func_name, uint add_length, uint num_params, uint ef_args);
static int gen_builtin_func(int context, Expr *ep);
static int gen_builtin_const(Expr *ep);

static void gen_prog_func(
	Expr *prog,
	const char *name,
	const char *doc,
	Expr *xp,
	void (*gen_body)(Expr *xp));
static void gen_prog_init_func(Expr *prog);

/*
 * Expression context. Certain nodes of the syntax tree are
 * interpreted differently depending on the context in which
 * they appear. For instance, the state change command is only
 * allowed in transition action context (C_TRANS).
 */
enum expr_context
{
	C_COND,		/* when() condition */
	C_TRANS,	/* state transition actions */
	C_ENTEX,	/* entry or exit block */
	C_INIT		/* variable initialization */
};

/*
 * HACK: use global variables to make program options and
 * symbol table available to subroutines
 */
static int global_opt_reent;
static SymTable global_sym_table;

void init_gen_ss_code(Program *program)
{
	/* HACK: intialise globals */
	global_opt_reent = program->options.reent;
	global_sym_table = program->sym_table;
}

/* Generate state set C code from analysed syntax tree */
void gen_ss_code(Program *program)
{
	Expr	*prog = program->prog;
	Expr	*ssp;
	Expr	*sp;
	uint	ss_num = 0;

	/* Generate program init func */
	gen_prog_init_func(prog);

	/* Generate program entry func */
	if (prog->prog_entry)
		gen_prog_func(prog, NM_ENTRY, "entry", prog->prog_entry, gen_entex_body);

	/* For each state set ... */
	foreach (ssp, prog->prog_statesets)
	{
		/* For each state ... */
		foreach (sp, ssp->ss_states)
		{
			gen_code("\n/****** Code for state \"%s\" in state set \"%s\" ******/\n",
				sp->value, ssp->value);

			/* Generate entry and exit functions */
			if (sp->state_entry)
				gen_state_func(ssp->value, ss_num, sp->value, 
					sp->state_entry, gen_entex_body,
					"Entry", NM_ENTRY, "void", "");
			if (sp->state_exit)
				gen_state_func(ssp->value, ss_num, sp->value,
					sp->state_exit, gen_entex_body,
					"Exit", NM_EXIT, "void", "");
			/* Generate function to set up for delay processing */
			gen_state_func(ssp->value, ss_num, sp->value,
				sp->state_whens, gen_delay_body,
				"Delay", NM_DELAY, "void", "");
			/* Generate event processing function */
			gen_state_func(ssp->value, ss_num, sp->value,
				sp->state_whens, gen_event_body,
				"Event", NM_EVENT, "seqBool",
				", int *" NM_PTRN ", int *" NM_PNST);
			/* Generate action processing function */
			gen_state_func(ssp->value, ss_num, sp->value,
				sp->state_whens, gen_action_body,
				"Action", NM_ACTION, "void",
				", int " NM_TRN ", int *" NM_PNST);
		}
		ss_num++;
	}

	/* Generate program exit func */
	if (prog->prog_exit)
		gen_prog_func(prog, NM_EXIT, "exit", prog->prog_exit, gen_entex_body);
}

/* Generate a local C variable declaration for each variable declared
   inside the body of an entry, exit, when, or compound statement block. */
static void gen_local_var_decls(Expr *scope, int level)
{
	Var	*vp;
	VarList	*var_list;

	var_list = var_list_from_scope(scope);

	/* Convert internal type to `C' type */
	foreach (vp, var_list->first)
	{
		assert(vp->type->tag != T_NONE);
		assert(vp->type->tag != T_EVFLAG);
		assert(vp->decl);

		gen_line_marker(vp->decl);
		indent(level);
		gen_var_decl(vp);

		/* optional initialisation */
		if (vp->init)
		{
			gen_code(" = ");
			gen_expr(C_INIT, vp->init, level);
		}
		gen_code(";\n");
	}
}

static void gen_state_func(
	const char *ss_name,
	uint ss_num,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *xp),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
)
{
	gen_code("\n/* %s function for state \"%s\" in state set \"%s\" */\n",
		title, state_name, ss_name);
	gen_code("static %s %s_%s_%d_%s(SS_ID " NM_SS ", SEQ_VARS *const " NM_VARS "%s)\n{\n",
		rettype, prefix, ss_name, ss_num, state_name, extra_args);
	gen_body(xp);
	gen_code("}\n");
}

static void gen_entex_body(Expr *xp)
{
	Expr	*ep;

	assert(xp->type == D_ENTEX);
	gen_local_var_decls(xp, 1);
	gen_defn_c_code(xp, 1);
	foreach (ep, xp->entex_stmts)
	{
		gen_expr(C_ENTEX, ep, 1);
	}
}

/* Generate a function for each state that sets up delay processing:
   This function gets called prior to the event function to guarantee
   that the initial delay value specified in delay() calls are used.
   Each delay() call is assigned a (per state) unique id.  The maximum
   number of delays is recorded in the state set structure. */
static void gen_delay_body(Expr *xp)
{
	Expr	*tp;

	/* for each transition */
	foreach (tp, xp)
	{
		assert(tp->type == D_WHEN);
		traverse_expr_tree(tp->when_cond, 1u<<E_DELAY, 0, 0, gen_delay, 0);
	}
}

/* Generate call to seq_delayInit() with extra arguments ssId and delay id,
   and cast the argument proper to double.
   Example:  seq_delayInit(ssId, 1, (double)(<some expression>)); */
static int gen_delay(Expr *ep, Expr *scope, void *parg)
{
	assert(ep->type == E_DELAY);
	gen_line_marker(ep);
	/* Generate 1-st part of function w/ 1-st 2 parameters */
	indent(1); gen_code("seq_delayInit(" NM_SS ", %d, (", ep->extra.e_delay);
	/* generate the 3-rd parameter (an expression) */
	gen_expr(C_COND, ep->delay_args, 0);
	/* Complete the function call */
	gen_code("));\n");
	return FALSE;	/* no sense descending into children, as delay cannot be nested */
}

/* Generate action processing functions:
   Each state has one action routine.  It's name is derived from the
   state set name and the state name. */
static void gen_action_body(Expr *xp)
{
	Expr		*tp;
	int		trans_num;
	const int	level = 1;

	/* "switch" statment based on the transition number */
	indent(level); gen_code("switch(" NM_TRN ")\n");
	indent(level); gen_code("{\n");
	trans_num = 0;

	/* For each transition ("when" statement) ... */
	foreach (tp, xp)
	{
		Expr *ap;

		assert(tp->type == D_WHEN);
		/* one case for each transition */
		indent(level); gen_code("case %d:\n", trans_num);

		/* block within case permits local variables */
		indent(level+1); gen_code("{\n");
		/* for each definition insert corresponding code */
		gen_local_var_decls(tp, level+2);
		gen_defn_c_code(tp, level+2);
		if (tp->when_defns)
			gen_code("\n");
		/* for each action statement insert action code */
		foreach (ap, tp->when_stmts)
		{
			gen_expr(C_TRANS, ap, level+2);
		}
		/* end of block */
		indent(level+1); gen_code("}\n");

		/* end of case */
		indent(level+1); gen_code("return;\n");
		trans_num++;
	}
	/* end of switch stmt */
	indent(level); gen_code("}\n");
}

/* Generate a C function that checks events for a particular state */
static void gen_event_body(Expr *xp)
{
	Expr		*tp;
	int		trans_num;
	const int	level = 1;

	trans_num = 0;
	/* For each transition generate an "if" statement ... */
	foreach (tp, xp)
	{
		Expr *next_sp;

		assert(tp->type == D_WHEN);
		if (tp->when_cond)
			gen_line_marker(tp->when_cond);
		indent(level); gen_code("if (");
		if (tp->when_cond == 0)
			gen_code("TRUE");
		else
			gen_expr(C_COND, tp->when_cond, 0);
		gen_code(")\n");
		indent(level); gen_code("{\n");

		next_sp = tp->extra.e_when->next_state;
		if (!next_sp)
		{
			/* "when(...) {...} exit" -> exit from program */
			indent(level+1);
			gen_code("seq_exit(" NM_SS ");\n");
		}
		else
		{
			indent(level+1);
			gen_code("*" NM_PNST " = %d;\n", next_sp->extra.e_state->index);
		}
		indent(level+1);gen_code("*" NM_PTRN " = %d;\n", trans_num);
		indent(level+1); gen_code("return TRUE;\n");
		indent(level); gen_code("}\n");
		trans_num++;
	}
	indent(level); gen_code("return FALSE;\n");
}

static void gen_var_access(Var *vp)
{
	const char *pre = global_opt_reent ? NM_VARS "->" : "";

	assert(vp);
	assert(vp->scope);

#ifdef DEBUG
	report("var_access: %s, scope=(%s,%s)\n",
		vp->name, expr_type_name(vp->scope), vp->scope->value);
#endif
	assert(is_scope(vp->scope));

	if (vp->type->tag == T_EVFLAG)
	{
		gen_code("%d/*%s*/", vp->chan.evflag->index, vp->name);
	}
	else if (vp->type->tag == T_NONE)
	{
		gen_code("%s", vp->name);
	}
	else if (vp->scope->type == D_PROG)
	{
		gen_code("%s%s", pre, vp->name);
	}
	else if (vp->scope->type == D_SS)
	{
		gen_code("%s%s_%s.%s", pre, NM_VARS, vp->scope->value, vp->name);
	}
	else if (vp->scope->type == D_STATE)
	{
		gen_code("%s%s_%s.%s_%s.%s", pre, NM_VARS,
			vp->scope->extra.e_state->var_list->parent_scope->value,
			NM_VARS, vp->scope->value, vp->name);
	}
	else	/* compound or when stmt => generate a local C variable */
	{
		gen_code("%s", vp->name);
	}
}

/* Recursively generate code for an expression (tree) */
static void gen_expr(
	int context,
	Expr *ep,		/* expression to generate code for */
	int level		/* indentation level */
)
{
	Expr	*cep;		/* child expression */

	if (ep == 0)
		return;

#ifdef	DEBUG
	report("gen_expr(%s,%s)\n", expr_type_name(ep), ep->value);
#endif

	switch(ep->type)
	{
	/* Statements */
	case S_CMPND:
		indent(level);
		gen_code("{\n");
		gen_local_var_decls(ep, level+1);
		gen_defn_c_code(ep, level+1);
		foreach (cep, ep->cmpnd_stmts)
		{
			gen_expr(context, cep, level+1);
		}
		indent(level);
		gen_code("}\n");
		break;
	case S_STMT:
		gen_line_marker(ep);
		indent(level);
		gen_expr(context, ep->stmt_expr, 0);
		gen_code(";\n");
		break;
	case S_IF:
		gen_line_marker(ep);
		indent(level);
		gen_code("if (");
		gen_expr(context, ep->if_cond, 0);
		gen_code(")\n");
		cep = ep->if_then;
		gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		if (ep->if_else)
		{
			indent(level);
			gen_code("else\n");
			cep = ep->if_else;
			gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		}
		break;
	case S_WHILE:
		gen_line_marker(ep);
		indent(level);
		gen_code("while (");
		gen_expr(context, ep->while_cond, 0);
		gen_code(")\n");
		cep = ep->while_stmt;
		gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		break;
	case S_FOR:
		gen_line_marker(ep);
		indent(level);
		gen_code("for (");
		gen_expr(context, ep->for_init, 0);
		gen_code("; ");
		gen_expr(context, ep->for_cond, 0);
		gen_code("; ");
		gen_expr(context, ep->for_iter, 0);
		gen_code(")\n");
		cep = ep->for_stmt;
		gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		break;
	case S_JUMP:
		indent(level);
		gen_code("%s;\n", ep->value);
		break;
	case S_CHANGE:
		if (context != C_TRANS)
		{
			error_at_expr(ep, "state change statement not allowed here\n");
			break;
		}
		indent(level);
		gen_code("{*" NM_PNST " = %d; return;}\n", ep->extra.e_change->extra.e_state->index);
		break;
	/* Expressions */
	case E_VAR:
		gen_var_access(ep->extra.e_var);
		break;
	case E_SUBSCR:
		gen_expr(context, ep->subscr_operand, 0);
		gen_code("[");
		gen_expr(context, ep->subscr_index, 0);
		gen_code("]");
		break;
	case E_CONST:
		if (gen_builtin_const(ep))
			break;
		gen_code("%s", ep->value);
		break;
	case E_STRING:
		gen_code("\"%s\"", ep->value);
		break;
	case E_DELAY:
	case E_FUNC:
		if (gen_builtin_func(context, ep))
			break;
		gen_code("%s(", ep->value);
		foreach (cep, ep->func_args)
		{
			gen_expr(context, cep, 0);
			if (cep->next)
				gen_code(", ");
		}
		gen_code(")");
		break;
	case E_INIT:
		gen_code("{");
		foreach (cep, ep->init_elems)
		{
			gen_expr(context, cep, 0);
			if (cep->next)
				gen_code(", ");
		}
		gen_code("}");
		break;
	case E_TERNOP:
		gen_expr(context, ep->ternop_cond, 0);
		gen_code(" ? ");
		gen_expr(context, ep->ternop_then, 0);
		gen_code(" : ");
		gen_expr(context, ep->ternop_else, 0);
		break;
	case E_BINOP:
		gen_expr(context, ep->binop_left, 0);
		gen_code(" %s ", ep->value);
		gen_expr(context, ep->binop_right, 0);
		break;
	case E_PAREN:
		gen_code("(");
		gen_expr(context, ep->paren_expr, 0);
		gen_code(")");
		break;
	case E_CAST:
		gen_code("(");
		gen_expr(context, ep->cast_type, 0);
		gen_code(")");
		gen_expr(context, ep->cast_operand, 0);
		break;
	case E_PRE:
		gen_code("%s", ep->value);
		gen_expr(context, ep->pre_operand, 0);
		break;
	case E_POST:
		gen_expr(context, ep->post_operand, 0);
		gen_code("%s", ep->value);
		break;
	/* C-code can be either definition, statement, or expression */
	case T_TEXT:
		indent(level);
		gen_code("%s\n", ep->value);
		break;
	case D_DECL:
		gen_var_decl(ep->extra.e_decl);
		break;
	default:
		assert_at_expr(impossible, ep, "unhandled expression (%s:%s)\n",
			expr_type_name(ep), ep->value);
	}
}

static int gen_builtin_const(Expr *ep)
{
	char	*const_name = ep->value;
	struct const_symbol *sym = lookup_builtin_const(global_sym_table, const_name);

	if (!sym)
		return CT_NONE;
	gen_code("%s", const_name);
	return sym->type;
}

/* Generate builtin function call */
static int gen_builtin_func(int context, Expr *ep)
{
	char	*func_name = ep->value;	/* function name */
	Expr	*ap;			/* argument expr */
	struct func_symbol *sym = lookup_builtin_func(global_sym_table, func_name);

	if (!sym)
		return FALSE;	/* not a special function */

#ifdef	DEBUG
	report("gen_builtin_func: name=%s, type=%u, add_length=%u, "
		"default_args=%u, ef_action_only=%u, ef_args=%u\n",
		func_name, sym->type, sym->add_length, sym->default_args,
		sym->ef_action_only, sym->ef_args);
#endif
	/* All builtin functions require ssId as 1st parameter */
	gen_code("seq_%s(" NM_SS "", func_name);
	switch (sym->type)
	{
	case FT_DELAY:
		gen_code(", %d)", ep->extra.e_delay);
		break;
	case FT_EVENT:
		/* Event flag functions */
		gen_ef_func(context, ep, func_name, sym->ef_action_only);
		break;
	case FT_PV:
		gen_pv_func(context, ep, func_name, sym->add_length,
			sym->default_args, sym->ef_args);
		break;
	case FT_OTHER:
		/* just fill in user-supplied parameters */
		foreach (ap, ep->func_args)
		{
			gen_code(", ");
			gen_expr(context, ap, 0);
		}
		gen_code(")");
		break;
	default:
		assert(impossible);
	}
	return TRUE;
}

/* Check an event flag argument */
static void gen_ef_arg(
	const char	*func_name,	/* function name */
	Expr		*ap,		/* argument expression */
	uint		index		/* argument index */
)
{
	Var	*vp;

	assert(ap);
	if (ap->type != E_VAR)
	{
		error_at_expr(ap,
		  "argument %d to built-in function %s must be an event flag\n",
		  index, func_name);
		return;
	}
	vp = ap->extra.e_var;
	assert(vp->type);
	if (vp->type->tag != T_EVFLAG)
	{
		error_at_expr(ap,
		  "argument to built-in function %s must be an event flag\n", func_name);
		return;
	}
	gen_var_access(vp);
}

/* Generate code for all event flag functions */
static void gen_ef_func(
	int		context,
	Expr		*ep,		/* function call expression */
	const char	*func_name,	/* function name */
	uint		action_only	/* not allowed in cond */
)
{
	Expr	*ap;			/* argument expression */

	ap = ep->func_args;

	if (action_only && context == C_COND)
	{
		error_at_expr(ep,
		  "calling %s is not allowed inside a when condition\n", func_name);
		return;
	}
	if (!ap)
	{
		error_at_expr(ep,
		  "built-in function %s requires an argument\n", func_name);
		return;
	}
	gen_code(", ");
	gen_ef_arg(func_name, ap, 1);
	gen_code(")");
}

/* Generate code for pv functions requiring a database variable.
   The channel id (index into channel array) is substituted for the variable.
   "add_length" => the array length (1 if not an array) follows the channel id 
   "num_params > 0" => add default (zero) parameters up to the spec. number */
static void gen_pv_func(
	int		context,
	Expr		*ep,		/* function call expression */
	const char	*func_name,	/* function name */
	uint		add_length,	/* add array length after channel id */
	uint		num_params,	/* number of params to add (if omitted) */
	uint		ef_args		/* extra args are event flags */
)
{
	Expr	*ap, *subscr = 0;
	Var	*vp = 0;
	uint	num_extra_parms = 0;

	ap = ep->func_args;
	/* first parameter is always */
	if (ap == 0)
	{
		error_at_expr(ep,
			"function '%s' requires a parameter\n", func_name);
		return;
	}

	if (ap->type == E_VAR)
	{
		vp = ap->extra.e_var;
	}
	else if (ap->type == E_SUBSCR)
	{
		/* Form should be: <pv variable>[<expression>] */
		Expr *operand = ap->subscr_operand;
		subscr = ap->subscr_index;
		if (operand->type == E_VAR)
		{
			vp = operand->extra.e_var;
		}
	}
	if (vp == 0)
	{
		error_at_expr(ep,
		  "parameter 1 to '%s' must be a variable or subscripted variable\n",
		  func_name);
		return;
	}

#ifdef	DEBUG
	report("gen_pv_func: fun=%s, var=%s\n", ep->value, vp->name);
#endif
	gen_code(", ");
	if (vp->assign == M_NONE)
	{
		error_at_expr(ep,
			"parameter 1 to '%s' was not assigned to a pv\n", func_name);
		gen_code("?/*%s*/", vp->name);
	}
	else if (ap->type == E_SUBSCR && vp->assign != M_MULTI)
	{
		error_at_expr(ep,
			"parameter 1 to '%s' is subscripted but the variable "
			"it refers to has not been assigned to multiple pvs\n", func_name);
		gen_code("%d/*%s*/", vp->index, vp->name);
	}
	else
	{
		gen_code("%d/*%s*/", vp->index, vp->name);
	}

	if (ap->type == E_SUBSCR)
	{
		/* e.g. pvPut(xyz[i+2]); => seq_pvPut(ssId, 3 + (i+2)); */
		gen_code(" + (VAR_ID)(");
		/* generate the subscript expression */
		gen_expr(context, subscr, 0);
		gen_code(")");
	}

	/* If requested, add length parameter (if subscripted variable,
	   length is always 1) */
	if (add_length)
	{
		if (ap->type != E_SUBSCR)
		{
			gen_code(", %d", type_array_length1(vp->type));
		}
		else
		{
			gen_code(", 1");
		}
	}

	/* Add any additional parameter(s) */
	foreach (ap, ap->next)
	{
		num_extra_parms++;
		gen_code(", ");
		if (ef_args)
		{
			/* special case: constant NOEVFLAG */
			if (ap->type == E_CONST)
			{
				if (gen_builtin_const(ap)!=CT_EVFLAG)
					error_at_expr(ap,
					  "argument %d to built-in function %s must be an event flag\n",
					  num_extra_parms+1, func_name);
			}
			else
				gen_ef_arg(func_name, ap, num_extra_parms+1);
		}
		else
		{
			gen_expr(context, ap, 0);
		}
	}

	/* If not enough additional parameter(s) were specified, add
	   extra zero parameters */
	while (num_extra_parms < num_params)
	{
		gen_code(", 0");
		num_extra_parms++;
	}

	/* Close the parameter list */
	gen_code(")");
#ifdef	DEBUG
	report("gen_pv_func: done (fun=%s, var=%s)\n", ep->value, vp->name);
#endif
}

static void gen_var_init(Var *vp, int level)
{
	assert(vp->init);
	gen_line_marker(vp->init);
	indent(level); gen_code("{ static ");
	gen_var_decl(vp);
	gen_code(" = ");
	gen_expr(C_INIT, vp->init, level);
	gen_code("; memcpy(&");
	gen_var_access(vp);
	gen_code(", &%s, sizeof(%s)); }\n", vp->name, vp->name);
}

/* Generate initializers for variables of global lifetime */
static void gen_user_var_init(Expr *prog, int level)
{
	Var *vp;
	Expr *ssp;

	assert(prog->type == D_PROG);
	/* global variables */
	foreach(vp, prog->extra.e_prog->first)
	{
		if (vp->init)
		{
			if (vp->type->tag == T_NONE)
				error_at_expr(vp->init,
					"foreign variable '%s' cannot be initialized\n",
					vp->name);
			else if (vp->type->tag == T_EVFLAG)
				error_at_expr(vp->init,
					"event flag '%s' cannot be initialized\n",
					vp->name);
			else
				gen_var_init(vp, level);
		}
	}
	/* state and state set variables */
	foreach (ssp, prog->prog_statesets)
	{
		Expr *sp;

		assert(ssp->type == D_SS);
		/* state set variables */
		foreach(vp, ssp->extra.e_ss->var_list->first)
		{
			if (vp->init)
			{
				gen_var_init(vp, level);
			}
		}
		foreach (sp, ssp->ss_states)
		{
			assert(sp->type == D_STATE);
			/* state variables */
			foreach (vp, sp->extra.e_state->var_list->first)
			{
				if (vp->init)
				{
					gen_var_init(vp, level);
				}
			}
		}
	}
}

static void gen_prog_init_func(Expr *prog)
{
	assert(prog->type == D_PROG);
	gen_code("\n/* Program init func */\n");
	gen_code("static void " NM_INIT "(SEQ_VARS *const " NM_VARS ")\n{\n");
	gen_user_var_init(prog, 1);
	gen_code("}\n");
}

static void gen_prog_func(
	Expr *prog,
	const char *name,
	const char *doc,
	Expr *xp,
	void (*gen_body)(Expr *xp))
{
	assert(prog->type == D_PROG);
	gen_code("\n/* Program %s func */\n", doc);
	gen_code("static void %s(SS_ID " NM_SS ", SEQ_VARS *const " NM_VARS ")\n{\n",
		name);
	if (xp && gen_body) gen_body(xp);
	gen_code("}\n");
}
