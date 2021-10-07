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
** $Id: distinct.c,v 1.7 2005/06/25 20:50:53 bambi Exp $
**
*/

/*
** Module	: main : distinct
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

#include <common/portability.h>

/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/main/util.h>
#include <msqld/main/table.h>
#include <msqld/main/compare.h>
#include <common/types/types.h>
#include <libmsql/msql.h>
                           


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/

extern	char	*packet,
		errMsg[];
extern	int	outSock;


int distinctCreateTable(server, entry)
	msqld	*server;
	cache_t	*entry;
{
	idx_hnd handle;
	idx_nod	node;
	mField_t *curField;
	row_t	row,
		dupRow;
	u_int	curRowNum,
		curDupNum;
	int	flist[MAX_FIELDS],
		*offset,
		onDisk,
		dataLength,
		needTextCheck,
		res;
        char 	path[MSQL_PATH_LEN];
	

	if(tableInitTable(entry,FULL_REMAP) < 0)
	{
		return(-1);
	}
	if(entry->sblk->numRows == 0)
	{
		return(0);
	}
	if((entry->sblk->numRows * entry->rowLen) < 
		(server->config.sortMaxMem * 1000))
	{
		onDisk = 0;
	}
	else
	{
		onDisk = 1;
	}

	if (utilSetupFields(entry,flist,entry->def) < 0)
	{
		return(-1);
	}

	if (onDisk)
	{
		snprintf(path,MSQL_PATH_LEN,"%s/.tmp/distinct-%d",
			server->config.dbDir, getpid());
		idxCreate(path, IDX_AVL, 0600, entry->rowDataLen, IDX_BYTE, 
			IDX_DUP, NULL);
		idxOpen(path, IDX_AVL, NULL, &handle);
	}
	else
	{
		idxCreate(NULL, IDX_MEM_AVL, 0, entry->rowDataLen, IDX_BYTE, 
			IDX_DUP, NULL);
		idxOpen(NULL, IDX_MEM_AVL, NULL, &handle);
	}

	curRowNum = 0;
	while(tableReadRow(entry,&row,curRowNum) > 0)
	{
		if (!row.header->active)
		{
			curRowNum++;
			continue;
		}

		res = idxLookup(&handle, (char*)row.data, entry->rowDataLen, 
			IDX_EXACT, &node);
		if (res == IDX_OK)
		{
			row.header->active = 0;
		}
		else
		{
			idxInsert(&handle,(char*)row.data,entry->rowDataLen,0);
		}
		curRowNum++;
	}
	idxClose(&handle);
	if (onDisk)
		unlink(path);

	/*
	** Handle TEXT fields with overlows.  This is a pain but there's
	** not much else you can do with variable length fields
	*/
	curRowNum = 0;
	while(tableReadRow(entry,&row,curRowNum) > 0)
	{
		needTextCheck = 0;
		offset = flist;
		curField = entry->def;
		while(curField)
		{
			if (curField->type == TEXT_TYPE)
			{
				bcopy(row.data + *offset + 1, &dataLength, 4);
				if (dataLength > curField->length)
				{
					needTextCheck = 1;
					break;
				}
			}
			offset++;
			curField = curField->next;
		}
		if (needTextCheck)
		{
			curDupNum = curRowNum + 1;
			while(tableReadRow(entry,&dupRow,curDupNum) > 0)
			{
				if (dupRow.header->active == 0)
				{
					curDupNum++;
					continue;
				}
				if (checkDupRow(entry,row.data,dupRow.data)==0)
				{
					dupRow.header->active = 0;
				}
				curDupNum++;
			}
		}
		curRowNum++;
	}
	
	return(0);
}

