/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

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

#include	"types.h"
#include	"lexer.h"
#include	"analysis.h"
#include	"gen_code.h"
#include	"snc_main.h"

#include	"snc_version.h"

static Options options = DEFAULT_OPTIONS;

static char *in_file;	/* input file name */
static char *out_file;	/* output file name */

static void parse_args(int argc, char *argv[]);
static void parse_option(char *s);
static void print_usage(void);

/* The streams stdin and stdout are redirected to files named in the
   command parameters.  This accomodates the use by lex of stdin for input
   and permits printf() to be used for output. */
int main(int argc, char *argv[])
{
	FILE	*infp, *outfp;
	Program	*prg;
        Expr    *exp;

	/* Get command arguments */
	parse_args(argc, argv);

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

	/* Use line buffered output */
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
	setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

	printf("/* %s: %s */\n", snc_version, in_file);

	exp = parse_program(in_file);

#if 0
	/* HACK *** TEMPORARY *** HACK */
	options.line = FALSE;
#endif

        prg = analyse_program(exp, options);
	generate_code(prg);

	exit(0);
}

/* Initialize options, in_file, and out_file from arguments. */
static void parse_args(int argc, char *argv[])
{
	int i;

	if (argc < 2)
	{
		print_usage();
		exit(1);
	}

	for (i=1; i<argc; i++)
	{
		char *s = argv[i];

		if (strcmp(s,"-o") == 0)
		{
			if (i+1 == argc)
			{
				report("missing filename after option -o\n");
				print_usage();
				exit(1);
			}
			else
			{
				i++;
				out_file = argv[i];
				continue;
			}
		}
		else if (s[0] != '+' && s[0] != '-')
		{
			in_file = s;
			continue;
		}
		else
		{
			parse_option(s);
		}
	}

	if (!in_file)
	{
		report("no input file argument given\n");
		print_usage();
		exit(1);
	}

	if (!out_file)	/* no -o option given */
	{
		int l = strlen(in_file);
		char *ext = strrchr(in_file, '.');

		if (ext && strcmp(ext,".st") == 0)
		{
			out_file = (char*)malloc(l);
			strcpy(out_file, in_file);
			strcpy(out_file+(ext-in_file), ".c\n");
		}
		else
		{
			out_file = (char*)malloc(l+3);
			sprintf(out_file, "%s.c", in_file);
		}
	}
}

static void parse_option(char *s)
{
	int		opt_val;

	opt_val = (*s == '+');

	switch (s[1])
	{
	case 'a':
		options.async = opt_val;
		break;
	case 'c':
		options.conn = opt_val;
		break;
	case 'd':
		options.debug = opt_val;
		break;
	case 'e':
		options.newef = opt_val;
		break;
	case 'r':
		options.reent = opt_val;
		break;
	case 'i':
		options.init_reg = opt_val;
		break;
	case 'l':
		options.line = opt_val;
		break;
	case 'm':
		options.main = opt_val;
		break;
	case 'w':
		options.warn = opt_val;
		break;
	default:
		report("unknown option ignored: '%s'\n", s);
		break;
	}
}

static void print_usage(void)
{
	report("%s\n", snc_version);
	report("usage: snc <options> <infile>\n");
	report("options:\n");
	report("  -o <outfile> - override name of output file\n");
	report("  +a           - do asynchronous pvGet\n");
	report("  -c           - don't wait for all connects\n");
	report("  +d           - turn on debug run-time option\n");
	report("  -e           - don't use new event flag mode\n");
	report("  -l           - suppress line numbering\n");
	report("  +m           - generate main program\n");
	report("  -i           - don't register commands/programs\n");
	report("  +r           - make reentrant at run-time\n");
	report("  -w           - suppress compiler warnings\n");
	report("example:\n snc +a -c vacuum.st\n");
}

void gen_line_marker_prim(int line_num, char *src_file)
{
	if (options.line)
		printf("# line %d \"%s\"\n", line_num, src_file);
}

/* Errors and warnings */

void report_loc(const char *src_file, int line_num)
{
	fprintf(stderr, "%s:%d: ", src_file, line_num);
}

void report_at(const char *src_file, int line_num, const char *format, ...)
{
	va_list args;

	report_loc(src_file, line_num);

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void report_at_expr(Expr *ep, const char *format, ...)
{
	va_list args;

	report_loc(ep->src_file, ep->line_num);

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void report(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
