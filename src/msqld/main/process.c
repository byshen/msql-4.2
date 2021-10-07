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
** $Id: process.c,v 1.28 2012/01/17 02:26:22 bambi Exp $
**
*/

/*
** Module	: main : process
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
#  include <dirent.h>
#else
#  include <direct.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/config/config.h>

#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/includes/errmsg.h>
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/table.h>
#include <msqld/main/index.h>
#include <msqld/main/net.h>
#include <msqld/main/version.h>
#include <msqld/main/util.h>
#include <msqld/main/cache.h>
#include <msqld/main/varchar.h>
#include <msqld/main/acl.h>
#include <msqld/main/compare.h>
#include <msqld/main/select.h>
#include <msqld/main/parse.h>
#include <msqld/main/process.h>
#include <msqld/main/sort.h>
#include <common/types/types.h>
#include <msqld/cra/cra.h>
#include <msqld/lock/lock.h>
#include <libmsql/msql.h>
#include <msqld/broker/broker.h>

/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

/* HACK */
extern char errMsg[];
extern char *packet;


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

#define _checkWriteAccess(server)					\
	if (!aclCheckPerms(WRITE_ACCESS) || server->config.readOnly)	\
	{								\
		netError(query->clientSock,"Access Denied\n");		\
		debugTrace(TRACE_OUT,"processQuery()");			\
		return;							\
	}



static int _populateIndexFile(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	cache_t		*entry;
	mIndex_t 	*idx;
	u_int		rowNum;
	row_t		row;
	int		fullFlist[MAX_FIELDS],
			res;

	entry = tableLoadDefinition(server, query->indexDef.table, NULL,
		query->curDB);
	if (!entry)
	{
		return(-1);
	}
	idx = entry->indices;
	while(idx)
	{
		if ( *(idx->name) != *(query->indexDef.name))
		{
			idx = idx->next;
			continue;
		}
		if (strcmp(idx->name, query->indexDef.name) != 0)
		{
			idx = idx->next;
			continue;
		}
		break;
	}
	if (idx == NULL)
	{
               	snprintf(errMsg,MAX_ERR_MSG,"Can't find index def for '%s'\n",
			query->indexDef.name);
               	msqlDebug1(MOD_ERR, "Can't find index def for '%s'\n",
			query->indexDef.name);
               	debugTrace(TRACE_OUT,"_populateIndexFile()");
                abort();
	}
	utilSetupFields(entry,fullFlist, entry->def);
	rowNum = 0;
	while(rowNum < entry->sblk->numRows)
	{
		if (tableReadRow(entry,&row,rowNum) < 0)
		{
			return(-1);
		}
		if (row.header->active == 0)
		{
			rowNum++;
			continue;
		}
		tableExtractValues(entry,&row,entry->def,fullFlist,query);
		if (indexCheckNullFields(entry,&row,idx,fullFlist) < 0)
        	{
			processDropIndex(server, query,1);
                	return(-1);
        	}
        	res = indexCheckIndex(entry, entry->def, NULL, idx, NO_POS);
        	if (res < 0)
        	{
			processDropIndex(server, query,1);
                	debugTrace(TRACE_OUT,"_populateIndexFile()");
                	return(-1);
        	}
        	if (res == 0)
        	{
			processDropIndex(server, query,1);
                	snprintf(errMsg,MAX_ERR_MSG,KEY_UNIQ_ERROR);
                	msqlDebug0(MOD_ERR,
				"Non unique value for unique index\n");
                	debugTrace(TRACE_OUT,"_populateIndexFile()");
                	return(-1);
        	}
		indexInsertIndexValue(entry, entry->def, rowNum, idx);
		rowNum++;
	}
        debugTrace(TRACE_OUT,"_populateIndexFile()");
	return(0);
}


static int _checkNullFields(cacheEntry,row, flist)
	cache_t	*cacheEntry;
	row_t	*row;
	int	*flist;
{
	mField_t	*curField;
	int	*curfl;
	u_char	*data;

	debugTrace(TRACE_IN,"checkNullFields()");
	data = row->data;
	curfl = flist;
	curField = cacheEntry->def;
	while(curField)
	{
		if (!*(data + *curfl) && (curField->flags & NOT_NULL_FLAG))
		{
			snprintf(errMsg,MAX_ERR_MSG,BAD_NULL_ERROR, 
				curField->name);
			msqlDebug1(MOD_ERR,"Field \"%s\" cannot be null\n",
				curField->name);
			debugTrace(TRACE_OUT,"checkNullFields()");
			return(-1);
		}
		curfl++;
		curField = curField->next;
	}
	debugTrace(TRACE_OUT,"checkNullFields()");
	return(0);
}


static void _freeFieldList(head)
        mField_t *head;
{
        mField_t *cur,
                *prev;

        cur = head;
        while(cur)
        {
                prev = cur;
                cur = cur->next;
		if (prev->value)
		{
			parseFreeValue(prev->value);
			prev->value = NULL;
		}
		prev->next = NULL;
		free(prev);
        }
}



static int _deleteDirectory(path)
	char		*path;
{
	char		tmpPath[MSQL_PATH_LEN + 1];
	DIR		*dirp;
	struct stat	sbuf;
#ifdef HAVE_DIRENT_H
	struct	dirent *cur;
#else
	struct	direct *cur;
#endif


	dirp = opendir(path);
	if (!dirp)
	{
		return(-1);
	}

	/*
	** Blow away any files but dodge '.' and '..'.  IF we find a directory
	** then we just call ourselves with the path
	*/

	cur = readdir(dirp);
	while(cur)
	{
		if (*cur->d_name == '.')
		{
			cur = readdir(dirp);
			continue;
		}
		snprintf(tmpPath, MSQL_PATH_LEN, "%s/%s", path, cur->d_name);
		stat(tmpPath, &sbuf);
		if(sbuf.st_mode & S_IFDIR)
		{
			if(_deleteDirectory(tmpPath) < 0)
			{
				closedir(dirp);
				return(-1);
			}
		}
		else
		{
			if(unlink(tmpPath) < 0)
			{
				closedir(dirp);
				return(-1);
			}
		}
		cur = readdir(dirp);
	}
	if (rmdir(path) < 0)
	{
		closedir(dirp);
		return(-1);
	}
	closedir(dirp);
	return(0);
}



/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


void processListDBs(server, sock)
	msqld	*server;
	int	sock;
{
	DIR		*dirp;
#ifdef HAVE_DIRENT_H
	struct	dirent *cur;
#else
	struct	direct *cur;
#endif

	debugTrace(TRACE_IN,"processListDBs()");
	dirp = opendir(server->config.dbDir);
	if (!dirp)
	{
		snprintf(errMsg,MAX_ERR_MSG,BAD_DIR_ERROR,server->config.dbDir, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Can't open directory \"%s\"\n", 
			server->config.dbDir);
		debugTrace(TRACE_OUT,"processListDBs()");
		return;
	}
	
	/*
	** Grab the names dodging any . files
	*/

	cur = readdir(dirp);
	while(cur)
	{
		if (*cur->d_name == '.')
		{
			cur = readdir(dirp);
			continue;
		}
		snprintf(packet,PKT_LEN, "%d:%s\n",(int)strlen(cur->d_name),
			cur->d_name);
		netWritePacket(sock);
		cur = readdir(dirp);
	}
	netEndOfList(sock);
	closedir(dirp);
	debugTrace(TRACE_OUT,"processListDBs()");
	return;
}







