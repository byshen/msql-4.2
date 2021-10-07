/*
** NK Config Private Header
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef CONFIG_MODULE_PRIV_H

#define CONFIG_MODULE_PRIV_H 1

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
#define	CONFIG_CHAR_TYPE	1
#define CONFIG_INT_TYPE		2

#define CONFIG_SEC_GENERAL	1

#define skipWhite(cp) while(*cp=='\t' || *cp==' '){cp++;}
#define getLine(b,s,f)	fgets(b,s,f); curLine++

/***********************************************************************
** Type Definitions
*/

typedef struct {
	char	*section,
		*handle;
	int	type;
	char 	*charVal;
	int	intVal,
		allowNull;
} config_entry;




/***********************************************************************
** Standard header file footer.
*/
  
#ifdef __cplusplus
        }
#endif /* __cplusplus */
#endif /* file inclusion */

