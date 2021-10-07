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
** $Id: type_date.c,v 1.4 2002/08/12 06:32:37 bambi Exp $
**
*/

/*
** Module	: types : type_date
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



void typePrintDate(buf,len,val)
	char	*buf;
	int	len,
		val;
{
	int 	year, mon, day,
		recalc = 0;

	if (val == 0)
	{
		*buf = 0;
		return;
	}
	year = (val >> 9);
	mon = (val - (year << 9)) >> 5;
	day = val - (year << 9) - (mon << 5) ;
	if (day == 0)
	{
		val -= 1;
		recalc = 1;
	}
	if (mon == 0)
	{
		val -= 1 << 5;
		recalc = 1;
	}
	if (recalc)
	{
		year = (val >> 9);
		mon = (val - (year << 9)) >> 5;
		day = val - (year << 9) - (mon << 5) ;
	}
	year -= 4096;
	switch(mon)
	{
		case 1: snprintf(buf,len,"%02d-Jan-%04d",day,year);
			break;
		case 2: snprintf(buf,len,"%02d-Feb-%04d",day,year);
			break;
		case 3: snprintf(buf,len,"%02d-Mar-%04d",day,year);
			break;
		case 4: snprintf(buf,len,"%02d-Apr-%04d",day,year);
			break;
		case 5: snprintf(buf,len,"%02d-May-%04d",day,year);
			break;
		case 6: snprintf(buf,len,"%02d-Jun-%04d",day,year);
			break;
		case 7: snprintf(buf,len,"%02d-Jul-%04d",day,year);
			break;
		case 8: snprintf(buf,len,"%02d-Aug-%04d",day,year);
			break;
		case 9: snprintf(buf,len,"%02d-Sep-%04d",day,year);
			break;
		case 10: snprintf(buf,len,"%02d-Oct-%04d",day,year);
			break;
		case 11: snprintf(buf,len,"%02d-Nov-%04d",day,year);
			break;
		case 12: snprintf(buf,len,"%02d-Dec-%04d",day,year);
			break;
	}
}




int typeScanCharDateValue(val, errBuf, errLen)
	char	*val,
		*errBuf;
	int	errLen;
{
	char	*cp,
		*cp2;
	int	day,
		month,
		year;
	time_t	timeVal;

	/*
	** Scan the day value
	*/
	if (val == NULL || *val == 0)
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_ERROR);
		return(-1);
	}
	cp = (char *)index(val,'-');
	if (!cp)
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_ERROR);
		return(-1);
	}
	day = atoi(val);
	if (day == 0)
		return(-1);

	/*
	** Scan the month value
	*/
	cp2 = (char *)index(cp+1,'-');
	if (!cp2)
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_ERROR);
		return(-1);
	}
	*(cp+1) = toupper(*(cp+1));
	if (strncmp(cp+1,"Jan-",4) == 0)
		month = 1;
	else if (strncmp(cp+1,"Feb-",4) == 0)
		month = 2;
	else if (strncmp(cp+1,"Mar-",4) == 0)
		month = 3;
	else if (strncmp(cp+1,"Apr-",4) == 0)
		month = 4;
	else if (strncmp(cp+1,"May-",4) == 0)
		month = 5;
	else if (strncmp(cp+1,"Jun-",4) == 0)
		month = 6;
	else if (strncmp(cp+1,"Jul-",4) == 0)
		month = 7;
	else if (strncmp(cp+1,"Aug-",4) == 0)
		month = 8;
	else if (strncmp(cp+1,"Sep-",4) == 0)
		month = 9;
	else if (strncmp(cp+1,"Oct-",4) == 0)
		month = 10;
	else if (strncmp(cp+1,"Nov-",4) == 0)
		month = 11;
	else if (strncmp(cp+1,"Dec-",4) == 0)
		month = 12;
	else
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_ERROR);
		return(-1);
	}

	/*
	** Scan the year value
	*/
	year = atoi(cp2+1);
	if (year == 0)
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_ERROR);
		return(-1);
	}
	if (year < 100 && strlen(cp2+1) == 2)
	{
		char	yearBuf[10];
		struct	tm *locTime;
		time_t	clock;

		clock = time(NULL);
		locTime = localtime(&clock);
		strftime(yearBuf,10,"%Y",locTime);
		yearBuf[2] = 0;
		year = (atoi(yearBuf) * 100) + year;
	}
	year += 4096;

	timeVal = (year << 9) + (month << 5) + day;
	return((int)timeVal);
}


int typeScanDate(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
	if (value->val.charVal == NULL || *value->val.charVal==0)
	{
		value->nullVal = 1;
		return(0);
	}
	return(typeScanCharDateValue((char*)value->val.charVal,errBuf,errLen));
}
