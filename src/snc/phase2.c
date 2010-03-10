/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

 	phase2.c,v 1.2 1995/06/27 15:25:52 wright Exp

	DESCRIPTION: Phase 2 code generation routines for SNC.
	Produces code and tables in C output file.
	See also:  gen_ss_code.c and gen_tables.c
	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Replaced lstLib calls with built-in linked list.
19nov91,ajk	Removed extraneous "static" from "UserVar" declaration.
01mar94,ajk	Implemented new interface to sequencer (seqCom.h).
01mar94,ajk	Implemented assignment of array elements to db channels.
01mar94,ajk	Changed algorithm for assigning event bits.
20jul95,ajk	Added unsigned types.
11aug96,wfl	Supported syncQ queues.
13jan98,wfl     Supported E_COMMA token (for compound expressions).
01oct98,wfl	Supported setting initial value on declaration.
29apr99,wfl     Avoided compilation warnings; removed unused include files.
17may99,wfl	Added main program under UNIX.
06jul99,wfl	Changed to use "+m" (main) option; minor cosmetic changes.
07sep99,wfl	Added ASYNC/SYNC defns in generated output;
		Supported E_DECL (for local declarations).
22sep99,grw     Supported entry and exit actions.
18feb00,wfl     More partial support for local declarations (still not done).
29feb00,wfl	Added errlogInit() and taskwdInit() to Unix main program.
06mar00,wfl	Added threadInit() to main program; removed ASYNC/SYNC #defines.
17mar00,wfl	Added necessary includes for C main program.
31mar00,wfl	Supported entry handler.
***************************************************************************/
/*#define DEBUG 1*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"parse.h"
#include	"phase2.h"
#include	"gen_ss_code.h"
#include	"gen_tables.h"
#include	"snc_main.h"

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif  /*TRUE*/

static void gen_preamble(char *prog_name,
	int num_ss, int num_channels, int num_events, int num_queues);
static void gen_opt_defn(int opt, char *defn_name);
static void reconcile_variables(Scope *scope, Expr *expr_list);
static void connect_variable(Expr *ep, Scope *scope);
static void reconcile_states(Expr *ss_list);
static void gen_var_decl(Scope *scope);
static void gen_defn_c_code(Expr *defn_c_list);
static void gen_global_c_code(Expr *global_c_list);
static void gen_init_reg(char *prog_name);
static int assign_ef_bits(Scope *scope, ChanList *chan_list);
static void assign_delay_ids(Expr *ss_list);
static void assign_next_delay_id(Expr *ep, int *delay_id);
static int db_queue_count(Scope *scope);
static int db_chan_count(ChanList *chan_list);

/*+************************************************************************
*  NAME: phase2
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*
*  RETURNS: n/a
*
*  FUNCTION: Generate C code from parsing lists.
*-*************************************************************************/
void phase2(Program *program)
{
	/* Count number of db channels and state sets defined */
	program->num_queues = db_queue_count(program->global_scope);
	program->num_channels = db_chan_count(program->chan_list);
	program->num_ss = expr_count(program->ss_list);

	/* Reconcile all variable and tie each to the appropriate VAR struct */
	reconcile_variables(program->global_scope, program->ss_list);
	reconcile_variables(program->global_scope, program->entry_code_list);
	reconcile_variables(program->global_scope, program->exit_code_list);

	/* reconcile all state names, including next state in transitions */
	reconcile_states(program->ss_list);

	/* Assign bits for event flags */
	program->num_events = assign_ef_bits(program->global_scope, program->chan_list);

#ifdef	DEBUG
	fprintf(stderr, "gen_tables:\n");
	fprintf(stderr, " num_channels = %d\n", program->num_channels);
	fprintf(stderr, " num_events = %d\n", program->num_events);
	fprintf(stderr, " num_queues = %d\n", program->num_queues);
	fprintf(stderr, " num_ss = %d\n", program->num_ss);
#endif	/*DEBUG*/

	/* Assign delay id's */
	assign_delay_ids(program->ss_list);

	/* Generate preamble code */
	gen_preamble(program->prog_name, program->num_ss,
		program->num_channels, program->num_events, program->num_queues);

	/* Generate variable declarations */
	gen_var_decl(program->global_scope);

	/* Generate definition C code */
	gen_defn_c_code(program->global_defn_list);

	/* Generate code for each state set */
	gen_ss_code(program);

	/* Generate tables */
	gen_tables(program);

	/* Output global C code */
	gen_global_c_code(program->global_c_list);

	/* Sequencer registration (if "init_register" option set) */
	gen_init_reg(program->prog_name);

	exit(0);
}
/* Generate preamble (includes, defines, etc.) */
static void gen_preamble(char *prog_name,
	int num_ss, int num_channels, int num_events, int num_queues)
{
	Options *options = globals->options;

	/* Program name (comment) */
	printf("\n/* Program \"%s\" */\n", prog_name);

	/* Include files */
	printf("#include \"seqCom.h\"\n");

	/* Local definitions */
	printf("\n#define NUM_SS %d\n", num_ss);
	printf("#define NUM_CHANNELS %d\n", num_channels);
	printf("#define NUM_EVENTS %d\n", num_events);
	printf("#define NUM_QUEUES %d\n", num_queues);

	/* The following definition should be consistant with db_access.h */
	printf("\n");
	printf("#define MAX_STRING_SIZE 40\n");

	/* #define's for compiler options */
	printf("\n");
	gen_opt_defn(options->async, "ASYNC_OPT");
	gen_opt_defn(options->conn,  "CONN_OPT" );
	gen_opt_defn(options->debug, "DEBUG_OPT");
	gen_opt_defn(options->main,  "MAIN_OPT" );
	gen_opt_defn(options->newef, "NEWEF_OPT" );
	gen_opt_defn(options->reent, "REENT_OPT");

	/* Forward references of tables: */
	printf("\nextern struct seqProgram %s;\n", prog_name);

        /* Main program (if "main" option set) */
	if (options->main) {
	    printf("\n/* Main program */\n");
	    printf("#include <string.h>\n");
	    printf("#include \"epicsThread.h\"\n");
	    printf("#include \"iocsh.h\"\n");
	    printf("\n");
	    printf("int main(int argc,char *argv[]) {\n");
            printf("    char * macro_def;\n");
            printf("    epicsThreadId threadId;\n");
            printf("    int callIocsh = 0;\n");
            printf("    if(argc>1 && strcmp(argv[1],\"-s\")==0) {\n");
            printf("        callIocsh=1;\n");
            printf("        --argc; ++argv;\n");
            printf("    }\n");
	    printf("    macro_def = (argc>1)?argv[1]:NULL;\n");
	    printf("    threadId = seq((void *)&%s, macro_def, 0);\n", prog_name);
            printf("    if(callIocsh) {\n");
            printf("        seqRegisterSequencerCommands();\n");
            printf("        iocsh(0);\n");
            printf("    } else {\n");
            printf("        epicsThreadExitMain();\n");
            printf("    }\n");
            printf("    return(0);\n");
	    printf("}\n");
	}

	return;
}

