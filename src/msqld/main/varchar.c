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
** $Id: varchar.c,v 1.10 2010/07/13 04:06:45 bambi Exp $
**
*/

/*
** Module	: main : varchar
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

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <msqld/index/index.h>
#include <msqld/includes/errmsg.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/regex.h>
#include <libmsql/msql.h>
#include "varchar.h"


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

/* HACK */
extern	char    errMsg[];

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


static u_int readOverflowFreeList(cacheEntry,pos)
	cache_t	*cacheEntry;
	u_int	pos;
{
	char	*cp;
	u_int	*next;

	/*
	** Note, skip the leading freelist header
	*/
	cp = ((char *)cacheEntry->overflowMap) + sizeof(u_int) +
		(pos * (OFB_SIZE + sizeof(u_int)));
	next = (u_int *)cp;
	return(*next);
}


static int readOverflow(entry, pos, nextPos, buf, numBytes)
	cache_t	*entry;
	u_int	pos,
		*nextPos;
	u_char	*buf;
	int	numBytes;
{
	u_char	*cp;
	u_int	offset;


	offset = sizeof(u_int) + (pos * (OFB_SIZE + sizeof(u_int)));
	if (offset > entry->overflowSize)
	{
		/*
		** This entry is not correct.  Return an error so
		** that the caller can drop this pointer
		*/
		return(-1);
	}
	cp = (u_char *)entry->overflowMap + offset;
	bcopy(cp, nextPos, sizeof(u_int));
	bcopy(cp + sizeof(u_int), buf, numBytes);
	return(0);
}


static int writeOverflowFreeList(cacheEntry,pos,value)
	cache_t	*cacheEntry;
	u_int	pos,
		value;
{
	char	*cp;

	cp = ((char *)cacheEntry->overflowMap) + sizeof(u_int) +
		(pos * (OFB_SIZE + sizeof(u_int)));
	bcopy(&value, cp, sizeof(u_int));
	return(0);
}

static u_int writeOverflow(entry, pos, lastPos, data, length)
	cache_t	*entry;
	u_int	pos,
		lastPos;
	u_char	*data;
	int	length;
{
	u_char	*cp;
	off_t	offset;
	static  u_char	buf[OFB_SIZE + sizeof(u_int)];

	/*
	** Check the mapping
	*/
	if (pos != NO_POS)
	{
		cp = (u_char *)entry->overflowMap + sizeof(u_int) +
			(pos * (OFB_SIZE + sizeof(u_int)));
		bcopy(&lastPos, cp, sizeof(u_int));
		bcopy(data, cp + sizeof(u_int), length);
	}
	else
	{
		lseek(entry->overflowFD, 0L, SEEK_END);
		offset = lseek(entry->overflowFD, 0L, SEEK_CUR);
		bzero(buf,sizeof(buf));
		cp = buf;
		bcopy(&lastPos,cp,sizeof(u_int));
		bcopy(data, cp + sizeof(u_int), length);
		write(entry->overflowFD,buf, sizeof(buf));
		entry->remapOverflow = 1;
		pos = (offset - sizeof(u_int)) / (OFB_SIZE + sizeof(u_int));

		/* NOTE : In a broker environment, the other processes
		** don't know that this file needs to be remapped.  The
		** best solution would be to include the map size in
		** the data file sblk like we do for the data file itself.
		** But, that would force a table format change requiring
		** all users to dump and reload their databases.  This
		** "work-around" achieves the same thing in a less than
		** elegant manner.  By setting the recorded data size
		** to 0 all process will think something has changed
		** and remap everything.
		*/
		entry->sblk->dataSize = 0;
	}
	return(pos);
}



static u_int popOverflowPos(cacheEntry)
        cache_t *cacheEntry;
{
        u_int   pos,
		*posPtr;

        debugTrace(TRACE_IN,"popOverflowPos()");
        posPtr = (u_int *)cacheEntry->overflowMap;
        if (!posPtr)
                return(NO_POS);
	pos = *posPtr;
        if (pos != NO_POS)
        {
                *posPtr = readOverflowFreeList(cacheEntry,pos);
        }
        return(pos);
}



