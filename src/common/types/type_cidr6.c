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
** $Id: type_cidr6.c,v 1.2 2012/01/16 23:37:55 bambi Exp $
**
*/

/*
** Module	: types : type_cidr6
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

#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

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


void typePrintCIDR6(buf, bufLen, val)
	char	*buf;
	int	bufLen;
	void	*val;
{
	struct 	sockaddr_in6 sa;
	char	length[5];
	u_char	lenVal;

	bcopy(val, &(sa.sin6_addr), sizeof(struct in6_addr));
	bcopy(val, &(sa.sin6_addr), 16);
	inet_ntop(AF_INET6, &(sa.sin6_addr), buf, bufLen);
	lenVal = (u_char)*(u_char*)(val+16);
	snprintf(length,5,"/%u", lenVal);
	strcat(buf,length);
}


void *typeScanCharCIDR6Value(val, errBuf, errLen)
	char	*val;
	char	*errBuf;
	int	errLen;
{
	struct 	in6_addr addr;
	void	*result;
	int	res,
		lenVal;
	char	charVal,
		*slash;

	slash = index(val, '/');
	if (slash)
	{
		*slash = 0;
		if(sscanf(slash+1,"%u",&lenVal) != 1)
		{
			*slash = '/';
			snprintf(errBuf, errLen, CIDR6_ERROR);
			return(NULL);
		}
	}
	else
	{
		lenVal = 128;
	}
	res = inet_pton(AF_INET6, val, &addr);
	if (slash)
	{
		*slash = '/';
	}
	if (res == 1)
	{
		result = (void *)malloc(sizeof(struct in6_addr)+1);
		bcopy(&addr, result, sizeof(struct in6_addr));
		charVal = (char)lenVal;
		*(char*)(result+16) = charVal;
		return(result);
	}
	snprintf(errBuf, errLen, CIDR6_ERROR);
	return(NULL);
}


void *typeScanCIDR6(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
        if (value->val.charVal == NULL || *value->val.charVal==0)
        {
                value->nullVal = 1;
                return(0);
        }
	return(typeScanCharCIDR6Value((char*)value->val.charVal,errBuf,errLen));
}
