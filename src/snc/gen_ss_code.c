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

static void gen_local_var_decls(Expr *scope, int context, int level);
static void gen_state_func(
	const char *ss_name,
	uint ss_num,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *xp, int context),
	int context,
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
);
static void gen_entex_body(Expr *xp, int context);
static void gen_event_body(Expr *xp, int context);
static void gen_action_body(Expr *xp, int context);
static void gen_expr(int context, Expr *ep, int level);
static void gen_ef_func(int context, Expr *ep, const char *func_name, uint ef_action_only);
static void gen_pv_func(int context, Expr *ep,
	const char *func_name, uint add_length,
	uint num_params, uint ef_args,
	const char *default_values[]);
static void gen_builtin_func(int context, Expr *ep);

static void gen_prog_func(
	Expr *prog,
	const char *doc,
	const char *name,
	void (*gen_body)(Expr *prog)
);
static void gen_prog_entex_func(
	Expr *prog,
	const char *doc,
	const char *name,
	void (*gen_body)(Expr *)
);
static void gen_prog_init_body(Expr *prog);
static void gen_prog_entry_body(Expr *prog);
static void gen_prog_exit_body(Expr *prog);
static void gen_funcdef(Expr *fp);

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
	C_SS,		/* otherwise inside a state set */
	C_FUNC,		/* inside a function definition */
	C_GLOBAL	/* outside state sets and functions */
};

/*
 * HACK: use global variable to make reentrant option available
 * to gen_var_access below. Otherwise we'd have to pass it through
 * almost every subroutine in this module.
 */
static Options global_options;

/* Generate state set C code from analysed syntax tree */
void gen_ss_code(Expr *prog, Options options)
{
	Expr	*sp, *ssp, *fp;
	uint	ss_num = 0;

	/* HACK: intialise global variable as implicit parameter */
	global_options = options;

	/* Generate program init func */
	gen_prog_func(prog, "init", NM_INIT, gen_prog_init_body);

	/* Generate program entry func */
	if (prog->prog_entry)
		gen_prog_entex_func(prog, "entry", NM_ENTRY, gen_prog_entry_body);

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
					C_SS, "Entry", NM_ENTRY, "void", "");
			if (sp->state_exit)
				gen_state_func(ssp->value, ss_num, sp->value,
					sp->state_exit, gen_entex_body,
					C_SS, "Exit", NM_EXIT, "void", "");
			/* Generate event processing function */
			gen_state_func(ssp->value, ss_num, sp->value,
				sp->state_whens, gen_event_body,
				C_SS, "Event", NM_EVENT, "seqBool",
				", int *"NM_PTRN", int *"NM_PNST);
			/* Generate action processing function */
			gen_state_func(ssp->value, ss_num, sp->value,
				sp->state_whens, gen_action_body,
				C_TRANS, "Action", NM_ACTION, "void",
				", int "NM_TRN", int *"NM_PNST);
		}
		ss_num++;
	}

	/* Generate program exit func */
	if (prog->prog_exit)
		gen_prog_entex_func(prog, "exit", NM_EXIT, gen_prog_exit_body);

	/* Generate function definitions */
	foreach(fp, prog->prog_funcdefs)
	{
		gen_funcdef(fp);
	}
}

/* Generate a local C variable declaration for each variable declared
   inside the body of an entry, exit, when, or compound statement block. */
static void gen_local_var_decls(Expr *scope, int context, int level)
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
			gen_expr(context, vp->init, level);
		}
		gen_code(";\n");
	}
}

static void gen_state_func(
	const char *ss_name,
	uint ss_num,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *xp, int context),
	int context,
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
)
{
	gen_code("\n/* %s function for state \"%s\" in state set \"%s\" */\n",
		title, state_name, ss_name);
	gen_code("static %s %s_%s_%d_%s(SS_ID " NM_SS ", SEQ_VARS *const " NM_VARS_ARG "%s)\n",
		rettype, prefix, ss_name, ss_num, state_name, extra_args);
	gen_body(xp, context);
}

static void gen_block(Expr *xp, int context, int level)
{
	Expr	*cxp;

	assert(xp->type == S_CMPND);
	gen_code("{\n");
	gen_local_var_decls(xp, context, level+1);
	gen_defn_c_code(xp, level+1);
	foreach (cxp, xp->cmpnd_stmts)
	{
		gen_expr(context, cxp, level+1);
	}
	indent(level); gen_code("}\n");
}

