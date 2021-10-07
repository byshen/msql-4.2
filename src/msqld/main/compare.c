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
** $Id: compare.c,v 1.27 2012/01/15 06:19:59 bambi Exp $
**
*/

/*
** Module	: main : compare
** Purpose	: 
** Exports	: 
** Depends Upon	: 
*/


#define HAVE_STRCOLL

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
#ifdef HAVE_STDINT_H
#  include <stdint.h>
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
#include <msqld/main/main.h>
#include <msqld/main/yaccer.h>
#include <msqld/main/varchar.h>
#include <msqld/main/sysvar.h>
#include <msqld/main/util.h>
#include <msqld/main/table.h>
#include <msqld/main/compare.h>
#include <msqld/main/parse.h>
#include <msqld/main/regex.h>
#include <msqld/main/memory.h>
#include <common/types/types.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

/* HACK*/
extern	char    errMsg[];

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

/*
** Operator class macros
*/
#define ISA_NULL_OP(op) ((op == EQ_OP) || (op == NE_OP))
#define ISA_LIKE_OP(op) ((op >= LIKE_OP) && (op <= NOT_SLIKE_OP))


int localByteCmp(b1, b2, len)
        void 	*b1, 
		*b2;
        int	len;
{
        register u_char *p1, *p2;

        if (len == 0)
                return(0);
        p1 = (u_char *)b1;
        p2 = (u_char *)b2;
        while (len)
	{
                if (*p1 != *p2)
                        break;
		p1++;
		p2++;
		len--;
	}
	if (len == 0)
		return(0);
	if (*p1 > *p2)
		return(1);
        return(-1);
}


/****************************************************************************
**      _byteMatch
**
**      Purpose : comparison suite for single bytes.
**      Args    :
**      Returns :
**      Notes   : in-lined for performance
**
*/

#define byteMatch(v1,v2,op, result)				\
{								\
        switch(op)						\
        {							\
                case EQ_OP:					\
                        result = ((char)v1 == (char)v2);	\
                        break;					\
                case NE_OP:					\
                        result = ((char)v1 != (char)v2);	\
                        break;					\
                case LT_OP:					\
                        result = ((char)v1 < (char)v2);		\
                        break;					\
                case LE_OP:					\
                        result = ((char)v1 <= (char)v2);	\
                        break;					\
                case GT_OP:					\
                        result = ((char)v1 > (char)v2);		\
                        break;					\
                case GE_OP:					\
                        result = ((char)v1 >= (char)v2);	\
                        break;					\
        }							\
}




/****************************************************************************
** 	_intMatch
**
**	Purpose	: comparison suite for integer fields.
**	Args	: 
**	Returns	: 
**	Notes	: in-lined for performance
*/

#define int32Match(v1,v2,op,result)			\
{							\
	switch(op)					\
	{						\
		case EQ_OP:				\
			result = (v1 == v2); 		\
			break;				\
		case NE_OP:				\
			result = (v1 != v2);		\
			break;				\
		case LT_OP:				\
			result = (v1 < v2);		\
			break;				\
		case LE_OP:				\
			result = (v1 <= v2);		\
			break;				\
		case GT_OP:				\
			result = (v1 > v2);		\
			break;				\
		case GE_OP:				\
			result = (v1 >= v2);		\
			break;				\
	}						\
}

#define int8Match(v1,v2,op,result) 	int32Match(v1,v2,op,result)
#define int16Match(v1,v2,op,result) 	int32Match(v1,v2,op,result)

/****************************************************************************
** 	_uintMatch
**
**	Purpose	: comparison suite for unsigned integer fields.
**	Args	: 
**	Returns	: 
**	Notes	: in-lined for performance
*/

#define uint8Match(v1,v2,op,result)			\
{							\
	switch(op)					\
	{						\
		case EQ_OP:				\
			result = ((uint8_t)v1 == (uint8_t)v2); \
			break;				\
		case NE_OP:				\
			result = ((uint8_t)v1 != (uint8_t)v2);\
			break;				\
		case LT_OP:				\
			result = ((uint8_t)v1 < (uint8_t)v2);\
			break;				\
		case LE_OP:				\
			result = ((uint8_t)v1 <= (uint8_t)v2);\
			break;				\
		case GT_OP:				\
			result = ((uint8_t)v1 > (uint8_t)v2);\
			break;				\
		case GE_OP:				\
			result = ((uint8_t)v1 >= (uint8_t)v2);\
			break;				\
	}						\
}



#define uint16Match(v1,v2,op,result)			\
{							\
	switch(op)					\
	{						\
		case EQ_OP:				\
			result = ((uint16_t)v1 == (uint16_t)v2); \
			break;				\
		case NE_OP:				\
			result = ((uint16_t)v1 != (uint16_t)v2);\
			break;				\
		case LT_OP:				\
			result = ((uint16_t)v1 < (uint16_t)v2);\
			break;				\
		case LE_OP:				\
			result = ((uint16_t)v1 <= (uint16_t)v2);\
			break;				\
		case GT_OP:				\
			result = ((uint16_t)v1 > (uint16_t)v2);\
			break;				\
		case GE_OP:				\
			result = ((uint16_t)v1 >= (uint16_t)v2);\
			break;				\
	}						\
}



#define uint32Match(v1,v2,op,result)			\
{							\
	switch(op)					\
	{						\
		case EQ_OP:				\
			result = ((uint32_t)v1 == (uint32_t)v2); \
			break;				\
		case NE_OP:				\
			result = ((uint32_t)v1 != (uint32_t)v2);\
			break;				\
		case LT_OP:				\
			result = ((uint32_t)v1 < (uint32_t)v2);\
			break;				\
		case LE_OP:				\
			result = ((uint32_t)v1 <= (uint32_t)v2);\
			break;				\
		case GT_OP:				\
			result = ((uint32_t)v1 > (uint32_t)v2);\
			break;				\
		case GE_OP:				\
			result = ((uint32_t)v1 >= (uint32_t)v2);\
			break;				\
	}						\
}





