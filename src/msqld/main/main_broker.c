/*
** Copyright (c) 1995-2001  Hughes Technologies Pty Ltd.  All rights
** reserved.  
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this 
** software, including all implied warranties of merchantability and 
** fitness, in no event shall Hughes Technologies be liable for any 
** special, indirect or consequential damages or any damages whatsoever 
** resulting from loss of use, data or profits, whether in an action of 
** contract, negligence or other tortious action, arising out of or in 
** connection with the use or performance of this software.
**
**
** $Id: main_broker.c,v 1.25 2012/06/04 05:40:02 bambi Exp $
**
*/

/*
** Module	: main : msqld_main
** Purpose	: 
** Exports	: 
** Depends Upon	: 
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <common/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <signal.h>

#ifdef HAVE_SETRLIMIT
#  include <sys/resource.h>
#endif

#if HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/portability.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/includes/errmsg.h>
#include <msqld/main/main.h>
#include <msqld/broker/broker.h>
#include <msqld/main/version.h>
#include <msqld/main/process.h>
#include <msqld/main/table.h>
#include <msqld/main/net.h>
#include <msqld/main/acl.h>
#include <msqld/main/cache.h>
#include <msqld/main/util.h>
#include <msqld/main/parse.h>
#include <msqld/main/tcp.h>
#include <msqld/lock/lock.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

msqld	*globalServer;

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

static void _zeroMessageStruct(msg)
	mMsg_t	*msg;
{
	bzero(msg, sizeof(mMsg_t));
	return;
	msg->command = msg->access = msg->client = 0;
	*msg->db = *msg->table = *msg->user = *msg->client_ip = 0;
}
	

/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


extern  u_char  *yytext,
		*yyprev;
extern	int	yylineno;
extern	int	yydebug;

extern	char	errMsg[];

int	curSock,
	vFlag = 0,
	rFlag = 0,
	cFlag = 0,
	numCons = 0,
	conCount = 0,
	numKids = 0,
	eintrCount = 0,
	msyncTimer,
	startTime,
	explainOnly;

FILE	*logFP;

u_int	serverStartTime = 0,
	serverNumCons = 0,
	serverNumQueries = 0;

char	*unixPort,
	confFile[MSQL_PATH_LEN];

extern	char	*packet;

char	PROGNAME[] = "msqld",
	BLANK_ARGV[] = "						  ";


static char *comTable[] = {
	"???", "Quit", "Init DB", "Query", "DB List", "Table List",
	"Field List", "Create DB", "Drop DB", "Reload ACL",
	"Shutdown", "Index List", "Stats", "Seq Info", "Move DB",
	"Copy DB", "???" };


#if defined(_OS_WIN32)
RETSIGTYPE sigTrap(int sig);
RETSIGTYPE puntServer(int sig);
RETSIGTYPE puntClient(int sig);
#endif






void childStartup(server)
	msqld		*server;
{
	mQuery_t	*query,
			dummyQuery;
	cinfo_t		*conArray;
	int		parent,
			count,
			clientCommand,
			remain,
			numBytes,
			numFDs,
			offset,
			comSock,
			endOfQueue,
			haveLock,
			clientsProcessed;
	char		dbname[255],
			*arg, *arg2, *arg3,
			*cp,
			go,
			tmpChar;
	fd_set  	readFDs,
			clientFDs;
	mMsg_t		message;
	struct timeval	timeout;



	/* Ensure there's no wierd FD's open */
	for ( count = BROKER_OOB_FD + 1; count<=255; count++)
	{
		close(count);
	}

	/* Try to set the locale but don't bail if it fails (freeBSD) */
	setlocale(LC_ALL, "");


	/*
	** Initialise the connection array.  We get the conArray address
	** for shorthand.
	*/
	conArray = server->conArray;
	count = 0;
	while (count < MAX_CONNECTIONS)
	{
		conArray[count].sock = -1;
		conArray[count].db = NULL;
		count++;
	}


	/*
	** OK, on with the show
	*/

	bzero(&dummyQuery, sizeof(dummyQuery));
	strcpy(PROGNAME,"child");
	yytext = NULL;

	umask(0077);
	setupSignals();
	aclLoadFile(0);
	(void)bzero(&readFDs,sizeof(fd_set));
	FD_ZERO(&clientFDs);

	parent = getppid();
	while(1)
	{
		/*
		** Never leave a zombie.  Exit if our parent has gone away
		*/
		if (getppid() != parent)
		{
			if (vFlag)
			{
				printf("FATAL : Parent changed in process %d\n",
					getpid());
				exit(1);
			}
		}

		/*
		** Before we waste cycles getting the IPC lock (which
		** we only need if we select() on the client connection
		** sockets), check to see if there's anything waiting
		** on the broker socket (which is dedicated to us and
		** we can do with as we please).
		*/
		haveLock = 0;
		FD_ZERO(&readFDs);
		FD_SET(BROKER_FROM_FD,&readFDs);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if(select(FD_SETSIZE,&readFDs,0,0,&timeout) == 1)
		{
			/*
			** The broker has jabbed us.  Run the queue
			** without worrying about locking for access
			** to the client sockets.  We won't touch them
			** this time.
			*/
			msqlDebug0(MOD_GENERAL,
				"Found something on the broker sock\n");
		}
		else
		{
			/*
			** OK, a poll of the broker socket didn't get
			** us anywhere.  Grab the lock and check out all
			** the descriptors
			*/
			bcopy(&clientFDs, &readFDs, sizeof(readFDs));
			FD_SET(BROKER_FROM_FD,&readFDs);
			lockGetIpcLock(server);
			haveLock = 1;
			numFDs = select(FD_SETSIZE,&readFDs,0,0,0);
			if(numFDs < 0)
			{
				if (errno == EINTR)
					continue;
				perror("select");
				puntServer(0);
			}
			msqlDebug0(MOD_GENERAL,"Select broken\n");
		}

		/*
		** Is there something in the broker's queue?
		*/
		if (FD_ISSET(BROKER_FROM_FD,&readFDs))
		{
			/*
			** Hmmm, something came in while we were
			** sleeping in the select.
			**
			** The broker has jabbed us.  Drop the lock,
			** read the jab byte, and ask it for any 
			** queued messages.
			*/

			if (haveLock)
			{
				lockReleaseIpcLock(server);
				haveLock = 0;
			}

			read(BROKER_FROM_FD,&go,1);

			_zeroMessageStruct(&message);
			message.command = CMD_RUN_QUEUE;
			message.client = 0;
			brokerChildSendMessage(&message);

			endOfQueue = 0;
			while(endOfQueue == 0)
			{
				remain = sizeof(mMsg_t);
				offset = 0;
				while(remain)
				{
					cp = ((char *)&message) + offset;
					numBytes = read(BROKER_FROM_FD,cp,
						remain);
					if (numBytes <= 0)
					{
						perror("read");
						puntServer(SIGQUIT);
					}
					remain -= numBytes;
					offset += numBytes;
				}


				switch(message.command)
				{
				case CMD_QUEUE_END:
					msqlDebug0(MOD_BROKER,
					    "Processing Broker QUEUE_END\n");
					endOfQueue = 1;
					break;

				case CMD_CLIENT_OPEN:
					msqlDebug0(MOD_BROKER,
					    "Processing Broker CLIENT_OPEN\n");
					comSock = brokerRecvFD(BROKER_FROM_FD);
					if (comSock < 0)
					{
						perror("brokerRecvFD");
						puntServer(0);
					}
					msqlDebug2(MOD_BROKER,
					    "Got new connection (%d on %d)\n",
					    message.client, comSock);
					FD_SET(comSock, &clientFDs);
					conArray[message.client].sock = comSock;
					conArray[message.client].user =
						(char *)strdup(message.user);
					conArray[message.client].connectTime =
						time(NULL);
					conCount++;
					numCons++;
					break;

				
				case CMD_FLUSH_CACHE:
					msqlDebug2(MOD_BROKER,
					    "Processing Broker FLUSH(%s.%s)\n",
					    message.db, message.table);
					if (*message.table)
						cacheInvalidateTable(server,
							message.db,
							message.table);
					else
						cacheInvalidateDatabase(
							server,
							message.db);
					break;
				
				case CMD_CLIENT_DB:
					msqlDebug0(MOD_BROKER,
					       "Processing Broker CLIENT_DB\n");
					if(conArray[message.client].db)
					    free(conArray[message.client].db);
					conArray[message.client].db =
						strdup(message.db);
					conArray[message.client].access =
						message.access;
					break;

				case CMD_CLIENT_CLOSE:
					count = message.client;
					comSock = conArray[count].sock;
					msqlDebug2(MOD_BROKER,
					 "Processing Broker CLOSE (%d on %d)\n",
						count, comSock);
					if (comSock < 0)
					{
						msqlDebug2(MOD_BROKER,
						 "Close ignored (%d on %d)\n",
						count, comSock);
						break;
					}
					FD_CLR(comSock,&clientFDs);
					shutdown(comSock,2);
					close(comSock);
					conArray[count].sock = -1;
					if(conArray[count].user)
					{
						free(conArray[count].user);
						conArray[count].user = NULL;
					}
					if(conArray[count].host)
					{
						free(conArray[count].host);
						conArray[count].host = NULL;
					}
					if(conArray[count].db)
					{
						free(conArray[count].db);
						conArray[count].db = NULL;
					}
					numCons--;
					break;
				}
			}
			continue;
		}

		/*
		** Must be coming from an active client
		*/
		count = 0;
		clientsProcessed = 0;
		while(count <= 255)
		{
			if (conArray[count].sock == -1)
			{
				count++;
				continue;
			}
			if (!FD_ISSET(conArray[count].sock,&readFDs))
			{
				count++;
				continue;
			}

			/* Got one ! */
			clientsProcessed++;
			comSock = conArray[count].sock;
			if (netReadPacket(comSock) <= 0)
			{
				msqlDebug1(MOD_GENERAL,
					"Command read on sock %d failed!\n",
					comSock);
				clientCommand = QUIT;
			}
			else
			{
				clientCommand = atoi(packet);
			}
			msqlDebug3(MOD_GENERAL,"Command on sock %d = %d (%s)\n",
				comSock, clientCommand, 
				comTable[clientCommand]);

			/*
			** with it.
			if (clientCommand != QUIT && clientCommand != INIT_DB)
				lockReleaseIpcLock(server);
			*/

			switch(clientCommand)
			{
			    case QUIT:
				msqlDebug0(MOD_GENERAL,"DB QUIT!\n");
				FD_CLR(comSock,&clientFDs);
				shutdown(comSock,2);
				close(comSock);
				if(conArray[count].user)
				{
					free(conArray[count].user);
					conArray[count].user = NULL;
				}
				if(conArray[count].host)
				{
					free(conArray[count].host);
					conArray[count].host = NULL;
				}
				if(conArray[count].db)
				{
					free(conArray[count].db);
					conArray[count].db = NULL;
				}
				conArray[count].sock = -1;

				/* Send broker update request */
				_zeroMessageStruct(&message);
				message.command = CMD_CLIENT_CLOSE;
				message.client = count;
				brokerChildSendMessage(&message);
				break;


			    case INIT_DB:
				cp=(char *)strtok(packet+2,"\n\r");
				if (!cp)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				strcpy(dbname,cp);
				msqlDebug1(MOD_GENERAL,"DBName = %s\n", dbname);
				conArray[count].access = aclCheckAccess(
					dbname, conArray + count);
				if(conArray[count].access == NO_ACCESS)
				{
					netError(comSock,ACCESS_DENIED_ERROR);
					break;
				}
				if(conArray[count].db)
				{
					free(conArray[count].db);
					conArray[count].db = NULL;
				}
				conArray[count].db = (char *)strdup(dbname);
				if (utilCheckDB(server, dbname) < 0)
				{
					netError(comSock,errMsg);
					break;
				}

				/* Send broker update */
				_zeroMessageStruct(&message);
				message.command = CMD_CLIENT_DB;
				message.access = conArray[count].access;
				message.client = count;
				strcpy(message.db,dbname);
				brokerChildSendMessage(&message);
				netOK(comSock);
				break;

			    case QUERY:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				curSock = comSock;
				cp=(char *)(packet+2);
				arg = (char *)strdup(cp);
				tmpChar = 0;
				if (strlen(arg) > MAX_QUERY_LEN)
				{
					tmpChar = *(arg+MAX_QUERY_LEN);
					*(arg+MAX_QUERY_LEN) = 0;
				}
				msqlDebug1(MOD_QUERY,"Query = %s",arg);
				if (tmpChar)
					*(arg+MAX_QUERY_LEN) = tmpChar;
				aclSetPerms(conArray[count].access);
				query = parseQuery(server, arg,comSock,
					conArray[count].user, 
					conArray[count].db);
				if (query)
				{
					if (haveLock)
					{
						lockReleaseIpcLock(server);
						haveLock = 0;
					}
					processQuery(server,query,count,arg);
					parseCleanQuery(query);
				}
				if(arg)
					free(arg);
				/* cacheSyncCache(); */
				break;


			    case EXPLAIN:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				curSock = comSock;
				cp=(char *)(packet+2);
				while(*cp != ':') cp++;
				arg = (char *)strdup(cp + 1);
				tmpChar = 0;
				if (strlen(arg) > MAX_QUERY_LEN)
				{
					tmpChar = *(arg+MAX_QUERY_LEN);
					*(arg+MAX_QUERY_LEN) = 0;
				}
				msqlDebug1(MOD_QUERY,"Explain = %s",arg);
				if (tmpChar)
					*(arg+MAX_QUERY_LEN) = tmpChar;
				aclSetPerms(conArray[count].access);
				query = parseQuery(server, arg,comSock,
					conArray[count].user, 
					conArray[count].db);
				if (query)
				{
					query->explainOnly = 1;
					processQuery(server,query,count,arg);
					parseCleanQuery(query);
				}
				if(arg)
					free(arg);
				/* cacheSyncCache(); */
				break;


			    case DB_LIST:
				curSock = comSock;
				processListDBs(server, comSock);
				break;

			    case SEQ_INFO:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				cp = (char *)index(packet,':');
				cp=(char *)strtok(cp+1, "\n\r");
				arg = (char *)strdup(cp);
				curSock = comSock;
				processSequenceInfo(server, comSock,
					arg,conArray[count].db);
				if(arg)
					free(arg);
				break;

			    case TABLE_LIST:
				if (!conArray[count].db)
				{
					netError(comSock, NO_DB_ERROR);
					break;
				}
				curSock = comSock;
				processListTables(server, comSock,
					conArray[count].db);
				break;

			    case TABLE_INFO:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				cp = (char *)index(packet,':');
				cp=(char *)strtok(cp+1, "\n\r");
				arg = (char *)strdup(cp);
				curSock = comSock;
				processTableInfo(server, comSock,
					arg,conArray[count].db);
				if(arg)
					free(arg);
				break;

			    case FIELD_LIST:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				cp=(char *)strtok(packet+2,
					"\n\r");
				arg = (char *)strdup(cp);
				curSock = comSock;
				processListFields(server, comSock, arg,
					conArray[count].db);
				if(arg)
					free(arg);
				break;

			    case INDEX_LIST:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				cp=(char *)strtok(packet+2,
					":\n\r");
				arg = (char *)strdup(cp);
				cp=(char *)strtok(NULL,"\n\r");
				arg2 = (char *)strdup(cp);
				curSock = comSock;
				processListIndex(server, comSock, arg2, arg,
					conArray[count].db);
				if(arg)
					free(arg);
				if(arg2)
					free(arg2);
				break;

			    case EXPORT:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				arg = arg2 = arg3 = NULL;
				cp=(char *)strtok(packet+2, ":\n\r");
				arg = (char *)strdup(cp);
				cp=(char *)strtok(NULL,":\n\r");
				arg2 = (char *)strdup(cp);
				cp=(char *)strtok(NULL,":\n\r");
				if (cp)
				{
					arg3 = (char *)strdup(cp);
				}

				curSock = comSock;
				processExportTable(server, comSock,
					server->conArray[count].db, arg, 
					arg2, arg3, NULL);
				if(arg)
					free(arg);
				if(arg2)
					free(arg2);
				if(arg3)
					free(arg3);
				break;

			    case IMPORT:
				if (!conArray[count].db)
				{
					netError(comSock,NO_DB_ERROR);
					break;
				}
				cp=(char *)strtok(packet+2,
					":\n\r");
				arg = (char *)strdup(cp);
				cp=(char *)strtok(NULL,":\n\r");
				arg2 = (char *)strdup(cp);
				curSock = comSock;
				dummyQuery.curUser = conArray[count].user;
				dummyQuery.curDB = conArray[count].db;
				processImportTable(server,comSock, 
					conArray[count].db, arg,arg2, 
					&dummyQuery);
				if(arg)
					free(arg);
				if(arg2)
					free(arg2);
				break;

			    case CREATE_DB:
				if (!aclCheckLocal(conArray + count))
				{
					netError(comSock,PERM_DENIED_ERROR);
					break;
				}
				cp=(char *)strtok(packet+2,
					"\n\r");
				arg = (char *)strdup(cp);
				processCreateDB(server, comSock,arg);
				if(arg)
					free(arg);
				break;

			    case COPY_DB:
				if (!aclCheckLocal(conArray + count))
				{
					netError(comSock,PERM_DENIED_ERROR);
					break;
				}
				cp=(char *)strtok(packet+2, ":\n\r");
				arg = (char *)strdup(cp);
				cp=(char *)strtok(NULL, "\n\r");
				arg2 = (char *)strdup(cp);
				processCopyDB(server, comSock,arg, arg2);
				if(arg)
					free(arg);
				if(arg2)
					free(arg2);
				break;

			    case MOVE_DB:
				if (!aclCheckLocal(conArray + count))
				{
					netError(comSock,PERM_DENIED_ERROR);
					break;
				}
				cp=(char *)strtok(packet+2, ":\n\r");
				arg = (char *)strdup(cp);
				cp=(char *)strtok(NULL, "\n\r");
				arg2 = (char *)strdup(cp);
				processMoveDB(server, comSock,arg, arg2);
				if(arg)
					free(arg);
				if(arg2)
					free(arg2);
				
				/* Send broker update request */
				_zeroMessageStruct(&message);
				message.command = CMD_FLUSH_CACHE;
				message.client = 0;
				strcpy(message.db, arg);
				brokerChildSendMessage(&message);
				break;

			    case DROP_DB:	
				if (!aclCheckLocal(conArray + count))
				{
					netError(comSock,PERM_DENIED_ERROR);
					break;
				}
				cp=(char *)strtok(packet+2,
					"\n\r");
				arg = (char *)strdup(cp);
				processDropDB(server, comSock,arg);
				
				/* Send broker update request */
				_zeroMessageStruct(&message);
				message.command = CMD_FLUSH_CACHE;
				message.client = 0;
				strcpy(message.db, arg);
				brokerChildSendMessage(&message);
				if(arg)
					free(arg);
				break;

			    case RELOAD_ACL:
				if (!aclCheckLocal(conArray + count))
				{
					netError(comSock,PERM_DENIED_ERROR);
					break;
				}
				aclReloadFile(comSock);
				netEndOfList(comSock);
				break;

			    case SHUTDOWN:
				if (!aclCheckLocal(conArray + count))
				{
					netError(comSock,PERM_DENIED_ERROR);
					break;
				}
				netEndOfList(comSock);
				puntServer(-1);
				kill(parent,SIGUSR1);
				exit(0);
				break;

			    case SERVER_STATS:
				if (!aclCheckLocal(conArray + count))
				{
					netError(comSock,PERM_DENIED_ERROR);
					break;
				}
				sendServerStats(server, comSock);
				netEndOfList(comSock);
				break;

			    default:
				netError(comSock, UNKNOWN_COM_ERROR);
				break;
			}
			
			/*
			** Fall out of the loop so that we process
			** requests in sequence.  Otherwise we may
			** have given away the select lock when we
			** actually need it (like INIT_DB)
			*/
			break;
		}
		if(haveLock)
		{
		       	lockReleaseIpcLock(server);
		}
		if (clientsProcessed == 0)
		{
			/*
			** Something strange is going on.  We fell out
			** of select but didn't do anything.  The client
			** fdset and the connection table must not match
			*/
			printf("Bad client FD_SET in %d!\n", (int)getpid());
			abort();
		}
	}
}


