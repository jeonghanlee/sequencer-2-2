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
#ifndef INCLgensscodeh
#define INCLgensscodeh

#include "types.h"

void init_gen_ss_code(Program *program);
void gen_ss_code(Program *program);
void gen_ss_user_var_init(Expr *ssp, int level);
void gen_var_init(Var *vp, int level);

#endif	/*INCLgensscodeh*/
