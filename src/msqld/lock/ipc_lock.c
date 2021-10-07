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
** $Id: ipc_lock.c,v 1.3 2002/05/28 01:28:53 bambi Exp $
**
*/

/*
** Module	: lock : ipc_lock
** Purpose	: Mutex locking between backend processes
** Exports	: 
** Depends Upon	: 
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


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <fcntl.h>
#include <sys/file.h>
#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/lock/lock.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

static int      lockFD = -1,
                haveLock = 0;
static char    	lockPath[255];

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


void lockGetIpcLock(server)
	msqld	*server;
{
        if (lockFD < 0)
        {
                strcpy(lockPath,"/tmp/msqld.lock");
                lockFD = open(lockPath,O_CREAT|O_TRUNC|O_RDWR,0700);
                if (lockFD < 0)
                {
                        perror("open of lock");
                        exit(1);
                }
        }
        msqlDebug0(MOD_GENERAL,"Getting select lock\n");
	lockGetFileLock(server,lockFD, MSQL_WR_LOCK);
        haveLock = 1;
        msqlDebug0(MOD_GENERAL,"Got select lock\n");
}


void lockReleaseIpcLock(server)
	msqld	*server;
{
        if (!haveLock)
                return;
        msqlDebug0(MOD_GENERAL,"Select lock released\n");
	lockGetFileLock(server, lockFD, MSQL_UNLOCK);
        haveLock = 0;
}

 
