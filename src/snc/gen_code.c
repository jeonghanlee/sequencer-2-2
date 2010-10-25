/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)
***************************************************************************
		Code generation
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

static int assert_var_declared(Expr *ep, Expr *scope, void *parg)
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
		printf("    threadId = seq(&%s, macro_def, 0);\n", prog_name);
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

	if (opt_reent) printf("struct %s {\n", VAR_PREFIX);
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
			indent(level); printf("struct %s_%s {\n", VAR_PREFIX, ssp->value);
			foreach (vp, ssp->extra.e_ss->var_list->first)
			{
				indent(level+1); gen_var_decl(vp); printf(";\n");
			}
			foreach (sp, ssp->ss_states)
			{
				int s_empty = !sp->extra.e_state->var_list->first;
				if (!s_empty)
				{
					indent(level+1);
					printf("struct %s_%s_%s {\n",
						VAR_PREFIX, ssp->value, sp->value);
					foreach (vp, sp->extra.e_state->var_list->first)
					{
						indent(level+2); gen_var_decl(vp); printf(";\n");
					}
					indent(level+1);
					printf("} %s_%s;\n", VAR_PREFIX, sp->value);
				}
			}
			indent(level); printf("} %s_%s;\n", VAR_PREFIX, ssp->value);
		}
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
	printf("#include \"epicsExport.h\"\n");
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
