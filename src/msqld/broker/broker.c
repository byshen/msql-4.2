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
** $Id: broker.c,v 1.11 2002/07/17 04:48:39 bambi Exp $
**
*/

/*
** Module	: broker : broker.c
** Purpose	: broker related routines
** Exports	: 
** Depends Upon	: 
*/


/*
** Note on terminology - a child (or kid) is a child process created by
** the main broker process and is responsible for handling query
** requests.  The children are the backend database processes.  A client
** is an unrelated process that is connecting to us to use the database
*/


/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <common/config.h>
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

#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "common/msql_defs.h"
#include "common/debug/debug.h"
#include "common/config/config.h"
#include "msqld/broker/filelib.h"
#include "msqld/index/index.h"
#include "msqld/includes/msqld.h"
#include "msqld/main/main.h"
#include "msqld/broker/broker.h"
#include "msqld/broker/broker_priv.h"


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

static 	ipc_t	*ipcInfo;
static	int	clientSockRefCount[255],
		numSpareQueueEntries = 0,
		numKids = -1,
		initialised = 0;
static	mMsg_q 	*spareQueueEntries[BROKER_QUEUE_MAX_SPARE];
static	char 	blank[] = "";
static 	char 	*brokerCommandNames[] = {
        		"???", "Client Open", "Client Close", "Flush Cache",
        		"Client DB", "Run Queue", "Queue End", "???" };

extern	msqld*	globalServer;


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

/*
** Private
** _initialiseBroker
*/

static void _initialiseBroker()
{
	if (initialised)
		return;

	numKids = configGetIntEntry("system","num_children");
	initialised++;
}


/*
** Private
** _getQueueEntry()		- Grab a spare queue entry for reuse
*/
static mMsg_q *_getQueueEntry()
{
	mMsg_q	*new;

	/* 
	** If there's a spare return it.  Else create one 
	*/
	if (numSpareQueueEntries > 0)
	{
		numSpareQueueEntries--;
		new = spareQueueEntries[numSpareQueueEntries];
	}
	else
	{
		new = (mMsg_q *)malloc(sizeof(mMsg_q));
	}
	bzero(new, sizeof(mMsg_t));
	/*
	new->message.command = new->message.access = new->message.client = 0;
	*new->message.db = *new->message.table = *new->message.user = 
		*new->message.client_ip = 0;
	new->next = NULL;
	*/
	new->fd = -1;
	return(new);
}


/*
** Private
** _saveQueueEntry()		- Save a now unused queue entry for later use
*/
static void _saveQueueEntry(entry)
	mMsg_q	*entry;
{
	/*
	** If we can handle more spares, keep the entry.  Else free it.
	*/
	if (numSpareQueueEntries < BROKER_QUEUE_MAX_SPARE)
	{
		spareQueueEntries[numSpareQueueEntries] = entry;
		numSpareQueueEntries++;
	}
	else
	{
		free(entry);
	}
}


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


/*
** Public
** brokerRunMessageQueue		- Send pending messages to kid
*/

/*
** We queue up all messages destined for the kids.  We don't just write
** the message and expect the kernel to handle the queueing.  Expecting
** the kernel to handle an unknown number of broker messages is not an
** acceptable solution as a long running queury on a busy system can
** exhaust the kernels queue capacity and cause the broker process to
** block on a write - resulting in all further attempts to contact the
** the broker to be locked out.
*/

