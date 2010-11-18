/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)
***************************************************************************
		State set code generation
***************************************************************************/
#include	<stdio.h>
#include	<string.h>
#include	<assert.h>

#include	"parse.h"
#include	"analysis.h"
#include	"gen_code.h"
#include	"snc_main.h"
#include	"sym_table.h"
#include	"gen_ss_code.h"

static const int impossible = 0;

enum {
	C_NONE, C_TRUE, C_FALSE, C_SYNC, C_ASYNC, C_CODE_LAST
};

static char *const_code_str[] = {
	NULL, "TRUE", "FALSE", "SYNC", "ASYNC", NULL
};

enum
{
	F_NONE,
	F_DELAY,
	F_EFCLEAR,
	F_EFSET,
	F_EFTEST,
	F_EFTESTANDCLEAR,
	F_MACVALUEGET,
	F_OPTGET,
	F_PVASSIGN,
	F_PVASSIGNCOUNT,
	F_PVASSIGNED,
	F_PVCHANNELCOUNT,
	F_PVCONNECTCOUNT,
	F_PVCONNECTED,
	F_PVCOUNT,
	F_PVDISCONNECT,
	F_PVFLUSH,
	F_PVFLUSHQ,
	F_PVFREEQ,
	F_PVGET,
	F_PVGETCOMPLETE,
	F_PVGETQ,
	F_PVINDEX,
	F_PVMESSAGE,
	F_PVMONITOR,
	F_PVNAME,
	F_PVPUT,
	F_PVPUTCOMPLETE,
	F_PVSEVERITY,
	F_PVSTATUS,
	F_PVSTOPMONITOR,
	F_PVSYNC,
	F_PVTIMESTAMP,
	F_SEQLOG,
	F_CODE_LAST
};

static char *fcode_str[] = {
	NULL,
	"delay",
	"efClear",
	"efSet",
	"efTest",
	"efTestAndClear",
	"macValueGet",
	"optGet",
	"pvAssign",
	"pvAssignCount",
	"pvAssigned",
	"pvChannelCount",
	"pvConnectCount",
	"pvConnected",
	"pvCount",
	"pvDisconnect",
	"pvFlush",
	"pvFlushQ",
	"pvFreeQ",
	"pvGet",
	"pvGetComplete",
	"pvGetQ",
	"pvIndex",
	"pvMessage",
	"pvMonitor",
	"pvName",
	"pvPut",
	"pvPutComplete",
	"pvSeverity",
	"pvStatus",
	"pvStopMonitor",
	"pvSync",
	"pvTimeStamp",
	"seqLog",
	NULL
};

static void gen_local_var_decls(Expr *scope, int level);
static void gen_state_func(
	const char *ss_name,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *xp),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
);
static void gen_entry_body(Expr *xp);
static void gen_exit_body(Expr *xp);
static void gen_delay_body(Expr *xp);
static void gen_event_body(Expr *xp);
static void gen_action_body(Expr *xp);
static int gen_delay(Expr *ep, Expr *scope, void *parg);
static void gen_expr(int context, Expr *ep, int level);
static void gen_ef_func(int context, Expr *ep, char *fname, int func_code);
static void gen_pv_func(int context, Expr *ep,
	char *fname, int func_code, int add_length, int num_params);
static int special_func(int context, Expr *ep);
static int special_const(int context,	Expr *ep);

static void gen_prog_func(
	Expr *prog,
	const char *name,
	Expr *xp,
	void (*gen_body)(Expr *xp));
static void gen_prog_init_func(Expr *prog, int opt_reent);

/*
 * Expression context. Certain nodes of the syntax tree are
 * interpreted differently depending on the context in which
 * they appear. For instance, the state change command is only
 * allowed in transition action context (C_TRANS).
 */
enum expr_context {
	C_COND,		/* when() condition */
	C_TRANS,	/* state transition actions */
	C_ENTRY,	/* entry block */
	C_EXIT,		/* exit block */
	C_INIT		/* variable initialization */
};

