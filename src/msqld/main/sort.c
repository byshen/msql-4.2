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
** $Id: sort.c,v 1.17 2011/11/23 00:44:28 bambi Exp $
**
*/

/*
** Module	: main : sort
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

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <common/types/types.h>
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/version.h>
#include <msqld/main/table.h>
#include <msqld/main/util.h>
#include <msqld/main/compare.h>
#include <libmsql/msql.h>


#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

/* HACK */
extern	char		*packet,
			errMsg[];
extern	int		outSock;

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static void swapRows(entry, low, high, query)
	cache_t		*entry;
	u_int		low,
			high;
	mQuery_t	*query;
{
	row_t		lowRow,
			highRow,
			*tmp;

	tableReadRow(entry,&lowRow,low);
	tableReadRow(entry,&highRow,high);
	tmp = utilDupRow(entry,&lowRow,&(entry->row));
	tablePlaceRow(entry, &highRow, low, query);
	tablePlaceRow(entry, tmp, high, query);
}


static void bSort(entry, query, olist, first, last)
	cache_t		*entry;
	mQuery_t	*query;
	int		*olist;
	u_int		first,
			last;
{
	int	curRow,
		res,
		swapCount;
	row_t	row1,
		row2;

	while(first < last)
	{
		curRow = first;
		/*
		maxNum = first;
		minNum = last;
		tableReadRow(entry, &rowMin, minNum);
		tableReadRow(entry, &rowMax, maxNum);
		*/
		
		swapCount = 0;
		while(curRow < last)
		{
			res = tableReadRow(entry, &row1, curRow);
			if (res < 0)
				return;
			res = tableReadRow(entry, &row2, curRow + 1);
			if (res < 0)
				return;
			res = compareRows(entry,&row1, &row2, query->orderHead,
				olist);
			if (res > 0 )
			{
				swapRows(entry, curRow, curRow + 1, query);
				swapCount = 1;
			}
			curRow++;
		}
		if (swapCount == 0)
		{
			break;
		}
	}
}


#ifdef INCLUDE_QSORT

static void qSort(entry, query, olist, lBound, uBound)
	cache_t		*entry;
	mQuery_t	*query;
	int		*olist;
	u_int		lBound,
			uBound;
{
	int	lCount,
		uCount,
		res;
	row_t	curRow,
		pivotRow;

	res = tableReadRow(entry, &pivotRow, lBound);
	if (res < 0)
		return;
	lCount = lBound + 1;
	uCount = uBound;

	while(lCount <= uCount)
	{
		res = tableReadRow(entry,&curRow, lCount);
		if (res < 0)
			return;
		while(lCount <= uBound &&
			compareRows(entry,&curRow, &pivotRow, query->orderHead,
			olist)<=0)
		{
			lCount++;
			res = tableReadRow(entry,&curRow, lCount);
			if (res < 0)
			{
				return;
			}
		}

		res =  tableReadRow(entry,&curRow, uCount);
		if (res < 0)
			return;
		while(uCount > lBound &&
			compareRows(entry,&curRow, &pivotRow, query->orderHead,
			olist)>=0)
		{
			uCount--;
			res = tableReadRow(entry,&curRow, uCount);
			if (res < 0)
			{
				return;
			}
		}

		if (lCount < uCount)
		{
			swapRows(entry, lCount, uCount, query);
			lCount++;
			uCount--;
		}
	}

	/*
	** Move the pivot into place
	*/
	swapRows(entry, uCount, lBound, query);
	if (uCount != lBound)
	{
		qSort(entry, query, olist, lBound, uCount - 1);
	}
	if (uCount < uBound)
	{
		qSort(entry, query, olist, uCount+1, uBound);
	}
}

#endif


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


