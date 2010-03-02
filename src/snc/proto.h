/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1989-93, The Regents of the University of California.
		         Los Alamos National Laboratory

 	proto.h,v 1.2 2001/03/21 15:06:10 mrk Exp
	DESCRIPTION: Function prototypes for state notation language parser
	ENVIRONMENT: UNIX
	HISTORY:
29apr99,wfl	Created.

***************************************************************************/

#ifndef INCLsnch
#define INCLsnch

#include "parse.h"

/* Prototypes for external functions */
void compile(void);

void phase2(Parse *parse);

int  expr_count(Expr*);

void gen_ss_code(Parse *parse);

void gen_tables(Parse *parse);

void add_var(Var*);

void print_line_num(int,char*);

typedef void expr_fun(Expr *, void *);

void traverse_expr_tree(
	Expr	    *ep,	/* ptr to start of expression */
	int	    type,	/* to search for */
	char	    *value,	/* with optional matching value */
	expr_fun    *funcp,	/* function to call */
	void	    *argp	/* ptr to argument to pass on to function */
);

void snc_err(char *err_txt);

#endif	/*INCLsnch*/
