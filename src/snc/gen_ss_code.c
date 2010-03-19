/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	DESCRIPTION: gen_ss_code.c -- routines to generate state set code

	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Changed find_var() to findVar().
28apr92,ajk	Implemented efClear() & efTestAndClear().
01mar94,ajk	Changed table generation to the new structures defined 
		in seqCom.h.
09aug96,wfl	Supported pvGetQ().
13aug96,wfl	Supported pvFreeQ().
23jun97,wfl	Avoided SEGV if variable or event flag was undeclared.
13jan98,wfl     Fixed handling of compound expressions, using E_COMMA.
29apr99,wfl     Avoided compilation warnings.
29apr99,wfl	Removed unnecessary include files.
06jul99,wfl	Cosmetic changes to improve look of generated C code
07sep99,wfl	Added support for local declarations (not yet complete);
		Added support for "pvName", "pvMessage" and pvPutComplete";
		Supported "pv" functions with array length and optional parms;
		Added sequencer variable name to generated seq_pv*() calls
22sep99,grw	Supported entry and exit actions
18feb00,wfl	More partial support for local declarations (still not done)
31mar00,wfl	Put 'when' code in a block (allows local declarations);
		supported entry handler
***************************************************************************/
#include	<stdio.h>
#include	<string.h>
#include	<assert.h>

#include	"parse.h"
#include	"analysis.h"
#include	"gen_code.h"
#include	"snc_main.h"
#include	"sym_table.h"

/* #define DEBUG */

static const int impossible = 0;

enum fcode {
	F_NONE, F_DELAY, F_EFSET, F_EFTEST, F_EFCLEAR, F_EFTESTANDCLEAR,
	F_PVGET, F_PVGETQ, F_PVFREEQ, F_PVPUT, F_PVTIMESTAMP, F_PVASSIGN,
	F_PVMONITOR, F_PVSTOPMONITOR, F_PVCOUNT, F_PVINDEX, F_PVNAME,
	F_PVSTATUS, F_PVSEVERITY, F_PVMESSAGE, F_PVFLUSH, F_PVERROR,
	F_PVGETCOMPLETE, F_PVASSIGNED, F_PVCONNECTED, F_PVPUTCOMPLETE,
	F_PVCHANNELCOUNT, F_PVCONNECTCOUNT, F_PVASSIGNCOUNT,
	F_PVDISCONNECT, F_SEQLOG, F_MACVALUEGET, F_OPTGET, F_CODE_LAST
};

static char *fcode_str[] = {
	NULL, "delay", "efSet", "efTest", "efClear", "efTestAndClear",
	"pvGet", "pvGetQ", "pvFreeQ", "pvPut", "pvTimeStamp", "pvAssign",
	"pvMonitor", "pvStopMonitor", "pvCount", "pvIndex", "pvName",
	"pvStatus", "pvSeverity", "pvMessage", "pvFlush", "pvError",
	"pvGetComplete", "pvAssigned", "pvConnected", "pvPutComplete",
	"pvChannelCount", "pvConnectCount", "pvAssignCount",
	"pvDisconnect", "seqLog", "macValueGet", "optGet", NULL
};

static void gen_local_var_decls(Expr *scope, int level);
static void gen_state_func(
	const char *ss_name,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *sp, int level),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
);
static void gen_entry_body(Expr *xp, int level);
static void gen_exit_body(Expr *xp, int level);
static void gen_delay_body(Expr *xp, int level);
static void gen_event_body(Expr *xp, int level);
static void gen_action_body(Expr *xp, int level);
static void gen_delay(Expr *ep, Expr *scope, void *parg);
static void gen_expr(int stmt_type, Expr *ep, int level);
static void gen_ef_func(int stmt_type, Expr *ep, char *fname, int func_code);
static void gen_pv_func(int stmt_type, Expr *ep,
	char *fname, int func_code, int add_length, int num_params);
static void gen_entry_handler(Expr *prog);
static void gen_exit_handler(Expr *prog);
static int special_func(int stmt_type, Expr *ep);

