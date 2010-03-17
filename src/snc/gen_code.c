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

static const int impossible = 0;

static void gen_preamble(char *prog_name, Options options,
	int num_ss, int num_channels, int num_events, int num_queues);
static void gen_global_var_decls(Program *p);
static void gen_global_c_code(Expr *global_c_list);
static void gen_init_reg(char *prog_name);
static int assign_ef_bits(Expr *scope, ChanList *chan_list);

/* Generate C code from parse tree. */
void generate_code(Program *p)
{
#ifdef	DEBUG
	report("---- Code Generation ----\n");
#endif	/*DEBUG*/

	/* Assign bits for event flags */
	p->num_events = assign_ef_bits(p->prog, p->chan_list);

#ifdef	DEBUG
	report("gen_tables:\n");
	report(" num_channels = %d\n", p->num_channels);
	report(" num_events = %d\n", p->num_events);
	report(" num_queues = %d\n", p->num_queues);
	report(" num_ss = %d\n", p->num_ss);
#endif	/*DEBUG*/

	/* Generate preamble code */
	gen_preamble(p->name, p->options, p->num_ss,
		p->num_channels, p->num_events, p->num_queues);

	/* Generate global variable declarations */
	gen_global_var_decls(p);

	/* Generate definition C code */
	gen_defn_c_code(p->prog);

	/* Generate code for each state set */
	gen_ss_code(p);

	/* Generate tables */
	gen_tables(p);

	/* Output global C code */
	gen_global_c_code(p->prog->prog_ccode);

	/* Sequencer registration (if "init_register" option set) */
	if (p->options.init_reg) {
		gen_init_reg(p->name);
	}
}

/* Generate preamble (includes, defines, etc.) */
static void gen_preamble(char *prog_name, Options options,
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

	/* Forward references of tables: */
	printf("\nextern struct seqProgram %s;\n", prog_name);

        /* Main program (if "main" option set) */
	if (options.main) {
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
}

void gen_var_decl(Var *vp, const char *prefix)
{
	char	*vstr;

	switch (vp->type)
	{
	case V_CHAR:	vstr = "char";		break;
	case V_INT:	vstr = "int";		break;
	case V_LONG:	vstr = "long";		break;
	case V_SHORT:	vstr = "short";		break;
	case V_UCHAR:	vstr = "unsigned char";	break;
	case V_UINT:	vstr = "unsigned int";	break;
	case V_ULONG:	vstr = "unsigned long";	break;
	case V_USHORT:	vstr = "unsigned short";break;
	case V_FLOAT:	vstr = "float";		break;
	case V_DOUBLE:	vstr = "double";	break;
	case V_STRING:	vstr = "char";		break;
	case V_EVFLAG:
	case V_NONE:
		return;
	default:
		assert(impossible);
	}
	printf("%s%s\t", prefix, vstr);
	if (vp->class == VC_POINTER || vp->class == VC_ARRAYP)
		printf("*");
	printf("%s", vp->name);
	if (vp->class == VC_ARRAY1 || vp->class == VC_ARRAYP)
		printf("[%d]", vp->length1);
	else if (vp->class == VC_ARRAY2)
		printf("[%d][%d]", vp->length1, vp->length2);
	if (vp->type == V_STRING)
		printf("[MAX_STRING_SIZE]");
	printf(";\n");
}

/* Generate a C variable declaration for each variable declared in SNL */
static void gen_global_var_decls(Program *p)
{
	int	opt_reent = p->options.reent;
	Var	*vp;
	Expr	*sp, *ssp;

	printf("\n/* Variable declarations */\n");

	if (opt_reent) printf("struct UserVar {\n");
	/* Convert internal type to `C' type */
	foreach (vp, p->prog->extra.e_prog->first)
	{
		if (vp->decl)
			gen_line_marker(vp->decl);
		gen_var_decl(vp, opt_reent ? "\t" : "static ");
	}
	foreach (ssp, p->prog->prog_statesets)
	{
		printf("%sstruct UserVar_ss_%s {\n", opt_reent?"\t":"", ssp->value);
		foreach (vp, ssp->extra.e_ss->var_list->first)
		{
			gen_var_decl(vp, opt_reent?"\t\t":"\t");
		}
		foreach (sp, ssp->ss_states)
		{
			printf("%sstruct UserVar_ss_%s_state_%s {\n", opt_reent?"\t\t":"\t", ssp->value, sp->value);
			foreach (vp, sp->extra.e_state->var_list->first)
			{
				gen_var_decl(vp, opt_reent?"\t\t\t":"\t\t");
			}
			printf("%s} UserVar_ss_%s_state_%s;\n", opt_reent?"\t\t":"\t", ssp->value, sp->value);
		}
		printf("%s} UserVar_ss_%s;\n", opt_reent?"\t":"", ssp->value);
	}
	if (opt_reent) printf("};\n");

	/* Avoid compilation warnings if not re-entrant */
	if (!opt_reent)
	{
		printf("\n");
		printf("/* Not used (avoids compilation warnings) */\n");
		printf("struct UserVar {\n");
		printf("\tint\tdummy;\n");
		printf("};\n");
	}
}

/* Generate definition C code (C code in definition section) */
void gen_defn_c_code(Expr *scope)
{
	Expr	*ep;
	int	first = TRUE;
	Expr	*defn_list = defn_list_from_scope(scope);

	foreach (ep, defn_list)
	{
		if (ep->type == T_TEXT)
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
			assert(ep->type == T_TEXT);
			gen_line_marker(ep);
			printf("%s\n", ep->value);
		}
	}
}

/* Assign event bits to event flags and associate db channels with
 * event flags. Return number of event flags found.
 */
static int assign_ef_bits(Expr *scope, ChanList *chan_list)
{
	Var	*vp;
	Chan	*cp;
	int	n;
	int	num_events;
	VarList	*var_list;

	/* Assign event flag numbers (starting at 1) */
	printf("\n/* Event flags */\n");

	var_list = *pvar_list_from_scope(scope);

	num_events = 0;
	for (vp = var_list->first; vp != NULL; vp = vp->next)
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
		else
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

static void gen_init_reg(char *prog_name)
{
	printf("\n\n/* Register sequencer commands and program */\n");
	printf("\nvoid %sRegistrar (void) {\n", prog_name);
	printf("    seqRegisterSequencerCommands();\n");
	printf("    seqRegisterSequencerProgram (&%s);\n", prog_name);
	printf("}\n");
	printf("epicsExportRegistrar(%sRegistrar);\n", prog_name);
}
