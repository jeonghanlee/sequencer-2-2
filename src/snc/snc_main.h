#ifndef INCLsncmainh
#define INCLsncmainh

struct options			/* compile & run-time options */
{
	int	async;		/* do pvGet() asynchronously */
	int	conn;		/* wait for all conns to complete */
	int	debug;		/* run-time debug */
	int	newef;		/* new event flag mode */
	int	init_reg;	/* register commands/programs */
	int	line;		/* line numbering */
	int	main;		/* main program */
	int	reent;		/* reentrant at run-time */
	int	warn;		/* compiler warnings */
};
typedef struct options Options;

struct globals
{
	char	*src_file;	/* ptr to (effective) source file name */
	int	line_num;	/* current src file line number */
	Options *options;	/* compile & run-time options */
};
typedef struct globals Globals;

extern Globals *globals;

void yyerror(char *err);
void snc_err(char *err_txt);
void print_line_num(int line_num, char *src_file);

#endif	/*INCLsncmainh*/