/* Generate defines for compiler options */
static void gen_opt_defn(int opt, char *defn_name)
{
	if (opt)
		printf("#define %s TRUE\n", defn_name);
	else
		printf("#define %s FALSE\n", defn_name);
}

/* Reconcile all variables in an expression,
 * and tie each to the appropriate VAR structure.
 */
int	printTree = FALSE; /* For debugging only */

static void reconcile_variables(Scope *scope, Expr *expr_list)
{
	Expr			*ep;

	for (ep = expr_list; ep != 0; ep = ep->next)
	{
		traverse_expr_tree(ep, E_VAR, 0, connect_variable, scope);
	}
}

/* Connect a variable in an expression to the the Var structure */
static void connect_variable(Expr *ep, Scope *scope)
{
	Var		*vp;

	if (ep->type != E_VAR)
		return;
#ifdef	DEBUG
	fprintf(stderr, "connect_variable: \"%s\", line %d\n", ep->value, ep->line_num);
#endif	/*DEBUG*/
	vp = find_var(scope, ep->value);
#ifdef	DEBUG
	fprintf(stderr, "\t \"%s\" was %s\n", ep->value, vp ? "found" : "not found" );
#endif	/*DEBUG*/
	if (vp == 0)
	{	/* variable not declared; add it to the variable list */
		if (globals->options->warn)
			fprintf(stderr,
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
			    fprintf(stderr,
			       "State \"%s\" is duplicated in state set \"%s\"\n",
			       sp->value, ssp->value);
			}
		}		
	    }
	}
}

/* Generate a C variable declaration for each variable declared in SNL */
static void gen_var_decl(Scope *scope)
{
	Var		*vp;
	char		*vstr;
	int		nv;

	printf("\n/* Variable declarations */\n");

	/* Convert internal type to `C' type */
	if (globals->options->reent)
		printf("struct UserVar {\n");
	for (nv=0, vp = scope->var_list->first; vp != NULL; nv++, vp = vp->next)
	{
		switch (vp->type)
		{
		  case V_CHAR:
			vstr = "char";
			break;
		  case V_INT:
			vstr = "int";
			break;
		  case V_LONG:
			vstr = "long";
			break;
		  case V_SHORT:
			vstr = "short";
			break;
		  case V_UCHAR:
			vstr = "unsigned char";
			break;
		  case V_UINT:
			vstr = "unsigned int";
			break;
		  case V_ULONG:
			vstr = "unsigned long";
			break;
		  case V_USHORT:
			vstr = "unsigned short";
			break;
		  case V_FLOAT:
			vstr = "float";
			break;
 		  case V_DOUBLE:
			vstr = "double";
			break;
		  case V_STRING:
			vstr = "char";
			break;
		  case V_EVFLAG:
		  case V_NONE:
			vstr = NULL;
			break;
		  default:
			vstr = "int";
			break;
		}
		if (vstr == NULL)
			continue;

		if (globals->options->reent)
			printf("\t");
		else
			printf("static ");

		printf("%s\t", vstr);

		if (vp->class == VC_POINTER || vp->class == VC_ARRAYP)
			printf("*");

		printf("%s", vp->name);

		if (vp->class == VC_ARRAY1 || vp->class == VC_ARRAYP)
			printf("[%d]", vp->length1);

		else if (vp->class == VC_ARRAY2)
			printf("[%d][%d]", vp->length1, vp->length2);

		if (vp->type == V_STRING)
			printf("[MAX_STRING_SIZE]");

		if (vp->value != NULL)
			printf(" = %s", vp->value);

		printf(";\n");
	}
	if (globals->options->reent)
		printf("};\n");

	/* Avoid compilation warnings if not re-entrant */
	if (!globals->options->reent)
	{
		printf("\n");
		printf("/* Not used (avoids compilation warnings) */\n");
		printf("struct UserVar {\n");
		printf("\tint\tdummy;\n");
		printf("};\n");
	}
	return;
}

