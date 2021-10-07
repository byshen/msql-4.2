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


/*
** Module	: main_common
** Purpose	: Routines that are common to the standard and broker mains
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


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


extern  msqld 	*globalServer;
extern  char    *packet;




RETSIGTYPE sigTrap(sig)
	int	sig;
{
	int		clientSock;
	char		path[MSQL_PATH_LEN + 1];

#ifdef WIN32
	fprintf(stderr,"\nExiting on signal\n\n");
#else
	signal(sig,SIG_IGN);
        fprintf(stderr,"Process %d exiting on a sig %d!\n", getpid(), sig);
#endif
	clientSock = 3;
	msqlDebug0(MOD_ANY,"Forced server shutdown due to bad signal!\n");
	while(clientSock < MAX_CONNECTIONS)
	{
		if (globalServer->conArray[clientSock].db)
		{
			printf("Forcing close on Socket %d\n",clientSock);
			shutdown(clientSock,2);
			close(clientSock);
		}
		clientSock++;
	}
	if (globalServer->unixSock >= 0)
	{
		shutdown(globalServer->unixSock,2);
		close(globalServer->unixSock);
		unlink(globalServer->unixPort);
		free(globalServer->unixPort);
	}
	/*
	** Close off the children
	*/
	sync();
	fprintf(stderr,"Closing backend connections.\n\n");
	terminateChildren();
	printf("\n");


#ifdef HAVE_SETRLIMIT
	{
        	struct rlimit   limit;
                limit.rlim_cur = limit.rlim_max = RLIM_INFINITY;
                setrlimit(RLIMIT_CORE, &limit);
	}
#endif
	if (*globalServer->queryBuf)
	{
		printf("Last query : %s\n\n", globalServer->queryBuf);
	}
	snprintf(path,MSQL_PATH_LEN, "%s/core",  INST_DIR);
	mkdir(path, 0666);
	chdir(path);
	abort();
}



