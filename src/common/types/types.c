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
** $Id: types.c,v 1.19 2012/01/15 06:19:59 bambi Exp $
**
*/

/*
** Module	: types : types
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

#include <common/portability.h>
#include <common/msql_defs.h>
#include <msqld/includes/errmsg.h>

/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/errmsg.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/sysvar.h>
#include <libmsql/msql.h>
#include "types.h"


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

#define REG     	register

#define	MAX_INT8	0x7f
#define	MAX_UINT8	0xff
#define	MAX_INT16	0x7fff
#define	MAX_UINT16	0xffff
#define	MAX_INT32	0x7fffffff
#define	MAX_UINT32	0xffffffff

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static int _checkIntValueRange(valType, value, minVal, maxVal)
	int	valType;
	mVal_t	*value;
	int	minVal,
		maxVal;
{
	if (valType ==  MONEY_TYPE)
	{
		value->type = INT32_TYPE;
		value->val.int32Val = value->val.int32Val / 100;
		valType = INT32_TYPE;
	}
	if (valType == INT8_TYPE || valType == UINT8_TYPE)
	{
		if (value->val.int8Val<=maxVal && value->val.int8Val>=minVal)
		{
			return(0);
		}
		return(-1);
	}
	if (valType == INT16_TYPE || valType == UINT16_TYPE)
	{
		if (value->val.int16Val<=maxVal && value->val.int16Val>=minVal)
		{
			return(0);
		}
		return(-1);
	}
	if (valType == INT32_TYPE || valType == UINT32_TYPE)
	{
		if (value->val.int32Val<=maxVal && value->val.int32Val>=minVal)
		{
			return(0);
		}
		return(-1);
	}

	if (valType == INT64_TYPE || valType == UINT64_TYPE ) 
	{
		if (value->val.int64Val<=maxVal  && value->val.int64Val>=minVal)
		{
			/* ??? 
			value->type = INT_TYPE;
			value->val.intVal = (int)value->val.int64Val;
			*/
			return(0);
		}
		return(-1);
	}
	return(-2);
}


static int _checkUintValueRange(valType, value, minVal, maxVal)
	int	valType;
	mVal_t	*value;
	uint	minVal,
		maxVal;
{
	if (valType ==  MONEY_TYPE)
	{
		value->type = INT32_TYPE;
		value->val.int32Val = value->val.int32Val / 100;
		valType = INT32_TYPE;
	}
	if (valType == INT8_TYPE || valType == UINT8_TYPE)
	{
		if (value->val.int8Val<=maxVal && value->val.int8Val>=minVal)
		{
			return(0);
		}
		return(-1);
	}
	if (valType == INT16_TYPE || valType == UINT16_TYPE)
	{
		if (value->val.int16Val<=maxVal && value->val.int16Val>=minVal)
		{
			return(0);
		}
		return(-1);
	}
	if (valType == INT32_TYPE || valType == UINT32_TYPE)
	{
		if (value->val.int32Val<=maxVal && value->val.int32Val>=minVal)
		{
			return(0);
		}
		return(-1);
	}

	if (valType == INT64_TYPE || valType == UINT64_TYPE ) 
	{
		if (value->val.int64Val<=maxVal  && value->val.int64Val>=minVal)
		{
			/* ??? 
			value->type = INT_TYPE;
			value->val.intVal = (int)value->val.int64Val;
			*/
			return(0);
		}
		return(-1);
	}
	return(-2);
}

/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



extern	char    errMsg[];




/**************************************************************************
** Type determination routines
*/

int typeFieldSize(type)
	int	type;
{

	/*
	** Look for a specific field length for the type
	*/
	switch(type)
	{
		case CIDR4_TYPE:
		case DATETIME_TYPE:
			return(5);

		case MILLIDATETIME_TYPE:
			return(6);

		case IPV6_TYPE:
			return(16);

		case CIDR6_TYPE:
			return(17);
	}

	/*
	** OK, we'll just use the size of the base type
	*/
	switch(typeBaseType(type))
	{
		case INT8_TYPE:
		case UINT8_TYPE:
			return(1);

		case INT16_TYPE:
		case UINT16_TYPE:
			return(2);

		case INT32_TYPE:
		case UINT32_TYPE:
			return(4);

		case INT64_TYPE:
		case UINT64_TYPE:
			return(8);

		case REAL_TYPE:
			return(8);
	}
	return(0);
}



