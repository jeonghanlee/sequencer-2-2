#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "snl.h"
#include "snc_main.h"

#define	EOI		0

typedef unsigned int uint;
typedef unsigned char uchar;

#define	BSIZE	8192

#define	YYCTYPE			uchar
#define	YYCURSOR		cursor
#define	YYLIMIT			s->lim
#define	YYMARKER		s->ptr
#define	YYFILL			cursor = fill(s, cursor);
#define	YYDEBUG(state, current) fprintf(stderr, "state = %d, current = %c\n", state, current);

#define	RET(i)			{s->cur = cursor; return i;}

typedef struct Scanner {
	int	fd;	/* file descriptor */
	uchar	*bot;	/* pointer to bottom (start) of buffer */
	uchar	*tok;	/* pointer to start of current token */
	uchar	*end;	/* pointer to end of token (or 0, then use cur) */
	uchar	*ptr;	/* marker for backtracking (always > tok) */
	uchar	*cur;	/* saved scan position between calls to scan() */
	uchar	*lim;	/* pointer to one position after last read char */
	uchar	*top;	/* pointer to (one after) top of allocated buffer */
	uchar	*eof;	/* pointer to (one after) last char in file (or 0) */
	char	*file;	/* source file name */
	uint	line;	/* line number */
} Scanner;