void brokerRunMessageQueue(child)
	int	child;
{
	ipc_t	*curIPC;
	mMsg_q	*curMessage,
		*tmpMessage;
	mMsg_t	message;

	msqlDebug1(MOD_BROKER,"Running broker queue for kid %d\n",child);
	curIPC = ipcInfo + child;
	curMessage = curIPC->messages_head;
	while(curMessage)
	{
		/*
		** Send the message to the child
		*/
		msqlDebug3(MOD_BROKER,"Sending %s to kid %d on IPC sock %d\n",
			brokerCommandNames[curMessage->message.command],child,
			curIPC->toSock);
		write(curIPC->toSock, &(curMessage->message), sizeof(mMsg_t));

		/*
		** If it's an open, send the socket as well
		*/
		if (curMessage->message.command == CMD_CLIENT_OPEN)
		{
			msqlDebug3(MOD_BROKER, 
				"Sending fd %d to kid %d pid %d\n",
                                curMessage->fd,child,curIPC->pid);
                        brokerSendFD(curIPC->toSock, curMessage->fd);
		}

		/*
		** If it's a close, decrement the client's socket ref
		** count.  If the ref count hits zero then shut it down
		*/
		if (curMessage->message.command == CMD_CLIENT_CLOSE)
		{
			clientSockRefCount[curMessage->message.client]--;
			msqlDebug2(MOD_BROKER,
				"Ref count for client %d now %d\n",
				curMessage->message.client,
				clientSockRefCount[curMessage->message.client]);
			if(clientSockRefCount[curMessage->message.client] <= 0)
			{
				int	sock;

				sock = curMessage->message.client;
				shutdown(sock,2);
				close(sock);
				freeClientConnection(globalServer, sock);
				msqlDebug1(MOD_BROKER,
					"Client sock %d closed\n",sock);
			}
		}

		tmpMessage = curMessage;
		curMessage = curMessage->next;
		tmpMessage->next = NULL;
		_saveQueueEntry(tmpMessage);
	}

	/*
	** Clear out the message queue and send an end of queue message
	*/
	curIPC->messages = curIPC->jabbed = 0;
	curIPC->messages_head = NULL;
	curIPC->messages_tail = NULL;
	bzero(&message, sizeof(message));
	message.command = CMD_QUEUE_END;
	write(curIPC->toSock, &message, sizeof(mMsg_t));
	msqlDebug1(MOD_BROKER,"Sending EndOfQueue to kid %d\n",child);
}



/*
** Public
** brokerNotifyChild			- Queue a message for a child
*/

void brokerNotifyChild(kid,command,sock,user,db,table,access,ipAddr)
	int	kid,
		command,
		sock;
	char	*user,
		*db,
		*table;
	int	access;
	char	*ipAddr;
{
	ipc_t	*curIPC;
	mMsg_q	*new;
	char	ack=0;

	/*
	** Create a message struct 
	*/
	curIPC = ipcInfo + kid;
	msqlDebug3(MOD_BROKER,"Queueing %s for kid %d pid %d\n",
		brokerCommandNames[command],kid,curIPC->pid);

	new = _getQueueEntry();
	new->message.command = command;
	new->message.client = sock;
	new->message.access = access;
	if (user)
		strncpy(new->message.user,user, NAME_LEN);
	if (db)
		strncpy(new->message.db,db, NAME_LEN);
	if (table)
		strncpy(new->message.table,table, NAME_LEN);
	if (ipAddr)
		strncpy(new->message.client_ip, ipAddr,15);


	/*
	** Add it to the childs message queue
	*/	
	if (curIPC->messages_head == NULL)
	{
		curIPC->messages_head = new;
		curIPC->messages_tail = curIPC->messages_head;
	}
	else
	{
		curIPC->messages_tail->next = new;
		curIPC->messages_tail = curIPC->messages_tail->next;
	}
	curIPC->messages_tail->next = NULL;
	curIPC->messages_tail->fd = -1;
	if(command == CMD_CLIENT_OPEN)
	{
		curIPC->messages_tail->fd = sock;
	}

	/*
	** If we haven't 'jabbed' the client the do so to 
	** tell it that it has pending messages
	*/
	if (curIPC->jabbed == 0)
	{
		msqlDebug2(MOD_BROKER,"Jabbing kid %d pid %d\n",
			kid,curIPC->pid);
		write(curIPC->toSock,&ack,1);
		curIPC->jabbed = 1;
	}
	curIPC->messages++;
	msqlDebug2(MOD_BROKER, "Queued length for kid %d is now %d\n",
		kid, curIPC->messages);
}


