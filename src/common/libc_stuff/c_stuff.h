/*
** libc_stuff Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef LIBC_STUFF_H

#define LIBC_STUFF_H 1

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
** Function Prototypes
*/

#ifdef HUGE_T

void strtohuge __ANSI_PROTO((char*, HUGE_T*));

#endif

/***********************************************************************
** Type Definitions
*/


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

