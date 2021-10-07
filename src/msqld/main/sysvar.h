/*
** sysvar Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_SYSVAR_H

#define MAIN_SYSVAR_H 1

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

void sysvarResetVariables __ANSI_PROTO(());
void sysvarGetVariable __ANSI_PROTO((cache_t*, row_t*, mField_t*,mQuery_t*));
mField_t *sysvarGetDefinition __ANSI_PROTO((mField_t*));
int sysvarCompare __ANSI_PROTO((cache_t*, row_t*,mCond_t*, mVal_t*));
int sysvarCheckVariable __ANSI_PROTO((cache_t*, mField_t*));
int sysvarCheckCondition __ANSI_PROTO((mCond_t*));
int sysvarGetVariableType __ANSI_PROTO((char*));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

