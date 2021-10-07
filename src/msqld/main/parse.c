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
** $Id: parse.c,v 1.28 2012/05/31 04:22:03 bambi Exp $
**
*/

/*
** Module	: main : parse
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
#include <stdint.h>
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
#include <common/libc_stuff/c_stuff.h>

/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/includes/errmsg.h>
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/lexer.h>
#include <msqld/main/sysvar.h>
#include <msqld/main/util.h>
#include <msqld/main/net.h>
#include <msqld/main/parse.h>
#include <msqld/main/memory.h>
#include <common/types/types.h>
#include <libmsql/msql.h>

/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

static	cstk_t 	*condStack = NULL;
static	int	insertOffset = 0;

int yyparse();
unsigned long local_strtoul();

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static void _cleanCond(curCond)
	mCond_t 	*curCond;
{
	mCond_t 	*tmpCond;

	while(curCond)
	{
		if (curCond->subCond != NULL)
		{
			_cleanCond(curCond->subCond);
			curCond->subCond = NULL;
		}
		if (curCond->value)
		{
			parseFreeValue(curCond->value);
			curCond->value = NULL;
		}
		if (curCond->maxValue)
		{
			parseFreeValue(curCond->maxValue);
			curCond->maxValue = NULL;
		}
		curCond->op = curCond->bool = 0;
		tmpCond = curCond;
		curCond = curCond->next;
		tmpCond->next = NULL;
		memFreeCondition(tmpCond);
	}
}



void _cleanFields(curField)
	mField_t	*curField;
{
	mField_t	*tmpField;

	while(curField)
	{
		parseFreeValue(curField->value);
		if (curField->function)
		{
			_cleanFields(curField->function->paramHead);
			free(curField->function);
			curField->function = NULL;
		}
		curField->value = NULL;
		tmpField = curField;
		curField = curField->next;
		tmpField->next = NULL;
		memFreeField(tmpField);
	}
}


static u_char expandEscape(c,remain)
	u_char	*c;
	int	remain;
{
	u_char	ret;

	switch(*c)
	{
		case 'n':
			ret = '\n';
			break;
		case 't':
			ret = '\t';
			break;
		case 'r':
			ret = '\r';
			break;
		case 'b':
			ret = '\b';
			break;
		default:
			ret = *c;
			break;
	}
	return(ret);
}


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



/****************************************************************************
** 	parseCleanQuery - clean out the internal structures
**
**	Purpose	: Free all space and reset structures after a query
**	Args	: None
**	Returns	: Nothing
**	Notes	: Updates all public data structures
*/

void parseCleanQuery(query)
	mQuery_t	*query;
{
	register mTable_t 	*curTable, *tmpTable;
	register mOrder_t 	*curOrder, *tmpOrder;
	register mValList_t	*curValue, *tmpValue;
	register mField_t	*curField;
	extern	u_char		*yyprev, *yytext;

	debugTrace(TRACE_IN,"parseCleanQuery()");
	if (yyprev)
	{
		free(yyprev);
		yyprev = NULL;
	}
	yytext = NULL;

	/*
	** blow away the table list from the query
	*/
	curTable = query->tableHead;
	while(curTable)
	{
		tmpTable = curTable;
		curTable = curTable->next;
		tmpTable->next = NULL;
		memFreeTable(tmpTable);
	}

	/*
	** Blow away the insert value list
	*/
	if (query->insertValHead)
	{
		/* 
		** Ensure that none of these value pointers
		** are referenced from the field list so that
		** we don't double free them
		*/
		curField = query->fieldHead;
		while(curField)
		{
			curField->value = NULL;
			curField = curField->next;
		}
	}
	curValue = query->insertValHead;
	while(curValue)
	{
		tmpValue = curValue;
		curValue = curValue->next;
		parseFreeValue(tmpValue->value);
		tmpValue->next = NULL;
		memFreeValList(tmpValue);
	}


	/*
	** blow away the field list from the query
	*/
	_cleanFields(query->fieldHead);

	/*
	** Blow away the condition list from the query (recurse for
	** subconds)
	*/
	_cleanCond(query->condHead);

	/*
	** Blow away the order list from the query
	*/
	curOrder = query->orderHead;
	while(curOrder)
	{
		curOrder->dir = 0;
		tmpOrder = curOrder;
		curOrder = curOrder->next;
		tmpOrder->next = NULL;
		memFreeOrder(tmpOrder);
	}


	/*
	** Anything else need to be freed?
	*/

	if (query->targetTable)
	{
		free(query->targetTable);
		query->targetTable = NULL;
	}

	/*
	** Reset the list pointers
	*/
	sysvarResetVariables();
	query->fieldHead = NULL;
	query->condHead = NULL;
	query->tableHead = NULL;
	query->orderHead = NULL;
	query->insertValHead = NULL;
	memFreeQuery(query);
	debugTrace(TRACE_OUT,"parseClean()");
}



