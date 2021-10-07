/*
** Copyright (c) 1995-2002  Hughes Technologies Pty Ltd.  All rights
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
** $Id: tcp.c,v 1.3 2002/07/17 07:49:45 bambi Exp $
**
*/

/*
** Module	: main : tcp.c
** Purpose	: TCP/IP related routines
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


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <signal.h>

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/portability.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/includes/errmsg.h>
#include <msqld/main/main.h>
#include <msqld/broker/broker.h>
#include <msqld/main/version.h>
#include <msqld/main/process.h>
#include <msqld/main/table.h>
#include <msqld/main/net.h>
#include <msqld/main/acl.h>
#include <msqld/main/cache.h>
#include <msqld/main/util.h>
#include <msqld/main/parse.h>
#include <msqld/lock/lock.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

#define STRN_CPY(d,s,l)   { strncpy((d),(s),(l)); (d)[(l)-1] = 0; }
#define STR_NE(x,y)       (strcasecmp((x),(y)) != 0)
#define STRING_LENGTH     128
   

/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/

/*
**  auth_hostname:                      
**                                      
**    Perform tcpd style host authentication.
*/
      
int auth_hostname (sin)
	struct sockaddr_in *sin;
{                                       
	char name[STRING_LENGTH];
	struct hostent *hp;
	int i;

  	/* 
  	** On some systems, for example Solaris 2.3, 
	** gethostbyaddr(0.0.0.0) does not fail.  Instead it returns 
	** "INADDR_ANY".  Unfortunately, this does not work the other 
	** way around: gethostbyname("INADDR_ANY") fails.  We
	** have to special-case 0.0.0.0, in order to avoid false alerts 
	** from the host name/address checking code below.
	*/
        
	if (sin != 0 && sin->sin_addr.s_addr != 0 &&
		(hp = gethostbyaddr((char *) &(sin->sin_addr),
		 sizeof(sin->sin_addr), AF_INET)) != 0) 
	{
		STRN_CPY(name, hp->h_name, sizeof(name));

		/*
		** Verify that the address is a member of the address 
		** list returned by gethostbyname(hostname).
    		**
    		** Verify also that gethostbyaddr() and gethostbyname() 
		** return the same hostname, otherwise there is still 
		** potential for a spoof.
		*/
		if ((hp = gethostbyname(name)) == 0) 
		{
      			return(-1);
		} 
		else if (STR_NE(name, hp->h_name) &&
			STR_NE(name, "localhost")) 
		{

			/*
			** The gethostbyaddr() and gethostbyname() calls did 
			** not return the same hostname.  
			*/
     			return(-1);
		} 
		else 
		{
			/*
			**  If it in the address list ?
			*/
			for (i = 0; hp->h_addr_list[i]; i++) 
			{
				if (memcmp(hp->h_addr_list[i],
					(char *) &sin->sin_addr,
					sizeof(sin->sin_addr)) == 0)
				{
					return(0); 
				}
			}
		}
	} 
	/*
	** If we make it here it's a fail
	*/
	return(-1);
} 

