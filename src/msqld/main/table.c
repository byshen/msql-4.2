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
** $Id: table.c,v 1.32 2011/10/14 11:32:14 bambi Exp $
**
*/

/*
** Module	: main : table
** Purpose	: 
** Exports	: 
** Depends Upon	: 
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <common/config.h>
#include <common/config_extras.h>
#include <common/config/config.h>

#include <stdio.h>
#include <stdlib.h>
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

#ifdef HAVE_DIRENT_H
#    include <dirent.h>
#endif

#ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
#endif

#if HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>


#include <common/portability.h>
#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/version.h>
#include <msqld/main/table.h>
#include <msqld/main/sysvar.h>
#include <msqld/main/varchar.h>
#include <msqld/main/index.h>
#include <msqld/main/parse.h>
#include <msqld/main/memory.h>
#include <msqld/main/cache.h>
#include <common/types/types.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

extern	char		errMsg[];
extern	cache_t 	*tableCache;

char *msql_tmpnam();

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

static mField_t *_readTableDef(server, table, alias, db)
	msqld		*server;
	char		*table,
			*alias,
			*db;
{
	mField_t	*headField=NULL,
			*tmpField=NULL,
			*prevField=NULL,
			*curField=NULL;
	char		path[MSQL_PATH_LEN];
	int		numFields,
			numBytes,
			fieldCount,
			fd,
			version;
	static	char 	buf[MAX_FIELDS * sizeof(mField_t) + sizeof(int)];

	debugTrace(TRACE_IN,"readTableDef()");
	(void)snprintf(path, MSQL_PATH_LEN, "%s/%s/%s.def",
		server->config.dbDir, db,table);
	fd = open(path,O_RDONLY,0);
	if (fd < 0)
	{
		snprintf(errMsg, MAX_ERR_MSG, BAD_TABLE_ERROR,table);
		msqlDebug1(MOD_ERR,"Unknown table \"%s\"\n",table);
		debugTrace(TRACE_OUT,"readTableDef()");
		return(NULL);
	}
	numBytes = read(fd,buf,sizeof(buf));
	if (numBytes < 1)
	{
		snprintf(errMsg, MAX_ERR_MSG, TABLE_READ_ERROR,table,
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,
			"Error reading table \"%s\" definition\n",
			table);
		close(fd);
		debugTrace(TRACE_OUT,"readTableDef()");
		return(NULL);
	}

	bcopy(buf,&version,sizeof(int));
	if (version != DB_VERSION)
	{
		snprintf(errMsg, MAX_ERR_MSG, 
			"Table \"%s\" wrong database format\n", table);
		msqlDebug1(MOD_ERR,
			"Table \"%s\" wrong database format\n", table);
		close(fd);
		debugTrace(TRACE_OUT,"readTableDef()");
		return(NULL);
	}
	numFields = (numBytes - sizeof(int)) / sizeof(mField_t);
	fieldCount = 0;
	headField = NULL;
	while(fieldCount < numFields)
	{
		tmpField = (mField_t *)(buf + sizeof(int) +
			(fieldCount * sizeof(mField_t)));
		curField = memMallocField();
		if (!headField)
		{
			headField = prevField = curField;
		}
		else
		{
			prevField->next = curField;
			prevField = curField;
		}
		/*bcopy(tmpField, curField, sizeof(mField_t));*/
		memCopyField(tmpField, curField);
		if (alias)
		{
			strcpy(curField->table,alias);
		}
		curField->value = NULL;
		fieldCount++;
	}
	close(fd);
	debugTrace(TRACE_OUT,"readTableDef()");
	return(headField);
}



static int findRowLen(cacheEntry)
	cache_t	*cacheEntry;
{
	int	rowLen;
	mField_t	*fieldDef;

	rowLen = 0;
	fieldDef = cacheEntry->def;
	while(fieldDef)
	{
		rowLen += fieldDef->dataLength +1;  /* +1 for NULL indicator */
		fieldDef = fieldDef->next;
	}
	return(rowLen);
}