mIdent_t *parseCreateIdent(seg1,seg2,query)
	char		*seg1,
			*seg2;
	mQuery_t	*query;
{
	mIdent_t	*new;

	debugTrace(TRACE_IN,"parseCreateIdent()");
	if (seg1)
	{
		if ((int)strlen(seg1) > NAME_LEN)
		{
			netError(query->clientSock,
				"Identifier name '%s' too long\n",seg1);
			debugTrace(TRACE_OUT,"parseCreateIdent()");
			return(NULL);
		}
	}
	if (seg2)
	{
		if ((int)strlen(seg2) > NAME_LEN)
		{
			netError(query->clientSock, 
				"Identifier name '%s' too long\n",seg2);
			debugTrace(TRACE_OUT,"parseCreateIdent()");
			return(NULL);
		}
	}
	new = memMallocIdent();
	if (seg1)
	{
		(void)strncpy(new->seg1,seg1,NAME_LEN);
	}
	else
	{
		*(new->seg1) = 0;
	}
	if (seg2)
	{
		(void)strncpy(new->seg2,seg2, NAME_LEN);
	}
	else
	{
		*(new->seg1) = 0;
	}
	debugTrace(TRACE_OUT,"parseCreateIdent()");
	return(new);
}



mVal_t *parseCreateValue(textRep,type,tokLen)
	u_char	*textRep;
	int	type,
		tokLen;
{
	mVal_t	*new;
	int	length,
		remain,
		escCount;
	register u_char	*cp,
			*cp2;
	char	*cp3;

	debugTrace(TRACE_IN,"parseCreateValue()");

	new = memMallocValue();
	new->type = type;
	new->dataLen = tokLen;
	new->nullVal = 0;
	new->precision = 0;
	new->val.charVal = NULL;
	switch(type)
	{
		case NULL_TYPE:
			new->nullVal = 1;
			break;
		case IDENT_TYPE:
		case SYSVAR_TYPE:
			new->val.identVal = (mIdent_t *)textRep;
			break;
		case CHAR_TYPE:
			remain = length = tokLen - 2;
			escCount = 0;
			new->val.charVal = (u_char *)malloc(length+1);
			cp = textRep+1;
			cp2 = new->val.charVal;
			while(remain)
			{
				if (*cp == '\\')
				{
					escCount ++;
					remain--;
					*cp2 = expandEscape(++cp,remain);
					if (*cp2)
					{
						cp2++;
						cp++;
						remain--;
					}
				}
				else
				{
					*cp2++ = *cp++;
					remain--;
				}
			}
			*cp2 = 0;
			new->dataLen = tokLen - 2 - escCount;
			break;

		case INT8_TYPE:
		case INT16_TYPE:
		case INT32_TYPE:
		case INT64_TYPE:
			new->type = INT64_TYPE;
			sscanf((char*)textRep, "%lld", &new->val.int64Val);
			break;
#ifdef NOTDEF
			new->val.intVal=local_strtoul((char *)textRep,NULL,10,
				&res);
#ifdef HUGE_T
			snprintf(tmpIntBuf, sizeof(tmpIntBuf), "%d",
				new->val.intVal);
			if (res < 0 || strcmp(tmpIntBuf, (char*)textRep) != 0)
			{
				new->val.intVal = 0;
				new->type = INT64_TYPE;
				sscanf((char*)textRep, "%llu",
					&new->val.int64Val);
			}
#endif
#endif

		case UINT8_TYPE:
		case UINT16_TYPE:
		case UINT32_TYPE:
		case UINT64_TYPE:
			new->type = UINT64_TYPE;
			sscanf((char*)textRep, "%llu", &new->val.int64Val);
			break;

#ifdef NOTDEF
			new->val.intVal=local_strtoul((char *)textRep,NULL,10,
				&res);
#ifdef HUGE_T
			snprintf(tmpIntBuf, sizeof(tmpIntBuf), "%u",
				new->val.intVal);
			if (res < 0 || strcmp(tmpIntBuf, (char*)textRep) != 0)
			{
				new->val.intVal = 0;
				new->type = UINT64_TYPE;
				strtohuge((char *)textRep, &new->val.int64Val);
			}
#endif
#endif
			break;

		case REAL_TYPE:
			sscanf((char *)textRep ,"%lg",&new->val.realVal);
			cp3 = (char *)index((char*)textRep,'.');
			if (cp3)
			{
				new->precision = strlen((char*)textRep) -
					(cp3 - (char *)textRep) - 1;
				if (new->precision < 0)
					new->precision = 0;
			}
			break;
	}
	debugTrace(TRACE_OUT,"parseCreateValue()");
	return(new);
}