void processListTables(server, sock,db)
	msqld	*server;
	int	sock;
	char	*db;
{
	char	path[MSQL_PATH_LEN],
		*cp;
	DIR	*dirp;
#ifdef HAVE_DIRENT_H
	struct	dirent *cur;
#else
	struct	direct *cur;
#endif

	debugTrace(TRACE_IN,"msqlListTables()");
	(void)snprintf(path,MSQL_PATH_LEN,"%s/%s",server->config.dbDir,db);
	dirp = opendir(path);
	if (!dirp)
	{
		snprintf(errMsg,MAX_ERR_MSG,BAD_DIR_ERROR,path, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Can't open directory \"%s\"\n",path);
		debugTrace(TRACE_OUT,"msqlListTables()");
		return;
	}
	
	/*
	** Grab the names
	*/

	cur = readdir(dirp);
	while(cur)
	{
		if (*cur->d_name == '.')
		{
			cur = readdir(dirp);
			continue;
		}
		cp = (char *)rindex(cur->d_name,'.');
		if (!cp)
		{
			cur = readdir(dirp);
			continue;
		}
	
		if (strcmp(cp,".def") == 0)
		{
			*cp = 0;
			snprintf(packet,PKT_LEN, "%d:%s\n",
				(int)strlen(cur->d_name),cur->d_name);
			netWritePacket(sock);
		}
		cur = readdir(dirp);
	}
	netEndOfList(sock);
	closedir(dirp);
	debugTrace(TRACE_OUT,"msqlListTables()");
	return;
}



void processSequenceInfo(server, sock, table, db)
	msqld	*server;
	int	sock;
	char	*table,
		*db;
{
	cache_t	*cacheEntry;

	debugTrace(TRACE_IN,"processSequenceInfo()");
	if((cacheEntry = tableLoadDefinition(server, table,NULL,db)))
	{
		if (cacheEntry->sblk->sequence.step)
		{
			snprintf(packet,PKT_LEN,"1:%d:%d",
				cacheEntry->sblk->sequence.step,
				cacheEntry->sblk->sequence.value);
			netWritePacket(sock);
		}
		else
		{
			netError(sock, "No sequence defined on '%s'\n", table);
		}
	}
	else
	{
		netError(sock, "%s\n",errMsg);
	}
	debugTrace(TRACE_OUT,"processSequenceInfo()");
}



void processListFields(server, sock,table,db)
	msqld	*server;
	int	sock;
	char	*table,
		*db;
{
 	mField_t	*curField;
	char		buf[50],
			buf2[50];
	cache_t		*cacheEntry;
	mIndex_t	*curIndex;

	debugTrace(TRACE_IN,"msqlListFields()");
	msqlDebug1(MOD_GENERAL,"Table to list = %s\n",table);
	*errMsg = 0;
	if((cacheEntry = tableLoadDefinition(server,table,NULL,db)))
	{
      		curField = cacheEntry->def;
		while(curField)
		{
			snprintf(buf,sizeof(buf),"%d",curField->length);
			snprintf(buf2,sizeof(buf2),"%d",curField->type);
			snprintf(packet,PKT_LEN,"%d:%s%d:%s%d:%s%d:%s1:%s1:%s", 
				(int)strlen(table), table,
				(int)strlen(curField->name), curField->name, 
				(int)strlen(buf2), buf2,
				(int)strlen(buf), buf, 
				curField->flags & NOT_NULL_FLAG?"Y":"N", "N");
			netWritePacket(sock);
			curField = curField->next;
		}

      		curIndex = cacheEntry->indices;
		while(curIndex)
		{
			snprintf(buf2,sizeof(buf2),"%d",IDX_TYPE);
			snprintf(packet,PKT_LEN,"%d:%s%d:%s%d:%s%d:%s1:%s1:%s", 
				(int)strlen(table), table,
				(int)strlen(curIndex->name), curIndex->name, 
				(int)strlen(buf2), buf2,
				1, "0", "N",
				curIndex->unique?"Y":"N");
			netWritePacket(sock);
			curIndex = curIndex->next;
		}
		netEndOfList(sock);
	}
	else
	{
		if (*errMsg == 0)
			netError(sock, "Unknown table '%s'\n", table);
		else
			netError(sock, errMsg);
	}
	debugTrace(TRACE_OUT,"msqlListFields()");
}



void processTableInfo(server, sock,table,db)
	msqld	*server;
	int	sock;
	char	*table,
		*db;
{
	cache_t	*cacheEntry;

	debugTrace(TRACE_IN,"processTableInfo()");
	if((cacheEntry = tableLoadDefinition(server, table,NULL,db)))
	{
		snprintf(packet,PKT_LEN,"1:%lu:%lu:%lu:%lu:%lu",
			(unsigned long)cacheEntry->rowLen,
			(unsigned long)(cacheEntry->sblk->activeRows *
				cacheEntry->rowLen) ,
			(unsigned long)cacheEntry->size,
			(unsigned long)cacheEntry->sblk->activeRows,
			(unsigned long)cacheEntry->sblk->numRows);
		netWritePacket(sock);
	}
	else
	{
		netError(sock, "%s\n",errMsg);
	}
	debugTrace(TRACE_OUT,"processTableInfo()");
}



void processListIndex(server, sock,index,table,db)
	msqld	*server;
	int	sock;
	char	*index,
		*table,
		*db;
{
 	mIndex_t	*curIndex;
	cache_t	*cacheEntry;
	char	*idxType,
		idxCount[20],
		idxKeys[20];
	mField_t	*curField;
	int	count,
		offset;

	debugTrace(TRACE_IN,"msqlListIndex()");
	msqlDebug1(MOD_GENERAL,"Index to list = %s\n",index);
	if((cacheEntry = tableLoadDefinition(server,table,NULL,db)))
	{
      		curIndex = cacheEntry->indices;
		while(curIndex)
		{
			if (*(curIndex->name) != *index)
			{
				curIndex = curIndex->next;
				continue;
			}
			if (strcmp(curIndex->name,index) != 0)
			{
				curIndex = curIndex->next;
				continue;
			}
			break;
		}
		if (curIndex)
		{
			idxType = idxGetIndexType(&curIndex->handle);
			snprintf(idxCount,sizeof(idxCount), "%d",
				idxGetNumEntries(&curIndex->handle));
			snprintf(idxKeys,sizeof(idxKeys), "%d",
				idxGetNumKeys(&curIndex->handle));
			snprintf(packet,PKT_LEN,"%d:%s\n",
				(int)strlen(idxType), idxType);
			netWritePacket(sock);
			snprintf(packet,PKT_LEN,"%d:%s\n",
				(int)strlen(idxCount), idxCount);
			netWritePacket(sock);
			snprintf(packet,PKT_LEN,"%d:%s\n",
				(int)strlen(idxKeys), idxKeys);
			netWritePacket(sock);
			if (curIndex)
			{
				count = 0;
				while(count < curIndex->fieldCount)
				{
					offset = curIndex->fields[count];
					curField = cacheEntry->def;
					while(offset)
					{
						curField = curField->next;
						offset--;
					}
					snprintf(packet,PKT_LEN,"%d:%s\n",
						(int)strlen(curField->name),
						curField->name);
					count ++;
					netWritePacket(sock);
				}
			}
		}
	}
	netEndOfList(sock);
	debugTrace(TRACE_OUT,"msqlListFields()");
}


int processCreateTable(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	char		*table;
	char		datPath[255],
			oflPath[255];
	int		fd,
			hash;
	sblk_t 		sblock;
	u_int		freeList;
	struct stat	sbuf;


	debugTrace(TRACE_IN,"processCreateTable()");

	table = query->tableHead->name;
	hash = tableDirHash(table);

	/*
	** Ensure the hash dir exists
	*/

	snprintf(datPath,MSQL_PATH_LEN, "%s/%s/%02d",
		server->config.dbDir, query->curDB, hash);
	if (stat(datPath, &sbuf) < 0)
	{
		if (mkdir(datPath, 0700) < 0)
		{
			snprintf(errMsg, MAX_ERR_MSG,DATA_FILE_ERROR,table, 
				(char*)strerror(errno));
			msqlDebug1(MOD_ERR,"Error creating table file for %s\n",
				table);
			debugTrace(TRACE_OUT,"processCreateTable()");
			return(-1);
		}
	}

	/*
	** Write the catalog entry
	*/
	if (tableCreateDefinition(server, table,query) < 0)
	{
		debugTrace(TRACE_OUT,"processCreateTable()");
		return(-1);
	}
		
	/*
	** Create an empty table.  Use the new layout for any new tables
	*/
	
	snprintf(datPath,MSQL_PATH_LEN, "%s/%s/%02d/%s.dat", 
		server->config.dbDir, query->curDB, hash,table);
	fd = open(datPath,O_CREAT | O_WRONLY , 0600);
	if (fd < 0)
	{
		unlink(datPath);
		snprintf(errMsg, MAX_ERR_MSG,DATA_FILE_ERROR,table, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Error creating table file for \"%s\"\n",
			table);
		debugTrace(TRACE_OUT,"processCreateTable()");
		return(-1);
	}
	bzero(&sblock, SBLK_SIZE);
	sblock.version = DB_VERSION;
	sblock.numRows = sblock.activeRows = 0;
	sblock.freeList = NO_POS;
	sblock.sequence.step = sblock.sequence.value = 0;
	if (write(fd,&sblock,SBLK_SIZE) < SBLK_SIZE)
	{
		close(fd);
		unlink(datPath);
		snprintf(errMsg, MAX_ERR_MSG, DATA_FILE_ERROR,table, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Error creating table file for \"%s\"\n",
			table);
		debugTrace(TRACE_OUT,"processCreateTable()");
		return(-1);
	}
	close(fd);


	/*
	** NOTE : The index def file file doesn't need to
	** 	  be created here as it is a 0 length file.  It
	**	  is now created using the open when the table is
	**	  first loaded into the table cache
	** 
	** Create an empty overflow file
	*/
	snprintf(oflPath,MSQL_PATH_LEN,"%s/%s/%02d/%s.ofl", 
		server->config.dbDir, query->curDB, hash, table);
	fd = open(oflPath,O_CREAT | O_WRONLY , 0600);
	if (fd < 0)
	{
		unlink(datPath);
		snprintf(errMsg,MAX_ERR_MSG,DATA_FILE_ERROR,table, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Error creating table file for \"%s\"\n",
			table);
		debugTrace(TRACE_OUT,"processCreateTable()");
		return(-1);
	}
	freeList = NO_POS;
	if (write(fd,&freeList,sizeof(u_int)) < sizeof(u_int))
	{
		close(fd);
		unlink(datPath);
		snprintf(errMsg,MAX_ERR_MSG,DATA_FILE_ERROR,table, 
			(char*)strerror(errno));
		msqlDebug1(MOD_ERR,"Error creating table file for \"%s\"\n",
			table);
		debugTrace(TRACE_OUT,"processCreateTable()");
		return(-1);
	}
	close(fd);

	netOK(query->clientSock);
	debugTrace(TRACE_OUT,"processCreateTable()");
	return(0);
}



int processCreateIndex(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	char		defPath[255],
			idxPath[255],
			*indexName,
			*tableName,
			*dbName;
	mField_t	*curField,
			*indexField;
	int		fd,
			fieldLoc,
			fieldCount,
			length,
			existingData,
			numBytes;
	cache_t		*entry;
	mIndex_t	tmp;

	debugTrace(TRACE_IN,"processCreateIndex()");

	/*
	** Ensure the index name doesn't clash with a field
	*/
	tableName = query->indexDef.table;
	indexName = query->indexDef.name;
	dbName = query->curDB;
	if ((entry = tableLoadDefinition(server,tableName,NULL,dbName)) == NULL)
	{
		debugTrace(TRACE_OUT,"processCreateIndex()");
		return(-1);
	}

	existingData = (entry->sblk->activeRows > 0)?1:0;
	length = 0;
	curField = entry->def;
	while(curField)
	{
		if ( *(curField->name) != *(indexName))
		{
			curField = curField->next;
			continue;
		}
		if (strcmp(curField->name,indexName) != 0)
		{
			curField = curField->next;
			continue;
		}

		snprintf(errMsg,MAX_ERR_MSG, "Bad index name '%s'", indexName);
		debugTrace(TRACE_OUT,"processCreateIndex()");
		return(-1);
	}

	/*
	** Can't clash with another index either
	*/
	if (entry->fileLayout == LAYOUT_FLAT)
	{
		snprintf(defPath,MSQL_PATH_LEN,"%s/%s/%s.idx",
			server->config.dbDir, dbName, tableName);
		snprintf(idxPath,MSQL_PATH_LEN,"%s/%s/%s.idx-%s",
			server->config.dbDir, dbName, tableName, indexName);
	}
	else
	{
		snprintf(defPath,MSQL_PATH_LEN,"%s/%s/%02d/%s.idx",
			server->config.dbDir, dbName, entry->dirHash,tableName);
		snprintf(idxPath,MSQL_PATH_LEN,"%s/%s/%02d/%s.idx-%s",
			server->config.dbDir, dbName, entry->dirHash,
			tableName, indexName);
	}

	fd = open(defPath,O_RDWR | O_CREAT , 0600);
	if (fd < 0)
	{
		(void)close(fd);
		strcpy(errMsg,"Can't open index definition file");
		debugTrace(TRACE_OUT,"processCreateIndex()");
		return(-1);
	}

	while(read(fd, &tmp, sizeof(tmp)) == sizeof(tmp))
	{
		if ( *(tmp.name) != *(indexName))
			continue;
		if (strcmp(tmp.name, indexName) != 0)
			continue;
		
		snprintf(errMsg,MAX_ERR_MSG, "Bad index name '%s'", indexName);
		debugTrace(TRACE_OUT,"processCreateIndex()");
		close(fd);
		return(-1);
	}
	
	/*
	** OK, setup the struct and add it to the index def file
	*/
	indexField = query->fieldHead;
	fieldCount = 0;
	while(indexField)
	{
		fieldLoc = 0;
		curField = entry->def;
		while(curField)
		{
			if ( *(curField->name) != *(indexField->name))
			{
				curField = curField->next;
				fieldLoc++;
				continue;
			}
			if (strcmp(curField->name, indexField->name)!=0)
			{
				curField = curField->next;
				fieldLoc++;
				continue;
			}
			break;
		}
		if (!curField)
		{
			snprintf(errMsg,MAX_ERR_MSG,"Unknown field '%s'",
				indexField->name);
			debugTrace(TRACE_OUT,"msqlCreateIndex()");
			close(fd);
			return(-1);
		}
		if (curField->type == TEXT_TYPE)
		{
			strcpy(errMsg,"Can't index on a TEXT field!");
			debugTrace(TRACE_OUT,"msqlCreateIndex()");
			close(fd);
			return(-1);
		}
		query->indexDef.fields[fieldCount] = fieldLoc;
		fieldCount++;
		length += curField->length;
		if(fieldCount > MAX_INDEX_WIDTH - 1)
		{
			strcpy(errMsg,"Too many fields in index");
			debugTrace(TRACE_OUT,"msqlCreateIndex()");
			close(fd);
			return(-1);
		}
		indexField = indexField->next;
	}

	if (fieldCount == 1)
	{
		switch(typeBaseType(curField->type))
		{
			case CHAR_TYPE:
				query->indexDef.keyType = IDX_CHAR;
				break;

			case INT8_TYPE:
				query->indexDef.keyType = IDX_INT8;
				break;
			case INT16_TYPE:
				query->indexDef.keyType = IDX_INT16;
				break;
			case INT32_TYPE:
				query->indexDef.keyType = IDX_INT32;
				break;
			case INT64_TYPE:
				query->indexDef.keyType = IDX_INT64;
				break;

			case UINT8_TYPE:
				query->indexDef.keyType = IDX_UINT8;
				break;
			case UINT16_TYPE:
				query->indexDef.keyType = IDX_UINT16;
				break;
			case UINT32_TYPE:
				query->indexDef.keyType = IDX_UINT32;
				break;
			case UINT64_TYPE:
				query->indexDef.keyType = IDX_UINT64;
				break;

			case REAL_TYPE:
				query->indexDef.keyType = IDX_REAL;
				break;

			default:
				query->indexDef.keyType = IDX_BYTE;
				break;
		}
	}
	else
	{
		query->indexDef.keyType = IDX_BYTE;
	}

	query->indexDef.fields[fieldCount] = -1;
	query->indexDef.length = length;
	query->indexDef.fieldCount = fieldCount;
	numBytes = write(fd, &(query->indexDef), sizeof(mIndex_t));
	close(fd);
	if (numBytes < 0)
	{
		return(-1);
	}

	idxCreate(idxPath, query->indexDef.idxType, 0600, length, 
		query->indexDef.keyType, IDX_DUP, &query->indexDef.environ);


        /*
        ** Invalidate the cache entry so that we reload it
        */

	cacheInvalidateEntry(entry);

	/*
	** If there's data in the table then prime the index
	*/
	if (existingData)
	{
		if(_populateIndexFile(server, query) < 0)
		{
			debugTrace(TRACE_OUT,"msqlCreateIndex()");
			return(-1);
		}
	}
	if (server->config.hasBroker)
		brokerChildSendFlush(query->curDB,query->indexDef.table);
	netOK(query->clientSock);
	debugTrace(TRACE_OUT,"msqlCreateIndex()");
	return(0);
}




int processDropSequence(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	cache_t		*entry;

	debugTrace(TRACE_IN,"processDropSequence()");

	/*
	** See if there is a sequence on this table
	*/
	if ((entry = tableLoadDefinition(server,query->sequenceDef.table, NULL,
		query->curDB)) == NULL)
	{
		debugTrace(TRACE_OUT,"processDropSequence()");
		return(-1);
	}
	if (entry->sblk->sequence.step == 0)
	{
		snprintf(errMsg,MAX_ERR_MSG,
			"Table '%s' does not have a sequence",
			query->sequenceDef.table);
		debugTrace(TRACE_OUT,"processDropSequence()");
		return(-1);
	}

	entry->sblk->sequence.step = 0;
	entry->sblk->sequence.value = 0;
	if (server->config.hasBroker)
		brokerChildSendFlush(query->curDB, query->sequenceDef.table);
	netOK(query->clientSock);
	debugTrace(TRACE_OUT,"processDropSequence()");
	return(0);
}



int processCreateSequence(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	cache_t		*entry;

	debugTrace(TRACE_IN,"processCreateSequence()");

	/*
	** See if there is a sequence on this table already
	*/
	if ((entry = tableLoadDefinition(server,query->sequenceDef.table,NULL,
		query->curDB)) == NULL)
	{
		debugTrace(TRACE_OUT,"processCreateSequence()");
		return(-1);
	}
	if (entry->sblk->sequence.step != 0)
	{
		snprintf(errMsg,MAX_ERR_MSG,
			"Table '%s' already has a sequence",
			query->sequenceDef.table);
		debugTrace(TRACE_OUT,"processCreateSequence()");
		return(-1);
	}

	entry->sblk->sequence.step = query->sequenceDef.step;
	entry->sblk->sequence.value = query->sequenceDef.value;
	if (server->config.hasBroker)
		brokerChildSendFlush(query->curDB,query->sequenceDef.table);
	netOK(query->clientSock);
	debugTrace(TRACE_OUT,"processCreateSequence()");
	return(0);
}




int processDropTable(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	extern	cache_t	*tableCache;

	char		path[MSQL_PATH_LEN],
			defPath[MSQL_PATH_LEN],
			*cp;
	FILE		*fp;
	int		fd;
	mIndex_t	tmp;
	cache_t 	*entry;
	int		count,
			dirHash,
			fileLayout;

	debugTrace(TRACE_IN,"processDropTable()");

	/* 
	** Invalidate the cache entry so that we don't use it again 
	*/

	fileLayout = dirHash = 0;
	entry = tableLoadDefinition(server,query->tableHead->name,NULL,
		query->curDB);
	count = 0;
	while(count < server->config.tableCache)
	{
		entry = tableCache + count;
		if (*entry->db == *query->curDB)
		{
			if((strcmp(entry->db,query->curDB)==0) &&
		   	(strcmp(entry->table,query->tableHead->name)==0 ||
		    	strcmp(entry->cname,query->tableHead->name)==0))
			{
				fileLayout = entry->fileLayout;
				dirHash = entry->dirHash;
				cacheInvalidateEntry(entry);
			}
		}
		count++;
	}

	/*
	** Now blow away the table files
	*/
	snprintf(path,MSQL_PATH_LEN,"%s/%s/%s.def",server->config.dbDir,
			query->curDB, query->tableHead->name);
	fp = fopen(path,"rb");
	if (!fp)
	{
		snprintf(errMsg,MAX_ERR_MSG,BAD_TABLE_ERROR,
			query->tableHead->name);
		msqlDebug1(MOD_ERR,"Unknown table \"%s\"\n",
			query->tableHead->name);
		debugTrace(TRACE_OUT,"processDropTable()");
		return(-1);
	}
	(void)fclose(fp);
	unlink(path);

	if (fileLayout == LAYOUT_FLAT)
	{
		snprintf(path,MSQL_PATH_LEN,"%s/%s/%s.",server->config.dbDir,
			query->curDB, query->tableHead->name);
	}
	else
	{
		snprintf(path,MSQL_PATH_LEN,"%s/%s/%02d/%s.",
			server->config.dbDir, query->curDB, 
			dirHash, query->tableHead->name);
	}
	cp = (char *)rindex(path,'.');
	*cp = 0;
	strcat(path,".dat");
	unlink(path);
	*cp = 0;
	strcat(path,".ofl");
	unlink(path);

	/*
	** Take care of the index files.
	*/
	*cp = 0;
	strcpy(defPath,path);
	strcat(defPath,".idx");
	*cp = 0;
	strcat(path,".idx-");
	cp = path + strlen(path);
	fd = open(defPath,O_RDONLY ,0);
	if (fd >= 0)
	{
		while(read(fd,&tmp,sizeof(tmp)) == sizeof(tmp))
		{
			strcat(path,tmp.name);
			unlink(path);
			strcat(path,".ptr");
			unlink(path);
			*cp = 0;
		}
		close(fd);
	}
	unlink(defPath);

	if (server->config.hasBroker)
		brokerChildSendFlush(query->curDB, query->tableHead->name);
	netOK(query->clientSock);
	debugTrace(TRACE_OUT,"processDropTable()");
	return(0);
}



int processDropIndex(server, query, internal)
	msqld		*server;
	mQuery_t	*query;
	int		internal;
{
	char		defPath[255],
			tmpPath[255];
	int		in,
			out,
			found;
	mIndex_t	tmp;
	cache_t		*entry;


	/* 
	** Note : internal flag used to determine if we've been called
	** directly from a "drop index" query or if some other part
	** of the backend has called us (eg. during the processing
	** of a table drop)
	*/

	debugTrace(TRACE_IN,"processDropIndex()");

	/*
	** Open the definition file and a file to copy it into
	*/
	if ((entry = tableLoadDefinition(server,query->indexDef.table, NULL,
		query->curDB)) == NULL)
	{
		debugTrace(TRACE_OUT,"processDropIndex()");
		return(-1);
	}

	if (entry->fileLayout == LAYOUT_FLAT)
	{
		snprintf(defPath,MSQL_PATH_LEN,"%s/%s/%s.idx", 
			server->config.dbDir, query->curDB, 
			query->indexDef.table);
	}
	else
	{
		snprintf(defPath,MSQL_PATH_LEN,"%s/%s/%02d/%s.idx", 
			server->config.dbDir, query->curDB, 
			entry->dirHash, query->indexDef.table);
	}
	in = open(defPath,O_RDONLY ,0600);
	if (in < 0)
	{
		snprintf(errMsg,MAX_ERR_MSG,"No indices defined for '%s'",
			query->indexDef.table);
		debugTrace(TRACE_OUT,"processDropIndex()");
		return(-1);
	}
	if (entry->fileLayout == LAYOUT_FLAT)
	{
		snprintf(tmpPath,MSQL_PATH_LEN,"%s/%s/%s.idx-tmp", 
			server->config.dbDir, query->curDB, 
			query->indexDef.table);
	}
	else
	{
		snprintf(tmpPath,MSQL_PATH_LEN,"%s/%s/%02d/%s.idx-tmp", 
			server->config.dbDir, query->curDB, 
			entry->dirHash, query->indexDef.table);
	}
	out = open(tmpPath,O_RDWR | O_CREAT ,0600);
	if (out < 0)
	{
		(void)close(in);
		snprintf(errMsg,MAX_ERR_MSG,
			"Can't create index copy file - %s", 
			strerror(errno));
		debugTrace(TRACE_OUT,"processDropIndex()");
		return(-1);
	}

	/*
	** Copy the definitions over, skipping the dropped index
	*/
	found = 0;
	while( read(in, &tmp, sizeof(tmp)) == sizeof(tmp))
	{
		if (strcmp(tmp.name, query->indexDef.name) != 0)
		{
			if(write(out, &tmp, sizeof(tmp)) < 0)
			{
				close(in);
				close(out);
				snprintf(errMsg,MAX_ERR_MSG, "Write failed");
				return(-1);
			}
				
		}
		else
		{
			found = 1;
		}
	}
	
	/*
	** Did we find it?  
	*/
	close(in);
	close(out);
	if (!found)
	{
		snprintf(errMsg,MAX_ERR_MSG, "Unknown index '%s' for '%s'",
			query->indexDef.name, query->indexDef.table);
		unlink(tmpPath);
		debugTrace(TRACE_OUT,"processDropIndex()");
		return(-1);
	}

	/*
	** Yup.  Do the rest of the job.
	*/
	unlink(defPath);
	rename(tmpPath,defPath);
	if (entry->fileLayout == LAYOUT_FLAT)
	{
		snprintf(tmpPath,MSQL_PATH_LEN,"%s/%s/%s.idx-%s", 
		server->config.dbDir, query->curDB, query->indexDef.table, 
		query->indexDef.name);
	}
	else
	{
		snprintf(tmpPath,MSQL_PATH_LEN,"%s/%s/%02d/%s.idx-%s", 
		server->config.dbDir, query->curDB, entry->dirHash,
		query->indexDef.table, query->indexDef.name);
	}
	unlink(tmpPath);

        /*
        ** Invalidate the cache entry so that we reload it
        */

	cacheInvalidateEntry(entry);

	if (! internal)
	{
		if (server->config.hasBroker)
		{
			brokerChildSendFlush(query->curDB, 
				query->indexDef.table);
		}
		netOK(query->clientSock);
	}
	debugTrace(TRACE_OUT,"processDropIndex()");
	return(0);
}



int processDelete(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	int		flist[MAX_FIELDS],
			*curOff,
			rowLen,
			res,
			haveText;
	u_int		count,
			rowNum,
			pos;
	row_t		row;
	mField_t	*fields,
			*curField;
	cache_t		*cacheEntry;
	mCand_t		*candidate;


	debugTrace(TRACE_IN,"processDelete()");
	if((cacheEntry = tableLoadDefinition(server,query->tableHead->name,NULL,
		query->curDB)) == NULL)
	{
		debugTrace(TRACE_OUT,"processDelete()");
		return(-1);
	}

	/*
	** Find the offsets of the given condition
	*/
	utilQualifyConds(query);
	if (utilSetupConds(cacheEntry, query->condHead) < 0)
	{
		debugTrace(TRACE_OUT,"processDelete()");
		return(-1);
	}

	if (tableInitTable(cacheEntry,FULL_REMAP) < 0)
	{
		debugTrace(TRACE_OUT,"processDelete()");
		return(-1);
	}


	fields = utilDupFieldList(cacheEntry);
	if (utilSetupFields(cacheEntry,flist, fields) < 0)
	{
		_freeFieldList(fields);
		debugTrace(TRACE_OUT,"processDelete()");
		return(-1);
	}

	
	lockGetFileLock(server,cacheEntry->dataFD,MSQL_WR_LOCK);

	rowLen = cacheEntry->rowLen;
	count = 0;
	candidate = craSetupCandidate(cacheEntry, query, KEEP_IDENT);
	if (!candidate)
	{
		lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
		_freeFieldList(fields);
		debugTrace(TRACE_OUT,"processDelete");
		return(-1);
	}

	haveText = -1;
	rowNum = craGetCandidate(cacheEntry,candidate);
	while(rowNum != NO_POS)
	{
		/*
		** In a broker environment we don't want one process
		** to hog the lock on a file.  Release the lock now and
		** then to stop other process from blocking for too long
		** if we are doing a huge delete.  Force a small sleep so
		** we can context switch to other processes that may be
		** waiting to get this lock.
		*/
		if (count % 25000 == 0 && count != 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			usleep(100);
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_WR_LOCK);
		}


		/*
		** Is there a limit clause?
		*/
		if (query->rowLimit > 0 && count >= query->rowLimit)
		{
			break;
		}


		if (tableReadRow(cacheEntry,&row,rowNum) < 0)
		{
			break;
		}
		if (!row.header->active)
		{
			rowNum = craGetCandidate(cacheEntry,candidate);
			continue;
		}
		res = compareMatchRow(cacheEntry,&row,query->condHead,query);
		if (res < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			_freeFieldList(fields);
			craFreeCandidate(candidate);
			debugTrace(TRACE_OUT,"processDelete()");
			return(res);
		}
		if (res == 1)
		{
			/*
			** Flag the row as inactive
			*/
			row.header->active = 0;
			count++;
			cacheEntry->sblk->activeRows--;

			/*
			** Blow away any varChar overflow buffers
			** allocated to this row.  If haveText == 0
			** then don't bother trying.  haveText should
			** be set after the first scan.
			*/
			if (haveText != 0)
			{
				curField = cacheEntry->def;
				curOff = flist;
				haveText = 0;
				while(curField)
				{
					if (curField->type != TEXT_TYPE)
					{
						curField = curField->next;
						curOff++;
						continue;
					}
					haveText = 1;
					if (* (row.data + *curOff) == 1)
					{
						bcopy(row.data + *curOff + 1 + 
						sizeof(int),&pos,sizeof(u_int));
						varcharDelete(cacheEntry, pos);
					}
					curField = curField->next;
					curOff++;
				}
			}

			/*
			** Blow away any index entries
			*/
			tableExtractValues(cacheEntry,&row,fields,flist,query);
			indexDeleteIndices(cacheEntry,fields,rowNum);
			tablePushBlankPos(cacheEntry, query->curDB,
				query->tableHead->name, rowNum);

			/*
			** Have to reset this.  If it's an index based
			** lookup then the delete may have shuffled a 
			** a dup up the chain.  A getnext would skip the
			** correct entry.
			*/
			craResetCandidate(candidate, 1);
		}
		rowNum = craGetCandidate(cacheEntry,candidate);
	}
	lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
	snprintf(packet,PKT_LEN,"%d:\n",count);
	netWritePacket(query->clientSock);
	debugTrace(TRACE_OUT,"processDelete()");
	craFreeCandidate(candidate);
	_freeFieldList(fields);
	return(0);
}




