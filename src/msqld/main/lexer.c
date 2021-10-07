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
** $Id: lexer.c,v 1.18 2012/01/15 06:19:59 bambi Exp $
**
*/

/*
** Module	: main : lex
** Purpose	: 
** Exports	: 
** Depends Upon	: 
*/


/*
** This is a hand crafted scanner that looks and smells like a lex
** generated scanner.  I've kept the same interface so that unmodified
** yacc parsers can run with this.
**
** This scanner uses a state machine to translate the input data into
** tokens.  Failed matches cause a fallback to the start of the token
** scan and a possible transition to a known alternate state.  The
** state structure is defined in doc/scanner.doc
**
** NOTE : Because the scanner must revert back to the start of the
**	token on a failure it can only work from an input buffer. It
**	cannot work from a file or anything else.
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
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include <common/portability.h>


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#ifdef YYSTYPE
#  undef YYSTYPE
#endif
typedef char    * C_PTR;
#define YYSTYPE C_PTR

#include <ctype.h>
#include <common/msql_defs.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/lexer.h>
#include <msqld/main/net.h>
#include <msqld/main/memory.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

u_char	*yytext 	= NULL,
	*yyprev		= NULL,
	*yyrep		= NULL;
u_int	yytoklen	= 0;


static	u_char 		*tokPtr,
			*tokStart;
static	int		state,
			yylineno=1;



#ifdef DEBUG
	YYSTYPE		yylval;
	mQuery_t 	fakeQuery,
			*curQuery=&fakeQuery;
#else
	extern YYSTYPE	yylval;
#endif



symtab_t symtab[NUM_HASH][16] = {
	{ /* 0 */
		{ "insert",	token(MSQL_INSERT)},
		{ "into",	token(INTO)},
		{ "index",	token(INDEX)},
		{ "int",	token(MSQL_INT)},
		{ "int4",	token(MSQL_INT)},
		{ "int8",	token(MSQL_INT8)},
		{ "int16",	token(MSQL_INT16)},
		{ "int32",	token(MSQL_INT)},
		{ "int64",	token(MSQL_INT64)},
		{ "integer",	token(MSQL_INT)},
		{ 0,		0}
	},
	{ /* 1 */
		{ "like",	token(LIKE)},
		{ "limit",	token(LIMIT)},
		{ 0,		0}
	},
	{ /* 2 */
		{ "ipv4",	token(MSQL_IPV4)},
		{ "ipv6",	token(MSQL_IPV6)},
		{ "ipaddr",	token(MSQL_IPV4)},
		{ 0,		0}
	},
	{ /* 3 */
		{ "min",	token(SET_FUNCT)},
		{ "millitime",	token(MSQL_MILLITIME)},
		{ "millidatetime",	token(MSQL_MILLIDATETIME)},
		{ 0,		0}
	},
	{ /* 4 */
		{ "offset",	token(OFFSET)},
		{ 0,		0}
	},
	{ /* 5 */
		{ 0,		0}
	},
	{ /* 6 */
		{ 0,		0}
	},
	{ /* 7 */
		{ 0,		0}
	},
	{ /* 8 */
		{ 0,		0}
	},
	{ /* 9 */
		{ "between",	token(BETWEEN)},
		{ "real",	token(MSQL_REAL)},
		{ "table",	token(TABLE)},
		{ "money",	token(MSQL_MONEY)},
		{ "date",	token(MSQL_DATE)},
		{ "datetime",	token(MSQL_DATETIME)},
		{ 0,		0}
	},
	{ /* 10 */
		{ 0,		0}
	},
	{ /* 11 */
		{ "not",	token(NOT)},
		{ "select",	token(MSQL_SELECT)},
		{ "set",	token(SET)},
		{ "sequence",	token(SEQUENCE)},
		{ 0,		0}
	},
	{ /* 12 */
		{ "on",        	token(ON)},
		{ 0,		0}
	},
	{ /* 13 */
		{ "value",      token(VALUE)},
		{ "delete",	token(MSQL_DELETE)},
		{ "values",	token(VALUES)},
		{ "bigint",  	token(MSQL_INT)},
		{ "text",	token(MSQL_TEXT)},
		{ "desc",	token(DESC)},
		{ 0,		0}
	},
	{ /* 14 */
		{ "char",	token(MSQL_CHAR)},
		{ "all",	token(ALL)},
		{ "character",  token(MSQL_CHAR)},
		{ 0,		0}
	},
	{ /* 15 */
		{ "cidr4",	token(MSQL_CIDR4)},
		{ "cidr6",	token(MSQL_CIDR6)},
		{ 0,		0}
	},
	{ /* 16 */
		{ "or",		token(OR)},
		{ "and",	token(AND)},
		{ "order",	token(ORDER)},
		{ "rlike",	token(RLIKE)},
		{ 0,		0}
	},
	{ /* 17 */
		{ "distinct",	token(DISTINCT)},
		{ "null",	token(NULLSYM)},
		{ "time",	token(MSQL_TIME)},
		{ "tinyint",	token(MSQL_INT)},
		{ 0,		0}
	},
	{ /* 18 */
		{ "primary",	token(PRIMARY)},
		{ "clike",	token(CLIKE)},
		{ "slike",	token(SLIKE)},
		{ 0,		0}
	},
	{ /* 19 */
		{ "uint",	token(MSQL_UINT)},
		{ "uint8",	token(MSQL_UINT8)},
		{ "uint16",	token(MSQL_UINT16)},
		{ "uint32",	token(MSQL_UINT)},
		{ "uint64",	token(MSQL_UINT64)},
		{ "smallint",	token(MSQL_INT)},
		{ 0,		0}
	},
	{ /* 20 */
		{ 0,		0}
	},
	{ /* 21 */
		{ "<=",		token(LE)},
		{ "as",		token(AS)},
		{ "asc",	token(ASC)},
		{ "count",	token(SET_FUNCT)},
		{ 0,		0}
	},
	{ /* 22 */
		{ "where",	token(WHERE)},
		{ "<>",         token(NE)},
		{ "cpi",	token(CPI_INDEX)},
		{ 0,		0}
	},
	{ /* 23 */
		{ "double",	token(MSQL_REAL)},
		{ 0,		0}
	},
	{ /* 24 */
		{ "<",		token(LT)},
		{ "create",	token(MSQL_CREATE)},
		{ "unique",	token(UNIQUE)},
		{ "float",	token(MSQL_REAL)},
		{ "float8",	token(MSQL_REAL)},
		{ "avl",	token(AVL_INDEX)},
		{ "avg",	token(SET_FUNCT)},
		{ 0,		0}
	},
	{ /* 25 */
		{ ">=",		token(GE)},
		{ 0,		0}
	},
	{ /* 26 */
		{ "=",          token(EQ)},
		{ "update",	token(MSQL_UPDATE)},
		{ "drop",	token(MSQL_DROP)},
		{ "step",	token(STEP)},
		{ 0,		0}
	},
	{ /* 27 */
		{ "sum",	token(SET_FUNCT)},
		{ "max",	token(SET_FUNCT)},
		{ "key",	token(KEY)},
		{ 0,		0}
	},
	{ /* 28 */
		{ ">",          token(GT)},
		{ 0,		0}
	},
	{ /* 29 */
		{ "by",		token(BY)},
		{ 0,		0}
	},
	{ /* 30 */
		{ "from",	token(FROM)},
		{ 0,		0}
	},
	{ /* 31 */
		{ 0,		0}
	}
};


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/



