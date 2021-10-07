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
** $Id: type_ipv4.c,v 1.2 2002/06/29 04:08:56 bambi Exp $
**
*/

/*
** Module	: types : type_ipv4
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


void typePrintIPv4(buf, bufLen, val)
	char	*buf;
	int	bufLen;
	u_int	val;
{
	u_int	val1, val2, val3, val4;

	val1 = (val & 0xFF000000) >> 24;
	val2 = (val & 0x00FF0000) >> 16;
	val3 = (val & 0x0000FF00) >> 8;
	val4 = val & 0x000000FF;
	snprintf(buf,bufLen,"%d.%d.%d.%d",val1,val2,val3,val4);
}


u_int typeScanCharIPv4Value(val, errBuf, errLen)
	char	*val,
		*errBuf;
	int	errLen;
{
	u_int	res, res1, res2, res3, res4;
	char	*buf,
		*cp,
		*seg1, *seg2, *seg3, *seg4;

	cp = val;
	while(*cp)
	{
		if (!isdigit((int)*cp) && *cp != '.')
		{
			snprintf(errBuf,errLen,IPV4_ERROR);
			return(-1);
		}
		cp++;
	}
	buf = (char *)strdup(val);
	seg1 = buf;
	seg2 = index(buf,'.');
	if (!seg2)
	{
		free(buf);
		snprintf(errBuf,errLen,IPV4_ERROR);
		return(-1);
	}
	*seg2 = 0;
	seg2++;
	seg3 = index(seg2,'.');
	if (!seg3)
	{
		free(buf);
		snprintf(errBuf,errLen,IPV4_ERROR);
		return(-1);
	}
	*seg3 = 0;
	seg3++;
	seg4 = index(seg3,'.');
	if (!seg4)
	{
		free(buf);
		snprintf(errBuf,errLen,IPV4_ERROR);
		return(-1);
	}
	*seg4 = 0;
	seg4++;

	res1 = atoi(seg1);
	res2 = atoi(seg2);
	res3 = atoi(seg3);
	res4 = atoi(seg4);

	if (res1 > 255 || res2 > 255 || res3 > 255 || res4 > 255)
	{
		free(buf);
		snprintf(errBuf,errLen,IPV4_ERROR);
		return(-1);
	}
	res = (res1 << 24) + (res2 << 16) + (res3 << 8) + res4;
	free(buf);
	return(res);
}


u_int typeScanIPv4(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
	if (value->val.charVal == NULL || *value->val.charVal==0)
	{
		value->nullVal = 1;
		return(0);
	}

	return(typeScanCharIPv4Value((char*)value->val.charVal,errBuf, errLen));
}
