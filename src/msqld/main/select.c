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
** $Id: select.c,v 1.22 2011/11/22 11:47:18 bambi Exp $
**
*/


/*
** Module	: main : select
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
#if HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/cra/cra.h>
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/table.h>
#include <msqld/main/net.h>
#include <msqld/main/util.h>
#include <msqld/main/sysvar.h>
#include <msqld/main/compare.h>
#include <msqld/main/select.h>
#include <msqld/main/parse.h>
#include <msqld/main/funct.h>
#include <msqld/main/distinct.h>
#include <msqld/main/sort.h>
#include <msqld/main/memory.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

typedef struct {
	mCond_t	*head,
		*t1,
		*t2;
} join_t;




extern	char	*packet,
		errMsg[];




static void mergeRows(result,row,table1,t1Row,table2,t2Row,query)
	cache_t		*result;
	row_t		*row,
			*t1Row,
			*t2Row;
	cache_t		*table1,
			*table2;
	mQuery_t	*query;
{
	mField_t	*curField;
	char		*cp;
	int		sysvarRow;

	/*
	** Drag the 2 rows over
	*/
	bcopy(t1Row->data,row->data,table1->rowDataLen);
	bcopy(t2Row->data,row->data+table1->rowDataLen,
		table2->rowDataLen);
	cp = (char *)row->data + table1->rowDataLen + table2->rowDataLen;

	/*
	** If there are any system variables in the join result we
	** have to manaully add them.  tableCreateTmpTable() forces them
	** to be at the end of the table row so we just need to append
	** to correct values to the newly created row.
	*/
	curField = result->def;
	while(curField)
	{
		if (*curField->name == '_')
		{
			sysvarRow = 0;
			if (strcmp(curField->table,table1->table)==0)
				sysvarRow = 1;
			if (strcmp(curField->table,table2->table)==0)
				sysvarRow = 2;
			if (sysvarRow == 0)
			{
				curField = curField->next;
				continue;
			}
			if (sysvarRow == 1)
				sysvarGetVariable(table1,t1Row,curField,query);
			else
				sysvarGetVariable(table2,t2Row,curField,query);
			*cp = 1;
			cp++;
			parseCopyValue(cp,curField->value,curField->type,
				curField->length,1);
			cp += curField->length;
		}
		curField = curField->next;
	}
}




static int checkForPartialMatch(conds)
	mCond_t		*conds;
{
	mCond_t		*curCond;
	int		res;

	if (!conds)
		return(0);	
	res = 1;
	curCond = conds;
	while(curCond)
	{
		if (curCond->value->type == IDENT_TYPE)
		{
			return(0);
		}
		if (curCond->bool == OR_BOOL)
		{
			return(0);
		}
		curCond=curCond->next;
	}
	return(res);
}





static mField_t *buildIdentList(outer, inner, conds, flist, status)
	cache_t		*outer,
			*inner;
	mCond_t		*conds;
	int		*flist;
	int		*status;
{
	mField_t	*identFields,
			*tail,
			*curField;
	mCond_t		*curCond;
	int 		*curOffset;
	cache_t		*curTable;

	*status = 0;
	tail = identFields = NULL;
	curCond = conds;
	curOffset = flist;
	while(curCond)
	{

		/*
		** Find an IDENT cond and setup the field struct
		*/
		if (curCond->value->type != IDENT_TYPE)
		{
			curCond = curCond->next;
			continue;
		}
		curField = memMallocField();
		strcpy(curField->name, curCond->name);
		strcpy(curField->table, curCond->table);
		curField->dataLength = curField->length = 0;
		if (tail)
		{
			tail->next = curField;
		}
		else
		{
			identFields = curField;
		}
		tail = curField;
		curField->next = NULL;

		/*
		** Fill in the blanks
		*/
		curTable = outer;
		curField = outer->def;
		*curOffset = 0;
		while(curField)
		{
			if (strcmp(curField->name, 
				curCond->value->val.identVal->seg2)!=0 ||
			    strcmp(curField->table, 
				curCond->value->val.identVal->seg1)!=0)
			{
				*curOffset += curField->dataLength + 1;
				curField = curField->next;
				if (curField == NULL && curTable == outer)
				{
					curTable = inner;
					curField = inner->def;
				}
				continue;
			}
			tail->dataLength = curField->dataLength;
			tail->length = curField->length;
			tail->type = curField->type;
			tail->offset = *curOffset;
			tail->fieldID = curCond->fieldID;
			curOffset++;
			break;
		}
		if (tail->dataLength == 0)
		{
			/* Didn't find the ident var */
			snprintf(errMsg, MAX_ERR_MSG, 
				"Unknown field '%s.%s' in condition",
                                curCond->value->val.identVal->seg1,
				curCond->value->val.identVal->seg2);
			*status = -1;
		}
		curCond = curCond->next;
	}
	*curOffset = -1;
	return(identFields);
}


static void _freeIdentFieldList(fields)
	mField_t	*fields;
{
	mField_t	*curField,
			*prevField;

	curField = fields;
	while(curField)
	{
		prevField = curField;
		curField = curField->next;
		if (prevField->value)
		{
			parseFreeValue(prevField->value);	
			prevField->value = NULL;
		}
		memFreeField(prevField);
	}
}



static int invertOp(op)
	int	op;
{
	switch(op)
	{
		case GT_OP:
			return(LT_OP);
		case LT_OP:
			return(GT_OP);
		case GE_OP:
			return(LE_OP);
		case LE_OP:
			return(GE_OP);
	}
	return(op);
}



