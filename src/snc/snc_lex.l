%{
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	snc_lex.l,v 1.2 1995/06/27 15:26:09 wright Exp
	ENVIRONMENT: UNIX
	HISTORY:
20nov91,ajk	Added OPTION token.
15jan92,ajk	Fixed to allow "!" as a unary operator.
17Jul92,rcz	changed warn_flag to warn_opt
17Jul92,rcz	Ported ajk version from lanl
04apr93,ajk	Increased STR_BFR_SIZE from 30000 to 300000
19nov93,ajk	Added definitions for octal and hex numbers.
19nov93,ajk	Removed optional "-" from definition of FPNUM.  This was
		causing problems with statements like "k-1".
27man94,ajk	Implemented dynamic allocation of strings, thus eliminating
		huge string buffer.
31may94,ajk	Changed handling of escaped C code.
17feb95,ajk	Removed includes "parse.h" & "snc.h", because this module
		now gets included in snc.y.
20jul95,ajk	Added unsigned types.
11jul96,ajk	Added character constants
08aug96,wfl	Added SYNCQ token.
23jun97,wfl	Added ^L as white-space char; permitted '"x" "y"' syntax.
01oct98,wfl     Replaced FPNUM with version from Sun lex manual ("E" support)
7sep99,wfl	Added QUESTION token (for ternary operator).
22sep99,grw     Supported ENTRY token (for entry action).
06mar00,wfl	Supported ASYNC and STNC tokens (avoids need for #define).
***************************************************************************/
/* Lexical analyzer for State Notation Compiler (snc).
 *
 * This routine recognizes State Notation Language (SNL) syntax,
 * and passes tokens to yacc().
 * All C code is passed through as a stream, without conversion.
 * Hence, the C compiler may find errors not reported by SNC.
 * Comments are recognized as part of the syntax.
 */

#include	<string.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	"snc_main.h"
#ifdef USE_LEMON
#include	"token.h"
#include	"snl.h"
#endif

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif

#ifdef	DEBUG
#define	RETURN(param) {\
	fprintf(stderr, "return(" #param ")\n");\
	globals->prev_line_num = tok_line_num;\
	tok_line_num = globals->line_num;\
	return(param);\
	}
#else
#define	RETURN(param) {\
	globals->prev_line_num = tok_line_num;\
	tok_line_num = globals->line_num;\
	return(param);\
	}
#endif

#define	STR_BFR_SIZE	1000

static char		strBfr[STR_BFR_SIZE];	/* holding place for strings */
static char		*pStr;			/* current ptr to strBfr */

%}

/* Start conditions (SNL, C code, comment, string, pre-processor, pre-proc. string) */
%Start	SNL C_CODE COMMENT STR PP PP_SP PP_STR PP_END

OCT	[0-7]
DEC	[0-9]
LET	[a-zA-Z_]
HEX	[a-fA-F0-9]
EXP	[DEde][+-]?{DEC}+
FS	[fFlL]
IS	[uUlL]*
ESC	\\([abfnrtv?'"\\]|"x"{HEX}+|{OCT}+)

NAME	{LET}({LET}|{DEC})*
HEXNUM	0[xX]{HEX}+{IS}?
OCTNUM	0{OCT}+{IS}?
DECNUM	{DEC}+{IS}?
CHRCON	'({ESC}|[^\n\\'])*'

INTNUM	{HEXNUM}|{OCTNUM}|{DECNUM}|{CHRCON}
FPNUM	{DEC}+{EXP}{FS}?|{DEC}*\.{DEC}+{EXP}?{FS}?|{DEC}+\.{DEC}*{EXP}?{FS}?
/*
FPNUM	{DEC}+({EXP})?|{DEC}+\.{DEC}*({EXP})?|{DEC}*\.{DEC}+({EXP})?
*/

%% /* Begin rules */

<C_CODE>\n {
		globals->line_num++;
}
<C_CODE>"}%" {
		BEGIN SNL;
}
<C_CODE>([^}\n]|[}][^%])* {
		globals->c_line_num = globals->line_num;
		yylval.str = strdup((char*)yytext);
		RETURN(C_STMT);
}

<COMMENT>\n	globals->line_num++;
<COMMENT>"*/"	BEGIN SNL;
<COMMENT>.	;

<STR>"\\\"" {
		*pStr++ = yytext[0];
		*pStr++ = yytext[1];
}
<STR>\"[\n\t\ ]*\" /* ignored */ ;
<STR>\"	 {
		*pStr++ = 0;
		yylval.str = strdup(strBfr);
		BEGIN SNL;
		RETURN(STRING);
}
<STR>.		{ *pStr++ = yytext[0]; }
<STR>\n	{
		*pStr++ = '?';
		if (globals->options->warn) {
			parse_error("warning: newline in string");
		}
		globals->line_num++;
}

<PP>[\t\ ]+	;
<PP>{DEC}+ {
		globals->line_num = atoi((char*)yytext) - 1;
		pStr = strBfr;
		BEGIN PP_SP;
}
<PP>.		BEGIN PP_END;
<PP_SP>[\t\ ]+	;
<PP_SP>\" {
		pStr = strBfr;
		BEGIN PP_STR;
}
<PP_SP>.	BEGIN PP_END;
<PP_STR>\" {	/* done scanning the string */
		*pStr++ = 0;
		if (strcmp(strBfr,globals->src_file) != 0) {
			globals->src_file = strdup(strBfr);
		}
		BEGIN PP_END;
}
<PP_STR>.	{ *pStr++ = yytext[0]; }
<PP_STR>\n	{ *pStr++ = '?'; globals->line_num++; }
<PP_END>.*	;
<PP_END>\n	{ globals->line_num++; BEGIN SNL; }