#ifdef HUGE_T

/****************************************************************************
** 	_int64Match
**
**	Purpose	: comparison suite for 64bit integer fields.
**	Args	: 
**	Returns	: 
**	Notes	: in-lined for performance
*/


static int int64Match(v1,v2,op)
        HUGE_T 	v1,
                v2;
        int     op;
{
	int	result = 0;

	switch(op)
	{
		case EQ_OP:
			result = (v1 == v2);
			break;
		case NE_OP:
			result = (v1 != v2);
			break;
		case LT_OP:
			result = (v1 < v2);
			break;
		case LE_OP:
			result = (v1 <= v2);
			break;
		case GT_OP:
			result = (v1 > v2);
			break;	
		case GE_OP:
			result = (v1 >= v2);
			break;	
	}
	return(result);
}

/****************************************************************************
** 	_uint64Match
**
**	Purpose	: comparison suite for unsigned 64bit integer fields.
**	Args	: 
**	Returns	: 
**	Notes	: in-lined for performance
*/


static int uint64Match(int1,int2,op)
        UHUGE_T	int1,
                int2;
        int     op;
{
	int	result = 0;

	switch(op)
	{
		case EQ_OP:
			result = (int1 == int2);
			break;
		case NE_OP:
			result = (int1 != int2);
			break;
		case LT_OP:
			result = (int1 < int2);
			break;
		case LE_OP:
			result = (int1 <= int2);
			break;
		case GT_OP:
			result = (int1 > int2);
			break;	
		case GE_OP:
			result = (int1 >= int2);
			break;	
	}
	return(result);
}


#else
#define int64Match	intMatch
#define uint64Match	uintMatch
#endif


/****************************************************************************
** 	_charMatch
**
**	Purpose	: Comparison suite for text fields
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

static int charMatch(v1,v2,op,maxLen)
	char	*v1,
		*v2;
	int	op,
		maxLen;
{
	int	v1Len, v2Len; /* actual length of input data */
	int	result = 0,
		cmp = 0;

	/* needed for both ordinary and *LIKE operators */
	v1Len = regexStringLength( v1, maxLen ); 
	v2Len = strlen( v2 );

	/* common stuff for ordinary operators (=, <, ...) */
	if (!ISA_LIKE_OP(op))
	{
		/*
		** We can short cut this if the first char is different. Save
		** a few CPU cycles.
		*/
		if (*v1 != *v2)
		{
			if (*v1 < *v2)
				cmp = -1;
			else
				cmp = 1;
		}
		else
		{
#ifdef HAVE_STRCOLL
			{
				/*
				** Sadly there isn't an strncoll so we must
				** ensure the data from the row is NULL
				** terminated.
				*/
				char tmp;
				tmp = *(v1+v1Len);
				*(v1+v1Len) = 0;
				cmp = strcoll( v1, v2);
				*(v1+v1Len) = tmp;
			}
#else
			cmp = strncmp(v1,v2, (v1Len < v2Len) ? v1Len : v2Len );
#endif
		}
		if (cmp == 0)
		{
			cmp = v1Len - v2Len;
		}
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
			
		case LE_OP:
			result = (cmp <= 0);
			break;
			
		case GT_OP:
			result = (cmp > 0);
			break;
			
		case GE_OP:
			result = (cmp >= 0);
			break;

		case RLIKE_OP:
			result = rLikeTest(v1,v2,v1Len);
			break;

		case LIKE_OP:
			result = likeTest(v1,v2,v1Len, 0, CHAR_TYPE);
			break;

		case CLIKE_OP:
			result = likeTest(v1,v2,v1Len, 1, CHAR_TYPE);
			break;

		case SLIKE_OP:
			result = sLikeTest(v1,v2,v1Len);
			break;

		case NOT_RLIKE_OP:
			result = !(rLikeTest(v1,v2,v1Len));
			break;

		case NOT_LIKE_OP:
			result = !(likeTest(v1,v2,v1Len, 0, CHAR_TYPE));
			break;

		case NOT_CLIKE_OP:
			result = !(likeTest(v1,v2,v1Len, 1, CHAR_TYPE));
			break;

		case NOT_SLIKE_OP:
			result = !(sLikeTest(v1,v2,v1Len));
			break;
	}
	return(result);
}


/****************************************************************************
** 	_byteRangeMatch
**
**	Purpose	: Comparison suite for byte ranges (i.e. basetype = BYTE)
**	Args	: 
**	Returns	: 
**	Notes	: 
*/

static int byteRangeMatch(v1,v2,op,len)
	void	*v1,
		*v2;
	int	op,
		len;
{
	int	result = 0,
		cmp = 0;

	cmp = localByteCmp( v1, v2, len);
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
			
		case LE_OP:
			result = (cmp <= 0);
			break;
			
		case GT_OP:
			result = (cmp > 0);
			break;
			
		case GE_OP:
			result = (cmp >= 0);
			break;
	}
	return(result);
}


/****************************************************************************
** 	_cidr4Match
**
**	Purpose	: Comparison suite for cidr4 fields
**	Args	: 
**	Returns	: 
**	Notes	: 
*/



