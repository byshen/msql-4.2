/*	memory.c	- 
**
**
** Copyright (c) 2002  Hughes Technologies Pty Ltd
**
** Permission to use, copy, and distribute for non-commercial purposes,
** is hereby granted without fee, providing that the above copyright
** notice appear in all copies and that both the copyright notice and this
** permission notice appear in supporting documentation.
**
** The software may be modified for your own purposes, but modified versions
** may not be distributed.
**
** This software is provided "as is" without any expressed or implied warranty.
**
**
*/

#include <common/config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include <common/msql_defs.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>

#define IN_MEMORY_C_SOURCE
#include "memory.h"

#define	CACHE_LEN	10
#define	TOKEN_CACHE_LEN	40
#define TOKEN_BUF_LEN	NAME_LEN + 15


static int		fieldCount = 0,
			identCount = 0,
			tokenCount = 0,
			queryCount = 0,
			valueCount = 0,
			valListCount = 0,
			tableCount = 0,
			condCount = 0,
			orderCount = 0;

static mField_t		*fieldCache[CACHE_LEN];
static mIdent_t		*identCache[CACHE_LEN];
static mQuery_t		*queryCache[CACHE_LEN];
static mVal_t		*valueCache[CACHE_LEN];
static mValList_t	*valListCache[CACHE_LEN];
static mTable_t		*tableCache[CACHE_LEN];
static char		*tokenCache[TOKEN_CACHE_LEN];
static mCond_t		*condCache[CACHE_LEN];
static mOrder_t		*orderCache[CACHE_LEN];


mField_t * memMallocField()
{
	mField_t	*new;

	/*
	** Get us a field struct either from the cache or a fresh malloc
	*/
	if (fieldCount > 0)
	{
		fieldCount--;
		new = fieldCache[fieldCount];
		fieldCache[fieldCount] = NULL;
	}
	else
	{
		new = (mField_t *)malloc(sizeof(mField_t));
		bzero(new, sizeof(mField_t));
	}

	/*
	** Initialise the fields
	*/
	*new->table = *new->name = 0;
	new->value = NULL;
	new->function = NULL;
	new->entry = NULL;
	new->next = NULL;
	new->type = new->sysvar = new->length = new->dataLength = 
		new->offset = new->null = new->flags = new->fieldID =
		new->literalParamFlag = new->functResultFlag = 
		new->overflow = 0;
	return(new);
}


void memFreeField(ptr)
	mField_t 	*ptr;
{

	if (ptr->value != NULL)
	{
		printf("Non-null value pointer in memFreeField()\n");
		abort();
	}
	if (fieldCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		fieldCache[fieldCount] = ptr;
		fieldCount++;
	}
}


mIdent_t * memMallocIdent()
{
	mIdent_t	*new;

	/*
	** Get us an ident struct either from the cache or a fresh malloc
	*/
	if (identCount > 0)
	{
		identCount--;
		new = identCache[identCount];
		identCache[identCount] = NULL;
	}
	else
	{
		new = (mIdent_t *)malloc(sizeof(mIdent_t));
	}

	/*
	** Initialise the fields
	*/
	*new->seg1 = *new->seg2 = 0;
	return(new);
}


void memFreeIdent(ptr)
	mIdent_t 	*ptr;
{

	if (identCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		identCache[identCount] = ptr;
		identCount++;
	}
}

mQuery_t * memMallocQuery()
{
	mQuery_t	*new;

	/*
	** Get us a query struct either from the cache or a fresh malloc
	*/
	if (queryCount > 0)
	{
		queryCount--;
		new = queryCache[queryCount];
		queryCache[queryCount] = NULL;
	}
	else
	{
		new = (mQuery_t *)malloc(sizeof(mQuery_t));
	}

	/*
	** Initialise the fields
	*/
	bzero(new, sizeof(mQuery_t));
	return(new);
}


void memFreeQuery(ptr)
	mQuery_t 	*ptr;
{

	if (queryCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		queryCache[queryCount] = ptr;
		queryCount++;
	}
}


mVal_t * memMallocValue()
{
	mVal_t	*new;

	/*
	** Get us a value struct either from the cache or a fresh malloc
	*/
	if (valueCount > 0)
	{
		valueCount--;
		new = valueCache[valueCount];
		valueCache[valueCount] = NULL;
	}
	else
	{
		new = (mVal_t *)malloc(sizeof(mVal_t));
	}

	/*
	** Initialise the fields
	bzero(new, sizeof(mVal_t));
	*/
	new->val.int64Val = (HUGE_T)0;
	new->val.charVal = new->val.byteVal = NULL;
	new->type = new->nullVal = new->dataLen = new->precision = 0;
	return(new);
}


void memFreeValue(ptr)
	mVal_t 	*ptr;
{
	if (valueCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		valueCache[valueCount] = ptr;
		valueCount++;
	}
}



