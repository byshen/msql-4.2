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
** $Id: file_lock.c,v 1.5 2002/05/28 01:28:53 bambi Exp $
**
*/

/*
** Module	: lock : file_lock
** Purpose	: 
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

#include <sys/file.h>
#include <fcntl.h>

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/lock/lock.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

#ifdef HAVE_FLOCK
#  define USE_FLOCK
#else
#  define USE_FCNTL
#endif

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


#ifdef NO_FILE_LOCK
/* Only use for performance comparison testing !! */

void lockGetFileLock(server, fd, type)
	msqld	*server;
        int     fd;
        int     type;
{
}

#endif


#ifdef USE_FCNTL

/****************************************************************************
**      Data file locking routines
*/

static  u_int   curLockTypes[255];

void lockGetFileLock(server, fd, type)
	msqld	*server;
        int     fd;
        int     type;
{
        struct  flock lock;

	/*
	** If this is being called by a single process server then it
	** should have set the field to turn off file locking
	*/
	if (server->config.needFileLock == 0)
		return;

        /*  
        ** If we have a lock on the file just return
        */ 
        if (curLockTypes[fd] >= type && type != MSQL_UNLOCK)
        {
                return;
        }


        /*
        ** If we have another lock then we have to release it
        */
        if (curLockTypes[fd] > 0 && type != MSQL_UNLOCK)
        {
                lock.l_type = F_UNLCK;
                lock.l_start = 0;
                lock.l_whence = SEEK_SET;
                lock.l_len = 1;
                lock.l_pid = 0;
                fcntl(fd, F_SETLKW, &lock);
                msqlDebug1(MOD_LOCK,"Released lock on %d\n",fd);
        }



        /*
        ** Set/release the lock and block until we get it
        */
        if (type == MSQL_WR_LOCK)
        {
                lock.l_type = F_WRLCK;
                msqlDebug1(MOD_LOCK,"Setting write lock on %d\n",fd);
        }
        if (type == MSQL_RD_LOCK)
        {
                lock.l_type = F_RDLCK;
                msqlDebug1(MOD_LOCK,"Setting read lock on %d\n",fd);
        }
        if (type == MSQL_UNLOCK)
        {
                lock.l_type = F_UNLCK;
                msqlDebug1(MOD_LOCK,"Releasing lock on %d\n",fd);
        }
        lock.l_start = 0;
        lock.l_whence = SEEK_SET;
        lock.l_len = 1;
        lock.l_pid = 0;

        fcntl(fd, F_SETLKW, &lock);

        if (type == MSQL_UNLOCK)
        {
                curLockTypes[fd] = 0;
        }
        else
        {
                curLockTypes[fd] = type;
        }
}



int lockNonBlockingLock(fd)
        int     fd;
{
        struct  flock lock;


        /*
        ** Set the lock 
        */
	lock.l_type = F_WRLCK;
        lock.l_start = 0;
        lock.l_whence = SEEK_SET;
        lock.l_len = 1;
        lock.l_pid = 0;
        if (fcntl(fd, F_SETLK, &lock) < 0)
		return(-1);
	else
		return(0);
}
#endif  /* Use flock() */



#ifdef USE_FLOCK

static  u_int   curLockTypes[255];

void lockGetFileLock(server, fd, type)
	msqld	*server;
        int     fd;
        int     type;
{
	/*
	** If this is being called by a single process server then it
	** should have set the field to turn off file locking
	*/
	if (server->config.needFileLock == 0)
		return;

        /*  
        ** If we have a lock on the file just return
        */ 
        if (curLockTypes[fd] >= type && type != MSQL_UNLOCK)
        {
                return;
        }


        /*
        ** If we have another lock then we have to release it
        */
        if (curLockTypes[fd] > 0 && type != MSQL_UNLOCK)
        {
                flock(fd, LOCK_UN);
                msqlDebug1(MOD_LOCK,"Released lock on %d\n",fd);
        }



        /*
        ** Set/release the lock and block until we get it
        */
        if (type == MSQL_WR_LOCK)
        {
                msqlDebug1(MOD_LOCK,"Setting write lock on %d\n",fd);
		flock(fd, LOCK_EX);
        }
        if (type == MSQL_RD_LOCK)
        {
                msqlDebug1(MOD_LOCK,"Setting read lock on %d\n",fd);
                flock(fd, LOCK_SH);
        }
        if (type == MSQL_UNLOCK)
        {
                msqlDebug1(MOD_LOCK,"Releasing lock on %d\n",fd);
                flock(fd, LOCK_UN);
        }

        if (type == MSQL_UNLOCK)
        {
                curLockTypes[fd] = 0;
        }
        else
        {
                curLockTypes[fd] = type;
        }
}



int lockNonBlockingLock(fd)
        int     fd;
{
        /*
        ** Set the lock
        */
	if (flock(fd, LOCK_EX | LOCK_NB )  < 0)
		return(-1);
	else
		return(0);
}
#endif
