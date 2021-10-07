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
** $Id: libmsql.c,v 1.14 2011/11/22 11:47:18 bambi Exp $
**
*/

/*
** Module	:  libmsql
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
#include <sys/param.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include <common/portability.h>


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/msql_defs.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/version.h>

#include <signal.h>
#include <pwd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#if defined(_OS_WIN32)
#  include <winsock.h>
#endif
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#ifdef HAVE_STDARG_H
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#define	MSQL3_NEED_TYPE_NAMES

#include "msql.h"
#include "net_client.h"

/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

#ifndef	INADDR_NONE
#  define INADDR_NONE	-1
#endif
#define	ERR_BUF_LEN	200
#define	resetError()	bzero(msqlErrMsg,sizeof(msqlErrMsg))
#define chopError()	{ char *cp; cp = msqlErrMsg+strlen(msqlErrMsg) -1; \
				if (*cp == '\n') *cp = 0;}
extern	char	*clientPacket;

static	char	serverInfo[80],
		hostInfo[80];
static	int	curServerSock,
		numFields,
		queryTableSize,
		fieldTableSize,
		protoInfo,
		initialised;
static	m_data *tmpDataStore = NULL,
		*queryData = NULL,
		*fieldData = NULL;

#ifdef msqlErrMsg
# undef msqlErrMsg
#endif
static char	msqlErrMsg[ERR_BUF_LEN];

#define	MOD_QUERY	1
#define MOD_API		2
#define MOD_MALLOC	4
#define MOD_ERROR	8
#define MOD_INFO	16


static int 	debugLevel = 0,
		debugInit = 0;

/**************************************************************************

** PRIVATE ROUTINES
**************************************************************************/

static void _initialiseApi()
{
	initNet();
	initialised = 1;
}




static void _initDebug()
{
	char	*env,
		*tmp,
		*tok;

	env = getenv("MSQL_DEBUG");
	if(env)
	{
		tmp = (char *)strdup(env);
	}
	else
		return;
	printf("\n-------------------------------------------------------\n");
	printf("MSQL_DEBUG found. libmsql started with the following:-\n\n");
	tok = (char *)strtok(tmp,":");
	while(tok)
	{
		if (strcmp(tok,"msql_query") == 0)
		{
			debugLevel |= MOD_QUERY;
			printf("Debug level : query\n");
		}
		if (strcmp(tok,"msql_api") == 0)
		{
			debugLevel |= MOD_API;
			printf("Debug level : api\n");
		}
		if (strcmp(tok,"msql_malloc") == 0)
		{
			debugLevel |= MOD_MALLOC;
			printf("Debug level : malloc\n");
		}
		tok = (char *)strtok(NULL,":");
	}
	safeFree(tmp);
	printf("\n-------------------------------------------------------\n\n");
}




/*
** A cache of m_fdata structs so we don't malloc and free them continually
*/
#define FD_CACHE_LEN	48
static m_fdata	*fieldDataCache[FD_CACHE_LEN];
static int fdCount;

static m_fdata *mallocFieldDataStruct()
{
	m_fdata	*new;

	/*
	** Get us a struct either from the cache or a fresh malloc
	*/
	if (fdCount > 0)
	{
		fdCount--;
		new = fieldDataCache[fdCount];
		fieldDataCache[fdCount] = NULL;
	}
	else
	{
		new = (m_fdata *)malloc(sizeof(m_fdata));
	}

	/*
	** Initialise the fields
	*/
	new->field.table = new->field.name = NULL;
	new->field.type = new->field.length = new->field.flags = 0;
	new->next = NULL;
	return(new);
}


static void freeFieldDataStruct(ptr)
	m_fdata *ptr;
{
	if (fdCount >= FD_CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		fieldDataCache[fdCount] = ptr;
		fdCount++;
	}
}


/*
** A cache of m_data structs so we don't malloc and free them continually
*/
#define MD_CACHE_LEN	48
static m_data	*dataCache[MD_CACHE_LEN];
static int dCount;

static m_data *mallocDataStruct()
{
	m_data	*new;

	/*
	** Get us a struct either from the cache or a fresh malloc
	*/
	if (dCount > 0)
	{
		dCount--;
		new = dataCache[dCount];
		dataCache[dCount] = NULL;
	}
	else
	{
		new = (m_data *)malloc(sizeof(m_data));
	}

	/*
	** Initialise the fields
	*/
	return(new);
}


static void freeDataStruct(ptr)
	m_data *ptr;
{
	if (dCount >= MD_CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		dataCache[dCount] = ptr;
		dCount++;
	}
}

/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



char *APIENTRY msqlGetErrMsg(msg)
	char	*msg;
{
	if (!msg)
		return((char *)&msqlErrMsg);
	strncpy(msg, msqlErrMsg, strlen(msqlErrMsg));
	return(msg);
}






#ifdef HAVE_STDARG_H
static void msqlDebug(int module, ...)
#else
static void msqlDebug(va_alist)
	va_dcl
