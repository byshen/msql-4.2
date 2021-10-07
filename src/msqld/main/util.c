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
** $Id: util.c,v 1.25 2012/01/17 02:26:22 bambi Exp $
**
*/

/*
** Module	: main : util
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
#include <sys/stat.h>

#ifdef HAVE_DIRENT_H
#    include <dirent.h>   
#endif

#ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
#endif

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/version.h>
#include <msqld/main/table.h>
#include <msqld/main/util.h>
#include <msqld/main/sysvar.h>
#include <msqld/main/memory.h>
#include <msqld/cra/cra.h>
#include <common/types/types.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

char	errMsg[MAX_ERR_MSG];


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static char *escapeExport(buf)
        char    *buf;
{
        char    *cp1,
                *cp2;
        static  char newBuf[1024];

        cp1 = buf;
        cp2 = newBuf;
        while(*cp1)
        {
                if (*cp1 == '\\' || *cp1 == ',')
                {
                        *cp2 = '\\';
                        cp2++;
                }
                *cp2 = *cp1;
                cp1++;
                cp2++;
        }
        *cp2 = 0;
        return(newBuf);
}


static void _formatData(packet,fields, export, del_char)
        char    *packet;
        mField_t *fields;
        int     export;
	char	del_char;
{
        char    outBuf[100],
                realFmt[10],
		delimiter[2],
                *cp;
        u_char  *outData = NULL;
        mField_t *curField;

        debugTrace(TRACE_IN,"formatPacket()");
	if (export)
	{
		delimiter[0] = del_char;
		delimiter[1] = 0;
	}
        *packet = 0;
        cp = packet;
        curField = fields;
        while(curField)
        {
                if (!curField->value->nullVal)
                {
                        switch(curField->type)
                        {
                                case INT8_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%hhd",
                                            curField->value->val.int8Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case INT16_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%hd",
                                            curField->value->val.int16Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case INT32_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%d",
                                            curField->value->val.int32Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case INT64_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%lld",
                                            curField->value->val.int64Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case UINT8_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%hhu",
                                            curField->value->val.int8Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case UINT16_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%hu",
                                            curField->value->val.int16Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case UINT32_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%u",
                                            curField->value->val.int32Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case UINT64_TYPE:
                                        snprintf(outBuf,sizeof(outBuf),"%llu",
                                            curField->value->val.int64Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case IPV4_TYPE:
                                        typePrintIPv4(outBuf,sizeof(outBuf),
                                                curField->value->val.int32Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case CIDR4_TYPE:
                                        typePrintCIDR4(outBuf,sizeof(outBuf),
                                                curField->value->val.byteVal);
                                        outData = (u_char *)outBuf;
                                        break;
                                case IPV6_TYPE:
                                        typePrintIPv6(outBuf,sizeof(outBuf),
                                                curField->value->val.byteVal);
                                        outData = (u_char *)outBuf;
                                        break;
                                case CIDR6_TYPE:
                                        typePrintCIDR6(outBuf,sizeof(outBuf),
                                                curField->value->val.byteVal);
                                        outData = (u_char *)outBuf;
                                        break;
                                case DATE_TYPE:
                                        typePrintDate(outBuf,sizeof(outBuf),
                                                curField->value->val.int32Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case TIME_TYPE:
                                        typePrintTime(outBuf,sizeof(outBuf),
                                                curField->value->val.int32Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case DATETIME_TYPE:
                                        typePrintDateTime(outBuf,sizeof(outBuf),
                                                curField->value->val.byteVal);
                                        outData = (u_char *)outBuf;
                                        break;
                                case MILLITIME_TYPE:
                                        typePrintMilliTime(outBuf,
						sizeof(outBuf),
                                                curField->value->val.int32Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case MILLIDATETIME_TYPE:
                                        typePrintMilliDateTime(outBuf,
						sizeof(outBuf),
                                                curField->value->val.byteVal);
                                        outData = (u_char *)outBuf;
                                        break;
                                case MONEY_TYPE:
                                        typePrintMoney(outBuf,sizeof(outBuf),
                                                curField->value->val.int32Val);
                                        outData = (u_char *)outBuf;
                                        break;
                                case CHAR_TYPE:
                                case TEXT_TYPE:
                                        outData = curField->value->val.charVal;
                                        break;
                                case REAL_TYPE:
                                        snprintf(realFmt,sizeof(realFmt),
                                                "%%.%df",
                                                curField->value->precision);
                                        snprintf(outBuf,sizeof(outBuf),
                                                realFmt,
                                                curField->value->val.realVal);
                                        outData = (u_char *)outBuf;
                                        break;
				default:
					fprintf(stderr,"Unknown data type %d\n",
						curField->type);
                        }

                        if (export == 0)
                        {
                                /* inline sprintf dodge */
                                register int loop = 100000,
                                                start=0,
						vlen =strlen((char*)outData),
						v;
				v = vlen;
                                while(loop) {
                                        if (v / loop > 0 || start) {
                                                *cp++=(v / loop) + '0';
                                                v = v % loop;
                                                start=1;
                                        }
                                        loop /= 10;
                                }
                                *cp++ = ':';
                                *cp = 0;
                                strcat(cp, (char *)outData);
                               /* cp = cp + strlen(cp); */
                                cp = cp + vlen;
                        }
                        else
                        {
                                if (curField != fields)
                                        strcat(packet,delimiter);
                                outData = (u_char*)escapeExport((char*)outData);
                                strcat(packet,(char *)outData);
                        }
                }
                else
                {
                        strcat(cp,"-2:");
                        cp += 3;
                }
                curField = curField->next;
        }
        strcat(packet,"\n");
        debugTrace(TRACE_OUT,"formatPacket()");
}



