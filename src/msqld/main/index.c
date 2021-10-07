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
** $Id: index.c,v 1.13 2010/11/15 04:24:23 bambi Exp $
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

#include <fcntl.h>
#include <errno.h>

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/main/index.h>
#include <msqld/main/table.h>
#include <msqld/main/parse.h>
#include <msqld/main/memory.h>
#include <msqld/main/compare.h>
#include <common/types/types.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

extern	char		errMsg[];

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/



static int _fillIndexBuffer(index,newFields, oldFields)
	mIndex_t 	*index;
	mField_t	*newFields,
			*oldFields;
{
	char		*cp;
	int		count,
			fieldID;
	mField_t	*curField;

	cp = index->buf;	
	if (index->idxType == IDX_CHAR || index->idxType == IDX_BYTE)
	{
		bzero(cp, index->length + 1);
	}
	count = 0;
	while(index->fields[count] != -1)
	{
		fieldID = index->fields[count];
		curField = newFields;
		while(curField)
		{
			if (curField->fieldID == fieldID)
				break;
			curField = curField->next;
		}
		if (!curField)
		{
			curField = oldFields;
			while(curField)
			{
				if (curField->fieldID == fieldID)
					break;
				curField = curField->next;
			}
		}
		if (!curField)
		{
			/* missing field index field.  Trapped later */
			return(-1);
		}
		if (parseCopyValue(cp,curField->value,curField->type,
			curField->length,0) < 0)
		{
			snprintf(errMsg,MAX_ERR_MSG,NULL_JOIN_COND_ERR,
				curField->name);
			return(-1);
		}
		cp += curField->length;
		count++;
	}
	return(0);
}


static int _compareIndexValues(val1,val2)
        mVal_t	*val1,
                *val2;
{
	int	res = 0;

        switch(typeBaseType(val1->type))
        {
                case CHAR_TYPE:
			res = strcmp((char*)val1->val.charVal,
				(char*)val2->val.charVal);
			res = res == 0;
			break;

                case BYTE_TYPE:
			res = localByteCmp(val1->val.byteVal,val2->val.byteVal,
				typeFieldSize(val1->type));
			res = res == 0;
			break;

                case INT8_TYPE:
                case UINT8_TYPE:
			res = val1->val.int8Val == val2->val.int8Val;
			break;

                case INT16_TYPE:
                case UINT16_TYPE:
			res = val1->val.int16Val == val2->val.int16Val;
			break;

                case INT32_TYPE:
                case UINT32_TYPE:
			res = val1->val.int32Val == val2->val.int32Val;
			break;

                case INT64_TYPE:
                case UINT64_TYPE:
			res = val1->val.int64Val == val2->val.int64Val;
			break;

                case REAL_TYPE:
			res = val1->val.realVal == val2->val.realVal;
			break;
        }
	return(res);
}


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



void indexCloseIndices(entry)
	cache_t	*entry;
{
	mIndex_t	*cur,
			*prev;

	cur = entry->indices;
	while(cur)
	{
		if (cur->handle.native)
		{
			/* Not a dirty index so close it */
			idxClose(&cur->handle);
		}
		if (cur->buf)
		{
			free(cur->buf);
			cur->buf = NULL;
		}
		prev = cur;
		cur = cur->next;
		free(prev);
	}
}



mIndex_t *indexLoadIndices(server, entry, table,db)
	msqld	*server;
	cache_t	*entry;
        char    *table,
                *db;
{
        mIndex_t *headIndex = NULL,
                *tmpIndex,
                *prevIndex,
                *curIndex,
		buf;
        char    path[MSQL_PATH_LEN];
        int     numBytes,
		count,
                fd;

        debugTrace(TRACE_IN,"loadIndices()");
	if (entry->fileLayout == LAYOUT_FLAT)
	{
        	(void)snprintf(path,MSQL_PATH_LEN,"%s/%s/%s.idx", 
			server->config.dbDir, db,table);
	}
	else
	{
        	(void)snprintf(path,MSQL_PATH_LEN,"%s/%s/%02d/%s.idx", 
			server->config.dbDir, db, entry->dirHash, table);
	}
        fd = open(path,  O_RDWR | O_CREAT, 0600);
        if (fd < 0)
        {
                snprintf(errMsg,MAX_ERR_MSG, 
			"Can't open index definition for '%s'",table);
		msqlDebug0(MOD_ERR,errMsg);
                debugTrace(TRACE_OUT,"loadIndices()");
                return(NULL);
        }
        numBytes = read(fd,&buf,sizeof(buf));
	count = 0;
	while(numBytes > 0)
	{
        	if (numBytes < sizeof(buf))
        	{
                	snprintf(errMsg,MAX_ERR_MSG, TABLE_READ_ERROR,table,
				(char*)strerror(errno));
                	msqlDebug1(MOD_ERR, 
				"Error reading \"%s\" index definition\n",
				table);
			close(fd);
                	debugTrace(TRACE_OUT,"loadIndices()");
                	return(NULL);
        	}

                curIndex = (mIndex_t *)malloc(sizeof(mIndex_t));
                bcopy(&buf, curIndex, sizeof(mIndex_t));
                if (!headIndex)
                {
                        headIndex = prevIndex = curIndex;
			curIndex->next = NULL;
                }
                else
                {
			/* ensure the list is sorted by DESC field count */
			tmpIndex = headIndex;
			prevIndex = NULL;
			while(tmpIndex)
			{
				if (curIndex->fieldCount > tmpIndex->fieldCount)
				{
					curIndex->next = tmpIndex;
					if (prevIndex)
						prevIndex->next=curIndex;
					else
						headIndex = curIndex;
					break;
				}
				prevIndex = tmpIndex;
				tmpIndex = tmpIndex->next;
			}
			if (!tmpIndex)
			{
                        	prevIndex->next = curIndex;
				curIndex->next = NULL;
			}
                }
		if (entry->fileLayout == LAYOUT_FLAT)
		{
			snprintf(path,MSQL_PATH_LEN,"%s/%s/%s.idx-%s",
				server->config.dbDir,db,table, curIndex->name);
		}
		else
		{
			snprintf(path,MSQL_PATH_LEN,"%s/%s/%02d/%s.idx-%s",
				server->config.dbDir,db, entry->dirHash, 
				table, curIndex->name);
		}
		if (idxOpen(path, curIndex->idxType, &curIndex->environ,
			&curIndex->handle) < 0)
		{
			msqlDebug2(MOD_ERR,"Bad index file  %s.%s\n", db,table);
		}

		curIndex->buf = (char *)malloc(curIndex->length + 1);
        	numBytes = read(fd,&buf,sizeof(buf));
		count++;
        }
        close(fd);

        debugTrace(TRACE_OUT,"loadIndices()");
        return(headIndex);
}




