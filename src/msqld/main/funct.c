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
** $Id: funct.c,v 1.13 2003/01/03 05:30:15 bambi Exp $
**
*/

/*
** Module	: 
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

#include <ctype.h>

#include <libmsql/msql.h>
#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/main.h>
#include <msqld/main/funct.h>
#include <msqld/main/net.h>
#include <msqld/main/regex.h>
#include <msqld/main/memory.h>
#include <msqld/main/parse.h>
#include <common/types/types.h>



/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

extern char	errMsg[];

/* Declare the functions we will be defining */

static int _functUpper();
static int _functLower();
static int _functLength();
static int _functSubstr();
static int _functChop();
static int _functTranslate();
static int _functReplace();
static int _functSoundex();

static int _functAbs();
static int _functCeil();
static int _functFloor();
static int _functMod();
static int _functSign();
static int _functPower();

static int _functCidr4Len();

/* Function definition table */

static	mFunct_t functions[] = {

	/* String related value functions */

	{ "upper", _functUpper, FUNCT_VALUE, CHAR_TYPE, 1, { CHAR_TYPE }},
	{ "lower", _functLower, FUNCT_VALUE, CHAR_TYPE, 1, { CHAR_TYPE }},
	{ "length", _functLength, FUNCT_VALUE, INT64_TYPE, 1, { CHAR_TYPE }},
	{ "substr", _functSubstr, FUNCT_VALUE, CHAR_TYPE, 3, 
		{ CHAR_TYPE, INT64_TYPE, INT64_TYPE }},
	{ "chop", _functChop, FUNCT_VALUE, CHAR_TYPE, 1, { CHAR_TYPE }},
	{ "translate", _functTranslate, FUNCT_VALUE, CHAR_TYPE, 3, 
		{ CHAR_TYPE, CHAR_TYPE, CHAR_TYPE }},
	{ "replace", _functReplace, FUNCT_VALUE, CHAR_TYPE, 3, 
		{ CHAR_TYPE, CHAR_TYPE, CHAR_TYPE }},
	{ "soundex", _functSoundex, FUNCT_VALUE, INT64_TYPE, 1, { CHAR_TYPE }},

	/* Number related value functions */

	{ "abs", _functAbs, FUNCT_VALUE, INT64_TYPE, 1, { INT64_TYPE }},
	{ "ceil", _functCeil, FUNCT_VALUE, INT64_TYPE, 1, { REAL_TYPE }},
	{ "floor", _functFloor, FUNCT_VALUE, INT64_TYPE, 1, { REAL_TYPE }},
	{ "mod", _functMod, FUNCT_VALUE, INT64_TYPE, 2, { INT64_TYPE, INT64_TYPE }},
	{ "sign", _functSign, FUNCT_VALUE, INT64_TYPE, 1, { INT64_TYPE }},
	{ "power", _functPower, FUNCT_VALUE, INT64_TYPE, 2, 
		{ INT64_TYPE, INT64_TYPE }},

	/* CIDR related value functions */

	{ "cidr4_length",_functCidr4Len,FUNCT_VALUE,INT64_TYPE,1,{ CIDR4_TYPE }},
	/* End of function list */

	{ NULL }
};

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/****
** CHAR / TEXT field related value functions
****/

static int _functUpper(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	char	*cp;

	value->val.charVal = (u_char*)strdup((char*)params->value->val.charVal);
	value->nullVal = 0;
	cp = (char*)value->val.charVal;
	while(*cp)
	{
		*cp = toupper(*cp);
		cp++;
	}
	return(0);
}


static int _functLower(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	char	*cp;

	value->val.charVal = (u_char*)strdup((char*)params->value->val.charVal);
	value->nullVal = 0;
	cp = (char*)value->val.charVal;
	while(*cp)
	{
		*cp = tolower(*cp);
		cp++;
	}
	return(0);
}

static int _functLength(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int	len;

	len =  strlen((char*)params->value->val.charVal);
	value->val.int64Val = len;
	value->nullVal = 0;
	return(0);
}



static int _functSubstr(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	char	*str,
		*new;
	int	start,
		length;

	str =  (char*)params->value->val.charVal;
	start =  params->next->value->val.int64Val;
	length =  params->next->next->value->val.int64Val;

	if ( (start < 0) ||
	     (length <= 0) )
	{
		return(-1);
	}
	new = (char *)malloc(length+1);
	(void)bzero(new, length+1);
	strncpy(new, str + start, length);
	value->val.charVal = (u_char*)new;
	value->nullVal = 0;
	return(0);
}

	
static int _functChop(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	char	*str,
		*new;

	str =  (char*)params->value->val.charVal;
	new = (char *)strdup(str);
	new[strlen(new) - 1]=0;
	value->val.charVal = (u_char*)new;
	value->nullVal = 0;
	return(0);
}