static int swapIdentConds(cond1, cond2, query)
	mCond_t		**cond1,
			**cond2;
	mQuery_t	*query;
{
	mCond_t		*tail,
			*cur,
			*tmp,
			*prev;
	mIdent_t	*newIdent,
			*curIdent;
	int		count;

	tail = prev = NULL;
	count = 0;
	cur = *cond2;
	while (cur)
	{
		tail = cur;
		cur = cur->next;
	}
	cur = *cond1;
	while (cur)
	{
		if (cur->value->type == IDENT_TYPE)
		{
			/*
			** ensure we don't have this already
			*/
			tmp = *cond2;
			while(tmp)
			{
				if (tmp->value->type != IDENT_TYPE)
				{
					tmp = tmp->next;
					continue;
				}
				curIdent = cur->value->val.identVal;
				if(*tmp->table != *curIdent->seg1 ||
				   *tmp->name != *curIdent->seg2)
				{
					tmp=tmp->next;
					continue;
				}
				if(strcmp(tmp->table,curIdent->seg1)!=0 ||
				   strcmp(tmp->name,curIdent->seg2)!=0)
				{
					tmp = tmp->next;
					continue;
				}
				curIdent = tmp->value->val.identVal;

				if(strcmp(cur->table,curIdent->seg1)!=0 ||
				   strcmp(cur->name,curIdent->seg2)!=0)
				{
					tmp = tmp->next;
					continue;
				}
				break;
			}
			if (tmp)
			{
				/* Dodge it.  It's already there! */
				cur = cur->next;
				continue;
			}
			count++;
			newIdent = (mIdent_t *)parseCreateIdent(cur->table, 
				cur->name, query);
			strcpy(cur->table,cur->value->val.identVal->seg1);
			strcpy(cur->name,cur->value->val.identVal->seg2);
			memFreeIdent(cur->value->val.identVal);
			cur->value->val.identVal = newIdent;
			cur->op = invertOp(cur->op);
			if (prev)
			{
				prev->next = cur->next;
			}
			else
			{
				*cond1 = cur->next;
			}
			if (tail)
			{
				tail->next = cur;
			}
			else
			{
				*cond2 = cur;
			}
			cur->next = NULL;
			tail = cur;
		}
		cur = cur->next;
	}
	return(count);
}


static mCond_t *condDup(cond)
	mCond_t	*cond;
{
	mCond_t	*new;

	new = memMallocCondition();
	bcopy(cond,new,sizeof(mCond_t));
	new->value = memMallocValue();
	bcopy(cond->value,new->value,sizeof(mVal_t));
	if (cond->maxValue)
	{
		new->maxValue = memMallocValue();
		bcopy(cond->maxValue,new->maxValue,sizeof(mVal_t));
	}
	new->next = NULL;
	switch(new->value->type)
	{
		case CHAR_TYPE:
			new->value->val.charVal = (u_char *)strdup(
				(char*)cond->value->val.charVal);
			if(cond->maxValue)
			{
				new->maxValue->val.charVal = (u_char *)strdup(
					(char*)cond->maxValue->val.charVal);
			}
			break;
		case BYTE_TYPE:
			/* Do we need to dup this value? */
			break;
		case IDENT_TYPE:
			new->value->val.identVal = memMallocIdent();
			bcopy(cond->value->val.identVal,
				new->value->val.identVal, sizeof(mIdent_t));
			break;
	}
	return(new);
}


static int checkFieldTable(fields,query)
	mField_t	*fields;
	mQuery_t	*query;
{
	mField_t	*curField;
	mTable_t	*curTable;

	curField = fields;
	while(curField)
	{
		if (curField->function)
		{
			if (checkFieldTable(curField->function->paramHead,
				query) < 0)
			{
				return(-1);
			}
			curField = curField->next;
			continue;
		}
		if (strcmp(curField->name, "*") == 0)
		{
			curField = curField->next;
			continue;
		}
		curTable = query->tableHead;
		while(curTable)
		{
			if ( *(curField->table) != *(curTable->name))
			{
				curTable = curTable->next;
				continue;
			}
			if (strcmp(curField->table,curTable->name) != 0)
			{
				curTable = curTable->next;
				continue;
			}
			break;
		}
		if (!curTable)
		{
			snprintf(errMsg,MAX_ERR_MSG, UNSELECT_ERROR,
				curField->table);
			debugTrace(TRACE_OUT,"checkFieldTable()");
			return(-1);
		}
		curField = curField->next;
	}
	return(0);
}


static void setJoinTableDone(tables,name)
	mTable_t	*tables;
	char	*name;
{
	mTable_t	*cur;

	cur = tables;
	while(cur)
	{
		if (strcmp(cur->name, name) == 0)
		{
			cur->done = 1;
			return;
		}
		cur = cur->next;
	}
	return;
}

static int checkJoinTableDone(tables,name)
	mTable_t	*tables;
	char	*name;
{
	mTable_t	*cur;

	cur = tables;
	while(cur)
	{
		if (strcmp(cur->name, name) == 0)
		{
			return(cur->done);
		}
		cur = cur->next;
	}
	return(0);
}