int cidr4Match(p1, p2, op)
	void	*p1,
		*p2;
	int	op;
{
	u_char	byte1, byte2, byte3, byte4, byte5;
	u_int	addr1, addr2,
		len, len1, len2,
		mask;
	int	result;

	byte1 = (u_char)*(u_char*)(p1);
	byte2 = (u_char)*(u_char*)(p1 + 1);
	byte3 = (u_char)*(u_char*)(p1 + 2);
	byte4 = (u_char)*(u_char*)(p1 + 3);
	byte5 = (u_char)*(u_char*)(p1 + 4);
	addr1 = ( byte1 << 24) + (byte2 << 16) + (byte3 << 8) + byte4;
	len1 = byte5;

	byte1 = (u_char)*(u_char*)(p2);
	byte2 = (u_char)*(u_char*)(p2 + 1);
	byte3 = (u_char)*(u_char*)(p2 + 2);
	byte4 = (u_char)*(u_char*)(p2 + 3);
	byte5 = (u_char)*(u_char*)(p2 + 4);
	addr2 = ( byte1 << 24) + (byte2 << 16) + (byte3 << 8) + byte4;
	len2 = byte5;

	len = (len1 < len2 ? len1 : len2);
	switch(len)
	{
		case 0 : mask = 0; break;
		case 1 : mask = 2147483648u; break;
		case 2 : mask = 3221225472u; break;
		case 3 : mask = 3758096384u; break;
		case 4 : mask = 4026531840u; break;
		case 5 : mask = 4160749568u; break;
		case 6 : mask = 4227858432u; break;
		case 7 : mask = 4261412864u; break;
		case 8 : mask = 4278190080u; break;
		case 9 : mask = 4286578688u; break;
		case 10 : mask = 4290772992u; break;
		case 11 : mask = 4292870144u; break;
		case 12 : mask = 4293918720u; break;
		case 13 : mask = 4294443008u; break;
		case 14 : mask = 4294705152u; break;
		case 15 : mask = 4294836224u; break;
		case 16 : mask = 4294901760u; break;
		case 17 : mask = 4294934528u; break;
		case 18 : mask = 4294950912u; break;
		case 19 : mask = 4294959104u; break;
		case 20 : mask = 4294963200u; break;
		case 21 : mask = 4294965248u; break;
		case 22 : mask = 4294966272u; break;
		case 23 : mask = 4294966784u; break;
		case 24 : mask = 4294967040u; break;
		case 25 : mask = 4294967168u; break;
		case 26 : mask = 4294967232u; break;
		case 27 : mask = 4294967264u; break;
		case 28 : mask = 4294967280u; break;
		case 29 : mask = 4294967288u; break;
		case 30 : mask = 4294967292u; break;
		case 31 : mask = 4294967294u; break;
		case 32 : mask = 4294967295u; break;
	}

	/*
	** The manual (and slow) way of working out a mask
	**
	while(count <  len)
        {
                mask = (mask << 1) + 1;
                count++;
        }
        mask = mask << (32 - len);
	**
	*/

	switch(op)
	{
		case EQ_OP:
			result = (addr1 == addr2) && (len1 == len2);
			break;

		case NE_OP:
			result = (addr1 != addr2) || (len1 != len2);
			break;

		case LT_OP:
			result = ((addr1 & mask) == (addr2 & mask)) &&
				(len1 < len2);
			break;

		case LE_OP:
			result = ((addr1 & mask) == (addr2 & mask)) &&
				(len1 <= len2);
			break;

		case GT_OP:
			result = ((addr1 & mask) == (addr2 & mask)) &&
				(len1 > len2);
			break;

		case GE_OP:
			result = ((addr1 & mask) == (addr2 & mask)) &&
				(len1 >= len2);
			break;

		default:
			strcpy(errMsg, "Invalid cidr4 comparison");
			return(-1);
	}
	return(result);
}



/****************************************************************************
** 	_cidr6Match
**
**	Purpose	: Comparison suite for cidr6 fields
**	Args	: 
**	Returns	: 
**	Notes	: 
*/



