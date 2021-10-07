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
** $Id: type_millitime.c,v 1.3 2004/08/02 01:44:20 bambi Exp $
**
*/

/*
** Module	: types : type_millitime
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

/*
** A millitime value is stored in a 4 byte u_int field representing
** the number of milliseconds since midnight in the current day.
*/


void typePrintMilliTime(buf,len,val)
	char	*buf;
	int	len;
	u_int	val;
{
	u_int 	hour, min, sec, usec;

	if (val == 0)
	{
		*buf = 0;
		return;
	}

	hour = val / 3600000;
	min = (val / (60 * 1000)) % 60;
	sec = (val / 1000) % 60;
	usec = val % 1000;
	
	snprintf(buf,len,"%02d:%02d:%02d.%03d", hour, min, sec, usec);
}




u_int typeScanCharMilliTimeValue(val, errBuf, errLen)
	u_char	*val;
	char	*errBuf;
	int	errLen;
{
	u_int	time, hour, min, sec, usec;

	if (val == NULL || *val == 0)
	{
		if (errBuf)
			snprintf(errBuf,errLen,MILLI_TIME_ERROR);
		return(-1);
	}
	if (sscanf((char*)val, "%d:%d:%d.%d", &hour, &min, &sec, &usec) != 4)
	{
		usec = 0;
		if (sscanf((char*)val, "%d:%d:%d", &hour, &min, &sec) != 3)
		{
			if (errBuf)
				snprintf(errBuf,errLen,MILLI_TIME_ERROR);
			return(-1);
		}
	}

	if (hour > 24 || min > 59 || sec > 59 || usec > 999 )
	{
		if (errBuf)
			snprintf(errBuf,errLen,MILLI_TIME_ERROR);
		return(-1);
	}
	time = (hour * 3600000) + (min * 60000) + (sec * 1000) + usec;
	return(time);
}


u_int typeScanMilliTime(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
	if (value->val.charVal == NULL || *value->val.charVal==0)
	{
		value->nullVal = 1;
		return(0);
	}
	return(typeScanCharMilliTimeValue(value->val.charVal, errBuf, errLen));
}