static join_t *setupJoinConditions(table1, table2, conds, tables)
	cache_t	*table1,
		*table2;
	mCond_t	*conds;
	mTable_t	*tables;
{
	mCond_t	*head,
		*tail,
		*t1,
		*t2,
		*cur,
		*newCond;
	char	buf[NAME_LEN + 1];
	static  join_t new;


	/*
	** Build a list of all conditions related to the join as a whole
	*/
	head = tail = NULL;
	cur = conds;
	while(cur)
	{
		if (cur->subCond)
		{
			cur = cur->next;
			continue;
		}
		if (strcmp(table1->table,cur->table) == 0 ||
			strcmp(table2->table,cur->table) == 0)
		{
			if (cur->value->type == IDENT_TYPE)
			{
				if (checkJoinTableDone(tables,
					cur->value->val.identVal->seg1) == 0)
				{
					cur = cur->next;
					continue;
				}
			}
			newCond = condDup(cur);
			if (!head)
			{
				tail = head = newCond;
			}
			else
			{
				tail->next = newCond;
				tail = newCond;
			}
			cur = cur->next;
			continue;
		}

		if (cur->value->type == IDENT_TYPE &&
		    (strcmp(table1->table,cur->value->val.identVal->seg1)==0||
		     strcmp(table2->table,cur->value->val.identVal->seg1)==0))
		{
			if (checkJoinTableDone(tables,cur->table) == 0)
			{
				cur = cur->next;
				continue;
			}
			newCond = condDup(cur);
			if (!head)
			{
				tail = head = newCond;
			}
			else
			{
				tail->next = newCond;
				tail = newCond;
			}
			cur = cur->next;
			continue;
		}
		cur = cur->next;
	}    


	/*
	** Build a list for T1 if it isn't a result table.
	*/
	t1 = tail = NULL;
	if (table1->result == 0)
	{
		cur = head;
		while(cur)
		{
			if (strcmp(table1->table, cur->table) == 0)
			{
				newCond = condDup(cur);
				if (!t1)
				{
					t1 = tail = newCond;
				}
				else
				{
					tail->next = newCond;
					tail = newCond;
				}
			}
			else
			if (cur->value->type == IDENT_TYPE &&
		  	strcmp(table1->table,cur->value->val.identVal->seg1)==0)
			{
				newCond = condDup(cur);
				if (!t1)
				{
					t1 = tail = newCond;
				}
				else
				{
					tail->next = newCond;
					tail = newCond;
				}
				strcpy(buf, newCond->table);
				strcpy(newCond->table, 
					newCond->value->val.identVal->seg1);
				strcpy(newCond->value->val.identVal->seg1, buf);
	
				strcpy(buf, newCond->name);
				strcpy(newCond->name, 
					newCond->value->val.identVal->seg2);
				strcpy(newCond->value->val.identVal->seg2, buf);
			}
			cur = cur->next;
		}
	}

	/*
	** Build a list for T2 if it isn't a result table
	*/
	t2 = tail = NULL;
	if (table2->result == 0)
	{
		cur = head;
		while(cur)
		{
			if (strcmp(table2->table, cur->table) == 0)
			{
				newCond = condDup(cur);
				if (!t2)
				{
					t2 = tail = newCond;
				}
				else
				{
					tail->next = newCond;
					tail = newCond;
				}
			}
			else
			if (cur->value->type == IDENT_TYPE &&
		  	strcmp(table2->table,cur->value->val.identVal->seg1)==0)
			{
				newCond = condDup(cur);
				if (!t2)
				{
					t2 = tail = newCond;
				}
				else
				{
					tail->next = newCond;
					tail = newCond;
				}
				strcpy(buf, newCond->table);
				strcpy(newCond->table, 
					newCond->value->val.identVal->seg1);
				strcpy(newCond->value->val.identVal->seg1, buf);
	
				strcpy(buf, newCond->name);
				strcpy(newCond->name, 
					newCond->value->val.identVal->seg2);
				strcpy(newCond->value->val.identVal->seg2, buf);
			}
			cur = cur->next;
		}
	}

	new.head = head;
	new.t1 = t1;
	new.t2 = t2;
	return(&new);
}