static cache_t *_createTargetTable(path,name,table1,table2,fields, flag)
	char	*path,
		*name;
	cache_t	*table1,
		*table2;
	mField_t	*fields;
	int	flag;
{
	cache_t 	*new;
	mField_t	*curField,
			*newField,
			*tmpField;
	int		fd,
			foundField,
			foundTable,
			curOffset = 0;
	sblk_t		sblk;


	/*
	** start building the table cache entry
	*/
	debugTrace(TRACE_IN,"createTargetTable()");
	new = (cache_t *)malloc(sizeof(cache_t));
	if (!new)
	{
		snprintf(errMsg,MAX_ERR_MSG,TMP_MEM_ERROR);
		msqlDebug1(MOD_ERR,"Out of memory for temporary table (%s)\n"
		,path);
		debugTrace(TRACE_OUT,"createTargetTable()");
		return(NULL);
	}
	bzero(new, sizeof(cache_t));
	(void)strcpy(new->table,name);
	fd = open(path,O_RDWR|O_CREAT|O_TRUNC, 0700);
	if (fd < 0)
	{
		snprintf(errMsg,MAX_ERR_MSG,TMP_CREATE_ERROR, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Couldn't create temporary table (%s)\n",
			path);
		(void)free(new);
		debugTrace(TRACE_OUT,"createTargetTable()");
		return(NULL);
	}
	bzero(&sblk, SBLK_SIZE);
	sblk.version = DB_VERSION;
	sblk.activeRows = sblk.numRows = sblk.dataSize = 0;
	sblk.sequence.step = sblk.sequence.value = 0;
	sblk.freeList = NO_POS;
	if (write(fd, &sblk, SBLK_SIZE) < SBLK_SIZE)
	{
		snprintf(errMsg,MAX_ERR_MSG, TMP_CREATE_ERROR, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Couldn't create temporary table (%s)\n",
			path);
		(void)free(new);
		debugTrace(TRACE_OUT,"createTargetTable()");
		return(NULL);
	}
	new->dataFD = fd;
	new->result = 1;
	new->size = SBLK_SIZE;
	new->dirty = 0;

	/*
	** Map the tmp table and setup the superblock
	*/
	new->dataMap = (caddr_t)mmap(NULL, (size_t)new->size,
		(PROT_READ|PROT_WRITE), MAP_SHARED, new->dataFD, (off_t)0);
	if (new->dataMap == (caddr_t)-1)
	{
		free(new);
		perror("mmap data file");
		return(NULL);
	}
#ifdef HAVE_MADVISE
	madvise(new->dataMap, new->size, MADV_SEQUENTIAL);
#endif
	new->sblk=(sblk_t*)new->dataMap;


	/*
	** Add the field definitions. Ensure that any key fields are
	** not flagged as such as we can't access the key data of the
	** original table when accessing this.
	*/
	curField = table1->def;
	newField = NULL;
	while(curField)
	{
		/*
		** If we've been given a list of fields, only add this
		** field to the tmp table if it's in the list.
		*/
		if (fields && flag == QUERY_FIELDS_ONLY)
		{
			foundField = 0;
			tmpField = fields;
			while(tmpField)
			{
				if(strcmp(tmpField->name,curField->name)==0 &&
				   strcmp(tmpField->table,curField->table)==0)
				{
					foundField = 1;
					break;
				}
				tmpField = tmpField->next;
			}
			if (!foundField)
			{
				curField = curField->next;
				continue;
			}
		}

		/*
		** O.k.  Add this field
		*/
		if (newField)
		{
			newField->next = memMallocField();
			newField = newField->next;
		}
		else
		{
			new->def = memMallocField();
			newField = new->def;
		}
		memCopyField(curField,newField);
		if (newField->function)
			newField->function = NULL;
		if( *(newField->table) == 0)
		{
			(void)strcpy(newField->table,table1->table);
		}
		new->rowLen += curField->dataLength + 1;
		newField->offset = curOffset;
		curOffset += curField->dataLength + 1;
		newField->value = NULL;
		newField->entry = curField->entry;
		if (newField->entry == NULL)
			newField->entry = table1;
		curField = curField->next;
	}
	if (table2)
	{
		curField = table2->def;
		while(curField)
		{
			/*
			** If we've been given a list of fields, only add this
			** field to the tmp table if it's in the list.
			*/
			if (fields && flag == QUERY_FIELDS_ONLY)
			{
				foundField = 0;
				tmpField = fields;
				while(tmpField)
				{
					if(strcmp(tmpField->name,
						curField->name)==0 &&
				   	strcmp(tmpField->table,
						curField->table)==0)
					{
						foundField = 1;
						break;
					}
					tmpField = tmpField->next;
				}
				if (!foundField)
				{
					curField = curField->next;
					continue;
				}
			}

			/*
			** Add it.
			*/
			if (newField)
			{
				newField->next = memMallocField();
				newField = newField->next;
			}
			else
			{
				new->def = newField = memMallocField();
			}
			/*bcopy(curField,newField,sizeof(mField_t));*/
			memCopyField(curField,newField);
			if( *(newField->table) == 0)
			{
				(void)strcpy(newField->table,table2->table);
			}
			new->rowLen += curField->dataLength + 1;
			newField->offset = curOffset;
			newField->value = NULL;
			newField->entry = curField->entry;
			if (newField->entry == NULL)
				newField->entry = table2;
			curOffset += curField->dataLength + 1;
			curField = curField->next;
		}
	}

	/*
	** Create real fields for any function return values
	*/
	curField = fields;
	while(curField)
	{
		if (curField->function == NULL)
		{
			curField = curField->next;
			continue;
		}

		if (newField)
		{
			newField->next = memMallocField();
			newField = newField->next;
		}
		else
		{
			new->def = memMallocField();
			newField = new->def;
		}
		/*bcopy(curField,newField,sizeof(mField_t));*/
		memCopyField(curField,newField);
		if (newField->function)
			newField->function = NULL;
		if( *(newField->table) == 0)
		{
			(void)strcpy(newField->table,table1->table);
		}
		new->rowLen += curField->dataLength + 1;
		newField->offset = curOffset;
		curOffset += curField->dataLength + 1;
		newField->value = NULL;
		newField->entry = curField->entry;
		if (newField->entry == NULL)
			newField->entry = table1;
		curField = curField->next;
	}


	/*
	** Now the ugly bit.  We have to create "real" fields for any
	** system variables the we want selected.  This is so that we
	** get the actual sysvar from the original table (such as _rowid)
	** rather than getting it generated on the fly from the result
	** table.
	*/
	curField = fields;
	while(curField)
	{
		if (*(curField->name) == '_')
		{
			foundTable = 0;
			if (strcmp(curField->table, table1->table) == 0)
			{
				foundTable = 1;
			}
			else if (table2)
			{
				if(strcmp(curField->table,table2->table) == 0)
				{
					foundTable = 1;
				}
			}
			if ( !foundTable)
			{
				curField = curField->next;
				continue;
			}

			/*
			** Add the field definition
			*/

			if (newField)
			{
				newField->next=sysvarGetDefinition(curField);
				newField = newField->next;
			}
			else
			{
				new->def=newField=sysvarGetDefinition(curField);
			}
			if (!newField)
			{
				snprintf(errMsg, MAX_ERR_MSG, SYSVAR_ERROR, 
					curField->name);
				return(NULL);
			}
			new->rowLen += newField->dataLength + 1;
			newField->offset = curOffset;
			curOffset += newField->dataLength + 1;
		}
		curField = curField->next;
	}

	if (newField)	
	{
		newField->next = NULL;
	}

	/*
	** Setup the 8 byte padding for the new table row
	*/
	new->rowDataLen = new->rowLen;
        new->rowLen = new->rowDataLen + (8 -  
		((new->rowDataLen + HEADER_SIZE) % 8));


	new->row.buf = (u_char *)malloc(new->rowLen + HEADER_SIZE + 2);
	new->row.header = (hdr_t *)new->row.buf;
	new->row.data = new->row.buf + HEADER_SIZE;
	new->fileLayout = LAYOUT_FLAT;
	debugTrace(TRACE_OUT,"createTargetTable()");
	return(new);
}



static int _tableWriteRow(cacheEntry,row,rowNum, active, query)
	cache_t		*cacheEntry;
	row_t		*row;
	u_int		rowNum;
	int		active;
	mQuery_t	*query;
{
	off_t		seekPos;
	char		*buf;
	struct stat 	sbuf;
	int		expandSize;

	if (cacheEntry->dataFD < 0)
	{
		abort();
	}

	cacheEntry->dirty = 1;
	if (rowNum == NO_POS)
	{
		msqlDebug1(MOD_ACCESS,"tableWriteRow() : append to %s\n",
			(cacheEntry->result)?cacheEntry->resInfo:
			cacheEntry->table);

	}
	else
	{
		msqlDebug2(MOD_ACCESS,
			"tableWriteRow() : write at row %u of %s\n",
			rowNum, (cacheEntry->result)?cacheEntry->resInfo:
			cacheEntry->table);
	}


	/*
	** Write the row to the table
	*/
	if (rowNum == NO_POS)  /* append */
	{
		seekPos = cacheEntry->sblk->numRows * 
			(cacheEntry->rowLen + HEADER_SIZE) + SBLK_SIZE;
		cacheEntry->sblk->numRows++;
	}
	else
	{
		seekPos = rowNum * (cacheEntry->rowLen + HEADER_SIZE) +
			SBLK_SIZE;
	}

	if (seekPos + cacheEntry->rowLen + HEADER_SIZE >= cacheEntry->size)
	{
		if (cacheEntry->result)
			expandSize = 2048;
		else
			expandSize = 512;
               	munmap(cacheEntry->dataMap, cacheEntry->size);
		cacheEntry->dataMap = NULL;
		cacheEntry->size = 0;
		lseek(cacheEntry->dataFD, seekPos + 
			(expandSize * cacheEntry->rowLen + HEADER_SIZE),
			SEEK_SET);
		write(cacheEntry->dataFD, "\0", 1);
		fstat(cacheEntry->dataFD, &sbuf);
		cacheEntry->size = sbuf.st_size;
		if (cacheEntry->size)
		{
			cacheEntry->dataMap = (caddr_t)mmap(NULL, 
				(size_t)cacheEntry->size,(PROT_READ|PROT_WRITE),
				MAP_SHARED, cacheEntry->dataFD, (off_t)0);
			if (cacheEntry->dataMap == (caddr_t)-1)
			{
				perror("mmap data file");
				return(-1);
			}
#ifdef HAVE_MADVISE
			madvise(cacheEntry->dataMap, cacheEntry->size, 
					MADV_SEQUENTIAL);
#endif
			cacheEntry->sblk=(sblk_t*)cacheEntry->dataMap;
			cacheEntry->sblk->dataSize = cacheEntry->size;
		}
	}
	buf = ((char *)cacheEntry->dataMap) + seekPos;
	if (row)
	{
		if (active)
			row->header->active = 1;
		bcopy(&(query->queryTime),&(row->header->timestamp),
			sizeof(query->queryTime));
		bcopy(row->buf, buf, HEADER_SIZE +
			cacheEntry->rowLen);
	}
	else
	{
		if (active)
			cacheEntry->row.header->active = 1;
		bcopy(&(query->queryTime),&(cacheEntry->row.header->timestamp),
			sizeof(query->queryTime));
		bcopy(cacheEntry->row.buf, buf,
			cacheEntry->rowLen + HEADER_SIZE);
	}
	return(0);
}