static int pushOverflowPos(cacheEntry, pos)
        cache_t *cacheEntry;
        u_int   pos;
{
	u_int	*posPtr;

        debugTrace(TRACE_IN,"pushOverflowPos()");

        posPtr = (u_int *)cacheEntry->overflowMap;
        if (*posPtr == NO_POS)
        {
                *posPtr = pos;
        }
        else
        {
		if (writeOverflowFreeList(cacheEntry,*posPtr,pos) < 0)
		{
                        return(-1);
                }
        }
        if (writeOverflowFreeList(cacheEntry,pos,NO_POS) < 0)
        {
                return(-1);
        }
        return(0);
}


/* RNS
 * nextOverflow -- just find the next overflow buffer in the chain.
 * NOTE:  The caller is responsible to call this routine *only* when
 * there is a next buffer.  See, for example, varcharDelete.
 * NOTE:  This is basically readOverflow without the data transfer.
 */
static void nextOverflow(entry, pos, nextPos)
	cache_t	*entry;
	u_int	pos,
		*nextPos;
{
	u_char	*cp;


	cp = (u_char *)entry->overflowMap + sizeof(u_int) + 
		(pos * (OFB_SIZE + sizeof(u_int)));
	bcopy(cp, nextPos, sizeof(u_int));
}


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


u_int varcharWrite(entry, data, length)
	cache_t	*entry;
	u_char	*data;
	int	length;
{
	u_char	*cp;
	int	remain,
		numBytes;
	u_int	pos,
		lastPos;

	remain = strlen((char *)data) - length;
	numBytes = remain % OFB_SIZE;
	lastPos = NO_POS;
	while(remain)
	{
		pos = popOverflowPos(entry);
		cp = data + length + remain - numBytes;
		pos = writeOverflow(entry, pos, lastPos, cp, numBytes);
                msqlDebug2(MOD_TEXT,"Wrote %d bytes at overflow %d\n",
			numBytes, pos);
		remain -= numBytes;
		numBytes = OFB_SIZE;
		lastPos = pos;
	}
	return(lastPos);
}




u_char *varcharRead(entry, data, fieldLen)
	cache_t	*entry;
	u_char	*data;
	int	fieldLen;
{
	u_char	*value,
		*cp;
	u_int	pos,
		nextPos;
	int	length,
		numBytes;

	bcopy(data,&length, sizeof(int));
	if (length <= 0)
		return((u_char *)strdup(""));
	bcopy(data + sizeof(int) ,&pos, sizeof(u_int));
	value = (u_char *)malloc(length + 1);
	if (!value)
	{
		return(NULL);
	}
	cp = value;
	if (fieldLen > length)
		numBytes = length;
	else
		numBytes = fieldLen;
	
	bcopy(data + sizeof(int)+ sizeof(u_int), cp, numBytes);
	cp += numBytes;
	length -= numBytes;
	while(pos != NO_POS)
	{
		if (length > OFB_SIZE)
			numBytes = OFB_SIZE;
		else
			numBytes = length;
		if(readOverflow(entry, pos, &nextPos, cp, numBytes) < 0)
		{
			nextPos = NO_POS;
			bcopy(&fieldLen, data, sizeof(fieldLen));
		}
                msqlDebug2(MOD_TEXT,"Read %d bytes at overflow %d\n",
			numBytes, pos);
		pos = nextPos;
		cp += numBytes;
		length -= numBytes;
	}
	/* RNS
	 * malloc is not guaranteed to initialize memory and
	 * varChars (TEXT) are not stored with nul-termination.
	 */
	*cp = '\0';
	return(value);
}



void varcharDelete(entry, pos)
	cache_t	*entry;
	u_int	pos;
{
	u_int	nextPos;

	while(pos != NO_POS)
	{
		nextOverflow(entry, pos, &nextPos);
		pushOverflowPos(entry, pos);
		pos = nextPos;
	}
}


/* RNS
 * compareVarChar -- used to compare two TEXT fields from same table
 * (the same entry and the same fieldLen).
 */