#endif
{
		va_list args;
	char	msg[1024],
		*fmt;

#ifdef HAVE_STDARG_H
	va_start(args, module);
#else
	int	module;

	va_start(args);
	module = (int) va_arg(args, int );
#endif

	if (! (module & debugLevel))
	{
		va_end(args);
		return;
	}

	fmt = (char *)va_arg(args, char *);
	if (!fmt)
        	return;
	(void)vsprintf(msg,fmt,args);
	va_end(args);
	fprintf(stderr,"[libmsql] %s",msg);
	fflush(stderr);
}


/**************************************************************************
**	_setServerSock 	
**
**	Purpose	: Store the server socket currently in use
**	Args	: Server socket
**	Returns	: Nothing
**	Notes	: The current socket is stored so that the signal
**		  handlers know which one to shut down.
*/

static void setServerSock(sock)
	int	sock;
{
	curServerSock = sock;
}


 

/**************************************************************************
**	_closeServer 	
**
**	Purpose	: Shut down the server connection
**	Args	: Server socket
**	Returns	: Nothing
**	Notes	: This is used by msqlClose and the signal handlers
*/

static void closeServer(sock)
	int	sock;
{
	msqlDebug(MOD_API,"Server socket (%d) closed\n", sock);
	shutdown(sock,2);
	close(sock);
	setServerSock(-1);
}





/**************************************************************************
**	_msqlClose
**
**	Purpose	: Send a QUIT to the server and close the connection
**	Args	: Server socket
**	Returns	: Nothing
**	Notes	: 
*/

void APIENTRY msqlClose(sock)
	int	sock;
{
	if (!initialised)
		_initialiseApi();
	setServerSock(sock);
	snprintf(clientPacket,PKT_LEN,"%d:\n",QUIT);
	netClientWritePacket(sock);
	closeServer(sock);
}





/**************************************************************************
**	_pipeHandler
**
**	Purpose	: Close the server connection if we get a SIGPIPE
**	Args	: sig
**	Returns	: Nothing
**	Notes	: 
*/

RETSIGTYPE pipeHandler(sig)
	int	sig;
{
	msqlDebug(MOD_API,"Hit by pipe signal\n");
	if (curServerSock > -1)
	{
		closeServer(curServerSock);
	}
	signal(SIGPIPE,pipeHandler);
	return;
}




static void freeQueryData(cur)
	m_data	*cur;
{
	m_data	*prev;
	int	offset;

	while(cur)
	{
		offset = 0;
		while(offset < cur->width)
		{
			safeFree(cur->data[offset]);
			offset++;
		}
		safeFree(cur->data);
		prev = cur;
		cur = cur->next;
		msqlDebug(MOD_MALLOC, "Query data row - free @ %X\n", prev);
		/*safeFree(prev);*/
		freeDataStruct(prev);
	}
}





static void freeFieldList(fieldData)
	m_fdata	*fieldData;
{
	m_fdata	*cur,
		*prev;

	cur = fieldData;
	while(cur)
	{
		prev = cur;
		cur = cur->next;
		safeFree(prev->field.table);
		safeFree(prev->field.name);
		msqlDebug(MOD_MALLOC, "Field List Entry- free @ %X\n", prev);
		freeFieldDataStruct(prev);
	}
}



/**************************************************************************
**	_msqlFreeResult
**
**	Purpose	: Free the memory allocated to a table returned by a select
**	Args	: Pointer to head of table
**	Returns	: Nothing
**	Notes	: 
*/

void APIENTRY msqlFreeResult(result)
	m_result  *result;
{
	freeQueryData(result->queryData);
	freeFieldList(result->fieldData);
	msqlDebug(MOD_MALLOC,"Result Handle - Free @ %X\n",result);
	safeFree(result);
}

		

static m_fdata *tableToFieldList(data)
	m_data	*data;
{
	m_data	*curRow;
	m_fdata	*curField,
		*prevField = NULL,
		*head = NULL;

	curRow = data;
	while(curRow)
	{
		/*curField = (m_fdata *)malloc(sizeof(m_fdata));*/
		curField = mallocFieldDataStruct();
		/*msqlDebug(MOD_MALLOC,"Field List Entry - malloc @ %X of %d\n",
			curField, sizeof(m_fdata));*/
		/*bzero(curField, sizeof(m_fdata));*/
		if (head)
		{
			prevField->next = curField;
			prevField = curField;
		}
		else
		{
			head = prevField = curField;
		}

		curField->field.table = (char *)strdup((char *)curRow->data[0]);
		curField->field.name = (char *)strdup((char *)curRow->data[1]);
		curField->field.type = atoi((char*)curRow->data[2]);
		curField->field.length = atoi((char*)curRow->data[3]);
		curField->field.flags = 0;
		if (*curRow->data[4] == 'Y')
			curField->field.flags |= NOT_NULL_FLAG;
		if (*curRow->data[5] == 'Y')
			curField->field.flags |= UNIQUE_FLAG;
		curRow = curRow->next;
	}
	return(head);
}


/**************************************************************************
**	_msqlConnect
**
**	Purpose	: Form a connection to a mSQL server
**	Args	: hostname of server
**	Returns	: socket for further use.  -1 on error
**	Notes	: If host == NULL, localhost is used via UNIX domain socket
*/

#if defined(_OS_OS2) || defined(_OS_WIN32)

int APIENTRY msqlConnect(host)
        char    *host;
{
        return msqlUserConnect( host, NULL );
}

