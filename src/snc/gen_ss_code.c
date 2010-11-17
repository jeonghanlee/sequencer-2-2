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
	F_PVERROR,
	F_PVFLUSH,
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
	"pvError",
	"pvFlush",
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
static void gen_expr(int stmt_type, Expr *ep, int level);
static void gen_ef_func(int stmt_type, Expr *ep, char *fname, int func_code);
static void gen_pv_func(int stmt_type, Expr *ep,
	char *fname, int func_code, int add_length, int num_params);
#if 0
static void gen_pv_func_va(int stmt_type, Expr *ep, char *fname, int func_code);
#endif
static int special_func(int stmt_type, Expr *ep);
static int special_const(int stmt_type,	Expr *ep);

static void gen_prog_func(
	Expr *prog,
	const char *name,
	Expr *xp,
	void (*gen_body)(Expr *xp));
static void gen_prog_init_func(Expr *prog);
static void gen_ss_init_func(Expr *ssp, int opt_safe);

#define	EVENT_STMT	1
#define	ACTION_STMT	2
#define	OTHER_STMT	3

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

/* Generate state set C code from analysed syntax tree */
void gen_ss_code(Program *program)
{
	Expr	*prog = program->prog;
	Expr	*ssp;
	Expr	*sp;

	/* HACK: intialise globals */
	global_opt_reent = program->options.reent;
	global_sym_table = program->sym_table;

	/* Insert special names into symbol table */
	register_special_funcs();
	register_special_consts();

	/* Generate program init func */
	gen_prog_init_func(prog);

	/* Generate program entry func */
	if (prog->prog_entry)
		gen_prog_func(prog, "entry", prog->prog_entry, gen_entry_body);

	/* For each state set ... */
	foreach (ssp, prog->prog_statesets)
	{
		/* Generate state set init function */
		gen_ss_init_func(ssp, program->options.safe);

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

	/* Generate exit func code */
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
			if (vp->value)
			{
#if 0
				if (vp->type->tag == V_STRING)
				{
					error_at_expr(vp->decl,
					  "initialisation not allowed for variables of this type");
					return;
				}
#endif
				printf(" = ");
				gen_expr(OTHER_STMT, vp->value, 0);
			}
			printf(";\n");
		}
	}
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

	assert(xp->type = D_ENTRY);
	gen_local_var_decls(xp, 1);
	gen_defn_c_code(xp, 1);
	foreach (ep, xp->entry_stmts)
	{
		gen_expr(OTHER_STMT, ep, 1);
	}
}