/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/






/****************************************************************************
** 	_tableOpenTable
**
**	Purpose	: Open the datafile for a given table
**	Args	: Database and Table names
**	Returns	: file descriptor for the data file
**	Notes	: 
*/

int tableOpenTable(server,entry, table,db)
	msqld	*server;
	cache_t	*entry;
	char	*table;
	char	*db;
{
	char	path[MSQL_PATH_LEN];
	int	fd;

	if (entry->fileLayout == LAYOUT_FLAT || strcmp(db,".tmp") == 0)
	{
		(void)snprintf(path, MSQL_PATH_LEN, "%s/%s/%s.dat",
			server->config.dbDir,db, table);
		fd = open(path,O_RDWR | O_CREAT, 0600);
	}
	else
	{
		(void)snprintf(path, MSQL_PATH_LEN, "%s/%s/%02d",
			server->config.dbDir,db, entry->dirHash);
		mkdir(path, 0600);
		(void)snprintf(path, MSQL_PATH_LEN, "%s/%s/%02d/%s.dat",
			server->config.dbDir,db, entry->dirHash, table);
		fd = open(path,O_RDWR | O_CREAT, 0600);
	}
	return(fd);
}


int tableOpenOverflow(server,entry,table,db)
	msqld	*server;
	cache_t	*entry;
	char	*table;
	char	*db;
{
	char	path[MSQL_PATH_LEN + 1];

	if (entry->fileLayout == LAYOUT_FLAT || strcmp(db,".tmp") == 0)
	{
		(void)snprintf(path, MSQL_PATH_LEN,"%s/%s/%s.ofl",
			server->config.dbDir,db, table);
	}
	else
	{
		(void)snprintf(path, MSQL_PATH_LEN,"%s/%s/%02d/%s.ofl",
			server->config.dbDir,db,entry->dirHash,table);
	}
	return(open(path,O_RDWR|O_CREAT, 0600));
}







int tableWriteFreeList(cacheEntry, pos, entry)
	cache_t	*cacheEntry;
	u_int	pos,
		entry;
{
	off_t	seekPos;
	char	*buf;


	seekPos = pos * (cacheEntry->rowLen + HEADER_SIZE) + SBLK_SIZE +
		HEADER_SIZE;
	buf = ((char *)cacheEntry->dataMap) + seekPos;
	bcopy(&entry,buf,sizeof(entry));
	return(0);
}


u_int tableReadFreeList(cacheEntry,pos)
	cache_t	*cacheEntry;
	u_int	pos;
{
	off_t	seekPos;
	char	*buf;
	u_int	entry;


	seekPos = pos * (cacheEntry->rowLen + HEADER_SIZE) + SBLK_SIZE +
		HEADER_SIZE;
	buf = ((char *)cacheEntry->dataMap) + seekPos;
	bcopy(buf, &entry, sizeof(entry));
	return(entry);
}



/****************************************************************************
** 	_popBlankPos
**
**	Purpose	: Pop the localtion of a hole from the table's free list
**	Args	: Database and table names
**	Returns	: Offset off hole, NO_POS if the free list is empty
**	Notes	: 
*/


u_int tablePopBlankPos(cacheEntry,db,table)
	cache_t	*cacheEntry;
	char	*db,
		*table;
{
	u_int	pos;
	sblk_t	*sblkPtr;

	debugTrace(TRACE_IN,"popBlankPos()");
	sblkPtr = (sblk_t *)cacheEntry->dataMap;
	if (!sblkPtr)
		return(NO_POS);
	pos = sblkPtr->freeList;
	if (pos != NO_POS)
	{
		sblkPtr->freeList = tableReadFreeList(cacheEntry,
			sblkPtr->freeList);
	}
	debugTrace(TRACE_OUT,"popBlankPos()");
	return(pos);
}


/****************************************************************************
** 	_pushBlankPos
**
**	Purpose	: Store the location of a data hole 
**	Args	: database and table names
**		  offset for the hole.
**	Returns	: -1 on error
**	Notes	: 
*/

int tablePushBlankPos(cacheEntry,db,table,pos)
	cache_t	*cacheEntry;
	char	*db,
		*table;
	u_int	pos;
{
	sblk_t	 *sblkPtr;

	debugTrace(TRACE_IN,"pushBlankPos()");

	sblkPtr = (sblk_t *)cacheEntry->dataMap;
	if (sblkPtr->freeList == NO_POS)
	{
		if (tableWriteFreeList(cacheEntry,pos,NO_POS) < 0)
		{
			return(-1);
		}
	}
	else
	{
		if (tableWriteFreeList(cacheEntry,pos,sblkPtr->freeList) < 0)
		{
			return(-1);
		}
	}
	sblkPtr->freeList = pos;
	debugTrace(TRACE_OUT,"pushBlankPos()");
	return(0);
}







/****************************************************************************
** 	_tableFreeDefinition
**
**	Purpose	: Free memory used by a table def
**	Args	: pointer to table def
**	Returns	: Nothing
**	Notes	: 
*/

void tableFreeDefinition(tableDef)
	mField_t	*tableDef;
{
	mField_t	*curField,
			*prevField;

	debugTrace(TRACE_IN,"tableFreeDefinition()");
	curField = tableDef;
	while(curField)
	{
		if (curField->value)
		{
			parseFreeValue(curField->value);
			curField->value = NULL;
		}
		prevField = curField;
		curField = curField->next;
		prevField->next = NULL;
		memFreeField(prevField);
	}
	debugTrace(TRACE_OUT,"tableFreeDefinition()");
}





