/*
** select Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_SELECT_H

#define MAIN_SELECT_H 1

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


/***********************************************************************
** Type Definitions
*/


/***********************************************************************
** Function Prototypes
*/

int selectProcessSelect __ANSI_PROTO((msqld*, mQuery_t*));
int doSelect(cache_t*, mQuery_t*, int, int, cache_t*);


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