int typeBaseType(type)
	int	type;
{
	switch(type)
	{
		case IDENT_TYPE:
			return(IDENT_TYPE);

		case CHAR_TYPE:
			return(CHAR_TYPE);

		case INT32_TYPE:
		case MONEY_TYPE:
			return(INT32_TYPE);

		case INT8_TYPE:
			return(INT8_TYPE);

		case INT16_TYPE:
			return(INT16_TYPE);

		case INT64_TYPE:
			return(INT64_TYPE);


		case UINT32_TYPE:
		case DATE_TYPE:
		case TIME_TYPE:
		case IPV4_TYPE:
		case MILLITIME_TYPE:
			return(UINT32_TYPE);

		case UINT8_TYPE:
			return(UINT8_TYPE);

		case UINT16_TYPE:
			return(UINT16_TYPE);

		case UINT64_TYPE:
			return(UINT64_TYPE);

		case REAL_TYPE:
			return(REAL_TYPE);

		case TEXT_TYPE:
			return(TEXT_TYPE);

		case CIDR4_TYPE:
		case CIDR6_TYPE:
		case IPV6_TYPE:
		case DATETIME_TYPE:
		case MILLIDATETIME_TYPE:
			return(BYTE_TYPE);

		default:
			fprintf(stderr,"\n\nERROR : Unknown type '%d'\n\n",
				type);
	}
	/* Unknown Type */
	exit(0);
}

int typeRepType(type)
	int	type;
{
	switch(type)
	{
		case CHAR_TYPE:
		case DATE_TYPE:
		case TEXT_TYPE:
		case TIME_TYPE:
		case IPV4_TYPE:
		case CIDR4_TYPE:
		case IPV6_TYPE:
		case CIDR6_TYPE:
		case DATETIME_TYPE:
		case MILLITIME_TYPE:
		case MILLIDATETIME_TYPE:
			return(CHAR_TYPE);

		case REAL_TYPE:
		case MONEY_TYPE:
			return(REAL_TYPE);

		case INT8_TYPE:
		case INT16_TYPE:
		case INT32_TYPE:
		case INT64_TYPE:
			return(INT32_TYPE);

		case UINT8_TYPE:
		case UINT16_TYPE:
		case UINT32_TYPE:
		case UINT64_TYPE:
			return(UINT32_TYPE);

		default:
			fprintf(stderr,"\n\nERROR : Unknown type '%d'\n\n",
				type);
	}
	/* Unknown Type */
	exit(0);
}


int64_t typeCastIntValTo64(value)
	mVal_t	*value;
{
	int64_t	int64;

	switch (value->type)
	{
		case INT8_TYPE:
		case UINT8_TYPE:
			int64 = value->val.int8Val;
			break;

		case INT16_TYPE:
		case UINT16_TYPE:
			int64 = value->val.int16Val;
			break;

		case INT32_TYPE:
		case UINT32_TYPE:
			int64 = value->val.int32Val;
			break;

		case INT64_TYPE:
		case UINT64_TYPE:
			int64 = value->val.int64Val;
			break;

		case MONEY_TYPE:
			int64 = value->val.int32Val / 100;
			break;

		case REAL_TYPE:
			int64 = (int64_t)value->val.realVal;
			break;
	}
	return(int64);
}


int typeCompatibleTypes(type1, type2)
	int	type1,
		type2;
{
	int	int1,
		int2;

	if (type1 == type2)
		return(1);

	int1 = int2 = 0;
	switch (type1)
	{
		case INT8_TYPE:
		case INT16_TYPE:
		case INT32_TYPE:
		case INT64_TYPE:
		case UINT8_TYPE:
		case UINT16_TYPE:
		case UINT32_TYPE:
		case UINT64_TYPE:
		case MONEY_TYPE:
		case REAL_TYPE:
			int1 = 1;
	}
	switch (type2)
	{
		case INT8_TYPE:
		case INT16_TYPE:
		case INT32_TYPE:
		case INT64_TYPE:
		case UINT8_TYPE:
		case UINT16_TYPE:
		case UINT32_TYPE:
		case UINT64_TYPE:
		case MONEY_TYPE:
		case REAL_TYPE:
			int2 = 1;
	}
	if (int1 == 1 && int2 == 1)
		return(1);
	return(0);
}