int indexInsertIndexValue(entry,fields,pos,index)
	cache_t		*entry;
	mField_t	*fields;
	u_int		pos;
	mIndex_t 	*index;
{
	int	res;

	if (pos == NO_POS)
		pos = entry->sblk->numRows;
	if(_fillIndexBuffer(index,fields,NULL) < 0)
	{
		return(-1);
	}

	if (!index->handle.native)
	{
		/* Skip this index as it's dirty */
		msqlDebug2(MOD_ERR,"Dirty index ignored %s.%s\n",
			entry->db, entry->table);
		return(0);
	}

	res = idxInsert(&index->handle, index->buf, index->length,(off_t)pos);
	if (res < 0)
		return(-1);
	return(0);
}




int indexInsertIndices(entry,fields,pos)
	cache_t	*entry;
	mField_t	*fields;
	u_int	pos;
{
	mIndex_t	*curIndex;

	curIndex = entry->indices;
	while(curIndex)
	{
		if (indexInsertIndexValue(entry,fields,pos,curIndex) < 0)
			return(-1);
		curIndex = curIndex->next;
	}
	return(0);
}





/****************************************************************************
**      _deleteIndices
**
**      Purpose : 
**      Args    : 
**      Returns : 
**      Notes   : 
*/

int indexDeleteIndices(entry,fields,pos)
	cache_t	*entry;
	mField_t	*fields;
	u_int	pos;
{
	mIndex_t	*curIndex;
	int	res;

	if (pos == NO_POS)
		pos = entry->sblk->numRows;
	curIndex = entry->indices;
	while(curIndex)
	{
		if (!curIndex->handle.native)
		{
			/* Skip this index as it's dirty */
			curIndex = curIndex->next;
			msqlDebug2(MOD_ERR,"Dirty index ignored %s.%s\n",
				entry->db, entry->table);
			continue;
		}

		if (_fillIndexBuffer(curIndex,fields,NULL) < 0)
			return(-1);
		res = idxDelete(&curIndex->handle, curIndex->buf,
			curIndex->length, (off_t)pos); 
		if (res < 0)
			return(-1);
		curIndex = curIndex->next;
	}
	return(0);
}




/****************************************************************************
**      _updateIndices
**
**      Purpose : 
**      Args    : 
**      Returns : 
**      Notes   : 
*/

