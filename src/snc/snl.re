#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "snl.h"
#include "snc_main.h"
#include "lexer.h"

#define	EOI		0

typedef unsigned int uint;
typedef unsigned char uchar;

#define	BSIZE	8192

#define	YYCTYPE			uchar
#define	YYCURSOR		cursor
#define	YYLIMIT			s->lim
#define	YYMARKER		s->ptr
#define	YYFILL			cursor = fill(s, cursor);
#define	YYDEBUG(state, current) report("state = %d, current = %c\n", state, current);

#define	RET(i,r) {\
	s->cur = cursor;\
	t->str = r;\
	return i;\
}

typedef struct Scanner {
	int	fd;	/* file descriptor */
	uchar	*bot;	/* pointer to bottom (start) of buffer */
	uchar	*tok;	/* pointer to start of current token */
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

	report_loc(s->file, s->line);
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

/* alias strdup_from_to: duplicate string from start to (exclusive) stop */
char *strdupft(uchar *start, uchar *stop) {
	char *result;
	char c = *stop;
	*stop = 0;
	result = strdup((char*)start);
	*stop = c;
#if 0
	int len = stop - start;
	result = malloc(len + 1);
	memcpy(result, start, len);
	result[len] = 0;
#endif
	return result;
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

static int scan(Scanner *s, Token *t) {
	uchar *cursor = s->cur;
	uchar *end = cursor;
	int in_c_code = 0;

snl:
	if (in_c_code)
		goto c_code;
	t->line = s->line;
	t->file = s->file;
	s->tok = cursor;

/*!re2c
	"\n"		{
				if(cursor == s->eof) RET(EOI,0);
				s->line++;
				goto snl;
			}
	["]		{
				s->tok = end = cursor;
				goto string_const;
			}
	"/*"		{ goto comment; }
	"#" SPC*	{
				s->tok = cursor;
				goto line_marker;
			}
	"%{"		{
				s->tok = cursor;
				in_c_code = 1;
				goto c_code;
			}
	"%%" SPC*	{
				s->tok = end = cursor;
				goto c_code_line;
			}
	"assign"	{ RET(ASSIGN,	"assign"); }
	"break"		{ RET(BREAK,	"break"); }
	"char"		{ RET(CHAR,	"char"); }
	"continue"	{ RET(CONTINUE,	"continue"); }
	"declare"	{ RET(DECLARE,	"declare"); }
	"delay"		{ RET(DELAY,	"delay"); }
	"double"	{ RET(DOUBLE,	"double"); }
	"else"		{ RET(ELSE,	"else"); }
	"entry"		{ RET(ENTRY,	"entry"); }
	"evflag"	{ RET(EVFLAG,	"evflag"); }
	"exit"		{ RET(EXIT,	"exit"); }
	"float"		{ RET(FLOAT,	"float"); }
	"for"		{ RET(FOR,	"for"); }
	"if"		{ RET(IF,	"if"); }
	"int"		{ RET(INT,	"int"); }
	"long"		{ RET(LONG,	"long"); }
	"monitor"	{ RET(MONITOR,	"monitor"); }
	"option"	{ RET(OPTION,	"option"); }
	"program"	{ RET(PROGRAM,	"program"); }
	"short"		{ RET(SHORT,	"short"); }
	"ss"		{ RET(SS,	"ss"); }
	"state"		{ RET(STATE,	"state"); }
	"string"	{ RET(STRING,	"string"); }
	"syncQ"		{ RET(SYNCQ,	"syncQ"); }
	"sync"		{ RET(SYNC,	"sync"); }
	"to"		{ RET(TO,	"to"); }
	"unsigned"	{ RET(UNSIGNED,	"unsigned"); }
	"when"		{ RET(WHEN,	"when"); }
	"while"		{ RET(WHILE,	"while"); }
	"TRUE"		{ RET(INTCON,	"TRUE"); }
	"FALSE"		{ RET(INTCON,	"FALSE"); }
	"ASYNC"		{ RET(INTCON,	"ASYNC"); }
	"SYNC"		{ RET(INTCON,	"SYNC"); }
	LET (LET|DEC)*	{ RET(NAME, strdupft(s->tok, cursor)); }
	("0" [xX] HEX+ IS?) | ("0" DEC+ IS?) | (DEC+ IS?) | (['] (ESC|ANY\[\n\\'])* ['])
			{ RET(INTCON, strdupft(s->tok, cursor)); }

	(DEC+ EXP FS?) | (DEC* "." DEC+ EXP? FS?) | (DEC+ "." DEC* EXP? FS?)
			{ RET(FPCON, strdupft(s->tok, cursor)); }

	">>="		{ RET(RSHEQ,	">>="); }
	"<<="		{ RET(LSHEQ,	"<<="); }
	"+="		{ RET(ADDEQ,	"+="); }
	"-="		{ RET(SUBEQ,	"-="); }
	"*="		{ RET(MULEQ,	"*="); }
	"/="		{ RET(DIVEQ,	"/="); }
	"%="		{ RET(MODEQ,	"%="); }
	"&="		{ RET(ANDEQ,	"&="); }
	"^="		{ RET(XOREQ,	"^="); }
	"|="		{ RET(OREQ,	"|="); }
	">>"		{ RET(RSHIFT,	">>"); }
	"<<"		{ RET(LSHIFT,	"<<"); }
	"++"		{ RET(INCR,	"++"); }
	"--"		{ RET(DECR,	"--"); }
	"->"		{ RET(POINTER,	"->"); }
	"&&"		{ RET(ANDAND,	"&&"); }
	"||"		{ RET(OROR,	"||"); }
	"<="		{ RET(LE,	"<="); }
	">="		{ RET(GE,	">="); }
	"=="		{ RET(EQ,	"=="); }
	"!="		{ RET(NE,	"!="); }
	";"		{ RET(SEMICOLON,";"); }
	"{"		{ RET(LBRACE,	"{"); }
	"}"		{ RET(RBRACE,	"}"); }
	","		{ RET(COMMA,	","); }
	":"		{ RET(COLON,	":"); }
	"="		{ RET(EQUAL,	"="); }
	"("		{ RET(LPAREN,	"("); }
	")"		{ RET(RPAREN,	")"); }
	"["		{ RET(LBRACKET,	"["); }
	"]"		{ RET(RBRACKET,	"]"); }
	"."		{ RET(PERIOD,	"."); }
	"&"		{ RET(AMPERSAND,"&"); }
	"!"		{ RET(NOT,	"!"); }
	"~"		{ RET(TILDE,	"~"); }
	"-"		{ RET(SUB,	"-"); }
	"+"		{ RET(ADD,	"+"); }
	"*"		{ RET(ASTERISK,	"*"); }
	"/"		{ RET(SLASH,	"/"); }
	"%"		{ RET(MOD,	"%"); }
	"<"		{ RET(LT,	"<"); }
	">"		{ RET(GT,	">"); }
	"^"		{ RET(CARET,	"^"); }
	"|"		{ RET(VBAR,	"|"); }
	"?"		{ RET(QUESTION,	"?"); }
	[ \t\v\f]+	{ goto snl; }
	ANY		{ scan_report(s, "invalid character\n"); RET(EOI,0); }
*/

string_const:
/*!re2c
	(ESC | [^"\n\\])*
			{ goto string_const; }
	["]		{
				end = cursor - 1;
				goto string_cat;
			}
	ANY		{ scan_report(s, "invalid character in string constant\n"); RET(EOI,0); }
*/

string_cat:
/*!re2c
	SPC+		{ goto string_cat; }
	"\n"		{
				if (cursor == s->eof) {
					cursor -= 1;
					RET(STRCON, strdupft(s->tok, end));
				}
				s->line++;
				goto string_cat;
			}
	["]		{
				uint len = end - s->tok;
				memmove(cursor - len, s->tok, len);
				s->tok = cursor - len;
				goto string_const;
			}
	ANY		{
				cursor -= 1;
				RET(STRCON, strdupft(s->tok, end));
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
					cursor -= 1;
					goto snl;
				}
				s->line++;
				goto string_cat;
			}
	.		{ goto line_marker_skip; }
*/

line_marker_skip:
/*!re2c
	.*		{ goto snl; }
	"\n"		{ cursor -= 1; goto snl; }
*/

comment:
/*!re2c
	"*/"		{ goto snl; }
	.		{ goto comment; }
	"\n"		{
				if (cursor == s->eof) {
					scan_report(s, "at eof: unterminated comment\n");
					RET(EOI,0);
				}
				s->tok = cursor;
				s->line++;
				goto comment;
			}
*/

c_code:
/*!re2c
	"}%"		{ RET(CCODE, strdupft(s->tok, cursor - 2)); }
	.		{ goto c_code; }
	"#" SPC*	{ goto line_marker; }
	"\n"		{
				if (cursor == s->eof) {
					scan_report(s, "at eof: unterminated literal c-code section\n");
					RET(EOI,0);
				}
				s->line++;
				goto c_code;
			}
*/

c_code_line:
/*!re2c
	.		{
				end = cursor;
				goto c_code_line;
			}
	SPC* "\n"	{
				if (cursor == s->eof) {
					cursor -= 1;
				}
				if (end > s->tok) {
					RET(CCODE, strdupft(s->tok, end));
				}
				goto snl;
			}
*/
}

#ifdef TEST_LEXER
void report_loc(const char *f, int l) {
	fprintf(stderr, "%s:%d: ", f, l);
}

int main() {
	Scanner s;
	int		tt;	/* token type */
	Token		tv;	/* token value */

	bzero(&s, sizeof(s));

	s.cur = fill(&s, s.cur);
	s.line = 1;

	while( (tt = scan(&s, &tv)) != EOI) {
		printf("%s:%d: %2d\t$%s$\n", tv.file, tv.line, tt, tv.str);
	}
	return 0;
}
#else

Expr *parse_program(const char *src_file)
{
	Scanner	s;
	int	tt;		/* token type */
	Token	tv;		/* token value */
        Expr	*result;	/* result of parsing */

	bzero(&s, sizeof(s));
	s.file = strdup(src_file);
	s.line = 1;

	void *parser = snlParserAlloc(malloc);
	do
	{
		tt = scan(&s, &tv);
#ifdef	DEBUG
		report_at(tv.file, tv.line, "%2d\t$%s$\n", tt, tv.str);
#endif
		snlParser(parser, tt, tv, &result);
	}
	while (tt);
	snlParserFree(parser, free);
	return result;
}

#endif /* TEST_LEXER */
