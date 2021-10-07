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
** $Id: type_time.c,v 1.4 2002/08/12 06:32:37 bambi Exp $
**
*/

/*
** Module	: types : type_time
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

#include <time.h>

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


void typePrintTime(buf,len,val)
	char	*buf;
	int	len;
	u_int	val;
{
	int	hour,
		min,
		sec;

	if (val == 0)
	{
		*buf = 0;
		return;
	}
	hour = val >> 12;
	min = (val - (hour << 12)) >> 6;
	sec = val - (hour << 12) - (min << 6) ;
	while(sec > 59)
	{
		sec -= 60;
		min++;
	}
	while(min > 59)
	{
		min -= 60;
		hour++;
	}
	snprintf(buf,len,"%02d:%02d:%02d",hour,min,sec);
}


int typeScanCharTimeValue(val, errBuf, errLen)
	char	*val,
		*errBuf;
	int	errLen;
{
	char	*cp,
		*cp2;
	int	hour,
		min,
		sec;
	time_t	timeVal;

	/*
	** Is it an empty value?
	*/
	if (*val == 0)
	{
		if (errBuf)
			snprintf(errBuf,errLen,TIME_ERROR);
		return(-1);
	}

	/*
	** Scan the hour value
	*/
	cp = (char *)index(val,':');
	if (!cp)
	{
		if (errBuf)
			snprintf(errBuf,errLen,TIME_ERROR);
		return(-1);
	}
	hour = atoi(val);

	/*
	** Scan the min value
	*/
	cp2 = (char *)index(cp+1,':');
	if (!cp2)
	{
		if (errBuf)
			snprintf(errBuf,errLen,TIME_ERROR);
		return(-1);
	}
	min = atoi(cp+1);
	if (min > 60)
	{
		if (errBuf)
			snprintf(errBuf,errLen,TIME_ERROR);
		return(-1);
	}

	/*
	** Scan the year value
	*/
	sec = atoi(cp2+1);
	if (sec > 60)
	{
		if (errBuf)
			snprintf(errBuf,errLen,TIME_ERROR);
		return(-1);
	}

	timeVal = (hour << 12) + (min << 6) + sec;
	return((int)timeVal);
}


int typeScanTime(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
        if (value->val.charVal == NULL || *value->val.charVal==0)
        {
                value->nullVal = 1;
                return(0);
        }
	return(typeScanCharTimeValue((char*)value->val.charVal,errBuf,errLen));
}
