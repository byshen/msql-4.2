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
** $Id: cra.c,v 1.19 2011/08/05 06:46:50 bambi Exp $
**
*/

/*
** Module	: cra : cra
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
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/cra/cra.h>
#include <msqld/cra/uidx.h>
#include <common/types/types.h>
#include <msqld/main/main.h>
#include <msqld/main/parse.h>
#include <msqld/main/sysvar.h>
#include <msqld/main/net.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

/* HACK */
extern	char	*msqlHomeDir;
extern	char	*packet;
extern  char    errMsg[];
extern	int	outSock;

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

static void extractCondValue(buf,cond,max)
        char    *buf;
        mCond_t  *cond;
	int	max;
{  
        int     length;
	int8_t	int8;
	int16_t	int16;

        switch(typeBaseType(cond->value->type))
        {
                case INT8_TYPE:
                case UINT8_TYPE:
			if (max)
                        	bcopy(&(cond->maxValue->val.int8Val),buf,1);
			else
                        	bcopy(&(cond->value->val.int8Val),buf,1);
                        break;

                case INT16_TYPE:
                case UINT16_TYPE:
			if (max)
                        	bcopy(&(cond->maxValue->val.int16Val),buf,2);
			else
                        	bcopy(&(cond->value->val.int16Val),buf,2);
			break;
                
                case INT32_TYPE:
                case UINT32_TYPE:
			if (max)
                        	bcopy(&(cond->maxValue->val.int32Val),buf,4);
			else
                        	bcopy(&(cond->value->val.int32Val),buf,4);
                        break;

                case INT64_TYPE:
                case UINT64_TYPE:
			if (max)
                        	bcopy(&(cond->maxValue->val.int64Val),buf,8);
			else
                        	bcopy(&(cond->value->val.int64Val),buf,8);
                        break;

                case REAL_TYPE:
			if (max)
                        	bcopy(&(cond->maxValue->val.realVal),buf,8);
			else
                        	bcopy(&(cond->value->val.realVal),buf,8);
                        break;

                case CHAR_TYPE:
			if (max)
                        	length = strlen(
					(char*)cond->maxValue->val.charVal);
			else
                        	length = strlen(
					(char*)cond->value->val.charVal);
                        if (length > cond->length)
                                length = cond->length;
			if (max)
                        	bcopy((char *)cond->maxValue->val.charVal,
					buf,length);
			else
                        	bcopy((char *)cond->value->val.charVal,
					buf,length);
                        break;

		case BYTE_TYPE:
			if (max)
			{
			 	bcopy((char *)cond->maxValue->val.byteVal,
                                        buf,cond->length);
			}
			else
			{
			 	bcopy((char *)cond->value->val.byteVal,
                                        buf,cond->length);
			}
			break;
        }
}


static void extractFieldValue(buf,cond)
	char	*buf;
	mCond_t	*cond;
{
	extractCondValue(buf,cond,0);
}

static void extractMaxFieldValue(buf,cond)
	char	*buf;
	mCond_t	*cond;
{
	if (cond->op == BETWEEN_OP) 
		extractCondValue(buf,cond,1);
	if (cond->op == LE_OP || cond->op == LT_OP)
		extractCondValue(buf,cond,0);
}



#define	CACHE_LEN	5
static 	int	candCount = 0;
static	mCand_t	*candCache[CACHE_LEN];

static mCand_t *_mallocCand()
{
	mCand_t	*new;

	if (candCount > 0)
        {
                candCount--;
                new = candCache[candCount];
        }
        else
        {
                new = (mCand_t *)malloc(sizeof(mCand_t));
        }

	new->type = new->index = new->ident = new->length = new->rowID =
		new->keyType = new->nextPos = 0;
	new->rangeMin = new->rangeMax = (u_char *) NULL;
	new->buf = new->maxBuf = NULL;
	*new->idx_name = 0;
	new->unionIndex = NULL;
	return(new);
}

