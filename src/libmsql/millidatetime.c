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
** $Id: millidatetime.c,v 1.2 2004/08/02 01:44:20 bambi Exp $
**
*/

/*
** Module	: main : millitime
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
#include "msql.h"

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

int msqlMillitimeToTimeval __ANSI_PROTO((char *, struct timeval *));

/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


char *msqlTimevalToMillidatetime(tv)
	struct  timeval *tv;
{
	static char	result[25];
	snprintf(result,24,"%s %s.%ld", 
		msqlUnixTimeToDate(tv->tv_sec), 
		msqlUnixTimeToTime(tv->tv_sec), 
		tv->tv_usec / 10);
	return(result);
}


int msqlMillidatetimeToTimeval(millidatetime, ptr)
	char	*millidatetime;
	struct 	timeval *ptr;
{
	char	*buf,
		*millitime;
	int	dateVal;

	ptr->tv_sec = 0;
	ptr->tv_usec = 0;
	if (!millidatetime)
		return(-1);
	buf = strdup(millidatetime);
	millitime = index(buf,' ');
	if (!millitime)
	{
		free(buf);
		return(-1);
	}

	*millitime = 0;
	if (msqlMillitimeToTimeval(millitime + 1, ptr) < 0)
	{
		free(buf);
		return(-1);
	}
	dateVal = msqlDateToUnixTime(buf);
	free(buf);
	if (dateVal < 0)
	{
		return(-1);
	}
	ptr->tv_sec += (u_int)dateVal;
	return(0);
}