int cidr6Match(p1, p2, op)
	void	*p1,
		*p2;
	int	op;
{
	u_char	byte1, byte2, byte3, byte4, lenByte;
	u_int	addr1, addr2;
	int	len, len1, len2, 
		tmpLen, tmpLen1, tmpLen2,
		mask;
	int	count, result;
	void	*v1, *v2;

	lenByte = (u_char)*(u_char*)(p1+16);
	len1 = lenByte;
	lenByte = (u_char)*(u_char*)(p2+16);
	len2 = lenByte;
	len = (len1 < len2 ? len1 : len2);

	count = 0;
	result = 0;
	while (count < 4)
	{
		v1 = p1 + (count * 4);
		byte1 = (u_char)*(u_char*)(v1);
		byte2 = (u_char)*(u_char*)(v1 + 1);
		byte3 = (u_char)*(u_char*)(v1 + 2);
		byte4 = (u_char)*(u_char*)(v1 + 3);
		addr1 = ( byte1 << 24) + (byte2 << 16) + (byte3 << 8) + byte4;

		v2 = p2 + (count * 4);
		byte1 = (u_char)*(u_char*)(v2);
		byte2 = (u_char)*(u_char*)(v2 + 1);
		byte3 = (u_char)*(u_char*)(v2 + 2);
		byte4 = (u_char)*(u_char*)(v2 + 3);
		addr2 = ( byte1 << 24) + (byte2 << 16) + (byte3 << 8) + byte4;

		tmpLen = len - (32 * count);
		if (tmpLen < 0)
		{
			break;
		}
		if (tmpLen  > 32)
		{
			tmpLen = 32;
		}
		tmpLen1 = len1 - (32 * count);
		if (tmpLen1  > 32)
		{
			tmpLen1 = 32;
		}
		tmpLen2 = len2 - (32 * count);
		if (tmpLen2  > 32)
		{
			tmpLen2 = 32;
		}

		switch(tmpLen)
		{
			case 0 : mask = 0; break;
			case 1 : mask = 2147483648u; break;
			case 2 : mask = 3221225472u; break;
			case 3 : mask = 3758096384u; break;
			case 4 : mask = 4026531840u; break;
			case 5 : mask = 4160749568u; break;
			case 6 : mask = 4227858432u; break;
			case 7 : mask = 4261412864u; break;
			case 8 : mask = 4278190080u; break;
			case 9 : mask = 4286578688u; break;
			case 10 : mask = 4290772992u; break;
			case 11 : mask = 4292870144u; break;
			case 12 : mask = 4293918720u; break;
			case 13 : mask = 4294443008u; break;
			case 14 : mask = 4294705152u; break;
			case 15 : mask = 4294836224u; break;
			case 16 : mask = 4294901760u; break;
			case 17 : mask = 4294934528u; break;
			case 18 : mask = 4294950912u; break;
			case 19 : mask = 4294959104u; break;
			case 20 : mask = 4294963200u; break;
			case 21 : mask = 4294965248u; break;
			case 22 : mask = 4294966272u; break;
			case 23 : mask = 4294966784u; break;
			case 24 : mask = 4294967040u; break;
			case 25 : mask = 4294967168u; break;
			case 26 : mask = 4294967232u; break;
			case 27 : mask = 4294967264u; break;
			case 28 : mask = 4294967280u; break;
			case 29 : mask = 4294967288u; break;
			case 30 : mask = 4294967292u; break;
			case 31 : mask = 4294967294u; break;
			case 32 : mask = 4294967295u; break;
		}

		switch(op)
		{
			case EQ_OP:
				result = (addr1 == addr2) && (tmpLen1==tmpLen2);
				break;

			case NE_OP:
				result = (addr1 != addr2) || (tmpLen1!=tmpLen2);
				break;

			case LT_OP:
				result = ((addr1 & mask) == (addr2 & mask)) &&
					(tmpLen1 < tmpLen2);
				break;

			case LE_OP:
				result = ((addr1 & mask) == (addr2 & mask)) &&
					(tmpLen1 <= tmpLen2);
				break;

			case GT_OP:
				result = ((addr1 & mask) == (addr2 & mask)) &&
					(tmpLen1 > tmpLen2);
				break;

			case GE_OP:
				result = ((addr1 & mask) == (addr2 & mask)) &&
					(tmpLen1 >= tmpLen2);
				break;

			default:
				strcpy(errMsg, "Invalid cidr6 comparison");
				return(-1);
		}
		if (result != 0 &&
			!( (addr1 == addr2) && 
			   (tmpLen == 32) &&
			   (op == EQ_OP || op == LE_OP || op == GE_OP)
			)
		    )
		{
			return(result);
		}
		count++;
	}
	return(result);
}


/****************************************************************************
** 	_realMatch
**
**	Purpose	: Comparison suite for real fields
**	Args	: 
**	Returns	: 
**	Notes	: in-lined for performance
*/

#define realMatch(v1,v2,op, result)			\
{							\
	switch(op)					\
	{						\
		case EQ_OP:				\
			result = (v1 == v2);		\
			break;				\
		case NE_OP:				\
			result = (v1 != v2);		\
			break;				\
		case LT_OP:				\
			result = (v1 < v2);		\
			break;				\
		case LE_OP:				\
			result = (v1 <= v2);		\
			break;				\
		case GT_OP:				\
			result = (v1 > v2);		\
			break;				\
		case GE_OP:				\
			result = (v1 >= v2);		\
			break;				\
	}						\
}





/****************************************************************************
** Row comparison routines
*/



