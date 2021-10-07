/*	memory.c	- 
**
**
** Copyright (c) 1996  Hughes Technologies Pty Ltd
**
** Permission to use, copy, and distribute for non-commercial purposes,
** is hereby granted without fee, providing that the above copyright
** notice appear in all copies and that both the copyright notice and this
** permission notice appear in supporting documentation.
**
** The software may be modified for your own purposes, but modified versions
** may not be distributed.
**
** This software is provided "as is" without any expressed or implied warranty.
**
**
*/

#include <common/config.h>

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#  include <unistd.h>
#  include <stdlib.h>
#  include <string.h>
#  include <arpa/inet.h>

#  include <sys/mman.h>

#ifdef HAVE_DIRENT_H
#    include <dirent.h>
#endif
#ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
#endif

#if defined(_OS_WIN32)
#  include <winsock.h>
#endif

#include <common/debug.h>
#include <common/site.h>
#include <common/portability.h>


#if defined(_OS_WIN32)
#  include "msql_yacc.h"
#else
#  include "y.tab.h"
#endif


/*#include "msql_priv.h"*/
#include <libmsql/msql.h>
#include <common/errmsg.h>

#define REG             register


#define	MEM_ALLOC	1
#define MEM_DEALLOC	2
#define MEM_KILL_AGE	500


static char *blockTags[] = {
        "ERROR : Unknown block tag",
        "malloc()",
        "mmap()",
        "ERROR : Unknown block tag"
};



#define MEM_KILL_AGE	500


typedef struct _memBlock {
	char	allocFile[40],
		deallocFile[40];
	int	allocLine,
		deallocLine,
		type,
		status,
		age;
	caddr_t	addr;
	off_t	size;
	struct _memBlock *next;
} memBlock;

static	memBlock *memHead = NULL;



static void pushBlock(file,line,type,addr,size)
	char	*file;
	int	line,
		type;
	caddr_t	addr;
	off_t	size;
{
	memBlock *new;

	new = (memBlock *)malloc(sizeof(memBlock));
	if (!new)
	{
		perror("malloc() :");
		return;
	}
	bzero(new,sizeof(memBlock));
	(void)strcpy(new->allocFile,file);
	new->allocLine = line;
	new->type = type;
	new->addr = addr;
	new->size = size;
	new->status = MEM_ALLOC;
	if (memHead)
	{
		new->next = memHead;
	}
	else
	{
		new->next = NULL;
	}
	memHead = new;
}


static void dropBlock(file,line,addr,type)
	char	*file;
	int	line;
	caddr_t	addr;
	int	type;
{
	memBlock *cur,
		 *prev = NULL;
	int	found = 0;

	cur = memHead;
	while(cur)
	{
		if(cur->status == MEM_DEALLOC)
		{
			cur->age++;
		}
		if (cur->age > MEM_KILL_AGE)
		{
			if (prev)
			{
				prev->next = cur->next;
			}
			else
			{
				memHead = cur->next;
			}
			if (type == MMAP_BLK)
			{
				munmap(cur->addr,cur->size);
			}
			else
			{
				free(cur->addr);
			}
			free(cur);
			if (prev)
				cur = prev->next;
			else
				cur = memHead;
			continue;
		}
		if (cur->addr == addr && cur->type == type)
		{
			if (cur->status == MEM_ALLOC)
			{
				(void)strcpy(cur->deallocFile,file);
				cur->deallocLine = line;
				cur->status = MEM_DEALLOC;
			}
			else
			{
				msqlDebug0(0xffff,
					"Error: Muliple deallocation\n");
				msqlDebug2(0xffff,"\t%u bytes at %X\n",
					cur->size, cur->addr);
				msqlDebug2(0xffff,"\tAllocated at %s:%d\n",
					cur->allocFile, cur->allocLine);
				msqlDebug2(0xffff,"\tDeallocated at %s:%d\n",
					cur->deallocFile, cur->deallocLine);
				fprintf(stderr, "[%s] line %d - crash\n",
					__FILE__, __LINE__);
				abort();
			}
			found = 1;
		}
		prev = cur;
		cur = cur->next;
	}
	if (!found)
	{
		msqlDebug1(0xffff,"Error : drop of unknown memory block (%X)\n",
			addr);
	}
}



