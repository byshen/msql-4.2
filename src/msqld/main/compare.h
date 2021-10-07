/*
** compare Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_COMPARE_H

#define MAIN_COMPARE_H 1

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

int compareMatchRow (cache_t*, row_t*, mCond_t*, mQuery_t*);
int compareRows (cache_t*, row_t*, row_t*,mOrder_t*,int*);
int localByteCmp (void*, void*, int);
int checkDupRow(cache_t *, u_char*, u_char*);


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

