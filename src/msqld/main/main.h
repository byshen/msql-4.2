/*
** main Header File	(Public or Private)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_MAIN_H

#define MAIN_MAIN_H 1

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

typedef struct {
	int		unixSock,
			ipSock,
			inServerShutdown;
	u_int		startTime,
			numCons,
			numKids,
			maxCons,
			maxSock,
			numQueries;
	char		*unixPort,
			queryBuf[MAX_QUERY_LEN + 1],
			confFile[MSQL_PATH_LEN + 1];
	cinfo_t		conArray[MAX_CONNECTIONS];
	fd_set		clientFDs;
	mConfig_t	config;
	FILE		*logFP,
			*updateFP;
} msqld;

/***********************************************************************
** Function Prototypes
*/

void sendServerStats (msqld*,int);
void freeClientConnection (msqld*,int);
void logQuery (FILE *, cinfo_t*, char*);
void terminateChildren();
void childStartup ();
void setupSignals();
void setupServer(msqld*);
void setupServerSockets(msqld*);
void usage(char*);
RETSIGTYPE puntServer (int);
RETSIGTYPE puntClient (int);
RETSIGTYPE sigTrap (int);


/***********************************************************************
** Standard header file footer.  
*/


#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