/*
 * HACK: use global variables to make program options and
 * symbol table available to subroutines
 */
static int global_opt_reent;
static SymTable global_sym_table;

static void register_special_funcs(void)
{
	int fcode;

	for (fcode = F_CODE_LAST-1; fcode_str[fcode] != NULL; fcode--)
	{
		/* use address of fcode_str array as the symbol type */
		sym_table_insert(global_sym_table, fcode_str[fcode], fcode_str, (void*)fcode);
	}
}

static int func_name_to_code(char *fname)
{
	int fcode = (int)sym_table_lookup(global_sym_table, fname, fcode_str);

	assert(fcode == 0 || strcmp(fname, fcode_str[fcode]) == 0);
	return fcode;
}

static void register_special_consts(void)
{
	int const_code;

	for (const_code = C_CODE_LAST-1; const_code_str[const_code] != NULL; const_code--)
	{
		/* use address of const_code_str array as the symbol type */
		sym_table_insert(global_sym_table, const_code_str[const_code], const_code_str, (void*)const_code);
	}
}

static int const_name_to_code(char *const_name)
{
	int const_code = (int)sym_table_lookup(global_sym_table, const_name, const_code_str);

	assert(const_code == 0 || strcmp(const_name, const_code_str[const_code]) == 0);
	return const_code;
}

void init_gen_ss_code(Program *program)
{
	/* HACK: intialise globals */
	global_opt_reent = program->options.reent;
	global_sym_table = program->sym_table;

	/* Insert special names into symbol table */
	register_special_funcs();
	register_special_consts();
}

/* Generate state set C code from analysed syntax tree */
void gen_ss_code(Program *program)
{
	Expr	*prog = program->prog;
	Expr	*ssp;
	Expr	*sp;

	/* Generate program init func */
	gen_prog_init_func(prog, program->options.reent);

	/* Generate program entry func */
	if (prog->prog_entry)
		gen_prog_func(prog, "entry", prog->prog_entry, gen_entry_body);

	/* For each state set ... */
	foreach (ssp, prog->prog_statesets)
	{
		/* For each state ... */
		foreach (sp, ssp->ss_states)
		{
			printf("\n/****** Code for state \"%s\" in state set \"%s\" ******/\n",
				sp->value, ssp->value);

			/* Generate entry and exit functions */
			if (sp->state_entry != 0)
				gen_state_func(ssp->value, sp->value, 
					sp->state_entry, gen_entry_body,
					"Entry", "I", "void", "");
			if (sp->state_exit != 0)
				gen_state_func(ssp->value, sp->value,
					sp->state_exit, gen_exit_body,
					"Exit", "O", "void", "");
			/* Generate function to set up for delay processing */
			gen_state_func(ssp->value, sp->value,
				sp->state_whens, gen_delay_body,
				"Delay", "D", "void", "");
			/* Generate event processing function */
			gen_state_func(ssp->value, sp->value,
				sp->state_whens, gen_event_body,
				"Event", "E", "unsigned",
				", short *pTransNum, short *pNextState");
			/* Generate action processing function */
			gen_state_func(ssp->value, sp->value,
				sp->state_whens, gen_action_body,
				"Action", "A", "void",
                                ", short transNum, short *pNextState");
		}
	}

	/* Generate program exit func */
	if (prog->prog_exit)
		gen_prog_func(prog, "exit", prog->prog_exit, gen_exit_body);
}

/* Generate a local C variable declaration for each variable declared
   inside the body of an entry, exit, when, or compound statement block. */
static void gen_local_var_decls(Expr *scope, int level)
{
	Var	*vp;
	VarList	*var_list;

	var_list = *pvar_list_from_scope(scope);

	/* Convert internal type to `C' type */
	foreach (vp, var_list->first)
	{
		if (vp->decl && vp->type->tag != V_NONE)
		{
			gen_line_marker(vp->decl);
			indent(level);
			gen_var_decl(vp);

			/* optional initialisation */
			if (vp->init)
			{
				printf(" = ");
				gen_expr(C_INIT, vp->init, level);
			}
			printf(";\n");
		}
	}
}