int APIENTRY msqlUserConnect(host, user)
        char    *host;
        char    *user;
#else
int msqlConnect(host)
        char    *host;
#endif
{
	char	*cp,
		*unixPort;
	struct	sockaddr_in IPaddr;

#ifdef HAVE_SYS_UN_H
	struct	sockaddr_un UNIXaddr;
#endif
	struct	hostent *hp;
	u_long	IPAddr;
	int	opt,
		sock,
		version,
		tcpPort;
	struct	passwd *pw;


	resetError();
	initNet();
	if (!debugInit)
	{
		debugInit++;
		_initDebug();
	}
	configLoadFile(NULL);

	/*
	** Grab a socket and connect it to the server
	*/

#ifndef HAVE_SYS_UN_H
	if (!host)
	{
		host = "127.0.0.1";
	}
#endif

	if (!host)
	{
#ifdef HAVE_SYS_UN_H
		/* Shouldn't get in here with UNIX socks */
	        unixPort = configGetCharEntry("general","unix_port");
		strcpy(hostInfo,"Localhost via UNIX socket");
		msqlDebug(MOD_API,"Server name = NULL.  Using UNIX sock(%s)\n",
			unixPort);
		sock = socket(AF_UNIX,SOCK_STREAM,0);
		if (sock < 0)
		{
			snprintf(msqlErrMsg,MAX_ERR_MSG,SOCK_ERROR);
			return(-1);
		}
		setServerSock(sock);
		opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, 
			sizeof(opt));

		bzero(&UNIXaddr,sizeof(UNIXaddr));
		UNIXaddr.sun_family = AF_UNIX;
		strcpy(UNIXaddr.sun_path, unixPort);
		if(connect(sock,(struct sockaddr *) &UNIXaddr, 
			sizeof(UNIXaddr))<0)
		{
			snprintf(msqlErrMsg,MAX_ERR_MSG,CONNECTION_ERROR);
			close(sock);
			return(-1);
		}
#endif
	}
	else
	{
	        tcpPort = configGetIntEntry("general","tcp_port");
		snprintf(hostInfo,sizeof(hostInfo),"%s via TCP/IP",host);
		msqlDebug(MOD_API,"Server name = %s.  Using TCP sock (%d)\n",
			host, tcpPort);
		sock = socket(AF_INET,SOCK_STREAM,0);
		if (sock < 0)
		{
			snprintf(msqlErrMsg,MAX_ERR_MSG,IPSOCK_ERROR);
			return(-1);
		}
		setServerSock(sock);
		opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, 
			sizeof(opt));

		bzero(&IPaddr,sizeof(IPaddr));
		IPaddr.sin_family = AF_INET;

		/*
		** The server name may be a host name or IP address
		*/
	
		if ((IPAddr = inet_addr(host)) != INADDR_NONE)
		{
			bcopy(&IPAddr,&IPaddr.sin_addr,sizeof(IPAddr));
		}
		else
		{
			hp = gethostbyname(host);
			if (!hp)
			{
				snprintf(msqlErrMsg,MAX_ERR_MSG,
					UNKNOWN_HOST,
					host);
				close(sock);
				return(-1);
			}
			bcopy(hp->h_addr,&IPaddr.sin_addr, hp->h_length);
		}
		IPaddr.sin_port = htons((u_short)tcpPort);
		if(connect(sock,(struct sockaddr *) &IPaddr, 
			sizeof(IPaddr))<0)
		{
			snprintf(msqlErrMsg,MAX_ERR_MSG,CONN_HOST_ERROR,host);
			close(sock);
			return(-1);
		}
	}

	signal(SIGPIPE,pipeHandler);
	msqlDebug(MOD_API,"Connection socket = %d\n",sock);

	/*
	** Check the greeting message and save the version info
	*/

	if(netClientReadPacket(sock) <= 0)
	{
		msqlDebug(MOD_API,"Initial read failed\n");
		perror("read");
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  First check the status code from the
	** server and then the protocol version.  If the status == -1
	** or the protocol doesn't match our version, bail out!
	*/

	if (atoi(clientPacket) == -1)  
	{
		if ((cp = (char *)index(clientPacket,':')))
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		closeServer(sock);
		return(-1);
	}

	
	cp = (char *)index(clientPacket,':');
	if (!cp)
	{
		strcpy(msqlErrMsg,PACKET_ERROR);
		closeServer(sock);
		return(-1);
	}
	version = atoi(cp + 1);
	if (version != PROTOCOL_VERSION) 
	{
		snprintf(msqlErrMsg, MAX_ERR_MSG, VERSION_ERROR, version, 
			PROTOCOL_VERSION);
		closeServer(sock);
		return(-1);
	}
	msqlDebug(MOD_API,"mSQL protocol version - API=%d, server=%d\n",
		PROTOCOL_VERSION, version);
	protoInfo = version;
	cp = (char *)index(cp+1,':');
	if (cp)
	{
		strcpy(serverInfo,cp+1);
		if (*(serverInfo+strlen(cp+1)-1) == '\n')
		{
			*(serverInfo+strlen(cp+1)-1) = 0;
		}
		msqlDebug(MOD_API,"Server greeting = '%s'\n",cp+1);
	}
	else
	{
		strcpy(serverInfo,"Error in server handshake!");
	}

	/*
	** Send the username for this process for ACL checks
	*/
#if defined(_OS_WIN32) || defined(_OS_OS2)
	if (!user || *user == '\0')
	{
		char	*psz;

		psz = getenv("USER");
		if (!psz)
			psz = "unknown";
		(void)snprintf(clientPacket,PKT_LEN, "%s\n", psz );
	}
	else
	{
		(void)snprintf(clientPacket,PKT_LEN,"x_%s\n", user );
	}
#else
		
	pw = getpwuid(geteuid());
	if (!pw)
	{
		char	*psz;

		psz = getenv("USER");
		if (!psz)
		{
			strcpy(msqlErrMsg,USERNAME_ERROR);
			closeServer(sock);
			return(-1);
		}
		(void)snprintf(clientPacket,PKT_LEN,"%s\n",psz);
	}
	else
		(void)snprintf(clientPacket,PKT_LEN,"%s\n",pw->pw_name);
#endif
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result
	*/

	if (atoi(clientPacket) == -1)
	{
		char	*cp;

		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		closeServer(sock);
		return(-1);
	}
	return(sock);
}