void terminateChildren()
{
	brokerCloseChildren();

	/*
	** Send another INT signal to the process group to make sure
	** all the children cop a signal.  Make sure we ignore it so we
	** don't start recursively trying to clean things up.
	*/
	signal(SIGINT, SIG_IGN);
	killpg(getpgid(0), SIGINT);
}



void debugTrap()
{
	/* We use this as a debugger breakpoint */
}




int daemonMain(server, argc, argv)
	msqld	*server;
	int	argc;
	char	**argv;
{
	int     sock,
		newSock,
		error,
		opt;
	char    *uname,
		*ipAddr;
	fd_set  readFDs;
	struct  sockaddr_in remote;
	struct	sockaddr_in 	cAddr;
	struct	sockaddr	dummy;
	int	parent,
		cAddrLen,
		dummyLen;



	setupServer(server);
	setupServerSockets(server);
	setupSignals();
	brokerStartChildren(server,argc,argv);
	aclLoadFile(1);

	umask(0077);
	msqlDebug0(MOD_GENERAL,
		"miniSQL debug mode.  Waiting for connections.\n");

	parent = getppid();
	while(1)
	{
		/*
		** Never leave a zombie.  Exit if our parent has gone away
		*/
		if (getppid() != parent)
		{
			if (vFlag)
			{
				printf("FATAL : Parent changed in process %d\n",
					getpid());
				exit(1);
			}
		}

		/*
		** Setup the read fd set
		*/
		FD_ZERO(&readFDs);
		if (server->ipSock >= 0)
			FD_SET(server->ipSock,&readFDs);
		if (server->unixSock >= 0)
			FD_SET(server->unixSock,&readFDs);
		brokerAddChildSockets(&readFDs);


		/* Wait for some action */
		/*if(select((server->maxCons+server->config.cacheDescriptors)*/
		if(select(FD_SETSIZE, &readFDs,0,0,0) < 0)
		{
			continue;
		}

		/* 
		** Is there a child notifying us? 
		*/

		brokerCheckChildren(&readFDs);

		/*
		** How about a new connection?  Grab the lock so we
		** can ensure the broker notification gets out before
		** the new connection becomes active
		*/

		sock = 0;
		if (server->ipSock >= 0)
		{
			if (FD_ISSET(server->ipSock,&readFDs))
			{
				sock = server->ipSock;
			}
		}
		if (server->unixSock >= 0)
		{
			if (FD_ISSET(server->unixSock,&readFDs))
			{
				sock = server->unixSock;
			}
		}
		if (!sock)
		{
			continue;
		}


		/*
		** We've got something on a client socket
		*/
		bzero(&cAddr, sizeof(cAddr));
		cAddrLen = sizeof(struct sockaddr_in);
		newSock = accept(sock, (struct sockaddr *)&cAddr, 
			(u_int*)&cAddrLen);
		if(newSock < 0)
		{
			perror("Error in accept ");
			puntServer(-1);
			exit(1);
		}
		if (newSock > server->maxSock)
		{
			server->maxSock = newSock;
		}
		dummyLen = sizeof(struct sockaddr);
		if (getsockname(newSock,&dummy,(u_int*)&dummyLen) < 0)
		{
			perror("Error on new connection socket");
			continue;
		}
		if (server->conArray[newSock].db)
		{
			if(server->conArray[newSock].db)
			{
				free(server->conArray[newSock].db);
				server->conArray[newSock].db = NULL;
			}
			if(server->conArray[newSock].host)
			{
				free(server->conArray[newSock].host);
				server->conArray[newSock].host = NULL;
			}
			if(server->conArray[newSock].user)
			{
				free(server->conArray[newSock].user);
				server->conArray[newSock].user = NULL;
			}
		}

		/*
		** Are we over the connection limit
		*/
		server->numCons++;
		if (server->numCons > server->maxCons)
		{
			server->numCons--;
			netError(newSock,CON_COUNT_ERROR);
			shutdown(newSock,2);
			close(newSock);
			continue;
		}


		/*
		** store the connection details
		*/

		msqlDebug1(MOD_GENERAL,
			"New connection received on %d\n", newSock);
		error = 0;
		if (sock == server->ipSock)
		{
			int	addrLen;
			struct	hostent *hp;

			addrLen = sizeof(struct sockaddr);
			getpeername(newSock, (struct sockaddr *) &remote, 
				(u_int*)&addrLen);
			ipAddr = inet_ntoa(remote.sin_addr);
			if (ipAddr)
			{
				server->conArray[newSock].clientIP =
					strdup(ipAddr);
			}
			if(configGetIntEntry("system","host_lookup")==1)
			{
				/* 
				** Validate remote host 
				*/

    				if(auth_hostname(&remote)) 
				{
					netError(newSock, BAD_HOST_ERROR);
					error = 1;
					server->numCons--;
					shutdown(newSock,2);
					close(newSock);
				}

				/*
				** Grab the hostname for later
				*/
				hp = (struct hostent *)gethostbyaddr(
					(char *)&remote.sin_addr,
					sizeof(remote.sin_addr),
					AF_INET);
			}
			else
			{
				hp = NULL;
			}

			if (!hp)
			{
				server->conArray[newSock].host = 
					(char *)strdup("UNKNOWN_HOST");
				msqlDebug0(MOD_GENERAL,
					"Host=UNKNOWN_HOST\n");
			}
			else
			{
				server->conArray[newSock].host = 
					strdup((char *)hp->h_name);
				msqlDebug1(MOD_GENERAL,"Host = %s\n",
					hp->h_name);
			}
		}
		else
		{
			server->conArray[newSock].host = NULL;
			server->conArray[newSock].clientIP = NULL;
			msqlDebug0(MOD_GENERAL,"Host = UNIX domain\n");
		}

		server->conArray[newSock].connectTime = time(NULL);
		server->conArray[newSock].numQueries = 0;

		if (!error)
		{
			opt=1;
			setsockopt(newSock,SOL_SOCKET,SO_KEEPALIVE,
				(char *) &opt, sizeof(opt));
			snprintf(packet, PKT_LEN, "0:%d:%s\n",
				PROTOCOL_VERSION,SERVER_VERSION);
			netWritePacket(newSock);
			if (netReadPacket(newSock) <=0)
			{
				netError(newSock,HANDSHAKE_ERROR);
				server->numCons--;
				shutdown(newSock,2);
				close(newSock);
				server->conArray[newSock].host = NULL;
				server->conArray[newSock].clientIP=NULL;
			}
			else
			{
				uname = (char *)strtok(packet,"\n");
				msqlDebug1(MOD_GENERAL, "User = %s\n",uname);
				if(server->conArray[newSock].user)
				{
					free(server->conArray[newSock].user);
				}
				server->conArray[newSock].user = strdup(uname);
				brokerNotifyAllChildren(CMD_CLIENT_OPEN,
					-1, newSock, uname, NULL, NULL,
					0, server->conArray[newSock].clientIP);
				netEndOfList(newSock);
			}
		}
	}
}



