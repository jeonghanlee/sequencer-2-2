#ifndef INCLphase2h
#define INCLphase2h

void phase2(Parse *parse);

int expr_count(Expr*);

typedef void expr_fun(Expr *, void *);

void traverse_expr_tree(
	Expr	    *ep,	/* ptr to start of expression */
	int	    type,	/* to search for */
	char	    *value,	/* with optional matching value */
	expr_fun    *funcp,	/* function to call */
	void	    *argp	/* ptr to argument to pass on to function */
);

#endif	/*INCLphase2h*/
