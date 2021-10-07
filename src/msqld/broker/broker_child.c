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
** $Id: broker_child.c,v 1.6 2008/10/02 05:28:11 bambi Exp $
**
*/

/*
** Module	: broker : broker_child
** Purpose	: Child process routines for IPC with the broker
** Exports	: 
** Depends Upon	: 
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "common/config.h"
#include "common/msql_defs.h"
#include "common/debug/debug.h"
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/broker/broker.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/



/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/

/*
** Public
** brokerChildSendMessage			- Send message up to the broker
*/
void brokerChildSendMessage(message)
        mMsg_t   *message;
{
        char    ack;

	msqlDebug1(MOD_BROKER,"Sending broker message %s\n",
		brokerGetCommandName(message->command));
        if (message->command == CMD_FLUSH_CACHE)
                message->client = 0;
        if (write(BROKER_TO_FD, message, sizeof(mMsg_t)) < 1)
        {
                puntServer(0);
        }
	msqlDebug0(MOD_BROKER,"Waiting for broker ACK\n");
	if(read(BROKER_OOB_FD, &ack, 1) < 1)
	{
		perror("Broker ACK read");
		exit(1);
	}
	msqlDebug0(MOD_BROKER,"Broker ACK received. Continuing\n");
}

/*
** Public
** brokerChildSendFlush		- Send a flush message to the broker
*/

/*
** A flush message is sent via the broker to the other children.  On
** receipt, the child must flush any entries associated with the
** database and table (or any table in the specified database if
** 'table' is NULL) from it's table cache.
*/
void brokerChildSendFlush(db, table)
	char	*db,
		*table;
{
        mMsg_t   message;

	bzero(&message, sizeof(message));                    
        message.command = CMD_FLUSH_CACHE;
        message.client = 0; 
	strcpy(message.db,db);
	strcpy(message.table,table);
        brokerChildSendMessage(&message);
}

