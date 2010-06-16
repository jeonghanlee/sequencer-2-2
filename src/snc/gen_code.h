/**************************************************************************
		GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin für Materialien
		und Energie GmbH, Berlin, Germany (HZB)
		(see file Copyright.HZB included in this distribution)
***************************************************************************
		Code generation
***************************************************************************/
#ifndef INCLgencodeh
#define INCLgencodeh

#include "types.h"

void generate_code(Program *p);
void gen_defn_c_code(Expr *scope, int level);
void gen_var_decl(Var *vp);
void gen_var_init(Var *vp, int level);
void indent(int level);

#define VAR_PREFIX "UserVar"

#endif	/*INCLgencodeh*/
