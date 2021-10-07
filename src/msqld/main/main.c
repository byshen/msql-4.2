/*
** Copyright (c) 2002  Hughes Technologies Pty Ltd.  All rights
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
** $Id: main.c,v 1.58 2012/06/04 05:40:02 bambi Exp $
**
*/

/* #define DEBUG_BSD_MALLOC */
/* #define DEBUG_MEMTRACE */

/*
** Module	: main
** Purpose	: Normal single server backend
** Exports	: 
** Depends Upon	: 
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <common/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
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
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/file.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

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
#include <msqld/main/version.h>
#include <msqld/main/process.h>
#include <msqld/main/table.h>
#include <msqld/main/net.h>
#include <msqld/main/acl.h>
#include <msqld/main/cache.h>
#include <msqld/main/util.h>
#include <msqld/main/parse.h>
#include <msqld/main/tcp.h>
#include <msqld/main/memory.h>
#include <msqld/lock/lock.h>

#ifdef DEBUG_MEMTRACE
#  include </usr/local/include/memtrace.h>
#endif

/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


extern  u_char  *yytext,
		*yyprev;
extern	int	yylineno;
extern	int	yydebug;

extern	char	errMsg[];
extern	char	*packet;

int	curSock,
	cFlag = 0,
	vFlag = 0,
	rFlag = 0,
	eintrCount = 0,
	explainOnly;

msqld	*globalServer;

char	PROGNAME[] = "msqld",
	BLANK_ARGV[] = "                                                  ";


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






void processClientRequest(server, client)
	msqld		*server;
	int		client;
{
	mQuery_t	*query,
			dummyQuery;
	int		clientCommand,
			comSock;
	char		dbname[255],
			*arg, *arg2, *arg3,
			*cp,
			tmpChar;



	comSock = server->conArray[client].sock;
	if (netReadPacket(comSock) <= 0)
	{
		msqlDebug1(MOD_GENERAL,"Command read on sock %d failed!\n",
			comSock);
		clientCommand = QUIT;
	}
	else
	{
		clientCommand = atoi(packet);
	}
	msqlDebug3(MOD_GENERAL,"Command on sock %d = %d (%s)\n", comSock, 
		clientCommand, comTable[clientCommand]);

	switch(clientCommand)
	{
		case QUIT:
			msqlDebug0(MOD_GENERAL,"DB QUIT!\n");
			FD_CLR(comSock,&server->clientFDs);
			shutdown(comSock,2);
			close(comSock);
			if(server->conArray[client].user)
			{
				free(server->conArray[client].user);
				server->conArray[client].user = NULL;
			}
			if(server->conArray[client].host)
			{
				free(server->conArray[client].host);
				server->conArray[client].host = NULL;
			}
			if(server->conArray[client].db)
			{
				free(server->conArray[client].db);
				server->conArray[client].db = NULL;
			}
			server->conArray[client].sock = -1;
			server->numCons--;
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
			server->conArray[client].access = aclCheckAccess(
                                        dbname, server->conArray + client);
			if(server->conArray[client].access == NO_ACCESS)
			{
				netError(comSock,ACCESS_DENIED_ERROR);
				break;
			}
			if(server->conArray[client].db)
			{
				free(server->conArray[client].db);
				server->conArray[client].db = NULL;
			}
			server->conArray[client].db = (char *)strdup(dbname);
			if (utilCheckDB(server, dbname) < 0)
			{
				netError(comSock,errMsg);
				break;
			}
			netOK(comSock);
			break;

		case QUERY:
			if (!server->conArray[client].db)
			{
				netError(comSock,NO_DB_ERROR);
				break;
			}
			curSock = comSock;
			cp=(char *)(packet+2);
			arg = cp;
			tmpChar = 0;
			if (strlen(arg) > MAX_QUERY_LEN)
			{
				tmpChar = *(arg+MAX_QUERY_LEN);
				*(arg+MAX_QUERY_LEN) = 0;
			}
			msqlDebug1(MOD_QUERY,"Query = %s",arg);

			if (tmpChar)
				*(arg+MAX_QUERY_LEN) = tmpChar;
			/*aclSetPerms(RW_ACCESS);*/
			aclSetPerms(server->conArray[client].access);
			strncpy(server->queryBuf,arg,MSQL_GLOBAL_BUF_LEN);
			query = parseQuery(server,arg, comSock, 
				server->conArray[client].user, 
				server->conArray[client].db);
			if (query)
			{
				query->queryTime = time(NULL);
				processQuery(server, query, client, arg);
				parseCleanQuery(query);
			}
			break;

		case EXPLAIN:
			if (!server->conArray[client].db)
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
			query = parseQuery(server, arg, comSock, 
				server->conArray[client].user, 
				server->conArray[client].db);
			if (query)
			{
				query->explainOnly = 1;
				processQuery(server, query, client, arg);
				parseCleanQuery(query);
			}
			if(arg)
				free(arg);
			break;

		case DB_LIST:
			curSock = comSock;
			processListDBs(server, comSock);
			break;

		case SEQ_INFO:
			if (!server->conArray[client].db)
			{
				netError(comSock,NO_DB_ERROR);
				break;
			}
			cp = (char *)index(packet,':');
			cp=(char *)strtok(cp+1, "\n\r");
			arg = (char *)strdup(cp);
			curSock = comSock;
			processSequenceInfo(server, comSock, arg, 
				server->conArray[client].db);
			if(arg)
				free(arg);
			break;

		case TABLE_LIST:
			if (!server->conArray[client].db)
			{
				netError(comSock, NO_DB_ERROR);
				break;
			}
			curSock = comSock;
			processListTables(server, comSock, 
				server->conArray[client].db);
			break;

		case FIELD_LIST:
			if (!server->conArray[client].db)
			{
				netError(comSock,NO_DB_ERROR);
				break;
			}
			cp=(char *)strtok(packet+2, "\n\r");
			arg = (char *)strdup(cp);
			curSock = comSock;
			processListFields(server, comSock, arg, 
				server->conArray[client].db);
			if(arg)
				free(arg);
			break;

		case INDEX_LIST:
			if (!server->conArray[client].db)
			{
				netError(comSock,NO_DB_ERROR);
				break;
			}
			cp=(char *)strtok(packet+2, ":\n\r");
			arg = (char *)strdup(cp);
			cp=(char *)strtok(NULL,"\n\r");
			arg2 = (char *)strdup(cp);
			curSock = comSock;
			processListIndex(server, comSock, arg2, arg, 
				server->conArray[client].db);
			if(arg)
				free(arg);
			if(arg2)
				free(arg2);
			break;

		case TABLE_INFO:
			if (!server->conArray[client].db)
			{
				netError(comSock,NO_DB_ERROR);
				break;
			}
			cp=(char *)strtok(packet+3, "\n\r");
			arg = (char *)strdup(cp);
			curSock = comSock;
			processTableInfo(server, comSock, arg, 
				server->conArray[client].db);
			if(arg)
				free(arg);
			break;

               	case EXPORT:
			if (!server->conArray[client].db)
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
			dummyQuery.curUser = server->conArray[client].user;
			dummyQuery.curDB = server->conArray[client].db;
			processExportTable(server, comSock, 
				server->conArray[client].db, arg, arg2, arg3,
				NULL);
			if(arg)
				free(arg);
			if(arg2)
				free(arg2);
			if(arg3)
				free(arg3);
			break;

               	case IMPORT:
			if (!server->conArray[client].db)
			{
				netError(comSock,NO_DB_ERROR);
				break;
			}
			cp=(char *)strtok(packet+2, ":\n\r");
			arg = (char *)strdup(cp);
			cp=(char *)strtok(NULL,":\n\r");
			arg2 = (char *)strdup(cp);
			curSock = comSock;
			dummyQuery.curUser = server->conArray[client].user;
			dummyQuery.curDB = server->conArray[client].db;
			processImportTable(server, comSock, 
				server->conArray[client].db, arg, arg2, NULL);
			if(arg)
				free(arg);
			if(arg2)
				free(arg2);
			break;

		case CREATE_DB:
			if (!aclCheckLocal(&server->conArray[client]))
			{
				netError(comSock,PERM_DENIED_ERROR);
				break;
			}
			cp=(char *)strtok(packet+2, "\n\r");
			arg = (char *)strdup(cp);
			processCreateDB(server, comSock, arg);
			if(arg)
				free(arg);
			break;

	 	case COPY_DB:
			if (!aclCheckLocal(&server->conArray[client]))
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
			if (!aclCheckLocal(&server->conArray[client]))
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
			break;

		case DROP_DB:	
			if (!aclCheckLocal(&server->conArray[client]))
			{
				netError(comSock,PERM_DENIED_ERROR);
				break;
			}
			cp=(char *)strtok(packet+2, "\n\r");
			arg = (char *)strdup(cp);
			processDropDB(server,comSock,arg);
			if(arg)
				free(arg);
			break;

		case RELOAD_ACL:
			if (!aclCheckLocal(&server->conArray[client]))
			{
				netError(comSock,PERM_DENIED_ERROR);
				break;
			}
			aclReloadFile(comSock);
			netEndOfList(comSock);
			break;


		case SHUTDOWN:
			if (!aclCheckLocal(&server->conArray[client]))
			{
				netError(comSock,PERM_DENIED_ERROR);
				break;
			}
			netEndOfList(comSock);
			puntServer(-1);
			if (rFlag)
			{
				/* Kill the restarter loop */
				kill(getppid(), SIGKILL);
			}
			exit(0);
			break;

		case SERVER_STATS:
			if (!aclCheckLocal(&server->conArray[client]))
			{
				netError(comSock,PERM_DENIED_ERROR);
				break;
			}
			sendServerStats(server,comSock);
			netEndOfList(comSock);
			break;

		case CHECK_TABLE:	
			if (!aclCheckLocal(&server->conArray[client]))
			{
				netError(comSock,PERM_DENIED_ERROR);
				break;
			}
			cp=(char *)strtok(packet+2, ":\n\r");
			arg = (char *)strdup(cp);
			cp=(char *)strtok(NULL, "\n\r");
			arg2 = (char *)strdup(cp);
			processCheckTable(server,comSock,arg,arg2);
			if(arg)
				free(arg);
			if(arg2)
				free(arg2);
			break;

		default:
			netError(comSock, UNKNOWN_COM_ERROR);
			break;
	}
}