static void _freeCand(ptr)
	mCand_t	*ptr;
{
        if (candCount >= CACHE_LEN)
        {
                free(ptr);
        }
        else
        {
                candCache[candCount] = ptr;
                candCount++;
        }
}

/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


/*
** This is the mSQL 2.0 Candidate Row Abstraction code.  In short, it's
** the query optimsation module.
*/

void craFreeCandidate(cand)
	mCand_t	*cand;
{
	if (cand)
	{
		if (cand->buf)
		{
			free(cand->buf);
			cand->buf = NULL;
		}
		if (cand->maxBuf)
		{
			free(cand->maxBuf);
			cand->maxBuf = NULL;
		}
		_freeCand(cand);
	}
}




int craSetCandidateValues(inner, cand, fields, conds, row, query)
	cache_t		*inner;
	mCand_t		*cand;
	mField_t	*fields;
	mCond_t		*conds;
	row_t		*row;
	mQuery_t	*query;
{
	mIndex_t	*curIndex;
	mField_t	*curField,
			tmpField;
	mCond_t		*curCond;
	int		count,
			fieldID;
	char		*cp;

	if (cand->type == CAND_SEQ)
	{
		return(0);
	}
	curIndex = inner->indices;
	count = cand->index;
	while(count && curIndex)
	{
		count--;
		curIndex = curIndex->next;
	}
	if (!curIndex)
		return(0);

	/*
	** We can't use fillIndexBuffer() here as some of the
	** values are in the cond list and others are in the ident
	** fields list.  Sigh...
	*/
	curIndex = inner->indices;
	count = 0;
	while(count < cand->index)
	{
		curIndex = curIndex->next;
		count ++;
	}
	cp = cand->buf;	
	bzero(cp, curIndex->length + 1);
	count = 0;
	while(curIndex->fields[count] != -1)
	{
		fieldID = curIndex->fields[count];

		/*
		** Look for an ident value first
		*/
		curField = fields;
		while(curField)
		{
			if(curField->fieldID == fieldID)
			{
				if (parseCopyValue(cp,curField->value,curField->type,
					curField->length,0) < 0)
				{
					snprintf(errMsg,MAX_ERR_MSG,
						NULL_JOIN_COND_ERR,
						curField->name);
					return(-1);
				}
				cp+=curField->length;
				break;
			}
			curField = curField->next;
		}
		if (curField)
		{
			/* Found it */	
			count++;
			continue;
		}


		/*
		** OK, check out the normal conditions
		*/
		curCond = conds;
		while(curCond)
		{
			if(curCond->fieldID == fieldID)
			{
				/*
				** Could be a sysvar ?
				*/
				strcpy(tmpField.name,curCond->name);
				if (sysvarCheckVariable(inner, &tmpField) == 0)
				{
					sysvarGetVariable(inner,row,&tmpField,
						query);
					if(parseCopyValue(cp,tmpField.value,
					     tmpField.type,tmpField.length,0)<0)
					{
						snprintf(errMsg,MAX_ERR_MSG,
							NULL_JOIN_COND_ERR,
							curField->name);
						return(-1);
					}
					cp += tmpField.length;
				}
				else
				{
					/*
					** Nope, it's a normal field
					*/
					if(parseCopyValue(cp,curCond->value,
					     curCond->type,curCond->length,0)<0)
					{
						snprintf(errMsg,MAX_ERR_MSG,
							NULL_JOIN_COND_ERR,
							curField->name);
						return(-1);
					}
					cp += curCond->length;
				}
				break;
			}
			curCond = curCond->next;
		}
		if (curCond)
		{
			/* Found it */	
			count++;
			continue;
		}
/***/		abort();
	}

	cand->lastPos = NO_POS;
	return(0);
}