int processInsert(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	int		flist[MAX_FIELDS],
			fullFlist[MAX_FIELDS],
			rowLen,
			res,
			count,
			curValueOffset;
	u_int		rowNum;
	row_t		*row;
	mField_t	*curField,
			*curField2;
	cache_t		*cacheEntry;
	mValList_t	*curValue;


	debugTrace(TRACE_IN,"processInsert()");
	if((cacheEntry = tableLoadDefinition(server,query->tableHead->name,NULL,
		query->curDB)) == NULL)
	{
		debugTrace(TRACE_OUT,"processInsert()");
		return(-1);
	}

	/*
	** Find the offsets of the given fields
	*/
	utilQualifyFields(query);
	if (utilSetupFields(cacheEntry,flist,query->fieldHead) < 0)
	{
		debugTrace(TRACE_OUT,"processInsert()");
		return(-1);
	}
	if (utilSetupFields(cacheEntry,fullFlist,cacheEntry->def) < 0)
	{
		debugTrace(TRACE_OUT,"processInsert()");
		return(-1);
	}

	/*
	** Ensure that no field is listed more than once
	*/
	curField = query->fieldHead;
	while(curField)
	{
		curField2 = curField;
		while(curField2)
		{
			if (curField2 == curField)
			{
				curField2 = curField2->next;
				continue;
			}
			if (strcmp(curField->name,curField2->name) == 0 &&
			    strcmp(curField->table,curField2->table) == 0)
			{
				snprintf(errMsg,MAX_ERR_MSG,NON_UNIQ_ERROR, 
					curField->name);
				msqlDebug1(MOD_ERR,"Field '%s' not unique",
					curField->name);
				debugTrace(TRACE_OUT,"processInsert()");
				return(-1);
			}
			curField2 = curField2->next;
		}
		curField = curField->next;
	}


	if (tableInitTable(cacheEntry,KEY_REMAP) < 0)
	{
		lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
		debugTrace(TRACE_OUT,"processInsert()");
		return(-1);
	}

	/*
	** Create a blank row
	*/
	rowLen = cacheEntry->rowLen;
	row = &(cacheEntry->row);

	/*
	** Work through the sets of insert values
	*/
	count = 0;
	curValue = query->insertValHead;
	while(curValue)
	{
		
		curValueOffset = -1;
		curField = query->fieldHead;
		while(curField)
		{
			if (curValue == NULL || 
				curValue->offset <= curValueOffset)
			{
				snprintf(errMsg,MAX_ERR_MSG,
					"Missing value for field '%s'",
					curField->name);
				msqlDebug1(MOD_ERR,
					"Missing value for field '%s'",
					curField->name);
				debugTrace(TRACE_OUT,"processInsert()");
				return(-1);
			}
			curField->value = curValue->value;

			/*
			** Check for a NULL value
			*/
			if ((curField->flags & NOT_NULL_FLAG) &&
				curField->value->nullVal == 1)
			{
				snprintf(errMsg,MAX_ERR_MSG,BAD_NULL_ERROR, 
					curField->name);
				msqlDebug1(MOD_ERR,
					"Field \"%s\" cannot be null\n",
					curField->name);
				debugTrace(TRACE_OUT,"processInsert()");
				return(-1);
			}

			/*
			** Check the field and value types
			*/
			res = 0;
			if (curField->value->nullVal == 0)
			{
                        	res = typeValidConditionTarget(curField->type,
                                	curField->value);
			}
                        if (res == -1)
                        {
                                snprintf(errMsg, MAX_ERR_MSG, TYPE_ERROR,
                                        curField->name);
                                msqlDebug1(MOD_ERR, TYPE_ERROR, curField->name);
                                return(-1);
                        }
                        if (res == -2)
                        {
                                /* Error msg already set */
                                return(-1);
                        }


			curField = curField->next;
			curValue = curValue->next;
		}
		

		/*
		** Find a place to put this row
		*/

		lockGetFileLock(server,cacheEntry->dataFD,MSQL_WR_LOCK);
		rowNum = tablePopBlankPos(cacheEntry,query->curDB,
			query->tableHead->name);


		/*
		** Check for a unique primary key if we have one.
		*/
		res=indexCheckIndices(cacheEntry,query->fieldHead,NULL,NO_POS);
		if (res < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			debugTrace(TRACE_OUT,"processInsert()");
			return(-1);
		}
		if (res == 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			strcpy(errMsg,KEY_UNIQ_ERROR);
			msqlDebug0(MOD_ERR,
				"Non unique value for unique index\n");
			debugTrace(TRACE_OUT,"processInsert()");
			return(-1);
		}

		/*
		** Fill in the given fields and dump it to the table file
		*/

		bzero(row->data,rowLen);
		if (tableFillRow(cacheEntry,row,query->fieldHead,flist) < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			debugTrace(TRACE_OUT,"processInsert()");
			return(-1);
		}

		if (_checkNullFields(cacheEntry,row,fullFlist) < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			debugTrace(TRACE_OUT,"processInsert()");
			return(-1);
		}

		if (indexCheckAllForNullFields(cacheEntry,row,fullFlist) < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			debugTrace(TRACE_OUT,"processInsert()");
			return(-1);
		}
		indexInsertIndices(cacheEntry, query->fieldHead, rowNum);
		if(tableWriteRow(cacheEntry,NULL,rowNum, query) < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			snprintf(errMsg,MAX_ERR_MSG,WRITE_ERROR, 
				(char*)strerror(errno));
			msqlDebug0(MOD_ERR,"Error on data write\n");
			debugTrace(TRACE_OUT,"processInsert()");
			return(-1);
		}
		cacheEntry->sblk->activeRows++;
		lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);	
		count++;
	}

	snprintf(packet,PKT_LEN,"%d:\n",count);
	netWritePacket(query->clientSock);
	debugTrace(TRACE_OUT,"processInsert()");
	return(0);
}