mVal_t *parseFillValue(val,type,length)
	char	*val;
	int	type,
		length;
{
	mVal_t	*new;

	debugTrace(TRACE_IN,"parseFillValue()");

	new = memMallocValue();
	new->type = type;
	new->nullVal = 0;
	switch(typeBaseType(type))
	{
		case CHAR_TYPE:
			new->val.charVal = (u_char *)malloc(length+1);
			bcopy(val,new->val.charVal,length);
			*(new->val.charVal + length) = 0;
			new->dataLen = strlen((char*)new->val.charVal);
			break;

		case INT8_TYPE:
		case UINT8_TYPE:
			bcopy(val, &new->val.int8Val, 1);
			new->dataLen = length;
			break;

		case INT16_TYPE:
		case UINT16_TYPE:
			bcopy(val, &new->val.int16Val, 2);
			new->dataLen = length;
			break;

		case INT32_TYPE:
		case UINT32_TYPE:
			new->val.int32Val = (int) * (int *)val;
			new->dataLen = length;
			break;

		case INT64_TYPE:
		case UINT64_TYPE:
			new->val.int64Val = (int64_t) * (int64_t *)val;
			new->dataLen = length;
			break;

		case REAL_TYPE:
			new->val.realVal = (double) * (double *)val;
			new->dataLen = length;
			break;

		case BYTE_TYPE:
			if (length != typeFieldSize(type))
			{
				fprintf(stderr,
					"parseFillValue : Size mismatch\n");
				exit(1);
			}
			new->dataLen = length;
			new->val.byteVal = (void*)malloc(length);
			bcopy((char *)val, (char *)new->val.byteVal, length);
			break;

		default:
			fprintf(stderr,"parseFillValue : Unknown type %d\n",
				typeBaseType(type));
			exit(1);
	}
	debugTrace(TRACE_OUT,"parseFillValue()");
	return(new);
}



int parseCopyValue(cp,value,type,length,nullOK)
	char	*cp;
	mVal_t	*value;
	int	type,
		length,
		nullOK;
{
	int	strLen;

	if (value->nullVal)
	{
		if (!nullOK)
		{
			return(-1);
		}
		else
		{
			bzero(cp,length);
			return(0);
		}
	}

	switch(typeBaseType(type))
	{
		case INT8_TYPE:
    		case UINT8_TYPE:
			bcopy(&(value->val.int8Val),cp,1);
			break;

		case INT16_TYPE:
    		case UINT16_TYPE:
			bcopy(&(value->val.int16Val),cp,2);
			break;

		case INT32_TYPE:
    		case UINT32_TYPE:
			bcopy(&(value->val.int32Val),cp,4);
			break;

		case INT64_TYPE:
		case UINT64_TYPE:
			bcopy(&(value->val.int64Val),cp,8);
			break;

		case REAL_TYPE:
			bcopy(&(value->val.realVal),cp,8);
			break;

		case CHAR_TYPE:
			value->val.charVal[value->dataLen]=0;
			strLen = strlen((char *)value->val.charVal);
			if (strLen > length)
				strLen = length;
			else
				strLen++;
			
			bcopy((char *)value->val.charVal,cp, strLen);
			break;

		case BYTE_TYPE:
			strLen = typeFieldSize(type);
			if (strLen != length)
			{
				fprintf(stderr,
					"parseFillValue : Size mismatch\n");
				exit(1);
			}
			bcopy((char *)value->val.byteVal,cp, strLen);
			break;
	}
	return(0);
}


