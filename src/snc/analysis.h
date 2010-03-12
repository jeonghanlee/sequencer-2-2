#ifndef INCLanalysish
#define INCLanalysish

#include "types.h"

void analyse_program(Program *p, Options *options);

void add_var(
	Scope *scope,		/* scope to add variable to */
	Var *vp			/* variable to add */
);

Var *find_var(
	Scope *scope,		/* scope where to first search for the variable */
	char *name		/* variable name to find */
);

int expr_count(Expr*);

typedef void expr_fun(Expr *, void *);

void traverse_expr_tree(
	Expr	    *ep,	/* ptr to start of expression */
	int	    type,	/* to search for */
	char	    *value,	/* with optional matching value */
	expr_fun    *funcp,	/* function to call */
	void	    *argp	/* ptr to argument to pass on to function */
);

#endif	/*INCLanalysish*/
