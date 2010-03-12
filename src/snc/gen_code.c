/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

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

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"analysis.h"
#include	"gen_ss_code.h"
#include	"gen_tables.h"
#include	"snc_main.h"
#include	"gen_code.h"

static void gen_preamble(char *prog_name, Options *options,
	int num_ss, int num_channels, int num_events, int num_queues);
static void gen_opt_defn(int opt, char *defn_name);
static void gen_var_decl(Scope *scope, int opt_reent);
static void gen_defn_c_code(Expr *defn_c_list);
static void gen_global_c_code(Expr *global_c_list);
static void gen_init_reg(char *prog_name);
static int assign_ef_bits(Scope *scope, ChanList *chan_list);
static void assign_delay_ids(Expr *ss_list);
static void assign_next_delay_id(Expr *ep, int *delay_id);

/* Generate C code from parse tree. */
void generate_code(Program *p)
{
	/* Assign bits for event flags */
	p->num_events = assign_ef_bits(p->global_scope, p->chan_list);

#ifdef	DEBUG
	report("gen_tables:\n");
	report(" num_channels = %d\n", p->num_channels);
	report(" num_events = %d\n", p->num_events);
	report(" num_queues = %d\n", p->num_queues);
	report(" num_ss = %d\n", p->num_ss);
#endif	/*DEBUG*/

	/* Assign delay id's */
	assign_delay_ids(p->ss_list);

	/* Generate preamble code */
	gen_preamble(p->name, p->options, p->num_ss,
		p->num_channels, p->num_events, p->num_queues);

	/* Generate variable declarations */
	gen_var_decl(p->global_scope, p->options->reent);

	/* Generate definition C code */
	gen_defn_c_code(p->global_defn_list);

	/* Generate code for each state set */
	gen_ss_code(p);

	/* Generate tables */
	gen_tables(p);

	/* Output global C code */
	gen_global_c_code(p->global_c_list);

	/* Sequencer registration (if "init_register" option set) */
	if (p->options->init_reg) {
		gen_init_reg(p->name);
	}
}

/* Generate preamble (includes, defines, etc.) */
static void gen_preamble(char *prog_name, Options *options,
	int num_ss, int num_channels, int num_events, int num_queues)
{
	/* Program name (comment) */
	printf("\n/* Program \"%s\" */\n", prog_name);

	/* Include files */
	printf("#include \"seqCom.h\"\n");

	/* Local definitions */
	printf("\n#define NUM_SS %d\n", num_ss);
	printf("#define NUM_CHANNELS %d\n", num_channels);
	printf("#define NUM_EVENTS %d\n", num_events);
	printf("#define NUM_QUEUES %d\n", num_queues);

	/* The following definition should be consistent with db_access.h */
	printf("\n");
	printf("#define MAX_STRING_SIZE 40\n");

	/* The following definition should be consistent with seq_if.c */
	printf("\n");
	printf("#define ASYNC %d\n", 1);
	printf("#define SYNC %d\n", 2);

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

/* Generate a C variable declaration for each variable declared in SNL */
static void gen_var_decl(Scope *scope, int opt_reent)
{
	Var		*vp;
	char		*vstr;
	int		nv;

	printf("\n/* Variable declarations */\n");

	/* Convert internal type to `C' type */
	if (opt_reent)
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

		if (opt_reent)
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
	if (opt_reent)
		printf("};\n");

	/* Avoid compilation warnings if not re-entrant */
	if (!opt_reent)
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
			gen_line_marker(ep);
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
		for (; ep != NULL; ep = ep->next)
		{
			assert(ep->type == E_TEXT);
			gen_line_marker(ep);
			printf("%s\n", ep->value);
		}
	}
	return;
}

/* Assign event bits to event flags and associate db channels with
 * event flags. Return number of event flags found.
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
	report("assign_delay_ids:\n");
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
				  (expr_fun*)assign_next_delay_id, &delay_id);
			}
		}
	}
}

static void assign_next_delay_id(Expr *ep, int *delay_id)
{
	ep->right = (Expr *)*delay_id;
	*delay_id += 1;
}

static void gen_init_reg(char *prog_name)
{
	printf("\n\n/* Register sequencer commands and program */\n");
	printf("\nvoid %sRegistrar (void) {\n", prog_name);
	printf("    seqRegisterSequencerCommands();\n");
	printf("    seqRegisterSequencerProgram (&%s);\n", prog_name);
	printf("}\n");
	printf("epicsExportRegistrar(%sRegistrar);\n\n", prog_name);
}