mVal_t *parseCreateNullValue()
{
	mVal_t	*new;

	new = memMallocValue();
	new->nullVal = 1;
	return(new);
}



void parseFreeValue(val)
	mVal_t	*val;
{
	debugTrace(TRACE_IN,"parseFreeValue()");
	if (!val)
	{
		debugTrace(TRACE_OUT,"parseFreeValue()");
		return;
	}
	if (!val->nullVal)
	{
		switch(val->type)
		{
			case IDENT_TYPE:
			case SYSVAR_TYPE:
				if (val->val.identVal)
				{
					memFreeIdent(val->val.identVal);
					val->val.identVal = NULL;
				}
				break;
			case CHAR_TYPE:
			case TEXT_TYPE:
				if (val->val.charVal)
				{
					(void)free(val->val.charVal);
					val->val.charVal = NULL;
				}
				break;

			case DATETIME_TYPE:
			case MILLIDATETIME_TYPE:
			case CIDR4_TYPE:
			case IPV6_TYPE:
			case CIDR6_TYPE:
				if (val->val.byteVal)
				{
					(void)free(val->val.byteVal);
					val->val.byteVal = NULL;
				}
				break;
		}
	}
	memFreeValue(val);
	debugTrace(TRACE_OUT,"parseFreeValue()");
}





void parseAddSequence(table, step, val, query)
	char	*table;
	int	step,
		val;
	mQuery_t *query;
{
	debugTrace(TRACE_IN,"parseAddSequence()");
	strcpy(query->sequenceDef.table,table);
	query->sequenceDef.step = step;
	query->sequenceDef.value = val;
	debugTrace(TRACE_OUT,"parseAddSequence()");
}



/****************************************************************************
** 	_msqlAddField - add a field definition to the list
**
**	Purpose	: store field details from the query for later use
**	Args	: field name, field type, field length, value
**	Returns	: Nothing
**	Notes	: Depending on the query in process, only some of the
**		  args will be supplied.  eg. a SELECT doesn't use the
**		  type arg.  The length arg is only used during a create
**		  if the field is of type CHAR
*/

