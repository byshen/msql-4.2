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
** $Id: time.c,v 1.8 2012/07/02 00:12:18 bambi Exp $
**
*/

/*
** Module	: main : time
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


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


static int days_in_month[2][13] = {
        {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

static char *month_names[13] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

#define leap_year(yr) (!((yr) % 4) && ((yr) % 100)) || (!((yr) % 400))


/* date value is  (year << 9) + (month << 5) + day */
#define	year_mask	0x007FFE00
#define	month_mask	0x000001E0
#define day_mask	0x0000001F

char *msqlUnixTimeToDate(clock)
	time_t clock;
{
	static char	buf[20];
	struct	tm *locTime;

	locTime = localtime(&clock);
	/*
	strftime(buf,sizeof(buf),"%d-%b-%Y",locTime);
	*/
	sprintf(buf, "%d-%s-%d", locTime->tm_mday, 
		month_names[locTime->tm_mon], locTime->tm_year + 1900);
	return(buf);
}


time_t msqlDateToUnixTime(date)
	char	*date;
{
	struct	tm locTime;
	time_t	res;

	bzero(&locTime, sizeof(struct tm));
	strptime(date,"%d-%b-%Y", &locTime);
	locTime.tm_sec = 0;
	locTime.tm_min = 0;
	locTime.tm_hour = 0;
	res = mktime(&locTime);
	return(res);
}



char *msqlUnixTimeToTime(clock)
	time_t clock;
{
	static char	buf[20];
	struct	tm *locTime;

	locTime = localtime(&clock);
	/*strftime(buf,sizeof(buf),"%H:%M:%S",locTime);*/
	snprintf(buf, sizeof(buf), "%d:%02d:%02d",
		locTime->tm_hour, locTime->tm_min, locTime->tm_sec);
	return(buf);
}


time_t msqlTimeToUnixTime(date)
	char	*date;
{
	struct	tm locTime,
		*gmtZero;
	time_t	res;
	int	gmtOffset;

	res = 0;
	gmtZero = localtime(&res);
#ifdef HAVE_GMTOFF
	gmtOffset = gmtZero->tm_gmtoff;
#endif
#ifdef HAVE_TIMEZONE
	gmtOffset = timezone;
#endif
	bzero(&locTime, sizeof(struct tm));
	strptime(date,"%H:%M:%S", &locTime);
	locTime.tm_year = 70;
	locTime.tm_mon = 0;
	locTime.tm_mday = 1;
	res = mktime(&locTime);
	res += gmtOffset;
	return(res);
}




char *msqlSumTimes(t1, t2)
	char	*t1, *t2;
{
	int	h1, m1, s1,
		h2, m2, s2,
		hr, mr, sr;
	static  char buf[80];
	char	*cp;

	h1 = atoi(t1);
	cp = (char *)index(t1,':');
	if (!cp)
		return(NULL);
	m1 = atoi(cp + 1);
	cp = (char *)index(cp + 1,':');
	if (!cp)
		return(NULL);
	s1 = atoi(cp + 1);

	h2 = atoi(t2);
	cp = (char *)index(t2,':');
	if (!cp)
		return(NULL);
	m2 = atoi(cp + 1);
	cp = (char *)index(cp + 1,':');
	if (!cp)
		return(NULL);
	s2 = atoi(cp + 1);

	hr = h1 + h2;
	mr = m1 + m2;
	sr = s1 + s2;

	while (sr > 59)
	{
		mr++;
		sr -= 60;
	}
	while (mr > 59)
	{
		hr++;
		mr -= 60;
	}
	
	sprintf(buf,"%d:%02d:%02d",hr,mr,sr);
	return(buf);
}


char *msqlDateOffset(date, dOff, mOff, yOff)
	char	*date;
	int	dOff, mOff, yOff;
{
	int	val, res,
		year, month, day,
		maxDays, addMaxDays,
		leap;
	static	char buf[80];

	if (dOff == 0 && mOff == 0 && yOff == 0)
	{
		strcpy(buf,date);
		return(buf);
	}
	
	val = typeScanCharDateValue(date,NULL, 0);
	year = ((val & year_mask) >> 9) - 4096 + yOff;
	month = ((val & month_mask) >> 5) + mOff;
	day = (val & day_mask) + dOff;

	addMaxDays = 0;

	while (1)
	{
		while(month > 12)
		{
			month -= 12;
			year++;
		}
		while(month < 1)
		{
			month += 12;
			year--;
		}
		leap = leap_year(year);
		if (leap)
			maxDays = days_in_month[1][month];	
		else
			maxDays = days_in_month[0][month];
		if (addMaxDays)
		{
			day += maxDays;
			addMaxDays = 0;
		}
		if (day > maxDays)
		{
			day -= maxDays;
			month++;
			continue;
		}
		if (day < 1)
		{
			month--;
			addMaxDays = 1;
			continue;
		}
		break;
	}
	year += 4096;
	res = (year << 9) + (month << 5) + day;
	typePrintDate(buf,80,res);
	return(buf);
}


#define	hour_mask	0x0001F000
#define	min_mask	0x00000FC0
#define	sec_mask	0x0000003F	

char *msqlDiffTimes(t1, t2)
	char	*t1, *t2;
{
	int	v1, v2,
		h1, m1, s1,
		h2, m2, s2,
		hr, mr, sr;
	static  char buf[80];

	v1 = typeScanCharTimeValue(t1, NULL, 0);
	v2 = typeScanCharTimeValue(t2, NULL, 0);
	if (v1 == -1 || v2 == -1)
		return(NULL);
	
	h1 = (v1 & hour_mask) >> 12;
	m1 = (v1 & min_mask) >> 6;
	s1 = (v1 & sec_mask) ;

	h2 = (v2 & hour_mask) >> 12;
	m2 = (v2 & min_mask) >> 6;
	s2 = (v2 & sec_mask) ;

	hr = h2 - h1;
	mr = m2 - m1;
	sr = s2 - s1;

	while(sr < 0)
	{
		sr+=60;
		mr--;
	}
	while(mr < 0)
	{
		mr+=60;
		hr--;
	}
	snprintf(buf,sizeof(buf),"%02d:%02d:%02d",hr,mr,sr);
	return(buf);
}


int msqlDiffDates(date1, date2)
	char	*date1, *date2;
{
	int	v1, v2,
		y1, m1, d1,
		y2, m2, d2,
		res,
		leap;

	v1 = typeScanCharDateValue(date1, NULL, 0);
	v2 = typeScanCharDateValue(date2, NULL, 0);
	if (v1 == -1 || v2 == -1)
		return(-1);
	if (v1 > v2)
		return(-1);

	y1 = ((v1 & year_mask) >> 9) - 4096;
	m1 = (v1 & month_mask) >> 5;
	d1 = (v1 & day_mask);

	y2 = ((v2 & year_mask) >> 9) - 4096;
	m2 = (v2 & month_mask) >> 5;
	d2 = (v2 & day_mask);

	/* Simple case */
	if ((m1 == m2) && (y1 == y2))
		return(d2 - d1);

	/* Not so simple.  Work our way forward */
	res = 0;
	while((m1 != m2) || (y1 != y2))
	{
		leap = leap_year(y1);
		res += days_in_month[leap][m1] - d1 + 1;
		m1++;
		if (m1 > 12)
		{
			m1 = 1;
			y1++;
		}
		d1 = 1;
	}
	res += d2 - d1;
	return(res);
}


