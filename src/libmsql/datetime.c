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
** $Id: datetime.c,v 1.2 2012/07/02 00:12:18 bambi Exp $
**
*/

/*
** Module	: main : datetime
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

#define __USE_XOPEN	/* For linux */
#include <time.h>
#undef __USE_XOPEN

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <common/types/types.h>
#include "msql.h"


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static char *month_names[13] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


char *msqlUnixTimeToDatetime(clock)
	time_t	clock;
{
	static char	result[25];
	struct	tm *locTime;
/*
	char		*date,
			*time;

	date = msqlUnixTimeToDate(clock);
	time = msqlUnixTimeToTime(clock);
	snprintf(result, sizeof(result), "%s %s", date?date:"", time?time:"");
*/
	locTime = localtime(&clock);
	snprintf(result, sizeof(result), "%d-%s-%d %d:%02d:%02d",
		locTime->tm_mday, month_names[locTime->tm_mon], 
		locTime->tm_year + 1900, locTime->tm_hour, 
		locTime->tm_min, locTime->tm_sec);
	return(result);
}


time_t msqlDatetimeToUnixTime2(datetime)
	char	*datetime;
{
	char	buf[100],
		*time;
	int	timeVal,
		dateVal;
	time_t	clock;

	if (!datetime)
		return(-1);
	strncpy(buf, datetime, 100);
	time = index(buf,' ');
	if (!time)
	{
		return(-1);
	}

	*time = 0;
	timeVal =  msqlTimeToUnixTime(time + 1);
	dateVal =  msqlDateToUnixTime(buf);
	if (timeVal < 0 || dateVal < 0)
	{
		return(-1);
	}
	clock = (u_int)dateVal + (u_int)timeVal;
	return(clock);
}


time_t msqlDatetimeToUnixTime(datetime)
	char	*datetime;
{
        struct  tm locTime;
        time_t  res;

	if (!datetime)
		return(-1);

	bzero(&locTime, sizeof(locTime));
        strptime(datetime,"%d-%b-%Y %H:%M:%S", &locTime);
        res = mktime(&locTime);
        return(res);
}