mTable_t *craReorderTableList(query)
	mQuery_t	*query;
{
	/*
	** This isn't part of the CRA but it still an important part
	** of the query optimiser so it's in this module.
	**
	** Work out the best order for the tables of a join to be
	** processed as the user may have specified them in a bad
	** order (like certain benchmark suites).
	*/

	mCond_t		*curCond;
	mTable_t	*head = NULL,
			*tail = NULL,
			*prevTable = NULL,
			*curTable = NULL,
			*pivotTable = NULL,
			*pivotPrev = NULL;
	int		condCount,
			maxCount;


	/*
	** Any table with a literal condition goes to the head
	** of the list
	*/
	curTable = query->tableHead;
	prevTable = NULL;
	while(curTable)
	{
		curCond = query->condHead;
		while(curCond)
		{
			if (strcmp(curCond->table,curTable->name) == 0 &&
				curCond->value->type != IDENT_TYPE)
			{
				break;
			}
			curCond = curCond->next;
		}
		if (curCond)
		{
			/* 
			** Pull it out of the original list 
			*/
			if (prevTable)
				prevTable->next = curTable->next;
			else
				query->tableHead = curTable->next;

			/* 
			** Add it to the new list 
			*/
			if (head)
				tail->next = curTable;
			else
				head = curTable;
			tail = curTable;
			tail->next = NULL;

			/* 
			** Move on to the next 
			*/
			if (prevTable)
				curTable = prevTable->next;
			else
				curTable = query->tableHead;
			continue;
		}
		prevTable = curTable;
		curTable = curTable->next;
	}

	/*
	** Next, move the remaining tables onto the list ensuring that
	** the "pivot" table of the remainder is moved first.
	*/
	while(query->tableHead)
	{
		maxCount = 0;
		curTable = query->tableHead;
		prevTable = NULL;
		while(curTable)
		{
			curCond = query->condHead;
			condCount = 0;
			while(curCond)
			{
				if (curCond->subCond)
				{
					curCond = curCond->next;
					continue;
				}
				if (strcmp(curCond->table, curTable->name)==0)
				{
					condCount++;
				}
				else 
				if (curCond->value)
				{
				    if(curCond->value->type==IDENT_TYPE &&
				     strcmp(curCond->value->val.identVal->seg1, 
				     curTable->name)==0)
				    {
					condCount++;
				    }
				}
				curCond = curCond->next;
				continue;
			}
			if (condCount > maxCount)
			{
				pivotTable = curTable;
				pivotPrev = prevTable;
				maxCount = condCount;
			}
			prevTable = curTable;
			curTable = curTable->next;
		}
		if (maxCount == 0)
		{
			/* 
			** Not good.  Break out so we just append the 
			** rest and bail 
			*/
			break;
		}
		if (pivotPrev)
			pivotPrev->next = pivotTable->next;
		else
			query->tableHead = pivotTable->next;
		if (head)
			tail->next = pivotTable;
		else
			head = pivotTable;
		tail = pivotTable;
		tail->next = NULL;
	}
	if (head)
		tail->next = query->tableHead;
	else
		head = query->tableHead;
	return(head);
}


