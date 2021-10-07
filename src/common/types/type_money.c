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
** $Id: type_money.c,v 1.2 2002/06/29 04:08:57 bambi Exp $
**
*/

/*
** Module	: type : type_money
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
#include <libmsql/msql.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/includes/errmsg.h>

#include "types.h"


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


void typePrintMoney(buf,bufLen,val)
	char	*buf;
	int	bufLen,
		val;
{
	char 	*cp;
	int	len;

	/* 
	** Due to some wierd floating point rounding errors we have to
	** do this as text (because 894/100 = 8.93999999999)
	*/
	if (val == 0)
	{
		strcpy(buf,"0.00");
		return;
	}
	snprintf(buf,bufLen,"%d",val);
	if (*buf == '-')
		buf++;
	len = strlen(buf);
	if (len > 2)
	{
		cp = buf + len;
		*(cp + 1) = 0;
		*cp = *(cp - 1);
		cp--;
		*cp = *(cp - 1);
		cp--;
		*cp = '.';
	}
	if (len == 2)
	{
		cp = buf + len+1;
		*(cp + 1) = 0;
		*cp = *(cp - 2);
		cp--;
		*cp = *(cp - 2);
		cp--;
		*cp = '.';
		cp--;
		*cp = '0';
	}
	if (len == 1)
	{
		cp = buf + len+2;
		*(cp + 1) = 0;
		*cp = *buf;
		cp--;
		*cp = '0';
		cp--;
		*cp = '.';
		cp--;
		*cp = '0';
	}
}


int typeScanMoney(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
	int	val,
		modVal;
	double	tmp;

	switch(value->type)
	{
		case REAL_TYPE:
			/* 
			** More hackery to get over floating point rounding
			** errors.  Fake a third digit of precision
			*/
			tmp = value->val.realVal * 1000;
			if (tmp < 0)
				modVal = (int)tmp % 10;
			else
				modVal = (unsigned int)tmp % 10;
	
			if (modVal > 5)
				tmp += 10 - modVal;
			if (modVal > 0 && modVal < 5)
				tmp -= modVal;
			if (modVal < 0 && modVal > -5)
				tmp -= modVal;
			if (modVal < 0 && modVal < -5)
				tmp += -10 - modVal;
			if (tmp < 0)
				val = ((int)tmp) / 10;
			else
				val = ((unsigned int)tmp) / 10;
			break;

		case INT8_TYPE:
		case UINT8_TYPE:
			val = value->val.int8Val * 100;
			break;

		case INT16_TYPE:
		case UINT16_TYPE:
			val = value->val.int16Val * 100;
			break;

		case INT32_TYPE:
		case UINT32_TYPE:
			val = value->val.int32Val * 100;
			break;

		case INT64_TYPE:
		case UINT64_TYPE:
			val = value->val.int64Val * 100;
			break;
	}
	return(val);
}