static void gen_entex_body(Expr *xp, int context)
{
	assert(xp->type == D_ENTEX);
	gen_block(xp->entex_block, context, 0);
}

/* Generate action processing functions:
   Each state has one action routine.  It's name is derived from the
   state set name and the state name. */
static void gen_action_body(Expr *xp, int context)
{
	Expr		*tp;
	int		trans_num;
	const int	level = 1;

	gen_code("{\n");
	/* "switch" statment based on the transition number */
	indent(level); gen_code("switch(" NM_TRN ")\n");
	indent(level); gen_code("{\n");
	trans_num = 0;

	/* For each transition ("when" statement) ... */
	foreach (tp, xp)
	{
		assert(tp->type == D_WHEN);
		/* one case for each transition */
		indent(level); gen_code("case %d: ", trans_num);
		gen_block(tp->when_block, context, level+1);
		/* end of case */
		indent(level+1); gen_code("return;\n");
		trans_num++;
	}
	/* end of switch stmt */
	indent(level); gen_code("}\n");
	/* end of function */
	gen_code("}\n");
}

/* Generate a C function that checks events for a particular state */
static void gen_event_body(Expr *xp, int context)
{
	Expr		*tp;
	int		trans_num;
	const int	level = 1;

	gen_code("{\n");
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
	/* end of function */
	gen_code("}\n");
}