int processUpdate(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	int		flist[MAX_FIELDS],
			fullFlist[MAX_FIELDS],
			rowLen,
			res,
			count,
			updated;
	u_int		rowNum;
	row_t		row;
	cache_t		*cacheEntry;
	mCand_t		*candidate;
	

	debugTrace(TRACE_IN,"processUpdate()");
	if((cacheEntry = tableLoadDefinition(server,query->tableHead->name,NULL,
		query->curDB)) == NULL)
	{
		debugTrace(TRACE_OUT,"processUpdate()");
		return(-1);
	}

	/*
	** Find the offsets of the given fields and condition
	*/
	utilQualifyFields(query);
	utilQualifyConds(query);
	if (utilSetupFields(cacheEntry,flist,query->fieldHead) < 0)
	{
		debugTrace(TRACE_OUT,"processUpdate()");
		return(-1);
	}
	if (utilSetupFields(cacheEntry,fullFlist,cacheEntry->def) < 0)
	{
		debugTrace(TRACE_OUT,"processUpdate()");
		return(-1);
	}
	if (utilSetupConds(cacheEntry,query->condHead) < 0)
	{
		debugTrace(TRACE_OUT,"processUpdate()");
		return(-1);
	}

	rowLen = cacheEntry->rowLen;

	if (tableInitTable(cacheEntry,FULL_REMAP) < 0)
	{
		debugTrace(TRACE_OUT,"processUpdate()");
		return(-1);
	}

	lockGetFileLock(server,cacheEntry->dataFD,MSQL_WR_LOCK);
	count = 0;
	candidate = craSetupCandidate(cacheEntry, query, KEEP_IDENT);
	if (!candidate)
	{
		lockGetFileLock(server,cacheEntry->dataFD, MSQL_UNLOCK);
		return(-1);
	}
	rowNum = craGetCandidate(cacheEntry, candidate);
	while(rowNum != NO_POS)
	{
		/*
		** In a broker environment we don't want one process
		** to hog the lock on a file.  Release the lock now and
		** then to stop other process from blocking for too long
		** if we are doing a huge delete.  Force a small sleep so
		** we can context switch to other processes that may be
		** waiting to get this lock.
		*/

		if (count % 25000 == 0 && count != 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			usleep(10);
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_WR_LOCK);
		}

		if (tableReadRow(cacheEntry,&row,rowNum) < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			return(-1);
		}
		if (!row.header->active)
		{
			rowNum = craGetCandidate(cacheEntry, candidate);
			continue;
		}
		res = compareMatchRow(cacheEntry,&row,query->condHead,query);
		if (res < 0)
		{
			lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
			craFreeCandidate(candidate);
			return(-1);
		}
		if (res == 1)
		{
			if (cacheEntry->indices)
			{
				tableExtractValues(cacheEntry,&row,
					cacheEntry->def,fullFlist,query);
			}
			bcopy(row.buf, cacheEntry->row.buf,
				(cacheEntry->rowLen + HEADER_SIZE));
			if (tableUpdateValues(cacheEntry,&row,query->fieldHead,
				flist) < 0)
			{
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
				debugTrace(TRACE_OUT,"processUpdate()");
				craFreeCandidate(candidate);
				return(-1);
			}
			/*
			** Force a remap just in case the overflow file
			** has been extended.  Also reset the pointers in
			** the rew struct as the data map may move as a
			** result of this.
			*/
			if (tableInitTable(cacheEntry,FULL_REMAP) < 0)
			{
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
				debugTrace(TRACE_OUT,"processUpdate()");
				craFreeCandidate(candidate);
				return(-1);
			}
			if (tableReadRow(cacheEntry,&row,rowNum) < 0)
			{
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
				return(-1);
			}
			if (_checkNullFields(cacheEntry,&row,fullFlist) < 0)
			{
				bcopy(cacheEntry->row.buf, row.buf,
					(cacheEntry->rowLen + HEADER_SIZE));
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
				debugTrace(TRACE_OUT,"processUpdate()");
				craFreeCandidate(candidate);
				return(-1);
			}
			if (indexCheckAllForNullFields(cacheEntry, &row,
				fullFlist)<0)
			{
				bcopy(cacheEntry->row.buf, row.buf,
					(cacheEntry->rowLen + HEADER_SIZE));
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
				debugTrace(TRACE_OUT,"processUpdate()");
				craFreeCandidate(candidate);
				return(-1);
			}
        		res = indexCheckIndices(cacheEntry, query->fieldHead, 
				cacheEntry->def, rowNum);
        		if (res < 0)
        		{
				bcopy(cacheEntry->row.buf, row.buf,
					(cacheEntry->rowLen + HEADER_SIZE));
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
				craFreeCandidate(candidate);
                		debugTrace(TRACE_OUT,"processUpdate()");
                		return(-1);
        		}
        		if (res == 0)
        		{
				bcopy(cacheEntry->row.buf, row.buf,
					(cacheEntry->rowLen + HEADER_SIZE));
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
                		strcpy(errMsg,KEY_UNIQ_ERROR);
                		msqlDebug0(MOD_ERR,
					"Non unique value for unique index\n");
                		debugTrace(TRACE_OUT,"processUpdate()");
				craFreeCandidate(candidate);
                		return(-1);
        		}

			if(tableWriteRow(cacheEntry,&row,rowNum,query) < 0)
			{
				lockGetFileLock(server,cacheEntry->dataFD,
					MSQL_UNLOCK);
				snprintf(errMsg,MAX_ERR_MSG,WRITE_ERROR, 
					(char*)strerror(errno));
				msqlDebug0(MOD_ERR,"Error on data write\n");
				debugTrace(TRACE_OUT,"processUpdate()");
				craFreeCandidate(candidate);
				return(-1);
			}
			count++;
			if (cacheEntry->indices)
			{
				if (indexUpdateIndices(cacheEntry,
					cacheEntry->def, rowNum, &row, 
					fullFlist,&updated,candidate->index,
					query)<0)
				{
					lockGetFileLock(server,
						cacheEntry->dataFD,MSQL_UNLOCK);
					craFreeCandidate(candidate);
					return(-1);
				}
				if (updated)
					craResetCandidate(candidate, 1);
			}
		}
		rowNum = craGetCandidate(cacheEntry, candidate);
	}
	lockGetFileLock(server,cacheEntry->dataFD,MSQL_UNLOCK);
	snprintf(packet,PKT_LEN,"%d:\n",count);
	netWritePacket(query->clientSock);
	craFreeCandidate(candidate);
	debugTrace(TRACE_OUT,"processUpdate()");
	return(0);
}