#define	EVENT_STMT	1
#define	ACTION_STMT	2

/*
 * HACK: use global variable to symbol table available to subroutines
 */
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

/* Generate state set C code from analysed syntax tree */
void gen_ss_code(Program *program)
{
	Expr	*ssp;
	Expr	*sp;

	/* HACK: intialise global variable */
	global_sym_table = program->sym_table;

	/* Insert special function names into symbol table */
	register_special_funcs();

	/* Generate entry handler code */
	gen_entry_handler(program->prog);

	/* For each state set ... */
	foreach (ssp, program->prog->prog_statesets)
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
				"Event", "E", "long",
				", short *pTransNum, short *pNextState");
			/* Generate action processing function */
			gen_state_func(ssp->value, sp->value,
				sp->state_whens, gen_action_body,
				"Action", "A", "void", ", short transNum");
		}
	}

	/* Generate exit handler code */
	gen_exit_handler(program->prog);
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
		if (vp->decl && vp->type != V_EVFLAG && vp->type != V_NONE)
		{
			gen_line_marker(vp->decl);
			indent(level);
			gen_var_decl(vp);
		}
	}
}

static void gen_state_func(
	const char *ss_name,
	const char *state_name,
	Expr *xp,
	void (*gen_body)(Expr *sp, int level),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
)
{
	printf("\n/* %s function for state \"%s\" in state set \"%s\" */\n",
 		title, state_name, ss_name);
	printf("static %s %s_%s_%s(SS_ID ssId, struct %s *pVar%s)\n{\n",
		rettype, prefix, ss_name, state_name, SNL_PREFIX, extra_args);
	gen_body(xp, 1);
	printf("}\n");
}

static void gen_entry_body(Expr *xp, int level)
{
	Expr	*ep;

	assert(xp->type = D_ENTRY);
	gen_local_var_decls(xp, level);
	gen_defn_c_code(xp, level);
	foreach (ep, xp->entry_stmts)
	{
		gen_expr(ACTION_STMT, ep, 1);
	}
}

static void gen_exit_body(Expr *xp, int level)
{
	Expr	*ep;

	assert(xp->type = D_EXIT);
	gen_local_var_decls(xp, level);
	gen_defn_c_code(xp, level);
	foreach (ep, xp->exit_stmts)
	{
		gen_expr(ACTION_STMT, xp, 1);
	}
}

/* Generate a function for each state that sets up delay processing:
 * This function gets called prior to the event function to guarantee
 * that the initial delay value specified in delay() calls are used.
 * Each delay() call is assigned a (per state) unique id.  The maximum
 * number of delays is recorded in the state set structure.
 */
static void gen_delay_body(Expr *xp, int level)
{
	Expr	*tp;

	/* for each transition */
	foreach (tp, xp)
	{
		assert(tp->type == D_WHEN);
		traverse_expr_tree(tp->when_cond, 1<<E_DELAY, 0, 0, gen_delay, 0);
	}
}

/* Evaluate the expression within a delay() function and generate
 * a call to seq_delayInit().  Adds ssId, delay id parameters and cast to
 * double.
 * Example:  seq_delayInit(ssId, 1, (double)(<some expression>));
 */
static void gen_delay(Expr *ep, Expr *scope, void *parg)
{
	assert(ep->type == E_DELAY);
	gen_line_marker(ep);
	/* Generate 1-st part of function w/ 1-st 2 parameters */
	indent(1); printf("seq_delayInit(ssId, %d, (", ep->extra.e_delay);
	/* generate the 3-rd parameter (an expression) */
	gen_expr(EVENT_STMT, ep->delay_args, 0);
	/* Complete the function call */
	printf("));\n");
}

