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

#define DEBUG

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

static void indent(int level);
static void gen_var_decls(Expr *scope, int level);
static void gen_state_func(
	Expr *sp,
	const char *ss_name,
	void (*gen_body)(Expr *sp),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
);
static void gen_entry_body(Expr *sp);
static void gen_exit_body(Expr *sp);
static void gen_delay_body(Expr *sp);
static void gen_event_body(Expr *sp);
static void gen_action_body(Expr *sp);
static void gen_delay(Expr *ep, Expr *scope, void *parg);
static void gen_expr(int stmt_type, Expr *ep, Expr *sp, int level);
static void gen_ef_func(int stmt_type, Expr *ep, char *fname, int func_code);
static void gen_pv_func(int stmt_type, Expr *ep, Expr *sp,
	char *fname, int func_code, int add_length, int num_params);
static void gen_entry_handler(Expr *expr_list);
static void gen_exit_handler(Expr *expr_list);
static int special_func(int stmt_type, Expr *ep, Expr *sp);

#define	EVENT_STMT	1
#define	ACTION_STMT	2
#define	DELAY_STMT	3
#define	ENTRY_STMT	4
#define	EXIT_STMT	5

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

/* Generate state set C code from analysed syntax tree */
void gen_ss_code(Program *program)
{
	Expr	*ssp;
	Expr	*sp;

	/* HACK: intialise globals */
	global_opt_reent = program->options.reent;
	global_sym_table = program->sym_table;

	/* Insert special function names into symbol table */
	register_special_funcs();

	/* Generate entry handler code */
	gen_entry_handler(program->prog);

	/* For each state set ... */
	foreach (ssp, program->prog->prog_statesets)
	{
#if 0
		printf("/* Code for state set \"%s\" */\n", ssp->value);
		gen_var_decls(ssp, 0);
#endif

		/* For each state ... */
		foreach (sp, ssp->ss_states)
		{
#if 0
			printf("\n/* Code for state \"%s\" in state set \"%s\" */\n",
				sp->value, ssp->value);
			gen_var_decls(sp, 0);
#endif

			/* Generate entry and exit functions */
			if (sp->state_entry != 0)
				gen_state_func(sp, ssp->value, gen_entry_body,
					"Entry", "I", "void", "");
			if (sp->state_exit != 0)
				gen_state_func(sp, ssp->value, gen_exit_body,
					"Exit", "O", "void", "");
			/* Generate function to set up for delay processing */
			gen_state_func(sp, ssp->value, gen_delay_body,
				"Delay", "D", "void", "");
			/* Generate event processing function */
			gen_state_func(sp, ssp->value, gen_event_body,
				"Event", "E", "long",
				", short *pTransNum, short *pNextState");
			/* Generate action processing function */
			gen_state_func(sp, ssp->value, gen_action_body,
				"Action", "A", "void", ", short transNum");
		}
	}

	/* Generate exit handler code */
	gen_exit_handler(program->prog->prog_exit);
}

/* Generate a C variable declaration for each variable declared in SNL
   (except the top-levbel vars, see gen_global_var_decls in gen_code).
 */
static void gen_var_decls(Expr *scope, int level)
{
	Var	*vp;
	VarList	*var_list;

	var_list = *pvar_list_from_scope(scope);

	indent(level);
	if (scope->type == D_SS)
		printf("struct UserVar_ss_%s {\n", scope->value);
	else if (scope->type == D_STATE)
		printf("struct UserVar_ss_%s_state_%s {\n",
			var_list->parent_scope->value, scope->value);

	/* Convert internal type to `C' type */
	foreach (vp, var_list->first)
	{
		if (vp->decl)
			gen_line_marker(vp->decl);
		indent(level+1);
		gen_var_decl(vp, "");
	}

	indent(level);
	if (scope->type == D_SS || scope->type == D_STATE)
		printf("};\n");
}

static void gen_state_func(
	Expr *sp,
	const char *ss_name,
	void (*gen_body)(Expr *sp),
	const char *title,
	const char *prefix,
	const char *rettype,
	const char *extra_args
)
{
	printf("\n/* %s function for state \"%s\" in state set \"%s\" */\n",
 		title, sp->value, ss_name);
	printf("static %s %s_%s_%s(SS_ID ssId, struct UserVar *pVar%s)\n{\n",
		rettype, prefix, ss_name, sp->value, extra_args);
	gen_body(sp);
	printf("}\n");
}

