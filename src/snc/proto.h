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

/* Don't require parse.h to have been included */
#ifndef INCLparseh
#define Expr void
#define Var  void
#endif

/* Prototypes for external functions */
extern void global_yyparse(void);

extern void phase2(void);

extern int  expr_count(Expr*);

extern void gen_ss_code(void);

extern void gen_tables(void);

extern void add_var(Var*);

extern void print_line_num(int,char*);

extern void traverse_expr_tree(
	Expr	*ep,		/* ptr to start of expression */
	int	type,		/* to search for */
	char	*value,		/* with optional matching value */
	void	(*funcp)(),	/* function to call */
	void	*argp		/* ptr to argument to pass on to function */
);

extern void snc_err(char *err_txt);

#endif	/*INCLsnch*/
