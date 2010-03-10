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
	char	*src_file;	/* current source file name */
	int	line_num;	/* current line number */
	int	prev_line_num;	/* line number for previous token */
	Options *options;	/* compile & run-time options */
};
typedef struct globals Globals;

extern Globals *globals;

/* append '# <line_num> "<src_file>"\n' to output */

void print_line_num(int line_num, char *src_file);

/* Error and warning message support */

/* this uses the global location information which is valid only during the
   parsing stage */
void parse_error(const char *format, ...);

/* just the location info, no newline */
void report_location(const char *src_file, int line_num);

/* these both add a trailing newline */
void report_with_location(
	const char *src_file, int line_num, const char *format, ...);
void report(const char *format, ...);

#endif	/*INCLsncmainh*/