void processCreateDB(server, sock, db)
	msqld	*server;
	int	sock;
	char	*db;
{
	char	path[MSQL_PATH_LEN];
	DIR	*dirp;

	/*
	** See if the directory exists
	*/

	(void)snprintf(path,MSQL_PATH_LEN,"%s/%s", server->config.dbDir, db);
	dirp = opendir(path);
	if (dirp)
	{
		closedir(dirp);
		snprintf(packet,PKT_LEN, 
			"-1:Error creating database : %s exists!\n",db);
                netWritePacket(sock);
		return;
	}

	/*
	** Create the directory
	*/
	if (mkdir(path,0700) < 0)
	{
		snprintf(packet,PKT_LEN, "-1:Error creating database (%s)\n",
			(char*)strerror(errno));
		netWritePacket(sock);
		return;
	}
	netEndOfList(sock);
}


void processCopyDB(server, sock,fromDB, toDB)
	msqld	*server;
	int	sock;
	char	*fromDB,
		*toDB;
{
	char	fromPath[MSQL_PATH_LEN],
		toPath[MSQL_PATH_LEN];


	(void)snprintf(toPath,MSQL_PATH_LEN,"%s/%s",server->config.dbDir,toDB);
	(void)snprintf(fromPath,MSQL_PATH_LEN, "%s/%s", server->config.dbDir,
		fromDB);
	if (tableCopyDirectory(fromPath, toPath) < 0)
	{
		snprintf(packet, PKT_LEN, "-1:%s\n", errMsg);
                netWritePacket(sock);
		processDropDB(server,-1,toDB);
		return;
	}
	netEndOfList(sock);
}