void debugTrap()
{
	/* We use this as a debugger breakpoint */
}

void childStartup()
{
	/*
	** The broker library makes reference to this routine.  Defined
	** a dummy one here.
	*/
}

void terminateChildren()
{
	/*
	** The signal handling code shared with the broker version
	** calls this so just define a dummy one here (the single
	** process server has no children)
	*/
}



int daemonMain(server)
	msqld   *server;

{
	int	sock,
		newSock,
		error,
		opt,
		count;
	char	*uname;
	fd_set	readFDs;


	/*
	** OK, on with the show
	*/

	setupServer(server);
	setupServerSockets(server);
	setupSignals();
	aclLoadFile(1);

	umask(0077);
	msqlDebug0(MOD_GENERAL,
		"miniSQL debug mode.  Waiting for connections.\n");

        FD_ZERO(&server->clientFDs);
	while(1)
	{
               	bcopy(&server->clientFDs, &readFDs, sizeof(readFDs));
		if (server->unixSock >= 0)
               		FD_SET(server->unixSock,&readFDs);
		if (server->ipSock >= 0)
               		FD_SET(server->ipSock,&readFDs);
                if(select(FD_SETSIZE,&readFDs,0,0,0) < 0)
                {
                       	if (errno == EINTR)
			{
				eintrCount++;
				if(eintrCount > 100)
				{
					printf(SELECT_EINTR_ERRORS);
					puntServer(0);
				}
                               	continue;
			}
                       	puntServer(0);
                }
		eintrCount = 0;

		/*
		** How about a new connection?  Grab the lock so we
		** can ensure the broker notification gets out before
		** the new connection becomes active
		*/
		sock = 0;
		if (server->unixSock>=0 && FD_ISSET(server->unixSock,&readFDs))
			sock = server->unixSock;
		if (server->ipSock >= 0 && FD_ISSET(server->ipSock,&readFDs))
			sock = server->ipSock;

		if (sock != 0)
		{
                        struct  sockaddr_un     cAddr;
                        int  cAddrLen;

			if (FD_ISSET(server->unixSock,&readFDs))
				sock = server->unixSock;
			else
				sock = server->ipSock;

			bzero(&cAddr, sizeof(cAddr));
			cAddrLen = sizeof(struct sockaddr_un);
			newSock = accept(sock, (struct sockaddr *)&cAddr, 
				(u_int*)&cAddrLen);
			if(newSock < 0)
			{
				perror("Error in accept ");
				puntServer(-1);
				exit(1);
			}
			if (newSock > server->maxSock)
				server->maxSock = newSock;
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
                                struct  hostent *hp;
				struct	sockaddr_in remote;
				char	*ipAddr;
                                int  	addrLen;

                                addrLen = sizeof(struct sockaddr);
                                getpeername(newSock, (struct sockaddr *)
                                        &remote, (u_int*)&addrLen);
                                ipAddr = inet_ntoa(remote.sin_addr);
                                if (ipAddr)
                                {
                                        server->conArray[newSock].clientIP =
                                                strdup(ipAddr);
                                }
                                if (configGetIntEntry("system","host_lookup")
					== 1)
                                {
                                        /*
                                        ** Validate remote host
                                        */

                                        if(auth_hostname(&remote))
                                        {
                                                netError(newSock,
                                                        BAD_HOST_ERROR);
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
                                        server->conArray[newSock].host=(char *)
                                                strdup("UNKNOWN_HOST");
                                        msqlDebug0(MOD_GENERAL,
                                                "Host=UNKNOWN_HOST\n");
                                }
                                else
                                {
                                        server->conArray[newSock].host=(char *)
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

			server->conArray[newSock].connectTime = 0;
			server->conArray[newSock].numQueries = 0;

			if (!error)
			{
				opt=1;
				setsockopt(newSock,SOL_SOCKET,SO_KEEPALIVE,
					(char *) &opt, sizeof(opt));
				/* fcntl(newSock, F_SETFL, O_NONBLOCK);*/
				snprintf(packet, PKT_LEN,
					"0:%d:%s\n",
					PROTOCOL_VERSION,SERVER_VERSION);
				netWritePacket(newSock);
				if (netReadPacket(newSock) <=0)
				{
					netError(newSock,HANDSHAKE_ERROR);
					server->numCons--;
					shutdown(newSock,2);
					close(newSock);
					server->conArray[newSock].host=NULL;
					server->conArray[newSock].clientIP=NULL;
				}
				else
				{
					FD_SET(newSock, &server->clientFDs);
					uname = (char *)strtok(packet,"\n");
					msqlDebug1(MOD_GENERAL,
						"User = %s\n",uname);
					if(server->conArray[newSock].user)
					   free(server->conArray[newSock].user);
					server->conArray[newSock].user = (char*)
						strdup(uname);
					server->conArray[newSock].sock =
						newSock;
					netEndOfList(newSock);
				}
			}
			continue;
		}

		/*
		** Looks like a request from an existing client
		*/
		count = 0;
		while(count <= server->maxSock)
		{
			if (server->conArray[count].sock == -1)
			{
				count++;
				continue;
			}
			if (!FD_ISSET(server->conArray[count].sock,&readFDs))
			{
				count++;
				continue;
			}
			processClientRequest(server, count);
			count++;
		}
	}
}



int main(argc,argv)
	int	argc;
	char	*argv[];
{
	msqld	server;
	int 	pid,
		childStatus,
		errFlag = 0,
		c;

        extern  char *optarg;

#ifdef 	DEBUG_MEMTRACE
	MemTrace_Init (argv[0], MEMTRACE_REPORT_ON_EXIT);
#endif


	/*
	** Handle the command line args
	*/
	bzero(&server, sizeof(server));
	*server.confFile = 0;
        while((c=getopt(argc,argv,"vrfc:"))!= -1)
        {
                switch(c)
                {
                        case 'c':
                                if (*server.confFile)
                                        errFlag++;
                                else
                                        strncpy(server.confFile,optarg,
						MSQL_PATH_LEN);
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
		usage("msqld");
		exit(1);
	}



        printf("\n\nMini SQL Version %s\n",SERVER_VERSION);
        printf("Copyright (c) 1993-94 David J. Hughes\n");
        printf("Copyright (c) 1995-2018 Hughes Technologies Pty Ltd.\n");
        printf("All rights reserved.\n\n");

        setlocale(LC_ALL, "");

	bzero(&server, sizeof(server));
	server.startTime = time(NULL);
	server.config.needFileLock = 0;
	server.config.hasBroker = 0;


	/*
	** This is the restarter loop.  We fork off a copy of ourselves and
	** wait for it to terminate.  If we've been told to run this in the
	** foreground then don't setup the restarter (for debugging etc)
	*/
	
	if (!rFlag)
	{
		daemonMain(&server);
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
				printf("Restarter is PID %d. "
					"Waiting for child %d\n", getpid(),pid);
			}

			waitpid(pid, &childStatus, 0);
			if (WEXITSTATUS(childStatus) == 0)
			{
				exit(1);
			}
			if (vFlag)
			{
				printf("Child terminated with status %d. "
					"Restarting in 30 sec\n", childStatus);
			}
			sleep(30);
		}
		else
		{
			daemonMain(&server);
		}
	}
}