static void gen_exit_body(Expr *xp)
{
	Expr	*ep;

	assert(xp->type = D_EXIT);
	gen_local_var_decls(xp, 1);
	gen_defn_c_code(xp, 1);
	foreach (ep, xp->exit_stmts)
	{
		gen_expr(OTHER_STMT, ep, 1);
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
	gen_expr(EVENT_STMT, ep->delay_args, 0);
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
			gen_expr(ACTION_STMT, ap, level+2);
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
		      gen_expr(EVENT_STMT, tp->when_cond, 0);
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
	char *pre = global_opt_reent ? "(pVar->" : "";
	char *post = global_opt_reent ? ")" : "";

	assert(vp);
	assert(vp->scope);

#ifdef DEBUG
	report("var_access: %s, scope=(%s,%s)\n",
		vp->name, expr_type_name(vp->scope), vp->scope->value);
#endif
	assert((1<<vp->scope->type) & scope_mask);

	if (vp->type->tag == V_EVFLAG)
	{
		printf("%d", vp->chan.evflag->index);
	}
	else if (vp->type->tag == V_NONE)
	{
		printf("%s", vp->name);
	}
	else if (vp->scope->type == D_PROG)
	{
		printf("%s%s%s", pre, vp->name, post);
	}
	else if (vp->scope->type == D_SS)
	{
		printf("%s%s_%s.%s%s", pre, VAR_PREFIX, vp->scope->value, vp->name, post);
	}
	else if (vp->scope->type == D_STATE)
	{
		printf("%s%s_%s.%s_%s.%s%s", pre, VAR_PREFIX,
			vp->scope->extra.e_state->var_list->parent_scope->value,
			VAR_PREFIX, vp->scope->value, vp->name, post);
	}
	else	/* compound or when stmt => generate a local C variable */
	{
		printf("%s", vp->name);
	}
}

void gen_string_assign(int stmt_type, Expr *left, Expr *right, int level)
{
	printf("(strncpy(");
	gen_expr(stmt_type, left, level);
	printf(", ");
	gen_expr(stmt_type, right, level);
	printf(", sizeof(string)), ");
	gen_expr(stmt_type, left, level);
	printf("[sizeof(string)-1] = '\\0')");
}

#if 0
int special_assign_op(int stmt_type, Expr *ep, int level)
{
	Expr *left = ep->binop_left;
	Expr *right = ep->binop_right;
	if (
		(left->type == E_VAR &&
		left->extra.e_var->type->tag == V_STRING)
		|| (left->type == E_SUBSCR
		&& left->subscr_operand->type == E_VAR
		&& type_base_type(left->subscr_operand->extra.e_var->type) == V_STRING)
	)
	{
		gen_string_assign(stmt_type, left, right, level);
		return TRUE;
	}
	return FALSE;
}
#endif

/* Recursively generate code for an expression (tree) */
static void gen_expr(
	int stmt_type,
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
			gen_expr(stmt_type, cep, level+1);
		}
		indent(level);
		printf("}\n");
		break;
	case S_STMT:
		gen_line_marker(ep);
		indent(level);
		gen_expr(stmt_type, ep->stmt_expr, 0);
		printf(";\n");
		break;
	case S_IF:
		gen_line_marker(ep);
		indent(level);
		printf("if (");
		gen_expr(stmt_type, ep->if_cond, 0);
		printf(")\n");
		cep = ep->if_then;
		gen_expr(stmt_type, cep, cep->type == S_CMPND ? level : level+1);
		if (ep->if_else != 0)
		{
			indent(level);
			printf("else\n");
			cep = ep->if_else;
			gen_expr(stmt_type, cep, cep->type == S_CMPND ? level : level+1);
		}
		break;
	case S_WHILE:
		gen_line_marker(ep);
		indent(level);
		printf("while (");
		gen_expr(stmt_type, ep->while_cond, 0);
		printf(")\n");
		cep = ep->while_stmt;
		gen_expr(stmt_type, cep, cep->type == S_CMPND ? level : level+1);
		break;
	case S_FOR:
		gen_line_marker(ep);
		indent(level);
		printf("for (");
		gen_expr(stmt_type, ep->for_init, 0);
		printf("; ");
		gen_expr(stmt_type, ep->for_cond, 0);
		printf("; ");
		gen_expr(stmt_type, ep->for_iter, 0);
		printf(")\n");
		cep = ep->for_stmt;
		gen_expr(stmt_type, cep, cep->type == S_CMPND ? level : level+1);
		break;
	case S_JUMP:
		indent(level);
		printf("%s;\n", ep->value);
		break;
	case S_CHANGE:
		if (stmt_type != ACTION_STMT)
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
		gen_expr(stmt_type, ep->subscr_operand, 0);
		printf("[");
		gen_expr(stmt_type, ep->subscr_index, 0);
		printf("]");
		break;
	case E_CONST:
		if (special_const(stmt_type, ep))
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
		if (special_func(stmt_type, ep))
			break;
		printf("%s(", ep->value);
		foreach (cep, ep->func_args)
		{
			gen_expr(stmt_type, cep, 0);
			if (cep->next)
				printf(", ");
		}
		printf(")");
		break;
	case E_TERNOP:
		gen_expr(stmt_type, ep->ternop_cond, 0);
		printf(" ? ");
		gen_expr(stmt_type, ep->ternop_then, 0);
		printf(" : ");
		gen_expr(stmt_type, ep->ternop_else, 0);
		break;
	case E_BINOP:
#if 0
		if (special_assign_op(stmt_type, ep, 0))
			break;
#endif
		gen_expr(stmt_type, ep->binop_left, 0);
		printf(" %s ", ep->value);
		gen_expr(stmt_type, ep->binop_right, 0);
		break;
	case E_PAREN:
		printf("(");
		gen_expr(stmt_type, ep->paren_expr, 0);
		printf(")");
		break;
	case E_PRE:
		printf("%s", ep->value);
		gen_expr(stmt_type, ep->pre_operand, 0);
		break;
	case E_POST:
		gen_expr(stmt_type, ep->post_operand, 0);
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

static int special_const(int stmt_type,	Expr *ep)
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
static int special_func(int stmt_type,	Expr *ep)
{
	char	*fname;		/* function name */
	Expr	*ap;		/* arguments */
	int	func_code;

	fname = ep->value;
	func_code = func_name_to_code(fname);
	if (func_code == F_NONE)
		return FALSE; /* not a special function */

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
		gen_ef_func(stmt_type, ep, fname, func_code);
		return TRUE;

	    case F_PVGETQ:
	    case F_PVFREEQ:
	    case F_PVTIMESTAMP:
	    case F_PVGETCOMPLETE:
	    case F_PVNAME:
	    case F_PVSTATUS:
	    case F_PVSEVERITY:
	    case F_PVMESSAGE:
	    case F_PVCONNECTED:
	    case F_PVASSIGNED:
	    case F_PVMONITOR:
	    case F_PVSTOPMONITOR:
	    case F_PVCOUNT:
	    case F_PVINDEX:
	    case F_PVDISCONNECT:
	    case F_PVASSIGN:
	    case F_PVSYNC:
		/* PV functions requiring a channel id */
		gen_pv_func(stmt_type, ep, fname, func_code, FALSE, 0);
		return TRUE;

	    case F_PVPUT:
	    case F_PVGET:
		/* PV functions requiring a channel id and defaulted
		   last 1 parameter */
		gen_pv_func(stmt_type, ep, fname, func_code, FALSE, 1);
		return TRUE;

	    case F_PVPUTCOMPLETE:
		/* PV functions requiring a channel id, an array length and
		   defaulted last 2 parameters */
		gen_pv_func(stmt_type, ep, fname, func_code, TRUE, 2);
		return TRUE;

	    case F_PVFLUSH:
	    case F_PVERROR:
	    case F_PVCHANNELCOUNT:
	    case F_PVCONNECTCOUNT:
	    case F_PVASSIGNCOUNT:
		/* pv functions NOT requiring a channel structure */
		printf("seq_%s(ssId)", fname);
		return TRUE;

	    case F_SEQLOG:
	    case F_MACVALUEGET:
	    case F_OPTGET:
		/* Any funtion that requires adding ssID as 1st parameter.
		 * Note:  name is changed by prepending "seq_". */
		printf("seq_%s(ssId", fname);
		/* now fill in user-supplied parameters */
		foreach (ap, ep->func_args)
		{
			printf(", ");
			gen_expr(stmt_type, ap, 0);
		}
		printf(")");
		return TRUE;

	    default:
		/* Not a special function */
		return FALSE;
	}
}