int varcharCompare(entry, data1, data2, fieldLen)
	cache_t	*entry;
	u_char	*data1,
		*data2;
	int	fieldLen;
{
	u_char	*cp1, 	*cp2,
		buf1[OFB_SIZE],
		buf2[OFB_SIZE];
	int	count1, count2,
		d1Len, 	d2Len,
		segLen1,segLen2;
	u_int	pos1, 	pos2;

	/* RNS Get the lengths of the data */
	bcopy(data1,&d1Len, sizeof(int));
	bcopy(data2,&d2Len, sizeof(int));

	/* RNS Short circuit for both or either of zero length */
	if (d1Len == 0 && d2Len == 0)
		return(0);
	if (d1Len == 0)
		return(-1);
	if (d2Len == 0)
		return(1);

	/* RNS Get overflow buffers, if any */
	bcopy(data1 + sizeof(int) ,&pos1, sizeof(u_int));
	bcopy(data2 + sizeof(int) ,&pos2, sizeof(u_int));

	/* RNS Position pointers to row data to get comparison started */
	cp1 = data1 + sizeof(int) + sizeof(u_int);
	cp2 = data2 + sizeof(int) + sizeof(u_int);

	/* RNS
	 * Comparison is segmented, row followed by overflow buffers.
	 * Lengths need to be watched carefully.
	 */
	segLen1 = count1 = d1Len > fieldLen? fieldLen : d1Len;
	segLen2 = count2 = d2Len > fieldLen? fieldLen : d2Len;

	while(d1Len && d2Len)
	{
		while(count1 && count2)
		{
			/* RNS
			 * I have re-arranged checks because I think (perhaps
			 * erroneously) inequality likely more common.
			 * It also eliminates need for equality comparison.
			 */
			if (*cp1 < *cp2)
				return(-1);
			if (*cp2 < *cp1)
				return(1);
			/* RNS
			 * Here we have identical characters and because
			 * lengths are used, we don't need to bother with
			 * nul character check.
			 */
			count1--;
			count2--;
			cp1++;
			cp2++;
		}

		/* RNS
		 * Check counts for when strings end in same "segment":
		 * We know that at least one has reached the end, but
		 * both must end in order to continue on to another
		 * segment.
		 */
		if (count1) /* d1 is longer */
			return(1);
		if (count2) /* d2 is longer */
			return(-1);

		/* RNS
		 * If both have more buffers (segments), then
		 *   go on to another buffer full,
		 * else
		 *   get out of loop (and compare the lengths).
		 */
		d1Len -= segLen1;
		d2Len -= segLen2;
		if (d1Len == 0 || d2Len == 0)
			break;

		/* RNS Get next d1 buffer */
		readOverflow(entry, pos1, &pos1, buf1,
			d1Len>OFB_SIZE? OFB_SIZE : d1Len);
		cp1 = buf1;
		count1 = segLen1 = d1Len>OFB_SIZE? OFB_SIZE : d1Len;

		/* RNS Get next d2 buffer */
		readOverflow(entry, pos2, &pos2, buf2,
			d2Len>OFB_SIZE? OFB_SIZE : d2Len);
		cp2 = buf2;
		count2 = segLen2 = d2Len>OFB_SIZE? OFB_SIZE : d2Len;
	}

	/* RNS
	 * At least one has reached the end, we use the same check as
	 * during short circuit at beginning.
	 */
	if (d1Len == 0 && d2Len == 0)
		return(0);
	if (d1Len == 0)
		return(-1);
	/* if (d2Len == 0) */
	return(1);
}