void checkBlocks(type)
	int	type;
{
	memBlock *cur;
	int	total,
		count;

	cur = memHead;
	total = count = 0;
	while(cur)
	{
		if (cur->status == MEM_DEALLOC)
		{
			cur = cur->next;
			continue;
		}
		total++;
		if (cur->type == type)
		{
			count++;
			msqlDebug1(0xffff,"%s leak :-\n", blockTags[type]);
			msqlDebug2(0xffff,"\t%u bytes at %X\n",
				cur->size, cur->addr);
			msqlDebug2(0xffff,"\tBlock created at %s:%d\n\n",
				cur->allocFile, cur->allocLine);
		}
		cur = cur->next;
	}
	msqlDebug3(0xffff,
		"Found %d leaked blocks of which %d where %s blocks\n",
		total,count,blockTags[type]);
}




caddr_t MMap(addr, len, prot, flags, fd, off,file,line)
	caddr_t addr;
	size_t len;
	int prot, flags, fd;
	off_t off;
	char	*file;
	int	line;
{
	caddr_t dest;

	dest = mmap(addr,len,prot,flags,fd,off);
	msqlDebug4(MOD_MMAP,"mmap'ing %u bytes at %X (%s:%d)\n",(unsigned)len,
		dest,file,line);
	if (dest == (caddr_t)-1)
	{
		perror("mmap");
	}
	if (debugSet(MOD_MMAP))
		pushBlock(file,line,MMAP_BLK,dest,len);
	return(dest);
}

int MUnmap(addr,len,file,line)
	caddr_t addr;
	size_t len;
	char	*file;
	int	line;
{
	int	res = 0;

	msqlDebug4(MOD_MMAP,"munmap'ing %u bytes from %X (%s:%d)\n",
		(unsigned)len,addr, file, line);
	if (debugSet(MOD_MMAP))
		dropBlock(file,line,addr,MMAP_BLK);
	else
		res = munmap(addr,len);
	if (res < 0)
	{
		perror("munmap");
	}
	return(res);
}

#define mmap(a,l,p,fl,fd,o)	MMap(a,(size_t)l,p,fl,fd,o,__FILE__,__LINE__)
#define munmap(a,l) 		MUnmap(a,(size_t)l,__FILE__,__LINE__)



void trapBigMalloc()
{
	/* This is just a debugger break point for wierd mallocs */
}

char *FastMalloc(size,file,line)
	int	size;
	char	*file;
	int	line;
{
	char	*cp;

	if (size > 10 * 1024)
		trapBigMalloc();
	cp = (char *)malloc(size);
	msqlDebug4(MOD_MALLOC,"Allocating %d bytes at %X (%s:%d)\n",size,cp,
		file,line);
	if (size > 1000000)
	{
		msqlDebug0(MOD_MALLOC,"Huge malloc trapped!\n");
		abort();
	}
	if (debugSet(MOD_MALLOC))
		pushBlock(file,line,MALLOC_BLK,cp,size);
	return(cp);
}


char *Malloc(size,file,line)
	int	size;
	char	*file;
	int	line;
{
	char	*cp;

	if (size > 10 * 1024)
		trapBigMalloc();
	cp = (char *)malloc(size);
	if (cp)
	{
		bzero(cp,size);
	}
	msqlDebug4(MOD_MALLOC,"Allocating %d bytes at %X (%s:%d)\n",size,cp,
		file,line);
	if (size > 1000000)
	{
		msqlDebug0(MOD_MALLOC,"Huge malloc trapped!\n");
		abort();
	}
	if (debugSet(MOD_MALLOC))
		pushBlock(file,line,MALLOC_BLK,cp,size);
	return(cp);
}

void Free(addr, file,line)
	char	*addr,
		*file;
	int	line;
{
	msqlDebug3(MOD_MALLOC,"Freeing address %X (%s:%d)\n",addr,
		file,line);
	if (debugSet(MOD_MALLOC))
		dropBlock(file,line,addr,MALLOC_BLK);
	else
		(void) free(addr);
}



char *Strdup(s, file, line)
	char	*s,
		*file;
	int	line;
{
	char	*new;
	int	len;

	len = strlen(s) + 1;
	new = Malloc(len, file, line);
	if (new)
	{
		bcopy(s, new, len);
	}
	return(new);
}



#define malloc(s)		Malloc(s,__FILE__,__LINE__)
#define fastMalloc(s)		FastMalloc(s,__FILE__,__LINE__)
#define free(a)			Free((char *)a,__FILE__,__LINE__)
#define	safeFree(x)		{if(x) { (void)free(x); x = NULL; } }