int parseAddField(ident,type,length,notNull,priKey,query)
	mIdent_t	*ident;
	int 		type;
	char		*length;
	int		notNull,
			priKey;
	mQuery_t	*query;
{
	mField_t	*new;
	char		*name,
			*table;

	debugTrace(TRACE_IN,"ParseAddField()");

	name = ident->seg2;
	table = ident->seg1;


	/*
	** Look for duplicate field names on a table create.  If the
	** type is set then we know this is a table creation.
	*/
	if (type != 0)
	{
		new = query->fieldHead;
		while(new)
		{
			if (strcmp(new->name,name) == 0)
			{
				netError(query->clientSock,
					"Duplicate field name '%s'\n", name);
				debugTrace(TRACE_OUT,"parseAddField()");
				memFreeIdent(ident);
				return(-1);
			}
			new = new->next;
		}
	}

	/*
	** Look for the obsolete prinary key construct
	*/
	if (priKey)
	{
		netError(query->clientSock,
			"Primary keys are obsolete.  Use CREATE INDEX\n");
		memFreeIdent(ident);
		debugTrace(TRACE_OUT,"parseAddField()");
		return(-1);
	}


	/*
	** Create the new field definition
	*/

	new = memMallocField();

	if (table)
	{
		(void)strncpy(new->table,table,NAME_LEN - 1);
	}
	(void)strncpy(new->name,name,NAME_LEN - 1);
	if (notNull)
	{
		new->flags |= NOT_NULL_FLAG;
	}
	if (type > 0)
	{
		switch(typeBaseType(type))
		{
			case INT8_TYPE:
			case UINT8_TYPE:
				new->type = type;
				new->dataLength = new->length = 1;
				break;

			case INT16_TYPE:
			case UINT16_TYPE:
				new->type = type;
				new->dataLength = new->length = 2;
				break;

			case INT32_TYPE:
			case UINT32_TYPE:
				new->type = type;
				new->dataLength = new->length = 4;
				break;

			case INT64_TYPE:
			case UINT64_TYPE:
				new->type = type;
				new->dataLength = new->length = 8;
				break;
	
			case CHAR_TYPE:
				new->type = CHAR_TYPE;
				new->dataLength = new->length = atoi(length);
				if (new->length > MSQL_MAX_CHAR)
				{
                                	netError(query->clientSock,
                                        "CHAR field'%s' too large.\n", name);
                                	debugTrace(TRACE_OUT,"parseAddField()");
                                	memFreeIdent(ident);
                                	return(-1);
				}
				break;
	
			case REAL_TYPE:
				new->type = REAL_TYPE;
				new->length = sizeof(double);
				new->dataLength = new->length + 1;
				break;
	
			case TEXT_TYPE:
				new->type = TEXT_TYPE;
				new->length = atoi(length);
				new->dataLength = new->length + VC_HEAD_SIZE;
				break;

			case BYTE_TYPE:
				new->type = type;
				new->dataLength = new->length = 
					typeFieldSize(type);
				break;

			default:
				new->type = 0;
				new->dataLength = new->length = 0;
				break;
		}
	}
	else
	{
		new->type = 0;
		new->dataLength = new->length = 0;
	}
	new->overflow = NO_POS;
	if (!query->fieldHead)
	{
		query->fieldHead = query->fieldTail = new;
	}
	else
	{
		query->fieldTail->next = new;
		query->fieldTail = new;
	}
	memFreeIdent(ident);
	query->fieldCount++;
	if (query->fieldCount > MAX_FIELDS)
	{
		netError(query->clientSock, TABLE_WIDTH_ERROR, MAX_FIELDS);
		msqlDebug1(MOD_ERR,"Too many fields in table (%d Max)\n",
                        MAX_FIELDS);
		debugTrace(TRACE_OUT,"parseAddField()");
		return(-1);
	}
	debugTrace(TRACE_OUT,"parseAddField()");
	return(0);
}


int parseAddFunction(name, query)
	char		*name;
	mQuery_t	*query;
{
	mField_t	*new;

	debugTrace(TRACE_IN,"ParseAddFunction()");

	/*
	** Create the new field definition
	*/

	new = memMallocField();
	new->overflow = NO_POS;

	new->function = (mFinfo_t *)malloc(sizeof(mFinfo_t));
	bzero(new->function, sizeof(mFinfo_t));
	(void)strncpy(new->function->name,name,NAME_LEN - 1);
	new->function->sequence = query->functSeq++;
	if (!query->fieldHead)
	{
		query->fieldHead = query->fieldTail = new;
	}
	else
	{
		query->fieldTail->next = new;
		query->fieldTail = new;
	}
	query->fieldCount++;

	debugTrace(TRACE_OUT,"parseAddFunction()");
	return(0);
}


int parseAddFunctParam(param, query)
	mIdent_t	*param;
	mQuery_t	*query;
{
	mField_t	*new,
			*funct;
	char		*name,
			*table;


	debugTrace(TRACE_IN,"ParseAddFunctParam()");
	
	funct = query->fieldTail;

        if (*(param->seg2))
        {       
                name = param->seg2;
                table = param->seg1;
        }
        else
        {
                name = param->seg1;
                table = NULL;
        }
	new = memMallocField();
	if (table)
	{
		(void)strncpy(new->table,table,NAME_LEN - 1);
	}
	(void)strncpy(new->name,name,NAME_LEN - 1);
	new->overflow = NO_POS;
	if (!funct->function->paramHead)
	{
		funct->function->paramHead = 
			funct->function->paramTail = new;
	}
	else
	{
		funct->function->paramTail->next = new;
		funct->function->paramTail = new;
	}
	memFreeIdent(param);
	debugTrace(TRACE_OUT,"ParseAddFunctParam()");
	return(0);
}