void tableCleanTmpDir(server)
	msqld	*server;
{
#ifdef HAVE_DIRENT_H
        struct  dirent *cur;
#else
        struct  direct *cur;
#endif
	DIR	*dirp;
	char	*dbDir,
		path[MSQL_PATH_LEN];

	if (server)
		dbDir = server->config.dbDir;
	else
		dbDir = configGetCharEntry("general", "db_dir");
	(void)snprintf(path,MSQL_PATH_LEN,"%s/.tmp", dbDir);
	dirp = opendir(path);
	if (!dirp)
	{
		/* Can't report this.  It'll be trapped later */
		return;
        }

        /*
        ** Blow away any files skipping . and ..
        */

	cur = readdir(dirp);
	while(cur)
	{
		if (*cur->d_name == '.')
		{
			cur = readdir(dirp);
			continue;
		}
		(void)snprintf(path,MSQL_PATH_LEN,"%s/.tmp/%s",dbDir,
			cur->d_name);
		unlink(path);
		cur = readdir(dirp);
	}
	closedir(dirp);
        return;
}





cache_t *tableCreateTmpTable(server,db,name,table1,table2,fields,flag)
	msqld		*server;
	char		*db,
			*name;
	cache_t		*table1,
			*table2;
	mField_t	*fields;
	int		flag;
{
	cache_t 	*new;
	char		path[MSQL_PATH_LEN],
			*tmpfile = NULL,
			*tmpStr = NULL,
			*cp = NULL;
	int		fd;
        u_int           freeList;


	/*
	** Create a name for this tmp table if we need to
	*/
	debugTrace(TRACE_IN,"createTmpTable()");
	if (name)
	{
		strcpy(path,name);
		snprintf(path,MSQL_PATH_LEN,"%s/%s/%s.dat",
			server->config.dbDir, db, name);
	}
	else
	{
		tmpStr = tmpfile = (char *)msql_tmpnam(NULL);
		cp = (char *)rindex(tmpfile,'/');
		if (cp)
		{
			tmpfile = cp+1;
		}
		snprintf(path,MSQL_PATH_LEN,"%s/.tmp/%s.dat",
			server->config.dbDir, tmpfile);
	}
	new = _createTargetTable(path,tmpfile,table1,table2,fields,flag);

	/* 
	** Create an empty overflow file
	*/
	cp = rindex(path, '.');
	*cp = 0;
	strcat(cp,".ofl");
	fd = open(path,O_CREAT | O_WRONLY , 0600);
	if (fd < 0)
	{
		unlink(path);
		snprintf(errMsg,MAX_ERR_MSG,DATA_FILE_ERROR,"tmp table", 
			(char*)strerror(errno));
		msqlDebug0(MOD_ERR,"Error creating tmp table file\n");
		return(NULL);
	}
	freeList = NO_POS;
	if (write(fd,&freeList,sizeof(u_int)) < sizeof(u_int))
	{
		close(fd);
		unlink(path);
		snprintf(errMsg,MAX_ERR_MSG,DATA_FILE_ERROR,"tmp table", 
			(char*)strerror(errno));
		msqlDebug0(MOD_ERR,"Error creating tmp table file\n");
		return(NULL);
	}
	close(fd);


	debugTrace(TRACE_OUT,"createTmpTable()");
	free(tmpStr);
	return(new);
}

cache_t *tableCreateDestTable(server, db, name, table, fields)
	msqld	*server;
	char	*db,
		*name;
	cache_t	*table;
	mField_t	*fields;
{
	cache_t	*new;
	char	path[MSQL_PATH_LEN];

	snprintf(path,MSQL_PATH_LEN,"%s/%s/%s.dat", server->config.dbDir, 
		db, name);
	new = _createTargetTable(path,name,table, NULL, fields, 0);
	return(new);
}




void tableFreeTmpTable(server, entry)
	msqld	*server;
	cache_t	*entry;
{
	char	path[MSQL_PATH_LEN],
		*cp;

        debugTrace(TRACE_IN,"freeTmpTable()");
	(void)snprintf(path,MSQL_PATH_LEN, "%s/.tmp/%s.dat",
		server->config.dbDir, entry->table);

	tableFreeDefinition(entry->def);
	entry->def = NULL;
	*(entry->db) = 0;
	*(entry->table) = 0;
	*(entry->cname) = 0;
	entry->age = 0;
	free(entry->row.buf);
	entry->row.buf = NULL;
	if (entry->dataMap != (caddr_t) NULL && (entry->size > 0))
	{
		munmap(entry->dataMap,entry->size);
	}
	entry->dataMap = NULL;
	entry->size = 0;
	close(entry->dataFD);
	indexCloseIndices(entry);
	(void)free(entry);
	unlink(path);
	cp = rindex(path, '.');
	if (cp)
	{
		*cp = 0;
		strcat(path, ".ofl");
		unlink(path);
	}
        debugTrace(TRACE_OUT,"freeTmpTable()");
}




/****************************************************************************
** 	_tableLoadDefinition
**
**	Purpose	: Locate a table definition
**	Args	: Database and Table names
**	Returns	: -1 on error
**	Notes	: Table description cache searched first.  If it's not
**		  there, the LRU entry is freed and the table def is
**		  loaded into the cache.  The tableDef, 
**		  cacheEntry and dataFD globals are set.
*/


