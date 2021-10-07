/*
** Config Header
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef CONFIG_MODULE_H

#define CONFIG_MODULE_H 1

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
#define CONFIG_OK		0
#define	CONFIG_ERR_LOADED	-1	/* config already loaded */
#define CONFIG_ERR_FILE	-2	/* error accessing file */
#define CONFIG_ERR_DATA	-3	/* invalid config data */
#define CONFIG_ERR_ENTRY	-4	/* unknown config entry */
#define CONFIG_ERR_VALUE	-5	/* invalid config value */
#define	CONFIG_MAX_ERR_LEN	255	/* Max lenght of err message */


/***********************************************************************
** Function Prototypes
*/

int 	configGetIntEntry __ANSI_PROTO((char*, char*));
char* 	configGetCharEntry __ANSI_PROTO((char*, char*));
int 	configLoadFile __ANSI_PROTO((char*));
int 	configReloadFile __ANSI_PROTO((char*));
char* 	configGetError __ANSI_PROTO(());


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

