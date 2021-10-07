/*
** memory Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_MEMORY_H

#define MAIN_MEMORY_H 1

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

#define memCopyField(src,dst)	bcopy(src,dst,sizeof(mField_t))

#define XXXmemCopyField(src,dst)	{			\
        strcpy(dst->table,src->table);			\
	strcpy(dst->name,src->name);			\
        dst->value = src->value;			\
        dst->function = src->function;			\
        dst->entry = src->entry;			\
        dst->type = src->type;				\
	dst->sysvar = src->sysvar;			\
	dst->length = src->length;			\
	dst->dataLength = src->dataLength;		\
	dst->offset = src->offset;			\
	dst->null = src->null;				\
	dst->flags = src->flags;			\
	dst->fieldID = src->fieldID;			\
	dst->literalParamFlag = src->literalParamFlag;	\
	dst->functResultFlag = src->functResultFlag;	\
	dst->overflow = src->overflow;			\
	}


/***********************************************************************
** Type Definitions
*/


/***********************************************************************
** Function Prototypes
*/

void 		memFreeToken __ANSI_PROTO((u_char*));
void 		memFreeField __ANSI_PROTO((mField_t*));
void 		memFreeIdent __ANSI_PROTO((mIdent_t*));
void 		memFreeQuery __ANSI_PROTO((mQuery_t*));
void 		memFreeValue __ANSI_PROTO((mVal_t*));
void 		memFreeTable __ANSI_PROTO((mTable_t*));
void 		memFreeCondition __ANSI_PROTO((mCond_t*));
void 		memFreeOrder __ANSI_PROTO((mOrder_t*));
void 		memFreeValList __ANSI_PROTO((mValList_t*));
void 		memDropCaches __ANSI_PROTO(());

char		*memMallocToken __ANSI_PROTO((char *, int));
mField_t 	*memMallocField __ANSI_PROTO(());
mIdent_t 	*memMallocIdent __ANSI_PROTO(());
mQuery_t 	*memMallocQuery __ANSI_PROTO(());
mVal_t 		*memMallocValue __ANSI_PROTO(());
mTable_t	*memMallocTable __ANSI_PROTO(());
mCond_t		*memMallocCondition __ANSI_PROTO(());
mOrder_t	*memMallocOrder __ANSI_PROTO(());
mValList_t	*memMallocValList __ANSI_PROTO(());


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