/*
** Public
** brokerNotifyAllChildren		- Queue a message to all kids
*/
void brokerNotifyAllChildren(command, exclude, sock, user, db, table, 
	access, ipAddr)
	int	command,
		exclude,
		sock;
	char	*user,
		*db,
		*table;
	int	access;
        char	*ipAddr;
{
	int 	count;

	/*
	** Send this message to all kids expcept the one listed in
	** 'exclude' as it is the child that sent it to us in the first 
	** place.
	*/
	msqlDebug1(MOD_BROKER,"Sending %s to all kids\n",
		brokerCommandNames[command]);

	for(count=0;count<configGetIntEntry("system","num_children");count++)
	{
		if (count == exclude)
		{
			msqlDebug1(MOD_BROKER,
				"Kid %d ignored as it was the message source\n",
				exclude);
			continue;
		}
		brokerNotifyChild(count,command,sock,user,db,
			table,access,ipAddr);
	}
}



/*
** Public
** brokerCloseClient			- Close a client connection
*/


/*
** Note 1:
**
** We can't just close the socket if a client has dropped it's connection.
** If there is an outstanding CLIENT_OPEN for that client pending for
** one of the kids then it will fail when we try to send it the FD
** (because we closed it).  Keep a reference count showing the number of
** children that have the socket open and only close it when the ref
** count hits 0.
**
** Note 2:
**
** If a child is processing a long running query it is possible for may
** clients to come and go while it is still busy.  This results in a
** large number of open sockets in the broker (held open by the
** reference count) and a long queue of OPEN/DB/CLOSE message sequences
** for the busy child.  Rather than build up the queue with useless
** messages (the client is gone so why bother telling the kids that it
** even existed), scan each childs queue for an OPEN message associated
** with the client.  If we find an open then we know the child isn't
** holding the socket.  In that case, remove the first OPEN and DB for
** that client from the queue (note - only the first).  If we don't find an
** open then the child is already holding the client socket.  In that case
** we have to queue up a CLOSE message and increment the ref count for that
** socket.
*/

void brokerCloseClient(sock, sourceChild)
	int	sock,
		sourceChild;
{
	ipc_t	*curIPC;
	int	curKid,
		command,
		state,
		refCount;
	mMsg_t	message;
	mMsg_q	*curMessage,
		*prevMessage,
		*tmpMessage;

	if (!initialised)
		_initialiseBroker();

	if (clientSockRefCount[sock] > 0)
	{
		msqlDebug1(MOD_BROKER,
		    "Ignoring close on client %d.  Close already in queue\n",
		    sock);
		return;
	}
	message.command = CMD_CLIENT_CLOSE;
	message.client = sock;

	refCount = 0;
	curKid = 0;
	while (curKid < numKids)
	{
		/*
		** Do not process the close for the child that sent
		** us the close command
		*/
		if (curKid == sourceChild)
		{
			curKid++;
			continue;
		}

		/*
		** Scan the queue looking for only the first OPEN and DB
		** for this client.  Keep a track of our state so we
		** don't blindly remove messages we shouldn't
		*/
		curIPC = ipcInfo + curKid;
		prevMessage = NULL;
		curMessage = curIPC->messages_head;
		state = 0;
		while(curMessage)
		{
			if (curMessage->message.client != sock)
			{
				prevMessage = curMessage;
				curMessage = curMessage->next;
				continue;
			}
			command = curMessage->message.command;
			if (state == 0 && command != CMD_CLIENT_OPEN)
			{
				/* We are looking for an OPEN */
				prevMessage = curMessage;
				curMessage = curMessage->next;
				continue;
			}
			if (state == 1 && command != CMD_CLIENT_DB)
			{
				/* We are looking for a DB */
				prevMessage = curMessage;
				curMessage = curMessage->next;
				continue;
			}

			msqlDebug2(MOD_BROKER,
				"Removing queued %s from kid %d\n",
				brokerCommandNames[command], curKid);
			state++;
			if (prevMessage == NULL)
			{
				curIPC->messages_head = curMessage->next;
			}
			else
			{
				prevMessage->next = curMessage->next;
			}
			tmpMessage = curMessage;
			curMessage = curMessage->next;
			tmpMessage->next = NULL;
			if (curIPC->messages_tail == tmpMessage)
				curIPC->messages_tail = prevMessage;
			_saveQueueEntry(tmpMessage);
			curIPC->messages--;
			msqlDebug2(MOD_BROKER,
				"Queued length for kid %d is now %d\n",
				curKid, curIPC->messages);
		}

		if (state == 0)
		{
			/*
			** We didn't find the CLIENT_OPEN so the
			** kid has already processed it.  We need
			** to send it a CLOSE and jack up the
			** ref count
			*/
			brokerNotifyChild(curKid,CMD_CLIENT_CLOSE,
				sock, blank, blank, blank, 0, blank);
			refCount++;
		}
		curKid++;
	}

	msqlDebug2(MOD_BROKER,"Ref count for client %d set to %d\n",
		sock, refCount);
	clientSockRefCount[sock] = refCount;
	if (refCount == 0)
	{
		shutdown(sock,2);
		close(sock);
		freeClientConnection(globalServer, sock);
		msqlDebug1(MOD_BROKER,
			"No pending closes for sock %d. Socket closed\n",sock);
	}
}