static char *_trSpec(spec)
	char	*spec;
{
	char	res[4096];
	register char	*cp1,
			*cp2,
			c;
			

	bzero(res,4096);
	cp1 = spec;
	cp2 = res;
	while(*cp1)
	{
		if (*cp1 == '-')
		{
			if (cp1 == spec)
			{
				return(NULL);
			}
			if (*(cp1+1) < (*cp1-1))
			{
				return(NULL);
			}
			c = *(cp1-1);
			while(c < *(cp1+1))
			{
				c++;
				*cp2++ = c;
			}
			cp1+=2;
			continue;
		}
		*cp2++ = *cp1++;
	}
	return((char *)strdup(res));
}



	
static int _functReplace(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	char	*spec1,
		*spec2,
		*str,
		*buf,
		*oldBuf;
	register char	*cpb,
			*cp;
	int	len,
		curLen,
		spec1Len,
		spec2Len;

	str =  (char*)params->value->val.charVal;
	spec1 =  (char*)params->next->value->val.charVal;
	spec2 =  (char*)params->next->next->value->val.charVal;

	curLen = strlen(str);
	spec1Len = strlen(spec1);
	spec2Len = strlen(spec2);
	len = 0;

	buf = (char *)malloc(curLen+1);
	(void)bzero(buf,curLen);
	cpb = buf;
	cp = str;
	while (*cp)
	{
		if (*cp != *spec1)
		{
			*cpb++ = *cp++;	
			len++;
			continue;
		}
		if (strncmp(cp,spec1,spec1Len) != 0)
		{
			*cpb++ = *cp++;	
			len++;
			continue;
		}
		len += spec2Len ;
		if (spec2Len > spec1Len)
		{
			oldBuf = buf;
			buf = (char *)realloc(buf, curLen + 
				(spec2Len-spec1Len)+1);
			cpb = buf + (cpb - oldBuf);
			curLen += (spec2Len-spec1Len)+1;
		}
		strcpy(cpb,spec2);
		cpb += spec2Len;
		cp += spec1Len;
	}
	*cpb = 0;
	value->val.charVal = (u_char*)buf;
	value->nullVal = 0;
	return(0);
}


static int _functTranslate(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	char	*trans1,
		*trans2,
		*spec1,
		*spec2,
		*str;
	register char	*cps,
			*cp;

	str =  (char*)params->value->val.charVal;
	spec1 =  (char*)params->next->value->val.charVal;
	spec2 =  (char*)params->next->next->value->val.charVal;

	value->val.charVal = (u_char*)strdup(str);
	value->nullVal = 0;
	if((trans1 = _trSpec(spec1)) == NULL)
	{
		return(0);
	}
	if((trans2 = _trSpec(spec2)) == NULL)
	{
		free(trans1);
		return(0);
	}
	cps = (char*)value->val.charVal;
	while(*cps)
	{
		cp = trans1;
		while(*cp)
		{
			if (*cp == *cps)
			{
				*cps = *(char *)(trans2 + (cp-trans1));
				break;
			}
			cp++;
		}
		cps++;
	}
	free(trans1);
	free(trans2);
	return(0);
}


static int _functSoundex(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int	val;

	val =  soundex((char*)params->value->val.charVal);
	value->val.int64Val = val;
	value->nullVal = 0;
	return(0);
}


/****
** NUM field related value functions
****/


static int _functAbs(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int64_t	int64;

	int64 = typeCastIntValTo64(params->value);
	if(int64 < 0)
		value->val.int64Val = 0 - int64;
	else
		value->val.int64Val = int64;
	value->nullVal = 0;
	return(0);
}


static int _functCeil(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int		res;

	res = (int)params->value->val.realVal;
	if ((double)res < params->value->val.realVal)
		res++;
	value->val.int64Val = res;
	value->nullVal = 0;
	return(0);
}


static int _functFloor(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int		res;

	res = (int)params->value->val.realVal;
	if ((double)res > params->value->val.realVal)
		res--;
	value->val.int64Val = res;
	value->nullVal = 0;
	return(0);
}


static int _functMod(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int64_t	int64_1,
		int64_2;

	int64_1 = typeCastIntValTo64(params->value);
	int64_2 = typeCastIntValTo64(params->next->value);
	value->val.int64Val = int64_1 % int64_2;
	value->nullVal = 0;
	return(0);
}


