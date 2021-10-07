/*
** Parse Header File
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_PARSE_H

#define MAIN_PARSE_H 1

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

mQuery_t *parseQuery __ANSI_PROTO((msqld*, char*, int, char*, char*));

mVal_t *parseCreateNullValue __ANSI_PROTO(());
mVal_t *parseFillValue __ANSI_PROTO((char*,int,int));
mVal_t *parseCreateValue __ANSI_PROTO((u_char*,int,int));

mIdent_t *parseCreateIdent __ANSI_PROTO((char*,char*,mQuery_t*));

void parseFreeValue __ANSI_PROTO((mVal_t*));
void parseCleanQuery __ANSI_PROTO((mQuery_t*));
void parseAddBetween __ANSI_PROTO((mIdent_t*,mVal_t*,mVal_t*,int,mQuery_t*));
void parseAddSubCond __ANSI_PROTO((int,mQuery_t*));
void parsePopCondition __ANSI_PROTO((mQuery_t*));
void parsePushCondition __ANSI_PROTO((mQuery_t*));
void parseAddFieldValue __ANSI_PROTO((mVal_t*,mQuery_t*));
void parseSetTargetTable __ANSI_PROTO((char *,mQuery_t*));
void parseSetRowLimit __ANSI_PROTO((mVal_t*,mQuery_t*));
void parseSetRowOffset __ANSI_PROTO((mVal_t*,mQuery_t*));
void parseAddSequence __ANSI_PROTO((char*,int,int,mQuery_t*));
void parseAddOrder __ANSI_PROTO((mIdent_t*,int,mQuery_t*));
void parseSetFunctOutputName __ANSI_PROTO((char*,mQuery_t*));

void parseAddInsertValue __ANSI_PROTO((mVal_t*, mQuery_t*));
void parseSetInsertOffset __ANSI_PROTO((int));
void _cleanFields(mField_t*);

int parseAddIndex __ANSI_PROTO((char*,char*,int,int,mQuery_t*));
int parseAddTable __ANSI_PROTO((char*,char*,mQuery_t*));
int parseAddField __ANSI_PROTO((mIdent_t*,int,char*,int,int,mQuery_t*));
int parseCopyValue __ANSI_PROTO((char*,mVal_t*,int,int,int));
int parseAddFunction __ANSI_PROTO((char*,mQuery_t*));
int parseAddCondition __ANSI_PROTO((mIdent_t*,int,mVal_t*,int,mQuery_t*));
int parseAddFunctParam __ANSI_PROTO((mIdent_t*,mQuery_t*));
int parseAddFunctLiteral __ANSI_PROTO((mVal_t*,mQuery_t*));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

