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
** $Id: type_cidr4.c,v 1.3 2002/12/13 04:03:35 bambi Exp $
**
*/

/*
** Module	: types : type_cidr4
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


void typePrintCIDR4(buf, bufLen, val)
	char	*buf;
	int	bufLen;
	void	*val;
{
	u_int	val1, val2, val3, val4, val5;

	val1 = (u_int)*(u_char*)(val);	
	val2 = (u_int)*(u_char*)(val + 1);	
	val3 = (u_int)*(u_char*)(val + 2);	
	val4 = (u_int)*(u_char*)(val + 3);	
	val5 = (u_int)*(u_char*)(val + 4);	
        snprintf(buf,bufLen,"%d.%d.%d.%d/%d",val1,val2,val3,val4,val5);
}


void *typeScanCharCIDR4Value(val, errBuf, errLen)
	char	*val;
	char	*errBuf;
	int	errLen;
{
	int	res1, res2, res3, res4, res5;
	char	*buf,
		*cp,
		*seg1, *seg2, *seg3, *seg4, *seg5;
	void	*valBuf;

	cp = val;
	while(*cp)
	{
		if (!isdigit((int)*cp) && *cp != '.' && *cp != '/')
		{
			snprintf(errBuf,errLen,CIDR4_ERROR);
			return(NULL);
		}
		cp++;
	}
	buf = (char *)strdup(val);
	seg1 = buf;
	seg2 = index(buf,'.');
	if (!seg2)
	{
		free(buf);
		snprintf(errBuf,errLen,CIDR4_ERROR);
		return(NULL);
	}
	*seg2 = 0;
	seg2++;
	seg3 = index(seg2,'.');
	if (!seg3)
	{
		free(buf);
		snprintf(errBuf,errLen,CIDR4_ERROR);
		return(NULL);
	}
	*seg3 = 0;
	seg3++;
	seg4 = index(seg3,'.');
	if (!seg4)
	{
		free(buf);
		snprintf(errBuf,errLen,CIDR4_ERROR);
		return(NULL);
	}
	*seg4 = 0;
	seg4++;
	seg5 = index(seg4,'/');
	if (seg5)
	{
		*seg5 = 0;
		seg5++;
		res5 = atoi(seg5);
	}
	else
	{
		res5 = 32;
	}
	

	res1 = atoi(seg1);
	res2 = atoi(seg2);
	res3 = atoi(seg3);
	res4 = atoi(seg4);

	if (res1 > 255 || res2 > 255 || res3 > 255 || res4 > 255 || res5 > 32)
	{
		free(buf);
		snprintf(errBuf,errLen,CIDR4_ERROR);
		return(NULL);
	}

	valBuf = malloc(5);
	*(char*)valBuf = (char)res1;
	*(char*)(valBuf+1) = (char)res2;
	*(char*)(valBuf+2) = (char)res3;
	*(char*)(valBuf+3) = (char)res4;
	*(char*)(valBuf+4) = (char)res5;
	free(buf);
	return(valBuf);
}


void *typeScanCIDR4(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
        if (value->val.charVal == NULL || *value->val.charVal==0)
        {
                value->nullVal = 1;
                return(0);
        }
	return(typeScanCharCIDR4Value((char*)value->val.charVal,errBuf,errLen));
}