int typeCastValue(fieldType, valType, value)
	int	fieldType,
		valType;
	mVal_t	*value;
{
	/*
	** Return : 	0 if all good
	**		-1 if invalid type for cast
	**		-2 if value out of bounds
	*/

	int64_t	int64;
	int	tmpInt;

	if (valType == 0)
	{
		/* Allow the type of a sysvar to be passed in */
		valType = value->type;
	}

	if (fieldType == valType)
	{
		return(0);
	}

	if (typeCompatibleTypes(fieldType, valType) != 1)
	{
		return(-1);
	}
	int64 = typeCastIntValTo64(value);

	switch(fieldType)
	{
		case INT8_TYPE:
			if (int64 > MAX_INT8 || int64 < 0 - MAX_INT8)
				return(-2);
			value->val.int8Val = (int8_t)int64;
			break;

		case UINT8_TYPE:
			if (int64 > MAX_UINT8 || int64 < 0)
				return(-2);
			value->val.int8Val = (uint8_t)int64;
			break;

		case INT16_TYPE:
			if (int64 > MAX_INT16 || int64 < 0 - MAX_INT16)
				return(-2);
			value->val.int16Val = (int16_t)int64;
			break;

		case UINT16_TYPE:
			if (int64 > MAX_UINT16 || int64 < 0)
				return(-2);
			value->val.int16Val = (uint16_t)int64;
			break;

		case INT32_TYPE:
			if (int64 > MAX_INT32 || int64 < 0 - MAX_INT32)
				return(-2);
			value->val.int32Val = (int32_t)int64;
			break;

		case UINT32_TYPE:
			if (int64 > MAX_UINT32 || int64 < 0)
				return(-2);
			value->val.int32Val = (uint32_t)int64;
			break;


		case INT64_TYPE:
			value->val.int64Val = (int64_t)int64;
			break;

		case UINT64_TYPE:
			value->val.int64Val = (uint64_t)int64;
			break;

		case REAL_TYPE:
			value->val.realVal = (double)int64;
			break;

		default:
			return(-1);
	}

	if (value->type != IDENT_TYPE && value->type != SYSVAR_TYPE)
	{
		value->type = fieldType;
	}
	return(0);
}