static int _findKeyword(tok,len)
	char	*tok;
	int	len;
{
	REG	char	*cp1,
			*cp2,
			tmp;
	REG	symtab_t *stab;
	int	found;
        REG 	int     hash=0;


        cp1 = tok;
	/*
        while(*cp1 && index++ < len)
        {
                hash += *cp1++;
        }
	*/

	if (len == 1)
	{
		hash = *(cp1) * 2;
	}
	else
	{
		hash = (*(cp1) * 2) + *(cp1+1);
	}
        hash = hash & (NUM_HASH - 1);

	stab = symtab[hash];
	while(stab->name)
	{
		cp1 = stab->name;
		cp2 = tok;
		found = 1;
		while(cp2 - tok < len)
		{
			if (!(*cp1))
			{
				found = 0;
				break;
			}
			tmp = *cp2++;
			if (tmp >64 && tmp<91)
				tmp+=32;
			if (tmp != *cp1++)
			{
				found = 0;
				break;
			}
		}
		if (*cp1)
		{
			found = 0;
		}
		if (found)
		{
			yytext = (u_char *)stab->name;
			yylval = (YYSTYPE)stab->name;
			return(stab->tok);
		}
		stab++;
	}
	return(0);
}



static u_char *_tokenDup(tok,len)
	u_char	*tok;
	int	len;
{
	u_char	*new;

	new = (u_char *)malloc(len+1);
	bcopy(tok,new,len);
	*(new + len) = 0;
	return(new);
}


