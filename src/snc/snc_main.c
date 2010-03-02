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

	/* Call the SNC parser */
	compile();

        return 0; /* never reached */
}
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
		fprintf(stderr, "Unknown option ignored: \"%s\"\n", s);
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
	return;
}

static void get_out_file(char *s)
{
	if (s == NULL)
	{
		print_usage();
		exit(1);
	}
	
	strcpy(out_file,s);
	return;
}

static void print_usage(void)
{
	fprintf(stderr, "%s\n", sncVersion);
	fprintf(stderr, "usage: snc <options> <infile>\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -o <outfile> - override name of output file\n");
	fprintf(stderr, "  +a           - do asynchronous pvGet\n");
	fprintf(stderr, "  -c           - don't wait for all connects\n");
	fprintf(stderr, "  +d           - turn on debug run-time option\n");
	fprintf(stderr, "  -e           - don't use new event flag mode\n");
	fprintf(stderr, "  -l           - suppress line numbering\n");
	fprintf(stderr, "  +m           - generate main program\n");
	fprintf(stderr, "  -i           - don't register commands/programs\n");
	fprintf(stderr, "  +r           - make reentrant at run-time\n");
	fprintf(stderr, "  -w           - suppress compiler warnings\n");
	fprintf(stderr, "example:\n snc +a -c vacuum.st\n");
}

/*+************************************************************************
*  NAME: snc_err
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-------------------------------------------------------------------
*	char		*err_txt	I	Text of error msg.
*	int		line		I	Line no. where error ocurred
*	int		code		I	Error code (see snc.y)
*
*  RETURNS: no return
*
*  FUNCTION: Print the SNC error message and then exit.
*
*  NOTES:
*-*************************************************************************/
void snc_err(char *err_txt)
{
	fprintf(stderr, "     %s\n", err_txt);
	exit(1);
}

/*+************************************************************************
*  NAME: yyerror
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*	char		*err		I	yacc error
*
*  RETURNS: n/a
*
*  FUNCTION: Print yacc error msg
*
*  NOTES:
*-*************************************************************************/
void yyerror(char *err)
{
	fprintf(stderr, "%s: line no. %d (%s)\n", err,
		globals->line_num, globals->src_file);
	return;
}

/*+************************************************************************
*  NAME: print_line_num
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*	int		line_num	I	current line number
*	char		src_file	I	effective source file
*
*  RETURNS: n/a
*
*  FUNCTION: Prints the line number and input file name for use by the
*	C preprocessor.  e.g.: # line 24 "something.st"
*
*  NOTES:
*-*************************************************************************/
void print_line_num(int line_num, char *src_file)
{
	if (globals->options->line)
		printf("# line %d \"%s\"\n", line_num, src_file);
	return;
}
