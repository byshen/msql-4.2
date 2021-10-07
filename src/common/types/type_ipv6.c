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
** $Id: type_ipv6.c,v 1.1 2012/01/15 06:19:59 bambi Exp $
**
*/

/*
** Module	: types : type_ipv6
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


void typePrintIPv6(buf, bufLen, val)
	char	*buf;
	int	bufLen;
	void	*val;
{
	struct 	in6_addr addr;

	bcopy(val, &addr, sizeof(struct in6_addr));
	inet_ntop(AF_INET6, &addr, buf, bufLen);
}


void *typeScanCharIPv6Value(val, errBuf, errLen)
	char	*val;
	char	*errBuf;
	int	errLen;
{
	struct 	in6_addr addr;
	void	*result;
	int	res;
	char	tmpBuf[100];

	res = inet_pton(AF_INET6, val, &addr);
	if (res == 1)
	{
		result = (void *)malloc(sizeof(struct in6_addr));
		bcopy(&addr, result, sizeof(struct in6_addr));
		return(result);
	}
	snprintf(errBuf, errLen, IPV6_ERROR);
	return(NULL);
}


void *typeScanIPv6(value, errBuf, errLen)
	mVal_t	*value;
	char	*errBuf;
	int	errLen;
{
        if (value->val.charVal == NULL || *value->val.charVal==0)
        {
                value->nullVal = 1;
                return(0);
        }
	return(typeScanCharIPv6Value((char*)value->val.charVal,errBuf,errLen));
}