/**************************************************************************
**	_msqlInitDB
**
**	Purpose	: Tell the server which database we want to use
**	Args	: Server sock and DB name
**	Returns	: -1 on error
**	Notes	: 
*/

int APIENTRY msqlSelectDB(sock,db)
	int	sock;
	char	*db;
{

        msqlDebug(MOD_API,"Select Database = \"%s\"\n",db);

	resetError();
	setServerSock(sock);
	
	/*
	** Issue the init DB command
	*/

	if (!initialised)
		_initialiseApi();
	(void)snprintf(clientPacket,PKT_LEN,"%d:%s\n",INIT_DB,db);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result
	*/

	if (atoi(clientPacket) == -1)
	{
		char	*cp;

		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}

	return(0);
}





/**************************************************************************
**	_msqlStoreResult
**
**	Purpose	: Store the data returned from a query
**	Args	: None
**	Returns	: Result handle or NULL if no data
**	Notes	: 
*/

m_result * APIENTRY msqlStoreResult()
{
	m_result *tmp;

	if (!queryData && !fieldData)
	{
		return(NULL);
	}
	tmp = (m_result *)malloc(sizeof(m_result));
	msqlDebug(MOD_MALLOC,"Result Handle - malloc @ %X of %d\n",
		tmp, sizeof(m_result));
	if (!tmp)
	{
		return(NULL);
	}
        tmp->queryData = tmp->cursor = NULL;
        tmp->fieldData = tmp->fieldCursor = NULL;
        tmp->numRows = numFields = 0;

	tmp->queryData = queryData;
	tmp->numRows = queryTableSize;
	tmp->fieldData = tableToFieldList(fieldData);
	tmp->numFields = fieldTableSize;
	tmp->cursor = tmp->queryData;
	tmp->fieldCursor = tmp->fieldData;
	freeQueryData(fieldData);
	queryData = NULL;
	fieldData = NULL;
	return(tmp);
}





/**************************************************************************
**	_msqlFetchField
**
**	Purpose	: Return a row of the query results
**	Args	: result handle
**	Returns	: pointer to row data
**	Notes	: 
*/

m_field	* APIENTRY msqlFetchField(handle)
	m_result *handle;
{
	m_field	*tmp;

	if (!handle->fieldCursor)
	{
		return(NULL);
	}
	tmp = &(handle->fieldCursor->field);
	handle->fieldCursor = handle->fieldCursor->next;
	return(tmp);
}



/**************************************************************************
**	_msqlFetchRow
**
**	Purpose	: Return a row of the query results
**	Args	: result handle
**	Returns	: pointer to row data
**	Notes	: 
*/

m_row	 APIENTRY msqlFetchRow(handle)
	m_result *handle;
{
	m_row	tmp;

	if (!handle->cursor)
	{
		return(NULL);
	}
	tmp = handle->cursor->data;
	handle->cursor = handle->cursor->next;
	return(tmp);
}




/**************************************************************************
**	_msqlFieldSeek
**
**	Purpose	: Move the result cursor
**	Args	: result handle, offset
**	Returns	: Nothing.  Just sets the cursor
**	Notes	: The data is a single linked list so we can go backwards
*/

void  APIENTRY msqlFieldSeek(handle, offset)
	m_result *handle;
	int	offset;
{
	m_fdata	*tmp;

	
	msqlDebug(MOD_API,"msqlFieldSeek() pos = \n",offset);
	tmp = handle->fieldData;
	while(offset)
	{
		if (!tmp)
			break;
		tmp = tmp->next;
		offset--;
	}
	handle->fieldCursor = tmp;
}

/**************************************************************************
**	_msqlDataSeek
**
**	Purpose	: Move the result cursor
**	Args	: result handle, offset
**	Returns	: Nothing.  Just sets the cursor
**	Notes	: The data is a single linked list so we can go backwards
*/

void  APIENTRY msqlDataSeek(handle, offset)
	m_result *handle;
	int	offset;
{
	m_data	*tmp;

	
	msqlDebug(MOD_API,"msqlDataSeek() pos = \n",offset);
	tmp = handle->queryData;
	while(offset)
	{
		if (!tmp)
			break;
		tmp = tmp->next;
		offset--;
	}
	handle->cursor = tmp;
}