static int processBetweenMatch(cacheEntry,curCond,data, offset, tmpVal)
	cache_t	*cacheEntry;
	mCond_t	*curCond;
	u_char	*data;
	int	*offset;
	mVal_t	*tmpVal;
{
	int	tmp = 0;
	double	fv;
	char	*cp;
	int8_t	int8;
	int16_t int16;
	int	int32;
	int64_t	int64;


	switch(typeBaseType(curCond->type))
	{
		case INT8_TYPE:
			bcopy((data + *offset +1),&int8, 1);
			int8Match(int8,curCond->value->val.int8Val,GE_OP,tmp);
			if (tmp == 1)
			{
				int8Match(int8,curCond->maxValue->val.int8Val,
					LE_OP,tmp);
			}
			break;

		case UINT8_TYPE:
			bcopy((data + *offset +1),&int8, 1);
			uint8Match(int8,curCond->value->val.int8Val,GE_OP,tmp);
			if (tmp == 1)
			{
				uint8Match(int8,curCond->maxValue->val.int8Val,
					LE_OP, tmp);
			}
			break;

		case INT16_TYPE:
			bcopy((data + *offset +1),&int16, 2);
			int16Match(int16,curCond->value->val.int16Val, GE_OP, 
				tmp);
			if (tmp == 1)
			{
				int16Match(int16,
					curCond->maxValue->val.int16Val,
					LE_OP,tmp);
			}
			break;

		case UINT16_TYPE:
			bcopy((data + *offset +1),&int16, 2);
			uint16Match(int16,curCond->value->val.int16Val,GE_OP,
				tmp);
			if (tmp == 1)
			{
				uint16Match(int16,
					curCond->maxValue->val.int16Val,
					LE_OP, tmp);
			}
			break;


		case INT32_TYPE:
			bcopy4((data + *offset +1),&int32);
			int32Match(int32,curCond->value->val.int32Val,GE_OP,
				tmp);
			if (tmp == 1)
			{
				int32Match(int32,
					curCond->maxValue->val.int32Val,
					LE_OP,tmp);
			}
			break;


		case UINT32_TYPE:
			bcopy4((data + *offset +1),&int32);
			uint32Match(int32,curCond->value->val.int32Val,GE_OP,
				tmp);
			if (tmp == 1)
			{
				uint32Match(int32,
					curCond->maxValue->val.int32Val,
					LE_OP, tmp);
			}
			break;

		case INT64_TYPE:
			bcopy((data + *offset +1), &int64, sizeof(HUGE_T));
			tmp = int64Match(int64, curCond->value->val.int64Val,
				 GE_OP);
			if (tmp == 1)
			{
				tmp = int64Match(int64,
					curCond->maxValue->val.int64Val,LE_OP);
			}
			break;


		case UINT64_TYPE:
			bcopy((data + *offset +1), &int64, sizeof(HUGE_T));
			tmp = uint64Match(int64, curCond->value->val.int64Val,
				GE_OP);
			if (tmp == 1)
			{
				tmp = uint64Match(int64,
					curCond->maxValue->val.int64Val, LE_OP);
			}
			break;

		case CHAR_TYPE:
			cp = (char *)data + *offset +1;
			tmp = charMatch(cp,(char*)curCond->value->val.charVal,
				GE_OP, curCond->length);
			if (tmp == 1)
			{
				tmp = charMatch(cp,
					(char*)curCond->maxValue->val.charVal,
					LE_OP, curCond->length);
			}
			break;


		case TEXT_TYPE:
			cp = (char *)data + *offset +1;
			tmp = varcharMatch(cacheEntry,(u_char*)cp, 
				(char*)curCond->value->val.charVal,
				curCond->length, GE_OP);
			if (tmp == 1)
			{
				tmp = varcharMatch(cacheEntry,(u_char*)cp, 
					(char*)curCond->maxValue->val.charVal,
					curCond->length, LE_OP);
			}
			break;

		case REAL_TYPE:
			bcopy8((data + *offset + 2),&fv);
			realMatch(fv,curCond->value->val.realVal,GE_OP, tmp);
			if (tmp == 1)
			{
				realMatch(fv,curCond->maxValue->val.realVal,
					LE_OP, tmp);
			}
			break;

		case BYTE_TYPE:
			cp = (char *)data + *offset +1;
			tmp = byteRangeMatch(cp,curCond->value->val.byteVal,
				GE_OP, curCond->length);
			if (tmp == 1)
			{
				tmp = byteRangeMatch(cp,
					curCond->maxValue->val.byteVal,
					LE_OP, curCond->length);
			}
			break;

	}
	return(tmp);
}
	