static cache_t *joinTables(server, table1,table2, query)
	msqld		*server;
	cache_t		*table1,
			*table2;
	mQuery_t	*query;
{
	cache_t		*tmpTable = NULL,
			*outer,
			*inner;
	int		outerRowNum, 	innerRowNum,
			identFList[MAX_FIELDS],
			t1Partial,	t2Partial,
			doPartial,	res,
			status;
	mCond_t		*newCondHead, 
			*t1CondHead, 	*t2CondHead, 	
			*outerConds, 	*innerConds,
			*newCond, 	*curCond;
	mField_t	*curField, 	*tmpField,
			*identFields;
	row_t		outerRow, 	innerRow,
			*row;
	mCand_t		*outerCand, 	*innerCand,
			*t1Cand,	*t2Cand;
	mQuery_t	t1Query,	t2Query;
	join_t		*joinInfo;



        debugTrace(TRACE_IN,"joinTables()");

	if (query->explainOnly)
	{
		snprintf(packet,PKT_LEN,"Joining %s and %s.\n",
			table1->table, table2->table);
		netWritePacket(query->clientSock);
	}

	/*
	** Work out the conditions that apply to the join as a whole and
	** the two tables individually
	if (utilSetupConds(table1,query->condHead) < 0)
	{
		return(NULL);
	}
	if (utilSetupConds(table2,query->condHead) < 0)
	{
		return(NULL);
	}
	*/
	joinInfo = setupJoinConditions(table1, table2, query->condHead, 
		query->tableHead);
	newCondHead = joinInfo->head;
	t1CondHead = joinInfo->t1;
	t2CondHead = joinInfo->t2;
	if(utilSetupConds(table1,t1CondHead) < 0)
	{
		return(NULL);
	}
	if(utilSetupConds(table2,t2CondHead) < 0)
	{
		return(NULL);
	}

	/*
	** See if we can do partial match optimisation on either table
	*/
	t1Partial = checkForPartialMatch(t1CondHead);
	t2Partial = checkForPartialMatch(t2CondHead);


	/*
	** What about index based lookups?  Look for a straight literal
	** index first and then an IDENT based key if we don't have
	** anything.  If we end up with 2 IDENT based indices then we
	** drop the first and make it sequential.
	*/
	bzero(&t1Query, sizeof(t1Query));
	bzero(&t2Query, sizeof(t2Query));

	if (query->explainOnly)
	{
		sprintf(packet,"Doing candidate setup for table 1 (%s)\n",
			table1->table);
		netWritePacket(query->clientSock);
		strcpy(packet,"Check for literal based index lookups\n");
		netWritePacket(query->clientSock);
	}
	t1Query.condHead = t1CondHead;
	t1Query.explainOnly = query->explainOnly;
	t1Cand = craSetupCandidate(table1, &t1Query, IGNORE_IDENT);
	if (!t1Cand)
		return(NULL);
	if (t1Cand->type == CAND_SEQ)
	{
		if (query->explainOnly)
		{
			strcpy(packet,"Current access method is sequential.");
			strcat(packet,"  Checking for ident index lookup.\n");
			netWritePacket(query->clientSock);
		}
		craFreeCandidate(t1Cand);
		t1Cand = craSetupCandidate(table1, &t1Query, KEEP_IDENT);
		if (query->explainOnly)
		{
			if (t1Cand->type == CAND_SEQ)
				strcpy(packet,"No.  Still using sequential.\n");
			else
				strcpy(packet,
					"Using ident based index lookup.\n");
			netWritePacket(query->clientSock);
		}
	}
	if (!t1Cand)
		return(NULL);

	if (query->explainOnly)
	{
		sprintf(packet,"Doing candidate setup for table 2 (%s)\n",
			table2->table);
		netWritePacket(query->clientSock);
		strcpy(packet,"Check for literal based index lookups\n");
		netWritePacket(query->clientSock);
	}
	t2Query.condHead = t2CondHead;
	t2Query.explainOnly = query->explainOnly;
	t2Cand = craSetupCandidate(table2, &t2Query, IGNORE_IDENT);
	if (!t2Cand)
	{
		craFreeCandidate(t1Cand);
		return(NULL);
	}
	if (t2Cand->type == CAND_SEQ)
	{
		if (query->explainOnly)
		{
			strcpy(packet,"Current access method is sequential.");
			strcat(packet,"  Checking for ident index lookup.\n");
			netWritePacket(query->clientSock);
		}
		craFreeCandidate(t2Cand);
		t2Cand=craSetupCandidate(table2,&t2Query, KEEP_IDENT);
		if (query->explainOnly)
		{
			if (t2Cand->type == CAND_SEQ)
				strcpy(packet,"No.  Still using sequential.\n");
			else
				strcpy(packet,
					"Using ident based index lookup.\n");
			netWritePacket(query->clientSock);
		}
	}
	if (!t2Cand)
	{
		craFreeCandidate(t1Cand);
		return(NULL);
	}

	if (t1Cand->type > CAND_SEQ && t2Cand->type > CAND_SEQ)
	{
		if (t1Cand->ident && t2Cand->ident)
		{
			t1Cand->type = CAND_SEQ;
			t1Cand->lastPos = NO_POS;
		}
	}

	/*
	** OK, we know all there is to know.  Now what can we do with it?
	*/
	if (t1Cand->type > CAND_SEQ)
	{
		/* We've got an index on T1 */
		if (t1Cand->ident == 0)
		{
			if (query->explainOnly)
			{
				strcpy(packet,"Checking ident order on table 2 and rerunning candidate setup.\n");
				netWritePacket(query->clientSock);
			}
			if (swapIdentConds(&t1CondHead, &t2CondHead, query) > 0)
			{
				utilSetupConds(table1,t1CondHead);
				utilSetupConds(table2,t2CondHead);
			}
			outer = table1;
			outerConds = t1CondHead;
			outerCand = t1Cand;
			inner = table2;
			innerConds = t2CondHead;
			craFreeCandidate(t2Cand);
			t2Cand=craSetupCandidate(table2, &t2Query, KEEP_IDENT);
			innerCand = t2Cand;
			doPartial = 0;
		}
		else
		{
			if (query->explainOnly)
			{
				strcpy(packet,"Checking ident order on table 2 and rerunning candidate setup.\n");
				netWritePacket(query->clientSock);
			}
			if (swapIdentConds(&t2CondHead, &t1CondHead,query) > 0)
			{
				utilSetupConds(table1,t1CondHead);
				utilSetupConds(table2,t2CondHead);
			}
			outer = table2;
			outerConds = t2CondHead;
			outerCand = t2Cand;
			inner = table1;
			innerConds = t1CondHead;
			craFreeCandidate(t1Cand);
			t1Cand=craSetupCandidate(table1, &t1Query, KEEP_IDENT);
			innerCand = t1Cand;
			doPartial = 0;
		}
	} else 
	if (t2Cand->type > CAND_SEQ)
	{
		/* We've got an index on T2 */
		if (t2Cand->ident == 0)
		{
			if (swapIdentConds(&t2CondHead, &t1CondHead,query) > 0)
			{
				utilSetupConds(table1,t1CondHead);
				utilSetupConds(table2,t2CondHead);
			}
			outer = table2;
			outerConds = t2CondHead;
			outerCand = t2Cand;
			inner = table1;
			innerConds = t1CondHead;
			craFreeCandidate(t1Cand);
			t1Cand=craSetupCandidate(table1, &t1Query, KEEP_IDENT);
			innerCand = t1Cand;
			doPartial = 0;
		}
		else
		{
			if (swapIdentConds(&t1CondHead, &t2CondHead,query) > 0)
			{
				utilSetupConds(table1,t1CondHead);
				utilSetupConds(table2,t2CondHead);
			}
			outer = table1;
			outerConds = t1CondHead;
			outerCand = t1Cand;
			inner = table2;
			innerConds = t2CondHead;
			craFreeCandidate(t2Cand);
			t2Cand=craSetupCandidate(table2, &t2Query, KEEP_IDENT);
			innerCand = t2Cand;
			doPartial = 0;
		}
	} else 
	if (t1Partial)
	{
		/* We've got a partial match on T1 */

		outer = table1;
		outerConds = t1CondHead;
		outerCand = t1Cand;
		inner = table2;
		innerConds = t2CondHead;
		innerCand = t2Cand;
		doPartial = 1;
	} else 
	if (t2Partial)
	{
		/* We've got a partial match on T2 */

		outer = table2;
		outerConds = t2CondHead;
		outerCand = t2Cand;
		inner = table1;
		innerConds = t1CondHead;
		innerCand = t1Cand;
		doPartial = 1;
	}
	else
	{
		/* We've got nothing to speed this up */

		outer = table1;
		outerConds = t1CondHead;
		outerCand = t1Cand;

		inner = table2;
		innerConds = t2CondHead;
		innerCand = t2Cand;
		doPartial = 0;
	} 
	
	
	/*
	** Now that we know the inner table, we need to create a field
	** list containing the fields from the outer table that are used
	** as IDENT_TYPE conditions for the inner table.  Without this
	** we can't do candidate based lookups for the inner table.
	*/

	identFields = buildIdentList(outer,inner, innerConds, identFList,
		&status);
	if (status < 0)
	{
		_freeIdentFieldList(identFields);
		craFreeCandidate(innerCand);
		craFreeCandidate(outerCand);
        	debugTrace(TRACE_OUT,"joinTables()");
		return(NULL);
	}


	/*
	** Create a table definition for the join result.  We can't do
	** this earlier as we must know which is the inner and outer table
	*/
	tmpTable = tableCreateTmpTable(server, NULL, NULL, outer,inner,
		query->fieldHead,ALL_FIELDS);
	if (!tmpTable)
	{
		craFreeCandidate(innerCand);
		craFreeCandidate(outerCand);
        	debugTrace(TRACE_OUT,"joinTables()");
		return(NULL);
	}
	(void)snprintf(tmpTable->resInfo,4 * (NAME_LEN + 1),
		"'%s (%s+%s)'",tmpTable->table, table1->table, table2->table);


	/*
	** Do the join
	*/

	row = &(tmpTable->row);
	if (utilSetupConds(tmpTable,newCondHead) < 0)
	{
		craFreeCandidate(innerCand);
		craFreeCandidate(outerCand);
		tableFreeTmpTable(server,tmpTable);
        	debugTrace(TRACE_OUT,"joinTables()");
		return(NULL);
	}

	if (query->explainOnly)
	{
		strcpy(packet,"Starting join\n");
		netWritePacket(query->clientSock);
	}
	outerRowNum = craGetCandidate(outer, outerCand);
	while(outerRowNum != NO_POS)
	{
		tableReadRow(outer,&outerRow,outerRowNum);
		/*
		** Dodge holes 
		*/
		if (!outerRow.header->active)
		{
			outerRowNum = craGetCandidate(outer, outerCand);
			continue;
		}

		/* 
		** Partial match optimisation ??
		*/
		if(doPartial)
		{
			if(compareMatchRow(outer,&outerRow,outerConds,query)!=1)
			{
				outerRowNum = craGetCandidate(outer, outerCand);
				continue;
			}
		}


		/*
		** Go ahead and join this row with the inner table
		*/

		tableExtractValues(outer,&outerRow,identFields,identFList,
			query);
		if (craSetCandidateValues(inner, innerCand, identFields, 
			innerConds, &outerRow, query) < 0)
		{
			tableFreeTmpTable(server, tmpTable);
			craFreeCandidate(innerCand);
			craFreeCandidate(outerCand);
			return(NULL);
		}
		craResetCandidate(innerCand, 0);
		innerRowNum = craGetCandidate(inner, innerCand);
		while(innerRowNum != NO_POS)
		{
			tableReadRow(inner,&innerRow,innerRowNum);
			if (!innerRow.header->active)
			{
				innerRowNum = craGetCandidate(inner, innerCand);
				continue;
			}
			row->header->active = 1;
			mergeRows(tmpTable,row,outer,&outerRow,inner,&innerRow,
				query);
			res=compareMatchRow(tmpTable,row,newCondHead,query);

			if (res < 0)
			{
				craFreeCandidate(innerCand);
				craFreeCandidate(outerCand);
				tableFreeTmpTable(server, tmpTable);
        			debugTrace(TRACE_OUT,"joinTables()");
				return(NULL);
			}
			if (res == 1)
			{
				if(tableWriteRow(tmpTable,NULL,NO_POS,query)<0)
				{
					tableFreeTmpTable(server, tmpTable);
					craFreeCandidate(outerCand);
					craFreeCandidate(innerCand);
        				debugTrace(TRACE_OUT,"joinTables()");
					return(NULL);
				}
			}
			innerRowNum = craGetCandidate(inner, innerCand);
		}
		outerRowNum = craGetCandidate(outer, outerCand);
	}
	craFreeCandidate(innerCand);
	craFreeCandidate(outerCand);

	/*
	** Free up the space allocated to the new condition list.
	** We don't need to free the value structs as we just copied
	** the pointers to them.  They'll be freed during msqlClen();
	*/
	curCond = newCondHead;
	while(curCond)
	{
                newCond = curCond;
                curCond = curCond->next;
		if (newCond->value)
		{
			parseFreeValue(newCond->value);
			newCond->value = NULL;
		}
		if (newCond->maxValue)
		{
			parseFreeValue(newCond->maxValue);
			newCond->maxValue = NULL;
		}
                memFreeCondition(newCond);
	}
	curCond = t1CondHead;
	while(curCond)
	{
                newCond = curCond;
                curCond = curCond->next;
		if (newCond->value)
		{
			parseFreeValue(newCond->value);
			newCond->value = NULL;
		}
		if (newCond->maxValue)
		{
			parseFreeValue(newCond->maxValue);
			newCond->maxValue = NULL;
		}
                memFreeCondition(newCond);
	}
	curCond = t2CondHead;
	while(curCond)
	{
                newCond = curCond;
                curCond = curCond->next;
		if (newCond->value)
		{
			parseFreeValue(newCond->value);
			newCond->value = NULL;
		}
		if (newCond->maxValue)
		{
			parseFreeValue(newCond->maxValue);
			newCond->maxValue = NULL;
		}
                memFreeCondition(newCond);
	}

	if (identFields)
	{
		curField = identFields;
		while(curField)
		{
			tmpField = curField;
			curField = curField->next;
			if (tmpField->value)
			{
				parseFreeValue(tmpField->value);	
				tmpField->value = NULL;
			}
			memFreeField(tmpField);
		}
	}

	if (query->explainOnly)
	{
		sprintf(packet,"Join complete - result in %s\n",
			tmpTable->table);
		netWritePacket(query->clientSock);
	}
	debugTrace(TRACE_OUT,"joinTables()");
	return(tmpTable);
}