/* Generate action processing functions:
   Each state has one action routine.  It's name is derived from the
   state set name and the state name.
*/
static void gen_action_body(Expr *xp, int level)
{
	Expr	*tp;
	int	trans_num;

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
static void gen_event_body(Expr *xp, int level)
{
	Expr	*tp;
	int	trans_num;

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
		indent(level+1);
		if (next_sp == 0)
		{
			printf("*pNextState = state_%s_does_not_exist;\n", "tp->value");
		}
		else
		{
			printf("*pNextState = %d;\n", next_sp->extra.e_state->index);
		}
		indent(level+1); printf("*pTransNum = %d;\n", trans_num);
		indent(level+1); printf("return TRUE;\n");
		indent(level); printf("}\n");
		trans_num++;
	}
	indent(level); printf("return FALSE;\n");
}

static void gen_var_access(Var *vp)
{
	char *pVar_arr = "pVar->";

#ifdef	DEBUG
	report("var_access: %s, scope=(%s,%s)\n",
		vp->name, expr_type_name(vp->scope), vp->scope->value);
#endif

	if (vp->type == V_NONE || vp->type == V_EVFLAG)
	{
		printf("%s", vp->name);
	}
	else if (vp->scope->type == D_PROG)
	{
		printf("(%s%s)", pVar_arr, vp->name);
	}
	else if (vp->scope->type == D_SS)
	{
		printf("(%s%s_ss_%s.%s)",
			pVar_arr, SNL_PREFIX, vp->scope->value, vp->name);
	}
	else if (vp->scope->type == D_STATE)
	{
		printf("(%s%s_ss_%s.%s_state_%s.%s)",
			pVar_arr, SNL_PREFIX,
			vp->scope->extra.e_state->var_list->parent_scope->value, SNL_PREFIX,
			vp->scope->value, vp->name);
	}
	else	/* compound or when => generate a local C variable */
	{
		printf("%s", vp->name);
	}
}

int special_assign_op(int stmt_type, Expr *ep, int level)
{
	if (ep->binop_left->type == E_VAR
		&& ep->binop_left->extra.e_var->type == V_STRING)
	{
		printf("strncpy(");
		gen_var_access(ep->binop_left->extra.e_var);
		printf(", ");
		gen_expr(stmt_type, ep->binop_right, level);
		printf(", MAX_STRING_SIZE-1)");
		return TRUE;
	}
	return FALSE;
}

/* Recursively generate code for an expression (tree) */
static void gen_expr(
	int stmt_type,
	Expr *ep,		/* expression to generate code for */
	int level		/* indentation level */
)
{
	Expr	*cep;		/* child expression */
	Var	*vp;

	if (ep == 0)
		return;

#ifdef	DEBUG
	report("gen_expr(%s,%s)\n", expr_type_name(ep), ep->value);
#endif

	switch(ep->type)
	{
	/* Definitions */
	case D_DECL:
		vp = ep->extra.e_decl;
		assert(vp != 0);
		assert(vp->decl != 0);
		if (vp->type != V_EVFLAG && vp->type != V_NONE)
		{
			gen_line_marker(vp->decl);
			indent(level);
			gen_var_decl(vp);
		}
		break;
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
	/* Expressions */
	case E_VAR:
		gen_var_access(ep->extra.e_var);
		break;
	case E_CONST:
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
		if (special_assign_op(stmt_type, ep, 0))
			break;
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
	case E_SUBSCR:
		gen_expr(stmt_type, ep->subscr_operand, 0);
		printf("[");
		gen_expr(stmt_type, ep->subscr_index, 0);
		printf("]");
		break;
	/* C-code can be either definition, statement, or expression */
	case T_TEXT:
		indent(level);
		printf("%s\n", ep->value);
		break;
#if 0
	default:
		error_at_expr(ep, "internal error: unhandled expression type %s=%s\n",
			expr_type_name(ep), ep->value);
#endif
	}
}

