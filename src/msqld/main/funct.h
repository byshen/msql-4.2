/*
** funct Header File	
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MSQLD_FUNCT_H

#define MSQLD_FUNCT_H 1

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

#define	FUNCT_VALUE	1
#define	FUNCT_AGGREGATE	2

#define FUNCT_MAX_PARAM	5


/***********************************************************************
** Type Definitions
*/

typedef struct {
	char	*name;
	int	(*functPtr) ();
	int	type,
		returnType,
		numParams,
		paramTypes[FUNCT_MAX_PARAM + 1];
} mFunct_t;
	
	


/***********************************************************************
** Function Prototypes
*/

void functProcessFunctions __ANSI_PROTO((cache_t*,mQuery_t *));
int functFindFunction __ANSI_PROTO((mQuery_t *));
int functCheckFunctions __ANSI_PROTO((mQuery_t *));


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