int parseAddFunctLiteral(value, query)
	mVal_t		*value;
	mQuery_t	*query;
{
	mField_t	*new,
			*funct;

	debugTrace(TRACE_IN,"ParseAddFunctLiteral()");
	
	funct = query->fieldTail;

	new = memMallocField();
	if (!funct->function->paramHead)
	{
		funct->function->paramHead = funct->function->paramTail = new;
	}
	else
	{
		funct->function->paramTail->next = new;
		funct->function->paramTail = new;
	}

	new->value = value;
	new->literalParamFlag = 1;
	debugTrace(TRACE_OUT,"ParseAddFunctLiteral()");
	return(0);
}



void parseSetFunctOutputName(name, query)
	char		*name;
	mQuery_t	*query;
{
	mField_t	*funct;
	char		*cp;

	debugTrace(TRACE_IN,"ParseSetFunctOutputName()");

	/*
	** The name is a text literal from the parser so we
	** have to dodge the quotes
	*/
	funct = query->fieldTail;
	cp = name + strlen(name) - 1;
	*cp = 0;
	strncpy(funct->function->outputName, name + 1, NAME_LEN);
}



void parseSetRowLimit(value, query)
	mVal_t		*value;
	mQuery_t	*query;
{
	query->rowLimit = value->val.int64Val;
}



void parseSetRowOffset(value, query)
	mVal_t		*value;
	mQuery_t	*query;
{
	query->rowOffset = value->val.int64Val;
}



void parseSetTargetTable(value, query)
	char		*value;
	mQuery_t	*query;
{
	if (value)
		query->targetTable = value;
	else
		query->targetTable = NULL;
}


void parseAddFieldValue(value, query)
	mVal_t		*value;
	mQuery_t	*query;
{
	register 	mField_t *fieldVal;
	u_char		*buf;

	debugTrace(TRACE_IN,"parseAddFieldValue()");
	if (!query->lastField)
	{
		query->lastField = fieldVal = query->fieldHead;
	}
	else
	{	
		fieldVal = query->lastField->next;
		query->lastField = query->lastField->next;
	}
	if (fieldVal)
	{
		if (fieldVal->type == CHAR_TYPE && value->nullVal == 0) 
		{
			/*
			** Just in case we have a 10 char string in
			** a 10 char space (i.e. no room for the null
			** termination) remalloc this field and ensure
			** there's room for a null
			*/
			buf = (u_char *)malloc(fieldVal->length+1);
			if (value->val.charVal)
			{
				strncpy((char*)buf,(char*)value->val.charVal,
					fieldVal->length);
				free(value->val.charVal);
			}
			else
			{
				*buf = 0;
			}
			value->val.charVal = buf;
		}
		fieldVal->value = value;
	}
	debugTrace(TRACE_OUT,"parseAddFieldValue()");
}


void parseAddInsertValue(value, query)
	mVal_t		*value;
	mQuery_t	*query;
{
	mValList_t 	*new;

	debugTrace(TRACE_IN,"parseAddInsertValue()");
	new = memMallocValList();
	new->value = value;
	new->next = NULL;
	new->offset = insertOffset++;
	if (query->insertValTail)
	{
		query->insertValTail->next = new;
	}
	else
	{
		query->insertValHead = new;
	}
	query->insertValTail = new;
	debugTrace(TRACE_OUT,"parseAddInsertValue()");
}



void parsePushCondition(query)
	mQuery_t	*query;
{
	cstk_t	*new;

	new = (cstk_t *)malloc(sizeof(cstk_t));
	new->next = condStack;
	new->head = query->condHead;
	new->tail = query->condTail;
	condStack = new;
	query->condHead = query->condTail = NULL;
}


void parsePopCondition(query)
	mQuery_t	*query;
{
	cstk_t	*tmp;

	tmp = condStack;
	condStack = condStack->next;
	query->condHead = tmp->head;
	query->condTail = tmp->tail;
	free(tmp);
}


