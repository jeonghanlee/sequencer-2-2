#ifndef INCLgencodeh
#define INCLgencodeh

#include "types.h"

void generate_code(Program *p);
void gen_defn_c_code(Expr *scope);
void gen_var_decl(Var *vp, const char *prefix);
void gen_var_init(Var *vp, int level);

#endif	/*INCLgencodeh*/
