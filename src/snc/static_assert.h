/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
		static (compile time) assertions
\*************************************************************************/
#ifndef INCLstatic_asserth
#define INCLstatic_asserth

#define CONCAT2(x,y) x ## y
#define CONCAT(x,y) CONCAT2(x,y)
#define STATIC_ASSERT(cond,msg) \
    typedef struct { int CONCAT(static_assertion_failed_,msg) : !!(cond); } \
        CONCAT(static_assertion_failed_,__COUNTER__)

/*
 * usage:
 *     STATIC_ASSERT(condition_to_assert, identifier_that_explains_the_assertion);
 */

#endif	/*INCLstatic_asserth*/