/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



void utilQualifyFields(query)
	mQuery_t	*query;
{
	mField_t	*curField,
			*oldField;

	debugTrace(TRACE_IN,"utilQualifyFields()");
	curField = query->fieldHead;
	while(curField)
	{
		if (curField->function)
		{
			oldField = curField;
			curField = curField->function->paramHead;
			while(curField)
			{
				if (*(curField->table) == 0)
				{
					strcpy(curField->table,
						query->tableHead->name);
				}
				curField=curField->next;
			}
			curField = oldField->next;
			continue;
		}
		if (*(curField->table) == 0)
		{
			(void)strcpy(curField->table,query->tableHead->name);
		}
		curField=curField->next;
	}
	debugTrace(TRACE_OUT,"utilQualifyFields()");
}


void utilQualifyConds(query)
	mQuery_t	*query;
{
	mCond_t		*curCond;
	mQuery_t	tmpQuery;

	debugTrace(TRACE_IN,"utilQualifyConds()");
	curCond = query->condHead;
	while(curCond)
	{
		if (curCond->subCond)
		{
			bzero(&tmpQuery, sizeof(tmpQuery));
			tmpQuery.tableHead = query->tableHead;
			tmpQuery.condHead = curCond->subCond;
			utilQualifyConds(&tmpQuery);
			curCond = curCond->next;
			continue;
		}
		if(*(curCond->table) == 0)
		{
			(void)strcpy(curCond->table,query->tableHead->name);
		}
		curCond=curCond->next;
	}
	debugTrace(TRACE_OUT,"utilQualifyConds()");
}



void utilQualifyOrder(query)
	mQuery_t	*query;
{
	mOrder_t	*curOrder;

	debugTrace(TRACE_IN,"utilQualifyOrder()");
	curOrder = query->orderHead;
	while(curOrder)
	{
		if(*(curOrder->table) == 0)
		{
			(void)strcpy(curOrder->table,query->tableHead->name);
		}
		curOrder=curOrder->next;
	}
	debugTrace(TRACE_OUT,"utilQualifyOrder()");
}