/**************************************************************************
**	_
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

int readQueryData(sock)
	int	sock;
{
	int	off,
		len,
		status,
		numRows;
	char	*cp,
		*tmpcp;
	m_data	*cur = NULL;
	
	if (netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}

	numRows = 0;
	status = atoi(clientPacket);
	while(status != -100)
	{
		if (status == -1)
		{
			cp = (char *)index(clientPacket,':');
			if (cp)
			{
				strcpy(msqlErrMsg,cp+1);
				chopError();
			}	
			else
			{
				strcpy(msqlErrMsg,UNKNOWN_ERROR);
			}
			return(-1);
		}
		numRows++;
		if(!tmpDataStore)
		{
			/*tmpDataStore=cur=(m_data *)malloc(sizeof(m_data));*/
			tmpDataStore = cur = mallocDataStruct();
		}
		else
		{
			/*cur->next = (m_data *)malloc(sizeof(m_data));*/
			cur->next = mallocDataStruct();
			cur = cur->next;
		}
		msqlDebug(MOD_MALLOC,"Query data row - malloc @ %X of %d\n",
			cur, sizeof(m_data));
		cur->width = 0;
        	cur->next = NULL;

		cur->data = (char **)malloc(numFields * sizeof(char *));
		bzero(cur->data,numFields * sizeof(char *));
		cur->width = numFields;
		off = 0;
		cp = clientPacket;
		while(off < numFields)
		{
			len = atoi(cp);
			cp = (char *)index(cp,':');
			if (len == -2)
			{
				cur->data[off] = (char *)NULL;
				cp++;
			}
			else
			{
				cur->data[off] = (char *)malloc(len+1);
				bcopy(cp+1,cur->data[off],len);
				tmpcp = (char *)cur->data[off];
				*(tmpcp + len) = 0;
				cp += len + 1;
			}
			off++;
		}

		if (netClientReadPacket(sock) <= 0)
		{
			closeServer(sock);
			strcpy(msqlErrMsg,SERVER_GONE_ERROR);
			return(-1);
		}
		status = atoi(clientPacket);
	}
	return(numRows);
}





/**************************************************************************
**	_
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

int  APIENTRY msqlQuery(sock,q)
	int	sock;
	char	*q;
{
	char	*cp;
	int	res,
		count = 0;
	


        msqlDebug(MOD_QUERY,"Query = \"%s\"\n",q);
	msqlDebug(MOD_API,"msqlQuery()\n");
	resetError();
	setServerSock(sock);
	if (!initialised)
		_initialiseApi();
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}


	/*
	** Issue the query
	*/

	(void)snprintf(clientPacket,PKT_LEN,"%d:%s\n",QUERY,q);
	netClientWritePacket(sock);
	msqlDebug(MOD_API,"	Query sent\n");
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}
	msqlDebug(MOD_API,"	Initial response received\n");


	/*
	** Check the result.  It may be an indication of further data to
	** come (ie. from a select)
	*/

	res = atoi(clientPacket);
	if (res == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strncpy(msqlErrMsg,cp+1, ERR_BUF_LEN - 1);
			*(msqlErrMsg + ERR_BUF_LEN - 1) = 0;
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}

	cp = (char *)index(clientPacket,':');
	numFields = 0;
	if (cp)
	{
		numFields = atoi(cp+1);
		if (numFields <= 0)
			return(res);
	}
	else
	{
		return(res);
	}

	msqlDebug(MOD_API,"	%d fields\n", numFields);

	/*
	** numFields > 0 therefore we have data waiting on the socket.
	** Grab it and dump it into a table structure.  If there's
	** uncollected data free it - it's no longer available.
	*/
	if (tmpDataStore)
	{
		if (tmpDataStore != queryData && tmpDataStore != fieldData)
		{
			/* A prior query ended badly and didn't clean up */
			freeQueryData(tmpDataStore);
			tmpDataStore = NULL;
		}
	}
	if (queryData)
	{
		freeQueryData(queryData);
		queryData = NULL;
	}
	if (fieldData)
	{
		freeQueryData(fieldData);
		fieldData = NULL;
	}

	queryTableSize = readQueryData(sock);
	if (queryTableSize < 0)
	{
		if (tmpDataStore)
		{
			freeQueryData(tmpDataStore);
			tmpDataStore = NULL;
		}
		return(-1);
	}
	count = queryTableSize;
	msqlDebug(MOD_API,"	%d rows\n", queryTableSize);
	queryData = tmpDataStore;
	tmpDataStore = NULL;
	numFields = 6;
	fieldTableSize = readQueryData(sock);
	msqlDebug(MOD_API,"	%d field rows\n", fieldTableSize);
	if (fieldTableSize < 0)
	{
		if (tmpDataStore)
		{
			freeQueryData(tmpDataStore);
			tmpDataStore = NULL;
		}
		return(-1);
	}
	fieldData = tmpDataStore;
	tmpDataStore = NULL;
	return(count);
}