cache_t *tableLoadDefinition(server, table,cname,db)
	msqld		*server;
	char		*table,
			*cname,
			*db;
{
	int		maxAge,
			cacheIndex;
	mField_t	*def,
			*curField;
	cache_t		*entry;
	int		count;
	char	 	*tableName;


	/*
	** Look for the entry in the cache.  Keep track of the oldest
	** entry during the pass so that we can replace it if needed
	*/
	debugTrace(TRACE_IN,"tableLoadDefinition()");
	msqlDebug2(MOD_CACHE,"Table cache search for %s:%s\n",table,db);
	count = cacheIndex = 0;
	maxAge = -1;
	if (cname)
	{
		if (!*cname)
		{
			cname = NULL;
		}
	}
	while(count < server->config.tableCache)
	{
		entry = &tableCache[count];
		msqlDebug5(MOD_CACHE,"Cache entry %d = %s:%s, age = %d (%s)\n", 
			count,
			*(entry->table)?entry->table:"NULL",
			*(entry->db)?entry->db:"NULL", 
			entry->age,
			entry->def?"OK":"NULL Def!!!");
		if (entry->age > 0 && *entry->db == *db)
		{
		    if (strcmp(entry->db,db)==0 && 
		    	strcmp(entry->table,table)==0 &&
		    	strcmp((cname)?cname:"",entry->cname)==0)
			{
				msqlDebug1(MOD_CACHE,"Found cache entry at %d\n"
					, count);
				entry->age = 1;
				debugTrace(TRACE_OUT,"tableLoadDefinition()");
				return(entry);
			}
		}
		if (entry->age > 0)
			entry->age++;

		/*
		** Empty entries have an age of 0.  If we're marking
		** an empty cache position just keep the mark
		*/
		if ((entry->age == 0) && (maxAge != 0))
		{
			maxAge = entry->age;
			cacheIndex = count;
		}
		else
		{
			if ((entry->age > maxAge) && (maxAge != 0))
			{
				maxAge = entry->age;
				cacheIndex = count;
			}
		}
		count++;
	}

	/*
	** It wasn't in the cache.  Free up the oldest cache entry 
	*/

	entry = &tableCache[cacheIndex];
	if(entry->def)
	{
		msqlDebug3(MOD_CACHE,"Removing cache entry %d (%s:%s)\n", 
			cacheIndex, entry->db, entry->table);
		if (entry->dataMap != (caddr_t) NULL && entry->size > 0)
		{
			MSYNC(entry->dataMap,entry->size,0);
			munmap(entry->dataMap,entry->size);
		}
		entry->dataMap = NULL;
		entry->size = 0;
		if (entry->overflowMap != (caddr_t) NULL  &&
		    entry->overflowSize > 0)
		{
			MSYNC(entry->overflowMap,entry->overflowSize,0);
			munmap(entry->overflowMap,entry->overflowSize);
		}
		entry->overflowMap = NULL;
		entry->overflowSize = 0;
		(void)close(entry->dataFD);
		(void)close(entry->overflowFD);
		indexCloseIndices(entry);
		tableFreeDefinition(entry->def);
		entry->def = NULL;
		*(entry->table)=0;
		*(entry->db)=0;
		entry->age=0;
		free(entry->row.buf);
		entry->row.buf = NULL;
	}

	/*
	** Now load the new entry
	*/
	*errMsg = 0;
	if (cname)
	{
		tableName = cname;
		def = _readTableDef(server,cname,table,db);
	}
	else
	{
		tableName = table;
		def = _readTableDef(server,table,NULL,db);
	}
	if (!def)
	{
		if (*errMsg == 0)
		{
			snprintf(errMsg, MAX_ERR_MSG,TABLE_READ_ERROR,table, 
				(char*)strerror(errno));
			msqlDebug1(MOD_ERR,
				"Couldn't read table definition for %s\n",
				table);
		}
		debugTrace(TRACE_OUT,"tableLoadDefinition()");
		return(NULL);
	}
	entry->def = def;
	entry->fileLayout = tableCheckFileLayout(server, tableName, db);
	entry->dirHash = tableDirHash(tableName);
	entry->indices = indexLoadIndices(server, entry, tableName,db);
	entry->age = 1;
	entry->result = 0;
	entry->dirty = 0;
	strcpy(entry->db,db);
	strcpy(entry->table,table);
	if (cname)
	{
		strcpy(entry->cname,cname);
	}
	else
	{
		*(entry->cname) = 0;
	}
	
	msqlDebug3(MOD_CACHE,"Loading cache entry %d (%s:%s)\n", cacheIndex, 
		entry->db, entry->table);
	entry->dataFD = tableOpenTable(server, entry, tableName, db);
	if(entry->dataFD < 0)
	{
		indexCloseIndices(entry);
		tableFreeDefinition(entry->def);
		entry->def = NULL;
		*(entry->table)=0;
		*(entry->db)=0;
		entry->age=0;
		snprintf(errMsg, MAX_ERR_MSG, DATA_OPEN_ERROR,tableName, 
			(char *)strerror(errno));
		debugTrace(TRACE_OUT,"tableLoadDefinition()");
		return(NULL);
	}
	entry->overflowFD = tableOpenOverflow(server,entry,tableName,db);
	if(entry->overflowFD < 0)
	{
		close(entry->dataFD);
		indexCloseIndices(entry);
		tableFreeDefinition(entry->def);
		entry->def = NULL;
		*(entry->table)=0;
		*(entry->db)=0;
		entry->age=0;
		snprintf(errMsg,MAX_ERR_MSG,DATA_OPEN_ERROR,tableName, 
			(char *)strerror(errno));
		debugTrace(TRACE_OUT,"tableLoadDefinition()");
		return(NULL);
	}
	curField = entry->def;
	while(curField)
	{
		curField->entry = entry;
		curField = curField->next;
	}

	/*
	** Setup for Mapping the data file
	*/
	entry->dataMap = NULL;
	entry->remapData = 1;
	entry->overflowMap = NULL;
	entry->remapOverflow = 1;
	if (tableInitTable(entry,FULL_REMAP) < 0)
	{
		close(entry->overflowFD);
		close(entry->dataFD);
		indexCloseIndices(entry);
		tableFreeDefinition(entry->def);
		entry->def = NULL;
		*(entry->table) = 0;
		*(entry->db) = 0;
		entry->age = 0;
		return(NULL);
	}

	/*
	** Set the globals and bail.  We need rowLen + 2 (one for the
	** active byte and also one for regexp over-run protection) and
	** keyLen + 1 (one for the active byte) buffers for performance.
	** Keep rows aligned on 8 byte blocks so mmap() access to the 
	** headers is OK.
	*/
	entry->rowDataLen = findRowLen(entry);
        entry->rowLen = entry->rowDataLen + (8 -  
		((entry->rowDataLen + HEADER_SIZE) % 8));
	entry->row.buf = (u_char *)malloc(entry->rowLen + HEADER_SIZE + 2);
	bzero(entry->row.buf, entry->rowLen + HEADER_SIZE + 2);
	entry->row.header = (hdr_t *)entry->row.buf;
        entry->row.data = entry->row.buf + HEADER_SIZE;
	entry->sblk = (sblk_t *)entry->dataMap;
	debugTrace(TRACE_OUT,"tableLoadDefinition()");
	return(entry);
}







/****************************************************************************
** 	_tableInitTable
**
**	Purpose	: Reset table pointers used during query processing
**	Args	: None
**	Returns	: Nothing
**	Notes	: This just puts the file into a known state, particular
**		  the current seek pointers.
*/

int tableInitTable(cacheEntry,mapFlag)
	cache_t	*cacheEntry;
	int	mapFlag;
{
	struct	stat sbuf;
	int	forcedRemap;

	debugTrace(TRACE_IN,"tableInitTable()");

	forcedRemap = 0;
	if (cacheEntry->dataMap != NULL &&
	    cacheEntry->size != cacheEntry->sblk->dataSize)
	{
		forcedRemap = 1;
	}

	if ((mapFlag & FULL_REMAP) || forcedRemap)
	{
		if (cacheEntry->remapData || forcedRemap)
		{
			if (cacheEntry->dataMap && cacheEntry->size)
			{
				munmap(cacheEntry->dataMap, cacheEntry->size);
			}
			if (fstat(cacheEntry->dataFD, &sbuf) < 0)
			{
				perror("fstat");
				return(-1);
			}
			cacheEntry->size = sbuf.st_size;
			if (cacheEntry->size)
			{
				cacheEntry->dataMap = (caddr_t)mmap(NULL, 
					(size_t)cacheEntry->size, 
					(PROT_READ | PROT_WRITE), 
					MAP_SHARED, cacheEntry->dataFD, 
					(off_t)0);
				if (cacheEntry->dataMap == (caddr_t)-1)
				{
					perror("mmap data file");
					return(-1);
				}
#ifdef HAVE_MADVISE
				madvise(cacheEntry->dataMap, cacheEntry->size, 
					MADV_SEQUENTIAL);
#endif
				cacheEntry->sblk=(sblk_t*)cacheEntry->dataMap;
			}
			cacheEntry->remapData = 0;
		}
		if ((cacheEntry->remapOverflow || forcedRemap) &&
		    (cacheEntry->overflowFD != 0))
		{
			if (cacheEntry->overflowMap && cacheEntry->overflowSize)
			{
				munmap(cacheEntry->overflowMap,
					cacheEntry->overflowSize);
			}
			fstat(cacheEntry->overflowFD, &sbuf);
			cacheEntry->overflowSize = sbuf.st_size;
			if (cacheEntry->overflowSize)
			{
				cacheEntry->overflowMap = (caddr_t)mmap(NULL, 
					(size_t)cacheEntry->overflowSize, 
					(PROT_READ | PROT_WRITE), 
					MAP_SHARED, cacheEntry->overflowFD, 
					(off_t)0);
				if (cacheEntry->overflowMap == (caddr_t)-1)
				{
					perror("mmap overflow file");
					return(-1);
				}
#ifdef HAVE_MADVISE
				madvise(cacheEntry->overflowMap, 
					cacheEntry->overflowSize, 
					MADV_SEQUENTIAL);
#endif
			}
			cacheEntry->remapOverflow = 0;
		}
	}
	debugTrace(TRACE_OUT,"tableInitTable()");
	return(0);
}

	