int typeValidConditionTarget(fieldType, value)
	int	fieldType;
	mVal_t	*value;
{
	int	valType,
		res;

	/*
	** Check out the field type and determine if the value's type
	** is usable or if it can be cast to something we can use
	*/

	valType = value->type;
	if (valType == SYSVAR_TYPE)
	{
		valType = sysvarGetVariableType(value->val.identVal->seg2);
	}
	switch(fieldType)
	{
		case INT8_TYPE:
		case INT16_TYPE:
		case INT32_TYPE:
		case INT64_TYPE:
		case UINT8_TYPE:
		case UINT16_TYPE:
		case UINT32_TYPE:
		case UINT64_TYPE:
		case REAL_TYPE:
			res = typeCastValue(fieldType, valType, value);
			if (res == -2)
			{
				snprintf(errMsg,MAX_ERR_MSG, NUM_RANGE_ERROR);
			}
			return(res);

#ifdef NOTDEF
			if (value->type == INT8_TYPE)
		    	res = _checkIntValueRange(valType, value, -127, 127);
		    	if (res < 0)
		    	{
				if (res == -1)
				{
					snprintf(errMsg,MAX_ERR_MSG,
						NUM_RANGE_ERROR);
					return(-2);
				}
				return(-1);
		    	}
			switch(value->type)
			{
				case INT8_TYPE:
				case UINT8_TYPE:
					return(0);

				case INT16_TYPE:
				case UINT16_TYPE:
					value->val.realVal=value->val.int16Val;
					break;

				case INT32_TYPE:
				case UINT32_TYPE:
					value->val.realVal=value->val.int32Val;
					break;
				case MONEY_TYPE:
					value->val.realVal=value->val.int32Val
						/ 100;
					break;
				default:
					return(-1);
			}
			value->type = REAL_TYPE;
			return(0);
			if(value->type==INT8_TYPE || value->type==UINT8_TYPE)
			{
				value->val.realVal = value->val.int8Val;
				return(0);
			}
			if(value->type==INT16_TYPE || value->type==UINT16_TYPE)
			{
				value->val.realVal = value->val.int16Val;
				return(0);
			}
			if(value->type==INT32_TYPE || value->type==UINT32_TYPE)
			{
				value->val.realVal = value->val.int32Val;
				return(0);
			}
			if(value->type ==  MONEY_TYPE)
			{
				value->val.realVal = value->val.int32Val / 100;
				return(0);
			}
		    return(0);

		case INT16_TYPE:
			res = typeCastValue(fieldType, valType, value);
			if (res == -2)
			{
				snprintf(errMsg,MAX_ERR_MSG, NUM_RANGE_ERROR);
			}
			return(res);

		    res = _checkIntValueRange(valType, value, -32767, 32767);
		    if (res < 0)
		    {
			if (res == -1)
			{
				snprintf(errMsg, MAX_ERR_MSG, NUM_RANGE_ERROR);
				return(-2);
			}
			return(-1);
		    }
		    return(0);

		case INT32_TYPE:
			res = typeCastValue(fieldType, valType, value);
			if (res == -2)
			{
				snprintf(errMsg,MAX_ERR_MSG, NUM_RANGE_ERROR);
			}
			return(res);

		    res = _checkIntValueRange(valType, value, -2147483647, 
			2147483647);
		    if (res < 0)
		    {
			if (res == -1)
			{
				snprintf(errMsg, MAX_ERR_MSG, NUM_RANGE_ERROR);
				return(-2);
			}
			return(-1);
		    }
		    return(0);


		case UINT8_TYPE:
			res = typeCastValue(fieldType, value);
			if (res == -2)
			{
				snprintf(errMsg,MAX_ERR_MSG, NUM_RANGE_ERROR);
			}
			return(res);

		    res = _checkUintValueRange(valType, value, 0, 255);
		    if (res < 0)
		    {
			if(res == -1)
			{
				snprintf(errMsg, MAX_ERR_MSG, NUM_RANGE_ERROR);
				return(-2);
			}
			return(-1);
		    }
		    return(0);

		case UINT16_TYPE:
		    res = _checkUintValueRange(valType, value, 0, 65535);
		    if (res < 0)
		    {
			if (res == -1)
			{
				snprintf(errMsg, MAX_ERR_MSG, NUM_RANGE_ERROR);
				return(-2);
			}
			return(-1);
		    }
		    return(0);

		case UINT32_TYPE:
		    res = _checkUintValueRange(valType, value, 0, 4294967295u);
		    if ( res < 0 && valType ==  MONEY_TYPE)
		    {
			value->type = INT32_TYPE;
			value->val.int32Val /= 100;
			res = 0;
		    }
		    if (res < 0)
		    {
			if (res == -1)
			{
				snprintf(errMsg, MAX_ERR_MSG, NUM_RANGE_ERROR);
				return(-2);
			}
			return(-1);
		    }
		    return(0);

		case INT64_TYPE:
		case UINT64_TYPE:
			if (typeCastValue(fieldType,value) == 0)
				return(0);
			return(-1);

			/* XXX
			if (valType == INT64_TYPE || valType == UINT64_TYPE )
			{
				return(0);
			}
			if (valType == INT_TYPE || valType == UINT_TYPE)
			{
				value->type = INT64_TYPE;
				value->val.int64Val = value->val.int32Val;
				return(0);
			}
			if (valType == INT8_TYPE || valType == UINT8_TYPE)
			{
				value->type = INT64_TYPE;
				value->val.int64Val = value->val.int8Val;
				return(0);
			}
			if (valType == INT16_TYPE || valType == UINT16_TYPE)
			{
				value->type = INT64_TYPE;
				value->val.int64Val = value->val.int16Val;
				return(0);
			}
			if (valType ==  MONEY_TYPE)
			{
				value->type = INT64_TYPE;
				value->val.int64Val = value->val.int32Val / 100;
				return(0);
			}
			*/
			return(-1);

		case REAL_TYPE:
			if( value->type == REAL_TYPE)
			{
				return(0);
			}
			switch(value->type)
			{
				case INT8_TYPE:
				case UINT8_TYPE:
					value->val.realVal=value->val.int8Val;
					break;

				case INT16_TYPE:
				case UINT16_TYPE:
					value->val.realVal=value->val.int16Val;
					break;

				case INT32_TYPE:
				case UINT32_TYPE:
					value->val.realVal=value->val.int32Val;
					break;
				case MONEY_TYPE:
					value->val.realVal=value->val.int32Val
						/ 100;
					break;
				default:
					return(-1);
			}
			value->type = REAL_TYPE;
			return(0);
#endif

		case CHAR_TYPE:
		case TEXT_TYPE:
			if (	value->type == CHAR_TYPE ||
				value->type == TEXT_TYPE )
			{	
				return(0);
			}
			return(-1);

		case MONEY_TYPE:
			if(value->type == MONEY_TYPE )
			{
				return(0);
			}
			if ( 	value->type == INT8_TYPE ||
				value->type == INT16_TYPE ||
				value->type == INT32_TYPE || 
				value->type == INT64_TYPE || 
				value->type == UINT8_TYPE ||
				value->type == UINT16_TYPE ||
				value->type == UINT32_TYPE ||
				value->type == UINT64_TYPE || 
				value->type == REAL_TYPE )
			{
				value->val.int32Val=typeScanMoney(value,errMsg,
					MAX_ERR_MSG);
				value->type = MONEY_TYPE;
				return(0);
			}
			return(-1);

		case DATE_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for a valid date
				*/
				value->val.int32Val = 
					typeScanDate(value,errMsg,MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = DATE_TYPE;
				if (value->val.int32Val == -1)
				{
					return(-2);
				}
			}
			if (value->type == DATE_TYPE)
			{
				return(0);
			}
			return(-1);

		case DATETIME_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for a valid date
				*/
				value->val.byteVal = typeScanDateTime(value,
					errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = DATETIME_TYPE;
				if (value->val.byteVal == NULL && 
					value->nullVal == 0)
				{
					return(-2);
				}
			}
			if (value->type == DATETIME_TYPE)
			{
				return(0);
			}
			return(-1);

		case MILLIDATETIME_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for a valid date
				*/
				value->val.byteVal = typeScanMilliDateTime(
					value, errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = MILLIDATETIME_TYPE;
				if (value->val.byteVal == NULL && 
					value->nullVal == 0)
				{
					return(-2);
				}
			}
			if (value->type == MILLIDATETIME_TYPE)
			{
				return(0);
			}
			return(-1);

		case TIME_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for a valid time
				*/
				value->val.int32Val = typeScanTime(value,
					errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = TIME_TYPE;
				if (value->val.int32Val == -1)
				{
					return(-2);
				}
			}
			if (value->type == TIME_TYPE)
			{
				return(0);
			}
			return(-1);

		case MILLITIME_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for a valid time
				*/
				value->val.int32Val = typeScanMilliTime(value,
					errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = MILLITIME_TYPE;
				if (value->val.int32Val == -1)
				{
					return(-2);
				}
			}
			if (value->type == MILLITIME_TYPE)
			{
				return(0);
			}
			return(-1);

		case IPV4_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for an IPv4 addr
				*/
				value->val.int32Val = typeScanIPv4(value,
					errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = IPV4_TYPE;
				if (value->val.int32Val == -1 &&
                                        value->nullVal == 0)
				{
					return(-2);
				}
			}
			if (value->type == IPV4_TYPE)
			{
				return(0);
			}
			return(-1);

		case CIDR4_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for a CIDR prefix
				*/
				value->val.byteVal = typeScanCIDR4(value,
					errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = CIDR4_TYPE;
				if (value->val.byteVal == NULL &&
                                        value->nullVal == 0)
				{
					return(-2);
				}
			}
			if (value->type == CIDR4_TYPE)
			{
				return(0);
			}
			return(-1);

		case IPV6_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for an IPv6 addr
				*/
				value->val.byteVal = typeScanIPv6(value,
					errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = IPV6_TYPE;
				if (value->val.byteVal == NULL &&
                                        value->nullVal == 0)
				{
					return(-2);
				}
			}
			if (value->type == IPV6_TYPE)
			{
				return(0);
			}
			return(-1);

		case CIDR6_TYPE:
			if (value->type == CHAR_TYPE)
			{
				/* 
				** Scan the literal value for a CIDR prefix
				*/
				value->val.byteVal = typeScanCIDR6(value,
					errMsg, MAX_ERR_MSG);
				free(value->val.charVal);
				value->type = CIDR6_TYPE;
				if (value->val.byteVal == NULL &&
                                        value->nullVal == 0)
				{
					return(-2);
				}
			}
			if (value->type == CIDR6_TYPE)
			{
				return(0);
			}
			return(-1);

		default:
			/* Hmmm, what is this thing then ? */
			msqlDebug1(MOD_ERR, "Unknown type %d", fieldType);
			return(-1);
	}
}



