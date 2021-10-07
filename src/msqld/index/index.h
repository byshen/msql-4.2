/*
** index Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef INDEX_H

#define INDEX_H 1

#if defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif

#ifdef __cplusplus
extern "C" {
#endif


#include "avl_tree.h"
#include "cpi.h"

/***********************************************************************
** Macro Definitions
*/

#define IDX_UNIQUE	1
#define IDX_DUP		2

#define IDX_BYTE	1
#define IDX_CHAR	2
#define IDX_INT32	3
#define IDX_REAL	4
#define IDX_UINT32	5
#define IDX_INT64	6
#define IDX_UINT64	7
#define IDX_INT8	8
#define IDX_UINT8	9
#define IDX_INT16	10	
#define IDX_UINT16	11

#define IDX_EXACT	1
#define IDX_CLOSEST	2

#define IDX_OK		0
#define IDX_DUP_ERR	-1
#define IDX_NOT_FOUND	-2
#define IDX_BAD_TYPE	-3
#define IDX_FILE_ERR	-4
#define IDX_UNKNOWN	-5

#define	IDX_AVL		1
#define	IDX_CPI		2
#define	IDX_MEM_AVL	3



/***********************************************************************
** Type Definitions
*/

typedef struct {		/* Index Node */
	char	*key;
	off_t	data;
	void	*native;
	cpi_nod	cpiNode;
} idx_nod;


typedef	struct {		/* Index handle */
	char	idxType,
		dataType,
		path[255];
	void	*native;
} idx_hnd;


typedef struct {		/* Index cursor */
	avl_cur	avlCur;
	cpi_cur	cpiCur;
} idx_cur;


typedef struct {		/* Index environment */
        u_int   cacheSize,
                pageSize;
} idx_env;



/***********************************************************************
** Function Prototypes
*/

int idxCreate __ANSI_PROTO((char *,int, int, int, int, int, idx_env *));
int idxClose __ANSI_PROTO((idx_hnd *));
int idxSync __ANSI_PROTO((idx_hnd *));
int idxOpen __ANSI_PROTO((char *, int, idx_env*, idx_hnd*));
int idxInsert __ANSI_PROTO((idx_hnd *, char *, int, off_t));
int idxDelete __ANSI_PROTO((idx_hnd *, char *, int, off_t));
int idxLookup __ANSI_PROTO((idx_hnd *,char *,int,int,idx_nod*));
int idxSetCursor __ANSI_PROTO((idx_hnd *, idx_cur *));
int idxCloseCursor __ANSI_PROTO((idx_hnd *, idx_cur *));
int idxGetFirst __ANSI_PROTO((idx_hnd *, idx_nod *));
int idxGetLast __ANSI_PROTO((idx_hnd *, idx_nod *));
int idxGetNext __ANSI_PROTO((idx_hnd *, idx_cur *, idx_nod *));
int idxGetPrev __ANSI_PROTO((idx_hnd *, idx_cur *, idx_nod *));
int idxTestIndex __ANSI_PROTO((idx_hnd *));
int idxExists __ANSI_PROTO((idx_hnd *, char *, int, off_t));
void idxPrintIndexStats __ANSI_PROTO((idx_hnd *));
void idxDumpIndex __ANSI_PROTO((idx_hnd *));
char *idxGetIndexType __ANSI_PROTO((idx_hnd *));
u_int idxGetNumEntries __ANSI_PROTO((idx_hnd *));
u_int idxGetNumKeys __ANSI_PROTO((idx_hnd *));

int idxCompareValues __ANSI_PROTO((int,char*,char*,int));
int idxByteCompare __ANSI_PROTO((char*,char*,int));
int idxCharCompare __ANSI_PROTO((char*,char*));
int idxRealCompare __ANSI_PROTO((char*,char*));
int idxUIntCompare __ANSI_PROTO((char*,char*));
int idxIntCompare __ANSI_PROTO((char*,char*));

int idxInt8Compare __ANSI_PROTO((char*,char*));
int idxInt16Compare __ANSI_PROTO((char*,char*));
int idxInt32Compare __ANSI_PROTO((char*,char*));
int idxInt64Compare __ANSI_PROTO((char*,char*));
int idxUInt8Compare __ANSI_PROTO((char*,char*));
int idxUInt16Compare __ANSI_PROTO((char*,char*));
int idxUInt32Compare __ANSI_PROTO((char*,char*));
int idxUInt64Compare __ANSI_PROTO((char*,char*));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */
