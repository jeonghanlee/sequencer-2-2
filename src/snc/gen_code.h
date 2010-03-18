#ifndef INCLgencodeh
#define INCLgencodeh

#include "types.h"

void generate_code(Program *p);
void gen_defn_c_code(Expr *scope, int level);
void gen_var_decl(Var *vp, const char *prefix);
void gen_var_init(Var *vp, int level);
void indent(int level);

#define SNL_PREFIX  "UserVar"

#endif	/*INCLgencodeh*/
