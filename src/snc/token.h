#ifndef INCLtokenh
#define INCLtokenh

/* This is stuff to interface flex generated scanner code with the
   lemon generated parser. */

#include "parse.h"
typedef union token {
	char	*str;
} Token;

#define YYSTYPE Token
YYSTYPE yylval;

#ifdef YY_DECL
#undef YY_DECL
#endif

#define YY_DECL int yylex (void)

extern YY_DECL;

#endif /* INCLtokenh */