/*
** Public
** brokerCloseChildren
*/

void brokerCloseChildren()
{
	int	count;
	ipc_t	*curIPC;

	if (!initialised)
		_initialiseBroker();

	for (count = 0; count < numKids; count++)
	{
                curIPC = ipcInfo + count;
                kill(curIPC->pid, SIGUSR1);
                count++;
	}
        while(1)
        {
                /* Wait for all the backends to terminate */
                if (wait(NULL) < 0)
                        break;
        }
}



/*
** Public
** brokerStartChildren
int brokerStartChildren(server, socks, argc,argv)
	msqld	*server;
	fd_set	*socks;
	int	argc;
	char	*argv[];
*/
int brokerStartChildren(server, argc,argv)
	msqld	*server;
	int	argc;
	char	*argv[];
{
	int	count,
		pid,
		to[2],
		from[2],
		oob[2],
		max = 0;
	ipc_t	*curIPC;
	char	path[255];

	if (!initialised)
		_initialiseBroker();

	sprintf(path,"%s/trace", globalServer->config.instDir);
	mkdir(path,0770);
	printf("\tStarting %d backends\n", numKids);
	ipcInfo = (ipc_t *)malloc(sizeof(ipc_t) * numKids);
	bzero(ipcInfo,sizeof(ipc_t) * numKids);
	count = 0;
	while(count < numKids)
	{
		curIPC = ipcInfo + count;
		if (socketpair(AF_UNIX, SOCK_STREAM,0,to) < 0)
		{
			perror("Failed to create socket pair");
			exit(1);
		}
		if (socketpair(AF_UNIX, SOCK_STREAM,0,from) < 0)
		{
			perror("Failed to create socket pair");
			exit(1);
		}
		if (socketpair(AF_UNIX, SOCK_STREAM,0,oob) < 0)
		{
			perror("Failed to create socket pair");
			exit(1);
		}

#ifdef NOTDEF
		setsockopt(to[0],SOL_SOCKET,SO_PASSCRED,&on, sizeof(on));
		setsockopt(to[1],SOL_SOCKET, SO_PASSCRED,&on, sizeof(on));
		setsockopt(from[0],SOL_SOCKET, SO_PASSCRED,&on, sizeof(on));
		setsockopt(from[1],SOL_SOCKET, SO_PASSCRED,&on, sizeof(on));
#endif
		/*
		** DEBUGGING : change dir to a new directory so we
		** can catch core dumps and profile outputs for this
		** child
		*/
		sprintf(path,"%s/trace/%d",globalServer->config.instDir,count);
		mkdir(path,0770);
		chdir(path);

		pid = fork();
		if (pid < 0)
		{
			perror("Fork failed");
			exit(1);
		}
		if (pid == 0)
		{
			/* Child */
			close(0);
			dup2(to[0],3);
			close(to[0]);
			close(to[1]);
			dup2(from[1],4);
			close(from[0]);
			close(from[1]);
			dup2(oob[0],5);
			close(oob[0]);
			close(oob[1]);
			childStartup(server);
			fprintf(stderr,"\nChild %d (%d) returned!  Bailing\n\n",
				count, (int)getpid());
			exit(1);
		}
		else
		{
			/* Parent */
			curIPC->toSock = to[1];
			curIPC->fromSock = from[0];
			curIPC->oobSock = oob[1];
			curIPC->pid = pid;
			curIPC->messages = 0;
			curIPC->jabbed = 0;
			curIPC->messages_head = NULL;
			curIPC->messages_tail = NULL;
			/* XXX FD_SET(from[1], socks);*/
			if (curIPC->toSock > max)
				max = curIPC->toSock;
			if (curIPC->fromSock > max)
				max = curIPC->fromSock;
			close(to[0]);
			close(from[1]);
		}
		count++;
	}
	sprintf(path,"%s/trace/broker",globalServer->config.instDir);
	mkdir(path,0770);
	chdir(path);
	return(max);
}

