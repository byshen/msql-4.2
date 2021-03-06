/*
** Copyright (c) 1995-2001  Hughes Technologies Pty Ltd.  All rights
** reserved.  
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this 
** software, including all implied warranties of merchantability and 
** fitness, in no event shall Hughes Technologies be liable for any 
** special, indirect or consequential damages or any damages whatsoever 
** resulting from loss of use, data or profits, whether in an action of 
** contract, negligence or other tortious action, arising out of or in 
** connection with the use or performance of this software.
**
**
** $Id: regex.c,v 1.7 2002/06/29 04:09:01 bambi Exp $
**
*/

/*
** Module	: main : regex
** Purpose	: Implementation of SQL regular expressions
** Exports	: 
** Depends Upon	: 
*/


/*
** Using basic ANSI SQL regular expression specification,
** see if a expression matches some data.  The expression
** is nul terminated and the data is terminated by the
** length dlen which is enforced by the use of msqlStringLength.
** (See charMatch.)  It would really be nice if mSQL kept
** track of the actual data lengths in the db (and of literals).
** In the meantime, using msqlStringLength is a win for
** expressions with multiple % wildcards, especially if the data is
** at least of moderate length.  It does slow down expressions
** without %'s or matches to very short data.
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <common/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif

#include <common/portability.h>

/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <ctype.h>

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/regexp/regexp.h>
#include <libmsql/msql.h>

#include "regex.h"
#include "varchar.h"


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

extern char errMsg[];

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static char *getNextChar(type, data, last)
	void	*data;
	char	*last;
{
	if (type == CHAR_TYPE)
	{
		char	*lastPtr = (char *)last;
		if (lastPtr == NULL)
		{
			return((char *)data);
		}
		else
		{
			lastPtr++;
			return(lastPtr);
		}
	}
	if (type == TEXT_TYPE)
	{
		return(varcharGetNext(data));
	}

	/*
	** Should never get here
	*/
	return(NULL);
}


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


/* 
** regexStringLength -- determine the actual string length of mSQL data string.
** dptr -- character data to determine length.
** maxLen -- maximum possible length. (Shorter if a nul character occurs.)
** returns an integer that is the actual number of characters (not
**   including a nul, if there is one.
*/
int regexStringLength(dptr, maxLen )
	char 	*dptr;
	int 	maxLen;
{
	int 	len;

	len = 0;
	while ((maxLen > 0) && (*dptr != '\0')) 
	{
		len++;
		maxLen--;
		dptr++;
	}
	return len;
}


/* 
** likeTest -- (simple) ANSI SQL regular expression matcher.
** dptr -- character data to be matched.  May or may not be nul terminated.
** eptr -- regular expression to match. Must be nul terminated.
** dlen -- length of the data pointed to by dptr.  Determines data end when
**     it is not nul terminated.
** returns true (non-zero) or false (0) for match or no match, respectively.
*/