<SNL>\n		{ globals->line_num++; }
<SNL>"%{"	BEGIN C_CODE;
<SNL>"%%".* {
		globals->c_line_num = globals->line_num;
		yylval.str = strdup((char*)yytext+2);
		RETURN(C_STMT);
}
<SNL>^# {
		BEGIN PP;
}
<SNL>"/*"	BEGIN COMMENT;
<SNL>\"		{ pStr = strBfr; BEGIN STR; }
<SNL>"ss"	RETURN(STATE_SET);
<SNL>"state"	RETURN(STATE);
<SNL>"when"	RETURN(WHEN);
<SNL>"monitor"	RETURN(MONITOR);
<SNL>"assign"	RETURN(ASSIGN);
<SNL>"unsigned"	RETURN(UNSIGNED);
<SNL>"char"	RETURN(CHAR);
<SNL>"short"	RETURN(SHORT);
<SNL>"int"	RETURN(INT);
<SNL>"long"	RETURN(LONG);
<SNL>"float"	RETURN(FLOAT);
<SNL>"double"	RETURN(DOUBLE);
<SNL>"string"	RETURN(STRING_DECL);
<SNL>"to"	RETURN(TO);
<SNL>"program"	RETURN(PROGRAM);
<SNL>"option"	RETURN(OPTION);
<SNL>"debug"	RETURN(DEBUG_PRINT);
<SNL>"evflag"	RETURN(EVFLAG);
<SNL>"sync"	RETURN(SYNC);
<SNL>"syncQ"	RETURN(SYNCQ);
<SNL>"if"	RETURN(IF);
<SNL>"else"	RETURN(ELSE);
<SNL>"while"	RETURN(WHILE);
<SNL>"for"	RETURN(FOR);
<SNL>"break"	RETURN(BREAK);
<SNL>"exit"	RETURN(EXIT);
<SNL>"entry"	RETURN(ENTRY);
<SNL>"TRUE" {
		yylval.str = "1";
		RETURN(INTNUM);
}
<SNL>"FALSE" {
		yylval.str = "0";
		RETURN(INTNUM);
}
<SNL>"ASYNC" {
		yylval.str = "1";
		RETURN(INTNUM);
}
<SNL>"SYNC" {
		yylval.str = "2";
		RETURN(INTNUM);
}
<SNL>{NAME} {
		yylval.str = strdup((char *)yytext);
		RETURN(NAME);
}
<SNL>"++"	RETURN(AUTO_INCR);
<SNL>"--"	RETURN(AUTO_DECR);
<SNL>"||"	RETURN(OR);
<SNL>"<<="	RETURN(LEFT_EQUAL);
<SNL>">>="	RETURN(RIGHT_EQUAL);
<SNL>"&&"	RETURN(AND);
<SNL>">>"	RETURN(R_SHIFT);
<SNL>">="	RETURN(GE);
<SNL>"=="	RETURN(EQ);
<SNL>"+="	RETURN(PLUS_EQUAL);
<SNL>"-="	RETURN(MINUS_EQUAL);
<SNL>"*="	RETURN(MULT_EQUAL);
<SNL>"/="	RETURN(DIV_EQUAL);
<SNL>"&="	RETURN(AND_EQUAL);
<SNL>"|="	RETURN(OR_EQUAL);
<SNL>"!="	RETURN(NE);
<SNL>"<<"	RETURN(L_SHIFT);
<SNL>"<="	RETURN(LE);
<SNL>"%="	RETURN(MODULO_EQUAL);
<SNL>"^="	RETURN(XOR_EQUAL);
<SNL>"->"	RETURN(POINTER);
<SNL>"+"	RETURN(PLUS);
<SNL>"-"	RETURN(MINUS);
<SNL>"!"	RETURN(NOT);
<SNL>"/"	RETURN(SLASH);
<SNL>"<"	RETURN(LT);
<SNL>">"	RETURN(GT);
<SNL>"|"	RETURN(BIT_OR);
<SNL>"^"	RETURN(BIT_XOR);
<SNL>"~"	RETURN(COMPLEMENT);
<SNL>"%"	RETURN(MODULO);
<SNL>"="	RETURN(EQUAL);
<SNL>"&"	RETURN(AMPERSAND);
<SNL>"*"	RETURN(ASTERISK);
<SNL>"{"	RETURN(L_BRACKET);
<SNL>"}"	RETURN(R_BRACKET);
<SNL>"["	RETURN(L_SQ_BRACKET);
<SNL>"]"	RETURN(R_SQ_BRACKET);
<SNL>"?"	RETURN(QUESTION);
<SNL>":"	RETURN(COLON);
<SNL>";"	RETURN(SEMI_COLON);
<SNL>"."	RETURN(PERIOD);
<SNL>"("	RETURN(L_PAREN);
<SNL>")"	RETURN(R_PAREN);
<SNL>","	RETURN(COMMA);
<SNL>{INTNUM} {
		yylval.str = strdup((char *)yytext);
		RETURN(INTNUM);
}
<SNL>{FPNUM} {
		yylval.str = strdup((char *)yytext);
		RETURN(FPNUM);
}
<SNL>[\f\t\ ]*	/* no action */ ;
<SNL>.		RETURN(BAD_CHAR);
.|\n		{ globals->line_num = 1; BEGIN SNL; yyless(0); }
