/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	snc_main.c,v 1.2 1995/06/27 15:26:11 wright Exp
		

	DESCRIPTION: Main program and miscellaneous routines for
	State Notation Compiler.

	ENVIRONMENT: UNIX
	HISTORY:
20nov91,ajk	Removed call to init_snc().
20nov91,ajk	Removed some debug stuff.
28apr92,ajk	Implemented new event flag mode.
29oct93,ajk	Added 'v' (vxWorks include) option.
17may94,ajk	Changed setlinebuf() to setvbuf().
17may94,ajk	Removed event flag option (-e).
17feb95,ajk	Changed yyparse() to Global_yyparse(), because FLEX makes
		yyparse() static.
02mar95,ajk	Changed bcopy () to strcpy () in 2 places.
26jun95,ajk	Due to popular demand, reinstated event flag (-e) option.
29apr99,wfl	Avoided compilation warnings.
29apr99,wfl	Removed unused vx_opt option.
06jul99,wfl	Supported "+m" (main) option; minor cosmetic changes.
***************************************************************************/
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdarg.h>

#include	"snc_main.h"

#ifndef	TRUE
#define	TRUE 1
#define	FALSE 0
#endif

extern char	*sncVersion;	/* snc version and date created */

extern void compile(void);	/* defined in snc.y */

static Options	default_options =
{
	FALSE,	/* async */
	TRUE,	/* conn */
	FALSE,	/* debug */
	TRUE,	/* newef */
	TRUE,	/* init_reg */
	TRUE,	/* line */
	FALSE,	/* main */
	FALSE,	/* reent */
	TRUE	/* warn */
};

static Globals	default_globals =
{
	0,0,0,&default_options
};

Globals *globals = &default_globals;

static char	in_file[200];	/* input file name */
static char	out_file[200];	/* output file name */

static void get_args(int argc, char *argv[]);
static void get_options(char *s);
static void get_in_file(char *s);
static void get_out_file(char *s);
static void print_usage(void);

/*+************************************************************************
*  NAME: main
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-------------------------------------------------------------
*	int		argc		I	arg count
*	char		*argv[]		I	array of ptrs to args
*
*  RETURNS: n/a
*
*  FUNCTION: Program entry.
*
*  NOTES: The streams stdin and stdout are redirected to files named in the
* command parameters.  This accomodates the use by lex of stdin for input
* and permits printf() to be used for output.  Stderr is not redirected.
*
* This routine calls yyparse(), which never returns.
*-*************************************************************************/
int main(int argc, char *argv[])
{
	FILE	*infp, *outfp;

	/* Get command arguments */
	get_args(argc, argv);

	/* Redirect input stream from specified file */
	infp = freopen(in_file, "r", stdin);
	if (infp == NULL)
	{
		perror(in_file);
		exit(1);
	}

	/* Redirect output stream to specified file */
	outfp = freopen(out_file, "w", stdout);
	if (outfp == NULL)
	{
		perror(out_file);
		exit(1);
	}

	/* src_file is used to mark the output file for snc & cc errors */
	globals->src_file = in_file;

	/* Use line buffered output */
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
	setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

	printf("/* %s: %s */\n", sncVersion, in_file);

	compile();

        return 0; /* never reached */
}

#ifdef USE_LEMON
#include "token.h"

extern void parser(
	void *yyp,	/* The parser */
	int yymajor,	/* The major token code number */
	Token yyminor,	/* The value for the token */
        int line_num
);
extern void *parserAlloc(void *(*mallocProc)(size_t));
void parserFree(
	void *p,		/* The parser to be deleted */
	void (*freeProc)(void*)	/* Function used to reclaim memory */
);

void compile(void)
{
	int tok;
	void *pParser = parserAlloc(malloc);
	do
	{
		tok = yylex();
		parser(pParser, tok, yylval, globals->c_line_num);
	}
	while (tok);
	parserFree(pParser, free);
}
#endif