void parseAddSubCond(bool, query)
	int		bool;
	mQuery_t	*query;
{
	mCond_t		*new = NULL;

	debugTrace(TRACE_IN,"parseAddSubCond()");

	/*
	** If there's only one cond in the sub-cond then we can
	** pull it up to the level above.  A lot of benchmark app's
	** use single entry sub-conds (as do badly written SQL queries)
	*/
	if (query->condHead->next == NULL)
	{
		new = query->condHead;
		parsePopCondition(query);
		new->next = NULL;
		new->bool = bool;
	}
	else
	{
		/*
		** Nope, it's a valid sub-cond.  Set it up
		*/
		new = memMallocCondition();
		new->op = 0;
		new->bool = bool;
		new->value = NULL;
		new->next = NULL;
		new->subCond = query->condHead;
		parsePopCondition(query);
	}

	if (!query->condHead)
	{
		query->condHead = query->condTail = new;
	}
	else
	{
		query->condTail->next = new;
		query->condTail = new;
	}
	debugTrace(TRACE_OUT,"parseAddSubCond()");
}


/****************************************************************************
**      _msqlAddBetween
**
**      Purpose :
**      Args    :
**      Returns : Nothing
**      Notes   : the BOOL field is only provided if this is not the first
**                element of a where_clause.
*/      

void parseAddBetween(ident,minVal,maxVal,bool,query)
        mIdent_t 	*ident;
        mVal_t   	*minVal,
                	*maxVal;
        int     	bool;
	mQuery_t	*query;
{
        register 	mCond_t *new;   
        char    	*name,
                	*table;

        debugTrace(TRACE_IN,"parseAddBetween()");

        if (*(ident->seg2))
        {       
                name = ident->seg2;
                table = ident->seg1;
        }
        else
        {
                name = ident->seg1;
                table = NULL;
        }

        new = memMallocCondition();
        (void)strcpy(new->name,name);
        if (table)
        {
                (void)strcpy(new->table,table);
        }

        new->op = BETWEEN_OP;
        new->bool = bool;
        new->value = minVal;
        new->maxValue = maxVal;
        new->subCond = NULL;
        new->next = NULL;

        if (!query->condHead)
        {
                query->condHead = query->condTail = new;
        }
        else
        {
                query->condTail->next = new;
                query->condTail = new;
        }
        memFreeIdent(ident);
        debugTrace(TRACE_OUT,"parseAddBetween()");
}


/****************************************************************************
** 	_msqlAddCond  -  add a conditional spec to the list
**
**	Purpose	: Store part of a "where_clause" for later use
**	Args	: field name, test op, value, bool (ie. AND | OR)
**	Returns	: Nothing
**	Notes	: the BOOL field is only provided if this is not the first
**		  element of a where_clause.
*/

int parseAddCondition(ident,op,value,bool,query)
	mIdent_t	*ident;
	int		op;
	mVal_t		*value;
	int		bool;
	mQuery_t	*query;
{
	mCond_t		*new;
	char		*name,
			*table;

	debugTrace(TRACE_IN,"parseAddCondition()");
	if (value == NULL)
		abort();

	query->numConds++;
	if (query->numConds > MAX_FIELDS)
	{
		memFreeIdent(ident);
		netError(query->clientSock, "Too many conditions in query");
		debugTrace(TRACE_OUT,"parseAddCondition()");
		return(-1);
	}

	if (*(ident->seg2))
	{
		name = ident->seg2;
		table = ident->seg1;
	}
	else
	{
		name = ident->seg1;
		table = NULL;
	}

	new = memMallocCondition();
	(void)strcpy(new->name,name);
	if (table)
	{
		(void)strcpy(new->table,table);
	}
	
	new->op = op;
	new->bool = bool;
	new->value = value;
	new->subCond = NULL;
	new->next = NULL;

	if (!query->condHead)
	{
		query->condHead = query->condTail = new;
	}
	else
	{
		query->condTail->next = new;
		query->condTail = new;
	}
	memFreeIdent(ident);
	debugTrace(TRACE_OUT,"parseAddCond()");
	return(0);
}