static void gen_type_default(Type *type)
{
	uint n;

	assert(type);
	switch(type->tag)
	{
	case V_STRING:
		printf("\"\"");
		break;
	case V_ARRAY:
		printf("{");
		for (n=0; n<type->val.array.num_elems; n++)
		{
			gen_type_default(type->val.array.elem_type);
			if (n+1<type->val.array.num_elems) printf(",");
		}
		printf("}");
		break;
	default:
		printf("0");
	}
}

void gen_var_init(Var *vp, int level)
{
	assert(vp);

	if (vp->init)
		gen_expr(C_INIT, vp->init, level);
	else
		gen_type_default(vp->type);
}

static void gen_state_func(
	const char *ss_name,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *xp),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
)
{
	printf("\n/* %s function for state \"%s\" in state set \"%s\" */\n",
 		title, state_name, ss_name);
	printf("static %s %s_%s_%s(SS_ID ssId, struct %s *pVar%s)\n{\n",
		rettype, prefix, ss_name, state_name, VAR_PREFIX, extra_args);
	gen_body(xp);
	printf("}\n");
}

static void gen_entry_body(Expr *xp)
{
	Expr	*ep;

	assert(xp->type == D_ENTRY);
	gen_local_var_decls(xp, 1);
	gen_defn_c_code(xp, 1);
	foreach (ep, xp->entry_stmts)
	{
		gen_expr(C_ENTRY, ep, 1);
	}
}

static void gen_exit_body(Expr *xp)
{
	Expr	*ep;

	assert(xp->type == D_EXIT);
	gen_local_var_decls(xp, 1);
	gen_defn_c_code(xp, 1);
	foreach (ep, xp->exit_stmts)
	{
		gen_expr(C_ENTRY, ep, 1);
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
		traverse_expr_tree(tp->when_cond, 1<<E_DELAY, 0, 0, gen_delay, 0);
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
	indent(1); printf("seq_delayInit(ssId, %d, (", ep->extra.e_delay);
	/* generate the 3-rd parameter (an expression) */
	gen_expr(C_COND, ep->delay_args, 0);
	/* Complete the function call */
	printf("));\n");
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
	indent(level); printf("switch(transNum)\n");
	indent(level); printf("{\n");
	trans_num = 0;

	/* For each transition ("when" statement) ... */
	foreach (tp, xp)
	{
		Expr *ap;

		assert(tp->type == D_WHEN);
		/* one case for each transition */
		indent(level); printf("case %d:\n", trans_num);

		/* block within case permits local variables */
		indent(level+1); printf("{\n");
		/* for each definition insert corresponding code */
		gen_local_var_decls(tp, level+2);
		gen_defn_c_code(tp, level+2);
		if (tp->when_defns)
			printf("\n");
		/* for each action statement insert action code */
		foreach (ap, tp->when_stmts)
		{
			gen_expr(C_TRANS, ap, level+2);
		}
		/* end of block */
		indent(level+1); printf("}\n");

		/* end of case */
		indent(level+1); printf("return;\n");
		trans_num++;
	}
	/* end of switch stmt */
	indent(level); printf("}\n");
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
		if (tp->when_cond != 0)
			gen_line_marker(tp->when_cond);
		indent(level); printf("if (");
		if (tp->when_cond == 0)
			printf("TRUE");
		else
			gen_expr(C_COND, tp->when_cond, 0);
		printf(")\n");
		indent(level); printf("{\n");

		next_sp = tp->extra.e_when->next_state;
		/* NULL at this point would have been an error in analysis phase */
		assert(next_sp != 0);
		indent(level+1);
		printf("*pNextState = %d;\n", next_sp->extra.e_state->index);
		indent(level+1); printf("*pTransNum = %d;\n", trans_num);
		indent(level+1); printf("return TRUE;\n");
		indent(level); printf("}\n");
		trans_num++;
	}
	indent(level); printf("return FALSE;\n");
}