/*+************************************************************************
*  NAME: get_args
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-----------------------------------------------------------
*	int		argc		I	number of arguments
*	char		*argv[]		I	shell command arguments
*  RETURNS: n/a
*
*  FUNCTION: Get the shell command arguments.
*
*  NOTES: If "*.s" is input file then "*.c" is the output file.  Otherwise,
*  ".c" is appended to the input file to form the output file name.
*  Sets the globals in_file[] and out_file[].
*-*************************************************************************/
static void get_args(int argc, char *argv[])
{
	char	*s;

	if (argc < 2)
	{
		print_usage();
		exit(1);
	}

	strcpy(in_file,"");
	strcpy(out_file,"");

	for (argc--, argv++; argc > 0; argc--, argv++)
	{
		s = *argv;
		if (*s != '+' && *s != '-')
		{
			get_in_file(s);
		}
		else if (*s == '-' && *(s+1) == 'o')
		{
			argc--; argv++; s = *argv;
			get_out_file(s);
		}
		else
		{
			get_options(s);
		}
	}

	if (strcmp(in_file,"") == 0)
	{
		print_usage();
		exit(1);
	}
}

static void get_options(char *s)
{
	int		opt_val;
	Options		*opt = globals->options;

	opt_val = (*s == '+');

	switch (s[1])
	{
	case 'a':
		opt->async = opt_val;
		break;
	case 'c':
		opt->conn = opt_val;
		break;
	case 'd':
		opt->debug = opt_val;
		break;
	case 'e':
		opt->newef = opt_val;
		break;
	case 'i':
		opt->init_reg = opt_val;
		break;
	case 'l':
		opt->line = opt_val;
		break;
	case 'm':
		opt->main = opt_val;
		break;
	case 'r':
		opt->reent = opt_val;
		break;
	case 'w':
		opt->warn = opt_val;
		break;
	default:
		report("unknown option ignored: \"%s\"", s);
		break;
	}
}

static void get_in_file(char *s)
{				
	int		ls;

	if (strcmp(in_file,"") != 0)
	{
		print_usage();
		exit(1);
	}

	ls = strlen (s);
	strcpy (in_file, s);

	if (strcmp(out_file,"") != 0)
	{
		return;
	}

	strcpy (out_file, s);
	if ( strcmp (&in_file[ls-3], ".st") == 0 )
	{
		out_file[ls-2] = 'c';
		out_file[ls-1] = 0;
	}
	else if (in_file[ls-2] == '.')
	{	/* change suffix to 'c' */
		out_file[ls -1] = 'c';
	}
	else
	{	/* append ".c" */
		out_file[ls] = '.';
		out_file[ls+1] = 'c';
		out_file[ls+2] = 0;
	}
}

static void get_out_file(char *s)
{
	if (s == NULL)
	{
		print_usage();
		exit(1);
	}
	
	strcpy(out_file,s);
}

static void print_usage(void)
{
	report("%s", sncVersion);
	report("usage: snc <options> <infile>");
	report("options:");
	report("  -o <outfile> - override name of output file");
	report("  +a           - do asynchronous pvGet");
	report("  -c           - don't wait for all connects");
	report("  +d           - turn on debug run-time option");
	report("  -e           - don't use new event flag mode");
	report("  -l           - suppress line numbering");
	report("  +m           - generate main program");
	report("  -i           - don't register commands/programs");
	report("  +r           - make reentrant at run-time");
	report("  -w           - suppress compiler warnings");
	report("example:\n snc +a -c vacuum.st");
}

void print_line_num(int line_num, char *src_file)
{
	if (globals->options->line)
		printf("# line %d \"%s\"\n", line_num, src_file);
}

/* Errors and warnings */

void parse_error(const char *format, ...)
{
	va_list args;

	report_location(globals->src_file, globals->line_num);

	va_start(args, format);
	report(format, args);
	va_end(args);
}

void report_location(const char *src_file, int line_num)
{
	fprintf(stderr, "%s:%d: ", src_file, line_num);
}

void report_with_location(
	const char *src_file, int line_num, const char *format, ...)
{
	va_list args;

	report_location(src_file, line_num);

	va_start(args, format);
	report(format, args);
	va_end(args);
}

void report(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fprintf(stderr, "\n");
}
