/*
** Util Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_UTIL_H

#define MAIN_UTIL_H 1


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

void utilFreeRow (row_t *);
void utilQualifyConds (mQuery_t*);
void utilQualifyOrder (mQuery_t*);
void utilFormatPacket (char*,mField_t*);
void utilFormatExport (char*,mField_t*,char);
void utilQualifyFields (mQuery_t*);
void utilExpandTableFields (msqld*, char*,mQuery_t*);

int utilCheckDB (msqld*,char*);
int utilSetupConds (cache_t*, mCond_t *);
int utilSetupOrder (cache_t*, int*, mOrder_t*);
int utilSetupFields (cache_t*, int*, mField_t*);
int utilSetCondValueType (mCond_t*,mVal_t*);
int utilSetFieldInfo (cache_t*, mField_t*);

mField_t *utilDupFieldList (cache_t*);
mField_t *utilExpandFieldWildCards (cache_t*,mField_t*);

row_t *utilDupRow (cache_t*,row_t*,row_t*);

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