int utilSetFieldInfo(cacheEntry, fields)
	cache_t		*cacheEntry;
	mField_t	*fields;
{
	mField_t	*curField,
			*fieldDef,
			*curParam;
	int		curOffset,
			count,
			res;

	debugTrace(TRACE_IN,"utilSetFieldsInfo()");
	curField = fields;
	curOffset = count = 0;
	while(curField)
	{
	    if (curField->literalParamFlag)
	    {
		curField = curField->next;
		continue;
	    }
	    if (curField->function)
	    {
		if (cacheEntry->result == 1)
		{
			curField = curField->next;	
			continue;
		}
		if(utilSetFieldInfo(cacheEntry,curField->function->paramHead)<0)
		{
			debugTrace(TRACE_OUT,"utilSetFieldInfo()");
			return(-1);
		}

		if (curField->type != CHAR_TYPE)
		{
			curField->dataLength = typeFieldSize(curField->type);
		}
		else
		{
			curParam = curField->function->paramHead;
			while(curParam)
			{
				if (curParam->length > curField->dataLength)
					curField->dataLength=curParam->length;
				curParam = curParam->next;
			}
			curField->dataLength *= 2;
		}
		curField->length = curField->dataLength;
		curField->offset = curOffset;
		curField->fieldID = count;
		curField->overflow = NO_POS;
		curField->functResultFlag = 1;
		curOffset+=curField->dataLength+1;
		curField = curField->next;	
		continue;
	    }
	    if (*curField->name == '_' && ! cacheEntry->result)
	    {
		if ((res = sysvarCheckVariable(cacheEntry,curField)) < 0)
		{
			if (res == -2)
				return(-1);
		
			snprintf(errMsg, MAX_ERR_MSG, SYSVAR_ERROR, 
				curField->name);
			msqlDebug1(MOD_ERR,SYSVAR_ERROR, curField->name);
			return(-1);
		}
	    	curField->sysvar = 1;
		curField = curField->next;
		continue;
	    }
	    curField->sysvar = 0;
	    fieldDef = cacheEntry->def;
	    while(fieldDef)
	    {
		if( *(curField->name) != *(fieldDef->name) ||
		    (curField->function == NULL && 
			*(curField->table) != *(fieldDef->table)))
		{
			curOffset+=fieldDef->dataLength+1;/* +1 for null flag */
			fieldDef = fieldDef->next;
			count++;
			continue;
		}
		if(strcmp(curField->name,fieldDef->name) != 0 ||
		    (curField->function == NULL && 
		   	strcmp(curField->table,fieldDef->table) != 0))
		{
			curOffset+=fieldDef->dataLength+1;/* +1 for null flag */
			fieldDef = fieldDef->next;
			count++;
			continue;
		}

		curField->type = fieldDef->type;
		curField->length = fieldDef->length;
		curField->dataLength = fieldDef->dataLength;
		curField->flags = fieldDef->flags;
		curField->entry = fieldDef->entry;
		curField->offset = curOffset;
		curField->fieldID = count;
		curField->overflow = NO_POS;
		curOffset+=curField->dataLength+1;
		break;
	    }
	    curField = curField->next;
	}
	debugTrace(TRACE_OUT,"utilSetFieldInfo()");
	return(0);
}