static void gen_var_access(Var *vp)
{
	char *pre = global_opt_reent ? "pVar->" : "";

	assert(vp);
	assert(vp->scope);

#ifdef DEBUG
	report("var_access: %s, scope=(%s,%s)\n",
		vp->name, expr_type_name(vp->scope), vp->scope->value);
#endif
	assert((1<<vp->scope->type) & scope_mask);

	if (vp->type->tag == V_EVFLAG)
	{
		printf("%d/*%s*/", vp->chan.evflag->index, vp->name);
	}
	else if (vp->type->tag == V_NONE)
	{
		printf("%s", vp->name);
	}
	else if (vp->scope->type == D_PROG)
	{
		printf("%s%s", pre, vp->name);
	}
	else if (vp->scope->type == D_SS)
	{
		printf("%s%s_%s.%s", pre, VAR_PREFIX, vp->scope->value, vp->name);
	}
	else if (vp->scope->type == D_STATE)
	{
		printf("%s%s_%s.%s_%s.%s", pre, VAR_PREFIX,
			vp->scope->extra.e_state->var_list->parent_scope->value,
			VAR_PREFIX, vp->scope->value, vp->name);
	}
	else	/* compound or when stmt => generate a local C variable */
	{
		printf("%s", vp->name);
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
		printf("{\n");
		gen_local_var_decls(ep, level+1);
		gen_defn_c_code(ep, level+1);
		foreach (cep, ep->cmpnd_stmts)
		{
			gen_expr(context, cep, level+1);
		}
		indent(level);
		printf("}\n");
		break;
	case S_STMT:
		gen_line_marker(ep);
		indent(level);
		gen_expr(context, ep->stmt_expr, 0);
		printf(";\n");
		break;
	case S_IF:
		gen_line_marker(ep);
		indent(level);
		printf("if (");
		gen_expr(context, ep->if_cond, 0);
		printf(")\n");
		cep = ep->if_then;
		gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		if (ep->if_else != 0)
		{
			indent(level);
			printf("else\n");
			cep = ep->if_else;
			gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		}
		break;
	case S_WHILE:
		gen_line_marker(ep);
		indent(level);
		printf("while (");
		gen_expr(context, ep->while_cond, 0);
		printf(")\n");
		cep = ep->while_stmt;
		gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		break;
	case S_FOR:
		gen_line_marker(ep);
		indent(level);
		printf("for (");
		gen_expr(context, ep->for_init, 0);
		printf("; ");
		gen_expr(context, ep->for_cond, 0);
		printf("; ");
		gen_expr(context, ep->for_iter, 0);
		printf(")\n");
		cep = ep->for_stmt;
		gen_expr(context, cep, cep->type == S_CMPND ? level : level+1);
		break;
	case S_JUMP:
		indent(level);
		printf("%s;\n", ep->value);
		break;
	case S_CHANGE:
		if (context != C_TRANS)
		{
			error_at_expr(ep, "state change statement not allowed here\n");
			break;
		}
		indent(level);
		printf("{*pNextState = %d; return;}\n", ep->extra.e_change->extra.e_state->index);
		break;
	/* Expressions */
	case E_VAR:
		gen_var_access(ep->extra.e_var);
		break;
	case E_SUBSCR:
		gen_expr(context, ep->subscr_operand, 0);
		printf("[");
		gen_expr(context, ep->subscr_index, 0);
		printf("]");
		break;
	case E_CONST:
		if (special_const(context, ep))
			break;
		printf("%s", ep->value);
		break;
	case E_STRING:
		printf("\"%s\"", ep->value);
		break;
	case E_DELAY:
		printf("seq_delay(ssId, %d)", ep->extra.e_delay);
		break;
	case E_FUNC:
		if (special_func(context, ep))
			break;
		printf("%s(", ep->value);
		foreach (cep, ep->func_args)
		{
			gen_expr(context, cep, 0);
			if (cep->next)
				printf(", ");
		}
		printf(")");
		break;
	case E_INIT:
		printf("{");
		foreach (cep, ep->init_elems)
		{
			gen_expr(context, cep, 0);
			if (cep->next)
				printf(", ");
		}
		printf("}");
		break;
	case E_TERNOP:
		gen_expr(context, ep->ternop_cond, 0);
		printf(" ? ");
		gen_expr(context, ep->ternop_then, 0);
		printf(" : ");
		gen_expr(context, ep->ternop_else, 0);
		break;
	case E_BINOP:
		gen_expr(context, ep->binop_left, 0);
		printf(" %s ", ep->value);
		gen_expr(context, ep->binop_right, 0);
		break;
	case E_PAREN:
		printf("(");
		gen_expr(context, ep->paren_expr, 0);
		printf(")");
		break;
	case E_PRE:
		printf("%s", ep->value);
		gen_expr(context, ep->pre_operand, 0);
		break;
	case E_POST:
		gen_expr(context, ep->post_operand, 0);
		printf("%s", ep->value);
		break;
	/* C-code can be either definition, statement, or expression */
	case T_TEXT:
		indent(level);
		printf("%s\n", ep->value);
		break;
	default:
		assert(impossible);
#ifdef DEBUG
		report_at_expr(ep, "unhandled expression (%s:%s)\n",
			expr_type_name(ep), ep->value);
#endif
	}
}

