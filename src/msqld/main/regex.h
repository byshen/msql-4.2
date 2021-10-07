/*
** Regex Header File
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_REGEX_H

#define MAIN_REGEX_H 1

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

int likeTest (void*,char*,int,char,char);
int sLikeTest (char*,char*,int);
int rLikeTest (char*,char*,int);
int regexStringLength (char*,int);
int soundex (char*);


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