static int _functSign(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int64_t	int64;

	int64 = typeCastIntValTo64(params->value);
	if (int64  < 0)
		value->val.int64Val = -1;
	else
		value->val.int64Val = 1;
	value->nullVal = 0;
	return(0);
}


static int _functPower(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	int64_t		val,
			res,
			count;

	val = typeCastIntValTo64(params->value);
	count = typeCastIntValTo64(params->next->value);
	res = 1;
	while(count > 0)
	{
		res = res * val;
		count--;
	}
	value->val.int64Val = res;
	value->nullVal = 0;
	return(0);
}



/****/

static int _functCidr4Len(params, value)
	mField_t	*params;
	mVal_t		*value;
{
	char	*cp;

	cp = params->value->val.byteVal;
	value->val.int64Val = (u_int)*(u_char*)(cp + 4);
	value->nullVal = 0;
	return(0);
}

/****/

static void _executeValueFunction(curField)
	mField_t	*curField;
{
	mField_t	*params;
	mVal_t		*val;
	mFunct_t	*functDef;
	int		res;

	functDef = &functions[curField->function->functNum];
	params = curField->function->paramHead;
	val = memMallocValue();
	
	res = (*functDef->functPtr)(params, val);
	if (curField->value)
	{
		parseFreeValue(curField->value);
	}
	curField->value = val;
	curField->value->type = curField->type;

	/*	
	switch(curField->type)
	{
		case	INT_TYPE:
			curField->length = curField->dataLength = 4;
			break;

		case	REAL_TYPE:
			curField->length = curField->dataLength = 8;
			break;

		case	CHAR_TYPE:
			curField->type = TEXT_TYPE;
			curField->length = curField->dataLength =
				curField->length * 1.25;
			break;
	}
	*/
}



/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


int functFindFunction(query)
	mQuery_t	*query;
{
	mField_t	*curField;
	mFunct_t	*curFunct;
	int		count;

	curField = query->fieldTail;
	curField->function->functNum = -1;
	curFunct = functions;
	count = 0;
	while (curFunct->name)
	{
		if (*curFunct->name == *curField->function->name &&
			strcmp(curFunct->name,curField->function->name) == 0)
		{
			curField->function->functNum = count;
			break;
		}
		count++;
		curFunct++;
	}
	if (curField->function->functNum == -1)
	{
                netError(query->clientSock, "Unknown function '%s'",
			curField->function->name);
		return(-1);
	}
	curField->function->returnType = curFunct->returnType;
	curField->function->functType = curFunct->type;

	/*
	** Set the "host" field's name
	*/

	if (*curField->function->outputName)
	{
		strcpy(curField->name,curField->function->outputName);
	}
	else
	{
		sprintf(curField->name,"%s_%d",curField->function->name,
			curField->function->sequence);
	}

	curField->type = curField->function->returnType;
	/*
	** Set the host field length
	switch(curField->type)
       	{
	    case INT_TYPE:
               	curField->length = curField->dataLength = 4;
		break;

	    case REAL_TYPE:
		curField->length = curField->dataLength = 8;
		break;

	    case CHAR_TYPE:
		curField->length = curField->dataLength = 100;
		break;
	}
	*/

	return(0);
}



int functCheckFunctions(query)
	mQuery_t	*query;
{
	mField_t	*curField,
			*curParam;
	mFunct_t	*curFunct;
	int		count,
			error;

	curField = query->fieldHead;
	while(curField)
	{
		if (curField->function == NULL)
		{
			curField = curField->next;
			continue;
		}

		/*
		** Check out the function params
		*/
		curFunct = &functions[curField->function->functNum];
		curParam = curField->function->paramHead;
		count = 0;
		while(count < curFunct->numParams)
		{
			error = 0;
			if (curParam->literalParamFlag)
			{
				if (typeCompatibleTypes(
					curFunct->paramTypes[count],
					curParam->value->type) != 1)
				{
					error = 1;
				}
			}
			else
			{
				if (typeCompatibleTypes(
					curFunct->paramTypes[count],
					curParam->type) != 1)
				{
					error = 1;
				}
			}
			if (error)
			{
				sprintf(errMsg,
					"Invalid param type for function '%s'",
					curFunct->name);
				return(-1);
			}
			curParam = curParam->next;
			count++;
		}

		curField = curField->next;
	}
	return(0);
}



void functProcessFunctions(cacheEntry,query)
	cache_t		*cacheEntry;
	mQuery_t	*query;
{
	mField_t	*curField;

	if (cacheEntry->result == 1)
		return;

	curField = query->fieldHead;
	while(curField)
	{
		if (curField->function)
		{
			_executeValueFunction(curField);
		}
		curField = curField->next;
	}
}