int tableWriteRow(cacheEntry,row,rowNum, query)
	cache_t		*cacheEntry;
	row_t		*row;
	u_int		rowNum;
	mQuery_t	*query;
{
	return(_tableWriteRow(cacheEntry,row,rowNum, 1, query));
}


int tablePlaceRow(cacheEntry,row,rowNum, query)
	cache_t		*cacheEntry;
	row_t		*row;
	u_int		rowNum;
	mQuery_t	*query;
{
	return(_tableWriteRow(cacheEntry,row,rowNum, 0, query));
}


/****************************************************************************
** 	_tableReadRow
**
**	Purpose	: Grab a row from a datafile
**	Args	: datafile FD, length the row, pointer to active flag buf
**	Returns	: pointer to static row buffer
**	Notes	: Some boxes (e.g. early SPARCs etc) don't do multiply
**		  in hardware.  As tableReadRow() is the most called function
**		  in the entire code we do a hack to reduce the hit
**		  on boxes without hardware math.
*/

int tableReadRow(cacheEntry,row,rowNum)
	cache_t		*cacheEntry;
	row_t		*row;
	u_int		rowNum;
{
	off_t		seekPos;
	static	u_int	lastRow,
			lastLen;
	static	off_t	lastPos;


	if (cacheEntry->dataFD < 0)
	{
		abort();
	}

	if (rowNum == lastRow + 1 && lastLen == cacheEntry->rowLen)
	{
		seekPos = lastPos + cacheEntry->rowLen + HEADER_SIZE;
	}
	else
	{
		seekPos = (rowNum * (cacheEntry->rowLen + HEADER_SIZE)) + 
			SBLK_SIZE;
	}
	lastPos = seekPos;
	lastRow = rowNum;
	lastLen = cacheEntry->rowLen;
	if ((seekPos >= cacheEntry->size) || (!cacheEntry->dataMap))
	{
		msqlDebug2(MOD_ACCESS,
			"tableReadRow() : %u of %s - No Such Row \n",
			rowNum, (cacheEntry->result)?cacheEntry->resInfo:
			cacheEntry->table);
		snprintf(errMsg, MAX_ERR_MSG,
			"tableReadRow() : %u of %s - No Such Row \n",
			rowNum, (cacheEntry->result)?cacheEntry->resInfo:
			cacheEntry->table);
		return(-1);
	}
	row->buf = ((u_char *)cacheEntry->dataMap) + seekPos;
	row->header = (hdr_t *)row->buf;
	row->data = row->buf + HEADER_SIZE;
	row->rowID = rowNum;
	msqlDebug3(MOD_ACCESS,"tableReadRow() : %u of %s - %s\n",
		rowNum, (cacheEntry->result)?cacheEntry->resInfo:
		cacheEntry->table,(row->header->active)?"Active":"Inactive");
	return(1);
}




/****************************************************************************
** 	_deleteRow
**
**	Purpose	: Invalidate a row in the table
**	Args	: datafile FD, rowlength, desired row location
**	Returns	: -1 on error
**	Notes	: This just sets the row header byte to 0 indicating
**		  that it's no longer in use 
*/

int tableDeleteRow(cacheEntry,rowNum)
	cache_t	*cacheEntry;
	u_int	rowNum;
{
	int	rowLen;
	hdr_t	*hdrPtr;

	debugTrace(TRACE_IN,"deleteRow()");
	msqlDebug2(MOD_ACCESS,"deleteRow() : row %u of %s\n",
		rowNum, (cacheEntry->result)?cacheEntry->resInfo:
		cacheEntry->table);
	rowLen = cacheEntry->rowLen;
	cacheEntry->dirty = 1;

	hdrPtr = (hdr_t *)((char *)cacheEntry->dataMap + 
		(rowNum * (rowLen + HEADER_SIZE) + SBLK_SIZE));
	hdrPtr->active = 0;
	debugTrace(TRACE_OUT,"deleteRow()");
	return(0);
}






/****************************************************************************
** 	_fillRow
**
**	Purpose	: Create a new row-buf using the info given
**	Args	: 
**	Returns	: 
**	Notes	: 
*/


int tableFillRow(entry,row,fields,flist)
	cache_t		*entry;
	row_t		*row;
	mField_t	*fields;
	int		flist[];
{
	int	*offset,
		length;
	u_char	*data,
		*cp;
	u_int	overflow;
	mField_t *curField;

	debugTrace(TRACE_IN,"fillRow()");
	data = row->data;
	curField = fields;
	offset = flist;
	while(curField)
	{
		if (curField->value->nullVal)
		{
			cp = data + *offset;
			bzero(cp, curField->length+1);
		}
		else
		{
			cp = data + *offset;
			*cp = '\001';
			cp++;
			switch(typeBaseType(curField->type))
			{
				case INT8_TYPE:
				case UINT8_TYPE:
					bcopy(&(curField->value->val.int8Val),
						cp, 1);
					break;

				case INT16_TYPE:
				case UINT16_TYPE:
					bcopy(&(curField->value->val.int16Val),
						cp, 2);
					break;

				case INT32_TYPE:
				case UINT32_TYPE:
					bcopy4(&(curField->value->val.int32Val),
						cp);
					break;

				case INT64_TYPE:
				case UINT64_TYPE:
					bcopy(&(curField->value->val.int64Val),
						cp, 8);
					break;

				case CHAR_TYPE:
					/* XXXXXX */
					length = curField->value->dataLen;
					curField->value->val.charVal[length]=0;
					length=strlen((char *)
						curField->value->val.charVal);
					bcopy(curField->value->val.charVal,cp,
						length);
					break;

				case TEXT_TYPE:
					/* XXXXXX */
					length = curField->value->dataLen;
					curField->value->val.charVal[length]=0;
					length=strlen((char *)
						curField->value->val.charVal);
					if (curField->overflow != NO_POS)
					{
						overflow = curField->overflow;
					}
					else
					{
						overflow = NO_POS;
						if (length > curField->length)
					    	  overflow = varcharWrite(entry,
						  curField->value->val.charVal,
						  curField->length);
					}
					bcopy(&length,cp,sizeof(int));
					cp += sizeof(int);
					bcopy(&overflow,cp,sizeof(u_int));
					cp += sizeof(u_int);
					bcopy(curField->value->val.charVal,
						cp,
						(length > curField->length) ? curField->length : length
						);
					break;

				case REAL_TYPE:
					*cp = curField->value->precision;
					bcopy8(&(curField->value->val.realVal),
						cp+1);
					break;

				case BYTE_TYPE:
					length = typeFieldSize(curField->type);
					bcopy(curField->value->val.byteVal,cp,
						length);
					break;

			}
		}
		offset++;
		curField = curField->next;
	}
	debugTrace(TRACE_OUT,"fillRow()");
	return(0);
}