int utilSetupFields(cacheEntry,flist, fields)
	cache_t		*cacheEntry;
	int		*flist;
	mField_t	*fields;
{
	mField_t	*curField,
			*fieldDef;
	int		numFields,
			*curFL,
			curOffset,
			count,
			res;

	debugTrace(TRACE_IN,"utilSetupFields()");
	numFields = 0;
	curField = fields;
	curFL = flist;
	while(curField)
	{
		numFields++;
		if (numFields < MAX_FIELDS)
			*curFL++ = -1;
		curField=curField->next;
	}
	if (numFields > MAX_FIELDS)
	{
		snprintf(errMsg,MAX_ERR_MSG,FIELD_COUNT_ERROR);
		msqlDebug0(MOD_ERR,"Too many fileds in query\n");
		debugTrace(TRACE_OUT,"utilSetupFields()");
		return(-1);
	}
	*curFL = -1;


	/*
	** If this is a result table then the field list will already
	** have the specific offsets we need (including the offsets of
	** unusual fields like function results etc).  Just scan the
	** field list and use those values
	**
	** NOTE: This code breaks the creation of tmp tables as the
	** offsets are incorrect (i.e. they can reflect the original
	** offset from the original table not the result offset of the
	** tmp table
	**
	if (cacheEntry->result)
	{
		curField = fields;
		curFL = flist;
		while(curField)
		{
			*curFL = curField->offset;
			curFL++;
			curField = curField->next;
		}
		return(0);
	}
	*/
	

	/*
	** OK, this is not a simple result table so we have to do all
	** the hard work
	*/

	curField = fields;
	curFL = flist;
	while(curField)
	{
	    curOffset = count = 0;
	    if (curField->literalParamFlag)
	    {
		curField = curField->next;
		continue;
	    }

	    if (curField->functResultFlag || curField->function)
	    {
		/*
		** Don't mess with the parameter offset setting if we
		** are being called for a result table.  We only set
		** parameter offsets for source tables!
		*/
		if (! cacheEntry->result)
		{
			utilSetupFields(cacheEntry,curField->function->flist, 
				curField->function->paramHead);
		}
		*curFL++ = curOffset;
		curOffset+=curField->dataLength+1;
		curField = curField->next;	
		continue;
	    }

	    if (*curField->name == '_' && ! cacheEntry->result)
	    {
		if ((res = sysvarCheckVariable(cacheEntry,curField)) < 0)
		{
			if (res == -2)
				return(-1);
		
			snprintf(errMsg, MAX_ERR_MSG, SYSVAR_ERROR, 
				curField->name);
			msqlDebug1(MOD_ERR,SYSVAR_ERROR, curField->name);
			return(-1);
		}
	    	curField->sysvar = 1;
		curField = curField->next;
		continue;
	    }
	    curField->sysvar = 0;
	    fieldDef = cacheEntry->def;
	    while(fieldDef)
	    {
		if( *(curField->name) != *(fieldDef->name) ||
		    (curField->function == NULL && 
			*(curField->table) != *(fieldDef->table)))
		{
			curOffset+=fieldDef->dataLength+1;/* +1 for null flag */
			fieldDef = fieldDef->next;
			count++;
			continue;
		}
		if(strcmp(curField->name,fieldDef->name) != 0 ||
		    (curField->function == NULL && 
		   	strcmp(curField->table,fieldDef->table) != 0))
		{
			curOffset+=fieldDef->dataLength+1;/* +1 for null flag */
			fieldDef = fieldDef->next;
			count++;
			continue;
		}

		curField->type = fieldDef->type;
		curField->length = fieldDef->length;
		curField->dataLength = fieldDef->dataLength;
		curField->flags = fieldDef->flags;
		curField->entry = fieldDef->entry;
		curField->offset = curOffset;
		curField->fieldID = count;
		curField->overflow = NO_POS;
		*curFL = curOffset;
		if (!curField->value)
			break;
		if (!curField->value->nullVal)
		{
			if (curField->length < curField->value->dataLen &&
			    curField->type == CHAR_TYPE)
			{
				snprintf(errMsg, MAX_ERR_MSG,
					VALUE_SIZE_ERROR, curField->name);
				msqlDebug1(MOD_ERR, VALUE_SIZE_ERROR,
					curField->name);
				return(-1);
			}
			res = typeValidConditionTarget(curField->type,
				curField->value);
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
		}
		break;
	    }
	    if(!fieldDef)  /* Bad entry */
	    {
		if (*(curField->table))
		{
		    snprintf(errMsg,MAX_ERR_MSG, BAD_FIELD_ERROR, 
			curField->table, curField->name);
		    msqlDebug2(MOD_ERR,"Unknown field \"%s.%s\"\n",
				curField->table,curField->name);
		    debugTrace(TRACE_OUT,"utilSetupFields()");
		    return(-1);
		}
		else
		{
		    snprintf(errMsg,MAX_ERR_MSG, BAD_FIELD_2_ERROR,
			curField->name);
		    msqlDebug1(MOD_ERR,"Unknown field \"%s\"\n",curField->name);
		    debugTrace(TRACE_OUT,"utilSetupFields()");
		    return(-1);
		}
	    }
	    curFL++;
	    curField = curField->next;
	}
	debugTrace(TRACE_OUT,"utilSetupFields()");
	return(0);
}