int likeTest(void* data, char *eptr, int dlen, char ignoreCase, char dataType)
{
	char 	eval, 
		*dptr;
	void	*cursor;

	eval = *eptr;
	dptr = getNextChar(dataType,data,NULL);
	while (eval != '\0') 
	{ 
		if (eval == '%') 
		{ 
			/*
			** ignore 0 or more characters
      			** try to find the next character that must match
      			** collapse special characters: %'s and _'s 
			*/
			eptr++;
			eval = *eptr;
			while (eval != '\0') 
			{
				if (eval == '%') 
				{ 
					/* any number of %'s same as one */
					eptr++;
					eval = *eptr;
					continue;
				}
				if (eval == '_') 
				{ 
					/* consume one char for each _ */
					if (dlen == 0) 
					{
	    					/* data ended : no match. */
	    					return 0;
	  				}
	  				eptr++;
	  				eval = *eptr;
					dptr = getNextChar(dataType,data,dptr);
	  				dlen--;
	  				continue;
				}
				break; 
			}
			/*
			** special case of ending with enough characters
			** for wild cards. 
			*/
      			if (eval == '\0') 
			{
				return 1;
      			}

			if (eval == '\\') 
			{
				/* 	
				** backslash escapes everything including 
				** itself, so skip it always. 
				*/
				eptr++;
				eval = *eptr;
				if (eval == '\0')
				{
					/*
					** end with match (skipping an 
					** ending backslash) 
					*/
	  				return 1;
				}
			}

			/*
	 		** At this point, we have a non-special character
			** to look for in data, then we recursively match
			** remainder of expression at points where that
			** character exists in the data.
			**
			** note: leave eval alone, but eptr positioned for
			** recursion. 
			*/
			eptr++;
			while (dlen != 0) 
			{
				while (dlen != 0) 
				{
					if (ignoreCase)
					{
						if(toupper(*dptr) == 
							toupper(eval))
						{
							break;
						}
					}
					else
					{
						if(*dptr == eval)
						{
							break;
						}
					}
					dptr = getNextChar(dataType,data,dptr);
					dlen--;
				}
				if (dlen == 0) 
				{ 
					/* there is no eval in data */
					return 0;
				}

				/*
				** we matched eval and need to check 
				** rest of exp and data 
				*/

				if (dataType == TEXT_TYPE)
				{
					cursor = varcharDupCursor(data);
					if (likeTest(cursor, eptr, dlen -1 ,
						ignoreCase, dataType)) 
					{
						/* match */
						free(cursor);
						return 1;
					} 
					dptr = getNextChar(dataType,data,dptr);
					dlen--;
				}
				else
				{
					dptr = getNextChar(dataType,data,dptr);
					dlen--;
					if (likeTest(dptr, eptr, dlen ,
						ignoreCase, dataType)) 
					{
						/* match */
						return 1;
					} 
				}

				/*
				** else just try to see if this position
				** is an eval 
				*/

			} /* end try to match rest */

			/* There is something after % and it didn't matched */
			return 0;
		} /* end if % */

		if (eval ==  '_') 
		{ 
			/* try to consume one char */
      			if (dlen == 0) 
			{
				/* data ended first: no match. */
				return 0;
			}

			eptr++;
			eval = *eptr;
			dptr++;
			dlen--;
			continue;
		} 

		if (eval == '\\')
		{
      			/* 
			** backslash escapes everything including self, so 
			** skip it always. 
			*/
			eptr++;
			eval = *eptr;
			if (eval == '\0') 
			{
				/*
				** if dlen == 0 then end with match 
				** (skipping an ending backslash) else 
				** expr ended before data
				*/
				return (dlen == 0);
			}
      			/* Fall through */
		}

		if (dlen == 0) 
		{
			/* data ends before expr */
			return 0;
		}

		if (ignoreCase)
		{
			if (toupper(eval) != toupper(*dptr))
				return(0);
		}
		else
		{
			if (eval != *dptr)
				return(0);
		}

		/* This character matched, consume it and continue */
		eptr++;
		eval = *eptr;
		dptr = getNextChar(dataType,data,dptr);
		dlen--;
	}

	/* if we matched everything, return true. */
	return (dlen == 0);
}



/* RNS
 * Non-ANSI, full-functioned regular expressions courtesy of
 * Henry Spencer.
 */

/*
 * RLIKE_DATA_MAXLEN -- the maximum length of a string that the
 * static data buffer can hold in order to use it rather than malloc.
 * 1024 is big enough for full pathnames on most UNIX boxes.
 */
#define RLIKE_DATA_MAXLEN 1024

/*
 * rLikeBuffer -- a static character buffer that should be faster
 * than using malloc.  If RLIKE_DATA_MAXLEN is too small, then
 * malloc is used.
 */
static char rLikeBuffer[RLIKE_DATA_MAXLEN + 1];

/*
 * RLIKE_NUM_REGEXPS -- the number of regular expression compilations
 * to cache (i.e., the RLIKE cache size).  RLIKE_NUM_REGEXPS must be
 * at least 1, but can be as large as needed.  Normally, a small number
 * like 5 is probably about right.  However, if you have an application
 * or set of applications that repeatedly use the same set of expressions,
 * then increasing the number to the size of the set of expressions (or
 * a bit larger) is the right thing to do.
 */
#define RLIKE_NUM_REGEXPS 5

/*
 * rLikePatternLengths -- an array in correspondence to rLikePatterns
 * that maintains the lengths of the cached expressions.
*/
static int rLikePatternLengths[RLIKE_NUM_REGEXPS];

/*
 * rLikePatterns -- an array in correspondence to the other rLike variables
 * that maintains the strings for the cached, compiled expressions.
*/
static char *rLikePatterns[RLIKE_NUM_REGEXPS];

/*
 * rLikeRegexps -- an array in correspondence to rLikePatterns
 * that maintains the compiled regular expressions.
*/
static regexp *rLikeRegexps[RLIKE_NUM_REGEXPS];

/*
 * regErrFlag -- regular expression error indicator
 */
static char regErrFlag;

/*
 * regerror -- function called by regular expression compiler and
 * executor in order to indicate that they have experienced errors.
 * In mSQL's case, simply incrementing regErrFlag is all that is needed.
 */
void regerror()
{
  regErrFlag++;
}

/*
 * rLikeTest -- use Henry Spencer's regular expressions to effect the
 * RLIKE comparison for a given data string, expression, and data string
 * length. It uses a simple cache to avoid compiling the expressions.
 * If msql*d is effectively repeating or alternating multiple expressions,
 * then this cache will be more effective than the stock 2.0B4 and
 * earlier versions.
 */
