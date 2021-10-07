/*
** CRA Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef CRA_H

#define CRA_H 1

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

#define CAND_SEQ        	1
#define CAND_IDX_ABS    	2
#define CAND_IDX_RANGE		3
#define CAND_IDX_RANGE_LE	4
#define CAND_IDX_RANGE_LT	5
#define CAND_IDX_RANGE_GE	6
#define CAND_IDX_RANGE_GT	7
#define CAND_ROWID      	8
#define CAND_SYS_VAR    	9
#define CAND_UNION      	10


/***********************************************************************
** Type Definitions
*/

typedef struct cand_s {
        int     type;           /* Lookup type - SEQ, IDX_ABS, IDX_RANGE */
        u_int   nextPos,        /* Seq search next pos */
                lastPos;        /* Last key based location */
        int     index,          /* Which index to use */
                ident,          /* Is it an IDENT based lookup? */
                length,         /* Length of buffer space */
                rowID,          /* Row ID for position lookups */
                keyType;        /* Index key type from mindex struct */
        u_char  *rangeMin,      /* IDX_RANGE start value */
                *rangeMax;      /* IDX_RANGE stop value */
        char    *buf,           /* Index value buffer */
                *maxBuf,
                idx_name[NAME_LEN + 1];
        idx_hnd handle;
        idx_cur cursor;
        idx_hnd *unionIndex;
} mCand_t;



/***********************************************************************
** Function Prototypes
*/
void craFreeCandidate __ANSI_PROTO((mCand_t *));
void craResetCandidate __ANSI_PROTO((mCand_t *,int));
u_int craGetCandidate __ANSI_PROTO((cache_t*,mCand_t *));

int craSetCandidateValues __ANSI_PROTO((cache_t*,mCand_t*,mField_t*,mCond_t*,row_t*,mQuery_t*));

mTable_t *craReorderTableList __ANSI_PROTO((mQuery_t*));
mCand_t *craSetupCandidate __ANSI_PROTO((cache_t*,mQuery_t*,int));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