int indexUpdateIndices(entry,oldFields,pos,row,flist,updated,candIdx,query)
	cache_t		*entry;
	mField_t	*oldFields;
	u_int		pos;
        row_t 		*row;
	int		*flist,
			*updated,
			candIdx;
	mQuery_t	*query;
{
	mIndex_t	*curIndex;
	int		count,
			offset,
			idxCount,
			needUpdate;
	mField_t	*newFields = NULL,
			*newTail = NULL,
			*newCur,
			*oldCur;

	/*
	** Create a duplicate field list for the complete row containing
	** the new values
	*/
	oldCur = oldFields;
	while(oldCur)
	{
		newCur = memMallocField();
		bcopy(oldCur,newCur,sizeof(mField_t));
		newCur->value = NULL;
		if (!newFields)
		{
			newFields = newTail = newCur;
			newFields->next = NULL;
		}
		else
		{
			newTail->next = newCur;
			newTail = newCur;
		}
		oldCur = oldCur->next;
	}
	tableExtractValues(entry, row, newFields, flist, query);

	/*
	** Work through the indices
	*/

	idxCount = 0;
	*updated = 0;
	curIndex = entry->indices;
	while(curIndex)
	{
		/*
		** Is this a valid index?
		*/
		if (!curIndex->handle.native)
		{
			/* Skip this index as it's dirty */
			curIndex = curIndex->next;
			idxCount++;
			msqlDebug2(MOD_ERR,"Dirty index ignored %s.%s\n",
				entry->db, entry->table);
			continue;
		}

		/*
		** Do we need to update this index?
		*/
		count = 0;
		needUpdate = 0;
		while(curIndex->fields[count] != -1)
		{
			offset = curIndex->fields[count];
			oldCur = oldFields;
			newCur = newFields;
			while(offset)
			{
				oldCur = oldCur->next;
				newCur = newCur->next;
				offset--;
			}
			if (_compareIndexValues(oldCur->value,newCur->value)!=1)
			{
				needUpdate = 1;
				break;
			}
			count++;
		}
		if (!needUpdate)
		{
			/* Skip this index */
			curIndex = curIndex->next;
			idxCount++;
			continue;
		}


		if (idxCount == candIdx)
			*updated = 1;
		if (_fillIndexBuffer(curIndex,oldFields,NULL) < 0)
			return(-1);
		if (idxDelete(&curIndex->handle, curIndex->buf,
			curIndex->length, (off_t)pos) < 0)
			return(-1);
		if (_fillIndexBuffer(curIndex,newFields,NULL) < 0)
			return(-1);
		if (idxInsert(&curIndex->handle, curIndex->buf,
			curIndex->length, (off_t)pos) < 0)
			return(-1);

		curIndex = curIndex->next;
		idxCount++;
	}

	/*
	** Free up the duplicatate field list and return
	*/
	newCur = newFields;
	while(newCur)
	{
		newCur = newCur->next;
		parseFreeValue(newFields->value);
		newFields->value = NULL;
		memFreeField(newFields);
		newFields = newCur;
	}
	return(0);
}



int indexCheckIndex(entry, newFields, oldFields, index, rowNum)
        cache_t *entry;
        mField_t *newFields,
		*oldFields;
	mIndex_t *index;
	u_int	rowNum;
{
	idx_nod	node;
	u_int	curRow;

	/*
	** If it's a non-unique index, just bail
	*/
	if (!index->unique)
		return(1);
		
	/*
	** Is this a valid index?
	*/
	if (!index->handle.native)
	{
		/* Skip this index as it's dirty */
		msqlDebug2(MOD_ERR,"Dirty index ignored %s.%s\n",
			entry->db, entry->table);
		return(-1);
	}

	/*
	** Try to read the index value and bail if it's there
	*/
	if(_fillIndexBuffer(index,newFields,oldFields) < 0)
		return(-1);
	if (idxLookup(&index->handle, index->buf, index->length, IDX_EXACT,
		&node) == IDX_OK)
	{
		curRow = (u_int)node.data;
		if (curRow != rowNum)
			return(0);
	}
	return(1);
}



int indexCheckIndices(entry, newFields, oldFields, rowNum)
        cache_t *entry;
        mField_t *newFields,
		*oldFields;
	u_int	rowNum;
{
	mIndex_t	*curIndex;

	curIndex = entry->indices;
	while(curIndex)
	{
		if (indexCheckIndex(entry, newFields, oldFields, curIndex,
			rowNum)==0)
		{
			return(0);
		}
		curIndex = curIndex->next;
	}
	return(1);
}



int indexCheckNullFields(entry,row,index,flist)
        cache_t *entry;
        row_t 	*row;
	mIndex_t *index;
	int	*flist;
{
        mField_t 	*curField;
        int     	*offset,
			count,
			field;
        u_char  *data;

        debugTrace(TRACE_IN,"checkIndexNullFields()");
        data = row->data;
	for (count=0; count<5; count++)
	{
       		offset = 0;
		field = index->fields[count];
		if(field == -1)
		{
			break;
		}
       		curField = entry->def;
		offset = flist;
       		while(curField && field)
       		{
			offset++;
			curField = curField->next;
			field--;
		}
               	if (!*(data + *offset))
               	{
                       	snprintf(errMsg,MAX_ERR_MSG, NULL_INDEX_ERROR, 
				curField->name);
                       	debugTrace(TRACE_OUT,"checkIndexNullFields()");
                       	return(-1);
		}
	}
	debugTrace(TRACE_OUT,"checkIndexNullFields()");
	return(0);
}



int indexCheckAllForNullFields(entry,row, flist)
        cache_t 	*entry;
        row_t   	*row; 
        int     	*flist;
{
        mIndex_t 	*curIndex;
        u_char  	*data;  

        debugTrace(TRACE_IN,"indexCheckAllForNullFields()");
        curIndex = entry->indices;
        data = row->data;
        while(curIndex) 
        {
                if (indexCheckNullFields(entry,row,curIndex,flist) < 0)
		{
        		debugTrace(TRACE_OUT,"indexCheckAllForNullFields()");
                        return(-1);
		}
                curIndex = curIndex->next;
        }
        debugTrace(TRACE_OUT,"indexCheckAllForNullFields()");
        return(0);
}