mCand_t *craSetupCandidate(entry, query, ignoreIdent)
	cache_t		*entry;
	mQuery_t	*query;
	int		ignoreIdent;
{
	mCand_t		*new;
	mIndex_t	*curIndex = NULL,
			*candIndex = NULL;
	mCond_t		*curCond,
			*idxCond;
	int		count,
			field,
			identKey,
			index,
			doRowID,
			doRange,
			haveUnique,
			indexMatches,
			rowID,
			validIndices[NUM_INDEX],
			indexWeights[NUM_INDEX],
			numKeys,
			numEntries;
	char		*tableName,
			*cp,
			*cp1;
#ifdef UNION_INDEX
	static  	idx_hnd	tmpIdx,
			prevIdx;
	int 		tmpIndex,
			tmpWeight, 
			done;
#endif

	/*
	** This is the query optimiser!  The conditions are checked to
	** see which access mechanism is going to provide the fastest
	** query result.
	*/

	if (query->explainOnly)
	{
		snprintf(packet,PKT_LEN,
		"Setup Candidate called\n\tTable = %s\n\tCheck ident comparisons = %s\n", 
		entry->result?entry->resInfo:entry->table, 
		ignoreIdent?"No":"Yes");
		netWritePacket(query->clientSock);
	}
	new = _mallocCand();
	new->buf = NULL;
	new->maxBuf = NULL;

	/*
	** We can't handle OR's yet so do a quick scan through the
	** conditions
	*/
	curCond = query->condHead;
	while(curCond)
	{
		if (curCond->bool == OR_BOOL)
		{
			new->type = CAND_SEQ;
			new->nextPos = 0;
			msqlDebug1(MOD_ACCESS,
				"setupCandidate() : Using SEQ for %s\n",
				entry->table);
			if (query->explainOnly)
			{
				strcpy(packet,"\tFound OR! Using SEQ\n");
				netWritePacket(query->clientSock);
			}
			return(new);
		}
		curCond = curCond->next;
	}

	/*
	** First test is to see if we can do a row_id based access
	*/
	rowID = 0;
	curCond = query->condHead;
	doRowID = 0;
	while(curCond)
	{
		if (strcmp(curCond->name, "_rowid")==0 && 
		    curCond->op == EQ_OP)
		{
			doRowID++;
			rowID = curCond->value->val.int64Val;
			break;
		}
		curCond = curCond->next;
	}
	if (doRowID == 1)
	{
		new->type = CAND_ROWID;
		new->lastPos = NO_POS;
		new->length = 4;
		new->rowID = rowID;
		msqlDebug1(MOD_ACCESS, 
			"setupCandidate() : Using _ROWID for %s\n",
			entry->table);
		if (query->explainOnly)
		{
			strcpy(packet,"\tUsing _ROWID\n");
			netWritePacket(query->clientSock);
		}
		return(new);
	}


	/*
	** Look for the wierd _seq case.  We need this because it's
	** possible (in fact normal) to just select the seq value.  In
	** that situation we can't expect to just fill in the blanks
	** for a table row access as there may not be any table data
	** yet (e.g. the first insert into a table that uses the SEQ
	** as a key).  Use this for everything but _rowid and _timestamp. 
	** It's ugly but it works.
	*/
	if (query->fieldHead)
	{
		if (query->fieldHead->next == NULL && 
			query->fieldHead->sysvar == 1)
		{
			if (strcmp(query->fieldHead->name, "_rowid") != 0 &&
			    strcmp(query->fieldHead->name, "_timestamp") != 0)
			{
				new->type = CAND_SYS_VAR;
				new->lastPos = NO_POS;
				new->length = 0;
				new->rowID = 0;
				msqlDebug1(MOD_ACCESS,
				"setupCandidate() : Fake sysvar for %s\n",
				entry->table);
				return(new);
			}
		}
	}

	/*
	** Check for an  equality index condition.  Match on the longest index
	** or the first unique.  Keep an eye out for index prefix
	** matches that we could use
	*/
	new->type = CAND_SEQ;
	new->nextPos = 0;
	new->length = 0;
	doRange = 0;
	curIndex = entry->indices;
	index = 0;
	if (*entry->cname != 0)
		tableName = entry->cname;
	else
		tableName = entry->table;

	haveUnique = indexMatches = 0;
	while(curIndex)
	{
		if (!curIndex->handle.native)
		{
                        msqlDebug2(MOD_ERR,
				"Invalid index on %s:%s\n",
                                curIndex->table,curIndex->name);
			curIndex = curIndex->next;
			continue;
		}
		idxCond = NULL;
		field = 0;
		identKey = 0;
		while(field < MAX_INDEX_WIDTH && curIndex->fields[field] != -1)
		{
			curCond = query->condHead;
			while(curCond)
			{
				if (strcmp(curCond->table, entry->table)!=0)
				{
					curCond=curCond->next;
					continue;
				}
				if (curCond->value->type == IDENT_TYPE ||
				    curCond->value->type == SYSVAR_TYPE)
				{
					identKey |= 1;
				}
				if(strcmp(tableName,curIndex->table)==0 &&
				   curCond->fieldID == curIndex->fields[field])
				{
					if( (curCond->op == EQ_OP ||
					     curCond->op == BETWEEN_OP ||
					     curCond->op == LT_OP ||
					     curCond->op == LE_OP ||
					     curCond->op == GT_OP ||
					     curCond->op == GE_OP) &&
					   !((curCond->value->type==IDENT_TYPE||
					      curCond->value->type==SYSVAR_TYPE)
					     && ignoreIdent))
					{
						break;
					}
				}
				curCond = curCond->next;
			}

			if (!curCond)
			{
				break;
			}
			switch(curCond->op)
			{
				case BETWEEN_OP:
					doRange = CAND_IDX_RANGE;
					break;

				case LE_OP:
					doRange = CAND_IDX_RANGE_LE;
					break;

				case LT_OP:
					doRange = CAND_IDX_RANGE_LT;
					break;

				case GE_OP:
					doRange = CAND_IDX_RANGE_GE;
					break;

				case GT_OP:
					doRange = CAND_IDX_RANGE_GT;
					break;
			}
			field++;
		}

		/*
		** Don't attempt to do a range match on a compound
		** index.  It can't be done.
		*/
		if (doRange)
		{
			if (curIndex->fields[1] != -1)
				curCond = NULL;
		}
		if (curCond)
		{
			if (identKey == 0)
			{
				validIndices[indexMatches] = index;
				numEntries = 
					idxGetNumEntries(&curIndex->handle);
				numKeys = idxGetNumKeys(&curIndex->handle);
				if (numKeys)
				{
					indexWeights[indexMatches] = 
						numEntries / numKeys;
				}
				else
				{
					indexWeights[indexMatches] = 0;
				}
				indexMatches++;
			}
			if (curIndex->unique)
			{
				haveUnique = 1;
				if (doRange)
					new->type = doRange;
				else
					new->type = CAND_IDX_ABS;
				new->index = index;
				new->ident = identKey;
				new->lastPos = NO_POS;
				new->length = curIndex->length;
				strcpy(new->idx_name, curIndex->name);
				candIndex = curIndex;
				if (query->explainOnly)
				{
					snprintf(packet,PKT_LEN,
					"\tFound unique %s index (%s)\n",
					new->type == CAND_IDX_RANGE?
					"range" : "absolute",
					curIndex->name);
					netWritePacket(query->clientSock);
				}
				break;
			}
			if (curIndex->length > new->length)
			{
				if (doRange)
					new->type = doRange;
				else
					new->type = CAND_IDX_ABS;
				new->index = index;
				new->ident = identKey;
				new->lastPos = NO_POS;
				new->length = curIndex->length;
				strcpy(new->idx_name, curIndex->name);
				if (query->explainOnly)
				{
					snprintf(packet,PKT_LEN,
					"\tFound %s index (%s) of length %d\n",
					new->type == CAND_IDX_RANGE?
					"range" : "absolute",
					curIndex->name, curIndex->length);
					netWritePacket(query->clientSock);
				}
				candIndex = curIndex;
			}
		}
		curIndex = curIndex->next;
		index++;
	}

        /*
        ** Can we do a union index?  Don't bother if we have a unique.
	** If we are going to do it then make sure it's done in the most
	** efficient order.
        */
#ifdef UNION_INDEX
        prevIdx.native = NULL;
        if (indexMatches > 1 && haveUnique == 0)
        {
                /*
                ** Yup we can.  Sort those puppies based on index weight
		** (i.e. num entries / num keys)
		*/
		while(1)
		{
			index = 1;
			done = 1;
			while(index < indexMatches)
			{
				if(indexWeights[index-1] > indexWeights[index])
				{
					tmpIndex = validIndices[index -1];
					validIndices[index - 1] =
						validIndices[index];
					validIndices[index] = tmpIndex;

					tmpWeight = indexWeights[index -1];
					indexWeights[index - 1] =
						indexWeights[index];
					indexWeights[index] = tmpWeight;
					done = 0;
				}
				index++;
			}
			if (done)
				break;
		}

	
		/*
		** Scan through the valid indices and
                ** create an AVL tree containing just the rowid's of
                ** the intersection of the indices (i.e. the union index)
                */
                index = 0;
                tmpIdx.native = prevIdx.native = NULL;
                while(index < indexMatches)
                {
                        curIndex = entry->indices;
                        count = 0;
                        while(count < validIndices[index])
                        {
                                curIndex = curIndex->next;
                                count++;
                        }
                        msqlDebug3(MOD_CANDIDATE, 
				"Union index on %s:%s with weight %d\n",
                                curIndex->table,curIndex->name,
				indexWeights[index]);
                        if (unionIdxCreateTmpIndex(&tmpIdx, &prevIdx, curIndex, 
				query->condHead) < 0)
			{
				tmpIdx.native = NULL;
			}
                        if (prevIdx.native)
                        {
                                unionIdxFree(&prevIdx);
                        }
                        bcopy(&tmpIdx,&prevIdx,sizeof(tmpIdx));
                        curIndex = curIndex->next;
                        index++;
                }
                new->unionIndex = &tmpIdx;
                new->type = CAND_UNION;
                msqlDebug1(MOD_CANDIDATE,
                        "setupCandidate() : Using UNION_INDEX for %s\n",
                        entry->table);
                return(new);
        }
#endif /* UNION_INDEX */


	/*
	** Setup the index stuff
	*/

	if (new->type == CAND_IDX_ABS || new->type == CAND_IDX_RANGE || 
	    new->type == CAND_IDX_RANGE_LE || new->type == CAND_IDX_RANGE_LT ||
	    new->type == CAND_IDX_RANGE_GE || new->type == CAND_IDX_RANGE_GT )
	{
		new->handle = candIndex->handle;
		new->buf = (char *)malloc(new->length + 1);
		new->maxBuf = (char *)malloc(new->length + 1);
		new->keyType = candIndex->keyType;


		/* Setup the key buffer */
		count = 0;
		cp = new->buf;
		bzero(new->buf,new->length + 1);
		cp1 = new->maxBuf;
		bzero(new->maxBuf,new->length + 1);

		while(candIndex->fields[count] != -1)
		{
			curCond = query->condHead;
			while(curCond)
			{
				if (curCond->subCond)
				{
					curCond = curCond->next;
					continue;
				}
				/*
				** This field can only hold the value we want
				** if the comparison OP and the index eval
				** type match (i.e. ABS must have EQ etc)
				*/
				if(curCond->fieldID==candIndex->fields[count]
				  && curCond->value->type != IDENT_TYPE 
				  && ((new->type == CAND_IDX_ABS  &&
					curCond->op == EQ_OP) ||
				      (new->type != CAND_IDX_ABS  &&
					curCond->op != EQ_OP)))
				{
					if (curCond->value->nullVal)
					{
                				snprintf(errMsg,MAX_ERR_MSG,
							NULL_COND_ERR, 
							curCond->name);
               					return(NULL);
					}
					extractFieldValue(cp,curCond);
					extractMaxFieldValue(cp1,curCond);
					cp += curCond->length;
					cp1 += curCond->length;
					break;
				}
				curCond = curCond->next;
			}
			count++;
		}
		msqlDebug2(MOD_ACCESS, 
			"setupCandidate() : Using IDX %d for %s\n",
			new->index, entry->table);
		return(new);
	}
	if (query->explainOnly)
	{
		strcpy(packet,"\tDidn't find anything of use. Using SEQ\n");
		netWritePacket(query->clientSock);
	}
	msqlDebug1(MOD_ACCESS, 
		"setupCandidate() : Using SEQ for %s\n",
		entry->table);
	return(new);
}


