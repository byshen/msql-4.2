/*
** index Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_INDEX_H

#define MAIN_INDEX_H 1

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

void indexCloseIndices __ANSI_PROTO((cache_t*));
mIndex_t *indexLoadIndices __ANSI_PROTO((msqld*,cache_t*,char *, char *));
int indexInsertIndexValue __ANSI_PROTO((cache_t*,mField_t*,u_int,mIndex_t*));
int indexInsertIndices __ANSI_PROTO((cache_t*,mField_t*,u_int));
int indexDeleteIndices __ANSI_PROTO((cache_t*,mField_t*,u_int));
int indexCheckIndex __ANSI_PROTO((cache_t*,mField_t*,mField_t*,mIndex_t*,u_int));
int indexCheckIndices __ANSI_PROTO((cache_t*,mField_t*,mField_t*,u_int));
int indexCheckNullFields __ANSI_PROTO((cache_t*,row_t*,mIndex_t*,int*));
int indexCheckAllForNullFields __ANSI_PROTO((cache_t*,row_t*,int*));
int indexUpdateIndices __ANSI_PROTO((cache_t*,mField_t*,u_int,row_t*,int*,int*,int,mQuery_t*));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

