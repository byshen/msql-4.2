/*
** varchar Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_VARCHAR_H

#define MAIN_VARCHAR_H 1

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
	cache_t	*cacheEntry;
	char	buf[OFB_SIZE],
		*cp,
		*data;
	u_int	pos,
		segLen,
		dataLen,
		remain;
} vc_cursor;
	


/***********************************************************************
** Function Prototypes
*/

u_char *varcharRead (cache_t*, u_char*, int);
u_int varcharWrite (cache_t*, u_char*, int);
void varcharDelete (cache_t*,u_int);
int varcharCompare (cache_t*,u_char*,u_char*,int);
int varcharMatch (cache_t*,u_char*,char*,int,int);

char *varcharGetNext (vc_cursor *);
void *varcharDupCursor (vc_cursor*);
vc_cursor *varcharMakeCursor (cache_t*, u_char *, u_int);

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