int doSelect(cacheEntry, query, dest, checkConds, tmpTable)
	cache_t		*cacheEntry;
	mQuery_t	*query;
	int		dest,
			checkConds;
	cache_t		*tmpTable;
{
	int	flist[MAX_FIELDS],
		tmpFlist[MAX_FIELDS],
		rowLen,
		rowNum,
		numFields,
		abortSelect,
		count,
		res;
	char	outBuf[100],
		outBuf2[100],
		*fieldName;
	row_t	row;
	mCand_t	*candidate;
	mField_t *identFields;
	mField_t *curField;


	debugTrace(TRACE_IN,"doSelect()");

	identFields = NULL;
	
	numFields = 0;
	curField = query->fieldHead;
	while(curField)
	{
		numFields++;
		curField = curField->next;
	}

	/*
	** Find the offsets of the given fields and condition
	*/
	if (utilSetupFields(cacheEntry,flist,query->fieldHead) < 0)
	{
		debugTrace(TRACE_OUT,"doSelect()");
		return(-1);
	}
	if (checkConds)
	{
		if (utilSetupConds(cacheEntry,query->condHead) < 0)
		{
			debugTrace(TRACE_OUT,"doSelect()");
			return(-1);
		}
	}


	if (tmpTable)
	{
		if (utilSetupFields(tmpTable,tmpFlist,query->fieldHead) < 0)
		{
			debugTrace(TRACE_OUT,"doSelect()");
			return(-1);
		}
	}

	candidate = craSetupCandidate(cacheEntry, query, IGNORE_IDENT);
	if (!candidate)
	{
		return(-1);
	}

	rowLen = cacheEntry->rowLen;

	if (tableInitTable(cacheEntry,FULL_REMAP) < 0)
	{
		craFreeCandidate(candidate);
		debugTrace(TRACE_OUT,"doSelect()");
		return(-1);
	}


	if (functCheckFunctions(query) < 0)
	{
		craFreeCandidate(candidate);
		debugTrace(TRACE_OUT,"doSelect()");
		return(-1);
	}

	/*
	** Tell the client how many fields there are in a row
	*/

	if (dest == DEST_CLIENT && !query->explainOnly)
	{
		snprintf(packet,PKT_LEN,"1:%d:\n",numFields);
		netWritePacket(query->clientSock);
	}


	/*
	** Special case for singe field sys var access.  Check the comments
	** in craSetupCandidate() for info on why this is needed
	*/
	if (candidate->type == CAND_SYS_VAR)
	{
		tableExtractValues(cacheEntry, NULL, query->fieldHead, flist,
			query);
		utilFormatPacket(packet, query->fieldHead);
		netWritePacket(query->clientSock);
		netEndOfList(query->clientSock);

		/*
		** Send the field info down the line to the client
		*/
		snprintf(outBuf,sizeof(outBuf),"%d",query->fieldHead->length);
		snprintf(outBuf2,sizeof(outBuf2),"%d",query->fieldHead->type);
		snprintf(packet,PKT_LEN,"%d:%s%d:%s%d:%s%d:%s1:%s1:%s", 
			(int)strlen(query->fieldHead->table),
			query->fieldHead->table,
			(int)strlen(query->fieldHead->name),
			query->fieldHead->name, 
			(int)strlen(outBuf2), outBuf2,
			(int)strlen(outBuf), outBuf, 
			query->fieldHead->flags & NOT_NULL_FLAG ? "Y":"N",
			" ");
		netWritePacket(query->clientSock);
		netEndOfList(query->clientSock);
		debugTrace(TRACE_OUT,"doSelect()");
		craFreeCandidate(candidate);
		return(0);
	}

	/*
	** OK, no more wierd stuff.  Just do the usual.  If for some
	** reason the client socket is closed during all this (e.g. it 
	** gets force closed due to a SIGPIPE) then break out of the loop
	*/

	abortSelect = 0;
	count = 0;
	rowNum = craGetCandidate(cacheEntry, candidate);
	while (rowNum != NO_POS && abortSelect == 0)
	{
		if(tableReadRow(cacheEntry,&row,rowNum) < 0)
		{
			craFreeCandidate(candidate);
			return(-1);
		}
		if (row.header->active)
		{
			if (checkConds)
			{
				res = compareMatchRow(cacheEntry,&row,
					query->condHead, query);
			}
			else
			{
				res = compareMatchRow(cacheEntry,&row,
					NULL, query);
			}
			if (res < 0)
			{
				craFreeCandidate(candidate);
				return(-1);
			}
			if (res == 1)
			{
				tableExtractValues(cacheEntry, 
					&row,query->fieldHead,flist,query);
				functProcessFunctions(cacheEntry,query);
				if (dest == DEST_CLIENT)
				{
				    if (!query->explainOnly &&
				    	 count >= query->rowOffset)
				    {
					utilFormatPacket(packet, 
						query->fieldHead);
					if(netWritePacket(query->clientSock)<0)
					{
						abortSelect = 1;
						continue;
					}
				    }
				}
				else
				{
					bzero(tmpTable->row.data,
						tmpTable->rowLen);
					tableFillRow(tmpTable,
						&(tmpTable->row),
						query->fieldHead, tmpFlist);
					tableWriteRow(tmpTable,NULL,NO_POS,
						query);
				}
				count++;
			}
		}
		if (dest == DEST_CLIENT && query->rowLimit > 0)
		{
			if (count == query->rowLimit + query->rowOffset)
				break;
		}
		rowNum = craGetCandidate(cacheEntry, candidate);
	}
	if (dest == DEST_CLIENT && !query->explainOnly)
	{
		netEndOfList(query->clientSock);

		/*
		** Send the field info down the line to the client
		*/
		curField = query->fieldHead;
		while(curField)
		{
			if (curField->function)
			{
				if (*curField->function->outputName)
					fieldName = 
						curField->function->outputName;
				else
					fieldName = curField->function->name;
			}
			else
			{
				fieldName = curField->name;
			}
			snprintf(outBuf,sizeof(outBuf),"%d",curField->length);
			snprintf(outBuf2,sizeof(outBuf2),"%d",curField->type);
			snprintf(packet,PKT_LEN,"%d:%s%d:%s%d:%s%d:%s1:%s1:%s", 
				(int)strlen(curField->table), curField->table,
				(int)strlen(fieldName), fieldName,
				(int)strlen(outBuf2), outBuf2,
				(int)strlen(outBuf), outBuf, 
				curField->flags & NOT_NULL_FLAG ? "Y":"N",
				" ");
			netWritePacket(query->clientSock);
			curField = curField->next;
		}
		netEndOfList(query->clientSock);
	}
	else
	{
		if (query->explainOnly)
		{
			netEndOfList(query->clientSock);
		}
	}
	debugTrace(TRACE_OUT,"doSelect()");
	craFreeCandidate(candidate);
	return(0);
}



