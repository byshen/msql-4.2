/*
** cache Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_CACHE_H

#define MAIN_CACHE_H 1

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

void cacheDropTableCache __ANSI_PROTO(());
void cacheInvalidateEntry __ANSI_PROTO((cache_t*));
void cacheInvalidateTable __ANSI_PROTO((msqld*,char*,char*));
void cacheInvalidateDatabase __ANSI_PROTO((msqld*, char*));
void cacheInvalidateCache __ANSI_PROTO((msqld*));
void cacheSetupTableCache __ANSI_PROTO(());
void cacheSyncCache __ANSI_PROTO(());


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

