/*
** Broker Header File
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef BROKER_H

#define BROKER_H 1

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

#define	BROKER_FROM_FD	3
#define	BROKER_TO_FD	4
#define	BROKER_OOB_FD	5


#define CMD_CLIENT_OPEN         1
#define CMD_CLIENT_CLOSE        2
#define CMD_FLUSH_CACHE         3
#define CMD_CLIENT_DB           4
#define CMD_RUN_QUEUE           5
#define CMD_QUEUE_END           6


/***********************************************************************
** Type Definitions
*/

typedef struct {
        int     command,
                access,
                client;
        char    db[NAME_LEN + 1],
                table[NAME_LEN + 1],
                user[NAME_LEN + 1],
		client_ip[16];
} mMsg_t;

typedef struct msg_q_t{
        mMsg_t 	message;
        int     fd;
        struct  msg_q_t *next;
} mMsg_q;

typedef struct {   
        int     toSock,
                fromSock,
                oobSock,   
                pid,
                messages,
		jabbed;
        mMsg_q  *messages_head,
                *messages_tail; 
} ipc_t;



/***********************************************************************
** Function Prototypes
*/

void brokerRunMessageQueue __ANSI_PROTO((int));
void brokerNotifyChild __ANSI_PROTO((int,int,int,char*,char*,char*,int,char*));
void brokerNotifyAllChildren __ANSI_PROTO((int,int,int,char*,char*,char*,int,char*));
void brokerCloseClient __ANSI_PROTO((int,int));
void brokerCloseChildren __ANSI_PROTO(());
void brokerAddChildSockets __ANSI_PROTO((fd_set*));
void brokerReadChildMessage __ANSI_PROTO((int));
void brokerCheckChildren __ANSI_PROTO((fd_set*));
void brokerChildSendFlush __ANSI_PROTO((char*, char*));
void brokerChildSendMessage __ANSI_PROTO((mMsg_t*));

int brokerStartChildren __ANSI_PROTO((msqld*, int, char**));
int brokerSendFD __ANSI_PROTO((int,int));
int brokerRecvFD __ANSI_PROTO((int));

char *brokerGetCommandName __ANSI_PROTO((int));

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */

