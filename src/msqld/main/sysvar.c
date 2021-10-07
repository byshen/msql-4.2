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
** $Id: sysvar.c,v 1.6 2002/06/29 04:09:01 bambi Exp $
**
*/

/*
** Module	: main : sysvar
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
#include <time.h>
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
#include <msqld/main/memory.h>
#include <common/types/types.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

static	int	sysSeqInit = 1,
		sysTimeInit = 1,
		sysDateInit = 1;


/* HACK */
extern  char    errMsg[];

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/* In-lined version of intMatch() */

#define int32Match(v1,v2,op,result)                       \
{                                                       \
        switch(op)                                      \
        {                                               \
                case EQ_OP:                             \
                        result = (v1 == v2);            \
                        break;                          \
                case NE_OP:                             \
                        result = (v1 != v2);            \
                        break;                          \
                case LT_OP:                             \
                        result = (v1 < v2);             \
                        break;                          \
                case LE_OP:                             \
                        result = (v1 <= v2);            \
                        break;                          \
                case GT_OP:                             \
                        result = (v1 > v2);             \
                        break;                          \
                case GE_OP:                             \
                        result = (v1 >= v2);            \
                        break;                          \
        }                                               \
}




/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


void sysvarResetVariables()
{
	sysSeqInit = 1;
	sysDateInit = 1;
	sysTimeInit = 1;
}



void sysvarGetVariable(entry,row,curField,query)
        cache_t 	*entry;
        row_t   	*row;
	mField_t	*curField;
	mQuery_t	*query;
{
	static	int	curSequence,
			curDate,
			curTime;
	char		*tmp;

	if(curField->value)
	{
		memFreeValue(curField->value);
		curField->value = NULL;
	}
        curField->value = memMallocValue();
	if (strcmp(curField->name, "_timestamp") == 0)
	{
		curField->value->val.int32Val = (int) row->header->timestamp;
		curField->value->type = INT32_TYPE;
		curField->value->nullVal = 0;
		return;
	}

	if (strcmp(curField->name, "_rowid") == 0)
	{
		curField->value->val.int32Val = row->rowID;
		curField->value->type = INT32_TYPE;
		curField->value->nullVal = 0;
		return;
	}

	if (strcmp(curField->name, "_seq") == 0)
	{
		entry->dirty = 1;
		if (sysSeqInit == 0)
		{
			curField->value->val.int32Val = curSequence;
		}
		else
		{
			curField->value->val.int32Val = 
				entry->sblk->sequence.value;
			entry->sblk->sequence.value += 
				entry->sblk->sequence.step;
			sysSeqInit = 0;
			curSequence = curField->value->val.int32Val;
		}
		curField->value->type = INT32_TYPE;
		curField->value->nullVal = 0;
		return;
	}

	if (strcmp(curField->name, "_sysdate") == 0)
	{
		if (sysDateInit == 0)
		{
			curField->value->val.int32Val = curDate;
		}
		else
		{
			tmp = (char *)msqlUnixTimeToDate(time(NULL));
			curField->value->val.int32Val = 
				typeScanCharDateValue(tmp,errMsg,MAX_ERR_MSG);
			curField->value->type = DATE_TYPE;
			curField->value->nullVal = 0;
			sysDateInit = 0;
			curDate = curField->value->val.int32Val;
		}
		return;
	}

	if (strcmp(curField->name, "_systime") == 0)
	{
		if (sysTimeInit == 0)
		{
			curField->value->val.int32Val = curTime;
		}
		else
		{
			tmp = (char *)msqlUnixTimeToTime(time(NULL));
			curField->value->val.int32Val = 
				typeScanCharTimeValue(tmp,errMsg,MAX_ERR_MSG);
			curField->value->type = TIME_TYPE;
			curField->value->nullVal = 0;
			sysTimeInit = 0;
			curTime = curField->value->val.int32Val;
		}
		return;
	}

	if (strcmp(curField->name, "_user") == 0)
	{
		curField->value->val.charVal = (u_char *)query->curUser;
		curField->value->type = CHAR_TYPE;
		curField->value->nullVal = 0;
		return;
	}
}



