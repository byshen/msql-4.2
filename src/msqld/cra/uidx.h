/*
** Union Index Header File
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef CRA_UIDX_H

#define CRA_UIDX_H 1

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

typedef struct {
	void	*native;
	int	type;
} uidx_t;

/***********************************************************************
** Function Prototypes
*/

int unionIdxCreate __ANSI_PROTO((idx_hnd*));
int unionIdxCreateTmpIndex __ANSI_PROTO((idx_hnd*,idx_hnd*,mIndex_t*,mCond_t*));
int unionIdxLookup __ANSI_PROTO((idx_hnd*,int));
int unionIdxGet __ANSI_PROTO((idx_hnd*,int));
int unionIdxLookup __ANSI_PROTO((idx_hnd*,int));

void unionIdxFree __ANSI_PROTO((idx_hnd*));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */


