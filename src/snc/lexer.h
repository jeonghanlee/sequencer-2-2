#ifndef INCLlexerh
#define INCLlexerh

#include "types.h"

Program *parse_program(const char *src_file);

void parser(
	void *yyp,		/* the parser */
	int yymajor,		/* the major token code number */
	Token yyminor,		/* the value for the token */
	Program **presult	/* extra argument */
);

void *parserAlloc(void *(*mallocProc)(size_t));

void parserFree(
	void *p,		/* the parser to be deleted */
	void (*freeProc)(void*)	/* function used to reclaim memory */
);

#endif	/*INCLlexerh*/