static int checkConds(conds, tables)
	mTable_t	*tables;
	mCond_t	*conds;
{
	mCond_t	*curCond;
	mTable_t	*curTable;
	
	curCond = conds;
	while(curCond)
	{
		if (curCond->subCond)
		{
			if (checkConds(curCond->subCond, tables) < 0)
				return(-1);
			curCond = curCond->next;
			continue;
		}
		curTable = tables;
		while(curTable)
		{
			if ( *(curCond->table) != *(curTable->name))
			{
				curTable = curTable->next;
				continue;
			}
			if (strcmp(curCond->table,curTable->name) != 0)
			{
				curTable = curTable->next;
				continue;
			}
			break;
		}
		if (!curTable)
		{
			snprintf(errMsg,MAX_ERR_MSG, UNSELECT_ERROR,
				(char *)curCond->table);
			return(-1);
		}
		curCond = curCond->next;
	}
	return(0);
}



/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


int selectProcessSelect(server, query)
	msqld		*server;
	mQuery_t	*query;
{
	cache_t		*cacheEntry,
			*table1,
			*table2,
			*tmpTable;
	mTable_t	*curTable;
	mField_t	*curField;
	int		join,
			foundTable;
	u_int		freeList;
	struct  stat 	sbuf;


	debugTrace(TRACE_IN,"selectProcessSelect()");


	/*
	** Check out the tables and fields specified in the query.  If
	** multiple tables are specified all field specs must be
	** qualified and they must reference a selected table.
	*/
	if (query->tableHead->next)
	{
		if (query->explainOnly)
		{
			strcpy(packet,"Query is a join.\n");
			netWritePacket(query->clientSock);
			strcpy(packet,"Original table order is :\n");
			netWritePacket(query->clientSock);
			curTable = query->tableHead;
			while(curTable)
			{
				if (*curTable->cname)
				{
					snprintf(packet,PKT_LEN,"\t%s (%s)\n",
						curTable->name, 
						curTable->cname);
				}
				else
				{
					snprintf(packet,PKT_LEN,"\t%s\n",
						curTable->name) ;
				}
				netWritePacket(query->clientSock);
				curTable = curTable->next;
			}
		}
		join = 1;
		query->tableHead = (mTable_t *)craReorderTableList(query);
		if (query->explainOnly)
		{
			strcpy(packet,"Table list reorder completed.\n");
			netWritePacket(query->clientSock);
			strcpy(packet,"New table order is :\n");
			netWritePacket(query->clientSock);
			curTable = query->tableHead;
			while(curTable)
			{
				if (*curTable->cname)
				{
					snprintf(packet,PKT_LEN,"\t%s (%s)\n",
						curTable->name, 
						curTable->cname);
				}
				else
				{
					snprintf(packet,PKT_LEN,"\t%s\n",
						curTable->name) ;
				}
				netWritePacket(query->clientSock);
				curTable = curTable->next;
			}
		}
	}
	else
	{
		/*
		** If there's no joins ensure that each condition and field
		** is fully qualified with the correct table
		*/
		utilQualifyFields(query);
		utilQualifyConds(query);
		utilQualifyOrder(query);
		join = 0;
		if (query->explainOnly)
		{
			strcpy(packet,"Query is not a join.\n");
			netWritePacket(query->clientSock);
		}
	}
	curTable = query->tableHead;
	tmpTable = NULL;

	/*
	** Ensure that any field or condition refers to fields of 
	** selected tables
	*/
	if (checkFieldTable(query->fieldHead,query) < 0)
	{
		debugTrace(TRACE_OUT,"selectProcessSelect()");
		return(-1);
	}
	if (checkConds(query->condHead,query->tableHead) < 0)
	{
		debugTrace(TRACE_OUT,"selectProcessSelect()");
		return(-1);
	}


	curField = query->fieldHead;
	while (curField)
	{
		if (strcmp(curField->name, "*") == 0)
		{
			curField = curField->next;
			continue;
		}
		if (*(curField->table) == 0)
		{
			if (join)
			{
				snprintf(errMsg, MAX_ERR_MSG, 
					UNQUAL_JOIN_ERROR, curField->name);
				debugTrace(TRACE_OUT,"selectProcessSelect()");
				return(-1);
			}
			curField = curField->next;
			continue;
		}
		curTable = query->tableHead;
		foundTable = 0;
		while(curTable)
		{
			if ( *(curTable->name) != *(curField->table))
			{
				curTable = curTable->next;
				continue;
			}
			if (strcmp(curTable->name,curField->table) != 0)
			{
				curTable = curTable->next;
				continue;
			}
			foundTable = 1;
			break;
		}
		if (!foundTable)
		{
			snprintf(errMsg,MAX_ERR_MSG,UNSELECT_ERROR, 
				curField->table);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
		curField = curField->next;
	}


	/*
	** If there's multiple tables, join the suckers.
	*/

	if (join)
	{
		curTable = query->tableHead;
		while(curTable)
		{
			if (curTable == query->tableHead)
			{
				table1 = tableLoadDefinition(server,
					curTable->name, curTable->cname,
					query->curDB);
				if (!table1)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				if (tableInitTable(table1,FULL_REMAP) < 0)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				curTable = curTable->next;
				table2 = tableLoadDefinition(server,
					curTable->name, curTable->cname,
					query->curDB);
				if (!table2)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				if (tableInitTable(table2,FULL_REMAP) < 0)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				if (!table1 || !table2)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				setJoinTableDone(query->tableHead,
					table1->table);
				setJoinTableDone(query->tableHead,
					table2->table);
				tmpTable=joinTables(server,table1,table2,query);
				if (!tmpTable)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
			}
			else
			{
				table1 = tmpTable;
				table2 = tableLoadDefinition(server,
					curTable->name, curTable->cname,
					query->curDB);
				if (!table2)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				if (tableInitTable(table1,FULL_REMAP) < 0)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				if (tableInitTable(table2,FULL_REMAP) < 0)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
				setJoinTableDone(query->tableHead,
					table1->table);
				setJoinTableDone(query->tableHead,
					table2->table);
				tmpTable=joinTables(server,table1,table2,query);
				if (table1->result)
				{
					tableFreeTmpTable(server, table1);
				}
				if (!tmpTable)
				{
					debugTrace(TRACE_OUT,
						"selectProcessSelect()");
					return(-1);
				}
			}
			curTable = curTable->next;
		}
	}

	/*
	** Perform the actual select.  If there's an order clause or
	** a pending DISTINCT, send the results to a table for further 
	** processing.
	**
	** Look for the wildcard field spec.  Must do this before we
	** "setup" because it edits the field list.  selectWildcard
	** is a global set from inside the yacc parser.  Wild card
	** expansion is only called if this is set otherwise it will
	** consume 50% of the execution time of selects!
	*/

	if (!tmpTable)
	{
		if((cacheEntry = tableLoadDefinition(server, 
			query->tableHead->name, query->tableHead->cname, 
			query->curDB)) == NULL)
		{
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
	}
	else
	{
		cacheEntry = tmpTable;
	}
	if (query->selectWildcard)
	{
			query->fieldHead = utilExpandFieldWildCards(cacheEntry,
			query->fieldHead);
	}
	utilSetFieldInfo(cacheEntry, query->fieldHead);

	if (!query->orderHead && !query->selectDistinct && !query->targetTable)
	{
		if (doSelect(cacheEntry,query, DEST_CLIENT, BOOL_TRUE, NULL)<0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
		if (cacheEntry->result)
		{
			tableFreeTmpTable(server, cacheEntry);
		}
		debugTrace(TRACE_OUT,"selectProcessSelect()");
		return(0);
	}

	/*
	** From here on we just want a table with the required fields
	** (i.e. not all the fields of a join)
	*/
	tmpTable = tableCreateTmpTable(server, NULL, NULL, cacheEntry,NULL,
		query->fieldHead, QUERY_FIELDS_ONLY);
	if (!tmpTable)
	{
		if (cacheEntry->result)
			tableFreeTmpTable(server, cacheEntry);
		return(-1);
	}


	/*
	** If the end result is a target table then setup the overflow
	** file for the new table.  Otherwise just map back onto the overflow
	** file of the original table
	*/

	if (query->targetTable == NULL)
	{
		tmpTable->overflowMap = cacheEntry->overflowMap;
		tmpTable->overflowSize = cacheEntry->overflowSize;
	}
	else
	{
		tmpTable->overflowFD = tableOpenOverflow(server, cacheEntry,
			tmpTable->table, ".tmp");
		freeList = NO_POS;
        	write(tmpTable->overflowFD,&freeList,sizeof(u_int));
		fstat(tmpTable->overflowFD, &sbuf);
		tmpTable->overflowSize = sbuf.st_size;
		tmpTable->overflowMap = (caddr_t)mmap(NULL,
			(size_t)tmpTable->overflowSize,(PROT_READ | PROT_WRITE),
			MAP_SHARED, tmpTable->overflowFD, (off_t)0);
		tmpTable->remapOverflow = 0;
	}

	(void)snprintf(tmpTable->resInfo, 4 * (NAME_LEN + 1),
		"'%s (stripped %s)'", tmpTable->table,cacheEntry->table);
	if (doSelect(cacheEntry,query, DEST_TABLE, BOOL_TRUE, tmpTable) < 0)
	{
		if (cacheEntry->result)
			tableFreeTmpTable(server, cacheEntry);
		tableFreeTmpTable(server, tmpTable);
		debugTrace(TRACE_OUT,"selectProcessSelect()");
		return(-1);
	}
	if (cacheEntry->result)
	{
		tableFreeTmpTable(server, cacheEntry);
	}
	cacheEntry = tmpTable;

	/*
	** Blow away multiples if required
	*/
	if (query->selectDistinct)
	{
		if (distinctCreateTable(server, cacheEntry) < 0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
	}
	
	/*
	** Sort the result if required
	*/
	if (query->orderHead)
	{
		if (sortCreateSortedTable(server, cacheEntry,query) < 0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
	}


	/*
	** Send the result to the client if we haven't yet.
	*/
	if (query->targetTable != NULL)
	{
		/* 
		** The data is already in a tmp table.  All we have to
		** do is move the existing tmp table into place and
		** create the definition file.  First, ensure that what
		** we have is sane from a normal tables point of view.
		*/
		char	oldPath[255],
			newPath[255];

		if (tableCheckTargetDefinition(query) < 0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
		if (tableCreateDefinition(server,query->targetTable,query)<0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
		snprintf(oldPath,255,"%s/.tmp/%s.dat",server->config.dbDir,
			cacheEntry->table);
		snprintf(newPath,255,"%s/%s/%s.dat",server->config.dbDir,
			query->curDB, query->targetTable);
		if (rename(oldPath, newPath) < 0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
		snprintf(oldPath,255,"%s/.tmp/%s.ofl",server->config.dbDir,
			cacheEntry->table);
		snprintf(newPath,255,"%s/%s/%s.ofl",server->config.dbDir,
			query->curDB, query->targetTable);
		if (rename(oldPath, newPath) < 0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
		strcpy(packet,"1\n");
		netWritePacket(query->clientSock);
	}
	else
	{
		/* Send this to the client */
		if (doSelect(cacheEntry,query,DEST_CLIENT,BOOL_FALSE,NULL)<0)
		{
			if(cacheEntry->result)
				tableFreeTmpTable(server, cacheEntry);
			debugTrace(TRACE_OUT,"selectProcessSelect()");
			return(-1);
		}
	}

	/*
	** Free the result table
	*/
	if (cacheEntry->result)
	{
		tableFreeTmpTable(server, cacheEntry);
	}


	debugTrace(TRACE_OUT,"selectProcessSelect()");
	return(0);

}