mField_t *sysvarGetDefinition(curField)
	mField_t	*curField;
{

	mField_t	*new;

	new = memMallocField();
	strcpy(new->table, curField->table);
	strcpy(new->name, curField->name);
	new->sysvar = 1;
	new->fieldID = -1;
	new->flags = -1;
	new->null = 0;

	if (strcmp(curField->name, "_timestamp") == 0)
	{
		new->type = INT32_TYPE;
		new->length = new->dataLength = sizeof(int);
		return(new);
	}

	if (strcmp(curField->name, "_rowid") == 0)
	{
		new->type = INT32_TYPE;
		new->length = new->dataLength = sizeof(int);
		return(new);
	}

	if (strcmp(curField->name, "_seq") == 0)
	{
		new->type = INT32_TYPE;
		new->length = new->dataLength = sizeof(int);
		return(new);
	}

	if (strcmp(curField->name, "_sysdate") == 0)
	{
		new->type = DATE_TYPE;
		new->length = new->dataLength = sizeof(int);
		return(new);
	}

	if (strcmp(curField->name, "_systime") == 0)
	{
		new->type = TIME_TYPE;
		new->length = new->dataLength = sizeof(int);
		return(new);
	}

	if (strcmp(curField->name, "_user") == 0)
	{
		new->type = CHAR_TYPE;
		new->length = new->dataLength = 16;
		return(new);
	}
	return(NULL); /* Just for lint */
}


int sysvarCompare(entry,row,cond, value)
        cache_t *entry;
        row_t   *row;
	mCond_t	*cond;
	mVal_t	*value;
{
	int	int32 = 0,
		result = 0;

	if (strcmp(cond->name, "_timestamp") == 0)
	{
		int32 = (int)row->header->timestamp;
	}

	if (strcmp(cond->name, "_rowid") == 0)
	{
		int32 = row->rowID;
	}

	switch(cond->type)
	{
		case INT32_TYPE:
			int32Match(int32, value->val.int32Val,
				cond->op, result);
			break;
	}
	return(result);
}


int sysvarCheckVariable(entry,curField)
	cache_t	*entry;
	mField_t	*curField;
{

	if (strcmp(curField->name, "_timestamp") == 0 ||
	    strcmp(curField->name, "_rowid") == 0)
	{
		curField->type = INT32_TYPE;
		curField->length = 4;
		curField->flags = 0;
		curField->entry = NULL;
		return(0);
	}
	if(strcmp(curField->name, "_user") == 0)
	{
		curField->type = CHAR_TYPE;
		curField->length = 16;
		curField->flags = 0;
		curField->entry = NULL;
		return(0);
	}
	if(strcmp(curField->name, "_sysdate") == 0)
	{
		curField->type = DATE_TYPE;
		curField->length = 4;
		curField->flags = 0;
		curField->entry = NULL;
		return(0);
	}
	if(strcmp(curField->name, "_systime") == 0)
	{
		curField->type = TIME_TYPE;
		curField->length = 4;
		curField->flags = 0;
		curField->entry = NULL;
		return(0);
	}
	if(strcmp(curField->name, "_seq") == 0)
	{
		if (entry->sblk->sequence.step == 0)
		{
			strcpy(errMsg,"No sequence defined for this table");
			msqlDebug0(MOD_ERR,
				"No sequence defined for this table");
			return(-2);
		}
		curField->type = UINT32_TYPE;
		curField->length = 4;
		curField->flags = 0;
		curField->entry = NULL;
		return(0);
	}
	return(-1);
}



int sysvarGetVariableType(name)
	char	*name;
{

	if (strcmp(name, "_timestamp") == 0 ||
	    strcmp(name, "_rowid") == 0)
	{
		return(INT32_TYPE);
	}
	if(strcmp(name, "_sysdate") == 0)
	{
		return(DATE_TYPE);
	}
	if(strcmp(name, "_systime") == 0)
	{
		return(TIME_TYPE);
	}
	if(strcmp(name, "_user") == 0)
	{
		return(CHAR_TYPE);
	}
	if(strcmp(name, "_seq") == 0)
	{
		return(UINT32_TYPE);
	}
	return(-1);
}


int sysvarCheckCondition(curCond)
	mCond_t	*curCond;
{

	if (strcmp(curCond->name, "_timestamp") == 0 )
	{
		curCond->type = INT32_TYPE;
		curCond->length = 4;
		return(0);
	}
	if(strcmp(curCond->name, "_rowid") == 0)
	{
		curCond->type = INT32_TYPE;
		curCond->length = 4;
		return(0);
	}
	if(strcmp(curCond->name, "_seq") == 0)
	{
		strcpy(errMsg,"Can't use _seq in conditions");
		msqlDebug0(MOD_ERR,"Can't use _seq in conditions");
		return(-2);
	}
	if(strcmp(curCond->name, "_user") == 0)
	{
		strcpy(errMsg,"Can't use _user in conditions");
		msqlDebug0(MOD_ERR,"Can't use _user in conditions");
		return(-2);
	}
	if(strcmp(curCond->name, "_sysdate") == 0)
	{
		strcpy(errMsg,"Can't use _sysdate in conditions");
		msqlDebug0(MOD_ERR,"Can't use _sysdate in conditions");
		return(-2);
	}
	if(strcmp(curCond->name, "_systime") == 0)
	{
		strcpy(errMsg,"Can't use _systime in conditions");
		msqlDebug0(MOD_ERR,"Can't use _systime in conditions");
		return(-2);
	}
	return(-1);
}