void processMoveDB(server, sock, fromDB, toDB)
	msqld	*server;
	int	sock;
	char	*fromDB,
		*toDB;
{
	char	fromPath[MSQL_PATH_LEN],
		toPath[MSQL_PATH_LEN];
	DIR	*dirp;

	/*
	** See if the "to" directory exists
	*/
	(void)snprintf(toPath,MSQL_PATH_LEN,"%s/%s",server->config.dbDir,toDB);
	dirp = opendir(toPath);
	if (dirp)
	{
		closedir(dirp);
		snprintf(packet,PKT_LEN,
			"-1:Error moving database : %s exists!\n",toDB);
                netWritePacket(sock);
		return;
	}

	/*
	** See if the "from" directory exists
	*/

	(void)snprintf(fromPath,MSQL_PATH_LEN,"%s/%s", server->config.dbDir,
		fromDB);
	dirp = opendir(fromPath);
	if (!dirp)
	{
		snprintf(packet,PKT_LEN, 
			"-1:Error moving database : %s doesn't exist!\n",
			fromDB);
                netWritePacket(sock);
		return;
	}
	closedir(dirp);

	/*
	** Move the directory
	*/
	if (rename(fromPath, toPath) < 0)
	{
		snprintf(packet,PKT_LEN,"-1:Error moving database (%s)\n",
			(char*)strerror(errno));
		netWritePacket(sock);
		return;
	}
	netEndOfList(sock);
	cacheInvalidateDatabase(server, fromDB);
}