int main(argc,argv)
	int	argc;
	char	*argv[];
{
	msqld	server;
	int	errFlag = 0,
		childStatus,
		pid,
		c;
	extern  char *optarg;


	/*
	** Handle the command line args
	*/

	while((c=getopt(argc,argv,"vrc:"))!= -1)
	{
		switch(c)
		{
			case 'c':
				if (*server.confFile)
					errFlag++;
				else
					strncpy(server.confFile,optarg,
						MSQL_PATH_LEN);
				cFlag++;
				break;

			case 'v':
				vFlag++;
				break;

			case 'r':
				rFlag++;
				break;

			case '?':
				errFlag++;
				break;
		}
	}
	if (errFlag)
	{
		usage("msql_broker");
		exit(1);
	}


	printf("\n\nMini SQL Version %s (Multi process server)\n",
		SERVER_VERSION);
	printf("Copyright (c) 1993-94 David J. Hughes\n");
	printf("Copyright (c) 1995-2018 Hughes Technologies Pty Ltd.\n");
	printf("All rights reserved.\n\n");

	setlocale(LC_ALL, "");

	bzero(&server, sizeof(server));
	server.startTime = time(NULL);
	server.config.needFileLock = 1;
	server.config.hasBroker = 1;



	/*
	** This is the restarter loop.  We fork off a copy of ourselves and
	** wait for it to terminate.  If we've been told to run this in the
	** foreground then don't setup the restarter (for debugging etc)
	*/
	
	if (!rFlag)
	{
		daemonMain(&server, argc, argv);
		exit(0);
	}

	printf("\tStarting server in restarter loop. Restarter pid is %d\n", 
		getpid());

	while(1)
	{
		pid = fork();
		if (pid)
		{
			/* Parent */
			if (vFlag)
			{
				printf("\tRestarter is PID %d. "
					"Waiting for main broker process %d\n", 
					getpid(),pid);
			}

			waitpid(pid, &childStatus, 0);
			if (WEXITSTATUS(childStatus) == 0)
			{
				sleep(2);
				exit(1);
			}
			printf("Child terminated with status %d. "
				"Restarting in 30 sec\n", childStatus);
			sleep(30);
		}
		else
		{
			daemonMain(&server, argc, argv);
		}
	}
}