static void gen_entry_body(Expr *sp)
{
	Expr	*ep;

	gen_var_decls(sp->state_entry, 1);
	gen_defn_c_code(sp->state_entry);
	foreach (ep, sp->state_entry->entry_stmts)
	{
		gen_expr(ACTION_STMT, ep, sp, 1);
	}
}

static void gen_exit_body(Expr *sp)
{
	Expr	*ep;

	gen_var_decls(sp->state_exit, 1);
	gen_defn_c_code(sp->state_exit);
	foreach (ep, sp->state_exit->exit_stmts)
	{
		gen_expr(ACTION_STMT, ep, sp, 1);
	}
}

/* Generate a function for each state that sets up delay processing:
 * This function gets called prior to the event function to guarantee
 * that the initial delay value specified in delay() calls are used.
 * Each delay() call is assigned a (per state) unique id.  The maximum
 * number of delays is recorded in the state set structure.
 */
static void gen_delay_body(Expr *sp)
{
	Expr	*tp;

	/* for each transition */
	foreach (tp, sp->state_whens)
	{
		assert(tp->type == D_WHEN);
		traverse_expr_tree(tp->when_cond, 1<<E_DELAY, 0, 0, gen_delay, sp);
	}
}

/* Evaluate the expression within a delay() function and generate
 * a call to seq_delayInit().  Adds ssId, delay id parameters and cast to
 * double.
 * Example:  seq_delayInit(ssId, 1, (double)(<some expression>));
 */
static void gen_delay(Expr *ep, Expr *scope, void *parg)
{
	Expr *sp = (Expr *)parg;

	assert(ep->type == E_DELAY);
	gen_line_marker(ep);
	/* Generate 1-st part of function w/ 1-st 2 parameters */
	printf("\tseq_delayInit(ssId, %d, (", ep->extra.e_delay);
	/* generate the 3-rd parameter (an expression) */
	gen_expr(EVENT_STMT, ep->delay_args, sp, 0);
	/* Complete the function call */
	printf("));\n");
}

/* Generate action processing functions:
   Each state has one action routine.  It's name is derived from the
   state set name and the state name.
*/
static void gen_action_body(Expr *sp)
{
	Expr	*tp;
	int	trans_num;

	/* "switch" statment based on the transition number */
	printf("\tswitch(transNum)\n\t{\n");
	trans_num = 0;

	/* For each transition ("when" statement) ... */
	foreach (tp, sp->state_whens)
	{
		Expr *ap;

		assert(tp->type == D_WHEN);
		/* one case for each transition */
		printf("\tcase %d:\n", trans_num);
		/* block within case permits local variables */
		printf("\t\t{\n");
		/* for each definition insert corresponding code */
		gen_var_decls(tp, 1);
		gen_defn_c_code(tp);
		if (tp->when_defns)
			printf("\n");
		/* for each action statement insert action code */
		foreach (ap, tp->when_stmts)
		{ 
			gen_expr(ACTION_STMT, ap, sp, 3);
		}
		/* end of block */
		printf("\t\t}\n");
		/* end of case */
		printf("\t\treturn;\n");
		trans_num++;
	}
	/* end of switch stmt */
	printf("\t}\n");
}

/* Generate a C function that checks events for a particular state */
static void gen_event_body(Expr *sp)
{
	Expr	*tp;
	int	trans_num;

	trans_num = 0;
	/* For each transition generate an "if" statement ... */
	foreach (tp, sp->state_whens)
	{
		Expr *next_sp;

		assert(tp->type == D_WHEN);
		if (tp->when_cond != 0)
			gen_line_marker(tp->when_cond);
		printf("\tif (");
		if (tp->when_cond == 0)
		      printf("TRUE");
		else
		      gen_expr(EVENT_STMT, tp->when_cond, sp, 0);
		printf(")\n\t{\n");

		next_sp = tp->extra.e_when->next_state;
		if (next_sp == 0)
		{
			printf("\t\t*pNextState = state_%s_does_not_exist;\n", "tp->value");
		}
		else
		{
			printf("\t\t*pNextState = %d;\n", next_sp->extra.e_state->index);
		}
		printf("\t\t*pTransNum = %d;\n", trans_num);
		printf("\t\treturn TRUE;\n\t}\n");
		trans_num++;
	}
	printf("\treturn FALSE;\n");
}

