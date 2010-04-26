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

static void gen_preamble(char *prog_name, int opt_main);
static void gen_user_var(Program *p);
static void gen_global_c_code(Expr *global_c_list);
static void gen_init_reg(char *prog_name);

int assert_var_declared(Expr *ep, Expr *scope, void *parg)
{
#ifdef DEBUG
	report("assert_var_declared: '%s' in scope (%s:%s)\n",
		ep->value, expr_type_name(scope), scope->value);
#endif
	assert(ep->type == E_VAR);
	assert(ep->extra.e_var != 0);
	assert(ep->extra.e_var->decl != 0 ||
		(ep->extra.e_var->type == V_NONE &&
	 	ep->extra.e_var->class == VC_FOREIGN));
	return TRUE;		/* there are no children anyway */
}

/* Generate C code from parse tree. */
void generate_code(Program *p)
{
	/* assume there have been no errors, so all vars are declared */
	traverse_expr_tree(p->prog, 1<<E_VAR, 0, 0, assert_var_declared, 0);

#ifdef DEBUG
	report("-------------------- Code Generation --------------------\n");
#endif

#ifdef DEBUG
	report("gen_tables:\n");
	report(" num_event_flags = %d\n", p->num_event_flags);
	report(" num_ss = %d\n", p->num_ss);
#endif

	/* Generate preamble code */
	gen_preamble(p->name, p->options.main);

	/* Generate global variable declarations */
	gen_user_var(p);

	/* Generate definition C code */
	gen_defn_c_code(p->prog, 0);

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
static void gen_preamble(char *prog_name, int opt_main)
{
	/* Program name (comment) */
	printf("\n/* Program \"%s\" */\n", prog_name);

	/* Includes */
	printf("#include <string.h>\n");
	printf("#include \"seqCom.h\"\n");

	/* The following definition should be consistent with db_access.h */
	printf("\n");
	printf("#define MAX_STRING_SIZE 40\n");

	/* Main program (if "main" option set) */
	if (opt_main) {
		printf("\n/* Main program */\n");
		printf("#include \"epicsThread.h\"\n");
		printf("#include \"iocsh.h\"\n");
		printf("\n");
		printf("extern struct seqProgram %s;\n", prog_name);
		printf("\n");
		printf("int main(int argc,char *argv[]) {\n");
		printf("    char * macro_def;\n");
		printf("    epicsThreadId threadId;\n");
		printf("    int callIocsh = 0;\n");
		printf("\n");
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

void gen_var_decl(Var *vp)
{
	char	*type_str;

	assert(vp->type != V_NONE);

	switch (vp->type)
	{
	case V_CHAR:	type_str = "char";		break;
	case V_INT:	type_str = "int";		break;
	case V_LONG:	type_str = "long";		break;
	case V_SHORT:	type_str = "short";		break;
	case V_UCHAR:	type_str = "unsigned char";	break;
	case V_UINT:	type_str = "unsigned int";	break;
	case V_ULONG:	type_str = "unsigned long";	break;
	case V_USHORT:	type_str = "unsigned short";	break;
	case V_FLOAT:	type_str = "float";		break;
	case V_DOUBLE:	type_str = "double";		break;
	case V_STRING:	type_str = "char";		break;
	case V_NONE:
	default:
		assert(impossible);
	}
	printf("%s\t", type_str);
	if (vp->class == VC_POINTER || vp->class == VC_ARRAYP)
		printf("*");
	printf("%s", vp->name);
	if (vp->class == VC_ARRAY1 || vp->class == VC_ARRAYP)
		printf("[%d]", vp->length1);
	else if (vp->class == VC_ARRAY2)
		printf("[%d][%d]", vp->length1, vp->length2);
	if (vp->type == V_STRING)
		printf("[MAX_STRING_SIZE]");
}

/* Generate the UserVar struct containing all program variables with
   'infinite' (global) lifetime. These are: variables declared at the
   top-level, inside a state set, and inside a state. Note that state
   set and state local variables are _visible_ only inside the block
   where they are declared, but still have gobal lifetime. To avoid
   name collisions, generate a nested struct for each state set, and
   for each state in a state set. */
static void gen_user_var(Program *p)
{
	int	opt_reent = p->options.reent;
	Var	*vp;
	Expr	*sp, *ssp;

	printf("\n/* Variable declarations */\n");

	if (opt_reent) printf("struct %s {\n", SNL_PREFIX);
	/* Convert internal type to `C' type */
	foreach (vp, p->prog->extra.e_prog->first)
	{
		if (vp->decl && vp->type != V_NONE)
		{
			gen_line_marker(vp->decl);
			if (!opt_reent) printf("static");
			indent(1);
			gen_var_decl(vp); printf(";\n");
		}
	}
	foreach (ssp, p->prog->prog_statesets)
	{
		int level = opt_reent;
		indent(level); printf("struct UV_%s {\n", ssp->value);
		foreach (vp, ssp->extra.e_ss->var_list->first)
		{
			indent(level+1); gen_var_decl(vp); printf(";\n");
		}
		foreach (sp, ssp->ss_states)
		{
			indent(level+1);
			printf("struct UV_%s_%s {\n",
				ssp->value, sp->value);
			foreach (vp, sp->extra.e_state->var_list->first)
			{
				indent(level+2); gen_var_decl(vp); printf(";\n");
			}
			indent(level+1);
			printf("} UV_%s;\n", sp->value);
		}
		indent(level); printf("} UV_%s;\n", ssp->value);
	}
	if (opt_reent) printf("};\n");
}

/* Generate C code in definition section */
void gen_defn_c_code(Expr *scope, int level)
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
			indent(level);
			printf("%s\n", ep->value);
		}
	}
}

/* Generate global C code following state sets */
static void gen_global_c_code(Expr *global_c_list)
{
	Expr	*ep;

	if (global_c_list != NULL)
	{
		printf("\n/* Global C code */\n");
		foreach (ep, global_c_list)
		{
			assert(ep->type == T_TEXT);
			gen_line_marker(ep);
			printf("%s\n", ep->value);
		}
	}
}

static void gen_init_reg(char *prog_name)
{
	printf("\n/* Register sequencer commands and program */\n\n");
	printf("void %sRegistrar (void) {\n", prog_name);
	printf("    seqRegisterSequencerCommands();\n");
	printf("    seqRegisterSequencerProgram (&%s);\n", prog_name);
	printf("}\n");
	printf("epicsExportRegistrar(%sRegistrar);\n", prog_name);
}

void indent(int level)
{
	while (level-- > 0)
		printf("\t");
}
