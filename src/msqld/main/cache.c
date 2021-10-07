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
** $Id: cache.c,v 1.15 2007/06/14 00:15:14 bambi Exp $
**
*/

/*
** Module	: main : cache
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
#include <common/config.h>
#include <common/config_extras.h>
#include <common/config/config.h>
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
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef HAVE_DIRENT_H
#    include <dirent.h>   
#endif

#ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif

#include <common/portability.h>
#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/version.h>
#include <msqld/main/cache.h>
#include <msqld/main/table.h>
#include <msqld/main/index.h>
#include <msqld/cra/cra.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

cache_t	*tableCache;
extern	msqld *globalServer;    /* from main.c */


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static void _freeCacheEntry(entry)
	cache_t	*entry;
{
	debugTrace(TRACE_IN,"freeCacheEntry()");
	tableFreeDefinition(entry->def);
	entry->def = NULL;
	*(entry->db) = 0;
	*(entry->table) = 0;
	entry->age = 0;
	if (entry->dataMap != (caddr_t) NULL)
	{
		MSYNC(entry->dataMap,entry->size,0);
		munmap(entry->dataMap,entry->size);
		entry->dataMap = NULL;
		entry->size = 0;
	}
	if (entry->overflowMap != (caddr_t) NULL)
	{
		MSYNC(entry->overflowMap,entry->overflowSize,0);
		munmap(entry->overflowMap,entry->overflowSize);
		entry->overflowMap = NULL;
		entry->overflowSize = 0;
	}
	close(entry->dataFD);
	close(entry->overflowFD);
	indexCloseIndices(entry);
	if (entry->row.buf)
	{
		free(entry->row.buf);
		entry->row.buf = NULL;
	}
	debugTrace(TRACE_OUT,"freeCacheEntry()");
}




/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



void cacheDropTableCache()
{
	int	index = 0,
		cacheSize;

	cacheSize = configGetIntEntry("system", "table_cache");

	while(index < cacheSize)
	{
		if (tableCache[index].def)
		{
			_freeCacheEntry(tableCache + index);
		}
		index++;
	}
}




void cacheInvalidateEntry(entry)
	cache_t	*entry;
{
	msqlDebug2(MOD_CACHE,"Clearing cache entry (%s:%s)\n",
		entry->db,entry->table);

	tableFreeDefinition(entry->def);
	entry->def = NULL;
	if (entry->row.buf)
	{
		free(entry->row.buf);
		entry->row.buf = NULL;
	}
	*(entry->db) = 0;
	*(entry->table) = 0;
	entry->age = 0;
	if (entry->dataMap != (caddr_t) NULL)
	{
		munmap(entry->dataMap,entry->size);
		entry->dataMap = NULL;
		entry->size = 0;
	}
	if (entry->overflowMap != (caddr_t) NULL)
	{
		munmap(entry->overflowMap,entry->overflowSize);
		entry->overflowMap = NULL;
		entry->overflowSize = 0;
	}
	close(entry->dataFD);
	close(entry->overflowFD);
	if (entry->indices)
		indexCloseIndices(entry);
}


void cacheInvalidateTable(server, db,table)
	msqld	*server;
	char	*db,
		*table;
{
	int	index;
	cache_t	*entry;


        index = 0;
        while(index < server->config.tableCache)
        {
                entry = tableCache + index++;
                if (strcmp(entry->db,db) == 0 &&
			strcmp(entry->table,table) == 0)
                {
			cacheInvalidateEntry(entry);
                }
        }
}

void cacheInvalidateDatabase(server, db)
	msqld	*server;
	char	*db;
{
	int	index;
	cache_t	*entry;


        index = 0;
        while(index < server->config.tableCache)
        {
                entry = tableCache + index++;
                if (strcmp(entry->db,db) == 0)
                {
			cacheInvalidateEntry(entry);
                }
        }
}


void cacheInvalidateCache(server)
	msqld	*server;
{
        int     index;
        cache_t *entry;

        msqlDebug0(MOD_GENERAL,"Flushing cache\n");
        index = 0;
        while(index < server->config.tableCache)
        {
                entry = tableCache + index;
                if (entry->age > 0)
                        cacheInvalidateEntry(entry);
                index++;
        }
}

void cacheSetupTableCache(server)
	msqld	*server;
{
	int	size;

	size = configGetIntEntry("system", "table_cache");
	if (size < 2)
	{
		/* 
		** Must have at least 2 cache entries or we can't
		** do a join
		*/
		size = 2;
	}
	tableCache = (cache_t*)malloc(size * sizeof(cache_t));
	bzero(tableCache, size * sizeof(cache_t));
#if defined(_OS_UNIX) || defined(_OS_OS2)
	signal(SIGALRM, cacheSyncCache);
	alarm(server->config.msyncTimer);
#endif
}



void cacheSyncCache()
{
	extern int eintrCount;
	cache_t	*cur;
	int	count,
		size;
	mIndex_t *curIdx;
	char	msg[256];
	int	forceUnmap;


	/*
	** If we have MSYNC then sync all mapped regions
	*/
#if defined(MSYNC_2) || defined(MSYNC_3)
	{

		forceUnmap = configGetIntEntry("system", "force_munmap");
		for (count = 0; count < globalServer->config.tableCache;
			 count++)
		{
			cur = tableCache + count;
			if (cur->age == 0 || cur->dirty == 0)
			{
				continue;
			}
			if (cur->size)
			{
				if (forceUnmap)
				{
					munmap(cur->dataMap,cur->size);
                                	cur->dataMap=(caddr_t)mmap(NULL,
                                        	(size_t)cur->size,
                                        	(PROT_READ | PROT_WRITE),
                                        	MAP_SHARED, cur->dataFD,
                                        	(off_t)0);
				}
				else
				{
					if(cur->dataMap && cur->size)
						MSYNC(cur->dataMap,cur->size,0);
				}
			}
			if (cur->overflowSize)
			{
				if (forceUnmap)
				{
					munmap(cur->overflowMap,
						cur->overflowSize);
                                	cur->overflowMap=(caddr_t)mmap(NULL,
                                        	(size_t)cur->overflowSize,
                                        	(PROT_READ | PROT_WRITE),
                                        	MAP_SHARED, cur->overflowFD,
                                        	(off_t)0);
				}
				else
				{
					MSYNC(cur->overflowMap,
						cur->overflowSize,0);
				}
			}
			curIdx = cur->indices;
			while(curIdx)
			{
				if (curIdx->handle.native)
				{
					idxSync(&curIdx->handle);
				}
				curIdx = curIdx->next;
			}
			snprintf(msg,256,"msyncing '%s:%s'\n",cur->db,
				cur->table);
			msqlDebug0(MOD_MMAP, msg);
		}
	}
#endif

	/*
	** Now force an fsync on all open file descriptors
	*/
	for (count = 0; count < globalServer->config.tableCache; count++)
	{
		cur = tableCache + count;
		if (cur->age == 0 || cur->dirty == 0)
		{
			continue;
		}
		fsync(cur->dataFD);
		fsync(cur->overflowFD);
		cur->dirty = 0;
	}
	sync();


#if defined(_OS_UNIX) || defined(_OS_OS2)
	signal(SIGALRM, cacheSyncCache);
	alarm(globalServer->config.msyncTimer);
#endif

	eintrCount = 0;
}