void gen_var_access(Expr *ep)
{
	Var *vp = ep->extra.e_var;
	char *prefix = global_opt_reent ? "pVar->" : "";

#ifdef	DEBUG
	report_at_expr(ep, "var_access: '%s', scope=%s,'%s')\n",
		vp->name, expr_type_name(vp->scope), vp->scope->value);
#endif

	if (vp->type == V_NONE || vp->type == V_EVFLAG)
	{
		printf("%s", ep->value);
	}
	else if (vp->scope->type == D_PROG)
	{
		printf("%s%s", prefix, ep->value);
	}
	else if (vp->scope->type == D_SS)
	{
		printf("%sUserVar_ss_%s.%s",
			prefix, vp->scope->value, ep->value);
	}
	else if (vp->scope->type == D_STATE)
	{
		printf("%sUserVar_ss_%s.UserVar_state_%s.%s",
			prefix,
			vp->scope->extra.e_state->var_list->parent_scope->value,
			vp->scope->value, ep->value);
	}
	else	/* compound or when => must be a local variable */
	{
		printf("%s", ep->value);
	}
}

/* Recursively generate code for an expression (tree) */
static void gen_expr(
	int stmt_type,		/* EVENT_STMT, ACTION_STMT, or DELAY_STMT */
	Expr *ep,		/* ptr to expression */
	Expr *sp,		/* ptr to current State struct */
	int level		/* indentation level */
)
{
	Expr	*cep;		/* child expression */

	if (ep == 0)
		return;

	switch(ep->type)
	{
#if 0
	case D_DECL:
		if (stmt_type == ACTION_STMT) gen_line_marker(ep->children[]);
		indent(level);
		printf("%s ",ep->value);
		gen_expr(stmt_type, ep->children[], sp, 0);
		if (ep->children[] != 0)
		{
			printf(" = ");
			gen_expr(stmt_type, ep->children[], sp, 0);
		}
		printf(";\n");
		break;
#endif
	/* Statements */
	case S_CMPND:
		indent(level);
		printf("{\n");
		foreach (cep, ep->cmpnd_stmts)
		{
			gen_expr(stmt_type, cep, sp, level+1);
		}
		indent(level);
		printf("}\n");
		break;
	case S_STMT:
		if (stmt_type == ACTION_STMT) gen_line_marker(ep);
		indent(level);
		gen_expr(stmt_type, ep->stmt_expr, sp, 0);
		printf(";\n");
		break;
	case S_IF:
		if (stmt_type == ACTION_STMT) gen_line_marker(ep);
		indent(level);
		printf("if (");
		gen_expr(stmt_type, ep->if_cond, sp, 0);
		printf(")\n");
		cep = ep->if_then;
		gen_expr(stmt_type, cep, sp, cep->type == S_CMPND ? level : level+1);
		if (ep->if_else != 0)
		{
			printf("else\n");
			cep = ep->if_else;
			gen_expr(stmt_type, cep, sp, cep->type == S_CMPND ? level : level+1);
		}
		break;
	case S_WHILE:
		if (stmt_type == ACTION_STMT) gen_line_marker(ep);
		indent(level);
		printf("while (");
		gen_expr(stmt_type, ep->while_cond, sp, 0);
		printf(")\n");
		cep = ep->while_stmt;
		gen_expr(stmt_type, cep, sp, cep->type == S_CMPND ? level : level+1);
		break;
	case S_FOR:
		if (stmt_type == ACTION_STMT) gen_line_marker(ep);
		indent(level);
		printf("for (");
		gen_expr(stmt_type, ep->for_init, sp, 0);
		printf("; ");
		gen_expr(stmt_type, ep->for_cond, sp, 0);
		printf("; ");
		gen_expr(stmt_type, ep->for_iter, sp, 0);
		printf(")\n");
		cep = ep->for_stmt;
		gen_expr(stmt_type, cep, sp, cep->type == S_CMPND ? level : level+1);
		break;
	case S_BREAK:
		indent(level);
		printf("break;\n");
		break;
	case T_TEXT:
		indent(level);
		printf("%s\n", ep->value);
		break;
	/* Expressions */
	case E_VAR:
		gen_var_access(ep);
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
		if (special_func(stmt_type, ep, sp))
			break;
		{
			int n = 0;
			printf("%s(", ep->value);
			foreach (cep, ep->func_args)
			{
				if (n > 0)
					printf(", ");
				gen_expr(stmt_type, cep, sp, 0);
				n++;
			}
			printf(")");
		}
		break;
	case E_TERNOP:
		gen_expr(stmt_type, ep->ternop_cond, sp, 0);
		printf(" ? ");
		gen_expr(stmt_type, ep->ternop_then, sp, 0);
		printf(" : ");
		gen_expr(stmt_type, ep->ternop_else, sp, 0);
		break;
	case E_BINOP:
		gen_expr(stmt_type, ep->binop_left, sp, 0);
		printf(" %s ", ep->value);
		gen_expr(stmt_type, ep->binop_right, sp, 0);
		break;
	case E_PAREN:
		printf("(");
		gen_expr(stmt_type, ep->paren_expr, sp, 0);
		printf(")");
		break;
	case E_PRE:
		printf("%s", ep->value);
		gen_expr(stmt_type, ep->pre_operand, sp, 0);
		break;
	case E_POST:
		gen_expr(stmt_type, ep->post_operand, sp, 0);
		printf("%s", ep->value);
		break;
	case E_SUBSCR:
		gen_expr(stmt_type, ep->subscr_operand, sp, 0);
		printf("[");
		gen_expr(stmt_type, ep->subscr_index, sp, 0);
		printf("]");
		break;
	default:
		report_at_expr(ep, "internal error: unhandled expression type %s\n",
			expr_type_info[ep->type].name);
	}
}