/**************************************************************************
**	_
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

m_result * APIENTRY msqlListDBs(sock)
	int	sock;
{
	m_result *tmp;

	msqlDebug(MOD_API,"msqlListDBs(%d)\n",sock);
	if (!initialised)
		_initialiseApi();
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(NULL);
	}
	tmp = (m_result *)malloc(sizeof(m_result));
	if (!tmp)
	{
		return(NULL);
	}
	bzero(tmp, sizeof(m_result));
	msqlDebug(MOD_MALLOC,"Result Handle - malloc @ %X of %d\n",
		tmp, sizeof(m_result));
	snprintf(clientPacket,PKT_LEN,"%d:\n",DB_LIST);
	netClientWritePacket(sock);
	numFields = 1;
	tmp->numRows = readQueryData(sock);
	if (tmp->numRows < 0)
	{
		(void)free(tmp);
		return(NULL);
	}
	tmp->queryData = tmpDataStore;
	tmp->cursor = tmp->queryData;
	tmp->numFields = 1;
	/*tmp->fieldData = (m_fdata *)malloc(sizeof(m_fdata));*/
	/*msqlDebug(MOD_MALLOC,"Field List Entry - malloc @ %X of %d\n",
		tmp->fieldData, sizeof(m_fdata));
	bzero(tmp->fieldData, sizeof(m_fdata));*/
	tmp->fieldData = mallocFieldDataStruct();
	tmp->fieldData->field.table = (char *)strdup("mSQL Catalog");
	tmp->fieldData->field.name = (char *)strdup("Database");
	tmp->fieldData->field.type = CHAR_TYPE;
	tmp->fieldData->field.length = NAME_LEN;
	tmp->fieldData->field.flags = 0;
	tmp->fieldCursor = tmp->fieldData;
	tmpDataStore = NULL;
	return(tmp);
}





/**************************************************************************
**	_
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

m_result * APIENTRY msqlListTables(sock)
	int	sock;
{
	m_result *tmp;

	msqlDebug(MOD_API,"msqlListTables(%d)\n",sock);
	if (!initialised)
		_initialiseApi();
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(NULL);
	}
	tmp = (m_result *)malloc(sizeof(m_result));
	if (!tmp)
	{
		return(NULL);
	}
	msqlDebug(MOD_MALLOC,"Result Handle - malloc @ %X of %d\n",
		tmp, sizeof(m_result));
	bzero(tmp, sizeof(m_result));
	snprintf(clientPacket,PKT_LEN,"%d:\n",TABLE_LIST);
	netClientWritePacket(sock);
	numFields = 1;
	tmp->numRows = readQueryData(sock);
	if (tmp->numRows < 0)
	{
		(void)free(tmp);
		return(NULL);
	}
	tmp->queryData = tmpDataStore;
	tmp->numFields = 0;
	tmp->cursor = tmp->queryData;
	tmp->fieldCursor = NULL;
	tmpDataStore = NULL;
	tmp->numFields = 1;
	/* tmp->fieldData = (m_fdata *)malloc(sizeof(m_fdata));
	msqlDebug(MOD_MALLOC,"Field List Entry - malloc @ %X of %d\n",
		tmp->fieldData, sizeof(m_fdata));
	bzero(tmp->fieldData, sizeof(m_fdata)); */
	tmp->fieldData = mallocFieldDataStruct();
	tmp->fieldData->field.table = (char *)strdup("mSQL Catalog");
	tmp->fieldData->field.name = (char *)strdup("Table");
	tmp->fieldData->field.type = CHAR_TYPE;
	tmp->fieldData->field.length = NAME_LEN;
	tmp->fieldData->field.flags = 0;
	tmp->fieldCursor = tmp->fieldData;
	return(tmp);
}


/**************************************************************************
**	_
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

m_result * APIENTRY msqlListFields(sock,table)
	int	sock;
	char	*table;
{
	m_result *tmp;

	msqlDebug(MOD_API,"msqlListFields(%d,%s)\n",sock,table);
	if (!initialised)
		_initialiseApi();
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(NULL);
	}
	tmp = (m_result *)malloc(sizeof(m_result));
	if (!tmp)
	{
		return(NULL);
	}
	msqlDebug(MOD_MALLOC,"Result Handle - malloc @ %X of %d\n",
		tmp, sizeof(m_result));
	bzero(tmp, sizeof(m_result));
	snprintf(clientPacket,PKT_LEN,"%d:%s\n",FIELD_LIST,table);
	netClientWritePacket(sock);
	numFields = 6;
	tmp->numFields = readQueryData(sock);
	if(tmp->numFields < 0)
	{
		(void)free(tmp);
		return(NULL);
	}
	tmp->fieldData = tableToFieldList(tmpDataStore);
	tmp->fieldCursor = tmp->fieldData;
	tmp->queryData = NULL;
	tmp->cursor = NULL;
	tmp->numRows = 0;
	freeQueryData(tmpDataStore);
	tmpDataStore = NULL;
	return(tmp);
}


/**************************************************************************
**	_
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

m_seq * APIENTRY msqlGetSequenceInfo(sock,table)
	int	sock;
	char	*table;
{
	static m_seq seq;
	char	*cp;

	if (!initialised)
		_initialiseApi();
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(NULL);
	}
	msqlDebug(MOD_API,"msqlGetSequenceInfo(%d,%s)\n",sock,table);
	snprintf(clientPacket,PKT_LEN,"%d:%s\n",SEQ_INFO,table);
	netClientWritePacket(sock);

        if(netClientReadPacket(sock) <= 0)
        {
                closeServer(sock);
                strcpy(msqlErrMsg,SERVER_GONE_ERROR);
                return(NULL);
        }


        /*
        ** Check the result.
        */

        if (atoi(clientPacket) == -1)
        {
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(NULL);
        }
        cp = (char *)index(clientPacket,':');
	seq.step = atoi(cp+1);
	cp = (char *)index(cp+1,':');
	seq.value = atoi(cp+1);
	return(&seq);
}



