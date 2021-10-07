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
** $Id: net_client.c,v 1.4 2002/10/15 01:22:01 bambi Exp $
**
*/

/*
** Module	: libmsql : net
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
#include <common/config.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include <common/msql_defs.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>

/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


#ifndef EINTR
#  define EINTR 0
#endif

#if defined(_OS_WIN32)
#  include <winsock.h>
#  define NET_READ(fd,b,l) recv(fd,b,l,0)
#  define NET_WRITE(fd,b,l) send(fd,b,l,0)
#else
#  define NET_READ(fd,b,l) read(fd,b,l)
#  define NET_WRITE(fd,b,l) write(fd,b,l)
#endif


static 	u_char	packetBuf[PKT_LEN + 4];
static	int	readTimeout;
u_char	*clientPacket = NULL;


static void intToBuf(cp,val)
        u_char  *cp;
        int     val;
{
        *cp++ = (unsigned int)(val & 0x000000ff);
        *cp++ = (unsigned int)(val & 0x0000ff00) >> 8;
        *cp++ = (unsigned int)(val & 0x00ff0000) >> 16;
        *cp++ = (unsigned int)(val & 0xff000000) >> 24;
}


static int bufToInt(cp)
        u_char  *cp;
{
        int val;
 
        val = 0;
        val = *cp++;
        val += ((int) *cp++) << 8 ;
        val += ((int) *cp++) << 16;
        val += ((int) *cp++) << 24;
        return(val);
}


#if defined(_OS_WIN32)
void initWinsock()
{	
	WORD	wVersion;
	WSADATA	wsaData;

	wVersion = MAKEWORD(1,1);
	WSAStartup(wVersion, &wsaData);
}
#endif


void initNet()
{
	clientPacket = (u_char *)packetBuf + 4;
#if defined(_OS_WIN32)
	initWinsock();
#endif
}



int netClientWritePacket(fd)
	int	fd;
{
	int	len,
		offset,
		remain,
		numBytes;

	len = strlen((char *)clientPacket);
	intToBuf(packetBuf,len);
	offset = 0;
	remain = len+4;
	while(remain > 0)
	{
		numBytes = NET_WRITE(fd,packetBuf + offset, remain);
		if (numBytes == -1)
		{
			return(-1);
		}
		offset += numBytes;
		remain -= numBytes;
	}
	return(0);
}



int netClientReadPacket(fd)
	int	fd;
{
	u_char	 buf[4];
	int	len,
		remain,
		offset,
		numBytes;

	remain = 4;
	offset = 0;
	numBytes = 0;
	readTimeout = 0;
	while(remain > 0)
	{
		/* 
		** We can't just set an alarm here as on lots of boxes
		** both read and recv are non-interuptable.  So, we 
		** wait till there something to read before we start
		** reading in the server (not the client)
		*/
		if (!readTimeout)
		{
			numBytes = NET_READ(fd,buf + offset,remain);
			if (numBytes < 0 && errno != EINTR)
			{
				fprintf(stderr,"Socket read on %d for length failed : ",fd);
				perror("");
			}
			if (numBytes <= 0)
         			return(-1);
		}
		if (readTimeout)
			break;
		remain -= numBytes;
		offset += numBytes;
		
	}
	len = bufToInt(buf);
	if (len > PKT_LEN)
	{
		fprintf(stderr,"Packet too large (%d)\n", len);
		return(-1);
	}
	if (len < 0)
	{
		fprintf(stderr,"Malformed packet\n");
		return(-1);
	}
	remain = len;
	offset = 0;
	while(remain > 0)
	{
		numBytes = NET_READ(fd,clientPacket+offset,remain);
		if (numBytes <= 0)
		{
         		return(-1);
		}
		remain -= numBytes;
		offset += numBytes;
	}
	*(clientPacket + len) = 0;
        return(len);
}


#ifdef NOTDEF

/***********************************************************************
 * 
 * This section of code contains machine-specific code for handling integers.
 * Msql supports 32 bit 2's complement integers.  To hide the details of
 * converting integers for specific machines, the routines packInt32() and
 * unpackInt32() were written.  If you have a machine that has native ints
 * other than 32 bit 2's complement, you must either write your own versions
 * of packInt32() and unpackInt32(), or modify the supplied ones.  For any
 * machine using 2's complement ints, simple changes to the
 * BYTES_PER_INT, HIGH_BITS, HIGH_BITS_MASK, and SIGN_BIT_MASK macros should
 * make your code work.  If you have something else, you're on your own ...
 *
 ************************************************************************/

#if _CRAY
#define BYTES_PER_INT	8
#endif

#ifndef BYTES_PER_INT
#define BYTES_PER_INT	4		/* default.  most boxes fit here */
#endif

#if BYTES_PER_INT == 8
#define BIG_INTS	1
#define HIGH_BITS	32			/* bits-per-int minus 32 */
#define HIGH_BITS_MASK	0xffffffff00000000	/* mask of the high bits */
#define SIGN_BIT_MASK	0x0000000080000000      /* mask of your sign bit */
#endif

#if BYTES_PER_INT == 4
#define BIG_INTS      	0
#endif

/*
 * Pack a native integer into a character buffer.  The buffer is assumed
 * to be at least 4 bytes long.
 */

int
packInt32(num, buf)
int	num;
char	*buf;
{
#if BIG_INTS
	num <<= HIGH_BITS;
#endif

	bcopy4((char *)&num, buf);
	return 0;
}

/*
 * Extract a native integer from a character buffer.  The buffer is assumed
 * to have been formatted using the packInt32() routine.
 */

int
unpackInt32(buf)
char	*buf;
{
	int	num;

	bcopy4(buf, (char *)&num);

#if BIG_INTS
	num >>= HIGH_BITS;

	if (num & SIGN_BIT_MASK) {
		num |= HIGH_BITS_MASK;
	}
#endif

	return num;
}

#endif /*NOTDEF*/