void processDropDB(server, sock, db)
	msqld	*server;
	int	sock;
	char	*db;
{
	char	path[MSQL_PATH_LEN];

	/*
	** See if the directory exists
	*/

	(void)snprintf(path,MSQL_PATH_LEN,"%s/%s", server->config.dbDir, db);
	if (_deleteDirectory(path) < 0)
	{
		if (sock < 0)
			return;
		snprintf(packet,PKT_LEN, 
			"-1:Error dropping database : %s doesn't exist\n",db);
		netWritePacket(sock);
		return;
	}
	if (sock >= 0)
	{
		netEndOfList(sock);
	}
	cacheInvalidateDatabase(server, db);
}


void processCheckTable(server, sock, db, table)
	msqld	*server;
	int	sock;
	char	*db,
		*table;
{
	cache_t	*entry;
	mIndex_t *cur;

	entry = tableLoadDefinition(server,table,NULL,db);
	if (!entry)
	{
		snprintf(packet,PKT_LEN,
			"-1:Can't access table %s.%s!\n",db,table);
                netWritePacket(sock);
		return;
	}
	cur = entry->indices;
	while(cur)
	{
		if (!cur->handle.native)
		{
			snprintf(packet,PKT_LEN, "%d\n", MSQL_TABLE_BAD_INDEX);
                	netWritePacket(sock);
			return;
		}
		cur = cur->next;
	}
	snprintf(packet,PKT_LEN, "%d\n", MSQL_TABLE_OK);
	netWritePacket(sock);
}