int sortCreateSortedTable(server, entry, query)
	msqld		*server;
	cache_t		*entry;
	mQuery_t	*query;
{
	int8_t	int8;
	int16_t	int16;
	int	int32,
		olist[MAX_FIELDS],
		res,
		onDisk,
		idxType,
		idxMode;
	u_int	rowNum;
	double	realBuf;
	char	dataPath[255],
		oldPath[255],
		idxPath[255],
		*pathPtr,
		*bufPtr;
	idx_hnd handle;
	idx_nod	node;
	row_t	row;
	cache_t	tmp;
	sblk_t	sblock;
	idx_cur	cursor;
#ifdef HUGE_T
	int64_t	int64;
#endif


	debugTrace(TRACE_IN,"createSortedTable()");

	if(tableInitTable(entry,FULL_REMAP) < 0)
	{
		debugTrace(TRACE_OUT,"createSortedTable()");
		return(-1);
	}
	if (entry->sblk->numRows == 0)
	{
		return(0);
	}
	if (utilSetupOrder(entry,olist,query->orderHead) < 0)
	{
		debugTrace(TRACE_OUT,"createSortedTable()");
		return(-1);
	}


	/*
	** OK, this may look wierd.  Sorting a table using any
	** sorting algorithm sucks for performance.  The engine
	** can create a new, indexed table at a rate of over 1,000
	** rows per second on a Pentium yet a sort of 1200 row
	** table will take 20 to 30 seconds.  So, a quick way to
	** handle it is to create a dummy index containing the
	** sort field.  The rowID is stored in the index also.  A
	** left to right parse of the index tree once generated will
	** give us the sorted rowID's.
	**
	** Note, this only works for a single field.  For multi-fields
	** we still do this for the first order field to reduce the
	** work of the qSort implementation (should be mainly sorted by
	** the first effort).
	**
	** If the table is huge then we don't want to thrash the VM
	** subsytem by using an in-memory (i.e. malloced) AVL tree.
	** If we think the index will be larger than the configured
	** max memory limit then use an on-disk AVL
	*/

	if (query->orderHead->type != TEXT_TYPE)
	{
		if ((query->orderHead->length * entry->sblk->numRows) < 
			(server->config.sortMaxMem * 1000) &&
			(configGetIntEntry("system", "system_has_swap")))
		{
			onDisk = 0;
			pathPtr = NULL;
			idxType = IDX_MEM_AVL;
			idxMode = 0;
		}
		else
		{
			onDisk = 1;
			pathPtr = idxPath;
			idxType = IDX_AVL;
			idxMode = 0600;
			snprintf(idxPath,MSQL_PATH_LEN,"%s/.tmp/order-%d",
                        	server->config.dbDir, getpid());
		}

		switch(typeBaseType(query->orderHead->type))
		{
			case CHAR_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_CHAR, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case INT8_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_INT8, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case INT16_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_INT16, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;


			case INT32_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_INT32, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case INT64_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_INT64, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case UINT8_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_UINT8, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case UINT16_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_UINT16, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case UINT32_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_UINT32, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case UINT64_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_UINT64, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			case REAL_TYPE:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_REAL, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;

			default:
				idxCreate(pathPtr,idxType,idxMode,
					query->orderHead->length,
					IDX_BYTE, IDX_DUP, NULL);
				idxOpen(pathPtr, idxType, NULL, &handle);
				break;
		}

		/* 
		** Scan the table creating the index on the fly 
		*/
		rowNum = 0;
		while(rowNum < entry->sblk->numRows)
		{
			if (tableReadRow(entry,&row, rowNum) < 0)
				return(-1);
			if (row.header->active == 0)
			{
				rowNum++;
				continue;
			}
			switch(typeBaseType(query->orderHead->type))
			{
			    case CHAR_TYPE:
			    case BYTE_TYPE:
				bufPtr = (char *)row.data + *olist + 1;
				break;

			    case INT8_TYPE:
			    case UINT8_TYPE:
				bcopy(row.data + *olist + 1,&int8, 1);
				bufPtr = (char *)&int8;
				break;

			    case INT16_TYPE:
			    case UINT16_TYPE:
				bcopy(row.data + *olist + 1,&int16, 2);
				bufPtr = (char *)&int16;
				break;

			    case INT32_TYPE:
			    case UINT32_TYPE:
				bcopy4(row.data + *olist + 1,&int32);
				bufPtr = (char *)&int32;
				break;

			    case INT64_TYPE:
			    case UINT64_TYPE:
				bcopy8(row.data + *olist + 1,&int64);
				bufPtr = (char *)&int64;
				break;

			    case REAL_TYPE:
				bcopy8(row.data + *olist + 2,&realBuf);
				bufPtr = (char *)&realBuf;
				break;

			    default:
				snprintf(errMsg, MAX_ERR_MSG,
					"Invalid field type '%s' in sort",
					msqlTypeName(query->orderHead->type));
				return(-1);
				break;
			}

			idxInsert(&handle,(char *)bufPtr,
				query->orderHead->length, (off_t)rowNum);
			rowNum++;
		}

		/*
		** Create the dummy output table 
		*/
		snprintf(dataPath,MSQL_PATH_LEN,"%s/.tmp/%s.tmp", 
			server->config.dbDir, entry->table);
		tmp.dataFD = open(dataPath, O_CREAT|O_RDWR, 0600);
        	bzero(&sblock, sizeof(sblock));
        	sblock.version = DB_VERSION;
        	sblock.numRows = sblock.activeRows = 0;
        	sblock.freeList = NO_POS;
		write(tmp.dataFD,&sblock,SBLK_SIZE);

		tmp.indices = NULL;
		tmp.result = 0;
		tmp.def = entry->def;
		tmp.overflowFD = -1;
		tmp.dataMap = NULL;
		tmp.remapData = 1;
		tmp.remapOverflow = 0;
		tableInitTable(&tmp, FULL_REMAP);
		tmp.rowDataLen = entry->rowDataLen;
		tmp.rowLen = entry->rowDataLen + (8 -
                	((entry->rowDataLen + HEADER_SIZE) % 8));
        	tmp.row.buf = (u_char *)malloc(entry->rowLen + HEADER_SIZE + 2);
        	tmp.row.header = (hdr_t *)tmp.row.buf;
        	tmp.row.data = tmp.row.buf + HEADER_SIZE;
        	tmp.sblk = (sblk_t *)tmp.dataMap;


		/*
		** Parse the index creating a new data table
		*/
		if (query->orderHead->dir == DESC)
			idxGetLast(&handle, &node);
		else
			idxGetFirst(&handle, &node);
		idxSetCursor(&handle, &cursor);
		res = IDX_OK;
		
		while(res == IDX_OK)
		{
			rowNum = (u_int)node.data;
			tableReadRow(entry, &row, rowNum);
			tableWriteRow(&tmp, &row, NO_POS, query);
			if (query->orderHead->dir == DESC)
				res = idxGetPrev(&handle, &cursor, &node);
			else
				res = idxGetNext(&handle, &cursor, &node);
		}
		idxClose(&handle);
		if (onDisk == 1)
		{
			unlink(idxPath);
		}

		/*
		** Swap the new table into place
		*/
		snprintf(oldPath,MSQL_PATH_LEN,"%s/.tmp/%s.dat", 
			server->config.dbDir, entry->table);
		munmap(entry->dataMap, entry->size);
		close(entry->dataFD);
		unlink(oldPath);
		entry->dataFD = tmp.dataFD;
		entry->size = tmp.size;
		entry->dataMap = tmp.dataMap;
		entry->sblk = (sblk_t *)entry->dataMap;

#if defined(_OS_OS2)
           // under OS/2, is not possible to rename an open file!
           // and also the file is not deleted after use.
           // The code works, but files aren't renamed/deleted.
           // .tmp directory gets filled from tmp files.
           // Code could be hacked to allow rename, but it not
           // possible to delete it after close().
#endif
		rename(dataPath, oldPath);
		free(tmp.row.buf);
		tmp.row.buf = NULL;
	}


	/*
	** If it's a Multi-field sort, kick in the qSort.  If the table
	** is huge then a normal qsort will kill the box because of the
	** amount of recursion required.  Some benchmark tools sort a
	** 30,000 row table on multiple keys.
	*/

	if (query->orderHead->next || query->orderHead->type == TEXT_TYPE)
	{
		/* qSort(entry,query,olist,0,entry->sblk->numRows - 1); */
		bSort(entry, query, olist, 0, entry->sblk->numRows - 1);
	}
	debugTrace(TRACE_OUT,"createSortedTable()");
	return(0);
}