/**************************************************************************
**	_
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

m_result * APIENTRY msqlListIndex(sock,table, index)
	int	sock;
	char	*table,
		*index;
{
	m_result *tmp;

	if (!initialised)
		_initialiseApi();
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(NULL);
	}
	msqlDebug(MOD_API,"msqlListIndex(%d,%s,%s)\n",sock,table,index);
	tmp = (m_result *)malloc(sizeof(m_result));
	if (!tmp)
	{
		return(NULL);
	}
	msqlDebug(MOD_MALLOC,"Result Handle - malloc @ %X of %d\n",
		tmp, sizeof(m_result));
	bzero(tmp, sizeof(m_result));
	snprintf(clientPacket,PKT_LEN,"%d:%s:%s\n",INDEX_LIST,table,index);
	netClientWritePacket(sock);
	numFields = 1;
	tmp->numFields = readQueryData(sock);
	if(tmp->numFields < 0)
	{
		(void)free(tmp);
		return(NULL);
	}
	tmp->queryData = tmpDataStore;
	tmp->numFields = 0;
	tmp->cursor = tmp->queryData;
	tmp->fieldCursor = NULL;
	tmpDataStore = NULL;
	tmp->numFields = 1;
	tmp->fieldData = mallocFieldDataStruct();
	tmp->fieldData->field.table = (char *)strdup("mSQL Catalog");
	tmp->fieldData->field.name = (char *)strdup("Index");
	tmp->fieldData->field.type = CHAR_TYPE;
	tmp->fieldData->field.length = NAME_LEN;
	tmp->fieldData->field.flags = 0;
	return(tmp);
}




int  APIENTRY msqlCreateDB(sock,db)
	int	sock;
	char	*db;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlCreateDB(%d,%s)\n",sock,db);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	snprintf(clientPacket,PKT_LEN,"%d:%s\n",CREATE_DB,db);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}



int  APIENTRY msqlCopyDB(sock,fromDB, toDB)
	int	sock;
	char	*fromDB,
		*toDB;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlCopyDB(%d,%s,%s)\n",sock,fromDB, toDB);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	snprintf(clientPacket,PKT_LEN,"%d:%s:%s\n",COPY_DB,fromDB, toDB);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}


int  APIENTRY msqlExportTable(sock, db, table, sortField)
	int	sock;
	char	*db,
		*table,
		*sortField;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlExportTable(%d,%s,%s)\n",sock,db, table);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	if (sortField)
	{
		snprintf(clientPacket,PKT_LEN,"%d:%s:%s:%s\n",EXPORT, db, table,
			sortField);
	}
	else
	{
		snprintf(clientPacket,PKT_LEN,"%d:%s:%s\n",EXPORT,db, table);
	}
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}


int  APIENTRY msqlImportTable(sock, db, table)
	int	sock;
	char	*db,
		*table;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlImportTable(%d,%s,%s)\n",sock,db, table);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	snprintf(clientPacket,PKT_LEN,"%d:%s:%s\n",IMPORT,db, table);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}



int  APIENTRY msqlMoveDB(sock,fromDB, toDB)
	int	sock;
	char	*fromDB,
		*toDB;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlMoveDB(%d,%s,%s)\n",sock,fromDB, toDB);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	snprintf(clientPacket,PKT_LEN,"%d:%s:%s\n",MOVE_DB,fromDB, toDB);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}


int  APIENTRY msqlDropDB(sock,db)
	int	sock;
	char	*db;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlDropDB(%d,%s)\n",sock,db);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	snprintf(clientPacket,PKT_LEN,"%d:%s\n",DROP_DB,db);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}


int  APIENTRY msqlCheckTable(sock, db, table)
	int	sock;
	char	*db,
		*table;
{
	char	*cp;
	int	res;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlCheckTable(%d,%s,%s)\n",sock,db,table);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(MSQL_TABLE_UNKNOWN);
	}
	snprintf(clientPacket,PKT_LEN,"%d:%s:%s\n",CHECK_TABLE,db,table);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(MSQL_TABLE_UNKNOWN);
	}

	/*
	** Check the result.  
	*/

	res = atoi(clientPacket);
	if (res < 0)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
	}
	return(res);
}