static u_char *_readTextLiteral(tok)
	u_char	*tok;
{
	REG 	u_char c;
	int	bail;
	u_char	*new;

	bail = 0;
	while(!bail)
	{
		c = yyGet();
		switch(c)
		{
			case 0:
				return(NULL);

			case '\\':
				c = yyGet();
				if (!c)
					return(NULL);
				break;
	
			case '\'':
				bail=1;
				break;
		}
	}
	if (yytoklen < NAME_LEN)
		new = (u_char*)memMallocToken((char*)tok,yytoklen);
	else
		new = _tokenDup(tok,yytoklen);
	return(new);
}



/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


void yyInitScanner(buf)
	u_char	*buf;
{
	tokStart = buf;
	state = 0;
	yylineno = 1;
}




int yylex()
{
	REG	u_char	c;
	REG	u_char	t;
	int	tokval;
	static	u_char dummyBuf[2];
	static	char EOI[] = "*end of input*";


	/*
	** Handle the end of input.  We return an EOI token when we hit
	** the end and then return a 0 on the next call to yylex.  This
	** allows the parser to do the right thing with trailing garbage
	** in the expression.
	*/
	if (yyprev)
		free(yyprev);
	yyprev = yyrep;
	if (state == 1000)
	{
		yyprev = NULL;
		return(0);
	}
	state = 0;

	/*
	** Dive into the state machine
	*/
	while(1)
	{
		switch(state)
		{
			/* State 0 : Start of token */
			CASE(0)
				tokPtr = tokStart;
				yytext = NULL;
				yytoklen = 0;
				c = yyGet();
				while (iswhite(c))
				{
					if (c == '\n')
						yylineno++;
					c = yySkip();
					yytoklen = 1;
				}
				if (c == '\'')
				{
					state = 12;
					break;
				}
				if (c == '_')
				{
					state = 18;
					break;
				}
				if (isalpha(c))
				{
					state = 1;
					break;
				}
				if (isdigit(c))
				{
					state = 5;
					break;
				}
				if (c == '.')
				{
					t = yyGet();
					if ( isdigit(t) ) 
					{
						yyUnget();
						state = 7;
						break;
					} 
					else
						yyUnget();
				}
				if (c == '-' || c == '+')
				{
					state = 9;
					break;
				}
				if (iscompop(c))
				{
					state = 10;
					break;
				}
				if (c == '#')
				{
					state = 14;
					break;
				}
				if (c == 0)
				{
					state = 1000;
					break;
				}
				state = 999;
				break;

			/* State 1 : Incomplete keyword or ident */
			CASE(1)
				c = yyGet();
				if (isalpha(c) || isdigit(c))
				{
					state = 1;
					break;
				}
				if (c == '_')
				{
					state = 3;
					break;
				}
				state = 2;
				break;


			/* State 2 : Complete keyword or ident */
			CASE(2)
				yyUnget();
				tokval = _findKeyword((char*)tokStart,yytoklen);
				if (tokval)
				{
					yyReturn(tokval);
				}
				else
				{
					yytext = (u_char*)memMallocToken(
						(char*)tokStart, yytoklen);
					yylval = (YYSTYPE) yytext;
					yyReturn(token(IDENT));
				}
				break;


			/* State 3 : Incomplete ident */
			CASE(3)
				c = yyGet();
				if (isalnum(c) || c == '_')
				{
					state = 3;
					break;
				}
				state = 4;
				break;


			/* State 4: Complete ident */
			CASE(4)
				yyUnget();
				yytext = (u_char*)memMallocToken(
					(char*)tokStart,yytoklen);
				yylval = (YYSTYPE) yytext;
				yyReturn(token(IDENT));


			/* State 5: Incomplete real or int number */
			CASE(5)
				c = yyGet();
				if (isdigit(c))
				{
					state = 5;
					break;
				}
				if (c == '.')
				{
					state = 7;
					break;
				}
				state = 6;
				break;


			/* State 6: Complete integer number */
			CASE(6)
				yyUnget();
				yytext = (u_char*)memMallocToken(
					(char*)tokStart,yytoklen);
				yylval = (YYSTYPE) yytext;
				yyReturn(token(NUM));
				break;


			/* State 7: Incomplete real number */
			CASE(7)
				c = yyGet();

		                /* Analogy Start */
                                if(c == 'e' || c == 'E')
                                {
                                        state = 15;
                                        break;
                                }
                		/* Analogy End   */
 
				if (isdigit(c))
				{
					state = 7;
					break;
				}
				state = 8;
				break;


			/* State 8: Complete real number */
			CASE(8)
				yyUnget();
				yytext = (u_char*)memMallocToken(
					(char*)tokStart,yytoklen);
				yylval = (YYSTYPE) yytext;
				yyReturn(token(REAL_NUM));


			/* State 9: Incomplete signed number */
			CASE(9)
				c = yyGet();
				if (isdigit(c))
				{
					state = 5;
					break;
				}
				if (c == '.')
				{
					state = 7;
					break;
				}
				state = 999;
				break;


			/* State 10: Incomplete comparison operator */
			CASE(10)
				c = yyGet();
				if (iscompop(c))
				{
					state = 10;
					break;
				}
				state = 11;
				break;


			/* State 11: Complete comparison operator */
			CASE(11)
				yyUnget();
				tokval = _findKeyword((char*)tokStart,yytoklen);
				if (tokval)
				{
					yyReturn(tokval);
				}
				state = 999;
				break;

	
			/* State 12: Incomplete text string */
			CASE(12)
				yytext = _readTextLiteral(tokStart);
				yylval = (YYSTYPE) yytext;
				if (yytext)
				{
					state = 13;
					break;
				}
				state = 999;
				break;



			/* State 13: Complete text string */
			CASE(13)
				yyReturn(token(MSQL_TEXT));
				break;


			/* State 14: Comment */
			CASE(14)
				c = yySkip();
				if (c == '\n')
				{
					state = 0;
				}
				else
				{
					state = 14;
				}
				break;

			/* Analogy Start */
                        /* State 15: Exponent Sign in Scientific Notation */
                        CASE(15)
                                c = yyGet();
                                if(c == '-' || c == '+')
                                {
                                      state = 16;
                                      break;
                                }
                                state = 999;
                                break;

                        /* State 16: Exponent Value-first digit in Scientific 
			** Notation */
                        CASE(16)
                                c = yyGet();
                                if (isdigit(c))
                                {
                                        state = 17;
                                        break;
                                }
                                state = 999;  	/* if no digit, then token 
						** is unknown */
                                break;

                        /* State 17: Exponent Value in Scientific Notation */
                        CASE(17)
                                c = yyGet();
                                if (isdigit(c))
                                {
                                        state = 17;
                                        break;
                                }
                                state = 8;     	/* At least 1 exponent 
						** digit was required */
                                break;
			/* Analogy End */


			/* State 18 : Incomplete System Variable */
			CASE(18)
				c = yyGet();
				if (isalnum(c) || c == '_')
				{
					state = 18;
					break;
				}
				state = 19;
				break;


			/* State 19: Complete Sys Var */
			CASE(19)
				yyUnget();
				yytext = (u_char*)memMallocToken(
					(char*)tokStart,yytoklen);
				yylval = (YYSTYPE) yytext;
				yyReturn(token(SYS_VAR));
				

			/* State 999 : Unknown token.  Revert to single char */
			CASE(999)
				yyRevert();
				c = yyGet();
				*dummyBuf = c;
				*(dummyBuf+1) = 0;
				yytext = dummyBuf;
				yylval = (YYSTYPE) yytext;
				yyReturn(token(yytext[0]));


			/* State 1000 : End Of Input */
			CASE(1000)
				yytext = (u_char*)EOI;
				yylval = (YYSTYPE) EOI;
				yyReturn(token(END_OF_INPUT));

		}
	}
}