int utilSetCondValueType(curCond, value)
	mCond_t	*curCond;
	mVal_t	*value;
{
	int	res;

	if (value->nullVal)
	{
		return(0);
	}
	if (value->type == IDENT_TYPE)
	{
		return(0);
	}

	/*
	** Setup the cond value.  Scan to internal form if needed
	*/
	res = typeValidConditionTarget(curCond->type, value);
	if (res == -1)
	{
		snprintf(errMsg,MAX_ERR_MSG,BAD_TYPE_ERROR, curCond->name);
		msqlDebug1(MOD_ERR,"Bad type for comparison of '%s'",
                                curCond->name);
		return(-1);
	}
	if (res == -2)
	{
		/* Error msg already set */
		return(-1);
	}
	return(0);
}



/****************************************************************************
** 	_utilSetupConds
**
**	Purpose	: Determine the byte offset into a row for conditional
**		  data.
**	Args	: Condition list (field location) array,
**		  List of fileds used in conditionals
**	Returns	: -1 on error
**	Notes	: As per utilSetupFields.
*/

int utilSetupConds(cacheEntry,conds)
	cache_t	*cacheEntry;
	mCond_t	*conds;
{
	mCond_t		*curCond;
	mField_t	*fieldDef;
	int		numConds,
			*curFL,
			curOffset,
			fieldID,
			res;

	debugTrace(TRACE_IN,"utilSetupConds()");
	numConds = 0;
	curCond = conds;
	curFL = NULL;
	if (conds)
	{
		curFL = conds->clist;
	}
	while(curCond)
	{
		if (curCond->subCond)
		{
			curCond = curCond->next;
			continue;
		}
		numConds++;
		if (numConds < MAX_FIELDS)
			*curFL++ = -1;
		curCond=curCond->next;
	}
	if (numConds > MAX_FIELDS)
	{
		snprintf(errMsg,MAX_ERR_MSG,COND_COUNT_ERROR);
		msqlDebug0(MOD_ERR,"Too many fields in condition\n");
		debugTrace(TRACE_OUT,"utilSetupConds()");
		return(-1);
	}
	if (curFL)
		*curFL = -1;
	
	curCond = conds;
	curFL = conds->clist;
	while(curCond)
	{
		if (curCond->subCond)
		{
			if (utilSetupConds(cacheEntry, curCond->subCond)<0)
			{
				return(-1);	
			}
			curCond = curCond->next;
			continue;
		}
            	if (*curCond->name == '_')
            	{
			if ((res = sysvarCheckCondition(curCond)) < 0)
			{
				if (res == -2)
					return(-1);
                        	snprintf(errMsg, MAX_ERR_MSG, SYSVAR_ERROR, 
					curCond->name);
                        	msqlDebug1(MOD_ERR,SYSVAR_ERROR, curCond->name);
				return(-1);
			}
			curCond->sysvar = 1;
			curCond->fieldID = -2;
			curCond = curCond->next;
			continue;
		}
		curCond->sysvar = 0;
		fieldDef = cacheEntry->def;
		curOffset = 0;
		fieldID = 0;
		while(fieldDef)
		{
			if( *(curCond->name) != *(fieldDef->name) ||
			    *(curCond->table) != *(fieldDef->table))
			{
				/* +1 for null flag */
				curOffset += fieldDef->dataLength+1; 
				fieldDef = fieldDef->next;
				fieldID ++;
				continue;
			}
			
			if(strcmp(curCond->name,fieldDef->name) != 0 ||
			   strcmp(curCond->table,fieldDef->table) != 0)
			{
				/* +1 for null flag */
				curOffset += fieldDef->dataLength+1; 
				fieldDef = fieldDef->next;
				fieldID ++;
				continue;
			}

			curCond->type = fieldDef->type;
			curCond->length = fieldDef->length;
			curCond->fieldID = fieldID;
		
			if (utilSetCondValueType(curCond, curCond->value) < 0)
				return(-1);
			if (curCond->op == BETWEEN_OP)
			{
				if (utilSetCondValueType(curCond, 
					curCond->maxValue) < 0)
				{
					return(-1);
				}
			}
			*curFL = curOffset;
			break;
		}
		if (!fieldDef)
		{
			snprintf(errMsg, MAX_ERR_MSG, BAD_FIELD_2_ERROR, 
				curCond->name);
			msqlDebug1(MOD_ERR,
				"Unknown field in where clause \"%s\"\n",
				curCond->name);
			debugTrace(TRACE_OUT,"utilSetupConds()");
			return(-1);
		}
		curFL++;
		curCond->fieldID = fieldID;
		curCond->type = fieldDef->type;
		curCond = curCond->next;
	}
	debugTrace(TRACE_OUT,"utilSetupConds()");
	return(0);
}