/* Generate code for all event flag functions */
static void gen_ef_func(
	int	stmt_type,
	Expr	*ep,		/* function call expression */
	char	*fname,		/* function name */
	int	func_code	/* function code */
)
{
	Expr	*ap;
	Var	*vp = 0;

	ap = ep->func_args;

	if (ap != 0 && ap->type == E_VAR)
	{
		vp = ap->extra.e_var;
	}
	if ((func_code == F_EFSET || func_code == F_EFCLEAR) && stmt_type == EVENT_STMT)
	{
		error_at_expr(ep, "%s cannot be used in a when condition\n", fname);
		return;
	}
	if (vp->type->tag != V_EVFLAG)
	{
		error_at_expr(ep, "argument to '%s' must be an event flag\n", fname);
		return;
	}
	printf("seq_%s(ssId, %d)", fname, vp->chan.evflag->index);
}

#if 0
static void gen_pv_func_va(
	int	stmt_type,
	Expr	*ep,		/* function call expression */
	char	*fname,		/* function name */
	int	func_code	/* function code */
)
{
	Expr	*ap = 0;
	int	nargs = 0;

	foreach(ap, ep->func_args)
	{
		nargs++;
	}
	printf("seq_%s(ssId, %d", fname, nargs);
	foreach(ap, ep->func_args)
	{
		Expr	*subscr = 0;
		Var	*vp = 0;
		char	*vn = "?";
		int	id = -1;

		if (ap->type == E_VAR)
		{
			/* plain <pv variable> */
			vp = ap->extra.e_var;
		}
		else if (ap->type == E_SUBSCR)
		{
			/* <pv variable>[<expression>] */
			Expr *ep2 = ap->subscr_operand;
			subscr = ap->subscr_index;
			if (ep2->type == E_VAR)
			{
				vp = ep2->extra.e_var;
			}
		}
		assert(vp != 0);
		vn = vp->name;
		if (vp->assign == M_NONE)
		{
			error_at_expr(ep,
				"parameter to '%s' was not assigned to a pv\n", fname);
		}
		else
		{
			id = vp->index;
		}
		printf(", %d /* %s */", id, vn);
		if (ap->type == E_SUBSCR) /* subscripted variable? */
		{	/* e.g. pvPut(xyz[i+2]); => seq_pvPut(ssId, 3 + (i+2)); */
			printf(" + (");
			/* evalute the subscript expression */
			gen_expr(stmt_type, subscr, 0);
			printf(")");
		}
	}
	printf(")");
}
#endif

/* Generate code for pv functions requiring a database variable.
   The channel id (index into channel array) is substituted for the variable.
   "add_length" => the array length (1 if not an array) follows the channel id 
   "num_params > 0" => add default (zero) parameters up to the spec. number */