/****************************************************************************
** 	_updateValues
**
**	Purpose	: Modify a row-buf to reflect the contents of the field list
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

int tableUpdateValues(entry,row,fields,flist)
	cache_t		*entry;
	row_t		*row;
	mField_t	*fields;
	int		flist[];
{
	int		*offset,
			length;
	mField_t	*curField;
	u_char		*data,
			*cp;
	u_int		pos;
	char		nullField;

	debugTrace(TRACE_IN,"updateValues()");
	data = row->data;
	curField = fields;
	offset = flist;
	while(curField)
	{
		cp = data + *offset;
		nullField = *cp;
		if (!curField->value->nullVal)
                {
                        *cp = '\001';
                        cp++;
			switch(typeBaseType(curField->type))
			{
				case INT8_TYPE:
				case UINT8_TYPE:
					bcopy(&(curField->value->val.int8Val),
						cp, 1);
					break;

				case INT16_TYPE:
				case UINT16_TYPE:
					bcopy(&(curField->value->val.int16Val),
						cp, 2);
					break;

				case INT32_TYPE:
				case UINT32_TYPE:
					bcopy4(&(curField->value->val.int32Val),
						cp);
					break;
	
				case INT64_TYPE:
				case UINT64_TYPE:
					bcopy(&(curField->value->val.int64Val),
						cp, sizeof(HUGE_T));
					break;
		
				case CHAR_TYPE:
					length = strlen((char *) 
						curField->value->val.charVal);
					strncpy((char*)cp, (char*)
						curField->value->val.charVal,
						length);
					if (length < curField->length)
						*(cp + length) = 0;
					break;

				case TEXT_TYPE:
					bcopy(cp + sizeof(int), &pos,
						sizeof(u_int));
					if (nullField == 1)
						varcharDelete(entry, pos);

					length=strlen((char *)
						curField->value->val.charVal);
					pos = NO_POS;
					if (length > curField->length)
					    pos = varcharWrite(entry,
						curField->value->val.charVal,
						curField->length);
					bcopy(&length,cp,sizeof(int));
					cp += sizeof(int);
					bcopy(&pos,cp,sizeof(u_int));
					cp += sizeof(u_int);
					bcopy(curField->value->val.charVal,
						cp,
						(length > curField->length) ? 
						curField->length : length
						);
					break;

				case BYTE_TYPE:
					bcopy(curField->value->val.byteVal,cp,
						typeFieldSize(curField->type));
					break;

				case REAL_TYPE:
					*cp = curField->value->precision;
					bcopy8(&(curField->value->val.realVal),
						cp+1);
					break;
			}
		}
		else
		{
			*cp = '\000';
		}
		offset++;
		curField = curField->next;
	}
	debugTrace(TRACE_OUT,"updateValues()");
	return(0);
}





/****************************************************************************
** 	_tableExtractValues
**
**	Purpose	: Rip the required data from a row-buf
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

void tableExtractValues(entry,row,fields,flist,query)
	cache_t		*entry;
	row_t		*row;
	mField_t	*fields;
	int		flist[];
	mQuery_t	*query;
{
	mField_t	*curField;
	u_char		*cp = NULL,
			*data = NULL;
	int8_t		int8Val = 0;
	int16_t		int16Val = 0;
	int		int32Val = 0,
			*offset;
	double		*fp = 0;
	char		buf[8];
#ifdef HUGE_T
	HUGE_T		hp;
#endif

	debugTrace(TRACE_IN,"tableExtractValues()");
	if (row)
		data = row->data;
	curField = fields;
	offset = flist;
	while(curField)
	{
		if (curField->literalParamFlag)
		{
			curField = curField->next;
			continue;
		}
		if (curField->function && !entry->result)
		{
			tableExtractValues(entry, row,
				curField->function->paramHead,
				curField->function->flist,query);
			curField = curField->next;
			offset++;
			continue;
		}
		if (curField->value)
		{
			parseFreeValue(curField->value);
			curField->value = NULL;
		}
		if ( *(curField->name) == '_' && ! entry->result)
		{
			sysvarGetVariable(entry,row,curField,query);
			curField = curField->next;
			continue;
		}
		if ( * (data + *offset)) 
		{
			switch(typeBaseType(curField->type))
			{
				case INT8_TYPE:
				case UINT8_TYPE:
					bcopy(data + *offset + 1,&int8Val, 1);
					curField->value =(mVal_t *)
						parseFillValue((char *)&int8Val,
						curField->type, 1);
					break;

				case INT16_TYPE:
				case UINT16_TYPE:
					bcopy(data + *offset + 1,&int16Val, 2);
					curField->value =(mVal_t *)
						parseFillValue((char*)&int16Val,
						curField->type, 2);
					break;

					
				case INT32_TYPE:
				case UINT32_TYPE:
					bcopy4(data + *offset + 1,&int32Val);
					curField->value =(mVal_t *)
						parseFillValue((char*)&int32Val,
						curField->type, 4);
					break;

				case INT64_TYPE:
				case UINT64_TYPE:
					bcopy(data + *offset + 1,&hp, 8);
					curField->value =(mVal_t *)
						parseFillValue((char *)&hp,
						curField->type, 8);
					break;

				case CHAR_TYPE:
					cp = (u_char *)data + *offset + 1;
					curField->value = (mVal_t *)
						parseFillValue((char *)cp, 
						CHAR_TYPE, curField->length);
					break;

				case TEXT_TYPE:
					cp = (u_char *)data + *offset + 1;
					curField->value = memMallocValue();
					if (!query->targetTable)
					{
						bcopy(cp + sizeof(u_int), 
							&(curField->overflow),
							sizeof(u_int));
					}
					else
					{
						curField->overflow = NO_POS;
					}
					curField->value->type = TEXT_TYPE;
        				curField->value->nullVal = 0;
					curField->value->val.charVal = 
						varcharRead(curField->entry, cp,
						curField->length);
					curField->value->dataLen =
					  	strlen((char *)
						curField->value->val.charVal);
					break;

				case REAL_TYPE:
					bcopy8(data + *offset + 2,buf);
					fp = (double *)buf;
					curField->value =(mVal_t *)
						parseFillValue((char *)fp,
							REAL_TYPE, 8);
					curField->value->precision = *(
						data + *offset + 1);
					break;

				case BYTE_TYPE:
					cp = (u_char *)data + *offset + 1;
					curField->value = (mVal_t *)
						parseFillValue((char *)cp, 
						curField->type,
						curField->length);
					break;
			}
		} 
		else 
		{
			curField->value = parseCreateNullValue();
			curField->value->type = curField->type;
		}
		curField = curField->next;
		offset++;
	}
	debugTrace(TRACE_OUT,"tableExtractValues()");
}


int tableCopyFile(from, to)
	char	*from,
		*to;
{
	int	fd1,
		fd2,
		numBytes;
	char	buf[5 * 1024];

	fd1 = open(from, O_RDONLY);
	if (fd1 < 0)
	{
		snprintf(errMsg,MAX_ERR_MSG,"Copy of '%s' failed (%s)",from,
			(char*)strerror(errno));
		return(-1);
	}
	fd2 = open(to, O_CREAT | O_RDWR, 0700);
	if (fd2 < 0)
	{
		snprintf(errMsg,MAX_ERR_MSG,"Copy of '%s' failed (%s)",from,
			(char*)strerror(errno));
		return(-1);
	}
	numBytes = read(fd1,buf,sizeof(buf));
	while(numBytes >0 )
	{
		if (write(fd2,buf,numBytes) != numBytes)
		{
			snprintf(errMsg,MAX_ERR_MSG,"Copy of '%s' failed (%s)",
				from, (char*)strerror(errno));
			return(-1);
		}
		numBytes = read(fd1,buf,sizeof(buf));
	}
	close(fd1);
	close(fd2);
	return(0);
}


int tableCopyDirectory(from, to)
	char	*from,
		*to;
{
	char	fromFile[MSQL_PATH_LEN],
		toFile[MSQL_PATH_LEN];
	DIR	*dirp;
#ifdef HAVE_DIRENT_H
        struct  dirent *cur;
#else
        struct  direct *cur;
#endif


	/*
	** Check out the source
	*/

	dirp = opendir(from);
	if (!dirp)
	{
		snprintf(errMsg, MAX_ERR_MSG,
			"Error copying directory : %s doesn't exist!\n",
			from);
		return(-1);
	}

	/*
	** Create the destination
	*/
	if (mkdir(to,0700) < 0)
	{
		snprintf(errMsg,MAX_ERR_MSG, 
			"Error creating database (%s)\n",
			(char*)strerror(errno));
		closedir(dirp);
		return(-1);
	}

	/*
	** Copy the files - skip '.' and '..'
	*/
	cacheSyncCache();
	cur = readdir(dirp);
	while(cur)
	{
		if (*cur->d_name == '.')
		{
			cur = readdir(dirp);
			continue;
		}

		/*
		** Is this a file or a directory?
		*/
		snprintf(fromFile,MSQL_PATH_LEN,"%s/%s", from, cur->d_name);
		snprintf(toFile,MSQL_PATH_LEN,"%s/%s", to, cur->d_name);
		if (cur->d_type == DT_DIR)
		{
			if(tableCopyDirectory(fromFile, toFile) < 0)
			{
				closedir(dirp);
				return(-1);
			}
		}
		else
		{
			if (tableCopyFile(fromFile, toFile) < 0)
			{
				closedir(dirp);
				return(-1);
			}
		}
		cur = readdir(dirp);
	}
	closedir(dirp);
	return(0);
}