static void scan_report(Scanner *s, const char *format, ...)
{
	va_list args;

	report_location(s->file, s->line);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

/*
From the re2c docs:

   The generated code "calls" YYFILL(n) when the buffer needs (re)filling: at
   least n additional characters should be provided. YYFILL(n) should adjust
   YYCURSOR, YYLIMIT, YYMARKER and YYCTXMARKER as needed. Note that for typical
   programming languages n will be the length of the longest keyword plus one.

We also add a '\n' byte at the end of the file as sentinel.
*/
static uchar *fill(Scanner *s, uchar *cursor) {
	/* does not touch s->cur, instead works with argument cursor */
	if (!s->eof) {
		uint read_cnt;			/* number of bytes read */
		uint garbage = s->tok - s->bot;	/* number of garbage bytes */
		uint valid = s->lim - s->tok;	/* number of still valid bytes to copy */
		uchar *token = s->tok;		/* start of valid bytes */
		uint space = (s->top - s->lim) + garbage;
						/* remaining space after garbage collection */
		int need_alloc = space < BSIZE;	/* do we need to allocate a new buffer? */

		/* anything below s->tok is garbage, collect it */
		if (garbage) {
			if (!need_alloc) {
				/* shift valid buffer content down to bottom of buffer */
				memcpy(s->bot, token, valid);
			}
			/* adjust pointers */
			s->tok = s->bot;	/* same as s->tok -= garbage */
			s->ptr -= garbage;
			cursor -= garbage;
			s->lim -= garbage;
			/* invariant: s->bot, s->top, s->eof, s->lim - s->tok */
		}
		/* increase the buffer size if necessary, ensuring that we have
		   at least BSIZE bytes of free space to fill (after s->lim) */
		if (need_alloc) {
			uchar *buf = (uchar*) malloc((s->lim - s->bot + BSIZE)*sizeof(uchar));
			memcpy(buf, token, valid);
			s->tok = buf;
			s->ptr = &buf[s->ptr - s->bot];
			cursor = &buf[cursor - s->bot];
			s->lim = &buf[s->lim - s->bot];
			s->top = s->lim + BSIZE;
			free(s->bot);
			s->bot = buf;
		}
		/* fill the buffer, starting at s->lim, by reading a chunk of
		   BSIZE bytes (or less if eof is encountered) */
		if ((read_cnt = read(s->fd, (char*) s->lim, BSIZE)) != BSIZE) {
			s->eof = &s->lim[read_cnt];
			/* insert sentinel and increase s->eof */
			*(s->eof)++ = '\n';
		}
		s->lim += read_cnt;	/* adjust limit */
	}
	return cursor;
}

/*!re2c
	re2c:yyfill:parameter	= 0;

	ANY	= .|"\n";
	SPC	= [ \t];
	OCT	= [0-7];
	DEC	= [0-9];
	LET	= [a-zA-Z_];
	HEX	= [a-fA-F0-9];
	EXP	= [Ee] [+-]? DEC+;
	FS	= [fFlL];
	IS	= [uUlL]*;
	ESC	= [\\] ([abfnrtv?'"\\] | "x" HEX+ | OCT+);
*/

static int scan(Scanner *s) {
	uchar *cursor = s->cur;
	uchar *str_end = 0;

        s->end = 0;
snl:
	s->tok = cursor;

/*!re2c
	"\n"		{
				if(cursor == s->eof) RET(EOI);
				s->line++;
				goto snl;
			}
	["]		{
				s->tok = cursor;
				goto string_const;
			}
	"/*"		{ goto comment; }
	"#" SPC*	{
				s->tok = cursor;
				goto line_marker;
			}
	"%{"		{
				s->tok = cursor;
				goto c_code;
			}
	("%%" .*)	{
				s->tok += 2;
				RET(CCODE);
			}
	"assign"	{ RET(ASSIGN); }
	"break"		{ RET(BREAK); }
	"char"		{ RET(CHAR); }
	"double"	{ RET(DOUBLE); }
	"else"		{ RET(ELSE); }
	"entry"		{ RET(ENTRY); }
	"evflag"	{ RET(EVFLAG); }
	"exit"		{ RET(EXIT); }
	"float"		{ RET(FLOAT); }
	"for"		{ RET(FOR); }
	"if"		{ RET(IF); }
	"int"		{ RET(INT); }
	"long"		{ RET(LONG); }
	"monitor"	{ RET(MONITOR); }
	"option"	{ RET(OPTION); }
	"program"	{ RET(PROGRAM); }
	"short"		{ RET(SHORT); }
	"ss"		{ RET(SS); }
	"state"		{ RET(STATE); }
	"string"	{ RET(STRING); }
	"syncQ"		{ RET(SYNCQ); }
	"sync"		{ RET(SYNC); }
	"to"		{ RET(TO); }
	"unsigned"	{ RET(UNSIGNED); }
	"when"		{ RET(WHEN); }
	"while"		{ RET(WHILE); }
	"TRUE"		{ RET(INTCON); }
	"FALSE"		{ RET(INTCON); }
	"ASYNC"		{ RET(INTCON); }
	"SYNC"		{ RET(INTCON); }
	LET (LET|DEC)*	{ RET(NAME); }
	("0" [xX] HEX+ IS?) | ("0" DEC+ IS?) | (DEC+ IS?) | (['] (ESC|ANY\[\n\\'])* ['])
			{ RET(INTCON); }

	(DEC+ EXP FS?) | (DEC* "." DEC+ EXP? FS?) | (DEC+ "." DEC* EXP? FS?)
			{ RET(FPCON); }

	">>="		{ RET(RSHEQ); }
	"<<="		{ RET(LSHEQ); }
	"+="		{ RET(ADDEQ); }
	"-="		{ RET(SUBEQ); }
	"*="		{ RET(MULEQ); }
	"/="		{ RET(DIVEQ); }
	"%="		{ RET(MODEQ); }
	"&="		{ RET(ANDEQ); }
	"^="		{ RET(XOREQ); }
	"|="		{ RET(OREQ); }
	">>"		{ RET(RSHIFT); }
	"<<"		{ RET(LSHIFT); }
	"++"		{ RET(INCR); }
	"--"		{ RET(DECR); }
	"->"		{ RET(POINTER); }
	"&&"		{ RET(ANDAND); }
	"||"		{ RET(OROR); }
	"<="		{ RET(LE); }
	">="		{ RET(GE); }
	"=="		{ RET(EQ); }
	"!="		{ RET(NE); }
	";"		{ RET(SEMICOLON); }
	"{"		{ RET(LBRACE); }
	"}"		{ RET(RBRACE); }
	","		{ RET(COMMA); }
	":"		{ RET(COLON); }
	"="		{ RET(EQUAL); }
	"("		{ RET(LPAREN); }
	")"		{ RET(RPAREN); }
	"["		{ RET(LBRACKET); }
	"]"		{ RET(RBRACKET); }
	"."		{ RET(PERIOD); }
	"&"		{ RET(AMPERSAND); }
	"!"		{ RET(NOT); }
	"~"		{ RET(TILDE); }
	"-"		{ RET(SUB); }
	"+"		{ RET(ADD); }
	"*"		{ RET(ASTERISK); }
	"/"		{ RET(SLASH); }
	"%"		{ RET(MOD); }
	"<"		{ RET(LT); }
	">"		{ RET(GT); }
	"^"		{ RET(CARET); }
	"|"		{ RET(VBAR); }
	"?"		{ RET(QUESTION); }
	[ \t\v\f]+	{ goto snl; }
	ANY		{ scan_report(s, "invalid character\n"); RET(EOI); }
*/

string_const:
/*!re2c
	(ESC | [^"\n\\])*
			{ goto string_const; }
	["]		{
				str_end = cursor - 1;
				goto string_cat;
			}
	ANY		{ scan_report(s, "invalid character in string constant\n"); RET(EOI); }
*/

string_cat:
/*!re2c
	SPC+		{ goto string_cat; }
	"\n"		{
				if (cursor == s->eof) {
					s->end = str_end;
					cursor -= 1;
					RET(STRCON);
				}
				s->line++;
				goto string_cat;
			}
	["]		{
				uint len = str_end - s->tok;
				memmove(cursor - len, s->tok, len);
				s->tok = cursor - len;
				goto string_const;
			}
	ANY		{
				s->end = str_end;
				cursor -= 1;
				RET(STRCON);
			}
*/

line_marker:
/*!re2c
	DEC+SPC*	{
				s->line = atoi((char*)s->tok) - 1;
				s->tok = cursor;
				goto line_marker_str;
			}
	ANY		{ goto line_marker_skip; }
*/

line_marker_str:
/*!re2c
	(["] (ESC|ANY\[\n\\"])* ["])
			{
				cursor[-1] = 0;
				if (!s->file) {
					s->file = strdup((char *)(s->tok + 1));
				} else if (s->file && strcmp((char*)s->tok, s->file) != 0) {
					free(s->file);
					s->file = strdup((char *)(s->tok + 1));
				}
				goto line_marker_skip;
			}
	"\n"		{
				if (cursor == s->eof) {
					s->end = str_end;
					cursor -= 1;
					RET(STRCON);
				}
				s->line++;
				goto string_cat;
			}
	.		{ goto line_marker_skip; }
*/

line_marker_skip:
/*!re2c
	.*		{ goto snl; }
	"\n"		{ cursor -= 1; goto snl;}
*/

comment:
/*!re2c
	"*/"		{ goto snl; }
	.		{ goto comment; }
	"\n"		{
				if (cursor == s->eof) {
					scan_report(s, "at eof: unterminated comment\n");
					RET(EOI);
				}
				s->tok = cursor;
				s->line++;
				goto comment;
			}
*/

c_code:
/*!re2c
	"}%"		{
				s->end = cursor - 2;
				RET(CCODE);
			}
	.		{ goto c_code; }
	"\n"		{
				if (cursor == s->eof) {
					scan_report(s, "at eof: unterminated literal c-code section\n");
					RET(EOI);
				}
				s->line++;
				goto c_code;
			}
*/
}

#ifdef TEST_LEXER
int main() {
	Scanner s;
	int t;
	memset((char*) &s, 0, sizeof(s));
	s.fd = 0;
	s.cur = fill(&s, s.cur);
	s.line = 1;
	while( (t = scan(&s)) != EOI) {
		if (!s.end) s.end = s.cur;
		printf("%s:%d: %2d\t£%.*s£\n", s.file, s.line, t, s.end - s.tok, s.tok);
	}
	close(s.fd);
}
#else

extern void parser(
	void *yyp,		/* the parser */
	int yymajor,		/* the major token code number */
	char *yyminor		/* the value for the token */
);
extern void *parserAlloc(void *(*mallocProc)(size_t));
void parserFree(
	void *p,		/* the parser to be deleted */
	void (*freeProc)(void*)	/* function used to reclaim memory */
);

void compile(void)
{
	Scanner s;
	int t;
	char *x;

	bzero(&s, sizeof(s));
	s.cur = fill(&s, s.cur); /* otherwise scanner crashes in debug mode */
	s.line = 1;

	void *pParser = parserAlloc(malloc);
	do
	{
		globals->prev_line_num = s.line;
		t = scan(&s);
		globals->src_file = s.file;
		globals->line_num = s.line;

		if (!s.end) s.end = s.cur;
		scan_report(&s,"%2d\t£%.*s£\n", t, s.end - s.tok, s.tok);
		x = malloc(s.end - s.tok + 1);
		memcpy(x,s.tok,s.end - s.tok);
		x[s.end - s.tok] = 0;
		parser(pParser, t, x);
	}
	while (t);
	parserFree(pParser, free);
}

#endif
