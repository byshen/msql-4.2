/*
** Process Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_PROCESS_H

#define MAIN_PROCESS_H 1

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

void processQuery __ANSI_PROTO((msqld*, mQuery_t*, int, char*));
void processCopyDB __ANSI_PROTO((msqld*, int,char*,char*));
void processMoveDB __ANSI_PROTO((msqld*, int,char*,char*));
void processDropDB __ANSI_PROTO((msqld*, int,char*));
void processListDBs __ANSI_PROTO((msqld*, int));
void processCreateDB __ANSI_PROTO((msqld*, int,char*));
void processListIndex __ANSI_PROTO((msqld*, int,char*,char*,char*));
void processListTables __ANSI_PROTO((msqld*, int,char*));
void processListFields __ANSI_PROTO((msqld*, int,char*,char*));
void processExportTable __ANSI_PROTO((msqld*, int,char*,char*,char*,char*,mQuery_t*));
void processImportTable(msqld*, int, char*, char*, char*, mQuery_t*);
void processSequenceInfo __ANSI_PROTO((msqld*, int,char*,char*));
void processCheckTable(msqld*, int, char*, char*);
void processTableInfo(msqld*, int, char*, char*);

int processCreateTable __ANSI_PROTO((msqld*, mQuery_t*));
int processCreateIndex __ANSI_PROTO((msqld*, mQuery_t*));
int processDropSequence __ANSI_PROTO((msqld*, mQuery_t*));
int processCreateSequence __ANSI_PROTO((msqld*, mQuery_t*));
int processDropTable __ANSI_PROTO((msqld*, mQuery_t*));
int processDropIndex __ANSI_PROTO((msqld*, mQuery_t*, int));
int processDelete __ANSI_PROTO((msqld*, mQuery_t*));
int processInsert __ANSI_PROTO((msqld*, mQuery_t*));
int processUpdate __ANSI_PROTO((msqld*, mQuery_t*));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