static int processCondMatch(cacheEntry,curCond,value,row, data, offset,
		tmpVal)
	cache_t	*cacheEntry;
	mCond_t	*curCond;
	mVal_t	*value;
	row_t	*row;
	u_char	*data;
	int	*offset;
	mVal_t	*tmpVal;
{
	int		tmp = 0;
	int8_t		int8;
	uint8_t		uint8;
	int16_t		int16;
	uint16_t	uint16;
	int		int32;
	double		fv;
	char		*cp;
#ifdef HUGE_T
	HUGE_T		int64_1,
			int64_2;
#endif


	if (curCond->sysvar)
	{
		tmp = sysvarCompare(cacheEntry,row, curCond, value);
		return(tmp);
	}

	/*
	** Check to see if there is a type specific comparison routine
	*/
	switch(curCond->type)
	{
		case CIDR4_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			tmp = cidr4Match((data + *offset + 1), 
				value->val.byteVal, curCond->op);
			if (tmp < 0)
				return(-2);
			return(tmp);
			break;

		case CIDR6_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			tmp = cidr6Match((data + *offset + 1), 
				value->val.byteVal, curCond->op);
			if (tmp < 0)
				return(-2);
			return(tmp);
			break;

	}

	/*
	** If not just use the base type's comparison routine
	*/

	switch(typeBaseType(curCond->type))
	{
		case INT32_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy4((data + *offset +1), &int32);
			int32Match(int32,value->val.int32Val,curCond->op,tmp);
			break;

		case INT8_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy((data + *offset +1), &int8, 1);
			int8Match(int8,value->val.int8Val,curCond->op,tmp);
			break;

		case INT16_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy((data + *offset +1), &int16, 2);
			int16Match(int16,value->val.int16Val,curCond->op,tmp);
			break;

		case UINT32_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy4((data + *offset +1), &int32);
			uint32Match(int32,value->val.int32Val,curCond->op,tmp);
			break;

		case UINT8_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy((data + *offset +1), &uint8, 1);
			uint8Match(uint8,value->val.int8Val,curCond->op,tmp);
			break;

		case UINT16_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy((data + *offset +1), &uint16, 2);
			uint16Match(uint16,value->val.int16Val,curCond->op,tmp);
			break;


		case INT64_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy((data + *offset +1), &int64_1, sizeof(HUGE_T));
			bcopy(&value->val.int64Val, &int64_2, sizeof(HUGE_T));
			tmp=int64Match(int64_1,int64_2,curCond->op);
			break;

		case UINT64_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, INT_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			bcopy((data + *offset +1), &int64_1, sizeof(HUGE_T));
			bcopy(&value->val.int64Val, &int64_2, sizeof(HUGE_T));
			tmp=uint64Match(int64_1,int64_2,curCond->op);
			break;

		case CHAR_TYPE:
			if (curCond->sysvar)
			{
				tmp = sysvarCompare(cacheEntry,row,
					curCond, value);
				return(tmp);
			}
			cp = (char *)data + *offset +1;
			tmp = charMatch(cp,(char*)value->val.charVal,
				curCond->op, curCond->length);
			if (value == tmpVal)
			{
				free(tmpVal->val.charVal);
				tmpVal->val.charVal = NULL;
			}
			if (tmp < 0)
			{
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			break;

		case TEXT_TYPE:
			cp = (char *)data + *offset +1;
			if (curCond->op == RLIKE_OP || 
				curCond->op == SLIKE_OP||
			  	curCond->op == NOT_RLIKE_OP || 
				curCond->op == NOT_SLIKE_OP)
			{
				strcpy(errMsg, TEXT_REGEX_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			tmp = varcharMatch(cacheEntry,(u_char*)cp, 
				(char*)value->val.charVal,
				curCond->length, curCond->op);
			if (value == tmpVal)
			{
				free(tmpVal->val.charVal);
				tmpVal->val.charVal = NULL;
			}
			if (tmp < 0)
			{
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			break;

		case REAL_TYPE:
			if (curCond->sysvar)
			{
				tmp = sysvarCompare(cacheEntry,row,
					curCond, value);
				return(tmp);
			}
			bcopy8((data + *offset + 2),&fv);
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, REAL_LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			realMatch(fv,value->val.realVal, curCond->op, tmp);
			break;

		case BYTE_TYPE:
			if (ISA_LIKE_OP(curCond->op))
			{
				strcpy(errMsg, LIKE_ERROR);
				debugTrace(TRACE_OUT,"processCondMatch()");
				return(-2);
			}
			tmp = byteRangeMatch((data + *offset +1),
				value->val.byteVal, curCond->op,
				curCond->length);
			break;

	}
	return(tmp);
}
	


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/

int compareMatchRow(cacheEntry,row,conds,query)
	cache_t		*cacheEntry;
	row_t		*row;
	mCond_t		*conds;
	mQuery_t	*query;
{
	mCond_t		*curCond;
	int		result,
			freeTmpField = 0,
			tmp = 0;
	int		*offset,
			init=1,
			res;
	u_char		*data;
	mVal_t		*value,
			tmpVal;
	mField_t	*curField,
			tmpField,
			*tmpFieldPtr;
	int		tmpFlist[2],
			foundField;
	int		rhsType; /* saves rhs type prior to data fetch */
	int		lhsIsNull;/* temporary that indicates nullness of lhs */


	debugTrace(TRACE_IN,"compareMatchRow()");
	result=0;
	if (!conds)
	{
		debugTrace(TRACE_OUT,"compareMatchRow()");
		return(1);
	}
	data = row->data;
	curCond = conds;
	offset = conds->clist;
	while(curCond)
	{
		/*
		** If this is a subcond just recurse and continue
		*/
		if (curCond->subCond)
		{
			tmp = compareMatchRow(cacheEntry,row,curCond->subCond,
				query);
			if (tmp < 0)
				return(tmp);
			if (init)
			{
				result = tmp;
				init = 0;
			}
			else
			{
                        	switch(curCond->bool)
                        	{
                                	case NO_BOOL:
                                        	result = tmp;
                                        	break;

                                	case AND_BOOL:
                                        	result &= tmp;
                                        	break;

                                	case OR_BOOL:
                                        	result |= tmp;
                                        	break;
                        	}
			}
			curCond = curCond->next;
			continue;
		}



		/*
		** OK, it wasn't a sub cond.  Proceded as normal.
		**
		** If we are comparing 2 fields (e.g. in a join) then
		** grab the value of the second field so that we can do
		** the comparison.  Watch for type mismatches!
		*/
		foundField = 0;
		freeTmpField = 0;
		tmpField.value = NULL;
		rhsType = curCond->value->type;
		switch(curCond->value->type)
		{
		    case IDENT_TYPE:
			value = curCond->value;
			curField = cacheEntry->def;
			if (!*(value->val.identVal->seg1))
			{
				if (!cacheEntry->result)
				{
					strcpy(value->val.identVal->seg1,
						cacheEntry->table);
				}
				else
				{
					strcpy(errMsg,UNQUAL_ERROR);
					debugTrace(TRACE_OUT,
						"compareMatchRow()");
					return(-1);
				}
			}
			while(curField)
			{
				if (*(curField->table) != 
				    *(value->val.identVal->seg1) ||
				    *(curField->name) !=
				    *(value->val.identVal->seg2))
				{
					curField = curField->next;
					continue;
				}
				if (strcmp(curField->table,
					value->val.identVal->seg1) != 0 ||
				    strcmp(curField->name,
					value->val.identVal->seg2) != 0)
				{
					curField = curField->next;
					continue;
				}

				tmpFieldPtr = &tmpField;
				memCopyField(curField,tmpFieldPtr);
				tmpField.value=NULL;
				tmpField.next = NULL;
				utilSetupFields(cacheEntry,tmpFlist, &tmpField);
				tableExtractValues(cacheEntry,row,&tmpField,
					tmpFlist, query);
				bcopy(tmpField.value,&tmpVal,sizeof(mVal_t));

				/* RNS
				 * Character data needs to be copied, but
				 * only if there is data.
				 */
				if (tmpVal.type == CHAR_TYPE && !tmpVal.nullVal)
				{
				    tmpVal.val.charVal= (u_char*)
					malloc(curField->length + 1);
				    bcopy(tmpField.value->val.charVal,
					tmpVal.val.charVal, curField->length);
				    *(tmpVal.val.charVal+curField->length) = 0;
				}
				if (typeBaseType(tmpVal.type) == BYTE_TYPE && 
					!tmpVal.nullVal)
				{
				    tmpVal.val.byteVal= (u_char*)
					malloc(curField->length);
				    bcopy(tmpField.value->val.byteVal,
					tmpVal.val.byteVal, curField->length);
				}
				parseFreeValue(tmpField.value);
				tmpField.value = NULL;
				value = &tmpVal;
				foundField = 1;
				break;
			}
			if (!foundField)
			{
				snprintf(errMsg, MAX_ERR_MSG, BAD_FIELD_ERROR,
					value->val.identVal->seg1,
					value->val.identVal->seg2);
				msqlDebug2(MOD_ERR,"Unknown field \"%s.%s\"\n",
					value->val.identVal->seg1,
					value->val.identVal->seg2);
				debugTrace(TRACE_OUT,"compareMatchRow()");
				return(-1);
			}
			break;

		    case SYSVAR_TYPE:
			strcpy(tmpField.name,
				curCond->value->val.identVal->seg2);
			res = sysvarCheckVariable(cacheEntry, &tmpField);
                        if (res == -2)
                                return(-1);
			if (res == -1)
			{
                        	snprintf(errMsg, MAX_ERR_MSG, SYSVAR_ERROR, 
					curCond->value->val.identVal->seg2);
				return(-1);
			}

			sysvarGetVariable(cacheEntry,row,&tmpField,query);
			value = tmpField.value;
			freeTmpField = 1;
			break;

		    default:
			value = curCond->value;
			break;
		}


		/*
		** Ensure that the comparison is with the correct type.
		** We do this here and in utilSetupConds() as we have to wait
		** for the evaluation of field to field comparisons.  We
		** also fudge it for real/int comparisons.  It's done
		** in msqlutilpConds() to handle cases going to the
		** index lookup code and for literal comparisons.
		*/

		if(utilSetCondValueType(curCond, value) < 0)
		{
			if (freeTmpField)
				parseFreeValue(tmpField.value);
			return(-1);
		}


		/*
		** O.K. do the actual comparison
		*/
		if (*offset >=0)
		{
			lhsIsNull = (*(data + *offset) == 0);
		}
		else
		{
			/* 
			** Offset of -1 indicates a sysvar we don't
			** check the table data for a NULL value
			*/
			lhsIsNull = 0;
		}
		
		if ((rhsType == NULL_TYPE) && ISA_NULL_OP(curCond->op))
		{
			/* 
			** An explicit comparison to NULL.  
			*/
			byteMatch( *(data + *offset), 0, curCond->op, tmp );
		}
		else if (rhsType == NULL_TYPE)
		{
			/* 
			** SQL does not allow other operators for NULL.
			*/
			strcpy(errMsg, "Illegal operator applied to NULL.\n" );
			debugTrace(TRACE_OUT,"compareMatchRow()");
			return(-1);
		}
		else if (value->nullVal || lhsIsNull)
		{
			/* 
			 * SQL says that any compare of implicit NULL values
			 * should always fail (return false for now).
			 */
			tmp = 0;
		}
		else
		{
			if (curCond->op == BETWEEN_OP)
			{
				tmp = processBetweenMatch(cacheEntry,
					curCond, data, offset, &tmpVal);
			}
			else
			{
				tmp = processCondMatch(cacheEntry,curCond,
					value,row, data, offset, &tmpVal);
			}
		}
		if (freeTmpField)
			parseFreeValue(tmpField.value);

/*
		if (typeBaseType(value->type)==BYTE_TYPE && value->val.byteVal)
			free(value->val.byteVal);
		if (typeBaseType(value->type)==CHAR_TYPE && value->val.charVal)
			free(value->val.charVal);
*/

		if (tmp == -2)
			return(-1);

		if (init)
		{
			result = tmp;
			init = 0;
		}
		else
		{
			switch(curCond->bool)
			{
				case NO_BOOL:
					result = tmp;
					break;
	
				case AND_BOOL:
					result &= tmp;
					break;
	
				case OR_BOOL:
					result |= tmp;
					break;
			}
		}
		curCond = curCond->next;
		offset++;
	}
	debugTrace(TRACE_OUT,"compareMatchRow()");
	return(result);
}




int checkDupRow(entry,data1,data2)
	cache_t	*entry;
	u_char	*data1,
		*data2;
{
	mField_t *curField;
	u_char	*cp1, *cp2;
	int	res,
		offset;


	curField = entry->def;
	res = offset = 0;
	while(curField)
	{
		/*
		** Check for matching NULL values
		*/
		cp1 = data1+offset;
		cp2 = data2+offset;
		if (*cp1 != *cp2) 
		{
			res = 1;
			break;
		}
		if (*cp1 == 0) 
		{
			offset += curField->dataLength + 1;
			curField = curField->next;
			continue;
		}

		/*
		** Check the data
		*/
		cp1++;
		cp2++;
		switch (typeBaseType(curField->type))
		{
			case INT32_TYPE:
			case UINT32_TYPE:
			case REAL_TYPE:
				res = bcmp(cp1, cp2, curField->dataLength);
				break;
#ifdef HUGE_T
			case INT64_TYPE:
			case UINT64_TYPE:
				res = bcmp(cp1, cp2, curField->dataLength);
				break;
#endif

			case CHAR_TYPE:
			case BYTE_TYPE:
				res = bcmp(cp1, cp2, curField->length);
				break;

			case TEXT_TYPE:
				res = varcharCompare(entry,(u_char*)cp1,
					(u_char*)cp2, curField->length);
				break;
		}
		if (res != 0)
			break;
		offset += curField->dataLength + 1;
		curField = curField->next;
	}
	return(res);
}




int compareRows(entry,r1,r2,order,olist)
	cache_t	*entry;
	row_t	*r1,
		*r2;
	mOrder_t	*order;
	int	*olist;
{
	mOrder_t *curOrder;
	char	buf[sizeof(double)],
		*cp1,
		*cp2,
		tmp1,
		tmp2;
	u_char	*data1,
		*data2;
	int	res = 0,
		*offset,
		d1IsNull,
		d2IsNull;
	double	fp1,
		fp2;
	int8_t	int8p1, int8p2;
	int16_t	int16p1, int16p2;
	int	int32p1, int32p2;
#ifdef HUGE_T
	HUGE_T	int64p1, int64p2;
#endif


	/*
	** Allow for cases when rows are not defined
	*/
	debugTrace(TRACE_IN,"compareRows()");
	if (r1 && !r2)
	{
		debugTrace(TRACE_OUT,"compareRows()");
		return(-1);
	}
	if (!r1 && r2)
	{
		debugTrace(TRACE_OUT,"compareRows()");
		return(1);
	}
	if (!r1 && !r2)
	{
		debugTrace(TRACE_OUT,"compareRows()");
		return(0);
	}

	/*
	** OK, we have both rows.
	*/
	data1 = r1->data;
	data2 = r2->data;
	curOrder = order;
	offset = olist;
	while(curOrder)
	{
		/* RNS
		 * Allow for cases where data is not defined i.e.,
		 * try to do something reasonable with null values.
		 * How should we compare them?
		 * For now, treat them as less than anything else.
		 */
		d1IsNull = (*(data1 + *offset) == 0);
		d2IsNull = (*(data2 + *offset) == 0);
		if (d1IsNull || d2IsNull)
		{
			if (d1IsNull && d2IsNull)
			{
				res = 0;
			}
			else if (d1IsNull)
			{
				res = -1;
			}
			else
			{
				res = 1;
			}
		}
		else switch(typeBaseType(curOrder->type))
		{
			case INT32_TYPE:
			case UINT32_TYPE:
				bcopy4((data1 + *offset +1), &int32p1);
				bcopy4((data2 + *offset +1), &int32p2);
				if (int32p1 == int32p2)
					res = 0;
				if (int32p1 > int32p2)
					res = 1;
				if (int32p1 < int32p2)
					res = -1;
				break;

			case INT8_TYPE:
			case UINT8_TYPE:
				bcopy((data1 + *offset +1), &int8p1, 1);
				bcopy((data2 + *offset +1), &int8p2, 1);
				if (int8p1 == int8p2)
					res = 0;
				if (int8p1 > int8p2)
					res = 1;
				if (int8p1 < int8p2)
					res = -1;
				break;

			case INT16_TYPE:
			case UINT16_TYPE:
				bcopy((data1 + *offset +1), &int16p1, 2);
				bcopy((data2 + *offset +1), &int16p2, 2);
				if (int16p1 == int16p2)
					res = 0;
				if (int16p1 > int16p2)
					res = 1;
				if (int16p1 < int16p2)
					res = -1;
				break;

#ifdef HUGE_T
			case INT64_TYPE:
			case UINT64_TYPE:
				bcopy((data1 + *offset +1),buf, sizeof(HUGE_T));
				int64p1 = (HUGE_T) * (HUGE_T*)buf;
				bcopy((data2 + *offset +1),buf, sizeof(HUGE_T));
				int64p2 = (HUGE_T) * (HUGE_T*)buf;
				if (int64p1 == int64p2)
					res = 0;
				if (int64p1 > int64p2)
					res = 1;
				if (int64p1 < int64p2)
					res = -1;
				break;
#endif

			case CHAR_TYPE:
				cp1 = (char *)data1 + *offset +1;
				cp2 = (char *)data2 + *offset +1;
#ifdef HAVE_STRCOLL
				/* 
				** There's no strncoll so ensure the data
				** is null terminated
				*/
				tmp1 = *(cp1 + curOrder->length);
				tmp2 = *(cp2 + curOrder->length);
				*(cp1 + curOrder->length) = 0;
				*(cp2 + curOrder->length) = 0;
				res = strcoll(cp1,cp2);
				*(cp1 + curOrder->length) = tmp1;
				*(cp2 + curOrder->length) = tmp2;
#else
				res = strncmp(cp1,cp2,curOrder->length);
#endif
				break;

			case REAL_TYPE:
				bcopy8((data1+*offset+2),buf);
				fp1 = (double) * (double *)(buf);
				bcopy8((data2+*offset+2),buf);
				fp2 = (double) * (double *)(buf);
				if (fp1 == fp2)
					res = 0;
				if (fp1 > fp2)
					res = 1;
				if (fp1 < fp2)
					res = -1;
				break;

			case TEXT_TYPE:
				cp1 = (char *)data1 + *offset +1;
				cp2 = (char *)data2 + *offset +1;
				res = varcharCompare(curOrder->entry, 
					(u_char*)cp1, (u_char*)cp2, 
					curOrder->length);
				break;

			case BYTE_TYPE:
				cp1 = (char *)data1 + *offset +1;
				cp2 = (char *)data2 + *offset +1;
				res = localByteCmp(cp1,cp2,curOrder->length);
				break;
		}
		if (curOrder->dir == DESC)
		{
			res = 0 - res;
		}
		if (res != 0)
		{
			debugTrace(TRACE_OUT,"compareRows()");
			return(res);
		}
		curOrder = curOrder->next;
		offset++;
	}
	debugTrace(TRACE_OUT,"compareRows()");
	return(0);
}