void setupServer(server)
	msqld	*server;
{
	int	count,
		cacheSize,
		pidFD;
	char	*uname,
		pidBuf[10],
		path[MSQL_PATH_LEN + 1];
	struct  passwd *pwd;
	struct  stat sbuf;

#ifdef HAVE_SETRLIMIT
        struct  rlimit limit;
#endif


	/*
	** Setup the environment
	*/

	setbuf(stdout, NULL);
	setbuf(stdin, NULL);
        setlocale(LC_ALL, "");
	initDebug(1);
	netInitialise();



        /*
        ** Load in the runtime config
        */

        if (*server->confFile == 0)
        {
                snprintf(server->confFile,MSQL_PATH_LEN, "%s/etc/msql.conf",
			INST_DIR);
        }
        printf("\tLoading configuration from '%s'.\n",server->confFile);

        configLoadFile(server->confFile);
        globalServer = server;
        server->config.instDir=strdup(configGetCharEntry("general","inst_dir"));
        server->config.dbDir = strdup(configGetCharEntry("general", "db_dir"));
        server->config.readOnly = configGetIntEntry("system", "read_only");
        server->config.msyncTimer = configGetIntEntry("system", "msync_timer");
        server->config.sortMaxMem = configGetIntEntry("system","sort_max_mem");
        server->config.tableCache = configGetIntEntry("system","table_cache");
        server->config.cacheDescriptors = server->config.tableCache *
                (NUM_INDEX + 2);


	/* 
	** Make sure there's isn't a copy already running.  Do this before
	** we change to the msql user.
	*/
	(void)snprintf(path, MSQL_PATH_LEN, "%s", 
		(char *)configGetCharEntry("general","pid_file"));
	pidFD = open(path, O_CREAT|O_TRUNC|O_RDWR, 0700);
	if (pidFD < 0)
	{
		perror("Error : Couldn't open PID file");
		exit(1);
	}
        if (flock(pidFD, LOCK_EX | LOCK_NB) < 0)
        {
                printf("\nError : Can't get lock on the pid file.  "
			"Is mSQL already running?\n\n");
                exit(1);
        }
	sprintf(pidBuf,"%d", (int)getpid());
	write(pidFD, pidBuf, strlen(pidBuf) + 1);


	/*
	** Ensure there's no wierd FD's open.  Close of any FD under 256
	*/
	count = server->unixSock + 1;
	if (count < 5)
	{
		count=5;
	}
	while(count <= 255)
	{
		close(count);
		count++;
	}


	/*
	** Initialise internal data structures
	*/

	bzero(&server->conArray, MAX_CONNECTIONS * sizeof(cinfo_t));
	cacheSetupTableCache(server);
	server->numCons = 0;
	server->maxSock = 0;
	server->numQueries = 0;

        count = 0;
        while (count < MAX_CONNECTIONS)
        {
                server->conArray[count].sock = -1;
                server->conArray[count].db = NULL;
                count++;
        }


	/*
	** Are we running as the right user?
	*/
	pwd = getpwuid(getuid());
	if (!pwd)
	{
		printf("\nError!  No username for our UID (%d)\n\n",
			(int)getuid());
		exit(1);
	}
	uname = (char *)configGetCharEntry("general","msql_user");
	if (strcmp(uname,pwd->pw_name) != 0)
	{
		pwd = getpwnam(uname);
		if (!pwd)
		{
			printf("\nError!  Unknown username (%s)\n\n", uname);
			exit(1);
		}
		chown(path,pwd->pw_uid, pwd->pw_gid);
		chown("/proc/self/fd",pwd->pw_uid,pwd->pw_gid);
		if (setuid(pwd->pw_uid) < 0)
		{
			printf("\nError!  Can't run as user '%s'\n\n",uname);
			exit(1);
		}
	}
	printf("\tServer running as user '%s'.\n",uname);
	printf("\tServer mode is %s.\n", configGetIntEntry("system",
		"read_only")?"Read-Only":"Read/Write");

	/*
	** Ensure that the correct user owns the database files
	*/
	snprintf(path,MSQL_PATH_LEN, "%s/msqldb",
		(char *)configGetCharEntry("general","inst_dir"));
	if (stat(path,&sbuf) < 0)
	{
		printf("\nError!  Can't stat '%s'\n\n",path);
		exit(1);
	}

	if (sbuf.st_uid != getuid())
	{
		printf("\nError!  '%s' is not owned by '%s'\n\n",path,
			pwd->pw_name);
		exit(1);
	}

	umask(0);
	tableCleanTmpDir(server);


	/*
	** Check out the file descriptor limit
	**
	** Some boxes have broken getrlimit() implementations (or
	** missing RLIMIT_NOFILE) so we try to use sysconf first
	**
	** Watch out for broken BSDI here.  It looks like BSDI returns
	** the kernel MAX_FILES not the per process MAX_FILE in
	** getrlimit() which kinda sucks if you want to use select()!
	*/

#	ifdef HAVE_RLIMIT_NOFILE
		getrlimit(RLIMIT_NOFILE,&limit);
		limit.rlim_cur = (limit.rlim_max > 512)? 512 : limit.rlim_max;
		setrlimit(RLIMIT_NOFILE,&limit);
		getrlimit(RLIMIT_NOFILE,&limit);
		server->maxCons = limit.rlim_cur - 
			server->config.cacheDescriptors;
#	else
#	ifdef HAVE_SYSCONF
		server->maxCons = sysconf(_SC_OPEN_MAX) -
			server->config.cacheDescriptors;
#  	else
		server->maxCons = 64 - server->config.cacheDescriptors;
		printf("\tServer can accept %d connections.\n",server->maxCons);
#	endif
#	endif


	if (server->maxCons > 254)
	{
		server->maxCons = 254;
	}
	printf("\tServer process reconfigured to accept %d connections.\n", 
		server->maxCons);



	/*
	** Check the table cache.  If we don't have at least 2 entries
	** then we can't do a join.
	*/
	cacheSize = configGetIntEntry("system", "table_cache");
	if (cacheSize >= 2)
	{
		printf("\tServer table cache holds %d entries.\n", cacheSize);
	}
	else
	{
		printf("\tServer table cache config too small. "
			"Must be at least 2.\n");
		exit(1);
	}


	/*
	** Set up query logging if required
	*/
        server->logFP = NULL;
        if (configGetIntEntry("system","query_log"))
        {
                server->logFP = fopen(configGetCharEntry("system",
			"query_log_file"),"a");
                if (server->logFP)
		{
                        printf("\tQuery logging enabled.\n");
			setbuf(server->logFP, NULL);
		}
                else
		{
                        printf("\tERROR : Open of query log failed!\n");
		}
	}

        server->updateFP = NULL;
        if (configGetIntEntry("system","update_log"))
        {
                server->updateFP = fopen(configGetCharEntry("system",
			"update_log_file"),"a");
                if (server->updateFP)
		{
                        printf("\tUpdate logging enabled.\n");
			setbuf(server->updateFP, NULL);
		}
                else
		{
                        printf("\tERROR : Open of update log failed!\n");
		}
	}
}



