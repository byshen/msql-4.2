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
** $Id: uidx.c,v 1.4 2002/06/29 04:08:59 bambi Exp $
**
*/

/*
** Module	: cra : Union Index (uidx)
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

#include <common/portability.h>

/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/cra/uidx.h>
#include <msqld/main/main.h>
#include <msqld/main/parse.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

/* HACK */
extern	char	*msqlHomeDir;
extern  char    errMsg[];

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


int unionIdxCreate(new)
	idx_hnd	*new;
{
	static	char *dbDir = NULL;
	char	path[255];
	static	u_int count = 0;

	if (!dbDir)
		dbDir = configGetCharEntry("general","db_dir");
	count++;
	sprintf(path,"%s/.tmp/uidx-%d-%d",dbDir,(int)getpid(),count);
	unlink(path);

	if(idxCreate(path, IDX_MEM_AVL, 0600, sizeof(int), IDX_INT32,
		IDX_DUP,NULL)<0)
	{
		return(-1);
	}
	if(idxOpen(path,IDX_MEM_AVL,NULL,new) < 0)
		return(-1);
	return(0);
}
		


int unionIdxLookup(idx, val)
	idx_hnd	*idx;
	int	val;
{
	idx_nod	node;
	int	res;

	res = idxLookup(idx, (char *)&val, sizeof(int), IDX_EXACT,&node);
	if (res == IDX_OK)
		return(1);
	return(0);
}



int unionIdxGet(idx,pos)
	idx_hnd	*idx;
	int	pos;
{
	static	idx_cur cursor;
	static	idx_nod	node;
	int	rowid;

	if (pos == 0)
	{
		if (idxGetFirst(idx,&node) < 0)
			return(NO_POS);
		idxSetCursor(idx,&cursor);
	}
	else
	{
		if (idxGetNext(idx, &cursor, &node) < 0)
			return(NO_POS);
	}
	rowid = (int)*(int *)(node.key);
	return(rowid);
}



	
void unionIdxInsert(idx,val)
	idx_hnd	*idx;
	int	val;
{
	idxInsert(idx, (char *)&val, sizeof(int), val);
}


void unionIdxFree(idx)
	idx_hnd	*idx;
{
	char	path[255];

	strcpy(path, idx->path);
	idxClose(idx);
	unlink(path);
}



int unionIdxCreateTmpIndex(new, tmpIdx, index, conds)
	idx_hnd	*new,
		*tmpIdx;
	mIndex_t *index;
	mCond_t	*conds;
{
	int	res,
 		condCount;
	idx_nod	node;
	idx_cur	cursor;
	mCond_t	*curCond;
	char	*cp;

	msqlDebug0(MOD_CANDIDATE,"createTmpIndex entered\n");

	/*
	** Validate the conditions - must be literal.  Build the key
	** value while we're checking
	*/
	curCond = NULL;
	condCount = 0;
	bzero(index->buf, index->length+1);
	cp = index->buf;
	while(condCount < index->fieldCount)
	{
		curCond = conds;
		while(curCond)
		{
			if (curCond->fieldID == index->fields[condCount])
			{
				if (curCond->value->type == IDENT_TYPE ||
					curCond->value->type == SYSVAR_TYPE)
				{
					return(-1);
				}
				parseCopyValue(cp,curCond->value,curCond->type,
					curCond->length,0);
				cp += curCond->length;
				break;
			}
			curCond = curCond->next;
		}
		condCount++;
	}



	/*
	** OK, looks fine from here.  Build the new index
	*/
	unionIdxCreate(new);
	if (curCond->op == BETWEEN_OP)
	{
		res = idxLookup(&(index->handle), index->buf, index->length,
			IDX_CLOSEST, &node);
		parseCopyValue(index->buf,curCond->maxValue,
			curCond->type,curCond->length,0);
	}
	else
	{
		res = idxLookup(&(index->handle), index->buf, index->length,
			IDX_EXACT, &node);
	}
	if (res != IDX_OK)
	{
		return(-1);
	}
	idxSetCursor(&(index->handle), &cursor);  
	while(res == IDX_OK)
	{
		res = idxCompareValues(index->keyType,node.key,index->buf,
			index->length);
		if (curCond->op == BETWEEN_OP)
		{
			if (res >= 1)
				break;
		}
		else
		{
			if (res != 0)
				break;
		}
		msqlDebug1(MOD_KEY,"	Adding row %d\n",(int)node.data);
		if (tmpIdx->native)
		{
			if (unionIdxLookup(tmpIdx, (int)node.data))
			{
				unionIdxInsert(new,(int)node.data);
			}
		}
		else
		{
			unionIdxInsert(new,(int)node.data);
		}
		res = idxGetNext(&(index->handle), &cursor, &node);
	}
	idxCloseCursor(&(index->handle), &cursor);
	return(0);
}