void craResetCandidate(cand, deleteFlag)
	mCand_t	*cand;
	int	deleteFlag;
{
	/*
	** If it's a SEQ search candidate then just start at the top
	** again.  We need to reset it in this way when the candidate
	** is the inner loop of a join.  If this is from a delete only
	** reset index based candidates
	*/
	if (cand->type == CAND_SEQ && deleteFlag == 0)
		cand->nextPos = 0;
	else
		cand->lastPos = NO_POS;
}




u_int craGetCandidate(entry, cand)
	cache_t	*entry;
	mCand_t	*cand;
{
	int	length,
		rangeEnd,
		res = 0;
	u_int	pos;
	idx_nod	node;
	char	*tmpKey;

	bzero(&node, sizeof(idx_nod));
	switch(cand->type)
	{
	    case CAND_SEQ:
		cand->nextPos++;
		if (cand->nextPos > entry->sblk->numRows)
		{
			msqlDebug1(MOD_ACCESS, 
				"getCandidate() : SEQ on %s => NO_POS\n",
				entry->table);
			return(NO_POS);
		}
		else
		{
			msqlDebug2(MOD_ACCESS, 
				"getCandidate() : SEQ on %s => %d\n",
				entry->table, cand->nextPos -1);
			return(cand->nextPos -1);
		}
		break;


	    case CAND_IDX_ABS:
		msqlDebug2(MOD_ACCESS, 
			"getCandidate() : using IDX '%s' on %s\n",
			cand->idx_name, entry->table);
		msqlDebug3(MOD_ACCESS, 
			"getCandidate() : IDX key on %s = '%s','%d'\n",
			entry->table, cand->buf, (int) *(int*)cand->buf);
		length = cand->length;
		if (cand->lastPos == NO_POS)
		{
			res = idxLookup(&cand->handle, cand->buf,
				cand->length, IDX_EXACT, &node);
			idxSetCursor(&cand->handle, &cand->cursor);
		}
		else
		{
			res = idxGetNext(&cand->handle, &cand->cursor,&node);
		}
		if (res != IDX_OK)
		{
			idxCloseCursor(&cand->handle, &cand->cursor);
			msqlDebug1(MOD_ACCESS, 
				"getCandidate() : IDX on %s => NO_POS\n",
				entry->table);
			return(NO_POS);
		}
		if (cand->keyType == IDX_CHAR)
		{
			/* XXX */
			/*if (strcmp(node.key, cand->buf) != 0)*/
			tmpKey = (char*)((avl_nod*)node.native)+40;
			if (strcmp(tmpKey, cand->buf) != 0)
			{
				msqlDebug1(MOD_ACCESS, 
				    "getCandidate() : IDX on %s => NO_POS\n",
				    entry->table);
				return(NO_POS);
			}
		}
		else
		{
			if (bcmp(node.key, cand->buf, length) != 0)
			{
				msqlDebug1(MOD_ACCESS, 
				    "getCandidate() : IDX on %s => NO_POS\n",
				    entry->table);
				return(NO_POS);
			}
		}
		pos = node.data;
		if (cand->lastPos == NO_POS)
		{
			cand->lastPos = pos;
		}

		msqlDebug2(MOD_ACCESS, 
			"getCandidate() : IDX on %s => %d\n", 
			entry->table, pos);
		return(pos);


            case CAND_IDX_RANGE:
            case CAND_IDX_RANGE_LE:
            case CAND_IDX_RANGE_LT:
            case CAND_IDX_RANGE_GE:
            case CAND_IDX_RANGE_GT:
                msqlDebug2(MOD_ACCESS,
                        "getCandidate() : using RANGE IDX '%s' on %s\n",
                        cand->idx_name, entry->table);
                msqlDebug3(MOD_ACCESS,
                        "getCandidate() : IDX key on %s = '%s','%d'\n",
                        entry->table, cand->buf, (int) *(int*)cand->buf);
                length = cand->length;
                if (cand->lastPos == NO_POS)
                {
		    switch(cand->type)
		    {
			case CAND_IDX_RANGE:
			case CAND_IDX_RANGE_GE:
			case CAND_IDX_RANGE_GT:
				res = idxLookup(&cand->handle, cand->buf,
					cand->length, IDX_CLOSEST, &node);
				idxSetCursor(&cand->handle, &cand->cursor);
				break;

			case CAND_IDX_RANGE_LE:
			case CAND_IDX_RANGE_LT:
				res = idxGetFirst(&cand->handle,&node);
				idxSetCursor(&cand->handle, &cand->cursor);
				break;
		    }
                }
                else
                {
			res = idxGetNext(&cand->handle, &cand->cursor,&node);
		}
		if (res != IDX_OK)
		{
			idxCloseCursor(&cand->handle, &cand->cursor);
                        msqlDebug1(MOD_ACCESS,
                                "getCandidate() : RANGE IDX on %s => NO_POS\n",
                                entry->table);
                        return(NO_POS);
                }
                rangeEnd = 0;
		if (cand->type != CAND_IDX_RANGE_GT && 
		    cand->type != CAND_IDX_RANGE_GE)
		{
                	switch(cand->keyType)
                	{
                    	case IDX_CHAR:
                        	if (strcmp(node.key, cand->maxBuf) > 0)
                                	rangeEnd = 1;
                        	break;

                    	case IDX_INT8:
                        	if (idxInt8Compare(node.key, cand->maxBuf) > 0)
                                	rangeEnd = 1;
                        	break;

                    	case IDX_INT16:
                        	if (idxInt16Compare(node.key, cand->maxBuf) > 0)
                                	rangeEnd = 1;
                        	break;

                    	case IDX_INT32:
                        	if (idxInt32Compare(node.key, cand->maxBuf) > 0)
                                	rangeEnd = 1;
                        	break;

                    	case IDX_INT64:
                        	if (idxInt64Compare(node.key, cand->maxBuf) > 0)
                                	rangeEnd = 1;
                        	break;

                    	case IDX_UINT8:
                        	if (idxUInt8Compare(node.key,cand->maxBuf) > 0)
                               		rangeEnd = 1;
                        	break;

                    	case IDX_UINT16:
                        	if (idxUInt16Compare(node.key,cand->maxBuf) > 0)
                               		rangeEnd = 1;
                        	break;

                    	case IDX_UINT32:
                        	if (idxUInt32Compare(node.key,cand->maxBuf) > 0)
                               		rangeEnd = 1;
                        	break;

                    	case IDX_UINT64:
                        	if (idxUInt64Compare(node.key,cand->maxBuf) > 0)
                               		rangeEnd = 1;
                        	break;

                    	case IDX_REAL:
                        	if (idxRealCompare(node.key, cand->maxBuf) > 0)
                                	rangeEnd = 1;
				break;

                    	default:
                        	if(idxByteCompare(node.key,cand->maxBuf,
					length)>0)
				{
                                	rangeEnd = 1;
				}
			}
                }
                if (rangeEnd)
                {
			idxCloseCursor(&cand->handle, &cand->cursor);
                        msqlDebug1(MOD_ACCESS,
                                "getCandidate() : RANGE IDX on %s => NO_POS\n",
                                entry->table);
                        return(NO_POS);
                }
                pos = node.data;
                if (cand->lastPos == NO_POS)
                {
                        cand->lastPos = pos;
                }

                msqlDebug2(MOD_ACCESS,
			"getCandidate() : RANGE IDX on %s => %d\n",
                        entry->table, pos);
                return(pos);

            case CAND_UNION:
                if (cand->lastPos == NO_POS)
                {
                        cand->lastPos = 0;
                }
                cand->lastPos++;
                return(unionIdxGet(cand->unionIndex,cand->lastPos-1));




	    case CAND_ROWID:
		msqlDebug2(MOD_ACCESS, 
			"getCandidate() : using ROW ID '%d' on %s\n",
			cand->rowID, entry->table);
		if (cand->lastPos == NO_POS)
		{
			if (entry->sblk->numRows < cand->rowID)
			{
				cand->rowID = 0;
				return(NO_POS);
			}
			cand->lastPos = cand->rowID;
			return(cand->rowID);
		}	
		else
		{
			return(NO_POS);
		}
	}
	return(NO_POS);
}

