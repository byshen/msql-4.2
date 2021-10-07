/*
** Lexer Header File	(Private)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MSQLD_LEX_PRIV_H

#define MSQLD_LEX_PRIV_H 1

#if defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif

#ifdef __cplusplus
extern "C" {
#endif



/***********************************************************************
** Macro Definitions
*/

#define	REG		register
#define NUM_HASH	32

/*
** Macros for handling the scanner's internal pointers
*/
#define yyGet()		(*tokPtr++); yytoklen++
#define yyUnget()	tokPtr--; yytoklen--
#define yySkip()	(*tokPtr++); tokStart++
#define yyRevert()	{tokPtr=tokStart; yytoklen=0;}
#define yyReturn2(t)	{tokStart=tokPtr; yyrep=memMallocToken(yytext,yytoklen);return(t);}
#define yyReturn(t)	{tokStart=tokPtr; return(t);}


/*
** Macros for matching character classes.  These are in addition to
** those provided in <ctypes.h>
*/
#ifdef	iswhite
# undef iswhite
#endif
#define iswhite(c)	(c == ' ' || c == '\t' || c == '\n')

#ifdef	iscompop
# undef iscompop
#endif
#define iscompop(c)	(c == '<' || c == '>' || c == '=')


/*
** Debugging macros.
*/

/* Define this to watch the state transitions */
/* #define DEBUG_STATE	*/

#ifdef DEBUG
#  define token(x)	(int) x
#else
#  define token(x)	x
#endif /* DEBUG */

#ifdef DEBUG_STATE
#  define CASE(x)	case x: if (x) printf("%c -> state %d\n",c,x); \
				else printf("Scanner starting at state 0\n");
#else
#  define CASE(x)	case x:
#endif

/***********************************************************************
** Type Definitions
*/

typedef struct symtab_s {
	char	*name;
	int	tok;
} symtab_t;



/***********************************************************************
** Function Prototypes
*/

void yyError __ANSI_PROTO((char*));
void yyInitScanner __ANSI_PROTO((u_char*));

int yyLex __ANSI_PROTO(());


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