int varcharMatch(entry, data, cp, length, op)
	cache_t		*entry;
	u_char		*data;
	char		*cp;
	int		length,
			op;
{
	u_char		*cp1, 	*cp2,
			buf[OFB_SIZE];
	int		count,
			dLen,
			segLen,
			cmp,
			cpLen,
			result = 0;
	u_int		pos;
	vc_cursor	*cursor;


	/*
	** If it's a regex match then punt it to that routine
	*/
	if (op==LIKE_OP || op==CLIKE_OP || op==NOT_LIKE_OP || op==NOT_CLIKE_OP)
	{
		char	ignoreCase = 0;

		cursor = varcharMakeCursor(entry, data, length);
		if (op == CLIKE_OP || op == NOT_CLIKE_OP)
			ignoreCase = 1;
		result = likeTest((void*)cursor, cp, cursor->dataLen,
			ignoreCase, TEXT_TYPE);
		free(cursor);
		if (op == LIKE_OP || op == CLIKE_OP)
		{
			return(result);
		}
		else
		{
			return(!result);
		}
	}

	/*
	** OK, not a regex.  Just do a normal byte by byte comparison
	*/
	cmp = 0; /* current value of match, initially equal */
	bcopy(data,&dLen, sizeof(int)); /* get length of TEXT data */
	cpLen = strlen(cp); /* get length of string */

	if (dLen == 0 || cpLen == 0)
	{
		/* if either or both are zero length,
		 * set cmp so that inequalities work.
		 * Longer string is greater.
		 */
		cmp = dLen - cpLen;
	}
	else
	{
		/* examine individual characters */

		bcopy(data + sizeof(int) ,&pos, sizeof(u_int));

		cp1 = data + sizeof(int) + sizeof(u_int);
		cp2 = (u_char *)cp;
		segLen = count = dLen > length ? length : dLen;
		while(dLen && cpLen)
		{
			while(count && cpLen)
			{
				if ((cmp = *cp1 - *cp2) != 0)
					break;
				count--;
				cp1++;
				cpLen--;
				cp2++;
			}

			/* RNS
			 * If inner loop decided the value,
			 * break this loop keeping value.
			 */
			if (cmp != 0)
				break;

			/* RNS
			 * If TEXT is longer than cp,
			 * artificially set cmp and break
			 */
			if (count)
			{
				cmp = 1;
				break;
			}

			/* RNS
			 * At this point count is 0.
			 * If both TEXT and string have more, then
			 *   go on
			 * else
			 *   get out of loop (and compare the lengths).
			 */
			dLen -= segLen;
			if (dLen == 0 || cpLen == 0)
				break;

			/* RNS Get next buffer of TEXT */
			readOverflow(entry, pos, &pos, buf,
				dLen>OFB_SIZE? OFB_SIZE : dLen);
			cp1 = buf;
			count = segLen = dLen>OFB_SIZE? OFB_SIZE : dLen;
		}

		/* RNS
		 * Either they aren't equal or at least one has reached
		 * the end:
		 * If they are still equal,
		 * then the lengths must be used as the final arbiter.
		 */
		if (cmp == 0)
			cmp = dLen - cpLen;
	}

	switch(op)
	{
		case EQ_OP:
			result = (cmp == 0);
			break;
			
		case NE_OP:
			result = (cmp != 0);
			break;

		case LT_OP:
			result = (cmp < 0);
			break;

		case GT_OP:
			result = (cmp > 0);
			break;

		case LE_OP:
			result = (cmp <= 0);
			break;

		case GE_OP:
			result = (cmp >= 0);
			break;
	}
	return(result);
}

vc_cursor *varcharMakeCursor(cacheEntry, data, fieldLen)
	cache_t		*cacheEntry;
	u_char		*data;
	u_int		fieldLen;
{
	vc_cursor	*new;

	new = (vc_cursor*)malloc(sizeof(vc_cursor));
	bcopy(data,&new->dataLen, sizeof(int));
	bcopy(data + sizeof(int) ,&new->pos, sizeof(u_int));
	new->cp = NULL;
	new->data = (char*)data;
	new->segLen = new->dataLen > fieldLen? fieldLen : new->dataLen;
	new->remain = new->segLen;
	new->cacheEntry = cacheEntry;
	return(new);
}

char *varcharGetNext(cursor)
	vc_cursor	*cursor;
{
	char		*cp;
	static char	blank = 0;

	if (cursor->remain == 0)
	{
		cursor->dataLen -= cursor->segLen;
		if (cursor->dataLen == 0)
			return(&blank);
		if (cursor->dataLen > OFB_SIZE)
			cursor->segLen = OFB_SIZE;
		else
			cursor->segLen = cursor->dataLen;
		readOverflow(cursor->cacheEntry, cursor->pos, &cursor->pos, 
			(u_char*)cursor->buf, cursor->segLen);
		cursor->cp = cursor->buf;
		cursor->remain = cursor->segLen;
	}
	cursor->remain--;
	if (cursor->cp == NULL)
	{
		cursor->cp = cursor->data + sizeof(int) + sizeof(u_int);
	}
	cp = cursor->cp;
	cursor->cp++;
	return(cp);
}


void *varcharDupCursor(cursor)
	vc_cursor	*cursor;
{
	vc_cursor	*new;

	new = (vc_cursor *)malloc(sizeof(vc_cursor));
	bcopy(cursor,new,sizeof(vc_cursor));
	return((void *)new);
}