void yyerror(msg)
	char	*msg;
{
	static char	buf[160];
	extern	mQuery_t *curQuery;

	if (yytext && yylineno)
	{
		snprintf(buf, 160, "%s. Line %d near '%s'",
			msg, yylineno, yytext);
		netError(curQuery->clientSock, buf);
	}
	else
	{
		netError(curQuery->clientSock, msg);
	}
}



#ifdef DEBUG

#undef malloc
#undef free

char *Malloc(size,file,line)
	int	size;
	char	*file;
	int	line;
{
	return((char *)malloc(size));
}


void Free(ptr,file,line)
	char	*ptr;
	char	*file;
	int	line;
{
	free(ptr);
}


void netError(sock, msg)
	int	sock;
	char	*msg;
{
	printf("%s\n",msg);
}


main()
{
	char	*p,
		tmpBuf[10 * 1024];

	bzero(tmpBuf,sizeof(tmpBuf));
	read(fileno(stdin),tmpBuf,sizeof(tmpBuf));
	yyInitScanner(tmpBuf);
	while(p = (char *) yylex())
	{
		/*
		** printf("%-15.15s of length %u is \"%s\"\n", p, yytoklen,
		**	yytext?yytext:(u_char *)"(null)");
		*/
		printf("%-5d of length %u is \"%s\"\n", p, yytoklen,
			yytext?yytext:(u_char *)"(null)");
	}
}

#endif