/****************************************************************************
** 	_msqlAddOrder  -  add an order definition to the list
**
**	Purpose	: Store part of an "order_clause"
**	Args	: field name, order direction (ie. ASC or DESC)
**	Returns	: Nothing
**	Notes	: 
*/

void parseAddOrder(ident,dir, query)
	mIdent_t	*ident;
	int		dir;
	mQuery_t	*query;
{
	register 	mOrder_t *new;

	debugTrace(TRACE_IN,"parseAddOrder()");

	new = memMallocOrder();
	new->dir = dir;
	if (*ident->seg1)
	{
		(void)strcpy(new->table,ident->seg1);
	}
	(void)strcpy(new->name,ident->seg2);
	if (!query->orderHead)
	{
		query->orderHead = query->orderTail = new;
	}
	else
	{
		query->orderTail->next = new;
		query->orderTail = new;
	}
	memFreeIdent(ident);
	debugTrace(TRACE_OUT,"parseAddOrder()");
}




int parseAddTable(name,alias,query)
	char	*name,
		*alias;
	mQuery_t *query;
{
	register mTable_t	*new;
	char			*badName = NULL;

	debugTrace(TRACE_IN,"parseAddTable()");

	if (strlen(name) > NAME_LEN)
		badName = name;
	if (alias && strlen(alias) > NAME_LEN)
		badName = alias;

	if (badName)
	{	
		netError(query->clientSock,
			"Identifier name '%s' too long\n",badName);
		debugTrace(TRACE_OUT,"parseAddTable()");
		return(-1);
	}
	new = memMallocTable();

	if (alias)
	{
		(void)strcpy(new->name,alias);
		(void)strcpy(new->cname,name);
	}
	else
	{
		(void)strcpy(new->name,name);
		*(new->cname) = 0;
	}
	if (!query->tableHead)
	{
		query->tableHead = query->tableTail = new;
	}
	else
	{
		query->tableTail->next = new;
		query->tableTail = new;
	}
	debugTrace(TRACE_OUT,"parseAddTable()");
	return(0);
}



int parseAddIndex(name, table, uniq, type, query)
	char	*name,
		*table;
	int	uniq,
		type;
	mQuery_t *query;
{
	char	*badName = NULL;

	debugTrace(TRACE_IN,"parseAddIndex()");
	if ((int)strlen(name) > NAME_LEN)
		badName = name;
	if ((int)strlen(table) > NAME_LEN)
		badName = table;

	if (badName)
	{	
		netError(query->clientSock,
			"Identifier name '%s' too long\n",badName);
		debugTrace(TRACE_OUT,"parseAddIndex()");
		return(-1);
	}
	strncpy(query->indexDef.name,name,NAME_LEN);
	strncpy(query->indexDef.table,table,NAME_LEN);
	query->indexDef.unique = uniq;
	query->indexDef.idxType = type;
	debugTrace(TRACE_OUT,"parseAddIndex()");
	return(0);
}


void parseSetInsertOffset(value)
	int	value;
{
	insertOffset = value;
}




#ifdef NOT_DEF
static void parseQueryOverrunError(txt, query)
	char		*txt;
	mQuery_t	*query;
{

	debugTrace(TRACE_IN,"parseQueryOverrunError()");
	netError(query->clientSock,
		"Syntax error.  Bad text after query. '%s'\n",txt);
	debugTrace(TRACE_OUT,"parseQueryOverrunError()");
}
#endif


mQuery_t *parseQuery(server, inBuf, sock, user, db)
	msqld		*server;
	char		*inBuf;
	int		sock;
	char		*user,
			*db;
{
	mQuery_t	*query;
	extern mQuery_t *curQuery; /* from lexer.c */

	debugTrace(TRACE_IN,"parseQuery()");
	query = memMallocQuery();
	query->clientSock = sock;
	query->curUser = user;
	query->curDB = db;
	yyInitScanner((u_char*)inBuf);
	curQuery = query;
	curQuery->queryText = inBuf;
	if (yyparse() != 0)
	{
		debugTrace(TRACE_OUT,"parseQuery()");
		return(NULL);
	}
	debugTrace(TRACE_OUT,"parseQuery()");
	return(query);
}