mTable_t * memMallocTable()
{
	mTable_t 	*new;

	/*
	** Get us a table struct either from the cache or a fresh malloc
	*/
	if (tableCount > 0)
	{
		tableCount--;
		new = tableCache[tableCount];
		tableCache[tableCount] = NULL;
	}
	else
	{
		new = (mTable_t *)malloc(sizeof(mTable_t));
	}

	/*
	** Initialise the fields
	*/
	*new->name = *new->cname = 0;
	new->next = NULL;
	new->done = 0;

	return(new);
}


void memFreeTable(ptr)
	mTable_t 	*ptr;
{

	if (tableCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		tableCache[tableCount] = ptr;
		tableCount++;
	}
}




mCond_t * memMallocCondition()
{
	mCond_t 	*new;

	/*
	** Get us a table struct either from the cache or a fresh malloc
	*/
	if (condCount > 0)
	{
		condCount--;
		new = condCache[condCount];
		condCache[condCount] = NULL;
	}
	else
	{
		new = (mCond_t *)malloc(sizeof(mCond_t));
	}

	/*
	** Initialise the fields
	*new->table = *new->name = 0;
	new->value = new->maxValue = NULL;
	new->op = new->bool = new->type = new->length = new->sysvar = 
		new->fieldID = 0;
	new->next = new->subCond = NULL;
	*/
	bzero(new, sizeof(mCond_t));

	return(new);
}


void memFreeCondition(ptr)
	mCond_t 	*ptr;
{

	if (ptr->value != NULL || ptr->maxValue != NULL)
	{
		printf("Non-null value pointer in memFreeField()\n");
		abort();
	}
	if (condCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		condCache[condCount] = ptr;
		condCount++;
	}
}




mOrder_t * memMallocOrder()
{
	mOrder_t 	*new;

	/*
	** Get us an order struct either from the cache or a fresh malloc
	*/
	if (orderCount > 0)
	{
		orderCount--;
		new = orderCache[orderCount];
		orderCache[orderCount] = NULL;
	}
	else
	{
		new = (mOrder_t *)malloc(sizeof(mOrder_t));
	}

	/*
	** Initialise the fields
	*/
	*new->table = *new->name = 0;
	new->next = NULL;
	new->dir = new->type = new->length = 0;
	new->entry = NULL;
	return(new);
}


void memFreeOrder(ptr)
	mOrder_t 	*ptr;
{

	if (orderCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		orderCache[orderCount] = ptr;
		orderCount++;
	}
}


char * memMallocToken(buf, len)
	char	*buf;
	int	len;
{
	char	*new;

	/*
	** Get us a buffer
	*/
	if (len < TOKEN_BUF_LEN)
	{
		if (tokenCount > 0)
		{
			tokenCount--;
			new = tokenCache[tokenCount];
			tokenCache[tokenCount] = NULL;
		}
		else
		{
			new = malloc(TOKEN_BUF_LEN);
		}
	}
	else
	{
		new = malloc(len + 1);
	}

	/*
	** Initialise the buffer
	*/
        bcopy(buf,new,len);
        *(new + len) = 0;
	return(new);
}


void memFreeToken(ptr)
	u_char 	*ptr;
{
	if (tokenCount >= TOKEN_CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		tokenCache[tokenCount] = (char*)ptr;
		tokenCount++;
	}
}


mValList_t * memMallocValList()
{
	mValList_t 	*new;

	/*
	** Get us a valList struct either from the cache or a fresh malloc
	*/
	if (valListCount > 0)
	{
		valListCount--;
		new = valListCache[valListCount];
		valListCache[valListCount] = NULL;
	}
	else
	{
		new = (mValList_t *)malloc(sizeof(mValList_t));
	}

	/*
	** Initialise the fields
	*/
	new->offset = 0;
	new->value = NULL;
	new->next = NULL;

	return(new);
}


void memFreeValList(ptr)
	mValList_t 	*ptr;
{

	if (valListCount >= CACHE_LEN)
	{
		free(ptr);
	}
	else
	{
		valListCache[valListCount] = ptr;
		valListCount++;
	}
}



void memDropCaches()
{
	int	count;

	for(count = 0; count < fieldCount; count++)
		free(fieldCache[count]);
	for(count = 0; count < identCount; count++)
		free(identCache[count]);
	for(count = 0; count < tokenCount; count++)
		free(tokenCache[count]);
	for(count = 0; count < queryCount; count++)
		free(queryCache[count]);
	for(count = 0; count < valueCount; count++)
		free(valueCache[count]);
	for(count = 0; count < tableCount; count++)
		free(tableCache[count]);
	for(count = 0; count < condCount; count++)
		free(condCache[count]);
	for(count = 0; count < orderCount; count++)
		free(orderCache[count]);
	for(count = 0; count < valListCount; count++)
		free(valListCache[count]);
}