/* Generate definition C code (C code in definition section) */
static void gen_defn_c_code(Expr *defn_list)
{
	Expr		*ep;
	int		first = TRUE;

	for (ep = defn_list; ep != NULL; ep = ep->next)
	{
		if (ep->type == E_TEXT)
		{
			if (first)
			{
				first = FALSE;
				printf("\n/* C code definitions */\n");
			}
			print_line_num(ep->line_num, ep->src_file);
			printf("%s\n", ep->value);
		}
	}
	return;
}
/* Generate global C code (C code following state program) */
static void gen_global_c_code(Expr *global_c_list)
{
	Expr		*ep;

	ep = global_c_list;
	if (ep != NULL)
	{
		printf("\f/* Global C code */\n");
		print_line_num(ep->line_num, ep->src_file);
		for (; ep != NULL; ep = ep->next)
		{
			assert(ep->type == E_TEXT);
			printf("%s\n", ep->value);
		}
	}
	return;
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

/* Assign event bits to event flags and associate db channels with
 * event flags.
 */
static int assign_ef_bits(Scope *scope, ChanList *chan_list)
{
	Var		*vp;
	Chan		*cp;
	int		n;
	int		num_events;

	/* Assign event flag numbers (starting at 1) */
	printf("\n/* Event flags */\n");
	num_events = 0;
	for (vp = scope->var_list->first; vp != NULL; vp = vp->next)
	{
		if (vp->type == V_EVFLAG)
		{
			num_events++;
			vp->ef_num = num_events;
			printf("#define %s\t%d\n", vp->name, num_events);
		}
	}
			
	/* Associate event flags with DB channels */
	for (cp = chan_list->first; cp != NULL; cp = cp->next)
	{
		if (cp->num_elem == 0)
		{
			if (cp->ef_var != NULL)
			{
				vp = cp->ef_var;
				cp->ef_num = vp->ef_num;
			}
		}

		else /* cp->num_elem != 0 */
		{
			for (n = 0; n < cp->num_elem; n++)
			{
			    vp = cp->ef_var_list[n];
			    if (vp != NULL)
			    {
				cp->ef_num_list[n] = vp->ef_num;
			    }
			}
		}
	}

	return num_events;
}

/* Assign a delay id to each "delay()" in an event (when()) expression */
static void assign_delay_ids(Expr *ss_list)
{
	Expr			*ssp, *sp, *tp;
	int			delay_id;

#ifdef	DEBUG
	fprintf(stderr, "assign_delay_ids:\n");
#endif	/*DEBUG*/
	for (ssp = ss_list; ssp != 0; ssp = ssp->next)
	{
		for (sp = ssp->left; sp != 0; sp = sp->next)
		{
			/* Each state has it's own delay id's */
			delay_id = 0;
			for (tp = sp->left; tp != 0; tp = tp->next)
			{	/* ignore local declarations */
				if (tp->type == E_TEXT)
					continue;

				/* traverse event expression only */
				traverse_expr_tree(tp->left, E_FUNC, "delay",
					assign_next_delay_id, &delay_id);
			}
		}
	}
}

static void assign_next_delay_id(Expr *ep, int *delay_id)
{
	ep->right = (Expr *)*delay_id;
	*delay_id += 1;
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

	if (printTree)
		fprintf(stderr, "traverse_expr_tree: type=%s, value=%s\n",
		 expr_type_names[ep->type], ep->value);

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
		fprintf(stderr, "traverse_expr_tree: type=%d???\n", ep->type);
	}
}

static void gen_init_reg(char *prog_name)
{
	if (globals->options->init_reg) {
		printf("\n\n/* Register sequencer commands and program */\n");
		printf("\nvoid %sRegistrar (void) {\n", prog_name);
		printf("    seqRegisterSequencerCommands();\n");
		printf("    seqRegisterSequencerProgram (&%s);\n", prog_name);
		printf("}\n");
		printf("epicsExportRegistrar(%sRegistrar);\n\n", prog_name);
	}
}

/* Count the number of linked expressions */
int expr_count(Expr *ep)
{
	int		count;

	for (count = 0; ep != 0; ep = ep->next)
		count++;
	return count;
}
