/*
** table Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_TABLE_H

#define MAIN_TABLE_H 1

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

#define	TABLE_DIR_HASH	16


/***********************************************************************
** Type Definitions
*/


/***********************************************************************
** Function Prototypes
*/

int tableOpenTable __ANSI_PROTO((msqld*, cache_t*, char*, char*));
int tableOpenOverflow __ANSI_PROTO((msqld*,cache_t*,char*, char*));
int tableWriteFreeList __ANSI_PROTO((cache_t*, u_int, u_int));
int tableInitTable __ANSI_PROTO((cache_t*, int));
int tablePushBlankPos __ANSI_PROTO((cache_t*, char*, char*, u_int));
int tableWriteRow __ANSI_PROTO((cache_t*, row_t*, u_int, mQuery_t*));
int tablePlaceRow __ANSI_PROTO((cache_t*, row_t*, u_int, mQuery_t*));
int tableDeleteRow __ANSI_PROTO((cache_t*, u_int));
int tableReadRow __ANSI_PROTO((cache_t*, row_t*, u_int));
int tableFillRow __ANSI_PROTO((cache_t*, row_t*, mField_t*,int[]));
int tableUpdateValues __ANSI_PROTO((cache_t*, row_t*, mField_t*,int[]));
int tableCopyFile __ANSI_PROTO((char*, char*));
int tableCopyDirectory __ANSI_PROTO((char*, char*));
int tableCheckTargetDefinition __ANSI_PROTO((mQuery_t *));
int tableCreateDefinition __ANSI_PROTO((msqld*,char*, mQuery_t*));
int tableDirHash(char *);
int tableCheckFileLayout(msqld*, char*, char*);

void tableFreeTableDef __ANSI_PROTO((mField_t *));
void tableFreeTmpTable __ANSI_PROTO((msqld*,cache_t *));
void tableFreeDefinition __ANSI_PROTO((mField_t *));
void tableInvalidateCache __ANSI_PROTO(());
void tableInvalidateCacheEntry __ANSI_PROTO((cache_t *));
void tableInvalidateCachedTable __ANSI_PROTO((char *, char *));
void tableInvalidateCachedDatabase __ANSI_PROTO((char *));
void tableExtractValues __ANSI_PROTO((cache_t *, row_t*,mField_t*,int*,mQuery_t*));
void tableCleanTmpDir __ANSI_PROTO((msqld*));

u_int tableReadFreeList __ANSI_PROTO((cache_t*, u_int));
u_int tablePopBlankPos __ANSI_PROTO((cache_t*, char*, char*));

cache_t *tableCreateTmpTable __ANSI_PROTO((msqld*,char*,char*,cache_t*,cache_t*,mField_t*,int));
cache_t *tableCreateDestTable __ANSI_PROTO((msqld*, char*,char*,cache_t*,mField_t*));
cache_t *tableLoadDefinition __ANSI_PROTO((msqld*,char*,char*,char*));


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