/****************************************************************************
** 	_utilSetupOrder
**
**	Purpose	: Determine the byte offset into a row for order
**		  data.
**	Args	: Order list (field location) array,
**		  List of fileds used in order
**	Returns	: -1 on error
**	Notes	: As per utilSetupFields.
*/

int utilSetupOrder(cacheEntry,olist, order)
	cache_t	*cacheEntry;
	int	*olist;
	mOrder_t	*order;
{
	mOrder_t	*curOrder;
	mField_t	*fieldDef;
	int		numOrder,
			*curFL,
			curOffset;

	debugTrace(TRACE_IN,"utilSetupOrder()");
	numOrder = 0;
	curOrder = order;
	curFL = olist;
	while(curOrder)
	{
		numOrder++;
		if (numOrder < MAX_FIELDS)
			*curFL++ = -1;
		curOrder=curOrder->next;
	}
	if (numOrder > MAX_FIELDS)
	{
		snprintf(errMsg,MAX_ERR_MSG,ORDER_COUNT_ERROR);
		msqlDebug0(MOD_ERR,"Too many fields in order specification\n");
		debugTrace(TRACE_OUT,"utilSetupOrder()");
		return(-1);
	}
	*curFL = -1;
	
	curOrder = order;
	curFL = olist;
	while(curOrder)
	{
		fieldDef = cacheEntry->def;
		curOffset = 0;
		while(fieldDef)
		{
			if( *(curOrder->name) != *(fieldDef->name) ||
			    *(curOrder->table) != *(fieldDef->table))
			{
				/* +1 for null flag */
				curOffset += fieldDef->dataLength+1; 
				fieldDef = fieldDef->next;
				continue;
			}
		
			if(strcmp(curOrder->name,fieldDef->name) != 0 ||
			   strcmp(curOrder->table,fieldDef->table) != 0)
			{
				/* +1 for null flag */
				curOffset += fieldDef->dataLength+1; 
				fieldDef = fieldDef->next;
				continue;
			}

			curOrder->type = fieldDef->type;
			curOrder->length = fieldDef->length;
			curOrder->entry = fieldDef->entry;
			*curFL = curOffset;
			break;
		}
		if (!fieldDef)
		{
			snprintf(errMsg, MAX_ERR_MSG,
				cacheEntry->result==0?
				BAD_FIELD_2_ERROR:BAD_ORD_FIELD_2_ERROR,
				curOrder->name);
			msqlDebug1(MOD_ERR,
				"Unknown field in order clause \"%s\"\n",
				curOrder->name);
			debugTrace(TRACE_OUT,"utilSetupOrder()");
			return(-1);
		}
		curFL++;
		curOrder = curOrder->next;
	}
	debugTrace(TRACE_OUT,"utilSetupOrder()");
	return(0);
}