static int special_const(int context,	Expr *ep)
{
	int n_const;
	char *const_name = ep->value;
	int const_code = const_name_to_code(const_name);
	switch(const_code)
	{
		case C_TRUE:	n_const = 1; break;
		case C_FALSE:	n_const = 0; break;
		case C_ASYNC:	n_const = 1; break;
		case C_SYNC:	n_const = 2; break;
		default:	return FALSE;
	}
	printf("%d", n_const);
	return TRUE;
}

/* Process special function (returns TRUE if this is a special function)
	Checks for one of the following special functions:
	 - event flag functions, e.g. efSet()
	 - process variable functions, e.g. pvPut()
	 - delay()
	 - macValueGet()
	 - seqLog()
*/
static int special_func(int context,	Expr *ep)
{
	char	*fname;		/* function name */
	Expr	*ap;		/* arguments */
	int	func_code;

	fname = ep->value;
	func_code = func_name_to_code(fname);
	if (func_code == F_NONE)
		return FALSE;	/* not a special function */

#ifdef	DEBUG
	report("special_func: code=%d, name=%s\n", func_code, fname);
#endif
	switch (func_code)
	{
	case F_DELAY:
		return TRUE;

	case F_EFSET:
	case F_EFTEST:
	case F_EFCLEAR:
	case F_EFTESTANDCLEAR:
		/* Event flag functions */
		gen_ef_func(context, ep, fname, func_code);
		return TRUE;

	case F_PVASSIGN:
	case F_PVASSIGNED:
	case F_PVCONNECTED:
	case F_PVCOUNT:
	case F_PVDISCONNECT:
	case F_PVFLUSHQ:
	case F_PVFREEQ:
	case F_PVGETCOMPLETE:
	case F_PVGETQ:
	case F_PVINDEX:
	case F_PVMESSAGE:
	case F_PVMONITOR:
	case F_PVNAME:
	case F_PVSEVERITY:
	case F_PVSTATUS:
	case F_PVSTOPMONITOR:
	case F_PVSYNC:
	case F_PVTIMESTAMP:
		/* PV functions requiring a channel id and no default args */
		gen_pv_func(context, ep, fname, func_code, FALSE, 0);
		return TRUE;

	case F_PVPUT:
	case F_PVGET:
		/* PV functions requiring a channel id and
		   defaulted last 1 parameter */
		gen_pv_func(context, ep, fname, func_code, FALSE, 1);
		return TRUE;

	case F_PVPUTCOMPLETE:
		/* PV functions requiring a channel id, an (implicit) array
		   length and defaulted last 2 parameters */
		gen_pv_func(context, ep, fname, func_code, TRUE, 2);
		return TRUE;

	case F_MACVALUEGET:
	case F_OPTGET:
	case F_PVASSIGNCOUNT:
	case F_PVCHANNELCOUNT:
	case F_PVCONNECTCOUNT:
	case F_PVFLUSH:
	case F_SEQLOG:
		/* Any function that requires adding ssID as 1st parameter. */
		printf("seq_%s(ssId", fname);
		/* now fill in user-supplied parameters */
		foreach (ap, ep->func_args)
		{
			printf(", ");
			gen_expr(context, ap, 0);
		}
		printf(")");
		return TRUE;

	default:
		/* Not a special function */
		return FALSE;
	}
}