int tableCreateDefinition(server, table, query)
	msqld		*server;
	char		*table;
	mQuery_t	*query;
{
	int		fd,
			fieldCount,
			curOffset,
			version;
	char		path[255];
	struct stat	sbuf;
	mField_t	*curField;
	

        (void)snprintf(path, MSQL_PATH_LEN, "%s/%s/%s.def", 
		server->config.dbDir, query->curDB, table);
        if (stat(path, &sbuf) >= 0)
        {
                snprintf(errMsg,MAX_ERR_MSG,TABLE_EXISTS_ERROR,table);
                msqlDebug1(MOD_ERR,"Table \"%s\" exists\n",table);
                debugTrace(TRACE_OUT,"processCreateTable()");
                return(-1);
        }
        fd = open(path,O_WRONLY | O_CREAT , 0600);
        if (fd < 0)
        {
                snprintf(errMsg,MAX_ERR_MSG,TABLE_FAIL_ERROR,table);
                msqlDebug1(MOD_ERR,"Can't create table \"%s\"\n",table);
                debugTrace(TRACE_OUT,"processCreateTable()");
                return(-1);
        }

	/*
	** Set the database version 
	*/
	version = DB_VERSION;
	write(fd,&version, sizeof(int));
	
        /*
        ** Write out the definition and check to ensure that there
        ** aren't too many fields
        */
	curOffset = 0;
        curField = query->fieldHead;
        fieldCount = 0;
        while(curField)
        {
                (void)strcpy(curField->table,table);
                fieldCount++;
                curField->offset = curOffset;
                curOffset += curField->dataLength + 1;
                write(fd, curField, sizeof(mField_t));
                curField = curField->next;
        }
	close(fd);

        if (fieldCount > MAX_FIELDS)
        {
                unlink(path);
		snprintf(errMsg,MAX_ERR_MSG,TABLE_WIDTH_ERROR,MAX_FIELDS);
                msqlDebug1(MOD_ERR,"Too many fields in table (%d Max)\n",
                        MAX_FIELDS);
                debugTrace(TRACE_OUT,"processCreateTable()");
                return(-1);
        }
	return(0);
}


int tableCheckTargetDefinition(query)
	mQuery_t	*query;
{
	mField_t	*curField,
			*tmpField;
	int		curOffset;


	/*
	** Ensure there are no duplicate fields and that the offset
	** is set correctly;
	*/
	curOffset = 0;
	curField = query->fieldHead;
	while(curField)
	{
		if (curField->function)
		{
			strcpy(errMsg,
			   "Function results are not valid for INTO clauses");
			return(-1);
		}
		curField->offset = curOffset;
		curOffset += curField->dataLength + 1;
		tmpField = query->fieldHead;
		while(tmpField)
		{
			if (tmpField == curField)
			{
				tmpField = tmpField->next;
				continue;
			}
			if (*curField->name == *tmpField->name &&
				strcmp(curField->name,tmpField->name) == 0)
			{
				snprintf(errMsg,MAX_ERR_MSG,
				"Duplicate field name in INTO clause '%s'",
				curField->name);
				return(-1);
			}
			tmpField = tmpField->next;
		}
		curField = curField->next;
	}
	return(0);
}


int tableDirHash(table)
	char	*table;
{
	char	*cp;
	int	hash;

	cp = table;
	hash = 0;
	while(*cp)
	{
		hash += (int)*cp;
		cp++;
	}
	return(hash % TABLE_DIR_HASH);
}



int tableCheckFileLayout(server, table, db)
        msqld           *server;
	char		*table,
			*db;
{
	char	path[MSQL_PATH_LEN + 1];
	struct	stat sbuf;


	/*
	** If the data file exists in the old file layout then use
	** flat layout.  Otherwise we always use hash layout
	*/
        (void)snprintf(path, MSQL_PATH_LEN, "%s/%s/%s.dat", 
		server->config.dbDir, db, table);
        if (stat(path, &sbuf) == 0)
	{
		return(LAYOUT_FLAT);
	}
	return(LAYOUT_HASH);
};
