/*
** msql_defs Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MSQL_DEFS_H

#define MSQL_DEFS_H 1

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

#define	NAME_LEN	35	/* Max length of field or table name */
#define MAX_FIELDS      75	/* Max fields per query */
#define MSQL_PATH_LEN	255	/* Max length of a file path */
#define MSQL_MAX_CHAR	1000000	/* Max length of a TEXT field */
#define	LANG_ENGLISH		/* Language for error messages */
#define MSQL_GLOBAL_BUF_LEN 4097 /* max query buffer len for diags */


/***********************************************************************
** Function Prototypes
*/


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