m_tinfo * APIENTRY msqlTableInfo(sock, table)
	int	sock;
	char	*table;
{
	char	*cp;
	static	m_tinfo	info;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlTableInfo(%d,%s)\n",sock,table);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(NULL);
	}
	snprintf(clientPacket,PKT_LEN,"%d:%s\n",TABLE_INFO,table);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(NULL);
	}

	/*
	** Check the result.  
	*/

        if (atoi(clientPacket) == -1)
        {
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(NULL);
        }
	bzero(&info, sizeof(info));
        cp = (char *)index(clientPacket,':');
	info.rowLen = strtoul(cp+1,NULL,10);
	cp = (char *)index(cp+1,':');
	info.dataSize = strtoul(cp+1,NULL,10);
	cp = (char *)index(cp+1,':');
	info.fileSize = strtoul(cp+1,NULL,10);
	cp = (char *)index(cp+1,':');
	info.activeRows = strtoul(cp+1,NULL,10);
	cp = (char *)index(cp+1,':');
	info.totalRows = strtoul(cp+1,NULL,10);
	return(&info);
}



int  APIENTRY msqlShutdown(sock)
	int	sock;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlShutdown(%d)\n",sock);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	snprintf(clientPacket,PKT_LEN,"%d:\n",SHUTDOWN);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}



int  APIENTRY msqlReloadAcls(sock)
	int	sock;
{
	char	*cp;

	if (!initialised)
		_initialiseApi();
	msqlDebug(MOD_API,"msqlReloadAcl(%d)\n",sock);
	if (isatty(sock))
	{
		strcpy(msqlErrMsg,"Socket not connected to mSQL");
		return(-1);
	}
	snprintf(clientPacket,PKT_LEN,"%d:\n",RELOAD_ACL);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}


	/*
	** Check the result.  
	*/

	if (atoi(clientPacket) == -1)
	{
		cp = (char *)index(clientPacket,':');
		if (cp)
		{
			strcpy(msqlErrMsg,cp+1);
			chopError();
		}
		else
		{
			strcpy(msqlErrMsg,UNKNOWN_ERROR);
		}
		return(-1);
	}
	return(0);
}



char * APIENTRY msqlGetServerInfo()
{
	return(serverInfo);
}


char * APIENTRY msqlGetHostInfo()
{
	return(hostInfo);
}


int  APIENTRY msqlGetProtoInfo()
{
	return(protoInfo);
}


int  APIENTRY msqlGetServerStats(sock)
	int	sock;
{
	msqlDebug(MOD_API,"msqlShutdown(%d)\n",sock);
	if (!initialised)
		_initialiseApi();
	snprintf(clientPacket,PKT_LEN,"%d:\n",SERVER_STATS);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}
	if (atoi(clientPacket) == -1)
	{
		strcpy(msqlErrMsg,clientPacket+3);
		return(-1);
	}
	while(atoi(clientPacket) != -100)
	{
		printf("%s",clientPacket);
		if(netClientReadPacket(sock) <= 0)
		{
			closeServer(sock);
			strcpy(msqlErrMsg,SERVER_GONE_ERROR);
			return(-1);
		}
	}
	return(0);
}



int  APIENTRY msqlExplain(sock, q)
	int	sock;
	char	*q;
{
	msqlDebug(MOD_API,"msqlExplain(%d,'%s')\n",sock,q);
	if (!initialised)
		_initialiseApi();
        (void)snprintf(clientPacket,PKT_LEN,"%d:%s\n",EXPLAIN,q);
	netClientWritePacket(sock);
	if(netClientReadPacket(sock) <= 0)
	{
		closeServer(sock);
		strcpy(msqlErrMsg,SERVER_GONE_ERROR);
		return(-1);
	}
	printf("\n");
	if (atoi(clientPacket) == -1)
	{
		strcpy(msqlErrMsg,clientPacket+3);
		return(-1);
	}
	while(atoi(clientPacket) != -100)
	{
		if (atoi(clientPacket) == -1)
		{
			strcpy(msqlErrMsg,clientPacket+3);
			return(-1);
		}
		printf("%s", clientPacket);
		if(netClientReadPacket(sock) <= 0)
		{
			closeServer(sock);
			strcpy(msqlErrMsg,SERVER_GONE_ERROR);
			return(-1);
		}
	}
	printf("\n");
	return(0);
}


int APIENTRY msqlLoadConfigFile(file)
	char	*file;
{
	return(configLoadFile(file));
}


int APIENTRY msqlGetIntConf(sect, item)
	char	*sect,
		*item;
{
	return(configGetIntEntry(sect,item));
}


char *APIENTRY msqlGetCharConf(sect, item)
	char	*sect,
		*item;
{
	return(configGetCharEntry(sect,item));
}


char * APIENTRY msqlTypeName(type)
	int	type;
{
	switch(type)
	{
	case INT8_TYPE: return("INT8");
	case INT16_TYPE: return("INT16");
	case INT32_TYPE: return("INT");
	case INT64_TYPE: return("INT64");
	case CHAR_TYPE: return("CHAR");
	case REAL_TYPE: return("REAL");
	case IDENT_TYPE: return("IDENT");
	case NULL_TYPE: return("NULL");
	case TEXT_TYPE: return("TEXT");
	case DATE_TYPE: return("DATE");
	case UINT8_TYPE: return("UINT8");
	case UINT16_TYPE: return("UINT16");
	case UINT32_TYPE: return("UINT");
	case UINT64_TYPE: return("UINT64");
	case MONEY_TYPE: return("MONEY");
	case TIME_TYPE: return("TIME");
	case IPV4_TYPE: return("IPv4");
	default: return("UNKNOWN");
	}
}
