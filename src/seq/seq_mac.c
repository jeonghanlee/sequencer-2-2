/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	Macro routines for Sequencer.
	The macro table contains name & value pairs.  These are both pointers
	to strings.
***************************************************************************/
/* #define DEBUG printf */
#define DEBUG nothing

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "seq.h"

/* Macro table */
struct macro
{
	char	*pName;
	char	*pValue;
	MACRO	*next;
};

static unsigned seqMacParseName(char *pStr);
static unsigned seqMacParseValue(char *pStr);
static char *skipBlanks(char *pChar);
static MACRO *seqMacTblGet(SPROG *pSP, char *pName);

/* 
 *seqMacEval - substitute macro value into a string containing:
 * ....{mac_name}....
 */
void seqMacEval(SPROG *pSP, char *pInStr, char *pOutStr, size_t maxChar)
{
	char	name[50], *pValue, *pTmp;
	size_t	valLth, nameLth;

	DEBUG("seqMacEval: InStr=%s, ", pInStr);

	pTmp = pOutStr;
	while (*pInStr != 0 && maxChar > 0)
	{
		if (*pInStr == '{')
		{	/* Do macro substitution */
			pInStr++; /* points to macro name */
			/* Copy the macro name */
			nameLth = 0;
			while (*pInStr != '}' && *pInStr != 0)
			{
				name[nameLth] = *pInStr++;
				if (nameLth < (sizeof(name) - 1))
					nameLth++;
			}
			name[nameLth] = 0;
			if (*pInStr != 0)
				pInStr++;
				
			DEBUG("Macro name=%s, ", name);

			/* Find macro value from macro name */
			pValue = seqMacValGet(pSP, name);
			if (pValue != NULL)
			{	/* Substitute macro value */
				valLth = strlen(pValue);
				if (valLth > maxChar)
					valLth = maxChar;

				DEBUG("Value=%s, ", pValue);

				strncpy(pOutStr, pValue, valLth);
				maxChar -= valLth;
				pOutStr += valLth;
			}
			
		}
		else
		{	/* Straight substitution */
			*pOutStr++ = *pInStr++;
			maxChar--;
		}
	}
	*pOutStr = 0;

	DEBUG("OutStr=%s\n", pTmp);
}

/*
 * seqMacValGet - internal routine to convert macro name to macro value.
 */
char *seqMacValGet(SPROG *pSP, char *pName)
{
	MACRO	*pMac;

	DEBUG("seqMacValGet: name=%s", pName);
	foreach(pMac, pSP->pMacros)
	{
		if (pMac->pName && strcmp(pName, pMac->pName) == 0)
		{
			DEBUG(", value=%s\n", pMac->pValue);
			return pMac->pValue;
		}
	}
	DEBUG(", no value\n");
	return NULL;
}

/*
 * seqMacParse - parse the macro definition string and build
 * the macro table (name/value pairs). Returns number of macros parsed.
 * Assumes the table may already contain entries (values may be changed).
 * String for name and value are allocated dynamically from pool.
 */
void seqMacParse(SPROG *pSP, char *pMacStr)
{
	unsigned	nChar;
	MACRO		*pMac;		/* macro tbl entry */
	char		*pName, *pValue;

	if (pMacStr == NULL) return;

	while(*pMacStr)
	{
		/* Skip blanks */
		pMacStr = skipBlanks(pMacStr);

		/* Parse the macro name */
		nChar = seqMacParseName(pMacStr);
		if (nChar == 0)
			break;		/* finished or error */
		pName = newArray(char, nChar+1);
		if (pName == NULL)
			break;
		memcpy(pName, pMacStr, nChar);
		pName[nChar] = '\0';

		DEBUG("name=%s, nChar=%d\n", pName, nChar);

		pMacStr += nChar;

		/* Find a slot in the table */
		pMac = seqMacTblGet(pSP, pName);
		if (pMac == NULL)
			break;		/* table is full */
		if (pMac->pName == NULL)
		{	/* Empty slot, insert macro name */
			pMac->pName = pName;
		}

		/* Skip over blanks and equal sign or comma */
		pMacStr = skipBlanks(pMacStr);
		if (*pMacStr == ',')
		{
			/* no value after the macro name */
			pMacStr++;
			continue;
		}
		if (*pMacStr == '\0' || *pMacStr++ != '=')
			break;
		pMacStr = skipBlanks(pMacStr);

		/* Parse the value */
		nChar = seqMacParseValue(pMacStr);
		if (nChar == 0)
			break;

		/* Remove previous value if it exists */
		pValue = pMac->pValue;
		if (pValue != NULL)
			free(pValue);

		/* Copy value string into newly allocated space */
		pValue = newArray(char, nChar+1);
		if (pValue == NULL)
			break;
		pMac->pValue = pValue;
		memcpy(pValue, pMacStr, nChar);
		pValue[nChar] = '\0';

		DEBUG("value=%s, nChar=%d\n", pValue, nChar);

		/* Skip past last value and over blanks and comma */
		pMacStr += nChar;
		pMacStr = skipBlanks(pMacStr);
		if (*pMacStr == '\0' || *pMacStr++ != ',')
			break;
	}
}

/*
 * seqMacParseName() - Parse a macro name from the input string.
 */
static unsigned seqMacParseName(char *pStr)
{
	unsigned nChar;

	/* First character must be [A-Z,a-z] */
	if (!isalpha(*pStr))
		return 0;
	pStr++;
	nChar = 1;
	/* Character must be [A-Z,a-z,0-9,_] */
	while ( isalnum(*pStr) || *pStr == '_' )
	{
		pStr++;
		nChar++;
	}
	/* Loop terminates on any non-name character */
	return nChar;
}

/*
 * seqMacParseValue() - Parse a macro value from the input string.
 */
static unsigned seqMacParseValue(char *pStr)
{
	unsigned nChar;

	nChar = 0;
	/* Character string terminates on blank, comma, or EOS */
	while ( (*pStr != ' ') && (*pStr != ',') && (*pStr != 0) )
	{
		pStr++;
		nChar++;
	}
	return nChar;
}

/* skipBlanks() - skip blank characters */
static char *skipBlanks(char *pChar)
{
	while (*pChar == ' ')
		pChar++;
	return	pChar;
}

/*
 * seqMacTblGet - find a match for the specified name, otherwise
 * return a new empty slot in macro table.
 */
static MACRO *seqMacTblGet(SPROG *pSP, char *pName)
{
	MACRO	*pMac, *pLastMac = NULL;

	DEBUG("seqMacTblGet: name=%s\n", pName);
	foreach(pMac, pSP->pMacros)
	{
		pLastMac = pMac;
		if (pMac->pName != NULL &&
			strcmp(pName, pMac->pName) == 0)
		{
			return pMac;
		}
	}
	/* Not found, allocate an empty slot */
	pMac = new(MACRO);
	/* This assumes ptr assignment is atomic */
	if (pLastMac != NULL)
		pLastMac->next = pMac;
	else
		pSP->pMacros = pMac;
	return pMac;
}

/*
 * seqMacFree - free all the memory
 */
void seqMacFree(SPROG *pSP)
{
	MACRO	*pMac, *pLastMac = NULL;

	foreach(pMac, pSP->pMacros)
	{
		if (pMac->pName != NULL)
			free(pMac->pName);
		if (pMac->pValue != NULL)
			free(pMac->pValue);
		if (pLastMac != NULL)
			free(pLastMac);
		pLastMac = pMac;
	}
	if (pLastMac != NULL)
		free(pLastMac);
	pSP->pMacros = 0;
}