/****************************************************************************
** 	_dupFieldList
**
**	Purpose	: 
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

mField_t *utilDupFieldList(cacheEntry)
	cache_t	*cacheEntry;
{
	mField_t	*fieldDef,
		*prevField,
		*newField,
		*head;


	/*
	** Scan the field list
	*/

	debugTrace(TRACE_IN,"dupFieldList()");
	prevField = head = NULL;
	fieldDef = cacheEntry->def;
	while(fieldDef)
	{
		newField = memMallocField();
		strcpy(newField->name,fieldDef->name);
		strcpy(newField->table,fieldDef->table);
		if (fieldDef->function)
		{
			newField->function = (mFinfo_t*)malloc(
				sizeof(mFinfo_t));
			bcopy(fieldDef->function,newField->function,
				sizeof(mFinfo_t));
		}
		newField->value = NULL;
		newField->type = fieldDef->type;
		newField->sysvar = fieldDef->sysvar;
		newField->length = fieldDef->length;
		newField->dataLength = fieldDef->dataLength;
		newField->offset = fieldDef->offset;
		newField->null = fieldDef->null;
		newField->flags = fieldDef->flags;
		newField->fieldID = fieldDef->fieldID;
		newField->overflow = NO_POS;
		newField->entry = fieldDef->entry;
		newField->function = fieldDef->function;
		newField->functResultFlag = 0;
		newField->next = NULL;
		if (!prevField)
		{
			head = newField;
		}
		else
			prevField->next = newField;
		prevField = newField;
		fieldDef = fieldDef->next;
	}
	debugTrace(TRACE_OUT,"dupFieldList()");
	return(head);
}



/****************************************************************************
** 	_expandWildCards
**
**	Purpose	: Handle "*" in a select clause
**	Args	: 
**	Returns	: 
**	Notes	: This just drops the entire table into the field list
**		  when it finds a "*"
*/

mField_t *utilExpandFieldWildCards(cacheEntry,fields)
	cache_t	*cacheEntry;
	mField_t	*fields;
{
	mField_t	*curField,
			*fieldDef,
			*prevField,
			*newField,
			*tmpField,
			*head;


	/*
	** Scan the field list
	*/

	debugTrace(TRACE_IN,"expandWildcard()");
	head = curField = fields;
	prevField = NULL;
	while(curField)
	{
		if (strcmp(curField->name,"*") == 0)
		{
			/*
			** Setup a new entry for each field
			*/
			fieldDef = cacheEntry->def;
			while(fieldDef)
			{
				newField = memMallocField();
				strcpy(newField->name,fieldDef->name);
				strcpy(newField->table,fieldDef->table);
				if (fieldDef->function)
				{
					newField->function = (mFinfo_t*)malloc(
						sizeof(mFinfo_t));
					bcopy(fieldDef->function,
						newField->function,
						sizeof(mFinfo_t));
				}
				else
				{
					newField->function = NULL;
				}
				newField->value = NULL;
				newField->type = fieldDef->type;
				newField->sysvar = fieldDef->sysvar;
				newField->length = fieldDef->length;
				newField->dataLength = fieldDef->dataLength;
				newField->offset = fieldDef->offset;
				newField->null = fieldDef->null;
				newField->flags = fieldDef->flags;
				newField->fieldID = fieldDef->fieldID;
				newField->entry = fieldDef->entry;
				newField->overflow = NO_POS;
				newField->literalParamFlag = 0;
				newField->functResultFlag = 0;
				if (!prevField)
				{
					head = newField;
				}
				else
					prevField->next = newField;
				newField->next = curField->next;
				prevField = newField;
				fieldDef = fieldDef->next;
			}

			/*
			** Blow away the wildcard entry
			*/
			if (curField->type == CHAR_TYPE || 
			    curField->type == TEXT_TYPE)
			{
				if(curField->value->val.charVal)
				{
					free(curField->value->val.charVal);
					curField->value->val.charVal = NULL;
				}
			}
			tmpField = curField;
			curField = curField->next;
			if(tmpField)
				memFreeField(tmpField);
		}
		else
		{
			prevField = curField;
			curField = curField->next;
		}
	}
	debugTrace(TRACE_OUT,"expandWildcard()");
	return(head);
}



void utilExpandTableFields(server,table, query)
	msqld		*server;
	char    	*table;
	mQuery_t	*query;
{
	cache_t 	*cacheEntry;
	char		tableName[NAME_LEN];

	debugTrace(TRACE_IN,"expandTableFields()");
	strcpy(tableName,table);
	if((cacheEntry=tableLoadDefinition(server,tableName,NULL,query->curDB)))
	{
		query->fieldHead = utilExpandFieldWildCards(cacheEntry,
			query->fieldHead);
	}
	debugTrace(TRACE_OUT,"expandTableFields()");
}





