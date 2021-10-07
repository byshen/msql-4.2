/*
** memhash.c	- in-core hash table routines
**
** Copyright (c) 1998 Hughes Technologies Pty Ltd.
*/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "memhash.h"


#define	HASH_WIDTH	1024
#define	HASH_DEPTH	128
#define	NULL_ENTRY	0xFFFFFFFF
#define	HASH_HDR_LEN	(sizeof(char *) + sizeof(u_int))
#define	HASH_BKT_LEN(d)	(((d) * sizeof(u_int)) + HASH_HDR_LEN)


int __hashNumBuckets;

mhash_t *hashCreate(width, depth)
	int	width,
		depth;
{
	mhash_t	*new;
	char	*table;
	int	size;

	if (width == HT_HASH_DEFAULT)
		width = HASH_WIDTH;
	if (depth == HT_HASH_DEFAULT)
		depth = HASH_DEPTH;
	size = width * HASH_BKT_LEN(depth);
	table = (char *)malloc(size);
	memset(table,0xFF, size);
	new = (mhash_t *)malloc(sizeof(mhash_t));
	new->curBucket = -1;
	new->curEntry = -1;
	new->curPage = NULL;
	new->table = table;
	new->width = width;
	new->depth = depth;
	__hashNumBuckets = depth;
	return(new);
}


int hashInsert(ht, value)
	mhash_t	*ht;
	u_int	value;
{
	char	*table;
	int	hash,
		numEntries;
	char	*baseP,
		**cpp;
	u_int	*sizeP,
		*entryP;

	/*
	** Find the bucket
	*/
	table = ht->table;
	hash = value % ht->width;
	baseP = table + (hash * HASH_BKT_LEN(ht->depth));

	/*
	** If the pointer is set to a secondary bucket follow
	** the chain
	*/
	while (* (char **)baseP != (char *)NULL_ENTRY)
	{
		cpp = (char **)baseP;
		baseP = *cpp;
	}
	sizeP = (u_int *)(baseP + sizeof(char *));
	numEntries = *sizeP;
	if (numEntries == NULL_ENTRY)
	{
		numEntries = 0;
	}
	

	/*
	** Drop in the new value
	*/
	if (numEntries == ht->depth)
	{
		char	*tmp;

		/*
		** Create a new bucket and link it into the list
		*/
		tmp = (char *)malloc(HASH_BKT_LEN(ht->depth));
		if (tmp == NULL)
			return(-1);
		__hashNumBuckets++;
		memset(tmp,0xFF, HASH_BKT_LEN(ht->depth));
		cpp = (char **)baseP;
		*cpp = baseP = tmp;
		sizeP = (u_int *)(baseP + sizeof(char *));
		numEntries = *sizeP;
		if (numEntries == NULL_ENTRY)
		{
			numEntries = 0;
		}
	}
	entryP = (u_int *)(baseP + HASH_HDR_LEN + (numEntries * sizeof(u_int)));
	*entryP = value;
	numEntries++;
	*sizeP = numEntries;
	return(0);
}


int hashLookup(ht, value)
	mhash_t	*ht;
	u_int	value;
{
	char	*table;
	int	hash,
		numEntries,
		count;
	char	*baseP,
		**cpp;
	u_int	*sizeP,
		*entryP;

	/*
	** Find the bucket
	*/
	table = ht->table;
	hash = value % ht->width;
	baseP = table + (hash * HASH_BKT_LEN(ht->depth));

	/*
	** Scan all buckets in the chain looking for this value
	*/
	while(1)
	{
		sizeP = (u_int *)(baseP + sizeof(char *));
		numEntries = *sizeP;
		count = 0;
		entryP = (u_int *)(baseP + HASH_HDR_LEN);
		while(count < numEntries)
		{
			if (*entryP == value)
				return(1);
			entryP++;
			count++;
		}
		
		if (* (char **)baseP == (char *)NULL_ENTRY)
			break;
		cpp = (char **)baseP;
		baseP = *cpp;
	}
	return(0);
}


u_int hashGetFirst(ht)
	mhash_t	*ht;
{
	char	*table,
		*baseP;
	u_int	*sizeP,
		*entryP;

	ht->curBucket = 0;
	ht->curEntry = 0;
	
	table = ht->table;
	while(ht->curBucket < ht->width)
	{
		baseP = table + (ht->curBucket * HASH_BKT_LEN(ht->depth));
		sizeP = (u_int *)(baseP + sizeof(char *));
		if (*sizeP == 0 || *sizeP == NULL_ENTRY)
		{
			ht->curBucket++;
			continue;
		}
		ht->curPage = baseP;
		entryP = (u_int *)(baseP + HASH_HDR_LEN);
		return(*entryP);
	}
	return(NULL_ENTRY);
}



u_int hashGetNext(ht)
	mhash_t	*ht;
{
	char	*table,
		*baseP,
		**cpp;
	u_int	*sizeP,
		*entryP;

	ht->curEntry++;
	table = ht->table;
	baseP = ht->curPage;

	while(ht->curBucket < ht->width)
	{
		sizeP = (u_int *)(baseP + sizeof(char *));
		if (ht->curEntry >= *sizeP || *sizeP == NULL_ENTRY)
		{
			ht->curEntry = 0;
			if (* (char **)baseP == (char *)NULL_ENTRY)
			{
				ht->curBucket++;
				baseP = table + (ht->curBucket * 
					HASH_BKT_LEN(ht->depth));
				ht->curPage = baseP;
				continue;
			}
			cpp = (char **)baseP;
			baseP = *cpp;
			ht->curPage = baseP;
			continue;
		}
		entryP = (u_int *)(baseP + HASH_HDR_LEN);
		entryP += ht->curEntry;
		return(*entryP);
	}
	return(NULL_ENTRY);
}


hashDestroy(ht)
	mhash_t	*ht;
{
	char	*table,
		*baseP,
		*prevP,
		*cpp;
	int	count;

	table = ht->table;
	count = 0;
	while(count < ht->width)
	{
		baseP = table + (count * HASH_BKT_LEN(ht->depth));
		if (* (char **)baseP != (char *)NULL_ENTRY)
		{
			baseP = * (char **)baseP;
			while (* (char **)baseP != (char *)NULL_ENTRY)
			{
				prevP = baseP;
				baseP = * (char **)baseP;
				free(prevP);
			}
			free(baseP);
		}
		count++;
	}
	free(table);
	free(ht);
}

