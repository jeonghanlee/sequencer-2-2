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
                Code generation
\*************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "analysis.h"
#include "gen_ss_code.h"
#include "gen_tables.h"
#include "main.h"
#include "gen_code.h"

static void gen_preamble(char *prog_name);
static void gen_header_preamble(char *prog_name);
static void gen_main(char *prog_name);
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
		ep->extra.e_var->type->tag == T_NONE);
	return TRUE;		/* there are no children anyway */
}

/* Generate C code from parse tree. */
void generate_code(Program *p, const char *header_name)
{
	/* assume there have been no errors, so all vars are declared */
	traverse_expr_tree(p->prog, 1<<E_VAR, 0, 0, assert_var_declared, 0);

#ifdef DEBUG
	report("-------------------- Code Generation --------------------\n");
#endif

	/* Initialize tables in gen_ss_code module */
	/* TODO: find a better way to do this */
	init_gen_ss_code(p);

	set_gen_h();

	gen_code("/* Generated with snc from %s */\n", p->prog->src_file);
	gen_code("#ifndef INCL%sh\n", p->name);
	gen_code("#define INCL%sh\n", p->name);

	/* Generate preamble code */
	gen_header_preamble(p->name);

	/* Generate literal C code intermixed with global definitions */
	gen_defn_c_code(p->prog, 0);

	/* Generate global, state set, and state variable declarations */
	gen_user_var(p);
	gen_code("#endif /* INCL%sh */\n", p->name);

	set_gen_c();

	gen_code("/* Generated with snc from %s */\n", p->prog->src_file);
	gen_preamble(p->name);
	gen_code("#include \"%s\"\n", header_name);

	/* Generate code for each state set */
	gen_ss_code(p);

	/* Generate tables */
	gen_tables(p);

	/* Output global C code */
	gen_global_c_code(p->prog->prog_ccode);

	/* Generate main function */
	if (p->options.main) gen_main(p->name);

	/* Sequencer registration */
	gen_init_reg(p->name);
}

/* Generate main program */
static void gen_main(char *prog_name)
{
	gen_code("\n#define PROG_NAME %s\n", prog_name);
	gen_code("#include \"seqMain.c\"\n");
}

/* Generate header preamble (includes, defines, etc.) */
static void gen_header_preamble(char *prog_name)
{
	/* Program name (comment) */
	gen_code("\n/* Header file for program \"%s\" */\n", prog_name);

	/* Includes */
	gen_code("#include \"epicsTypes.h\"\n");
	gen_code("#include \"seqCom.h\"\n");
}

/* Generate preamble (includes, defines, etc.) */
static void gen_preamble(char *prog_name)
{
	/* Program name (comment) */
	gen_code("\n/* C code for program \"%s\" */\n", prog_name);

	/* Includes */
	gen_code("#include <string.h>\n");
	gen_code("#include <stddef.h>\n");
	gen_code("#include <stdio.h>\n");
	gen_code("#include <limits.h>\n");
}

void gen_var_decl(Var *vp)
{
	gen_type(vp->type, vp->name);
}

/* Generate the UserVar struct containing all program variables with
   'infinite' (global) lifetime. These are: variables declared at the
   top-level, inside a state set, and inside a state. Note that state
   set and state local variables are _visible_ only inside the block
   where they are declared, but still have global lifetime. To avoid
   name collisions, generate a nested struct for each state set, and
   for each state in a state set. */
static void gen_user_var(Program *p)
{
	int	opt_reent = p->options.reent;
	Var	*vp;
	Expr	*sp, *ssp;

	gen_code("\n/* Variable declarations */\n");

	if (opt_reent) gen_code("struct %s {\n", NM_VARS);
	/* Convert internal type to `C' type */
	foreach (vp, p->prog->extra.e_prog->first)
	{
		if (vp->decl && vp->type->tag >= V_CHAR)
		{
			gen_line_marker(vp->decl);
			if (!opt_reent) gen_code("static");
			indent(1);
			gen_var_decl(vp);
			if (!opt_reent)
			{
				gen_code(" = ");
				gen_var_init(vp, 0);
			}
			gen_code(";\n");
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
			indent(level); gen_code("struct %s_%s {\n", NM_VARS, ssp->value);
			foreach (vp, ssp->extra.e_ss->var_list->first)
			{
				indent(level+1); gen_var_decl(vp); gen_code(";\n");
			}
			foreach (sp, ssp->ss_states)
			{
				int s_empty = !sp->extra.e_state->var_list->first;
				if (!s_empty)
				{
					indent(level+1);
					gen_code("struct {\n");
					foreach (vp, sp->extra.e_state->var_list->first)
					{
						indent(level+2); gen_var_decl(vp); gen_code(";\n");
					}
					indent(level+1);
					gen_code("} %s_%s;\n", NM_VARS, sp->value);
				}
			}
			indent(level); gen_code("} %s_%s", NM_VARS, ssp->value);
			if (!opt_reent)
			{
				gen_code(" = ");
				gen_ss_user_var_init(ssp, level);
			}
			gen_code(";\n");
		}
	}
	if (opt_reent) gen_code("};\n");
	gen_code("\n");
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
				indent(level);
				gen_code("/* C code definitions */\n");
			}
			gen_line_marker(ep);
			indent(level);
			gen_code("%s\n", ep->value);
		}
	}
}

/* Generate global C code following state sets */
static void gen_global_c_code(Expr *global_c_list)
{
	Expr	*ep;

	if (global_c_list != 0)
	{
		gen_code("\n/* Global C code */\n");
		foreach (ep, global_c_list)
		{
			assert(ep->type == T_TEXT);
			gen_line_marker(ep);
			gen_code("%s\n", ep->value);
		}
	}
}

static void gen_init_reg(char *prog_name)
{
	gen_code("\n/* Register sequencer commands and program */\n");
	gen_code("#include \"epicsExport.h\"\n");
	gen_code("static void %sRegistrar (void) {\n", prog_name);
	gen_code("    seqRegisterSequencerCommands();\n");
	gen_code("    seqRegisterSequencerProgram (&%s);\n", prog_name);
	gen_code("}\n");
	gen_code("epicsExportRegistrar(%sRegistrar);\n", prog_name);
}

void indent(int level)
{
	while (level-- > 0)
		gen_code("\t");
}
