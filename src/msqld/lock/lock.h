/*
** lock Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef LOCK_H

#define LOCK_H 1

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

#define MSQL_WR_LOCK    1
#define MSQL_RD_LOCK    2
#define MSQL_UNLOCK     3



/***********************************************************************
** Type Definitions
*/


/***********************************************************************
** Function Prototypes
*/

void lockGetFileLock __ANSI_PROTO((msqld*,int,int));
void lockGetIpcLock __ANSI_PROTO((msqld*));
void lockReleaseIpcLock __ANSI_PROTO((msqld*));
int lockNonBlockingLock __ANSI_PROTO((int));


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