static void gen_var_access(Var *vp)
{
	const char *pre = global_options.reent ? NM_VARS_ARG "->" : "";

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
	else if (vp->type->tag == T_NONE || vp->type->tag == T_FUNCTION)
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
		gen_block(ep, context, level+1);
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
	case S_RETURN:
		if (context != C_FUNC)
		{
			error_at_expr(ep, "return statement not allowed here\n");
			break;
		}
		indent(level);
		gen_code("return "); gen_expr(context, ep->return_expr, level); gen_code(";\n");
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
		if (ep->extra.e_const)
			gen_code("%s", ep->extra.e_const->name);
		else
			gen_code("%s", ep->value);
		break;
	case E_STRING:
		gen_code("\"%s\"", ep->value);
		break;
	case E_FUNC:
		if (ep->func_expr->type == E_BUILTIN)
		{
			gen_builtin_func(context, ep);
			break;
		}
		gen_expr(context, ep->func_expr, 0);
		gen_code("(");
		if (ep->func_expr->type == E_VAR && ep->func_expr->extra.e_var->type->tag == T_FUNCTION)
		{
			/* direct call and not a foreign function => add implicit parameters */
			gen_code(NM_SS ", " NM_VARS_ARG);
			if (ep->func_args)
				gen_code(", ");
		}
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
	case E_SELECT:
		gen_expr(context, ep->select_left, 0);
		gen_code("%s", ep->value);
		gen_expr(context, ep->select_right, 0);
		break;
	case E_MEMBER:
		gen_code("%s", ep->value);
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

/* Generate builtin function call */
static void gen_builtin_func(int context, Expr *ep)
{
	Expr *ap;	/* argument expr */
	struct func_symbol *sym = ep->func_expr->extra.e_builtin;

	assert(ep->func_expr->type == E_BUILTIN);
	assert(sym);

#ifdef	DEBUG
	report("gen_builtin_func: name=%s, type=%u, add_length=%u, "
		"default_args=%u, ef_action_only=%u, ef_args=%u\n",
		sym->name, sym->type, sym->add_length, sym->default_args,
		sym->ef_action_only, sym->ef_args);
#endif
	/* All builtin functions require ssId as 1st parameter */
	assert_at_expr(context != C_GLOBAL, ep,
		"calling built-in function %s not allowed here\n", sym->name);
	gen_code("seq_%s("NM_SS, sym->name);
	if (context != C_COND && sym->cond_only)
	{
		error_at_expr(ep,
		  "calling built-in function %s not allowed here\n", sym->name);
		return;
	}
	switch (sym->type)
	{
	case FT_EVENT:
		/* Event flag functions */
		gen_ef_func(context, ep, sym->name, sym->ef_action_only);
		break;
	case FT_PV:
		gen_pv_func(context, ep, sym->name, sym->add_length,
			sym->default_args, sym->ef_args, sym->default_values);
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
	uint		ef_args,	/* extra args are event flags */
	const char	*default_values[]/* param values to add (if omitted) */
)
{
	Expr	*ap, *subscr = 0;
	Var	*vp = 0;
	uint	num_extra_parms = 0;

	ap = ep->func_args;
	if (ap == 0)
	{
		error_at_expr(ep,
			"function '%s' requires an argument\n", func_name);
		return;
	}

	if (ap->type == E_VAR)
	{
		vp = ap->extra.e_var;
		assert(vp);
		if (vp->assign == M_MULTI)
		{
			if (!add_length)
			{
				error_at_expr(ap,
					"passing multi-PV array '%s' to function '%s' is not "
					"(yet?) allowed\n",
					vp->name, func_name);
				return;
			}
			if (!global_options.newpv)
			{
				error_at_expr(ap,
					"passing multi-PV array '%s' to function '%s' is not "
					"allowed in compatibility mode (option -p)\n",
					vp->name, func_name);
				report_at_expr(ap, "Perhaps you meant to pass '%s[0]'?\n", vp->name);
				if (add_length)
				{
					report_at_expr(ap, "Use option +p to allow this but then "
						"pv functions operate on all contained PVs\n");
				}
				return;
			}
		}
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
	report("gen_pv_func: fun=%s, var=%s\n", func_name, vp->name);
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
				if (ap->extra.e_const && ap->extra.e_const->type == CT_EVFLAG)
					gen_expr(context, ap, 0);
				else
					error_at_expr(ap,
					  "argument %d to built-in function %s must "
					  "be an event flag\n",
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
		gen_code(", %s", default_values[num_extra_parms]);
		num_extra_parms++;
	}

	/* Close the parameter list */
	gen_code(")");
#ifdef	DEBUG
	report("gen_pv_func: done (fun=%s, var=%s)\n", func_name, vp->name);
#endif
}

static void gen_var_init(Var *vp, int context, int level)
{
	if (vp->init)
	{
		gen_line_marker(vp->init);
		indent(level); gen_code("{ static ");
		gen_type(vp->type, NM_INITVAR, vp->name);
		gen_code(" = ");
		gen_expr(context, vp->init, level);
		gen_code("; memcpy(&");
		gen_var_access(vp);
		gen_code(", &" NM_INITVAR "%s, sizeof(" NM_INITVAR "%s)); }\n",
			vp->name, vp->name);
	}
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
				gen_var_init(vp, C_GLOBAL, level);
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
			gen_var_init(vp, C_SS, level);
		}
		foreach (sp, ssp->ss_states)
		{
			assert(sp->type == D_STATE);
			/* state variables */
			foreach (vp, sp->extra.e_state->var_list->first)
			{
				gen_var_init(vp, C_SS, level);
			}
		}
	}
}

static void gen_prog_func(
	Expr *prog,
	const char *doc,
	const char *name,
	void (*gen_body)(Expr *prog)
)
{
	assert(prog->type == D_PROG);
	gen_code("\n/* Program %s func */\n", doc);
	gen_code("static void %s(PROG_ID "NM_PROG", SEQ_VARS *const "NM_VARS_ARG")\n{\n",
		name);
	gen_body(prog);
	gen_code("}\n");
}

static void gen_prog_entex_func(
	Expr *prog,
	const char *doc,
	const char *name,
	void (*gen_body)(Expr *)
)
{
	assert(prog->type == D_PROG);
	gen_code("\n/* Program %s func */\n", doc);
	gen_code("static void %s(SS_ID " NM_SS ", SEQ_VARS *const " NM_VARS_ARG ")\n",
		name);
	gen_body(prog);
}

static void gen_prog_init_body(Expr *prog)
{
	assert(prog->type == D_PROG);
	gen_user_var_init(prog, 1);
}

static void gen_prog_entry_body(Expr *prog)
{
	assert(prog->type == D_PROG);
	gen_entex_body(prog->prog_entry, C_SS);
}

static void gen_prog_exit_body(Expr *prog)
{
	assert(prog->type == D_PROG);
	gen_entex_body(prog->prog_exit, C_SS);
}

static void gen_funcdef(Expr *fp)
{
	Expr *param_decl;
	Var *vp = fp->funcdef_decl->extra.e_decl;

	assert(fp->type == D_FUNCDEF);
	gen_line_marker(vp->decl);
	gen_code("static ");
	gen_type(vp->type->val.function.return_type, "", vp->name);
	gen_code("(SS_ID " NM_SS ", SEQ_VARS *const " NM_VARS_ARG);
	foreach (param_decl,  fp->funcdef_params)
	{
		vp = param_decl->extra.e_decl;
		gen_code(", ");
		gen_var_decl(vp);
	}
	gen_code(")\n");
	gen_block(fp->funcdef_block, C_FUNC, 0);
}