/****************************************************************************
** 	_setupServerSockets
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

void setupServerSockets(server)
	msqld	*server;
{
	int	tcpPort,
		opt;
	struct	sockaddr_un	unixAddr;
	struct	sockaddr_in	ipAddr;



        /*
        ** Create an IP socket
        */
        if (configGetIntEntry("system", "remote_access"))
        {
                tcpPort = configGetIntEntry("general", "tcp_port");
                msqlDebug1(MOD_GENERAL,"IP Socket is %d\n",tcpPort);
                server->ipSock = socket(AF_INET, SOCK_STREAM, 0);
                if (server->ipSock < 0)
                {
                        perror("Can't start server : IP Socket ");
			exit(1);
                }
#               ifdef SO_REUSEADDR
                opt = 1;
                setsockopt(server->ipSock, SOL_SOCKET, SO_REUSEADDR, 
			(char *)&opt, sizeof(int));  
#               endif
         
                bzero(&ipAddr, sizeof(ipAddr));
                ipAddr.sin_family = AF_INET;
                ipAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                ipAddr.sin_port = htons((u_short)tcpPort);
                if (bind(server->ipSock,(struct sockaddr *)&ipAddr,
			sizeof(ipAddr)) < 0)
                {
			close(server->ipSock);
                        perror("Can't start server : IP Bind ");
			exit(1);
                }
                listen(server->ipSock,128);
        }
        else
        {
                server->ipSock = -1;
        }


	/*
	** Setup the UNIX domain socket
	*/
	server->unixPort = strdup(configGetCharEntry("general","unix_port"));
	msqlDebug1(MOD_GENERAL,"UNIX Socket is %s\n", server->unixPort);
	server->unixSock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server->unixSock < 0)
	{
		perror("Can't start server : UNIX Socket ");
		exit(1);
	}
	bzero(&unixAddr, sizeof(unixAddr));
	unixAddr.sun_family = AF_UNIX;
	strcpy(unixAddr.sun_path, server->unixPort);
	unlink(server->unixPort);
	if( bind(server->unixSock, (struct sockaddr *)&unixAddr,
		sizeof(unixAddr)) < 0)
	{
		close(server->unixSock);
		perror("Can't start server : UNIX Bind ");
		exit(1);
	}
	listen(server->unixSock,128);
	chmod(server->unixPort,0777);
}



RETSIGTYPE puntServer(sig)
	int	sig;
{
	int	clientSock;

	/*
	** make sure that we don't try to start shutting down due to a signal
	** if we are already in the process of shutting down
	*/
	if (globalServer->inServerShutdown)
	{
		return;
	}
	globalServer->inServerShutdown = 1;

	signal(sig, SIG_IGN);
	clientSock = 3;
	if (sig == -1)
	{
		printf("\n\nNormal Server shutdown!\n\n");
	}
	else
	{
		if (sig != SIGUSR1)
		{
			printf("\nServer Aborting (PID %d)!\n",getpid());
		}
	}
	while(clientSock < MAX_CONNECTIONS)
	{
		if (globalServer->conArray[clientSock].db)
		{
			printf("Forcing close on Socket %d\n",clientSock);
			shutdown(clientSock,2);
			close(clientSock);
		}
		clientSock++;
	}
	if (globalServer->unixSock >= 0)
	{
		shutdown(globalServer->unixSock,2);
		close(globalServer->unixSock);
		unlink(globalServer->unixPort);
		if(globalServer->unixPort)
		{
			free(globalServer->unixPort);
		}
		globalServer->unixPort = NULL;
	}
	printf("\n");
	cacheDropTableCache();
	signal(SIGINT, SIG_IGN);
	if (sig == -1)
	{
		killpg(getpgid(0), SIGUSR1);
	}
	else
	{
		killpg(getpgid(0), SIGINT);
	}

	if (debugSet(MOD_MALLOC))
	{
		/*
		**
		fprintf(stderr,"\n\nmalloc() leak detection .....\n");
		checkBlocks(MALLOC_BLK);
		**
		*/
	}
	if (debugSet(MOD_MMAP))
	{
		/*
		**
		fprintf(stderr,"\n\nmmap() leak detection .....\n");
		checkBlocks(MMAP_BLK);
		**
		*/
	}
	memDropCaches();
	if (globalServer->config.hasBroker)
	{
		printf("Daemon process %u Shutdown Complete.\n", getpid());
	}
	else
	{
		printf("\n\nmSQL Daemon Shutdown Complete.\n\n");
	}
	if (sig >= 0)
	{
		if(*globalServer->queryBuf)
		{
			printf("Last query executed : %s\n\n", 
				globalServer->queryBuf);
		}
		exit(1);
	}
	return;
}