static void gen_pv_func(
	int	stmt_type,
	Expr	*ep,		/* function call expression */
	char	*fname,		/* function name */
	int	func_code,	/* function code */
	int	add_length,	/* add array length after channel id */
	int	num_params	/* number of params to add (if omitted) */
)
{
	Expr	*ap, *subscr = 0;
	Var	*vp;
	char	*vn;
	int	id;
	int	num;

	ap = ep->func_args; /* ptr to 1-st parameter in the function */
	if (ap == 0)
	{
		error_at_expr(ep,
			"function '%s' requires a parameter\n", fname);
		return;
	}

	vp = 0;
	vn = "?";
	id = -1;
	if (ap->type == E_VAR)
	{
		vp = ap->extra.e_var;
	}
	else if (ap->type == E_SUBSCR)
	{
		/* Form should be: <pv variable>[<expression>] */
		Expr *ep2 = ap->subscr_operand;
		subscr = ap->subscr_index;
		if (ep2->type == E_VAR)
		{
			vp = ep2->extra.e_var;
		}
	}

	assert(vp != 0);
#ifdef	DEBUG
	report("gen_pv_func: fun=%s, var=%s\n", ep->value, vp->name);
#endif
	vn = vp->name;
	if (vp->assign == M_NONE)
	{
		error_at_expr(ep,
			"parameter to '%s' was not assigned to a pv\n", fname);
	}
	else
	{
		id = vp->index;
	}

	printf("seq_%s(ssId, %d /* %s */", fname, id, vn);

	if (ap->type == E_SUBSCR) /* subscripted variable? */
	{	/* e.g. pvPut(xyz[i+2]); => seq_pvPut(ssId, 3 + (i+2)); */
		printf(" + (");
		/* evalute the subscript expression */
		gen_expr(stmt_type, subscr, 0);
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
	num = 0;
	foreach (ap, ap->next)
	{
		printf(", ");
		gen_expr(stmt_type, ap, 0);
		num++;
	}

	/* If not enough additional parameter(s) were specified, add
	   extra zero parameters */
	for (; num < num_params; num++)
	{
		printf(", 0");
	}

	/* Close the parameter list */
	printf(")");
#ifdef	DEBUG
	report("gen_pv_func: done (fun=%s, var=%s)\n", ep->value, vp->name);
#endif
}

/* Generate initialisation code for one element of the UserVar struct. */
static int iter_user_var_init(Expr *dp, Expr *scope, void *parg)
{
	assert(dp);
	assert(dp->type == D_DECL);

	Var *vp = dp->extra.e_decl;

	assert(vp);
	if (vp->value && vp->decl)
	{
		if (vp->type->tag < V_CHAR)
		{
			error_at_expr(vp->decl,
			  "initialisation not allowed for variables of this type");
		}
		else if (type_base_type(vp->type) == V_STRING)
		{
			Expr *ep = new(Expr);
			ep->type = E_VAR;
			ep->extra.e_var = vp;
			indent(1);
			gen_string_assign(OTHER_STMT, ep, vp->value, 1);
			printf(";\n");
		}
		else
		{
			gen_line_marker(dp);
			indent(1);
			gen_var_access(vp);
			printf(" = ");
			gen_expr(OTHER_STMT, vp->value, 0);
			printf(";\n");
		}
	}
	return FALSE;		/* do not descend into children */
}

/* Generate initialisation for variables with global lifetime. */
static void gen_user_var_init(Expr *ep, int stop_mask)
{
	const int type_mask = (1<<D_DECL);
	traverse_expr_tree(ep, type_mask, stop_mask, 0, iter_user_var_init, 0);
}

static void gen_prog_init_func(Expr *prog)
{
	const int global_stop_mask = ~((1<<D_DECL)|(1<<D_PROG));

	assert(prog->type = D_PROG);
	printf("\n/* Program init func */\n");
	printf("static void global_prog_init(struct %s *pVar)\n{\n", VAR_PREFIX);
	/* initialize global variables */
	gen_user_var_init(prog, global_stop_mask);
	printf("}\n");
}

static void gen_ss_init_func(Expr *ssp, int opt_safe)
{
	const int ss_stop_mask = ~((1<<D_DECL)|(1<<D_SS)|(1<<D_STATE));

	assert(ssp->type = D_SS);
	printf("\n/* Init func for state set %s */\n", ssp->value);
	printf("static void ss_%s_init(struct %s *pVar)\n{\n",
		ssp->value, VAR_PREFIX);
	/* initialize state set and state variables */
	gen_user_var_init(ssp, ss_stop_mask);
	printf("}\n");
}

static void gen_prog_func(
	Expr *prog,
	const char *name,
	Expr *xp,
	void (*gen_body)(Expr *xp))
{
	assert(prog->type = D_PROG);
	printf("\n/* Program %s func */\n", name);
	printf("static void global_prog_%s(SS_ID ssId, struct %s *pVar)\n{\n",
		name, VAR_PREFIX);
	if (xp && gen_body) gen_body(xp);
	printf("}\n");
}