int rLikeTest(s,re,slen)
	char	*s,
		*re;
	int	slen;
{
  char *data; /* pointer to the string to match */
  int i;      /* loop index */
  int length; /* length of the current regular expression (re) */

  static char *lastRE = NULL; /*
			       * between uses, the previous expression used
			       * (copy of previous re)
			       */
  static regexp *reg = NULL;  /* between uses, the compilation of lastRE */
  int result;                 /* result to return from entire function */

  /* global initialization (done once; should be part of whole daemon init) */
  if (lastRE == NULL) {
    for( i = 0; i < RLIKE_NUM_REGEXPS; i++ ) {
      rLikePatternLengths[i] = -1;
      rLikePatterns[i] = NULL;
      rLikeRegexps[i] = NULL;
    }
  }

  /* prepare for cache search */
  lastRE = NULL;
  reg = NULL;
  length = strlen(re);

  for (i = 0; i < RLIKE_NUM_REGEXPS; i++) {

    if ((length == rLikePatternLengths[i])
	&& (strcmp(re, rLikePatterns[i]) == 0)) {

      /* We have a cache hit. */
      if (i != 0) {
	/*
	 * Move the matched pattern to the first slot in the
	 * cache and shift the other patterns down one position.
	 */
	int j;
	char *cachedString;

	cachedString = rLikePatterns[i];
	reg = rLikeRegexps[i];
	for (j = i-1; j >= 0; j--) {
	  rLikePatterns[j+1] = rLikePatterns[j];
	  rLikePatternLengths[j+1] = rLikePatternLengths[j];
	  rLikeRegexps[j+1] = rLikeRegexps[j];
	}
	rLikePatterns[0] = cachedString;
	rLikePatternLengths[0] = length;
	rLikeRegexps[0] = reg;
      }

      lastRE = rLikePatterns[0];
      reg = rLikeRegexps[0];
      break; /* we found it, so no need to check rest of cache. */
    }

  }

  /* initialize regular expression routines */
  regErrFlag = 0;

  /*
   * if needed, compile the regular expression, check for errors,
   * and add it to the cache
   */
  if (lastRE == NULL) {

    /* No match in the cache.  Compile the string and add it to the cache. */
    reg = regcomp(re);
    if (regErrFlag) {
      strcpy(errMsg, "RLIKE expression is malformed" );
      return(-1);
    }

    /* check to see if cache is full, free old one, if full. */
    if (rLikePatterns[RLIKE_NUM_REGEXPS-1] != NULL) {
	free(rLikePatterns[RLIKE_NUM_REGEXPS-1]);
	free(rLikeRegexps[RLIKE_NUM_REGEXPS-1]);
    }

    /* add to cache by shifting, then filling slot 0 */
    for (i = RLIKE_NUM_REGEXPS - 2; i >= 0; i--) {
      rLikePatterns[i+1] = rLikePatterns[i];
      rLikePatternLengths[i+1] = rLikePatternLengths[i];
      rLikeRegexps[i+1] = rLikeRegexps[i];
    }
    if ((rLikePatterns[0] = malloc(length+1)) == NULL) {
      strcpy(errMsg, "RLIKE failed to get pattern memory." );
      return(-1);
    }
    strcpy(rLikePatterns[0], re);
    rLikePatternLengths[0] = length;
    rLikeRegexps[0] = reg;
    lastRE = rLikePatterns[0];
  }

  /* RNS
   * As noted with the other LIKE operations, the data is
   * NOT guaranteed to be nul-terminated.  So, do that here.
   * We try to use a static buffer most of the time to avoid malloc/free.
   */
  if (slen > RLIKE_DATA_MAXLEN) {
    /* malloc: static buffer not big enough */
    if ((data = malloc(slen + 1)) == NULL) {
      strcpy(errMsg, "RLIKE nul-termination of data failed." );
      return(-1);
    }
  } else {
    /* use static buffer */
    data = &rLikeBuffer[0];
  }
  /* copy the data and terminate it */
  strncpy( data, s, slen );
  data[slen] = '\0';

  /* evaluate the expression for a match */
  result = regexec( reg, data );

  /* if necessary, free the copy of data */
  if (slen > RLIKE_DATA_MAXLEN) {
    free( data );
  }

  /* check for errors */
  if (regErrFlag) {
    strcpy(errMsg, BAD_LIKE_ERROR);
    return(-1);
  }

  return(result);
}

/* RNS
 * Data management for the SLIKE operator.
 */
/*
 * SLIKE_DATA_MAXLEN -- the maximum length of a string that the
 * static data buffer can hold in order to use it rather than malloc.
 * 1024 is big enough for full pathnames on most UNIX boxes.
 */
#define SLIKE_DATA_MAXLEN 1024