static void indent(int level)
{
	while (level-- > 0)
		printf("\t");
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
	int stmt_type,		/* ACTION_STMT or EVENT_STMT */
	Expr *ep,		/* ptr to function in the expression */
	Expr *sp		/* current State struct */
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
		gen_pv_func(stmt_type, ep, sp, fname, func_code, FALSE, 0);
		return TRUE;

	    case F_PVPUT:
	    case F_PVGET:
		/* PV functions requiring a channel id and defaulted
		   last 1 parameter */
		gen_pv_func(stmt_type, ep, sp, fname, func_code, FALSE, 1);
		return TRUE;

	    case F_PVPUTCOMPLETE:
		/* PV functions requiring a channel id, an array length and
		   defaulted last 2 parameters */
		gen_pv_func(stmt_type, ep, sp, fname, func_code, TRUE, 2);
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
			gen_expr(stmt_type, ap, sp, 0);
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
	int	stmt_type,	/* ACTION_STMT or EVENT_STMT */
	Expr	*ep,		/* ptr to function in the expression */
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
		report_at_expr(ep, "argument to '%s' must be an event flag\n", fname);
	}
	if ((func_code == F_EFSET || func_code == F_EFCLEAR) && stmt_type == EVENT_STMT)
	{
		report_at_expr(ep, "%s cannot be used in a when condition\n", fname);
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
	int	stmt_type,	/* ACTION_STMT or EVENT_STMT */
	Expr	*ep,		/* ptr to function in the expression */
	Expr	*sp,		/* current State struct */
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
		report_at_expr(ep,
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
		report_at_expr(ep,
			"parameter to '%s' is not a declared variable\n", fname);
		cp = 0;
	}
	else
	{
#ifdef	DEBUG
		report("gen_pv_func: var=%s\n", vp->name);
#endif	/*DEBUG*/
		vn = vp->name;
		cp = vp->chan;
		if (cp == 0)
		{
			report_at_expr(ep,
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
		gen_expr(stmt_type, subscr, sp, 0);
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
		gen_expr(stmt_type, ap, sp, 0);
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
}

static void gen_struct_var_init(Expr *prog)
{
	Expr *sp, *ssp;
	printf("/* TODO: Global variable init */\n");

	/* For each state set ... */
	foreach (ssp, prog->prog_statesets)
	{
		printf("/* TODO: Variable init for state set \"%s\" */\n", ssp->value);

		/* For each state ... */
		foreach (sp, ssp->ss_states)
		{
			printf("/* TODO: Variable init for state \"%s\" in state set \"%s\" */\n",
				sp->value, ssp->value);
		}
	}
}

/* Generate entry handler code */
static void gen_entry_handler(Expr *prog)
{
	Expr		*ep;

	printf("\n/* Entry handler */\n");
	printf("static void entry_handler(SS_ID ssId, struct UserVar *pVar)\n{\n");

	gen_struct_var_init(prog);

	foreach (ep, prog->prog_entry)
	{
		gen_expr(ENTRY_STMT, ep, NULL, 1);
	}
	printf("}\n");
}

/* Generate exit handler code */
static void gen_exit_handler(Expr *expr_list)
{
	Expr		*ep;

	printf("\n/* Exit handler */\n");
	printf("static void exit_handler(SS_ID ssId, struct UserVar *pVar)\n{\n");
	for (ep = expr_list; ep != 0; ep = ep->next)
	{
		gen_expr(EXIT_STMT, ep, NULL, 1);
	}
	printf("}\n");
}