void freeClientConnection(server, sock)
	msqld	*server;
	int	sock;
{
	if(server->conArray[sock].db)
	{
		free(server->conArray[sock].db);
		server->conArray[sock].db = NULL;
	}
	if(server->conArray[sock].host)
	{
		free(server->conArray[sock].host);
		server->conArray[sock].host = NULL;
	}
	if(server->conArray[sock].user)
	{
		free(server->conArray[sock].user);
		server->conArray[sock].user = NULL;
	}
	server->conArray[sock].sock = -1;
	server->numCons--;
}


RETSIGTYPE puntClient(sig)
	int	sig;
{
	extern	int netCurrentSock;

	if(globalServer->conArray[netCurrentSock].sock < 0)
	{
		/* Must have already been "force closed" */
		return;
	}
	if (sig)
	{
		signal(sig, puntClient);
		msqlDebug1(MOD_GENERAL,
			"Forced close of client on socket %d due to pipe sig\n",
			netCurrentSock);
	}
	else
	{
		msqlDebug1(MOD_GENERAL,
			"Closing client on socket %d\n", netCurrentSock);
	}
	shutdown(netCurrentSock,2);
	close(netCurrentSock);
	FD_CLR(netCurrentSock,&(globalServer->clientFDs));
	freeClientConnection(globalServer, netCurrentSock);
	return;
}


static char *calcUptime(server)
	msqld	*server;
{
	int	days,
		hours,
		mins,
		secs;
	u_int	tmp,
		uptime;
	static 	char msg[80];

	tmp = uptime = time(NULL) - server->startTime;
	days = tmp / 3600 / 24;
	tmp -= (days * 3600 * 24);
	hours = tmp  / 3600;
	tmp -= hours * 3600;
	mins = tmp / 60;
	tmp -= mins * 60;
	secs = tmp;
	snprintf(msg,80,"%d days, %d hours, %d mins, %d secs",
		days, hours, mins, secs);
	return(msg);
}

void sendServerStats(server, sock)
	msqld	*server;
	int	sock;
{
	struct	passwd *pw;
	int	uid,
		loop,
		cTime;
	u_int	curTime = time(NULL);
	char	buf[10];

	snprintf(packet,PKT_LEN, "%s%s\n%s\n%s\n%s\n\n", 
		"Mini SQL Version ", SERVER_VERSION,
		"Copyright (c) 1993-94 David J. Hughes",
		"Copyright (c) 1995-2018 Hughes Technologies Pty Ltd.",
		"All rights reserved.");
	netWritePacket(sock);
	snprintf(packet,PKT_LEN,"Config file      : %s\n", server->confFile);
	netWritePacket(sock);
	snprintf(packet,PKT_LEN,"Max connections  : %d\n", server->maxCons);
	netWritePacket(sock);
	snprintf(packet,PKT_LEN,"Cur connections  : %d\n", server->numCons);
	netWritePacket(sock);
	snprintf(packet,PKT_LEN,"Backend processes: %d\n", server->numKids);
	netWritePacket(sock);

#if defined(_OS_UNIX) || defined(_OS_OS2)
	uid = getuid();
	pw = getpwuid(uid);
	if (pw != NULL)
	{
		snprintf(packet,PKT_LEN,"Running as user  : %s\n", pw->pw_name);
	}
	else
	{
		snprintf(packet,PKT_LEN,"Running as user  : UID %d\n", uid);
	}
	netWritePacket(sock);
#endif

	snprintf(packet,PKT_LEN,"Server uptime    : %s\n", calcUptime(server));
	netWritePacket(sock);
	snprintf(packet,PKT_LEN,"Connection count : %d\n", server->numCons);
	netWritePacket(sock);

	strcpy(packet, "\nConnection table :\n");
	netWritePacket(sock);
	loop = 0;
	strcpy(packet,
	    "  Sock    Username       Hostname        Database    Connect   Idle   Queries\n");
	strcat(packet,
	    " +-----+------------+-----------------+------------+---------+------+--------+\n");
	netWritePacket(sock);
	while(loop < server->maxCons)
	{
		if (server->conArray[loop].user)
		{
			cTime = (int)(curTime - server->conArray[loop].connectTime)/60;
			snprintf(buf,sizeof(buf),
				"%2dH %2dM",cTime/60, cTime%60);
			snprintf(packet,PKT_LEN,
			 " | %3d | %-10s | %-15s | %-10s | %s | %4d | %6d |\n",
			    loop, server->conArray[loop].user, 
			    server->conArray[loop].host ?
				server->conArray[loop].host:"UNIX Sock",
			    server->conArray[loop].db ? 
				server->conArray[loop].db : "No DB",
			    buf,
			/*
			    (int)(curTime - conArray[loop].lastQuery)/60,
			*/ 
			    0,
			    server->conArray[loop].numQueries);
			netWritePacket(sock);
		}
		loop++;
	}
	strcpy(packet,
	    " +-----+------------+-----------------+------------+---------+------+--------+\n");
	netWritePacket(sock);

	strcpy(packet,"\n");
	netWritePacket(sock);
}