/* Check an event flag argument */
static void gen_ef_arg(
	char	*fname,		/* function name */
	Expr	*ap,		/* argument expression */
	int	index		/* argument index */
)
{
	Var	*vp;

	assert(ap);
	if (ap->type != E_VAR)
	{
		error_at_expr(ap,
		  "argument %d to built-in function %s must be an event flag\n",
		  index, fname);
		return;
	}
	vp = ap->extra.e_var;
	assert(vp->type);
	if (vp->type->tag != V_EVFLAG)
	{
		error_at_expr(ap,
		  "argument to built-in function %s must be an event flag\n", fname);
		return;
	}
	gen_var_access(vp);
}

/* Generate code for all event flag functions */
static void gen_ef_func(
	int	context,
	Expr	*ep,		/* function call expression */
	char	*fname,		/* function name */
	int	func_code	/* function code */
)
{
	Expr	*ap;

	ap = ep->func_args;

	if ((func_code == F_EFSET || func_code == F_EFCLEAR) && context == C_COND)
	{
		error_at_expr(ep,
		  "calling %s is not allowed inside a when condition\n", fname);
		return;
	}
	if (!ap)
	{
		error_at_expr(ep,
		  "built-in function %s requires an argument\n", fname);
		return;
	}
	printf("seq_%s(ssId, ", fname);
	gen_ef_arg(fname, ap, 1);
	printf(")");
}

/* Generate code for pv functions requiring a database variable.
   The channel id (index into channel array) is substituted for the variable.
   "add_length" => the array length (1 if not an array) follows the channel id 
   "num_params > 0" => add default (zero) parameters up to the spec. number */
static void gen_pv_func(
	int	context,
	Expr	*ep,		/* function call expression */
	char	*fname,		/* function name */
	int	func_code,	/* function code */
	int	add_length,	/* add array length after channel id */
	int	num_params	/* number of params to add (if omitted) */
)
{
	Expr	*ap, *subscr = 0;
	Var	*vp = NULL;
	int	num_extra_parms = 0;

	ap = ep->func_args;
	/* first parameter is always */
	if (ap == 0)
	{
		error_at_expr(ep,
			"function '%s' requires a parameter\n", fname);
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
		  fname);
		return;
	}

#ifdef	DEBUG
	report("gen_pv_func: fun=%s, var=%s\n", ep->value, vp->name);
