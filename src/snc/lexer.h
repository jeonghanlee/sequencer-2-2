#ifndef INCLlexerh
#define INCLlexerh

#include "types.h"

Expr *parse_program(const char *src_file);

void snlParser(
	void    *yyp,		/* the parser */
	int     yymajor,	/* the major token code number */
	Token   yyminor,	/* the value for the token */
	Expr    **presult	/* extra argument */
);

void *snlParserAlloc(void *(*mallocProc)(size_t));

void snlParserFree(
	void *p,		/* the parser to be deleted */
	void (*freeProc)(void*)	/* function used to reclaim memory */
);

#endif	/*INCLlexerh*/