void logQuery(logFP,con, query) 
	FILE	*logFP;
	cinfo_t	*con;
	char	*query;
{
	char	dateBuf[80];
	struct  tm *locTime;
	time_t	clock;

	clock = time(NULL);
        locTime = localtime(&clock);
        strftime(dateBuf,sizeof(dateBuf),"%d-%b-%Y %H:%M:%S",locTime);

	fprintf(logFP,"%s %s %s %s %d\n%s\n",
		dateBuf,
		con->user?con->user:"NO_USER",
		con->host?con->host:"UNIX_SOCK",
		con->db?con->db:"NO_DB",
		query?(int)strlen(query):13,
		query?query:"MISSING_QUERY");
	fflush(logFP);
}


void usage(prog)
	char	*prog;
{
        printf("\n\nMini SQL Version %s\n\n",SERVER_VERSION);
	printf("Usage :  %s -rv -c ConfFile\n\n", prog);
	printf("\tv : Verbose mode\n");
	printf("\tr : Run in a restarter process\n");
	printf("\tc : Load specified config file\n\n");
}







RETSIGTYPE childHandler(sig)
	int	sig;
{
	int	clientSock;


	/*
	** make sure that we don't try to start shutting down due to a signal
	** if we are already in the process of shutting down
	*/
	if (globalServer->inServerShutdown)
	{
		return;
	}
	globalServer->inServerShutdown = 1;

	printf("\n\nBackend process exited!  Terminating server.\n");
	clientSock = 3;
	while(clientSock < MAX_CONNECTIONS)
	{
		if (globalServer->conArray[clientSock].db)
		{
			printf("Forcing close on Socket %d\n",clientSock);
			shutdown(clientSock,2);
			close(clientSock);
		}
		clientSock++;
	}
	if (globalServer->ipSock >= 0)
	{
		shutdown(globalServer->ipSock,2);
		close(globalServer->ipSock);
	}
	if (globalServer->unixSock >= 0)
	{
		shutdown(globalServer->unixSock,2);
		close(globalServer->unixSock);
		unlink(globalServer->unixPort);
		free(globalServer->unixPort);
	}
	/*
	** Close off the children
	*/
	sync();
	fprintf(stderr,"Closing backend connections.\n\n");
	terminateChildren();
	printf("\n");
	exit(1);
}



void setupSignals()
{
	signal(SIGSEGV,sigTrap);
	signal(SIGBUS,sigTrap);
	signal(SIGUSR1,puntServer);
	signal(SIGINT,puntServer);
	signal(SIGQUIT,puntServer);
	signal(SIGKILL,puntServer);
	signal(SIGPIPE,puntClient);
	signal(SIGTERM,puntServer);
	signal(SIGCHLD,childHandler);
	signal(SIGHUP,SIG_IGN);
	signal(SIGCONT, SIG_IGN);
}