int utilCheckDB(server, db)
	msqld	*server;
	char	*db;
{
	char	path[MSQL_PATH_LEN];
	struct	stat buf;

	debugTrace(TRACE_IN,"msqlInit()");
	(void)snprintf(path, MSQL_PATH_LEN, "%s/%s",server->config.dbDir,db);
	if (stat(path,&buf) < 0)
	{
		snprintf(errMsg, MAX_ERR_MSG, BAD_DB_ERROR,db);
		msqlDebug1(MOD_ERR,"Unknown database \"%s\"\n",db);
		debugTrace(TRACE_OUT,"msqlInit()");
		return(-1);
	}
	debugTrace(TRACE_OUT,"msqlInit()");
	return(0);
}





void utilFormatPacket(packet,fields)
        char    *packet;
        mField_t *fields;
{
        _formatData(packet,fields,0,0);
}

void utilFormatExport(char *packet,mField_t *fields,char delimiter)
{
        _formatData(packet,fields,1,delimiter);
}





row_t *utilDupRow(entry,row, new)
	cache_t	*entry;
	row_t 	*row,
		*new;
{
	if (!new)
	{
		new = (row_t *)malloc(sizeof(row_t));
		new->buf = (u_char *)malloc(entry->rowLen+2 + HEADER_SIZE);
	}
	bcopy(row->buf,new->buf,entry->rowLen + HEADER_SIZE);
	new->header = (hdr_t *)new->buf;
	new->data = new->buf + HEADER_SIZE;
	return(new);
}


void utilFreeRow(row)
	row_t	*row;
{
	if(row->buf)
	{
		free(row->buf);
		row->buf = NULL;
	}
	if(row)
		free(row);
}


int utilIsUTF8(string)
	char	*string;
{
	unsigned char* bytes;

	if(!string)
		return 0;

	bytes = (unsigned char*)string;
	while(*bytes)
	{
		/*
		** ASCII : bytes[0] <= 0x7F to allow ASCII control characters
		*/
		if(bytes[0] == 0x09 || bytes[0] == 0x0A || bytes[0] == 0x0D ||
			(0x20 <= bytes[0] && bytes[0] <= 0x7E))
		{
			bytes += 1;
			continue;
		}

		/*
		** non-overlong 2-byte
		*/
		if((0xC2 <= bytes[0] && bytes[0] <= 0xDF) &&
			(0x80 <= bytes[1] && bytes[1] <= 0xBF))
		{
			bytes += 2;
			continue;
		}

		/* 
		** excluding overlongs
		*/
		if( 	(bytes[0] == 0xE0 &&
			(0xA0 <= bytes[1] && bytes[1] <= 0xBF) &&
			(0x80 <= bytes[2] && bytes[2] <= 0xBF))
		  ||
			(// straight 3-byte
			((0xE1 <= bytes[0] && bytes[0] <= 0xEC) ||
			bytes[0] == 0xEE || bytes[0] == 0xEF) &&
			(0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
			(0x80 <= bytes[2] && bytes[2] <= 0xBF))
		  ||
			(// excluding surrogates
			bytes[0] == 0xED &&
			(0x80 <= bytes[1] && bytes[1] <= 0x9F) &&
			(0x80 <= bytes[2] && bytes[2] <= 0xBF))
		)
		{
			bytes += 3;
			continue;
		}

		if( 	(// planes 1-3
			bytes[0] == 0xF0 &&
			(0x90 <= bytes[1] && bytes[1] <= 0xBF) &&
			(0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
			(0x80 <= bytes[3] && bytes[3] <= 0xBF))
		  ||
			(// planes 4-15
			(0xF1 <= bytes[0] && bytes[0] <= 0xF3) &&
			(0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
			(0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
			(0x80 <= bytes[3] && bytes[3] <= 0xBF))
		  ||
			(// plane 16
			bytes[0] == 0xF4 &&
			(0x80 <= bytes[1] && bytes[1] <= 0x8F) &&
			(0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
			(0x80 <= bytes[3] && bytes[3] <= 0xBF))
		)
		{
			bytes += 4;
			continue;
		}
		return 0;
	}
	return 1;
}

