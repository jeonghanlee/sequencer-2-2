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

void gen_ss_code(Program *program);
void gen_string_assign(int context, Expr *left, Expr *right, int level);

#endif	/*INCLgensscodeh*/