/*
 * sLikeBuffer -- a static character buffer that should be faster
 * than using malloc.  If SLIKE_DATA_MAXLEN is too small, then
 * malloc is used.
 */
static char sLikeBuffer[SLIKE_DATA_MAXLEN + 1];

#ifdef NOTDEF
/*
** This is a modification of the soundex algorithm. It returns a
** numeric value rather than a char string containing a char and a
** string of digits.  This allows the final comparison to be done 
** using an int compare rather than a string compare which will 
** help performance.  It also attempts to include vowel sounds.
**
** Bambi
*/

static int isvowel(c)
	char	c;
{
	c = tolower(c);
	if (c=='a'||c=='e'||c=='i'||c=='o'||c=='u')
		return(1);
	return(0);
}

#endif /* NOTDEF */

int soundex (word)
	char	*word;
{
  	int	result,
		sound,
		prevSound,
		numBits;

	prevSound = result = 0;
  	if (word == NULL || *word == 0)
    		return(0);

	result = toupper(*word) - 'A';
	numBits = 5;

  	while(*word)
   	{
		sound = 0;
		switch (toupper(*word))
		{
			/* Replace Labials with "1" */
			case 'B':
	  		case 'F':
	  		case 'P':
	  		case 'V': 
				if (prevSound != 1)
					sound = 1;
		    		break;

	  		/* Replace Gutterals & sibilants with "2" */
	  		case 'C':
	  		case 'G':
	  		case 'J':
	  		case 'K':
	  		case 'Q':
	  		case 'S':
	  		case 'X':
	  		case 'Z': 
				if (prevSound != 2)
					sound = 2;
		    		break;

	  		/* Replace Dentals with "3" */
	  		case 'D':
	  		case 'T': 
				if (prevSound != 3)
					sound = 3;
		    		break;

	  		/* Replace Longliquids with "4" */
	  		case 'L': 
				if (prevSound != 4)
					sound = 4;
		     		break;

	  		/* Replace Nasals with "5" */
	  		case 'M':
	  		case 'N': 
				if (prevSound != 5)
					sound = 5;
		    		break;

	  		/* Replace Shortliquids with "6" */
	  		case 'R': 
				if (prevSound != 6)
					sound = 6;
		    		break;

#ifdef NOTDEF
			/* Vowel sounds */
			case 'A':
				if (*(word+1)=='A' || *(word+1)=='E')
					sound = 7;
				if (!isvowel(*(word+1)))
					sound = 7;
				if (sound == 0)
					break;
				if (prevSound == 7)
					sound = 0;
				break;

			case 'E':
				if (*(word+1)=='E' || *(word+1)=='I')
					sound = 8;
				if (sound == 0)
					break;
				if (prevSound == 8)
					sound = 0;
				break;

			case 'I':
				if (*(word+1)=='E')
					sound = 8;
				if (sound == 0)
					break;
				if (prevSound == 8)
					sound = 0;
				break;

			case 'O':
				if (*(word+1)=='O')
					sound = 9;
				if (sound == 0)
					break;
				if (prevSound == 9)
					sound = 0;
				break;

			case 'U':
				if (*(word+1)=='E' || !isvowel(*(word+1)))
					sound = 9;
				if (sound == 0)
					break;
				if (prevSound == 9)
					sound = 0;
				break;
#endif
				
		}  

		if (sound != 0)
		{
			prevSound = sound;
			if (numBits < 29)
			{
				result = result << 3;
				result += sound;
				numBits += 3;
			}
			if (numBits >= 30)
				break;
		}
		word++;
   	}  
	return(result);
}


int sLikeTest(str1, str2, str1len)
     char *str1;
     char *str2;
     int str1len;
{
  char *data;
  int sound1;
  int sound2;

   /* RNS
    * At least as of 2.0 B6, it is still possible that data for the
    * first argument will be non-nul terminated as in rLikeTest.
    * We add a similar nul-terminating mechanism here.
    * We try to use a static buffer most of the time to avoid malloc/free.
    */
  if (str1len > SLIKE_DATA_MAXLEN) {
    /* malloc: static buffer not big enough */
    if ((data = malloc(str1len + 1)) == NULL) {
      strcpy(errMsg, "SLIKE nul-termination of data failed." );
      return(-1);
    }
  } else {
    /* use static buffer */
    data = &sLikeBuffer[0];
  }
  /* copy the data and terminate it */
  strncpy( data, str1, str1len );
  data[str1len] = '\0';

  /* evaluate the expression for a match */
  sound1 = soundex(data);
  sound2 = soundex(str2);

  /* if necessary, free the copy of data */
  if (str1len > SLIKE_DATA_MAXLEN) {
    free( data );
  }

  return( (sound1 == sound2) );
}