#endif
	printf("seq_%s(ssId, ", fname);
	if (vp->assign == M_NONE)
	{
		error_at_expr(ep,
			"parameter 1 to '%s' was not assigned to a pv\n", fname);
		printf("?/*%s*/", vp->name);
	}
	else if (ap->type == E_SUBSCR && vp->assign != M_MULTI)
	{
		error_at_expr(ep,
			"parameter 1 to '%s' is subscripted but the variable "
			"it refers to has not been assigned to multiple pvs\n", fname);
		printf("%d/*%s*/", vp->index, vp->name);
	}
	else
	{
		printf("%d/*%s*/", vp->index, vp->name);
	}

	if (ap->type == E_SUBSCR)
	{
		/* e.g. pvPut(xyz[i+2]); => seq_pvPut(ssId, 3 + (i+2)); */
		printf(" + (");
		/* generate the subscript expression */
		gen_expr(context, subscr, 0);
		printf(")");
	}

	/* If requested, add length parameter (if subscripted variable,
	   length is always 1) */
	if (add_length)
	{
		if (ap->type != E_SUBSCR)
		{
			printf(", %d", type_array_length1(vp->type));
		}
		else
		{
			printf(", 1");
		}
	}

	/* Add any additional parameter(s) */
	foreach (ap, ap->next)
	{
		num_extra_parms++;
		printf(", ");
		if (func_code == F_PVSYNC)
		{
			gen_ef_arg(fname, ap, num_extra_parms+1);
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
		printf(", 0");
		num_extra_parms++;
	}

	/* Close the parameter list */
	printf(")");
#ifdef	DEBUG
	report("gen_pv_func: done (fun=%s, var=%s)\n", ep->value, vp->name);
#endif
}

/* Generate initializer for the UserVar structs. */
void gen_ss_user_var_init(Expr *ssp, int level)
{
	Var *vp;
	Expr *sp;

	assert(ssp->type == D_SS);
	printf("{\n");
	foreach(vp, ssp->extra.e_ss->var_list->first)
	{
		indent(level+1); gen_var_init(vp, level+1); printf(",\n");
	}
	foreach (sp, ssp->ss_states)
	{
		int s_empty;

		assert(sp->type == D_STATE);
		s_empty = !sp->extra.e_state->var_list->first;
		if (!s_empty)
		{
			indent(level+1); printf("{\n");
			foreach (vp, sp->extra.e_state->var_list->first)
			{
					indent(level+2); gen_var_init(vp, level+2);
					printf("%s\n", vp->next ? "," : "");
			}
			indent(level+1);
			printf("}%s\n", sp->next ? "," : "");
		}
	}
	indent(level); printf("}");
}

/* Generate initializer for the UserVar structs. */
static void gen_user_var_init(Expr *prog, int level)
{
	Var *vp;
	Expr *ssp;

	assert(prog->type == D_PROG);
	printf("{\n");
	/* global variables */
	foreach(vp, prog->extra.e_prog->first)
	{
		if (vp->type->tag >= V_CHAR)
		{
			indent(level+1); gen_var_init(vp, level+1); printf(",\n");
		}
	}

	foreach (ssp, prog->prog_statesets)
	{
		Expr *sp;
		int ss_empty = !ssp->extra.e_ss->var_list->first;
		if (ss_empty)
		{
			foreach (sp, ssp->ss_states)
			{
				if (sp->extra.e_state->var_list->first)
				{
					ss_empty = 0;
					break;
				}
			}
		}
		if (!ss_empty)
		{
			indent(level+1);
			gen_ss_user_var_init(ssp, level+1);
			printf("%s\n", ssp->next ? "," : "");
		}
	}
	indent(level); printf("}");
}

static void gen_prog_init_func(Expr *prog, int opt_reent)
{
	assert(prog->type == D_PROG);
	printf("\n/* Program init func */\n");
	printf("static void global_prog_init(struct %s *pVar)\n{\n", VAR_PREFIX);
	if (opt_reent)
	{
		indent(1); printf("*pVar = (struct %s)", VAR_PREFIX);
		gen_user_var_init(prog, 1);
		printf(";\n");
	}
	printf("}\n");
}

static void gen_prog_func(
	Expr *prog,
	const char *name,
	Expr *xp,
	void (*gen_body)(Expr *xp))
{
	assert(prog->type == D_PROG);
	printf("\n/* Program %s func */\n", name);
	printf("static void global_prog_%s(SS_ID ssId, struct %s *pVar)\n{\n",
		name, VAR_PREFIX);
	if (xp && gen_body) gen_body(xp);
	printf("}\n");
}