char *typePrintValue(buf, len, type, value)
	char	*buf;
	int	len,
		type;	
	mVal_t	*value;
{
	char	format[10];

	switch(type)
	{
		case INT8_TYPE:
			snprintf(buf,len,"%hhd", value->val.int8Val);
			return(buf);
		case INT16_TYPE:
			snprintf(buf,len,"%hd", value->val.int16Val);
			return(buf);
		case INT32_TYPE:
			snprintf(buf,len,"%d", value->val.int32Val);
			return(buf);
		case INT64_TYPE:
			snprintf(buf,len,"%lld", value->val.int64Val);
			return(buf);

		case UINT8_TYPE:
			snprintf(buf,len,"%hhu",(uint8_t)value->val.int8Val);
			return(buf);
		case UINT16_TYPE:
			snprintf(buf,len,"%hu",(uint16_t)value->val.int16Val);
			return(buf);
		case UINT32_TYPE:
			snprintf(buf,len,"%u",(uint32_t)value->val.int16Val);
			return(buf);
		case UINT64_TYPE:
			snprintf(buf,len,"%llu",(uint64_t)value->val.int64Val);
			return(buf);

		case DATE_TYPE:
			typePrintDate(buf,len, value->val.int32Val);
			return(buf);

		case TIME_TYPE:
			typePrintTime(buf,len, value->val.int32Val);
			return(buf);

		case DATETIME_TYPE:
			typePrintDateTime(buf,len, value->val.byteVal);
			return(buf);

		case MILLITIME_TYPE:
			typePrintMilliTime(buf,len, value->val.int32Val);
			return(buf);

		case MILLIDATETIME_TYPE:
			typePrintMilliDateTime(buf,len, value->val.byteVal);
			return(buf);

		case IPV4_TYPE:
			typePrintIPv4(buf,len, value->val.int32Val);
			return(buf);

		case CIDR4_TYPE:
			typePrintCIDR4(buf,len, value->val.byteVal);
			return(buf);

		case IPV6_TYPE:
			typePrintIPv6(buf,len, value->val.byteVal);
			return(buf);

		case CIDR6_TYPE:
			typePrintCIDR6(buf,len, value->val.byteVal);
			return(buf);

		case MONEY_TYPE:
			typePrintMoney(buf, len, value->val.int32Val);
			return(buf);

		case CHAR_TYPE:
		case TEXT_TYPE:
			return((char*)value->val.charVal);

		case REAL_TYPE:
			snprintf(format,sizeof(format), "%%.%df",
				value->precision);
			snprintf(buf, len, format, value->val.realVal);
			return(buf);
	}
	return(NULL);
}


/*
char msqlTypeNames[][12] =
       {"???", "int", "char","real","ident","null","text","date","uint",
       "money","time","ip","int64","uint64","int8","int16","???"};
*/

char *typePrintTypeName(type)
	int	type;
{
	static	char unknown[] = "UNKNOWN TYPE";

	if (type < 1 || type > 9)
		return(unknown);
	return(msqlTypeNames[type]);
}

	
  