void processExportTable(server, sock, db, table, path, orderField, query)
	msqld		*server;
        int     	sock;
        char    	*db, 
			*table, 
			*path,
			*orderField;
	mQuery_t	*query;
{
        cache_t 	*entry,
			*tmpTable;
        off_t   	rowNum;
        row_t   	row;  
        int     	fullFlist[MAX_FIELDS],
                	fd,
			found;
	char		*tmpConfig,
			delimiter;
        struct  stat 	buf;
	mQuery_t	tmpQuery;
	mOrder_t	tmpOrder;
	mField_t	*curField;
        
        debugTrace(TRACE_IN,"msqlExportTable()");
        msqlDebug3(MOD_GENERAL,"Exporting '%s:%s' to '%s')\n",db,table,path);

	tmpConfig = configGetCharEntry("system", "export_delimiter");

	if (tmpConfig)
	{
		delimiter = *tmpConfig;
	}
	else
	{
		delimiter = ',';
	}
        if((entry = tableLoadDefinition(server,table,NULL,db)) == NULL)
        {
                snprintf(packet,PKT_LEN,"-1:%s\n",errMsg);
                netWritePacket(sock);
                debugTrace(TRACE_OUT,"msqlExportTable()");
                return;
        }

        if (stat(path,&buf) >= 0)
        {
                snprintf(packet,PKT_LEN,
                        "-1:Export will not overwrite an existing file\n");
                netWritePacket(sock);
                debugTrace(TRACE_OUT,"msqlExportTable()");
                return;
        }

	if (orderField)
	{
		found = 0;
		curField = entry->def;
		while(curField && !found)
		{
			if (strcmp(curField->name, orderField) == 0)
				found++;
			curField = curField->next;
		}
		if (!found)
		{
                	snprintf(packet,PKT_LEN,"-1:Unknown sort field\n");
                	netWritePacket(sock);
                	debugTrace(TRACE_OUT,"msqlExportTable()");
                	return;
		}
	}


        fd = open(path, O_CREAT|O_TRUNC|O_RDWR, 0700);
        if (fd < 0)
        {
                snprintf(packet,PKT_LEN,"-1:Can't create output file\n");
                netWritePacket(sock);
                debugTrace(TRACE_OUT,"msqlExportTable()");
                return;
        }

	/*
	** Create the result table ready for exporting
	*/

	bzero(&tmpQuery, sizeof(tmpQuery));

/*
	tmpQuery.orderHead = (mOrder_t *)malloc(sizeof(mOrder_t));
	bzero(tmpQuery.orderHead, sizeof(mOrder_t));
	tmpQuery.orderTail = tmpQuery.orderHead;
*/

	tmpQuery.fieldHead = (mField_t*)malloc(sizeof(mField_t));
	bzero(tmpQuery.fieldHead, sizeof(mField_t));
	tmpQuery.fieldTail = tmpQuery.fieldHead;
	strcpy(tmpQuery.fieldHead->name, "*");
	tmpQuery.fieldHead = utilExpandFieldWildCards(entry, 
		tmpQuery.fieldHead);

	tmpTable = tableCreateTmpTable(server, NULL, NULL, entry, NULL,
		tmpQuery.fieldHead, QUERY_FIELDS_ONLY);
	tmpTable->result = 0;

	if (doSelect(entry, &tmpQuery, DEST_TABLE, BOOL_TRUE, tmpTable) < 0)
	{
                snprintf(packet,PKT_LEN,"-1:Initial select failed\n");
                netWritePacket(sock);
		tableFreeTmpTable(server, tmpTable);
		/*free(tmpQuery.orderHead);*/
		_cleanFields(tmpQuery.fieldHead);
                debugTrace(TRACE_OUT,"msqlExportTable()");
                return;
	}

	/*
	** Sort the table if needed
	*/

	if (orderField)
	{
		bzero(&tmpOrder, sizeof(tmpOrder));
		strcpy(tmpOrder.table, table);
		strcpy(tmpOrder.name, orderField);
		tmpOrder.dir = ASC;
	
		tmpQuery.orderHead = &tmpOrder;
		tmpQuery.orderTail = tmpQuery.orderHead;
		utilQualifyOrder(&tmpQuery);
		if (sortCreateSortedTable(server, tmpTable, &tmpQuery) < 0)
		{
                	snprintf(packet,PKT_LEN,"-1:Table sort failed\n");
                	netWritePacket(sock);
			tableFreeTmpTable(server, tmpTable);
			/*free(tmpQuery.orderHead);*/
			_cleanFields(tmpQuery.fieldHead);
                	debugTrace(TRACE_OUT,"msqlExportTable()");
                	return;
		}
		tmpQuery.orderHead = tmpQuery.orderTail = NULL;

	}


	/*
	** Export the table
	*/
        if (utilSetupFields(tmpTable,fullFlist,tmpTable->def) < 0)
        {
                snprintf(packet,PKT_LEN,"-1:%s\n",errMsg);
                netWritePacket(sock);
		tableFreeTmpTable(server, tmpTable);
		/*free(tmpQuery.orderHead);*/
		_cleanFields(tmpQuery.fieldHead);
                debugTrace(TRACE_OUT,"msqlExportTable()");
                close(fd);
                return;
        }
        rowNum = 0;
        while(rowNum != NO_POS)
        {
                if (tableReadRow(tmpTable,&row,rowNum) < 0)
                {
                        break;
                }
                if (!row.header->active)
                {
                        rowNum++;
                        continue;
                }
                tableExtractValues(tmpTable,&row,entry->def,fullFlist,query);
                utilFormatExport(packet,entry->def,delimiter);
                if (write(fd,packet,strlen(packet)) < 0)
                {
                        close(fd);
                        snprintf(packet,PKT_LEN,
                                "-1:File write failed\n");
                        netWritePacket(sock);
			tableFreeTmpTable(server, tmpTable);
			/*free(tmpQuery.orderHead);*/
			_cleanFields(tmpQuery.fieldHead);
                        debugTrace(TRACE_OUT,"msqlExportTable()");
                        return;
                }
                rowNum++;
        }
        netEndOfList(sock);
        close(fd);
	/*free(tmpQuery.orderHead);*/
	_cleanFields(tmpQuery.fieldHead);
	tableFreeTmpTable(server, tmpTable);
}


void processImportTable(server, sock, db, table, path, query)
	msqld		*server;
        int     	sock;
        char    	*db, 
			*table, 
			*path;
	mQuery_t	*query;
{
        cache_t 	*entry;
        int     	fullFlist[MAX_FIELDS],
                	fd;
        struct  stat 	buf;
        
        debugTrace(TRACE_IN,"msqlImportTable()");
        msqlDebug3(MOD_GENERAL,"Importing '%s' to '%s:%s'\n",path,db,table);
        if((entry = tableLoadDefinition(server,table,NULL,db)) == NULL)
        {
                snprintf(packet,PKT_LEN,"-1:%s\n",errMsg);
                netWritePacket(sock);
                debugTrace(TRACE_OUT,"msqlImportTable()");
                return;
        }

        if (stat(path,&buf) < 0)
        {
                snprintf(packet,PKT_LEN,
                        "-1:Import file missing or unreadable\n");
                netWritePacket(sock);
                debugTrace(TRACE_OUT,"msqlImportTable()");
                return;
        }
        fd = open(path, O_RDONLY, 0700);
        if (fd < 0)
        {
                snprintf(packet,PKT_LEN,"-1:Can't open import file\n");
                netWritePacket(sock);
                debugTrace(TRACE_OUT,"msqlImportTable()");
                return;
        }
        if (utilSetupFields(entry,fullFlist,entry->def) < 0)
        {
                snprintf(packet,PKT_LEN,"-1:%s\n",errMsg);
                netWritePacket(sock);
                debugTrace(TRACE_OUT,"msqlImportTable()");
                close(fd);
                return;
        }
        netEndOfList(sock);
}



void processQuery(server, query, client, queryText)
	msqld		*server;
	mQuery_t	*query;
	int		client;
	char		*queryText;
{
	int		res = 0;

	debugTrace(TRACE_IN,"processQuery()");
	if (!query->curDB)
	{
		netError(query->clientSock,"No current database\n");
		msqlDebug0(MOD_ERR,"No current database\n");
		debugTrace(TRACE_OUT,"processQuery()");
		return;
	}

	/*
	** Do query logging if needed
	*/
	if (server->logFP)
		logQuery(server->logFP,&server->conArray[client],queryText);
	if (server->updateFP && query->command != MSQL_SELECT)
		logQuery(server->updateFP,&server->conArray[client],queryText);


	/*
	** Process the query
	*/
	switch(query->command)
	{
		case MSQL_SELECT: 
			if (!aclCheckPerms(READ_ACCESS))
			{
				netError(query->clientSock,"Access Denied\n");
				debugTrace(TRACE_OUT,"processQuery()");
				return;
			}
			res = selectProcessSelect(server, query);
			break;

		case CREATE_TABLE: 
			_checkWriteAccess(server);
			res = processCreateTable(server, query);
			break;

		case CREATE_INDEX: 
			_checkWriteAccess(server);
			res = processCreateIndex(server, query);
			break;

		case CREATE_SEQUENCE: 
			_checkWriteAccess(server);
			res = processCreateSequence(server, query);
			break;

		case MSQL_UPDATE: 
			_checkWriteAccess(server);
			res = processUpdate(server, query);
			if (configGetIntEntry("system", "sync_updates"))
			{
				cacheSyncCache();
			}
			break;

		case MSQL_INSERT: 
			_checkWriteAccess(server);
			res = processInsert(server, query);
			if (configGetIntEntry("system", "sync_updates"))
			{
				cacheSyncCache();
			}
			break;

		case MSQL_DELETE: 
			_checkWriteAccess(server);
			res = processDelete(server, query);
			if (configGetIntEntry("system", "sync_updates"))
			{
				cacheSyncCache();
			}
			break;

		case DROP_TABLE: 
			_checkWriteAccess(server);
			res = processDropTable(server, query);
			break;

		case DROP_INDEX: 
			_checkWriteAccess(server);
			res = processDropIndex(server, query, 0);
			break;

		case DROP_SEQUENCE: 
			_checkWriteAccess(server);
			res = processDropSequence(server, query);
			break;

	}
	if (res < 0)
	{
		netError(query->clientSock,errMsg);
	}
	debugTrace(TRACE_OUT,"processQuery()");
}
