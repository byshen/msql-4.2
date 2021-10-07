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
** $Id: type_datetime.c,v 1.5 2004/07/12 23:34:41 bambi Exp $
**
*/

/*
** Module	: types : type_datetime
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
** A datetime value is stored in a 5 byte field with the following
** format
**	Time of day in seconds		low 17 bits
**	Day 				5 bits
**	Month				4 bits
**	Year				12 bits (0 to 4096 AD)
*/


void typePrintDateTime(buf,len,val)
	char	*buf;
	int	len;
	u_char	*val;
{
	u_int 	year, mon, day,
		time, hour, min, sec;

	if (val == NULL)
	{
		*buf = 0;
		return;
	}
	year = ((*val & 0x3F)<< 6) + ((*(val + 1) >> 2) & 0x3F);
	mon = ((*(val+1) & 0x03) << 2) + ((*(val+2) >> 6) & 0x03);
	day = ((*(val+2) >> 1) & 0x1F);
	time = ((*(val+2) & 0x01) << 16) + (*(val+3) << 8) + (*(val+4));

	hour = time / 3600;
	min = (time - (hour * 3600)) / 60;
	sec = time % 60;
	
	switch(mon)
	{
		case 1: snprintf(buf,len,"%02d-Jan-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 2: snprintf(buf,len,"%02d-Feb-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 3: snprintf(buf,len,"%02d-Mar-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 4: snprintf(buf,len,"%02d-Apr-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 5: snprintf(buf,len,"%02d-May-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 6: snprintf(buf,len,"%02d-Jun-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 7: snprintf(buf,len,"%02d-Jul-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 8: snprintf(buf,len,"%02d-Aug-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 9: snprintf(buf,len,"%02d-Sep-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 10: snprintf(buf,len,"%02d-Oct-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 11: snprintf(buf,len,"%02d-Nov-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
		case 12: snprintf(buf,len,"%02d-Dec-%04d %02d:%02d:%02d",
			day,year,hour,min,sec);
			break;
	}
	/* 
	printf("%s : %u.%u.%u.%u.%u\n", buf, *(val), *(val+1),
		*(val+2), *(val+3), *(val+4));
	*/
}




void *typeScanCharDateTimeValue(val, errBuf, errLen)
	u_char	*val;
	char	*errBuf;
	int	errLen;
{
	char 	valBuf[6],
		*result;
	char	monthName[5];
	char	*cp;
	u_int	day, month, year,
		time, hour, min, sec;

	if (val == NULL || *val == 0)
	{
		if (errBuf)
			snprintf(errBuf,(size_t)errLen,DATE_TIME_ERROR);
		return(NULL);
	}
	if (sscanf((char*)val, "%d-%3s-%d %d:%d:%d", &day, monthName,
		&year, &hour, &min, &sec) != 6)
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_TIME_ERROR);
		return(NULL);
	}

	if (day == 0)
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_TIME_ERROR);
		return(NULL);
	}

	if (year < 100)
	{
		year += 2000;
	}

	cp = monthName;
	*cp = toupper(*cp);
	*(cp+1) = tolower(*(cp+1));
	*(cp+2) = tolower(*(cp+2));
	if (strcmp(monthName,"Jan") == 0) 	month = 1;
	else if (strcmp(monthName,"Feb") == 0) 	month = 2;
	else if (strcmp(monthName,"Mar") == 0) 	month = 3;
	else if (strcmp(monthName,"Apr") == 0) 	month = 4;
	else if (strcmp(monthName,"May") == 0) 	month = 5;
	else if (strcmp(monthName,"Jun") == 0) 	month = 6;
	else if (strcmp(monthName,"Jul") == 0) 	month = 7;
	else if (strcmp(monthName,"Aug") == 0) 	month = 8;
	else if (strcmp(monthName,"Sep") == 0) 	month = 9;
	else if (strcmp(monthName,"Oct") == 0) 	month = 10;
	else if (strcmp(monthName,"Nov") == 0) 	month = 11;
	else if (strcmp(monthName,"Dec") == 0) 	month = 12;
	else
	{
		if (errBuf)
			snprintf(errBuf,errLen,DATE_TIME_ERROR);
		return(NULL);
	}
	time = (hour * 3600) + (min * 60) + sec;

	/* Create the internal representation */
	cp = valBuf;
	*cp = year >> 6 & 0x3F;
	*(cp+1) = ((year & 0x3F) << 2) + ((month >> 2) & 0x03);
	*(cp+2) = ((month & 0x03) << 6) + (day << 1) + ((time >> 16) & 0x01);
	*(cp+3) = (time >> 8) & 0xFF;
	*(cp+4) = time & 0xFF;

	result = (char*)malloc(5);
	*(result) = *(valBuf);
	*(result+1) = *(valBuf+1);
	*(result+2) = *(valBuf+2);
	*(result+3) = *(valBuf+3);
	*(result+4) = *(valBuf+4);
	return((void*)result);
}


void *typeScanDateTime(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
	if (value->val.charVal == NULL || *value->val.charVal==0)
	{
		value->nullVal = 1;
		return(0);
	}
	return(typeScanCharDateTimeValue(value->val.charVal, errBuf, errLen));
}
