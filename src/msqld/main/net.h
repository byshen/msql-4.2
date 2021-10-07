/*
** net Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_NET_H

#define MAIN_NET_H 1

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


int netWritePacket __ANSI_PROTO((int));
int netReadPacket __ANSI_PROTO((int));

void netError __ANSI_PROTO((int, char*, ...));
void netOK __ANSI_PROTO((int));
void netEndOfList __ANSI_PROTO(());
void netInitialise __ANSI_PROTO(());

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