/* Process special function (returns TRUE if this is a special function)
	Checks for one of the following special functions:
	 - event flag functions, e.g. efSet()
	 - process variable functions, e.g. pvPut()
	 - delay()
	 - macValueGet()
	 - seqLog()
*/
static int special_func(
	int stmt_type,
	Expr *ep		/* function call expression */
)
{
	char	*fname;		/* function name */
	Expr	*ap;		/* arguments */
	int	func_code;

	fname = ep->value;
	func_code = func_name_to_code(fname);
	if (func_code == F_NONE)
		return FALSE; /* not a special function */

#ifdef	DEBUG
	report("special_func: func_code=%d\n", func_code);
#endif	/*DEBUG*/
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
		/* DB functions NOT requiring a channel structure */
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
	if (vp == 0 || vp->type != V_EVFLAG)
	{
		error_at_expr(ep, "argument to '%s' must be an event flag\n", fname);
	}
	if ((func_code == F_EFSET || func_code == F_EFCLEAR) && stmt_type == EVENT_STMT)
	{
		error_at_expr(ep, "%s cannot be used in a when condition\n", fname);
	}
	else
	{
		printf("seq_%s(ssId, %s)", fname, vp->name);
	}
}

/* Generate code for pv functions requiring a database variable.
 * The channel id (index into channel array) is substituted for the variable
 *
 * "add_length" => the array length (1 if not an array) follows the channel id 
 * "num_params > 0" => add default (zero) parameters up to the spec. number
 */
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
	Chan	*cp;
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
		/* Form should be: <db variable>[<expression>] */
		Expr *ep2 = ap->subscr_operand;
		subscr = ap->subscr_index;
		if (ep2->type == E_VAR)
		{
			vp = ep2->extra.e_var;
		}
	}

	if (vp == 0)
	{
		error_at_expr(ep,
			"parameter to '%s' is not a declared variable\n", fname);
		cp = 0;
	}
	else
	{
#ifdef	DEBUG
		report("gen_pv_func: fun=%s, var=%s\n", ep->value, vp->name);
#endif	/*DEBUG*/
		vn = vp->name;
		cp = vp->chan;
		if (cp == 0)
		{
			error_at_expr(ep,
				"parameter to '%s' was not assigned to a pv\n", fname);
		}
		else
		{
			id = cp->index;
		}
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
		if (vp != 0 && ap->type != E_SUBSCR)
		{
			printf(", %d", vp->length1);
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
#endif	/*DEBUG*/
}

static void var_init(Expr *dp, Expr *scope, void *parg)
{
	assert(dp);
	assert(dp->type == D_DECL);

	Var *vp = dp->extra.e_decl;
	
	assert(vp);
	if (vp->value && vp->decl)
	{
		if (vp->type == V_NONE || vp->type == V_EVFLAG || vp->type == V_STRING)
		{
			error_at_expr(vp->decl,
			  "initialisation not allowed for variables of this type");
			return;
		}
		indent(1);
		gen_var_access(vp);
		printf(" = ");
		gen_expr(ACTION_STMT, vp->value, 0);
		printf(";\n");
	}
}

static void gen_struct_var_init(Expr *prog)
{
	const int type_mask = (1<<D_DECL);
	const int stop_mask = ~((1<<D_DECL) | has_scope_mask);
	traverse_expr_tree(prog, type_mask, stop_mask, 0, var_init, 0);
}

/* Generate global entry handler code */
static void gen_entry_handler(Expr *prog)
{
	assert(prog->type = D_PROG);
	printf("\n/* Entry handler */\n");
	printf("static void entry_handler(SS_ID ssId, struct %s *pVar)\n{\n", SNL_PREFIX);
	gen_struct_var_init(prog);
	if (prog->prog_entry)
		gen_entry_body(prog->prog_entry, 1);
	printf("}\n");
}

/* Generate global exit handler code */
static void gen_exit_handler(Expr *prog)
{
	assert(prog->type = D_PROG);
	printf("\n/* Exit handler */\n");
	printf("static void exit_handler(SS_ID ssId, struct %s *pVar)\n{\n", SNL_PREFIX);
	if (prog->prog_exit)
		gen_exit_body(prog->prog_exit, 1);
	printf("}\n");
}