/*
** Public
** brokerAddChildSocket
*/

void brokerAddChildSockets(fds)
	fd_set	*fds;
{
	int	count;
	ipc_t	*curIPC;

	if (!initialised)
		_initialiseBroker();
	for (count = 0; count < numKids; count++)
	{
		curIPC = ipcInfo + count;
		FD_SET(curIPC->fromSock, fds);
	}
	return;
}


/*
** public
** brokerReadChildMessage
*/


void brokerReadChildMessage(child)
	int	child;
{
	mMsg_t	message;
	char	ack = 0;
	ipc_t	*curIPC;
	int	remain,
		numBytes;
	char	*cp;

	if (!initialised)
		_initialiseBroker();

	curIPC = ipcInfo + child;
	remain = sizeof(message);
	cp = (char *)&message;
	while(remain)
	{
		numBytes = read(curIPC->fromSock, cp, remain);
		if (numBytes < 0)
		{
			fprintf(stderr,
				"\nChild termination - shuting down\n\n");
			puntServer(-1);
			exit(1);
		}
		cp = cp + numBytes;
		remain = remain - numBytes;
	}
	
	msqlDebug3(MOD_BROKER,"Got child message from %d pid %d (%s)\n",
		child, curIPC->pid, brokerCommandNames[message.command]);
	switch(message.command)
	{
		case CMD_CLIENT_CLOSE:
			puntClient(0);
			brokerCloseClient(message.client,child);
			break;

		case CMD_FLUSH_CACHE:
		case CMD_CLIENT_DB:
			brokerNotifyAllChildren(message.command,child,
				message.client, message.user, message.db, 
				message.table, message.access, blank);
			break;

		case CMD_RUN_QUEUE:
			msqlDebug0(MOD_BROKER,"Sending ACK to child\n");
			write(curIPC->oobSock, &ack, 1);
			brokerRunMessageQueue(child);
			break;
	}
	if (message.command != CMD_RUN_QUEUE)
	{
		/* 
		** We must ack a run_queue straight away otherwise
		** we can fill the kernel's message queue to the
		** requesting client while it waits for the ack.
		** There's no race condition on a run queue so
		** there's no problems doing this
		*/
		msqlDebug0(MOD_BROKER,"Sending ACK to child\n");
		write(curIPC->oobSock, &ack, 1);
	}
}


/*
** Public
** brokerCheckChildren
*/

void brokerCheckChildren(fds)
	fd_set	*fds;
{
	int	count;
	ipc_t	*curIPC;

	if (!initialised)
		_initialiseBroker();

	for(count=0; count<numKids; count++)
	{
		curIPC = ipcInfo + count;
		if (FD_ISSET(curIPC->fromSock, fds))
			brokerReadChildMessage(count);
	}
}


char *brokerGetCommandName(cmd)
	int	cmd;
{
	return(brokerCommandNames[cmd]);
}



